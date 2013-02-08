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

#include "svn.h"
#include "apr_pool.hpp"
#include "svn_revision.hpp"

#include <svn_fs.h>
#include <svn_repos.h>

#include <QDebug>

class SvnPrivate
  {
  public:
    QList<MatchRuleList> allMatchRules;
    RepositoryHash repositories;
    IdentityHash identities;
    QString userdomain;

    SvnPrivate(const QString &pathToRepository);
    ~SvnPrivate();
    int youngestRevision();
    int exportRevision(int revnum);

    int openRepository(const QString &pathToRepository);

  private:
    AprPool global_pool;
    svn_fs_t *fs;
    svn_revnum_t youngest_rev;
  };

void Svn::initialize()
  {
  if (apr_initialize() != APR_SUCCESS)
    {
    fprintf(stderr, "You lose at apr_initialize().\n");
    exit(1);
    }
  // static destructor
  static struct Destructor
    {
    ~Destructor()
      {
      apr_terminate();
      }
    } destructor;
  }

Svn::Svn(const QString &pathToRepository)
    : d(new SvnPrivate(pathToRepository))
  {
  }

Svn::~Svn()
  {
  delete d;
  }

void Svn::setMatchRules(const QList<MatchRuleList> &allMatchRules)
  {
  d->allMatchRules = allMatchRules;
  }

void Svn::setRepositories(const RepositoryHash &repositories)
  {
  d->repositories = repositories;
  }

void Svn::setIdentityMap(const IdentityHash &identityMap)
  {
  d->identities = identityMap;
  }

void Svn::setIdentityDomain(const QString &identityDomain)
  {
  d->userdomain = identityDomain;
  }

int Svn::youngestRevision()
  {
  return d->youngestRevision();
  }

bool Svn::exportRevision(int revnum)
  {
  return d->exportRevision(revnum) == EXIT_SUCCESS;
  }

SvnPrivate::SvnPrivate(const QString &pathToRepository) : global_pool(NULL)
  {
  if (openRepository(pathToRepository) != EXIT_SUCCESS)
    {
    qCritical() << "Failed to open repository";
    exit(1);
    }
  // get the youngest revision
  svn_fs_youngest_rev(&youngest_rev, fs, global_pool);
  }

SvnPrivate::~SvnPrivate()
  {
  svn_pool_destroy(global_pool);
  }

int SvnPrivate::youngestRevision()
  {
  return youngest_rev;
  }

int SvnPrivate::openRepository(const QString &pathToRepository)
  {
  svn_repos_t *repos;
  QString path = pathToRepository;
  while (path.endsWith('/')) // no trailing slash allowed
    {
    path = path.mid(0, path.length() - 1);
    }
  SVN_ERR(svn_repos_open(&repos, QFile::encodeName(path), global_pool));
  fs = svn_repos_fs(repos);
  return EXIT_SUCCESS;
  }

int SvnPrivate::exportRevision(int revnum)
  {
  SvnRevision rev(revnum, fs, global_pool);
  rev.allMatchRules = allMatchRules;
  rev.repositories = repositories;
  rev.identities = identities;
  rev.userdomain = userdomain;

  // open this revision:
  printf("Exporting revision %d ", revnum);
  fflush (stdout);

  if (rev.open() == EXIT_FAILURE)
    {
    return EXIT_FAILURE;
    }
  if (rev.prepareTransactions() == EXIT_FAILURE)
    {
    return EXIT_FAILURE;
    }
  if (!rev.needCommit)
    {
    printf(" nothing to do\n");
    return EXIT_SUCCESS; // no changes?
    }
  if (rev.commit() == EXIT_FAILURE)
    {
    return EXIT_FAILURE;
    }
  printf(" done\n");
  return EXIT_SUCCESS;
  }
