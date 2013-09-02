// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include "importer.hpp"
#include "ruleset.hpp"
#include "svn.hpp"
#include "log.hpp"
#include "path.hpp"
#include <boost/range/adaptor/map.hpp>
#include <boost/function_output_iterator.hpp>
#include <boost/range/as_literal.hpp>
#include <svn_fs.h>
#include <apr_hash.h>

using boost::adaptors::map_values;
using boost::as_literal;

importer::importer(svn const& svn_repo, Ruleset const& ruleset)
    : svn_repository(svn_repo), ruleset(ruleset), revnum(0)
{
    for(auto const& rule : ruleset.repositories())
    {
        git_repository* repo = demand_repo(rule.name);

        repo->set_super_module( 
            demand_repo(rule.submodule_in_repo), rule.submodule_path);
    }
}

// Return a pointer to a git_repository object having the given
// repository name.  If name is empty, return null
inline git_repository*
importer::demand_repo(std::string const& name)
{
    if (name.empty())
        return nullptr;

    auto p = repositories.find(name);
    if (p == repositories.end())
    {
        p = repositories.emplace_hint(
            p, std::piecewise_construct, 
            std::make_tuple(name), std::make_tuple(name));
    }
    return &p->second;
};

// Return the number of the last SVN revision that was successfully
// convertd to Git
int importer::last_valid_svn_revision()
{
    return revnum;
}

// Unless the Git ref specified by match has already been completely
// processed in this revision, find it, mark it for modification, and
// return it.  Otherwise, discover_changes will be false.
git_repository::ref* importer::prepare_to_modify(Rule const* match, bool discover_changes)
{
    auto& repo = repositories.find(match->git_repo_name())->second;
    if (!discover_changes && changed_repositories.count(&repo) == 0)
        return nullptr;
    changed_repositories.insert(&repo);
    if (auto s = repo.in_super_module())
        changed_repositories.insert(s);
    return repo.modify_ref(match->git_ref_name(), discover_changes);
}

path importer::add_svn_tree_to_delete(path const& svn_path, Rule const* match)
{
    assert(match);
    assert(svn_path.starts_with(match->svn_path()));

    // Find the unmatched suffix of the path
    path path_suffix = svn_path.sans_prefix(match->svn_path());

    // Access the ref for modification
    auto* ref = prepare_to_modify(match, true);

    // Mark the git path to be deleted at the start of the commit
    ref->pending_deletions.insert( match->git_path()/path_suffix );

    return path_suffix;
}

void importer::invalidate_svn_tree(
    svn::revision const& rev, path const& svn_path, Rule const* match)
{
    path path_suffix = add_svn_tree_to_delete(svn_path, match);

    add_svn_tree_to_convert(rev, svn_path);

    // Mark every svn tree that's mapped into the rule's git subtree for
    // (re-)conversion.

    ruleset.matcher().git_subtree_rules(
        // FIXME: concatenating a subpath to a git address is pretty ugly!
        match->git_address() 
        + (match->git_path().str().empty() ? "" : "/") 
        + path_suffix.str(), 
        revnum,
        boost::make_function_output_iterator(
            [&](Rule const* r){ add_svn_tree_to_convert(rev, r->svn_path()); })
    );
}

void importer::add_svn_tree_to_convert(
    svn::revision const& rev, path const& svn_path)
{
    // Mark this svn_path for conversion.  
    auto kind = svn::call(
        svn_fs_check_path, rev.fs_root, svn_path.c_str(), rev.pool);

    if (kind != svn_node_none) {
        Log::trace() << "adding " << svn_path << " for conversion" << std::endl;
        svn_paths_to_convert.insert(svn_path);
    }
}

