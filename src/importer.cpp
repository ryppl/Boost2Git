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
        path const svn_path(svn_path_);

        // We have found a path being modified in SVN.  
        Rule const* const match = ruleset.matcher().longest_match(svn_path.str(), revnum);

        // Start by marking its Git target for deletion.  Note: it's
        // too early to error-out on unmapped SVN paths here: any that
        // are problematic will be picked up later.
        if (match)
            add_svn_tree_to_delete(svn_path, match);

        // If it wasn't being deleted in SVN, also convert all of its
        // files to Git.
        if (change->change_kind != svn_fs_path_change_delete)
            add_svn_tree_to_convert(rev, svn_path);

        // Assume it's a directory if it's not known to be a file.
        // This is conservative, in case node_kind == svn_node_unknown.
        if (change->node_kind != svn_node_file)
        {
            // Remember directory copy sources
            if (change->copyfrom_known && change->copyfrom_path != nullptr)
            {
                // It's OK to retain only the last source directory if
                // this target was copied-to more than once
                svn_directory_copies[svn_path] 
                    = std::make_pair(change->copyfrom_rev, change->copyfrom_path);
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
    }
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

        for (auto r : changed_repositories)
            r->open_commit(rev);
        
        for (auto& svn_path : svn_paths_to_convert)
            convert_svn_tree(rev, svn_path.c_str(), pass == 0);

        std::vector<git_repository*> closed_repositories;
        for (auto r : changed_repositories)
        {
            if (r->close_commit())
                closed_repositories.push_back(r);
        }

        for (auto r : closed_repositories)
            changed_repositories.erase(r);
        
        ++pass;
    }
    while(!changed_repositories.empty());
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

void importer::convert_svn_tree(
    svn::revision const& rev, path const& svn_path, bool discover_changes)
{
    if (boost::contains(svn_path.str(), "/CVSROOT/"))
        return;

    auto& log = Log::trace() << "converting " << svn_path << "... ";
    switch( svn::call(svn_fs_check_path, rev.fs_root, svn_path.c_str(), rev.pool) )
    {
    case svn_node_none: // If it turns out there's nothing here, there's nothing to do.
        log << std::endl;
        Log::error() << svn_path << " doesn't exist!" << std::endl;
        assert(!"We added a non-existent path to convert somehow?!");
        return;

    case svn_node_unknown:
        log << std::endl;
        Log::error() << svn_path << " has unknown type!" << std::endl;
        assert(!"SVN should know the type of every node in its filesystem?!");
        return;

    case svn_node_file:
    {
        log << "(a file)" << std::endl;
        convert_svn_file(rev, svn_path, discover_changes);
    }
    break;

    case svn_node_dir:
        log << "(a directory)" << std::endl;
        AprPool dir_pool = rev.pool.make_subpool();
        apr_hash_t *entries = svn::call(svn_fs_dir_entries, rev.fs_root, svn_path.c_str(), dir_pool);
        for (apr_hash_index_t *i = apr_hash_first(dir_pool, entries); i; i = apr_hash_next(i))
        {
            char const* subpath;
            void* value;
            apr_hash_this(i, (void const **)&subpath, nullptr, nullptr);
            convert_svn_tree(rev, (svn_path/subpath).c_str(), discover_changes);
        }
        break;
    };
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
    Rule const* const match = ruleset.matcher().longest_match(svn_path.str(), revnum);
    if (!match)
    {
        Log::error() << "Unmatched svn path " << svn_path 
                     << " in r" << revnum << std::endl;
        assert(!"unmatched SVN path");
        return;
    }

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

    fast_import.filemodify_hdr(
        match->git_path()/svn_path.sans_prefix(match->svn_path()) );

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
