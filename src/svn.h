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

#ifndef SVN_H
#define SVN_H

#include <QHash>
#include <QString>

#include <svn_fs.h>
#include <svn_repos.h>

#include "apr_pool.hpp"
#include "rules.hpp"

class Repository;

class Svn
  {
  public:
    Svn(std::string const& repo_path);
    ~Svn();

    void setMatchRules(const QList<QList<Rules::Match> > &matchRules);
    void setRepositories(const QHash<QString, Repository*> &repositories);
    void setIdentityMap(const QHash<QByteArray, QByteArray> &identityMap);

    int youngestRevision();
    bool exportRevision(int revnum);

  private:
    AprPool global_pool;
    svn_fs_t *fs;
    svn_revnum_t youngest_rev;

    QList<QList<Rules::Match> > allMatchRules;
    QHash<QString, Repository*> repositories;
    QHash<QByteArray, QByteArray> identities;
  };

#endif
