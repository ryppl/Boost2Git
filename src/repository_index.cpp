// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include "repository_index.hpp"
#include "ruleset.hpp"
#include <boost/range/adaptor/map.hpp>
#include "svn.hpp"
#include "log.hpp"

using namespace boost::adaptors;

repository_index::repository_index(Ruleset const& ruleset)
{
    for(auto const& rule : ruleset.repositories())
    {
        git_repository* repo = demand_repo(rule.name);

        repo->set_super_module( 
            demand_repo(rule.submodule_in_repo), rule.submodule_path);
    }
}

inline git_repository*
repository_index::demand_repo(std::string const& name)
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

int repository_index::last_valid_svn_revision()
{
    return 0; // pessimization for now.  Later we should read marks files, etc.
}

void repository_index::import_revision(svn const& svn_repository, int revnum, Ruleset const& ruleset)
{
    if (revnum % 1000 == 0) {
        Log::info() << "importing revision " << revnum << std::endl;
    }
}
