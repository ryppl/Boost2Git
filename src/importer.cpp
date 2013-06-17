// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include "importer.hpp"
#include "ruleset.hpp"
#include "svn.hpp"
#include "log.hpp"
#include <boost/range/adaptor/map.hpp>
#include <boost/function_output_iterator.hpp>

using boost::adaptors::map_values;

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

    if (boost::ends_with(p0, "/"))
        p0.pop_back();
    if (boost::starts_with(p0, "/"))
        p0.erase(p0.begin());

    return p0;
}

std::string importer::delete_svn_path(std::string const& svn_path, Rule const* match)
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

void importer::rewrite_svn_tree(std::string const& svn_path, Rule const* match)
{
    std::string path_suffix = delete_svn_path(svn_path, match);

    // Mark this svn_path for rewriting.  If the path is being
    // deleted this will ultimately have no effect.
    svn_paths_to_rewrite.insert(svn_path);

    // Mark every svn tree that's mapped into this git subtree for
    // rewriting.
    ruleset.matches().git_subtree_rules(
        path_join(match->git_address(), path_suffix), 
        revnum,
        boost::make_function_output_iterator(
            [&](Rule const* r){ svn_paths_to_rewrite.insert(r->svn_path()); })
    );
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
            Rule const* const match = ruleset.matches().longest_match(svn_path, revnum);
            // It's perfectly fine if an SVN path being deleted isn't
            // mapped anywhere in this revision; if the path ever
            // *was* mapped, the ruleset transition would have
            // handled its deletion anyway.
            if (match)
                delete_svn_path(svn_path, match);

            if (change->node_kind != svn_node_file)
            {
                // Rewrite everything that maps from a subtree of the
                // path in question.
                 ruleset.matches().svn_subtree_rules(
                     svn_path, revnum,
                     boost::make_function_output_iterator(
                         [&](Rule const* r){ delete_svn_path(r->svn_path(), r); }));
            }
        }
        else // all the other change kinds can be treated the same
        {
            // Git doesn't care about directory changes
            if (change->node_kind != svn_node_dir)
                svn_paths_to_rewrite.insert(std::move(svn_path));
        }
    }
}

void importer::map_svn_paths_to_git(svn::revision const& rev)
{
    for (auto& svn_path : svn_paths_to_rewrite)
    {
        svn_node_kind_t kind
            = svn::call(svn_fs_check_path, rev.fs_root, svn_path.c_str(), rev.pool);

        if (kind == svn_node_none) 
            continue;

        // we simply don't know how to deal with this case.  Let's
        // hope it never happens
        assert(kind != svn_node_unknown); 
    }
}

void importer::import_revision(int revnum)
{
    ((revnum % 1000) ? Log::trace() : Log::info())
        << "importing revision " << revnum << std::endl;

    this->revnum = revnum;
    svn_paths_to_rewrite.clear();
    changed_repositories.clear();

    // Deal with rules becoming active/inactive in this revision
    for (Rule const* r: ruleset.matches().rules_in_transition(revnum))
        rewrite_svn_tree(r->svn_path(), r);

    // Discover SVN paths that are being deleted/modified
    svn::revision rev = svn_repository[revnum];
    process_svn_changes(rev);

    Log::trace() 
        << svn_paths_to_rewrite.size() 
        << " SVN " 
        << (svn_paths_to_rewrite.size() == 1 ? "path" : "paths")
        << " to rewrite" << std::endl;
    
    // Connect each file path in SVN with a corresponding path in a
    // Git ref.
    map_svn_paths_to_git(rev);

    for (auto repo : changed_repositories)
        repo->write_changes();
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
