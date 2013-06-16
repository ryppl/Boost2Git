// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef GIT_REPOSITORY_DWA2013614_HPP
# define GIT_REPOSITORY_DWA2013614_HPP

# include "git_fast_import.hpp"

struct git_repository
{
    explicit git_repository(std::string const& git_dir);
    void set_super_module(git_repository* super_module, std::string const& submodule_path);
    
    void close_fast_import() { fast_import.close(); }

 private:
    void read_logfile();
    
    static bool ensure_existence(std::string const& git_dir);

    bool created;
    git_fast_import fast_import;
    git_repository* super_module;
    std::string submodule_path;
};

#endif // GIT_REPOSITORY_DWA2013614_HPP