// Deal with all the SVN changes in this revision.  We're not actually
// writing any file contents (blobs or trees) to Git in this step.
// Our job is merely to make a record of paths to be deleted in Git at
// the beginning of the commit and SVN files/directories to
// subsequently be traversed and converted to Git blobs and trees.
void importer::process_svn_changes(svn::revision const& rev)
{
    apr_hash_t *changes = svn::call(svn_fs_paths_changed2, rev.fs_root, rev.pool);
    for (apr_hash_index_t *i = apr_hash_first(rev.pool, changes); i; i = apr_hash_next(i))
    {
        const char *svn_path_ = 0;
        svn_fs_path_change2_t *change = 0;
        apr_hash_this(i, (const void**) &svn_path_, nullptr, (void**) &change);
        // According to the APR docs, this means the hash entry was
        // deleted, so it should never happen
        assert(change != nullptr); 

        // Ignore changes that only edit properties
        if (change->change_kind == svn_fs_path_change_modify && !change->text_mod)
            continue;

        path const svn_path(svn_path_);
        
        // We have found a path being modified in SVN.  Note: it's
        // too early to error-out on unmapped SVN paths here: any that
        // are problematic will be picked up later.
        Rule const* const match = match_svn_path(svn_path, revnum, false);

        // Start by marking its Git target for deletion.  
        if (match)
            add_svn_tree_to_delete(svn_path, match);

        // If it wasn't being deleted in SVN, also convert all of its
        // files to Git.
        if (change->change_kind != svn_fs_path_change_delete)
            add_svn_tree_to_convert(rev, svn_path);

        // Assume it's a directory if it's not known to be a file.
        // This is conservative, in case node_kind == svn_node_unknown.
        if (change->node_kind != svn_node_file)
            process_svn_directory_change(rev, change, svn_path);
    }
}

void importer::process_svn_directory_change(
    svn::revision const& rev, svn_fs_path_change2_t *change, path const& svn_path)
{
    // Remember directory copy sources
    if (change->copyfrom_known && change->copyfrom_path != nullptr)
    {
        // It's OK to retain only the last source directory if
        // this target was copied-to more than once
        auto& copy = svn_directory_copies[svn_path];
        copy.src_revision = change->copyfrom_rev;
        copy.src_directory = change->copyfrom_path;
    }

    // Handle rules that map SVN subtrees of the deleted path
     ruleset.matcher().svn_subtree_rules(
         svn_path.str(), revnum,
         // Mark the target Git tree for deletion, but
         // also convert all SVN trees being mapped into a
         // subtree of the Git tree.
         boost::make_function_output_iterator(
             [&](Rule const* r){ 
                 invalidate_svn_tree(rev, r->svn_path(), r); }));
}

