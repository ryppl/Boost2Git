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

#define SVN_DEPRECATED
#include "svn.h"
#include "apr_pool.hpp"
#include "svn_revision.hpp"

#include <svn_fs.h>
#include <svn_repos.h>

#include <iostream>

Svn::Svn(
    std::string const& repo_path,
    Authors const& authors,
    Ruleset const& ruleset)
    : global_pool(NULL), authors(authors), ruleset(ruleset)
  {
  try
    {
    svn_repos_t *repos;
    check_svn(svn_repos_open(&repos, repo_path.c_str(), global_pool));
    fs = svn_repos_fs(repos);
    check_svn(svn_fs_youngest_rev(&youngest_rev, fs, global_pool));
    }
  catch (...)
    {
    svn_pool_destroy(global_pool);
    }
  }

Svn::~Svn()
  {
  svn_pool_destroy(global_pool);
  }

void Svn::setRepositories(const QHash<QString, Repository*> &repositories)
  {
  this->repositories = repositories;
  }

int Svn::youngestRevision()
  {
  return youngest_rev;
  }

bool Svn::exportRevision(int revnum)
  {
  SvnRevision rev(*this, revnum, fs, global_pool);

  rev.open();
  if (rev.prepareTransactions() == EXIT_FAILURE)
    {
    return false;
    }
  rev.commit();

  return true;
  }
