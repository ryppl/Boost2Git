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

#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <QHash>
#include <QProcess>
#include <QVector>
#include <QFile>

#include "logging_process.hpp"
#include "ruleset.hpp"
#include <map>

namespace std
{
// This is totally ILL, but it's expedient.
inline uint qHash(std::string const& s)
  {
  return qHash(QByteArray(s.c_str(), s.size()));
  }
}

class Repository
  {
    typedef boost2git::BranchRule BranchRule;
  public:
    class Transaction
      {
        Q_DISABLE_COPY(Transaction)
        friend class Repository;

        Repository *repository;
        std::string branch;
        std::string svnprefix;
        std::string author;
        std::string log;
        uint datetime;
        int revnum;

        QVector<int> merges;

        QVector<std::string> deletedFiles;
        std::string modifiedFiles;

        inline Transaction()
            : repository(0), datetime(0), revnum(0) {}
      public:
        ~Transaction();
        void commit();

        void setAuthor(const std::string &author);
        void setDateTime(uint dt);
        void setLog(const std::string &log);

        void noteCopyFromBranch (const std::string &prevbranch, int revFrom);

        void deleteFile(const std::string &path);
        QIODevice *addFile(const std::string &path, int mode, qint64 length);

        void commitNote(const std::string &noteText, bool append,
          const std::string &commit = std::string());
      };
    
    Repository(
        const Ruleset::Repository &rule,
        bool incremental,
        QHash<QString, Repository*> const& repo_index);
    
    int setupIncremental(int &cutoff);
    void restoreLog();
    ~Repository();

    void reloadBranches();
    int createBranch(BranchRule const* branch, int revnum,
      const std::string &branchFrom, int revFrom);
    Repository::Transaction *newTransaction(BranchRule const* branch, const std::string &svnprefix, int revnum);
    int deleteBranch(BranchRule const* branch, int revnum);

    void createAnnotatedTag(BranchRule const* branch, const std::string &svnprefix, int revnum,
      const std::string &author, uint dt,
      const std::string &log);
    void finalizeTags();
    void commit();

    static std::string formatMetadataMessage(const std::string &svnprefix, int revnum,
      const std::string &tag = std::string());

    bool branchExists(const std::string& branch) const;
    const std::string branchNote(const std::string& branch) const;
    void setBranchNote(const std::string& branch, const std::string& noteText);

    std::string get_name() const { return name; }
  private:
    struct Branch
      {
      Branch(int created = 0)
          : created(created), last_submodule_update_rev(0) {}
      
      int created;
      int last_submodule_update_rev; // which SVN revision contributed the last change to the submodule list
      std::map<std::string, Repository const*> submodules;
      QVector<int> commits;
      QVector<int> marks;
      std::string note;
      };
    struct AnnotatedTag
      {
      std::string supportingRef;
      std::string svnprefix;
      std::string author;
      std::string log;
      uint dt;
      int revnum;
      };

    QHash<std::string, Branch> branches;
    QHash<std::string, AnnotatedTag> annotatedTags;
    std::string name;
    std::string prefix;
    Repository* submodule_in_repo;
    std::string submodule_path;
    LoggingQProcess fastImport;
    int commitCount;
    int outstandingTransactions;
    QHash<std::string, std::string> deletedBranches;
    QHash<std::string, std::string> resetBranches;

    /* starts at 0, and counts up.  */
    int last_commit_mark;

    /* starts at maxMark and counts down. Reset after each SVN revision */
    int next_file_mark;

    bool processHasStarted;
    bool incremental;
    
    void startFastImport();
    void closeFastImport();

    // called when a transaction is deleted
    void forgetTransaction(Transaction *t);

    int resetBranch(BranchRule const*, const std::string &branch, int revnum, int mark, const std::string &resetTo, const std::string &comment);
    int markFrom(const std::string &branchFrom, int branchRevNum, std::string &desc);
    Repository::Transaction *newTransaction(const std::string &branch, const std::string &svnprefix, int revnum);
    void submoduleChanged(Repository const* submodule, BranchRule const* branch_rule);

    friend class ProcessCache;
    Q_DISABLE_COPY(Repository)
  };

#endif
