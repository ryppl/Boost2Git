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

void importer::import_revision(int revnum)
{
    ((revnum % 1000) ? Log::trace() : Log::info())
        << "importing revision " << revnum << std::endl;

    invalid_svn_paths.clear();
    changed_repositories.clear();

    for (Rule const* r: ruleset.matches().rules_in_transition(revnum))
    {
        Log::trace() << *r << " is in transition" << std::endl;
        
        // This rule's Git path prefix will need to be deleted 
        auto& repo = repositories.find(r->git_repo_name())->second;
        repo.modify_ref(r->git_ref_name())
            .pending_deletions.insert(r->git_path());
        changed_repositories.insert(&repo);

        // Invalidate SVN paths
        int revnum_to_scan = revnum - (r->max < revnum ? 1 : 0);
        ruleset.matches().reverse_matches(
            r->git_address(), revnum_to_scan,
            boost::make_function_output_iterator(
                [&](Rule const* r){ invalid_svn_paths.insert(r->svn_path()); }));
    }

    svn::revision rev = svn_repository[revnum];

    // invalidate all paths changed in this revision
    apr_hash_t *changes = svn::call(svn_fs_paths_changed2, rev.fs_root, rev.pool);
    for (apr_hash_index_t *i = apr_hash_first(rev.pool, changes); i; i = apr_hash_next(i))
    {
        const char *key = 0;
        svn_fs_path_change2_t *change = 0;
        apr_hash_this(i, (const void**) &key, nullptr, (void**) &change);
        // According to the docs, this means the hash entry was
        // deleted, so it should never happen
        assert(change != nullptr); 
        // change is one of {modify, add, delete, replace}
    }
    
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