void importer::import_revision(int revnum)
{
    if (Log::get_level() >= Log::Trace)
    {
        Log::trace() 
        << "################## importing revision " 
        << revnum << " ##################" << std::endl;
    }
    else if (revnum % 1000 == 0)
    {
        Log::info() << "importing revision " << revnum << std::endl;
    }

    this->revnum = revnum;
    svn::revision rev = svn_repository[revnum];

    // Importing an SVN revision happens in two phases.  In the first
    // phase we discover actions to be performed: Git subtrees that
    // must be deleted and SVN subtrees whose files must be
    // (re-)convertd to Git.  In the second phase, we actually do
    // those deletions and translations.

    //
    // Phase I: Action Discovery.  
    //
    svn_paths_to_convert.clear();
    changed_repositories.clear();
    svn_directory_copies.clear();

    // Deal with rules becoming active/inactive in this revision
    for (Rule const* r: ruleset.matcher().rules_in_transition(revnum))
        invalidate_svn_tree(rev, r->svn_path(), r);

    // Discover SVN paths that are being deleted/modified
    process_svn_changes(rev);

    Log::trace() 
        << svn_paths_to_convert.size() 
        << " SVN " 
        << (svn_paths_to_convert.size() == 1 ? "path" : "paths")
        << " to convert" << std::endl;

    discover_merges(rev);

    //
    // Phase II: Writing to Git
    //

    // Though it is expected to be rare, a single SVN commit can
    // generate commits in multiple refs of the same Git repo.
    // However, the changes in a single Git ref's commit must all be
    // sent contiguously to the fast-import process.  Therefore, for
    // each Git ref that must be committed during this SVN revision, a
    // separate pass over the SVN trees to convert is required.
    //
    // During each pass, we map SVN paths to Git, and may discover Git
    // repositories and refs to that need to be committed in this SVN
    // revision.  As we handle refs and repositories, we remove them
    // from the set of changed things.  To avoid processing them
    // again, this discovery is only enabled in the first pass.  This
    // explains the "discover_changes" parameters you see in many of
    // this class' member functions.
    int pass = 0;
    do
    {
        Log::trace() << "pass " << pass << std::endl;
        assert(pass < 5000);

        // Make a copy so it can be modified as we work this pass
        auto changed_repos = changed_repositories;
        for (auto r : changed_repos)
            r->open_commit(rev);
        
        auto paths_to_convert = svn_paths_to_convert;
        for (auto& svn_path : paths_to_convert)
            convert_svn_tree(rev, svn_path.c_str(), pass == 0);

        for (auto r : changed_repos)
            r->prepare_to_close_commit();

        std::vector<git_repository*> closed_repositories;
        for (auto r : changed_repos)
        {
            if (r->close_commit())
                closed_repositories.push_back(r);
        }

        for (auto r : closed_repositories)
            changed_repositories.erase(r);
        
        ++pass;
    }
    while(!changed_repositories.empty());

    warn_about_cross_repository_copies();
}

void importer::warn_about_cross_repository_copies()
{
    for (auto& kv: svn_directory_copies)
    {
        if (kv.second.crossed_repositories.empty())
            continue;

        auto& warn = Log::warn() 
            << "In r" << revnum << ", SVN directory copy " 
            << kv.second.src_directory << " => " << kv.first
            << " crossed Git repositories:";

        for (auto& src_dst : kv.second.crossed_repositories)
        {
            warn << " (" << src_dst.first << " -> " << src_dst.second << ")";
        }

        warn << std::endl;
    }
}

importer::~importer()
{
    // Apparently there's at least some ordering constraint that is
    // violated by simply closing and waiting for the death of each
    // process, in sequence.  If we don't close all the input streams
    // here, we hang waiting for the first process to exit after
    // closing its stream.
    for (auto& repo : repositories | map_values)
        repo.fast_import().close();
}

template <class F>
void for_each_svn_file(
    svn::revision const& rev, path const& svn_path, F const& f, AprPool* pool_ = 0)
{
    if (boost::contains(svn_path.str(), "/CVSROOT/"))
        return;

    auto& pool = pool_ ? *pool_ : rev.pool;

    switch( svn::call(svn_fs_check_path, rev.fs_root, svn_path.c_str(), pool) )
    {
    case svn_node_none: // If it turns out there's nothing here, there's nothing to do.
        Log::error() << svn_path << " doesn't exist!" << std::endl;
        assert(!"We added a non-existent path to convert somehow?!");
        return;

    case svn_node_unknown:
        Log::error() << svn_path << " has unknown type!" << std::endl;
        assert(!"SVN should know the type of every node in its filesystem?!");
        return;

    case svn_node_file:
        f(svn_path);
        break;

    case svn_node_dir:
        AprPool dir_pool = pool.make_subpool();
        apr_hash_t *entries = svn::call(svn_fs_dir_entries, rev.fs_root, svn_path.c_str(), dir_pool);
        for (apr_hash_index_t *i = apr_hash_first(dir_pool, entries); i; i = apr_hash_next(i))
        {
            char const* subpath;
            void* value;
            apr_hash_this(i, (void const **)&subpath, nullptr, nullptr);
            for_each_svn_file(rev, svn_path/subpath, f, &dir_pool);
        }
        break;
    };
}

