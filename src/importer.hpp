// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef IMPORTER_DWA2013614_HPP
# define IMPORTER_DWA2013614_HPP

# include "git_repository.hpp"
# include <map>

struct Ruleset;
struct svn;

struct importer
{
    importer(svn const& svn_repo, Ruleset const& rules);
    ~importer();

    int last_valid_svn_revision();
    void import_revision(int revnum);

 private: // helpers
    git_repository* demand_repo(std::string const& name);

 private: // members
    std::map<std::string, git_repository> repositories;
    svn const& svn_repository;
    Ruleset const& ruleset;
};

#endif // IMPORTER_DWA2013614_HPP
