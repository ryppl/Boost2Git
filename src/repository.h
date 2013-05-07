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

class Repository
  {
    typedef boost2git::BranchRule BranchRule;
  public:
    class Transaction
      {
        Q_DISABLE_COPY(Transaction)
        friend class Repository;

        Repository *repository;
        QByteArray branch;
        QByteArray svnprefix;
        QByteArray author;
        QByteArray log;
        uint datetime;
        int revnum;

        QVector<int> merges;

        QStringList deletedFiles;
        QByteArray modifiedFiles;

        inline Transaction()
            : repository(0), datetime(0), revnum(0) {}
      public:
        ~Transaction();
        void commit();

        void setAuthor(const QByteArray &author);
        void setDateTime(uint dt);
        void setLog(const QByteArray &log);

        void noteCopyFromBranch (const QString &prevbranch, int revFrom);

        void deleteFile(const QString &path);
        QIODevice *addFile(const QString &path, int mode, qint64 length);

        void commitNote(const QByteArray &noteText, bool append,
          const QByteArray &commit = QByteArray());
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
      const QString &branchFrom, int revFrom);
    Repository::Transaction *newTransaction(BranchRule const* branch, const QString &svnprefix, int revnum);
    int deleteBranch(BranchRule const* branch, int revnum);

    void createAnnotatedTag(BranchRule const* branch, const QString &svnprefix, int revnum,
      const QByteArray &author, uint dt,
      const QByteArray &log);
    void finalizeTags();
    void commit();

    static QByteArray formatMetadataMessage(const QByteArray &svnprefix, int revnum,
      const QByteArray &tag = QByteArray());

    bool branchExists(const QString& branch) const;
    const QByteArray branchNote(const QString& branch) const;
    void setBranchNote(const QString& branch, const QByteArray& noteText);

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
      QByteArray note;
      };
    struct AnnotatedTag
      {
      QString supportingRef;
      QByteArray svnprefix;
      QByteArray author;
      QByteArray log;
      uint dt;
      int revnum;
      };

    QHash<QString, Branch> branches;
    QHash<QString, AnnotatedTag> annotatedTags;
    QString name;
    QString prefix;
    Repository* submodule_in_repo;
    QString submodule_path;
    LoggingQProcess fastImport;
    int commitCount;
    int outstandingTransactions;
    QHash<QString, QByteArray> deletedBranches;
    QHash<QString, QByteArray> resetBranches;

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

    int resetBranch(BranchRule const*, const QString &branch, int revnum, int mark, const QByteArray &resetTo, const QByteArray &comment);
    int markFrom(const QString &branchFrom, int branchRevNum, QByteArray &desc);
    Repository::Transaction *newTransaction(const QString &branch, const QString &svnprefix, int revnum);
    void submoduleChanged(Repository const* submodule, BranchRule const* branch_rule);

    friend class ProcessCache;
    Q_DISABLE_COPY(Repository)
  };

#endif
