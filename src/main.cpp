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
#include <boost/regex.hpp>

#include <fstream>
#include <limits.h>
#include <stdio.h>

#include "ruleset.hpp"
#include "git_repository.hpp"
#include "svn.hpp"
#include "log.hpp"
#include "importer.hpp"
#include "git_executable.hpp"

#include <utility>
#include <numeric>

Options options;

// Derived from http://stackoverflow.com/questions/445910/create-regex-from-glob-expression/445994#445994
static inline std::string glob_to_regex(std::string val)
{
    boost::trim(val);
    const char* expression = "(\\*)|(\\?)|([[:blank:]])|(\\.|\\+|\\^|\\$|\\[|\\]|\\(|\\)|\\{|\\}|\\\\)";
    const char* format = "(?1\\.*)(?2\\.)(?3\\\\s*)(?4\\\\$&)";
    std::stringstream final;
    std::ostream_iterator<char, char> oi(final);
    boost::regex re;
    re.assign(expression);
    boost::regex_replace(oi, val.begin(), val.end(), re, format, boost::match_default | boost::format_all);
    return final.str();
}

int main(int argc, char **argv)
{
    bool exit_success = false;
    std::string authors_file;
    std::string gitattributes_path;
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
            ("git", po::value(&options.git_executable)->value_name("PATH"), "Path to a Git executable containing the fix described in http://article.gmane.org/gmane.comp.version-control.git/228736")
            ("version,v", "print version string")
            ("quiet,q", "be quiet")
            ("verbose,V", "be verbose")
            ("extra-verbose,X", "be even more verbose")
            ("exit-success", "exit with 0, even if errors occured")
            ("authors", po::value(&authors_file)->value_name("FILENAME"), "map between svn username and email")
            ("svnrepo", po::value(&svn_path)->value_name("PATH")->required(), "path to svn repository")
            ("rules", po::value(&options.rules_file)->value_name("FILENAME")->required(), "file with the conversion rules")
            ("gitattributes,a", po::value(&gitattributes_path)->value_name("PATH"), "A file whose contents to inject as .gitattributes in every Git repository")
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
            std::cout << "Svn2Git 0.9" << std::endl;
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


        // Load the configuration
        Log::info() << "reading ruleset..." << std::endl;

        Ruleset ruleset(options.rules_file);
        Log::info() << "done reading ruleset." << std::endl;

        if (dump_rules)
        {
            std::cout << ruleset.matcher();
            exit(0);
        }

        if (match_path.size() > 0)
        {
            Rule const* r = ruleset.matcher().longest_match(match_path, match_rev);
            std::cout <<  "The path " << (r ? "was" : "wasn't") << " matched" << std::endl;
            exit(r ? 0 : 1);
        }

        Log::info() << "Opening SVN repository at " << svn_path << std::endl;
        svn svn_repo(svn_path, authors_file);

        if (!gitattributes_path.empty())
        {
            std::ifstream ifs(gitattributes_path);
            if (ifs.fail())
                throw std::runtime_error("Couldn't open .gitattributes file: " + gitattributes_path);
            ifs.exceptions( std::ifstream::badbit );
            ifs.seekg(0, std::ios::end);
            options.gitattributes_text.resize(ifs.tellg());
            ifs.seekg(0, std::ios::beg);
            ifs.read(&options.gitattributes_text[0], options.gitattributes_text.size());
            ifs.seekg(0, std::ios::beg);
            boost::property_tree::read_gitattributes(ifs, options.gitattributes_tree);
            //boost::property_tree::write_info(std::cout, options.gitattributes_tree);
            // Convert all globs to precompiled regexs for speed
            for(auto it=options.gitattributes_tree.begin(); it!=options.gitattributes_tree.end(); ++it)
            {
                if(it->first=="*") continue;
                std::string regex_str=glob_to_regex(it->first);
                //std::cout << it->first << " => " << regex_str << std::endl;
                options.glob_cache.push_back(std::make_pair(std::regex(regex_str), it));
            }
        }

        Log::info() << "preparing repositories and import processes..." << std::endl;
        importer imp(svn_repo, ruleset);
        Log::info() << "done preparing repositories and import processes." << std::endl;

        if (max_rev < 1)
            max_rev = svn_repo.latest_revision();

        Log::info() << "Using git executable: " << git_executable() << std::endl;

        for (int i = std::max(resume_from, imp.last_valid_svn_revision()); ++i <= max_rev;)
            imp.import_revision(i);

        coverage::report();
    }
    catch (std::exception const& error)
    {
        Log::error() << error.what() << "\n\n";
        return EXIT_FAILURE;
    }
    int result = Log::result();
    return exit_success ? EXIT_SUCCESS : result;
}
