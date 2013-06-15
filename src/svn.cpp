/*
 *  Copyright (C) 2007  Thiago Macieira <thiago@kde.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Based on svn-fast-export by Chris Lee <clee@kde.org>
 * License: MIT <http://www.opensource.org/licenses/mit-license.php>
 * URL: git://repo.or.cz/fast-import.git http://repo.or.cz/w/fast-export.git
 */

// Apparently some builds of libsvn_repos/libsvn_ra_local (like the
// one that comes with MacOS 10.8) don't have svn_repos_open2, so we
// use the deprecated svn_repos_open instead.
#define SVN_DEPRECATED

#include "svn.hpp"
#include "svn_error.hpp"
#include "apr_pool.hpp"

// Call an svn function with proper error reporting
template <class R, class...P, class...A>
R call(svn_error_t* (*f)(R*, P...), A const& ...args)
{
    R result;
    check_svn(f(&result, args...));
    return result;
}

svn::svn(
    std::string const& repo_path,
    std::string const& authors_file_path)
    : global_pool(NULL), 
      repos(call(svn_repos_open, repo_path.c_str(), global_pool)),
      fs(svn_repos_fs(repos)),
      authors(authors_file_path)
{
}

svn::~svn()
{}

int svn::latest_revision() const
{
    return call(svn_fs_youngest_rev, fs, global_pool);
}
