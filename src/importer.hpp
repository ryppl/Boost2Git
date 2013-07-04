// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef IMPORTER_DWA2013614_HPP
# define IMPORTER_DWA2013614_HPP

# include "git_repository.hpp"
# include "path_set.hpp"
# include "svn.hpp"
# include "path.hpp"

# include <boost/container/flat_set.hpp>
# include <boost/container/flat_map.hpp>
# include <map>

struct Rule;
struct Ruleset;
struct svn_fs_path_change2_t;

struct importer
{
    importer(svn const& svn_repo, Ruleset const& rules);
    ~importer();

    int last_valid_svn_revision();
    void import_revision(int revnum);

 private: // helpers
    git_repository* demand_repo(std::string const& name);
    git_repository::ref* prepare_to_modify(Rule const* match, bool discover_changes);
    void process_svn_changes(svn::revision const& rev);
    void process_svn_directory_change(
        svn::revision const& rev, svn_fs_path_change2_t *change, path const& svn_path);
    path add_svn_tree_to_delete(path const& svn_path, Rule const* match);
    void invalidate_svn_tree(
        svn::revision const& rev, path const& svn_path, Rule const* match);
    void add_svn_tree_to_convert(
        svn::revision const& rev, path const& svn_path);
    void convert_svn_tree(
        svn::revision const& rev, path const& svn_path, bool discover_changes);
    void convert_svn_file(
        svn::revision const& rev, path const& svn_path, bool discover_changes);

 private: // persistent members
    std::map<std::string, git_repository> repositories;
    svn const& svn_repository;
    Ruleset const& ruleset;

 private: // members used per SVN revision
    int revnum;
    path_set svn_paths_to_convert;
    boost::container::flat_set<git_repository*> changed_repositories;

    // A map from destination directory to (source revision, directory) pairs
    boost::container::flat_map<path, std::pair<std::size_t, path> > svn_directory_copies;
};

#endif // IMPORTER_DWA2013614_HPP
