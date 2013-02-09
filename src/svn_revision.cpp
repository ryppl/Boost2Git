/*
 *  Copyright (C) 2007  Thiago Macieira <thiago@kde.org>
 *  Copyright (C) 2013  Daniel Pfeifer <daniel@pfeifer-mail.de>
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

#define SVN_DEPRECATED
#include "svn_revision.hpp"
#include "authors.hpp"
#include "svn.h"

#include <QDebug>
#include <boost/date_time/posix_time/time_parsers.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>

#include <svn_fs.h>
#include <svn_pools.h>
#include <svn_repos.h>
#include <svn_types.h>

namespace
{

enum RuleType { AnyRule = 0, NoIgnoreRule = 0x01, NoRecurseRule = 0x02 };

MatchRuleList::ConstIterator findMatchRule(
    const MatchRuleList &matchRules,
    int revnum,
    const QString &current,
    int ruleMask = AnyRule)
  {
  MatchRuleList::ConstIterator it = matchRules.constBegin(), end = matchRules.constEnd();
  for (; it != end; ++it)
    {
        if (it->minRevision > revnum)
            continue;
        if (it->maxRevision != -1 && it->maxRevision < revnum)
            continue;
        if (it->action == Rules::Match::Ignore && ruleMask & NoIgnoreRule)
            continue;
        if (it->action == Rules::Match::Recurse && ruleMask & NoRecurseRule)
            continue;
        if (it->rx.indexIn(current) == 0) {
            return it;
        }
    }
    return end;
}

void splitPathName(
    const Rules::Match &rule,
    const QString &pathName,
    QString *svnprefix_p,
    QString *repository_p,
    QString *branch_p,
    QString *path_p)
  {
    QString svnprefix = pathName;
    svnprefix.truncate(rule.rx.matchedLength());

    if (svnprefix_p) {
        *svnprefix_p = svnprefix;
    }

    if (repository_p) {
        *repository_p = svnprefix;
        repository_p->replace(rule.rx, rule.repository);
        foreach (Rules::Match::Substitution subst, rule.repo_substs) {
            subst.apply(*repository_p);
        }
    }

    if (branch_p) {
        *branch_p = svnprefix;
        branch_p->replace(rule.rx, rule.branch);
        foreach (Rules::Match::Substitution subst, rule.branch_substs) {
            subst.apply(*branch_p);
        }
    }

    if (path_p) {
        QString prefix = svnprefix;
        prefix.replace(rule.rx, rule.prefix);
        *path_p = prefix + pathName.mid(svnprefix.length());
    }
}

int pathMode(svn_fs_root_t *fs_root, const char *pathname, apr_pool_t *pool)
  {
    svn_string_t *propvalue;
    check_svn(svn_fs_node_prop(&propvalue, fs_root, pathname, "svn:executable", pool));
    int mode = 0100644;
    if (propvalue)
        mode = 0100755;

    return mode;
}

svn_error_t *QIODevice_write(void *baton, const char *data, apr_size_t *len)
{
    QIODevice *device = reinterpret_cast<QIODevice *>(baton);
    device->write(data, *len);

    while (device->bytesToWrite() > 32*1024) {
        if (!device->waitForBytesWritten(-1)) {
            qFatal("Failed to write to process: %s", qPrintable(device->errorString()));
            return svn_error_createf(APR_EOF, SVN_NO_ERROR, "Failed to write to process: %s",
                                     qPrintable(device->errorString()));
        }
    }
    return SVN_NO_ERROR;
}

svn_stream_t *streamForDevice(QIODevice *device, apr_pool_t *pool)
{
    svn_stream_t *stream = svn_stream_create(device, pool);
    svn_stream_set_write(stream, QIODevice_write);

    return stream;
}

int dumpBlob(
    Repository::Transaction *txn,
    svn_fs_root_t *fs_root,
    const char *pathname,
    const QString &finalPathName,
    apr_pool_t *pool)
{
    AprPool dumppool(pool);
    // what type is it?
    int mode = pathMode(fs_root, pathname, dumppool);

    svn_filesize_t stream_length;

    check_svn(svn_fs_file_length(&stream_length, fs_root, pathname, dumppool));

    svn_stream_t *in_stream, *out_stream;
    if (!options.dry_run) {
        // open the file
        check_svn(svn_fs_file_contents(&in_stream, fs_root, pathname, dumppool));
    }

    // maybe it's a symlink?
    svn_string_t *propvalue;
    check_svn(svn_fs_node_prop(&propvalue, fs_root, pathname, "svn:special", dumppool));
    if (propvalue) {
        apr_size_t len = strlen("link ");
        if (!options.dry_run) {
            QByteArray buf;
            buf.reserve(len);
            check_svn(svn_stream_read(in_stream, buf.data(), &len));
            if (len == strlen("link ") && strncmp(buf, "link ", len) == 0) {
                mode = 0120000;
                stream_length -= len;
            } else {
                //this can happen if a link changed into a file in one commit
                qWarning("file %s is svn:special but not a symlink", pathname);
                // re-open the file as we tried to read "link "
                svn_stream_close(in_stream);
                check_svn(svn_fs_file_contents(&in_stream, fs_root, pathname, dumppool));
            }
        }
    }

    QIODevice *io = txn->addFile(finalPathName, mode, stream_length);

    if (!options.dry_run) {
        // open a generic svn_stream_t for the QIODevice
        out_stream = streamForDevice(io, dumppool);
        check_svn(svn_stream_copy(in_stream, out_stream, dumppool));
        svn_stream_close(out_stream);
        svn_stream_close(in_stream);

        // print an ending newline
        io->putChar('\n');
    }

    return EXIT_SUCCESS;
}

int recursiveDumpDir(
    Repository::Transaction *txn,
    svn_fs_root_t *fs_root,
    const QByteArray &pathname,
    const QString &finalPathName,
    apr_pool_t *pool)
{
    // get the dir listing
    apr_hash_t *entries;
    check_svn(svn_fs_dir_entries(&entries, fs_root, pathname, pool));
    AprPool dirpool(pool);

    // While we get a hash, put it in a map for sorted lookup, so we can
    // repeat the conversions and get the same git commit hashes.
    QMap<QByteArray, svn_node_kind_t> map;
    for (apr_hash_index_t *i = apr_hash_first(pool, entries); i; i = apr_hash_next(i)) {
        const void *vkey;
        void *value;
        apr_hash_this(i, &vkey, NULL, &value);
        svn_fs_dirent_t *dirent = reinterpret_cast<svn_fs_dirent_t *>(value);
        map.insertMulti(QByteArray(dirent->name), dirent->kind);
    }

    QMapIterator<QByteArray, svn_node_kind_t> i(map);
    while (i.hasNext()) {
        dirpool.clear();
        i.next();
        QByteArray entryName = pathname + '/' + i.key();
        QString entryFinalName = finalPathName + QString::fromUtf8(i.key());

        if (i.value() == svn_node_dir) {
            entryFinalName += '/';
            if (recursiveDumpDir(txn, fs_root, entryName, entryFinalName, dirpool) == EXIT_FAILURE)
                return EXIT_FAILURE;
        } else if (i.value() == svn_node_file) {
            if (dumpBlob(txn, fs_root, entryName, entryFinalName, dirpool) == EXIT_FAILURE)
                return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

bool wasDir(svn_fs_t *fs, int revnum, const char *pathname, apr_pool_t *pool)
{
    AprPool subpool(pool);
    svn_fs_root_t *fs_root;
    if (svn_fs_revision_root(&fs_root, fs, revnum, subpool) != SVN_NO_ERROR)
        return false;

    svn_boolean_t is_dir;
    if (svn_fs_is_dir(&is_dir, fs_root, pathname, subpool) != SVN_NO_ERROR)
        return false;

    return is_dir;
}

} // namespace

int SvnRevision::prepareTransactions()
{
    // find out what was changed in this revision:
    apr_hash_t *changes;
    check_svn(svn_fs_paths_changed(&changes, fs_root, pool));

    QMap<QByteArray, svn_fs_path_change_t*> map;
    for (apr_hash_index_t *i = apr_hash_first(pool, changes); i; i = apr_hash_next(i)) {
        const void *vkey;
        void *value;
        apr_hash_this(i, &vkey, NULL, &value);
        const char *key = reinterpret_cast<const char *>(vkey);
        svn_fs_path_change_t *change = reinterpret_cast<svn_fs_path_change_t *>(value);
        // If we mix path deletions with path adds/replaces we might erase a
        // branch after that it has been reset -> history truncated
        if (map.contains(QByteArray(key))) {
            // If the same path is deleted and added, we need to put the
            // deletions into the map first, then the addition.
            if (change->change_kind == svn_fs_path_change_delete) {
                // XXX
            }
            fprintf(stderr, "\nDuplicate key found in rev %d: %s\n", revnum, key);
            fprintf(stderr, "This needs more code to be handled, file a bug report\n");
            fflush(stderr);
            exit(1);
        }
        map.insertMulti(QByteArray(key), change);
    }

    QMapIterator<QByteArray, svn_fs_path_change_t*> i(map);
    while (i.hasNext()) {
        i.next();
        if (exportEntry(i.key(), i.value(), changes) == EXIT_FAILURE)
            return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static std::string get_string(apr_hash_t *revprops, char const *key)
  {
  std::string result;
  svn_string_t *str = (svn_string_t*) apr_hash_get(revprops, key, APR_HASH_KEY_STRING);
  if (str && !svn_string_isempty(str))
    {
    result.append(str->data, str->len);
    }
  return result;
  }

int SvnRevision::fetchRevProps()
  {
  if (propsFetched)
    {
    return EXIT_SUCCESS;
    }
  apr_hash_t *revprops;
  check_svn(svn_fs_revision_proplist(&revprops, fs, revnum, pool));

  author = svn.authors[get_string(revprops, "svn:author")];
  if (author.empty())
    {
    author = "nobody <nobody@localhost>";
    }

  epoch = 0;
  std::string svndate = get_string(revprops, "svn:date");
  if (!svndate.empty())
    {
    namespace dt = boost::date_time;
    namespace pt = boost::posix_time;
    pt::ptime ptime = dt::parse_delimited_time<pt::ptime>(svndate, 'T');
    static pt::ptime epoch_(boost::gregorian::date(1970, 1, 1));
    epoch = (ptime - epoch_).total_seconds();
    }

  log = get_string(revprops, "svn:log");
  if (log.empty())
    {
    log = "** empty log message **";
    }

  propsFetched = true;
  return EXIT_SUCCESS;
  }

int SvnRevision::commit()
  {
  if (fetchRevProps() != EXIT_SUCCESS)
    {
    return EXIT_FAILURE;
    }
  foreach(Repository *repo, svn.repositories.values())
    {
    repo->commit();
    }
  foreach(Repository::Transaction *txn, transactions)
    {
    txn->setAuthor(QByteArray(author.c_str(), author.length()));
    txn->setDateTime(epoch);
    txn->setLog(QByteArray(log.c_str(), log.length()));
    txn->commit();
    delete txn;
    }
  return EXIT_SUCCESS;
  }

int SvnRevision::exportEntry(const char *key, const svn_fs_path_change_t *change,
                             apr_hash_t *changes)
{
    AprPool revpool(pool.data());
    QString current = QString::fromUtf8(key);

    // was this copied from somewhere?
    svn_revnum_t rev_from;
    const char *path_from;
    check_svn(svn_fs_copied_from(&rev_from, &path_from, fs_root, key, revpool));

    // is this a directory?
    svn_boolean_t is_dir;
    check_svn(svn_fs_is_dir(&is_dir, fs_root, key, revpool));
    if (is_dir) {
        if (change->change_kind == svn_fs_path_change_modify ||
            change->change_kind == svn_fs_path_change_add) {
            if (path_from == NULL) {
                // freshly added directory, or modified properties
                // Git doesn't handle directories, so we don't either
                //qDebug() << "   mkdir ignored:" << key;
                return EXIT_SUCCESS;
            }

            qDebug() << "   " << key << "was copied from" << path_from << "rev" << rev_from;
        } else if (change->change_kind == svn_fs_path_change_replace) {
            if (path_from == NULL)
                qDebug() << "   " << key << "was replaced";
            else
                qDebug() << "   " << key << "was replaced from" << path_from << "rev" << rev_from;
        } else if (change->change_kind == svn_fs_path_change_reset) {
            qCritical() << "   " << key << "was reset, panic!";
            return EXIT_FAILURE;
        } else {
            // if change_kind == delete, it shouldn't come into this arm of the 'is_dir' test
            qCritical() << "   " << key << "has unhandled change kind " << change->change_kind << ", panic!";
            return EXIT_FAILURE;
        }
    } else if (change->change_kind == svn_fs_path_change_delete) {
        is_dir = wasDir(fs, revnum - 1, key, revpool);
    }

    if (is_dir)
        current += '/';

    //MultiRule: loop start
    //Replace all returns with continue,
    bool isHandled = false;
    foreach ( const MatchRuleList matchRules, svn.allMatchRules ) {
        // find the first rule that matches this pathname
        MatchRuleList::ConstIterator match = findMatchRule(matchRules, revnum, current);
        if (match != matchRules.constEnd()) {
            const Rules::Match &rule = *match;
            if ( exportDispatch(key, change, path_from, rev_from, changes, current, rule, matchRules, revpool) == EXIT_FAILURE )
                return EXIT_FAILURE;
            isHandled = true;
        } else if (is_dir && path_from != NULL) {
            qDebug() << current << "is a copy-with-history, auto-recursing";
            if ( recurse(key, change, path_from, matchRules, rev_from, changes, revpool) == EXIT_FAILURE )
                return EXIT_FAILURE;
            isHandled = true;
        } else if (is_dir && change->change_kind == svn_fs_path_change_delete) {
            qDebug() << current << "deleted, auto-recursing";
            if ( recurse(key, change, path_from, matchRules, rev_from, changes, revpool) == EXIT_FAILURE )
                return EXIT_FAILURE;
            isHandled = true;
        }
    }
    if ( isHandled ) {
        return EXIT_SUCCESS;
    }
    if (wasDir(fs, revnum - 1, key, revpool)) {
        qDebug() << current << "was a directory; ignoring";
    } else if (change->change_kind == svn_fs_path_change_delete) {
        qDebug() << current << "is being deleted but I don't know anything about it; ignoring";
    } else {
        qCritical() << current << "did not match any rules; cannot continue";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int SvnRevision::exportDispatch(const char *key, const svn_fs_path_change_t *change,
                                const char *path_from, svn_revnum_t rev_from,
                                apr_hash_t *changes, const QString &current,
                                const Rules::Match &rule, const MatchRuleList &matchRules, apr_pool_t *pool)
{
    //if(ruledebug)
    //  qDebug() << "rev" << revnum << qPrintable(current) << "matched rule:" << rule.lineNumber << "(" << rule.rx.pattern() << ")";
    switch (rule.action) {
    case Rules::Match::Ignore:
        //if(ruledebug)
        //    qDebug() << "  " << "ignoring.";
        return EXIT_SUCCESS;

    case Rules::Match::Recurse:
        if(ruledebug)
            qDebug() << "rev" << revnum << qPrintable(current) << "matched rule:" << rule.info() << "  " << "recursing.";
        return recurse(key, change, path_from, matchRules, rev_from, changes, pool);

    case Rules::Match::Export:
        if(ruledebug)
            qDebug() << "rev" << revnum << qPrintable(current) << "matched rule:" << rule.info() << "  " << "exporting.";
        if (exportInternal(key, change, path_from, rev_from, current, rule, matchRules) == EXIT_SUCCESS)
            return EXIT_SUCCESS;
        if (change->change_kind != svn_fs_path_change_delete) {
            if(ruledebug)
                qDebug() << "rev" << revnum << qPrintable(current) << "matched rule:" << rule.info() << "  " << "Unable to export non path removal.";
            return EXIT_FAILURE;
        }
        // we know that the default action inside recurse is to recurse further or to ignore,
        // either of which is reasonably safe for deletion
        qWarning() << "WARN: deleting unknown path" << current << "; auto-recursing";
        return recurse(key, change, path_from, matchRules, rev_from, changes, pool);
    }

    // never reached
    return EXIT_FAILURE;
}

int SvnRevision::exportInternal(const char *key, const svn_fs_path_change_t *change,
                                const char *path_from, svn_revnum_t rev_from,
                                const QString &current, const Rules::Match &rule, const MatchRuleList &matchRules)
{
    needCommit = true;
    QString svnprefix, repository, branch, path;
    splitPathName(rule, current, &svnprefix, &repository, &branch, &path);

    Repository *repo = svn.repositories.value(repository, 0);
    if (!repo) {
        if (change->change_kind != svn_fs_path_change_delete)
            qCritical() << "Rule" << rule
                        << "references unknown repository" << repository;
        return EXIT_FAILURE;
    }

//                qDebug() << "   " << qPrintable(current) << "rev" << revnum << "->"
//                         << qPrintable(repository) << qPrintable(branch) << qPrintable(path);

    if (change->change_kind == svn_fs_path_change_delete && current == svnprefix && path.isEmpty()) {
        if(ruledebug)
            qDebug() << "repository" << repository << "branch" << branch << "deleted";
        return repo->deleteBranch(branch, revnum);
    }

    QString previous;
    QString prevsvnprefix, prevrepository, prevbranch, prevpath;

    if (path_from != NULL) {
        previous = QString::fromUtf8(path_from);
        if (wasDir(fs, rev_from, path_from, pool.data())) {
            previous += '/';
        }
        MatchRuleList::ConstIterator prevmatch =
            findMatchRule(matchRules, rev_from, previous, NoIgnoreRule);
        if (prevmatch != matchRules.constEnd()) {
            splitPathName(*prevmatch, previous, &prevsvnprefix, &prevrepository,
                          &prevbranch, &prevpath);

        } else {
            qWarning() << "WARN: SVN reports a \"copy from\" @" << revnum << "from" << path_from << "@" << rev_from << "but no matching rules found! Ignoring copy, treating as a modification";
            path_from = NULL;
        }
    }

    // current == svnprefix => we're dealing with the contents of the whole branch here
    if (path_from != NULL && current == svnprefix && path.isEmpty()) {
        if (previous != prevsvnprefix) {
            // source is not the whole of its branch
            qDebug() << qPrintable(current) << "is a partial branch of repository"
                     << qPrintable(prevrepository) << "branch"
                     << qPrintable(prevbranch) << "subdir"
                     << qPrintable(prevpath);
        } else if (prevrepository != repository) {
            qWarning() << "WARN:" << qPrintable(current) << "rev" << revnum
                       << "is a cross-repository copy (from repository"
                       << qPrintable(prevrepository) << "branch"
                       << qPrintable(prevbranch) << "path"
                       << qPrintable(prevpath) << "rev" << rev_from << ")";
        } else if (path != prevpath) {
            qDebug() << qPrintable(current)
                     << "is a branch copy which renames base directory of all contents"
                     << qPrintable(prevpath) << "to" << qPrintable(path);
            // FIXME: Handle with fast-import 'file rename' facility
            //        ??? Might need special handling when path == / or prevpath == /
        } else {
            if (prevbranch == branch) {
                // same branch and same repository
                qDebug() << qPrintable(current) << "rev" << revnum
                         << "is reseating branch" << qPrintable(branch)
                         << "to an earlier revision"
                         << qPrintable(previous) << "rev" << rev_from;
            } else {
                // same repository but not same branch
                // this means this is a plain branch
                qDebug() << qPrintable(repository) << ": branch"
                         << qPrintable(branch) << "is branching from"
                         << qPrintable(prevbranch);
            }

            if (repo->createBranch(branch, revnum, prevbranch, rev_from) == EXIT_FAILURE)
                return EXIT_FAILURE;

            if(options.svn_branches) {
                Repository::Transaction *txn = transactions.value(repository + branch, 0);
                if (!txn) {
                    txn = repo->newTransaction(branch, svnprefix, revnum);
                    if (!txn)
                        return EXIT_FAILURE;

                    transactions.insert(repository + branch, txn);
                }
                if(ruledebug)
                    qDebug() << "Create a true SVN copy of branch (" << key << "->" << branch << path << ")";
                txn->deleteFile(path);
                recursiveDumpDir(txn, fs_root, key, path, pool);
            }
            if (rule.annotate) {
                // create an annotated tag
                fetchRevProps();
                repo->createAnnotatedTag(
                    branch,
                    svnprefix,
                    revnum,
                    QByteArray(author.c_str(), author.length()),
                    epoch,
                    QByteArray(log.c_str(), log.length()));
            }
            return EXIT_SUCCESS;
        }
    }
    Repository::Transaction *txn = transactions.value(repository + branch, 0);
    if (!txn) {
        txn = repo->newTransaction(branch, svnprefix, revnum);
        if (!txn)
            return EXIT_FAILURE;

        transactions.insert(repository + branch, txn);
    }

    //
    // If this path was copied from elsewhere, use it to infer _some_
    // merge points.  This heuristic is fairly useful for tracking
    // changes across directory re-organizations and wholesale branch
    // imports.
    //
    if (path_from != NULL && prevrepository == repository && prevbranch != branch) {
        if(ruledebug)
            qDebug() << "copy from branch" << prevbranch << "to branch" << branch << "@rev" << rev_from;
        txn->noteCopyFromBranch (prevbranch, rev_from);
    }

    if (change->change_kind == svn_fs_path_change_replace && path_from == NULL) {
        if(ruledebug)
            qDebug() << "replaced with empty path (" << branch << path << ")";
        txn->deleteFile(path);
    }
    if (change->change_kind == svn_fs_path_change_delete) {
        if(ruledebug)
            qDebug() << "delete (" << branch << path << ")";
        txn->deleteFile(path);
    } else if (!current.endsWith('/')) {
        if(ruledebug)
            qDebug() << "add/change file (" << key << "->" << branch << path << ")";
        dumpBlob(txn, fs_root, key, path, pool);
    } else {
        if(ruledebug)
            qDebug() << "add/change dir (" << key << "->" << branch << path << ")";
        txn->deleteFile(path);
        recursiveDumpDir(txn, fs_root, key, path, pool);
    }

    return EXIT_SUCCESS;
}

int SvnRevision::recurse(const char *path, const svn_fs_path_change_t *change,
                         const char *path_from, const MatchRuleList &matchRules, svn_revnum_t rev_from,
                         apr_hash_t *changes, apr_pool_t *pool)
{
    svn_fs_root_t *fs_root = this->fs_root;
    if (change->change_kind == svn_fs_path_change_delete)
        check_svn(svn_fs_revision_root(&fs_root, fs, revnum - 1, pool));

    // get the dir listing
    svn_node_kind_t kind;
    check_svn(svn_fs_check_path(&kind, fs_root, path, pool));
    if(kind == svn_node_none) {
        qWarning() << "WARN: Trying to recurse using a nonexistant path" << path << ", ignoring";
        return EXIT_SUCCESS;
    } else if(kind != svn_node_dir) {
        qWarning() << "WARN: Trying to recurse using a non-directory path" << path << ", ignoring";
        return EXIT_SUCCESS;
    }

    apr_hash_t *entries;
    check_svn(svn_fs_dir_entries(&entries, fs_root, path, pool));
    AprPool dirpool(pool);

    // While we get a hash, put it in a map for sorted lookup, so we can
    // repeat the conversions and get the same git commit hashes.
    QMap<QByteArray, svn_node_kind_t> map;
    for (apr_hash_index_t *i = apr_hash_first(pool, entries); i; i = apr_hash_next(i)) {
        dirpool.clear();
        const void *vkey;
        void *value;
        apr_hash_this(i, &vkey, NULL, &value);
        svn_fs_dirent_t *dirent = reinterpret_cast<svn_fs_dirent_t *>(value);
        if (dirent->kind != svn_node_dir)
            continue;           // not a directory, so can't recurse; skip
        map.insertMulti(QByteArray(dirent->name), dirent->kind);
    }

    QMapIterator<QByteArray, svn_node_kind_t> i(map);
    while (i.hasNext()) {
        dirpool.clear();
        i.next();
        QByteArray entry = path + QByteArray("/") + i.key();
        QByteArray entryFrom;
        if (path_from)
            entryFrom = path_from + QByteArray("/") + i.key();

        // check if this entry is in the changelist for this revision already
        svn_fs_path_change_t *otherchange =
            (svn_fs_path_change_t*)apr_hash_get(changes, entry.constData(), APR_HASH_KEY_STRING);
        if (otherchange && otherchange->change_kind == svn_fs_path_change_add) {
            qDebug() << entry << "rev" << revnum
                     << "is in the change-list, deferring to that one";
            continue;
        }

        QString current = QString::fromUtf8(entry);
        if (i.value() == svn_node_dir)
            current += '/';

        // find the first rule that matches this pathname
        MatchRuleList::ConstIterator match = findMatchRule(matchRules, revnum, current);
        if (match != matchRules.constEnd()) {
            if (exportDispatch(entry, change, entryFrom.isNull() ? 0 : entryFrom.constData(),
                               rev_from, changes, current, *match, matchRules, dirpool) == EXIT_FAILURE)
                return EXIT_FAILURE;
        } else {
            if (i.value() == svn_node_dir) {
                qDebug() << current << "rev" << revnum
                         << "did not match any rules; auto-recursing";
                if (recurse(entry, change, entryFrom.isNull() ? 0 : entryFrom.constData(),
                            matchRules, rev_from, changes, dirpool) == EXIT_FAILURE)
                    return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}
