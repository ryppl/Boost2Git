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

#include <boost/program_options.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QDebug>

#include <limits.h>
#include <stdio.h>

#include "rules_list.hpp"
#include "repository.h"
#include "svn.h"

QHash<QByteArray, QByteArray> loadIdentityMapFile(const QString &fileName)
  {
  QHash<QByteArray, QByteArray> result;
  if (fileName.isEmpty())
    {
    return result;
    }
  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly))
    {
    fprintf(
        stderr,
        "Could not open file %s: %s",
        qPrintable(fileName),
        qPrintable(file.errorString()));
    return result;
    }
  while (!file.atEnd())
    {
    QByteArray line = file.readLine();
    int comment_pos = line.indexOf('#');
    if (comment_pos != -1)
      {
      line.truncate(comment_pos);
      }
    line = line.trimmed();
    int space = line.indexOf(' ');
    if (space == -1)
      {
      continue; // invalid line
      }
    // Support git-svn author files, too
    // - svn2git native:  loginname Joe User <user@example.com>
    // - git-svn:         loginname = Joe User <user@example.com>
    int rightspace = line.indexOf(" = ");
    int leftspace = space;
    if (rightspace == -1)
      {
      rightspace = space;
      }
    else
      {
      leftspace = rightspace;
      rightspace += 2;
      }
    QByteArray realname = line.mid(rightspace).trimmed();
    line.truncate(leftspace);
    result.insert(line, realname);
    }
  file.close();
  return result;
  }

Options options;

int main(int argc, char **argv)
  {
  std::string authors;
  std::string svn_path;
  std::vector<std::string> rule_files;
  int resume_from = INT_MAX;
  int max_rev = 0;
  try
    {
    namespace po = boost::program_options;
    po::options_description allowed_options("Allowed options");
    allowed_options.add_options()
      ("help,h", "produce help message")
      ("version,v", "print version string")
      ("authors", po::value(&authors)->value_name("FILENAME"), "map between svn username and email")
      ("add-metadata", "if passed, each git commit will have svn commit info")
      ("add-metadata-notes", "if passed, each git commit will have notes with svn commit info")
      ("resume-from", po::value(&resume_from)->value_name("REVISION"), "start importing at svn revision number")
      ("max-rev", po::value(&max_rev)->value_name("REVISION"), "stop importing at svn revision number")
      ("dry-run", "don't actually write anything")
      ("debug-rules", "print what rule is being used for each file")
      ("commit-interval", po::value(&options.commit_interval)->value_name("NUMBER")->default_value(10000), "if passed the cache will be flushed to git every NUMBER of commits")
      ("stats", "after a run print some statistics about the rules")
      ("svn-branches", "Use the contents of SVN when creating branches, Note: SVN tags are branches as well")
      ;
    po::options_description hidden_options("Hidden options");
    hidden_options.add_options()
      ("svn", po::value(&svn_path)->required())
      ("rule", po::value(&rule_files))
      ;
    po::options_description all_options("All options");
    all_options
      .add(allowed_options)
      .add(hidden_options)
      ;
    po::positional_options_description positional_options;
    positional_options
      .add("svn", 1)
      .add("rule", -1)
      ;
    po::variables_map variables;
    store(po::command_line_parser(argc, argv)
      .options(all_options)
      .positional(positional_options)
      .run(), variables);
    if (variables.count("help"))
      {
      std::cout << allowed_options << std::endl;
      return 0;
      }
    if (variables.count("version"))
      {
      std::cout << "Svn2Git 0.1" << std::endl;
      return 0;
      }
    options.add_metadata = variables.count("add-metadata");
    options.add_metadata_notes = variables.count("add-metadata-notes");
    options.dry_run = variables.count("dry-run");
    options.debug_rules = variables.count("debug-rules");
    options.svn_branches = variables.count("svn-branches");
    notify(variables);
    }
  catch (std::exception& error)
    {
    std::cout << error.what() << std::endl;
    return -1;
    }

  QCoreApplication app(argc, argv);
  // Load the configuration
  RulesList rulesList(rule_files);

  QHash<QString, Repository*> repositories;

  int cutoff = resume_from;

retry:
  int min_rev = 1;
  foreach (Rules::Repository rule, rulesList.allRepositories())
    {
    Repository *repo = new Repository(rule);
    if (!repo)
      {
      return EXIT_FAILURE;
      }
    repositories.insert(rule.name, repo);

    int repo_next = repo->setupIncremental(cutoff);
    if (cutoff < resume_from && repo_next == cutoff)
      {
      /*
       * Restore the log file so we fail the next time
       * svn2git is invoked with the same arguments
       */
      repo->restoreLog();
      }

    if (cutoff < min_rev)
      { /*
       * We've rewound before the last revision of some
       * repository that we've already seen.  Start over
       * from the beginning.  (since cutoff is decreasing,
       * we're sure we'll make forward progress eventually)
       */
      goto retry;
      }
    if (min_rev < repo_next)
      {
      min_rev = repo_next;
      }
    }

  if (cutoff < resume_from)
    {
    qCritical()
      << "Cannot resume from"
      << resume_from
      << "as there are errors in revision"
      << cutoff
      ;
    return EXIT_FAILURE;
    }

  if (min_rev < resume_from)
    {
    qDebug()
      << "skipping revisions"
      << min_rev
      << "to"
      << resume_from - 1
      << "as requested"
      ;
    }
  if (resume_from)
    {
    min_rev = resume_from;
    }

  Svn::initialize();
  Svn svn(QString::fromStdString(svn_path));
  svn.setMatchRules(rulesList.allMatchRules());
  svn.setRepositories(repositories);
  svn.setIdentityMap(loadIdentityMapFile(QString::fromStdString(authors)));

  if (max_rev < 1)
    {
    max_rev = svn.youngestRevision();
    }
  bool errors = false;
  for (int i = min_rev; i <= max_rev; ++i)
    {
    if (!svn.exportRevision(i))
      {
      errors = true;
      break;
      }
    }
  foreach(Repository *repo, repositories)
    {
    repo->finalizeTags();
    delete repo;
    }
  return errors ? EXIT_FAILURE : EXIT_SUCCESS;
  }