void importer::discover_merges(svn::revision const& rev)
{
    for (auto& kv : svn_directory_copies)
    {
        for_each_svn_file(
            rev, kv.first,
            [=](path const& file_path) 
            {
                if (Rule const* const match = match_svn_path(file_path, revnum))
                {
                    auto* dst_ref = prepare_to_modify(match, true);
                    record_merges(dst_ref, file_path, match);
                }
            });
    }
}

void importer::convert_svn_tree(
    svn::revision const& rev, path const& svn_path, bool discover_changes)
{
    for_each_svn_file(
        rev, svn_path, 
        [=,&rev](path const& file_path) {
            convert_svn_file(rev, file_path, discover_changes); 
        });
}

extern "C"
{
    svn_error_t *fast_import_raw_bytes(void *baton, const char *data, apr_size_t *len)
    {
        auto& fast_import = *static_cast<git_fast_import*>(baton);
        try
        {
            fast_import.write_raw(data, *len);
            return SVN_NO_ERROR;
        }
        catch(std::exception const& e)
        {
            return svn_error_createf(APR_EOF, SVN_NO_ERROR, "%s", e.what());
        }
        catch(...)
        {
            return svn_error_createf(APR_EOF, SVN_NO_ERROR, "unknown error");
        }
    }
}

void importer::convert_svn_file(
    svn::revision const& rev, path const& svn_path, bool discover_changes)
{
    Rule const* const match = match_svn_path(svn_path, revnum);
    if (!match) return;

    // There are two reasons we might skip processing this file in
    // this pass and come back for it in a later one:
    //
    // 1. the target repository and/or ref has already been fully
    // processed for this revision in an earlier pass over the
    // invalidated SVN trees
    auto* dst_ref = prepare_to_modify(match, discover_changes);
    if (dst_ref == nullptr)
        return;

    // Mark the repository as having changes that will need to be written
    changed_repositories.insert(dst_ref->repo);

    // 2. A different target ref is currently being written in this
    // repository.
    if (dst_ref->repo->open_commit(rev) != dst_ref)
        return;

    auto& fast_import = dst_ref->repo->fast_import();

    std::string svn_eol_style;
    svn_string_t *svn_eol_style_raw = svn::call(
        svn_fs_node_prop, rev.fs_root, svn_path.c_str(), "svn:eol-style", rev.pool);
    if(svn_eol_style_raw) svn_eol_style=std::string(svn_eol_style_raw->data, svn_eol_style_raw->len);
    
    auto git_attribs = options.gitattributes_tree.end();
    // Need to glob. Last match is what takes effect.
    for(auto &m : options.glob_cache)
    {
        // Manually glob for speed if it's a *.ext pattern
        /*if(m.first.size()==7 && !strncmp(m.first.c_str(), ".*\\.", 4))
        {
            const char *period=strrchr(svn_path.c_str(), '.');
            if(period)
            {
                if(!strcmp(period+1, m.first.c_str()+4))
                  git_attribs = m.second;
            }
        }
        else if(std::regex_match(svn_path.str(), std::regex(m.first)))
        {
            git_attribs = m.second;
        }*/
        if(std::regex_match(svn_path.str(), m.first))
        {
            git_attribs = m.second;
        }
    }
    std::string git_eol_style("unset");
    if(options.gitattributes_tree.end()!=git_attribs)
        git_eol_style = git_attribs->second.get<std::string>("svneol", "unset");
    //std::cout << "File '" << svn_path.str() << "' has svn-eol-style=" << svn_eol_style << " and git-eol-style=" << git_eol_style << std::endl;
    int need_to_crlf_reduce = 0;
    if(!git_eol_style.empty() && boost::iequals("native", git_eol_style))
    {
        if(!svn_eol_style.empty())
            need_to_crlf_reduce = boost::iequals("crlf", svn_eol_style);
        else
        {
            // Git thinks this file ought to be text with native EOL, yet svn::eol-style not set
            // which means whatever is in there is in there.
            // Set to CRLF reduce anyway as the CRLF reduction routine copes with LF only input
            need_to_crlf_reduce = 2;
        }
    }
    if(need_to_crlf_reduce)
    {
        auto& warn = Log::warn() 
            << "In r" << revnum << ", text file '" << svn_path.str() << "' has "
            << (2==need_to_crlf_reduce ? "missing svn::eol-style." : "incorrect svn:eol-style=CRLF. Downconverting to LF ...")
            << std::endl;
    }

    auto propvalue = svn::call(
        svn_fs_node_prop, rev.fs_root, svn_path.c_str(), "svn:executable", rev.pool);

    fast_import.filemodify_hdr(
        match->git_path()/svn_path.sans_prefix(match->svn_path()), 
        propvalue ? 0100755 : 0100644 );

    auto file_length = svn::call(
        svn_fs_file_length, rev.fs_root, svn_path.c_str(), rev.pool);

    AprPool scope = rev.pool.make_subpool();
    svn_stream_t* in_stream = svn::call(
        svn_fs_file_contents, rev.fs_root, svn_path.c_str(), scope);

    // If it's a symlink, we may need to lop 5 bytes off the front of the stream.
    /*
      svn_string_t *special = svn::call(
      svn_fs_node_prop, rev.fs_root, svn_path.c_str(), "svn:special", scope);
    */

    fast_import.data_hdr(file_length);
    svn_stream_t* out_stream = svn_stream_create(&fast_import, scope);
    svn_stream_set_write(out_stream, fast_import_raw_bytes);
    check_svn(svn_stream_copy3(in_stream, out_stream, nullptr, nullptr, scope));
    fast_import << LF;
}

