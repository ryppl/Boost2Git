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
#include <boost/foreach.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QDebug>

#include <fstream>
#include <limits.h>
#include <stdio.h>

#include "apr_init.hpp"
#include "authors.hpp"
#include "ruleset.hpp"
#include "repository.h"
#include "svn.h"
#include "log.hpp"

Options options;

static std::set<int> load_ignore(std::string const& filename)
  {
  std::string line;
  std::set<int> result;
  std::ifstream file(filename.c_str());
  while (std::getline(file, line))
    {
    if (line.empty() || line[0] == '#')
      {
      continue;
      }
    result.insert(boost::lexical_cast<int>(line));
    }
  return result;
  }

int main(int argc, char **argv)
  {
  bool exit_success = false;
  std::string authors_file;
  std::string ignore_file;
  std::string svn_path;
  int resume_from = 0;
  int max_rev = 0;
  bool dump_rules = false;
  std::string match_path;
  int match_rev = 0;
  try
    {
    namespace po = boost::program_options;
    po::options_description program_options("Allowed options");
    program_options.add_options()
      ("help,h", "produce help message")
      ("version,v", "print version string")
      ("quiet,q", "be quiet")
      ("verbose,V", "be verbose")
      ("extra-verbose,VV", "be even more verbose")
      ("exit-success", "exit with 0, even if errors occured")
      ("authors", po::value(&authors_file)->value_name("FILENAME"), "map between svn username and email")
      ("svnrepo", po::value(&svn_path)->value_name("PATH")->required(), "path to svn repository")
      ("ignore", po::value(&ignore_file)->value_name("FILENAME"), "file with revisions to ignore")
      ("rules", po::value(&options.rules_file)->value_name("FILENAME")->required(), "file with the conversion rules")
      ("dry-run", "Write no Git repositories")
      ("coverage", "Dump an analysis of rule coverage")
      ("add-metadata", "if passed, each git commit will have svn commit info")
      ("add-metadata-notes", "if passed, each git commit will have notes with svn commit info")
      ("resume-from", po::value(&resume_from)->value_name("REVISION"), "start importing at svn revision number")
      ("max-rev", po::value(&max_rev)->value_name("REVISION"), "stop importing at svn revision number")
      ("debug-rules", "print what rule is being used for each file")
      ("commit-interval", po::value(&options.commit_interval)->value_name("NUMBER")->default_value(10000), "if passed the cache will be flushed to git every NUMBER of commits")
      ("svn-branches", "Use the contents of SVN when creating branches, Note: SVN tags are branches as well")
      ("dump-rules", "Dump the contents of the rule trie and exit")
      ("match-path", po::value(&match_path)->value_name("PATH"), "Path to match in a quick ruleset test")
      ("match-rev", po::value(&match_rev)->value_name("REVISION"), "Optional revision to match in a quick ruleset test")
      ;
    po::variables_map variables;
    store(po::command_line_parser(argc, argv)
      .options(program_options)
      .run(), variables);
    if (variables.count("help"))
      {
      std::cout << program_options << std::endl;
      return 0;
      }
    if (variables.count("version"))
      {
      std::cout << "Svn2Git 0.1" << std::endl;
      return 0;
      }
    if (variables.count("quiet"))
      {
      Log::set_level(Log::Warning);
      }
    if (variables.count("verbose"))
      {
      Log::set_level(Log::Debug);
      }
    if (variables.count("extra-verbose"))
      {
      Log::set_level(Log::Trace);
      }
    if (variables.count("exit-success"))
      {
      exit_success = true;
      }
    dump_rules = variables.count("dump-rules") > 0;
    options.add_metadata = variables.count("add-metadata");
    options.add_metadata_notes = variables.count("add-metadata-notes");
    options.dry_run = variables.count("dry-run");
    options.coverage = variables.count("coverage");
    options.debug_rules = variables.count("debug-rules");
    options.svn_branches = variables.count("svn-branches");
    notify(variables);
    }
  catch (std::exception& error)
    {
    std::cout << error.what() << std::endl;
    return -1;
    }

  AprInit apr_init;
  QCoreApplication app(argc, argv);

  try {

    Authors authors(authors_file);

    // Load the configuration
    Ruleset ruleset(options.rules_file);
    std::set<int> ignore_revisions = load_ignore(ignore_file);

    if (dump_rules)
      {
      std::cout << ruleset.matches();
      exit(0);
      }

    if (match_path.size() > 0)
      {
      Rule const* r = ruleset.matches().longest_match(match_path, match_rev);
      std::cout <<  "The path " << (r ? "was" : "wasn't") << " matched" << std::endl;
      exit(r ? 0 : 1);
      }
    
    QHash<QString, Repository*> repositories;

    int cutoff = resume_from ? resume_from : INT_MAX;

retry:
    int min_rev = 1;
    BOOST_FOREACH(Ruleset::Repository const& rule, ruleset.repositories())
      {
      Repository *repo = new Repository(rule, resume_from != 0, repositories);
      if (!repo)
        {
        return EXIT_FAILURE;
        }
      repositories.insert(QString::fromStdString(rule.name), repo);

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
      std::cerr
        << "Cannot resume from"
        << resume_from
        << "as there are errors in revision"
        << cutoff
        ;
      return EXIT_FAILURE;
      }

    if (min_rev < resume_from)
      {
      std::cout
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

    Svn svn(svn_path, authors, ruleset);
    svn.setRepositories(repositories);

    if (max_rev < 1)
      {
      max_rev = svn.youngestRevision();
      }
    bool errors = false;
    for (int i = min_rev; i <= max_rev; ++i)
      {
      Log::set_revision(i);
      if (ignore_revisions.find(i) != ignore_revisions.end())
        {
        continue;
        }
      if (!svn.exportRevision(i))
        {
        errors = true;
        break;
        }
      }
    coverage::report();
    foreach(Repository *repo, repositories)
      {
      repo->finalizeTags();
      delete repo;
      }
    return errors ? EXIT_FAILURE : EXIT_SUCCESS;
    }
  catch (std::exception const& error)
    {
    Log::error() << error.what() << "\n\n";
    return -1;
    }
  int result = Log::result();
  return exit_success ? EXIT_SUCCESS : result;
  }
