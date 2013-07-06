// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef GIT_REPOSITORY_DWA2013614_HPP
# define GIT_REPOSITORY_DWA2013614_HPP

# include "git_fast_import.hpp"
# include "path_set.hpp"
# include "svn.hpp"
# include <boost/container/flat_map.hpp>
# include <boost/container/flat_set.hpp>
# include <unordered_map>

struct git_repository
{
    explicit git_repository(std::string const& git_dir);
    void set_super_module(git_repository* super_module, std::string const& submodule_path);
    
    git_fast_import& fast_import() { return fast_import_; }

    // A branch or tag
    struct ref
    {
        ref(std::string name, git_repository* repo) 
            : name(std::move(name)), repo(repo) {}

        typedef boost::container::flat_map<std::size_t, std::size_t> rev_mark_map;

        // Maps a Git ref into an SVN revision from that ref that has
        // been merged into this ref.
        typedef boost::container::flat_map<ref const*, std::size_t> merge_map;

        std::string name;
        git_repository* repo;
        rev_mark_map marks;
        merge_map merged_revisions;
        merge_map pending_merges;
        path_set pending_deletions;
        std::string head_tree_sha;
    };

    ref* demand_ref(std::string const& name)
    {
        auto p = refs.emplace(name, ref(name, this)).first;
        return &p->second;
    }

    ref* modify_ref(std::string const& name, bool allow_discovery = true);

    // Begins a commit; returns the ref currently being written.
    ref* open_commit(svn::revision const& rev);

    // Returns true iff there are no further commits to make in this
    // repository for this SVN revision.
    bool close_commit(bool discover_changes); 

    std::string const& name() { return git_dir; }

    // Remember that the given ref is a descendant of the named source
    // ref at the given SVN revision
    void record_ancestor(ref* descendant, std::string const& src_ref_name, std::size_t revnum);

    bool has_submodules() const { return _has_submodules; }

 private:
    void read_logfile();
    static bool ensure_existence(std::string const& git_dir);
    void write_merges();

 private: // data members
    // Relative path to the repository from the current working
    // directory.  Also the repository's name
    std::string git_dir; 

    // This is just a place to hang a constructor initializer, that
    // ensures the repository is created before the git fast-import
    // process (next member) is started.
    bool created;

    // The process through which we write this Git repository
    git_fast_import fast_import_;

    // If this is a submodule, of whom and were?
    git_repository* super_module;
    std::string submodule_path;
    bool _has_submodules;
    // How many refs need to be written in submodules of this
    // repository before we can write this repo's changes?
    unsigned modified_submodule_refs; 

    // branches and tags
    std::unordered_map<std::string, ref> refs;
    boost::container::flat_set<ref*> modified_refs; // to be written in current revision

    int last_mark;       // The last commit mark written to fast-import
    ref* current_ref;    // The ref to which the fast-import process is currently writing
    
};

#endif // GIT_REPOSITORY_DWA2013614_HPP
