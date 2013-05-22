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

class Repository;
typedef QHash<QString, Repository*> RepoIndex;

class Repository
  {
  public:
    class Transaction
      {
        friend class Repository;

        Repository *repository;
        std::string branch;
        std::string svnprefix;
        std::string author;
        std::string log;
        uint datetime;
        int revnum;
        int commitMark;

        QVector<int> merges;

        QVector<std::string> deletedFiles;
        std::string modifiedFiles;

      public:
        inline Transaction()
            : repository(0), datetime(0), revnum(0), commitMark(0)
          {}
        void commit();
        
        void setAuthor(const std::string &author);
        void setDateTime(uint dt);
        void setLog(const std::string &log);

        void noteCopyFromBranch (const std::string &prevbranch, int revFrom);

        void deleteFile(const std::string &path);
        QIODevice *addFile(const std::string &path, int mode, qint64 length);
        void updateSubmodule(Repository const* submodule, int submoduleMark);

        void commitNote(const std::string &noteText, bool append,
          const std::string &commit = std::string());
      };
    
    Repository(
        const Ruleset::Repository &rule,
        bool incremental);
    
    int setupIncremental(int &cutoff, RepoIndex const& all_repositories);
    void restoreLog();
    
    void clear();
    ~Repository();

    void reloadBranches();
    
    int createBranch(
        std::string const& newBranch, int revnum, const std::string &branchFrom, int revFrom);
    
    Repository::Transaction *demandTransaction(
        const std::string &branch, const std::string &svnprefix, int revnum);
    
    int deleteBranch(std::string const& gitRefName, int revnum);

    void createAnnotatedTag(
        std::string const& gitRefName, const std::string &svnprefix,
        int revnum, const std::string &author, uint dt, const std::string &log);
    
    void finalizeTags();
    void prepare_commit(int revnum);
    void commit(
        std::string const& author, uint epoch, std::string const& log);


    static std::string formatMetadataMessage(const std::string &svnprefix, int revnum,
      const std::string &tag = std::string());

    // SUSPICIOUS: Rename this and/or inspect its usage.  It's a very
    // naive test without the same semantics as Branch::exists().
    bool branchExists(const std::string& branch) const;
    const std::string branchNote(const std::string& branch) const;
    void setBranchNote(const std::string& branch, const std::string& noteText);

    std::string get_name() const { return name; }
  private:
    struct Branch
      {
      Branch(int lastChangeRev = neverChanged)
          : lastChangeRev(lastChangeRev)
          , lastSubmoduleListChangeRev(neverChanged)
          , reset(false), deleted(false), modified(false) {}

      bool exists() const
        {
        return lastChangeRev != neverChanged && !marks.isEmpty() && marks.last() != 0;
        }

      static int const neverChanged = 0;
      
      int lastChangeRev;              // which SVN revision contributed the last change to this branch
      int lastSubmoduleListChangeRev; // which SVN revision contributed the last change to the submodule list
      typedef std::map<std::string, Repository const*> Submodules;
      Submodules submodules;
      QVector<int> commits;
      QVector<int> marks;
      std::string note;
      
      bool reset;
      bool deleted;
      bool modified;
      std::string resetCmds;
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

    typedef std::map<std::string,Branch> NamedBranches;
    typedef NamedBranches::value_type NamedBranch;
    
    NamedBranches branches;
    QHash<std::string, AnnotatedTag> annotatedTags;
    std::string name;
  public:
    Repository* submodule_in_repo;
    std::string submodule_path;
  private:
    LoggingQProcess fastImport;
    int commitCount;
    std::set<NamedBranch*> modifiedBranches;
    
    typedef std::map<std::string, Transaction> TransactionMap;
    TransactionMap transactions;
    
    // We might be tempted to use the SVN revision number as the fast-import commit mark.
    // However, a single SVN revision can modify multple branches, and thus lead to multiple
    // commits in the same repo.  So, we need to maintain a separate commit mark counter.
    
    /* starts at 0, and counts up.  */
    int last_commit_mark;

    /* starts at maxMark and counts down. Reset after each SVN revision */
    int next_file_mark;

    bool processHasStarted;
    bool incremental;
    
    void startFastImport();
    void closeFastImport();

    int resetBranch(const std::string &branch, int revnum, int mark, const std::string &resetTo, const std::string &comment);
    int markFrom(const std::string &branchFrom, int branchRevNum, std::string &desc);
    void submoduleChanged(Repository const* submodule, std::string const& gitRefName, int submoduleMark, int revnum);
    void update_dot_gitmodules(std::string const& branch_name, Branch const& b, int revnum);
    void setBranchSubmodules(
        std::string branchName, std::string submoduleNames, RepoIndex const& all_repositories);

    friend class ProcessCache;
    Q_DISABLE_COPY(Repository)
  };

#endif
