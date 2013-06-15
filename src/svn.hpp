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

#ifndef SVN_DWA2013615_HPP
# define SVN_DWA2013615_HPP

#include <svn_fs.h>
#include <svn_repos.h>

#include "apr_pool.hpp"
#include "authors.hpp"
#include <string>

class Authors;

class svn
{
 public:
    svn(std::string const& repo_path, 
        std::string const& authors_file_path);
    ~svn();

    int latest_revision() const;

 private:
    AprPool global_pool;

 public:
    svn_repos_t* repos;
    svn_fs_t* fs;
    Authors authors;
};

#endif
