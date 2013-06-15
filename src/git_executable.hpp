// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef GIT_EXECUTABLE_DWA2013614_HPP
# define GIT_EXECUTABLE_DWA2013614_HPP

# include <boost/process.hpp>
# include <string>

inline std::string const& git_executable()
{
    static std::string git_exe = boost::process::search_path("git");
    return git_exe;
}

#endif // GIT_EXECUTABLE_DWA2013614_HPP
