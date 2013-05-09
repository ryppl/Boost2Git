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
#include "log.hpp"

#include <QDebug>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/time_parsers.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>

#include <svn_fs.h>
#include <svn_pools.h>
#include <svn_repos.h>
#include <svn_types.h>

namespace
{

bool pathExists(apr_pool_t* pool, svn_fs_t *fs, std::string const& path, int revnum)
  {
  // Log::debug() << "### checking for existence of " << path << "@" << revnum << std::endl;
    
  AprPool subpool(pool);
  svn_fs_root_t *fs_root;
  check_svn(svn_fs_revision_root(&fs_root, fs, revnum, subpool));
  svn_boolean_t is_dir;

  check_svn(svn_fs_is_dir(&is_dir, fs_root, path.c_str(), subpool));

  if (is_dir)
    {
    // Log::debug() << "## identified as directory" << std::endl;
    apr_hash_t* tree_entries = apr_hash_make(subpool);
    check_svn(svn_fs_dir_entries(&tree_entries, fs_root, path.c_str(), subpool));

    // As far as Git is concerned, an empty directory might as well
    // not exist
    apr_hash_index_t *i = apr_hash_first(subpool, tree_entries);

    if (i == 0)
      {
      // Log::debug() << "## no entries." << std::endl;
      return false;
      }

    const void       *key;
    void             *val;
    apr_hash_this(i, &key, NULL, &val);

#if 0
    char const* node = static_cast<char const*>(key);
    Log::debug() << "## found entry " << node << std::endl;
#endif 
    }
  else
    {
    // Log::debug() << "## identified as file" << std::endl;
    svn_filesize_t stream_length;
    svn_error_t* err = svn_fs_file_length(&stream_length, fs_root, path.c_str(), subpool);
    if (err)
      {
      // Log::debug() << "## file_length => ERROR" << std::endl;
      return false;
      }
    else
      {
      // Log::debug() << "## file_length == " << stream_length << std::endl;
      }
    }
  return true;
  }


Rule const* find_match(patrie const& rules, std::string const& path, std::size_t revnum)
  {
  return rules.longest_match(path, revnum);
  }

void splitPathName(
    const Ruleset::Match &rule,
    const std::string &pathName,
    std::string *svnprefix_p,
    std::string *repository_p,
    std::string *branch_p,
    std::string *path_p)
  {
  std::string svn_path = rule.svn_path();
  if (svnprefix_p)
    {
    *svnprefix_p = svn_path;
    }
  if (repository_p)
    {
    *repository_p = rule.repo_rule->name;
    }
  if (branch_p)
    {
    *branch_p = git_ref_name(rule.branch_rule);
    }
  if (path_p)
    {
    std::string current = pathName;
    std::string path = rule.prefix() + current.substr(svn_path.length());
    *path_p = path;
    }
  }

int pathMode(svn_fs_root_t *fs_root, const char *pathname, apr_pool_t *pool)
  {
  svn_string_t *propvalue;
  check_svn(svn_fs_node_prop(&propvalue, fs_root, pathname, "svn:executable", pool));
  int mode = 0100644;
  if (propvalue)
    {
    mode = 0100755;
    }
  return mode;
  }

svn_error_t *QIODevice_write(void *baton, const char *data, apr_size_t *len)
  {
  QIODevice *device = reinterpret_cast<QIODevice *>(baton);
  device->write(data, *len);
  while (device->bytesToWrite() > 32 * 1024)
    {
    if (!device->waitForBytesWritten(-1))
      {
      qFatal(
          "Failed to write to process: %s",
          qPrintable(device->errorString()));
      return svn_error_createf(
          APR_EOF,
          SVN_NO_ERROR,
          "Failed to write to process: %s",
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

void dumpBlob(
    Repository::Transaction *txn,
    svn_fs_root_t *fs_root,
    const char *pathname,
    const std::string &finalPathName,
    apr_pool_t *pool)
  {
  AprPool dumppool(pool);
  // what type is it?
  int mode = pathMode(fs_root, pathname, dumppool);
  svn_filesize_t stream_length;
  check_svn(svn_fs_file_length(&stream_length, fs_root, pathname, dumppool));

  svn_stream_t *in_stream, *out_stream;
  if (!options.dry_run)
    {
    // open the file
    check_svn(svn_fs_file_contents(&in_stream, fs_root, pathname, dumppool));
    }

  // maybe it's a symlink?
  svn_string_t *propvalue;
  check_svn(svn_fs_node_prop(&propvalue, fs_root, pathname, "svn:special", dumppool));
  if (propvalue)
    {
    apr_size_t len = strlen("link ");
    if (!options.dry_run)
      {
      QByteArray buf;
      buf.reserve(len);
      check_svn(svn_stream_read(in_stream, buf.data(), &len));
      if (len == strlen("link ") && strncmp(buf, "link ", len) == 0)
        {
        mode = 0120000;
        stream_length -= len;
        }
      else
        {
        //this can happen if a link changed into a file in one commit
        qWarning("file %s is svn:special but not a symlink", pathname);
        // re-open the file as we tried to read "link "
        svn_stream_close(in_stream);
        check_svn(svn_fs_file_contents(&in_stream, fs_root, pathname, dumppool));
        }
      }
    }

  QIODevice *io = txn->addFile(finalPathName, mode, stream_length);

  if (!options.dry_run)
    {
    // open a generic svn_stream_t for the QIODevice
    out_stream = streamForDevice(io, dumppool);
    check_svn(svn_stream_copy(in_stream, out_stream, dumppool));
    svn_stream_close(out_stream);
    svn_stream_close(in_stream);
    // print an ending newline
    io->putChar('\n');
    }
  }

void recursiveDumpDir(
    Repository::Transaction *txn,
    svn_fs_root_t *fs_root,
    const std::string &pathname,
    const std::string &finalPathName,
    apr_pool_t *pool)
  {
  // get the dir listing
  apr_hash_t *entries;
  check_svn(svn_fs_dir_entries(&entries, fs_root, pathname.c_str(), pool));
  AprPool dirpool(pool);

  // While we get a hash, put it in a map for sorted lookup, so we can
  // repeat the conversions and get the same git commit hashes.
  QMap < std::string, svn_node_kind_t > map;
  for (apr_hash_index_t *i = apr_hash_first(pool, entries); i; i = apr_hash_next(i))
    {
    const void *vkey;
    void *value;
    apr_hash_this(i, &vkey, NULL, &value);
    svn_fs_dirent_t *dirent = reinterpret_cast<svn_fs_dirent_t *>(value);
    map.insertMulti(std::string(dirent->name), dirent->kind);
    }

  QMapIterator<std::string, svn_node_kind_t> i(map);
  while (i.hasNext())
    {
    dirpool.clear();
    i.next();
    std::string entryName = pathname + '/' + i.key();
    std::string entryFinalName = finalPathName + i.key();

    if (i.value() == svn_node_dir)
      {
      entryFinalName += '/';
      recursiveDumpDir(txn, fs_root, entryName, entryFinalName, dirpool);
      }
    else if (i.value() == svn_node_file)
      {
      dumpBlob(txn, fs_root, entryName.c_str(), entryFinalName, dirpool);
      }
    }
  }

bool existed(svn_fs_t *fs, int revnum, const char *pathname, apr_pool_t *pool)
  {
  svn_fs_root_t *fs_root;
  svn_node_kind_t kind;
  check_svn(svn_fs_revision_root(&fs_root, fs, revnum, pool));
  check_svn(svn_fs_check_path(&kind, fs_root, pathname, pool));
  return kind != svn_node_none;
  }

bool wasDir(svn_fs_t *fs, int revnum, const char *pathname, apr_pool_t *pool)
  {
  svn_fs_root_t *fs_root;
  if (svn_fs_revision_root(&fs_root, fs, revnum, pool) != SVN_NO_ERROR)
    {
    return false;
    }
  svn_boolean_t is_dir;
  if (svn_fs_is_dir(&is_dir, fs_root, pathname, pool) != SVN_NO_ERROR)
    {
    return false;
    }
  return is_dir;
  }

} // namespace

int SvnRevision::prepareTransactions()
  {
  // find out what was changed in this revision:
  apr_hash_t *changes;
  check_svn(svn_fs_paths_changed2(&changes, fs_root, pool));

  QMap<std::string, svn_fs_path_change2_t*> map;
  for (apr_hash_index_t *i = apr_hash_first(pool, changes); i; i = apr_hash_next(i))
    {
    const void *vkey;
    void *value;
    apr_hash_this(i, &vkey, NULL, &value);
    const char *key = static_cast<const char *>(vkey);
    svn_fs_path_change2_t *change = static_cast<svn_fs_path_change2_t *>(value);
    // If we mix path deletions with path adds/replaces we might erase a
    // branch after that it has been reset -> history truncated
    if (map.contains(std::string(key)))
      {
      // If the same path is deleted and added, we need to put the
      // deletions into the map first, then the addition.
      if (change->change_kind == svn_fs_path_change_delete)
        {
        // XXX
        }
      std::stringstream msg;
      msg << "Duplicate key found in rev " << revnum << ": " << key << '\n';
      msg << "This needs more code to be handled, file a bug report!";
      throw std::runtime_error(msg.str());
      }
    map.insertMulti(std::string(key), change);
    }

  QMapIterator<std::string, svn_fs_path_change2_t*> i(map);
  while (i.hasNext())
    {
    i.next();
    if (exportEntry(i.key().c_str(), i.value(), changes) == EXIT_FAILURE)
      {
      return EXIT_FAILURE;
      }
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

void SvnRevision::fetchRevProps()
  {
  if (propsFetched)
    {
    return;
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
  }

void SvnRevision::commit()
  {
  if (!needCommit)
    {
    return;
    }
  fetchRevProps();
  foreach(Repository *repo, svn.repositories.values())
    {
    repo->commit();
    }
  foreach(Repository::Transaction *txn, transactions)
    {
    txn->setAuthor(std::string(author.c_str(), author.length()));
    txn->setDateTime(epoch);
    txn->setLog(log);
    txn->commit();
    delete txn;
    }
  }

int SvnRevision::exportEntry(
    const char *key,
    const svn_fs_path_change2_t *change,
    apr_hash_t *changes)
  {
  if (boost::contains(key, "CVSROOT"))
    {
    return EXIT_SUCCESS;
    }
  std::string current = key;

  // was this copied from somewhere?
  svn_revnum_t rev_from;
  const char *path_from;
  check_svn(svn_fs_copied_from(&rev_from, &path_from, fs_root, key, pool));

  // is this a directory?
  svn_boolean_t is_dir;
  if (path_from)
    {
    is_dir = wasDir(fs, rev_from, path_from, pool);
    }
  else
    {
    check_svn(svn_fs_is_dir(&is_dir, fs_root, key, pool));
    }

  if (is_dir)
    {
    if (change->change_kind == svn_fs_path_change_modify || change->change_kind == svn_fs_path_change_add)
      {
      if (path_from == NULL)
        {
        // freshly added directory, or modified properties
        // Git doesn't handle directories, so we don't either
        //qDebug() << "   mkdir ignored:" << key;
        return EXIT_SUCCESS;
        }
      Log::debug() << key << " was copied from " << path_from << " rev " << rev_from << std::endl;
      }
    else if (change->change_kind == svn_fs_path_change_replace)
      {
      if (path_from == NULL)
        {
        Log::debug() << key << " was replaced" << std::endl;
        }
      else
        {
        Log::debug() << key << " was replaced from " << path_from << " rev " << rev_from << std::endl;
        }
      }
    else if (change->change_kind == svn_fs_path_change_reset)
      {
      throw std::runtime_error(std::string(key) + " was reset, panic!");
      }
    else
      {
      // if change_kind == delete, it shouldn't come into this arm of the 'is_dir' test
      std::stringstream error;
      error << key << " has unhandled change kind " << change->change_kind << ", panic!";
      throw std::runtime_error(error.str());
      }
    }
  else if (change->change_kind == svn_fs_path_change_delete)
    {
    // if it did not exist in the previous revision, we ignore the deletion
    // this happens if a folder is copied and part of its contents are removed
    if (!existed(fs, revnum - 1, key, pool))
      {
      Log::debug() << "Ignoring deletion of non-existing path: " << key << std::endl;
      return EXIT_SUCCESS;
      }
    is_dir = wasDir(fs, revnum - 1, key, pool);
    }

  if (is_dir)
    {
    current += '/';
    }

  //MultiRule: loop start
  //Replace all returns with continue,
  bool isHandled = false;
  MatchRuleList const& matchRules = svn.ruleset.matches();
  // find the first rule that matches this pathname
  Rule const* match = find_match(matchRules, current, revnum);
  if (match)
    {
    const Ruleset::Match &rule = *match;
    if (is_dir && rule.is_fallback())
      {
      if (recurse(key, change, path_from, matchRules, rev_from, changes) == EXIT_FAILURE)
        {
        return EXIT_FAILURE;
        }
      }
    else if (exportDispatch(key, change, path_from, rev_from, changes, current, rule, matchRules) == EXIT_FAILURE)
      {
      return EXIT_FAILURE;
      }
    isHandled = true;
    }
  else if (is_dir && path_from != NULL)
    {
    Log::debug() << current << " is a copy-with-history, auto-recursing" << std::endl;
    if ( recurse(key, change, path_from, matchRules, rev_from, changes) == EXIT_FAILURE )
      {
      return EXIT_FAILURE;
      }
    isHandled = true;
    }
  else if (is_dir && change->change_kind == svn_fs_path_change_delete)
    {
    Log::debug() << current << " deleted, auto-recursing" << std::endl;
    if ( recurse(key, change, path_from, matchRules, rev_from, changes) == EXIT_FAILURE )
      {
      return EXIT_FAILURE;
      }
    isHandled = true;
    }
  if (isHandled)
    {
    return EXIT_SUCCESS;
    }
  if (is_dir)
    {
    Log::warn() << "Folder '" << current << "' not accounted for. Recursing." << std::endl;
    return recurse(key, change, path_from, matchRules, rev_from, changes);
    }
  else
    {
    Log::warn() << "File '" << current << "' not accounted for. Putting to fallback." << std::endl;
    return exportDispatch(key, change, path_from, rev_from, changes, current, Ruleset::fallback, matchRules);
    }
  return EXIT_SUCCESS;
  }

int SvnRevision::exportDispatch(
    const char *key,
    const svn_fs_path_change2_t *change,
    const char *path_from,
    svn_revnum_t rev_from,
    apr_hash_t *changes,
    const std::string &current,
    const Ruleset::Match &rule,
    const MatchRuleList &matchRules)
  {
  Log::trace() << "rev " << revnum << ' ' << current << " matched rule: '" << rule.svn_path();

  if (change->change_kind == svn_fs_path_change_delete
    && !pathExists(pool, fs, current, revnum - 1))
    {
    Log::trace() << "but deleted path missing in previous revision." << std::endl;
    return EXIT_SUCCESS;
    }
  else
    {
    Log::trace() << "'; exporting to repository " << rule.repo_rule->name << " branch "
                 << rule.branch_rule->name << " path " << rule.prefix() << std::endl;
    }
  
  if (exportInternal(key, change, path_from, rev_from, current, rule, matchRules, changes) == EXIT_SUCCESS)
    {
    return EXIT_SUCCESS;
    }
  if (change->change_kind != svn_fs_path_change_delete)
    {
    Log::trace() << "rev " << revnum << ' ' << current << " matched rule: '" << rule.svn_path()
                 << "'; Unable to export non path removal." << std::endl;
    return EXIT_FAILURE;
    }
  // we know that the default action inside recurse is to recurse further or to ignore,
  // either of which is reasonably safe for deletion
  Log::warn() << "deleting unknown path '" << current << "'; auto-recursing" << std::endl;
  return recurse(key, change, path_from, matchRules, rev_from, changes);
  }

int SvnRevision::exportInternal(
    const char *key,
    const svn_fs_path_change2_t *change,
    const char *path_from,
    svn_revnum_t rev_from,
    const std::string &current,
    const Ruleset::Match &rule,
    const MatchRuleList &matchRules,
    apr_hash_t *cc)
  {
  needCommit = true;
  std::string svnprefix, repository, path;
  boost2git::BranchRule const* branch = rule.branch_rule;
  splitPathName(rule, current, &svnprefix, &repository, 0, &path);

  Repository *repo = svn.repositories.value(QString::fromStdString(repository), 0);
  if (!repo)
    {
    if (change->change_kind != svn_fs_path_change_delete)
      {
      throw std::runtime_error("Rule " + rule.svn_path() + " references unknown repository " + repository);
      }
    return EXIT_FAILURE;
    }

  std::string const branch_ref_name = git_ref_name(branch);
  
  if (change->change_kind == svn_fs_path_change_delete && current == svnprefix && path.empty())
    {
    Log::trace() << "repository " << repository << " branch " << branch_ref_name << " deleted" << std::endl;
    return repo->deleteBranch(branch, this);
    }

  std::string previous;
  std::string prevsvnprefix, prevrepository, prevpath;
  std::string prevbranch;

  if (path_from != NULL)
    {
    previous = path_from;
    bool was_dir = wasDir(fs, rev_from, path_from, pool.data());
    if (was_dir && !boost::ends_with(previous, "/"))
      {
      previous += '/';
      }
    Rule const* prevmatch = find_match(matchRules, previous, rev_from);
    if (prevmatch)
      {
      prevbranch = git_ref_name(prevmatch->branch_rule);
      splitPathName(*prevmatch, previous, &prevsvnprefix, &prevrepository, /*branch*/0, &prevpath);
      }
    else if (was_dir)
      {
      return recurse(key, change, path_from, matchRules, rev_from, cc);
      }
    else
      {
      Log::warn() << '"' << key << '@' << revnum << '"' << " is copied from " << '"'
                  << previous << '@' << rev_from << '"'
                  << " but no rules match the source of the copy!"
                  << " Ignoring copy; treating as a modification" << std::endl;
      path_from = NULL;
      }
    }

  // current == svnprefix => we're dealing with the contents of the whole branch here
  if (path_from != NULL && current == svnprefix && path.empty())
    {
    if (previous != prevsvnprefix)
      {
      // source is not the whole of its branch
      Log::debug() << current << "is a partial branch of repository "
                   << prevrepository << " branch " << prevbranch
                   << " subdir " << prevpath << std::endl;
      }
    else if (prevrepository != repository)
      {
      Log::warn() << current << " rev " << revnum
                  << " is a cross-repository copy (from repository "
                  << prevrepository << " branch " << prevbranch
                  << " path " << prevpath << " rev " << rev_from << ")" << std::endl;
      }
    else if (path != prevpath)
      {
      Log::debug() << current << " is a branch copy which renames base directory of all contents " << prevpath << " to " << path << std::endl;
      // FIXME: Handle with fast-import 'file rename' facility
      //        ??? Might need special handling when path == / or prevpath == /
      }
    else
      {
      if (prevbranch == branch_ref_name)
        {
        // same branch and same repository
        Log::debug() << current << " rev " << revnum << " is reseating branch "
                     << branch_ref_name << " to an earlier revision " << previous
                     << " rev " << rev_from << std::endl;
        }
      else
        {
        // same repository but not same branch
        // this means this is a plain branch
        Log::debug() << repository << ": branch " << branch_ref_name
                     << " is branching from " << prevbranch << std::endl;
        }
 
      if (repo->createBranch(branch, this, prevbranch, rev_from) == EXIT_FAILURE)
        {
        return EXIT_FAILURE;
        }

      if (options.svn_branches)
        {
        Repository::Transaction *txn = demandTransaction(repo, rule.branch_rule, svnprefix);
        Log::trace() << "Create a true SVN copy of branch (" << key << "->"
                     << branch_ref_name << "/" << path << ")" << std::endl;
        if (pathExists(pool, fs, current, revnum - 1))
          {
          txn->deleteFile(path);
          recursiveDumpDir(txn, fs_root, key, path, pool);
          }
        else
          {
          Log::trace() << "...deleted file missing in previous revision, so skipped" << std::endl;
          }
        }
//      if (rule.annotate)
//        {
//        // create an annotated tag
//        fetchRevProps();
//        repo->createAnnotatedTag(
//            branch,
//            svnprefix,
//            revnum,
//            std::string(author.c_str(), author.length()),
//            epoch,
//            std::string(log.c_str(), log.length()));
//        }
      return EXIT_SUCCESS;
      }
    }
  Repository::Transaction *txn = demandTransaction(repo, rule.branch_rule, svnprefix); 

  //
  // If this path was copied from elsewhere, use it to infer _some_
  // merge points.  This heuristic is fairly useful for tracking
  // changes across directory re-organizations and wholesale branch
  // imports.
  //
  if (path_from != NULL && prevrepository == repository && prevbranch != branch_ref_name)
    {
    Log::trace() << "copy from branch " << prevbranch << " to branch "
                 << branch_ref_name << "@rev" << rev_from << std::endl;
    txn->noteCopyFromBranch (prevbranch, rev_from);
    }

  if (change->change_kind == svn_fs_path_change_replace && path_from == NULL)
    {
    Log::trace() << "replaced with empty path (" << branch_ref_name
                 << "/" << path << ")" << std::endl;
    txn->deleteFile(path);
    }
  else if (change->change_kind == svn_fs_path_change_delete)
    {
    Log::trace() << "delete (" << branch_ref_name << "/" << path << ")" << std::endl;
    if (pathExists(pool, fs, current, revnum - 1)) {
      txn->deleteFile(path);
      }
    else
      {
      Log::trace() << "...deleted file missing in previous revision, so skipped" << std::endl;
      }
    }
  else if (!boost::ends_with(current, "/"))
    {
    Log::trace() << "add/change file (" << key << "->" << branch_ref_name
                 << "/" << path << ")" << std::endl;
    dumpBlob(txn, fs_root, key, path, pool);
    }
  else
    {
    Log::trace() << "add/change dir (" << key << "->" << branch_ref_name
                 << "/" << path << ")" << std::endl ;
    recursiveDumpDir(txn, fs_root, key, path, pool);
    }
  return EXIT_SUCCESS;
  }

int SvnRevision::recurse(
    const char *path,
    const svn_fs_path_change2_t *change,
    const char *path_from,
    const MatchRuleList &matchRules,
    svn_revnum_t rev_from,
    apr_hash_t *changes)
  {
  svn_fs_root_t *fs_root = this->fs_root;
  if (change->change_kind == svn_fs_path_change_delete)
    {
    check_svn(svn_fs_revision_root(&fs_root, fs, revnum - 1, pool));
    }

  // make sure it is a directory
  svn_node_kind_t kind;
  check_svn(svn_fs_check_path(&kind, fs_root, path, pool));
  if (kind != svn_node_dir)
    {
    char *msg = apr_pstrcat(pool, "Trying to recurse using a non-directory path '", path, "'", NULL);
    throw std::runtime_error(msg);
    }

  // get the dir listing
  apr_hash_t *entries;
  check_svn(svn_fs_dir_entries(&entries, fs_root, path, pool));

  for (apr_hash_index_t *i = apr_hash_first(pool, entries); i; i = apr_hash_next(i))
    {
    svn_fs_dirent_t *dirent;
    apr_hash_this(i, NULL, NULL, (void**) &dirent);

    if (strstr(dirent->name, "CVSROOT"))
      {
      continue;
      }

    const char *entry = apr_pstrcat(pool, path, "/", dirent->name, NULL);
    const char *entryFrom = path_from ? apr_pstrcat(pool, path_from, "/", dirent->name, NULL) : NULL;

    // check if this entry is in the changelist for this revision already
    svn_fs_path_change2_t *otherchange = (svn_fs_path_change2_t*) apr_hash_get(changes, entry, APR_HASH_KEY_STRING);
    if (otherchange && otherchange->change_kind == svn_fs_path_change_add)
      {
      Log::debug() << '"' << entry << '@' << revnum << '"'
                   << " is in the change-list, deferring to that one" << std::endl;
      continue;
      }

    const char *current = entry;
    if (dirent->kind == svn_node_dir)
      {
      current = apr_pstrcat(pool, current, "/", NULL);
      }

    // find the longest match for this pathname
    Rule const* match = find_match(matchRules, current, revnum);
    if (match)
      {
      if (exportDispatch(entry, change, entryFrom, rev_from, changes, current, *match, matchRules) == EXIT_FAILURE)
        {
        return EXIT_FAILURE;
        }
      }
    else if (dirent->kind == svn_node_dir)
      {
      Log::debug() << '"' << current << '@' << revnum << '"'
                   << " did not match any rules; auto-recursing" << std::endl;
      if (recurse(entry, change, entryFrom, matchRules, rev_from, changes) == EXIT_FAILURE)
        {
        return EXIT_FAILURE;
        }
      }
    else
      {
      Log::warn() << '"' << current << '@' << revnum << '"'
                  << " not accounted for. Putting to fallback." << std::endl;
      if (exportDispatch(entry, change, entryFrom, rev_from, changes, current, Ruleset::fallback, matchRules) == EXIT_FAILURE)
        {
        return EXIT_FAILURE;
        }
      }
    }
  return EXIT_SUCCESS;
  }

Repository::Transaction* SvnRevision::demandTransaction(
    Repository* repo, boost2git::BranchRule const* branch, std::string const& svnprefix)
  {
  std::string key = repo->get_name() + git_ref_name(branch);
  Repository::Transaction *txn = transactions.value(key, 0);
  if (!txn)
    {
    txn = repo->newTransaction(branch, svnprefix, this);
    transactions.insert(key, txn);
    }
  return txn;
  }

