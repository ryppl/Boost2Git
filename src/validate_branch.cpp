/*
 *  Copyright (C) 2013 Daniel Pfeifer <daniel@pfeifer-mail.de>
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

#include "apr_init.hpp"
#include "apr_pool.hpp"

#define SVN_DEPRECATED
#include <svn_fs.h>
#include <svn_repos.h>
#include "svn_error.hpp"

#include <cstdlib>
#include <string>
#include <iostream>

class Repository
  {
  public:
    Repository(char const* path)
      {
      svn_repos_t *repos;
      check_svn(svn_repos_open(&repos, path, apr_pool));
      fs = svn_repos_fs(repos);
      svn_revnum_t revnum;
      check_svn(svn_fs_youngest_rev(&revnum, fs, apr_pool));
      check_svn(svn_fs_revision_root(&fs_root, fs, revnum, apr_pool));
      }
    void set_revision(svn_revnum_t revnum)
      {
      if (revnum == 0)
        {
        check_svn(svn_fs_youngest_rev(&revnum, fs, apr_pool));
        }
      check_svn(svn_fs_revision_root(&fs_root, fs, revnum, apr_pool));
      }
    bool is_dir(std::string const& path) const
      {
      svn_boolean_t result;
      check_svn(svn_fs_is_dir(&result, fs_root, path.c_str(), apr_pool));
      return result;
      }
  private:
    AprInit apr_init;
    AprPool apr_pool;
    svn_fs_t *fs;
    svn_fs_root_t *fs_root;
  };

std::string test_branch(Repository& repo, std::string const& line)
  {
  std::size_t r1 = line.find('[') + 1;
  std::size_t r2 = line.find(':', r1);
  std::string revnum = line.substr(r1, r2 - r1);
  repo.set_revision(atoi(revnum.c_str()));

  std::size_t c1 = line.find('"') + 1;
  std::size_t c2 = line.find('"', c1);
  std::string path = line.substr(c1, c2 - c1);
  if (!repo.is_dir(path))
    {
    return line; // Not guaranteed!
    }
  if (repo.is_dir(path + "boost") && repo.is_dir(path + "libs"))
    {
    return line;
    }
  if (repo.is_dir(path + "boost/boost") && repo.is_dir(path + "boost/libs"))
    {
    return line.substr(0, c2) + "boost/" + line.substr(c2);
    }
  return line;
  //return "//" + line.substr(2) + " // not a common branch!";
  }

int main(int argc, char* argv[])
  {
  if (argc < 2)
    {
    std::cerr << "Insufficient arguments!" << std::endl;
    return -1;
    }
  Repository repo(argv[1]);

  std::string line;
  while (std::getline(std::cin, line))
    {
    if (line.empty())
      {
      continue;
      }
    std::cout << test_branch(repo, line) << std::endl;
    }
  return 0;
  }
