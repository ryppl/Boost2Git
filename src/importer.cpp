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

// if discover_changes is false and the repository is not already
// marked for modification, return NULL.  Otherwise, find the named
// repository, mark it for modification, and return it
git_repository* importer::modify_repo(std::string const& name, bool discover_changes)
{
    auto& repo = repositories.find(name)->second;
    if (!discover_changes && changed_repositories.count(&repo) == 0)
        return nullptr;
    changed_repositories.insert(&repo);
    return &repo;
}

path importer::add_svn_tree_to_delete(path const& svn_path, Rule const* match)
{
    assert(match);
    assert(svn_path.starts_with(match->svn_path()));

    // Find the unmatched suffix of the path
    path path_suffix = svn_path.sans_prefix(match->svn_path());

    // Access the repository for modification
    auto* repo = modify_repo(match->git_repo_name());

    // Access the ref for modification
    auto ref = repo->modify_ref(match->git_ref_name());
    assert(ref);

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

        // We have found a path being modified in SVN.  Start by
        // marking its Git target for deletion.
        if (Rule const* const match = ruleset.matcher().longest_match(svn_path.str(), revnum))
            add_svn_tree_to_delete(svn_path, match);
        else
            // It's perfectly fine if an SVN path being deleted isn't
            // mapped anywhere in this revision; if the path ever
            // *was* mapped, the ruleset transition would have
            // handled its deletion anyway.
            assert(change->change_kind == svn_fs_path_change_delete && "Unmapped SVN path!");

        // If it wasn't being deleted in SVN, also convert all of its
        // files to Git.
        if (change->change_kind != svn_fs_path_change_delete)
            add_svn_tree_to_convert(rev, svn_path);

        // Assume it's a directory if it's not known to be a file.
        // This is conservative, in case node_kind == svn_node_unknown.
        if (change->node_kind != svn_node_file)
        {
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
    ((revnum % 1000) ? Log::trace() : Log::info())
        << "################## importing revision " 
        << revnum << " ##################" << std::endl;

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
    // However, all the changes in a single Git ref's commit must all
    // be sent contiguously to the fast-import process.  Therefore,
    // for each Git ref that must be committed during this SVN
    // revision, a separate pass over the SVN trees to convert is
    // required.
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
    auto* const dst_repo = modify_repo(match->git_repo_name(), discover_changes);
    if (dst_repo == nullptr)
        return;
    auto* dst_ref = dst_repo->modify_ref(match->git_ref_name(), discover_changes);
    if (dst_ref == nullptr)
        return;

    // Mark the repository as having changes that will need to be written
    changed_repositories.insert(dst_repo);

    // 2. A different target ref is currently being written in this
    // repository.
    if (dst_repo->open_commit(rev) != dst_ref)
        return;

    auto& fast_import = dst_repo->fast_import();

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
