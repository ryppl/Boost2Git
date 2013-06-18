// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef GIT_REPOSITORY_DWA2013614_HPP
# define GIT_REPOSITORY_DWA2013614_HPP

# include "git_fast_import.hpp"
# include <boost/container/flat_map.hpp>
# include "path_set.hpp"
# include <unordered_map>
# include <boost/container/flat_set.hpp>

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
        typedef boost::container::flat_map<std::string, std::string> path_map;
        
        std::string name;
        rev_mark_map marks;
        path_set pending_deletions;
        path_map pending_translations;
    };

    ref& modify_ref(std::string const& name) 
    {
        auto p = refs.emplace(name, name).first;
        modified_refs.insert(&p->second);
        return p->second;
    }

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
    ref* in_progress; // The fast-import process is currently writing to this ref
};

#endif // GIT_REPOSITORY_DWA2013614_HPP
