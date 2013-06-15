// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef GIT_REPOSITORY_DWA2013614_HPP
# define GIT_REPOSITORY_DWA2013614_HPP

# include "git_fast_import.hpp"

struct git_repository
{
    explicit git_repository(std::string const& git_dir);

 private:
    static bool ensure_existence(std::string const& git_dir);

    bool created;
    git_fast_import fast_import;
};

#endif // GIT_REPOSITORY_DWA2013614_HPP