// Given the SVN path of a file being converted to Git, try to find an
// SVN directory copy that caused this file to be converted, and from
// that, extract Git merge information
void importer::record_merges(git_repository::ref* target, path const& dst_svn_path, Rule const* match)
{
    // Look for an svn directory copy whose target contains svn_path
    auto p = svn_directory_copies.lower_bound(dst_svn_path);
    if (p == svn_directory_copies.begin())
        return;
    if (!dst_svn_path.starts_with((--p)->first))
        return;

    // compute the path and revision in SVN corresponding to the
    // source of this file in that directory copy
    auto src_revnum = p->second.src_revision;
    auto src_svn_path = p->second.src_directory / dst_svn_path.sans_prefix(p->first);

    // Find out where that path landed in Git
    Rule const* const src_match = match_svn_path(src_svn_path, src_revnum);
    if (!src_match) return;
    
    // If in a different repository, there's nothing to be done but warn
    auto const& src_repo_name = src_match->repo_rule->git_repo_name;

    if (src_repo_name == target->repo->name())
    {
        // Update the latest source revision merged
        target->repo->record_ancestor(
            target, src_match->git_ref_name(), src_revnum);
    }
    else        // Prepare to warn about cross-repository copies
    {
        // ignore all cross-repository copies into the sandbox.  These
        // may represent experimentation that was never merged back
        // into the main work area.  HACK/FIXME: this should not be
        // hardcoded.  A --sandbox= command-line option or an
        // annotation in the repository grammar would work better.
        if (target->repo->name() != "sandbox")
        {
            p->second.crossed_repositories.insert(
                std::make_pair(src_repo_name, target->repo->name()));
        }
    }
}

Rule const* importer::match_svn_path(path const& svn_path, std::size_t revnum, bool require_match)
{
    Rule const* match = ruleset.matcher().longest_match(svn_path.str(), revnum);
    if (require_match && match == nullptr)
    {
        Log::error() << "Unmatched svn path " << svn_path 
                     << " in r" << revnum << std::endl;
        assert(!"unmatched SVN path");
    }
    return match;
}
