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
        ref(std::string name) 
            : name(std::move(name)) {}

        typedef boost::container::flat_map<std::size_t, std::size_t> rev_mark_map;
        
        std::string name;
        rev_mark_map marks;
        path_set pending_deletions;
    };

    ref* modify_ref(std::string const& name, bool allow_discovery = true) 
    {
        auto p = refs.emplace(name, name).first;
        if (!allow_discovery && modified_refs.count(&p->second) == 0)
            return nullptr;
        modified_refs.insert(&p->second);
        return &p->second;
    }

    // Begins a commit
    void open_commit(svn::revision const& rev);

    // Returns true iff there are no further commits to make in this
    // repository for this SVN revision.
    bool close_commit(); 

 private:
    void read_logfile();
    static bool ensure_existence(std::string const& git_dir);

 private: // data members
    std::string git_dir;
    bool created;
    git_fast_import fast_import_;
    git_repository* super_module;
    std::string submodule_path;
    std::unordered_map<std::string, ref> refs;
    boost::container::flat_set<ref*> modified_refs;
    int last_mark;       // The last commit mark written to fast-import
    ref* current_ref;    // The fast-import process is currently writing to this ref
};

#endif // GIT_REPOSITORY_DWA2013614_HPP
