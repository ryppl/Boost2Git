// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include "importer.hpp"
#include "ruleset.hpp"
#include "svn.hpp"
#include "log.hpp"
#include <boost/range/adaptor/map.hpp>
#include <boost/function_output_iterator.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/range/as_literal.hpp>
#include <svn_fs.h>
#include <apr_hash.h>

using boost::adaptors::map_values;
using boost::as_literal;

importer::importer(svn const& svn_repo, Ruleset const& ruleset)
    : svn_repository(svn_repo), ruleset(ruleset)
{
    for(auto const& rule : ruleset.repositories())
    {
        git_repository* repo = demand_repo(rule.name);

        repo->set_super_module( 
            demand_repo(rule.submodule_in_repo), rule.submodule_path);
    }
}

inline git_repository*
importer::demand_repo(std::string const& name)
{
    if (name.empty())
        return 0;

    auto p = repositories.find(name);
    if (p == repositories.end())
    {
        p = repositories.emplace_hint(
            p, std::piecewise_construct, 
            std::make_tuple(name), std::make_tuple(name));
    }
    return &p->second;
};

int importer::last_valid_svn_revision()
{
    return 1; // pessimization for now.  Later we should read marks files, etc.
}

git_repository& importer::modify_repo(std::string const& name)
{
    auto& repo = repositories.find(name)->second;
    changed_repositories.insert(&repo);
    return repo;
}

// Like path_append in rule.hpp, but never returns a path starting or ending in
// '/'.
static inline std::string path_join(std::string p0, std::string const& p1)
{
    if (!p1.empty()) // optimize a common case
        p0 = path_append(std::move(p0), p1);

    boost::algorithm::trim_if(p0, boost::is_any_of("/"));

    return p0;
}

std::string importer::add_svn_tree_to_delete(std::string const& svn_path, Rule const* match)
{
    assert(match);
    assert(boost::starts_with(svn_path, match->svn_path()));

    // Find the unmatched suffix of the path
    std::string path_suffix = svn_path.substr(match->svn_path().size());

    // Access the repository for modification
    auto& repo = modify_repo(match->git_repo_name());

    // Access the ref for modification
    auto& ref = repo.modify_ref(match->git_ref_name());

    // Mark the git path to be deleted at the start of the commit
    ref.pending_deletions.insert(path_join(match->git_path(), path_suffix));

    return path_suffix;
}

void importer::invalidate_svn_tree(
    svn::revision const& rev, std::string const& svn_path, Rule const* match)
{
    std::string path_suffix = add_svn_tree_to_delete(svn_path, match);

    add_svn_tree_to_rewrite(rev, svn_path);

    // Mark every svn tree that's mapped into the rule's git subtree for
    // rewriting.
    ruleset.matcher().git_subtree_rules(
        path_join(match->git_address(), path_suffix), 
        revnum,
        boost::make_function_output_iterator(
            [&](Rule const* r){ add_svn_tree_to_rewrite(rev, r->svn_path()); })
    );
}

void importer::add_svn_tree_to_rewrite(
    svn::revision const& rev, std::string const& svn_path)
{
    // Mark this svn_path for rewriting.  
    auto kind = svn::call(
        svn_fs_check_path, rev.fs_root, svn_path.c_str(), rev.pool);

    if (kind != svn_node_none) {
        Log::trace() << "adding " << svn_path << " for rewrite" << std::endl;
        svn_paths_to_rewrite.insert(svn_path);
    }
}

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
        std::string const svn_path(svn_path_);

        // kind is one of {modify, add, delete, replace}
        if (change->change_kind == svn_fs_path_change_delete)
        {
            Rule const* const match = ruleset.matcher().longest_match(svn_path, revnum);
            // It's perfectly fine if an SVN path being deleted isn't
            // mapped anywhere in this revision; if the path ever
            // *was* mapped, the ruleset transition would have
            // handled its deletion anyway.
            if (match)
                add_svn_tree_to_delete(svn_path, match);

            if (change->node_kind != svn_node_file)
            {
                // Handle rules that map SVN subtrees of the deleted path
                 ruleset.matcher().svn_subtree_rules(
                     svn_path, revnum,
                     // Mark the target Git tree for deletion, but
                     // also rewrite all SVN trees being mapped into a
                     // subtree of the Git tree.
                     boost::make_function_output_iterator(
                         [&](Rule const* r){ 
                             invalidate_svn_tree(rev, r->svn_path(), r); }));
            }
        }
        else // all the other change kinds can be treated the same
        {
            // Git doesn't care about directory changes
            if (change->node_kind != svn_node_dir)
                add_svn_tree_to_rewrite(rev, svn_path);
        }
    }
}

void importer::import_revision(int revnum)
{
    ((revnum % 1000) ? Log::trace() : Log::info())
        << "importing revision " << revnum << std::endl;

    this->revnum = revnum;
    svn_paths_to_rewrite.clear();
    changed_repositories.clear();

    svn::revision rev = svn_repository[revnum];

    // Deal with rules becoming active/inactive in this revision
    for (Rule const* r: ruleset.matcher().rules_in_transition(revnum))
        invalidate_svn_tree(rev, r->svn_path(), r);

    // Discover SVN paths that are being deleted/modified
    process_svn_changes(rev);

    Log::trace() 
        << svn_paths_to_rewrite.size() 
        << " SVN " 
        << (svn_paths_to_rewrite.size() == 1 ? "path" : "paths")
        << " to rewrite" << std::endl;

    // A single SVN commit can generate commits in multiple branches
    // of the same Git repo, but the changes in a single Git branch's
    // commit must all be sent contiguously to the fast-import
    // process.  Therefore, multiple passes through the rewritten SVN
    // paths may be required.
    do
    {
        for (auto& svn_path : svn_paths_to_rewrite)
            rewrite_svn_tree(rev, svn_path.c_str());

        for (auto r : changed_repositories)
        {
            if (r->close_commit())
                changed_repositories.erase(r);
        }
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

void importer::rewrite_svn_tree(
    svn::revision const& rev, std::string const& svn_path)
{
    if (boost::contains(svn_path, "/CVSROOT/"))
        return;

    auto& log = Log::trace() << "rewriting " << svn_path << "... ";
    switch( svn::call(svn_fs_check_path, rev.fs_root, svn_path.c_str(), rev.pool) )
    {
    case svn_node_none: // If it turns out there's nothing here, there's nothing to do.
        assert(!"We added a non-existent path to rewrite somehow?!");
        log << std::endl;
        Log::error() << svn_path << " doesn't exist!" << std::endl;
        return;
    case svn_node_unknown:
        assert(!"SVN should know the type of every node in its filesystem?!");
        log << std::endl;
        Log::error() << svn_path << " has unknown type!" << std::endl;
        return;
    case svn_node_file:
    {
        log << "(a file)" << std::endl;
        std::string pth = boost::starts_with(svn_path, "/") ? svn_path : "/" + svn_path;
        Rule const* const match = ruleset.matcher().longest_match(pth, revnum);
        assert(match); // At worst the path should get caught by a fallback rule, right?
    }
    break;

    case svn_node_dir:
        log << "(a directory)" << std::endl;
        apr_hash_t *entries = svn::call(svn_fs_dir_entries, rev.fs_root, svn_path.c_str(), rev.pool);
        for (apr_hash_index_t *i = apr_hash_first(rev.pool, entries); i; i = apr_hash_next(i))
        {
            char const* subpath;
            void* value;
            apr_hash_this(i, (void const **)&subpath, nullptr, nullptr);
            rewrite_svn_tree(rev, path_join(svn_path, subpath).c_str());
        }
        break;
    };
}
