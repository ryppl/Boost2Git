// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef GIT_REPOSITORY_DWA2013614_HPP
# define GIT_REPOSITORY_DWA2013614_HPP

# include "git_fast_import.hpp"
# include "path_set.hpp"
# include "path.hpp"
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
        ref(std::string const& name, git_repository* repo) 
            : name(name), repo(repo) 
            , super_module_ref(
                repo->super_module ? repo->super_module->demand_ref(name) : nullptr
            )
            , submodule_refs_written(0)
            , gitattributes_outdated(!options.gitattributes_text.empty())
        {}

        typedef boost::container::flat_map<std::size_t, std::size_t> rev_mark_map;

        // Maps a Git ref into an SVN revision from that ref that has
        // been merged into this ref.
        typedef boost::container::flat_map<ref const*, std::size_t> merge_map;

        // An open ref in a super-module can be committed only when every
        // participating submodule ref has been committed.
        bool can_close()
        {
            return submodule_refs_written == changed_submodule_refs.size();
        }

        std::string name;
        git_repository* repo;
        ref* super_module_ref;
        rev_mark_map marks;
        merge_map merged_revisions;
        merge_map pending_merges;
        path_set pending_deletions;
        // Submodule refs included in the previous commit
        boost::container::flat_set<ref const*> submodule_refs;
        // Submodule refs modified in the current commit
        boost::container::flat_set<ref const*> changed_submodule_refs;
        // How many of the changed submodule refs have been written in this commit
        unsigned submodule_refs_written;
        // Submodule refs that need to be refreshed: the submodule
        // itself may not have changed but the part of the
        // super-module where it lives is being rewritten.
        boost::container::flat_set<ref const*> stale_submodule_refs;
        std::string head_tree_sha;
        bool gitattributes_outdated;
    };

    ref* demand_ref(std::string const& name)
    {
        auto p = refs.emplace(name, ref(name, this)).first;
        return &p->second;
    }

    ref* modify_ref(std::string const& name, bool allow_discovery = true);

    // Begins a commit; returns the ref currently being written.
    ref* open_commit(svn::revision const& rev);

    void prepare_to_close_commit(); 

    // Returns true iff there are no further commits to make in this
    // repository for this SVN revision.
    bool close_commit(); 

    std::string const& name() const { return git_dir; }

    // Remember that the given ref is a descendant of the named source
    // ref at the given SVN revision
    void record_ancestor(ref* descendant, std::string const& src_ref_name, std::size_t revnum);

    git_repository* in_super_module() const { return super_module; }

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
    path submodule_path;

    // branches and tags
    std::unordered_map<std::string, ref> refs;
    boost::container::flat_set<ref*> modified_refs; // to be written in current revision

    int last_mark;       // The last commit mark written to fast-import
    ref* current_ref;    // The ref to which the fast-import process is currently writing
    
    // Whether or not we've sent the "ls" command to git fast-import
    bool prepared_to_close_commit;
};

#endif // GIT_REPOSITORY_DWA2013614_HPP
