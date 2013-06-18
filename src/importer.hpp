// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef IMPORTER_DWA2013614_HPP
# define IMPORTER_DWA2013614_HPP

# include "git_repository.hpp"
# include "path_set.hpp"
# include "svn.hpp"

# include <boost/container/flat_set.hpp>
# include <map>

struct Rule;
struct Ruleset;

struct importer
{
    importer(svn const& svn_repo, Ruleset const& rules);
    ~importer();

    int last_valid_svn_revision();
    void import_revision(int revnum);

 private: // helpers
    git_repository* demand_repo(std::string const& name);
    git_repository& modify_repo(std::string const& name);
    void process_svn_changes(svn::revision const& rev);
    void map_svn_paths_to_git(svn::revision const& rev);
    std::string add_svn_tree_to_delete(std::string const& svn_path, Rule const* match);
    void add_svn_tree_to_rewrite(std::string const& svn_path, Rule const* match);
    void rewrite_svn_tree(std::string const& svn_path);

 private: // persistent members
    std::map<std::string, git_repository> repositories;
    svn const& svn_repository;
    Ruleset const& ruleset;

 private: // members used per SVN revision
    int revnum;
    path_set svn_paths_to_rewrite;
    boost::container::flat_set<git_repository*> changed_repositories;
};

#endif // IMPORTER_DWA2013614_HPP
