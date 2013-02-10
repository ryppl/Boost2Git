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

#ifndef SVN_REVISION_HPP
#define SVN_REVISION_HPP

#include <svn_fs.h>

#include <QByteArray>
#include <QHash>

#include "apr_pool.hpp"
#include "options.hpp"
#include "repository.h"
#include "svn_error.hpp"

typedef std::vector<Ruleset::Match> MatchRuleList;
typedef QHash<QString, Repository*> RepositoryHash;

class Svn;

class SvnRevision
  {
  public:
    SvnRevision(Svn const& svn, int revision, svn_fs_t *f, apr_pool_t *parent_pool) :
        svn(svn), pool(parent_pool), fs(f), fs_root(0), revnum(revision), propsFetched(false)
      {
      ruledebug = options.debug_rules;
      }
    void open()
      {
      check_svn(svn_fs_revision_root(&fs_root, fs, revnum, pool));
      }
    int prepareTransactions();
    void commit();
  private:
    void fetchRevProps();
    int exportEntry(
        const char *path,
        const svn_fs_path_change_t *change,
        apr_hash_t *changes);
    int exportDispatch(
        const char *path,
        const svn_fs_path_change_t *change,
        const char *path_from,
        svn_revnum_t rev_from,
        apr_hash_t *changes,
        const QString &current,
        const Ruleset::Match &rule,
        const MatchRuleList &matchRules,
        apr_pool_t *pool);
    int exportInternal(
        const char *path,
        const svn_fs_path_change_t *change,
        const char *path_from,
        svn_revnum_t rev_from,
        const QString &current,
        const Ruleset::Match &rule,
        const MatchRuleList &matchRules);
    int recurse(
        const char *path,
        const svn_fs_path_change_t *change,
        const char *path_from,
        const MatchRuleList &matchRules,
        svn_revnum_t rev_from,
        apr_hash_t *changes,
        apr_pool_t *pool);
  private:
    Svn const& svn;
    AprPool pool;
    QHash<QString, Repository::Transaction*> transactions;

    svn_fs_t *fs;
    svn_fs_root_t *fs_root;
    int revnum;

    // must call fetchRevProps first:
    std::string author;
    std::string log;
    uint epoch;

    bool ruledebug;
    bool propsFetched;
    bool needCommit;
  };

#endif /* SVN_REVISION_HPP */
