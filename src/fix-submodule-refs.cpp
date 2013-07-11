// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include "mark_sha_map.hpp"
#include "to_string.hpp"
#include "AST.hpp"
#include "ruleset.hpp"
#include "marks_file_name.hpp"
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <set>
#include <map>
#include <fstream>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/range/adaptor/map.hpp>

namespace fix_submodule {

struct Repository
  {
  std::string name;
  Repository* submodule_in_repo;
  std::string submodule_path;
  mark_sha_map mark2sha;
  };

typedef std::map<std::string, Repository> RepoStore;
typedef std::map<std::string, Repository const*> SubmoduleMap;

struct Options
  {
  std::string rules_file;
  std::string repo_name;
  };

Options options;

void read_marks_file(Repository& repo)
  {
  std::string marks_path = marks_file_path(repo.name);
  std::ifstream marks(marks_path.c_str());
  if (marks.fail())
    throw std::runtime_error("Couldn't open marks file: " + marks_path);
  marks.exceptions( std::ifstream::badbit );

  char colon, newline;
  std::pair<unsigned long, std::string> mark_sha;
  while (marks >> colon >> mark_sha.first >> mark_sha.second)
    {
    if (colon != ':')
        throw std::runtime_error("Expected colon in marks file: " + marks_path);
    marks.read(&newline, 1);
    if (newline != '\n')
        throw std::runtime_error("Expected newline in marks file: " + marks_path);

    // Insert the mapping.  All kinds of efficiencies are possible
    // here, but I was more interested in getting the logic right.
    mark_sha_map::iterator where = find_sha_pos(repo.mark2sha, mark_sha.first);
    
    // Make sure we're not mapping the same mark twice.
    if (where != repo.mark2sha.end() && where->first == mark_sha.first)
        throw std::runtime_error("Duplicate mark mapping in " + marks_path);
    
    repo.mark2sha.insert(where, mark_sha);
    }
  }

void transform_import_stream(
    Repository const& super_module,
    SubmoduleMap const& submodules
  )
  {
  std::istream& in = std::cin;
  std::ostream& out = std::cout;
    
  char const submodule_prefix[] = "M 160000 ";
  std::size_t const submodule_prefix_length = std::strlen(submodule_prefix);
  std::size_t const sha_length = 40;

  char const data_prefix[] = "data ";
  std::size_t const data_prefix_length = std::strlen(data_prefix);

  in.exceptions( std::istream::badbit );
  out.exceptions( std::ostream::failbit | std::ostream::badbit );
  
  std::string line;
  while (getline(in, line))
    {
    if (boost::starts_with(line, submodule_prefix))
      {
      unsigned long mark = boost::lexical_cast<unsigned long>(
          line.substr(submodule_prefix_length, sha_length));
      std::string submodule_path = line.substr(submodule_prefix_length + sha_length + 1);

      SubmoduleMap::const_iterator sub_repo = submodules.find(submodule_path);
      assert(sub_repo != submodules.end());
        
      mark_sha_map::const_iterator const mark_sha = find_sha_pos(sub_repo->second->mark2sha, mark);
      if (mark_sha->first != mark)
        {
        throw std::runtime_error(
            "unmapped mark " + to_string(mark) + " in " + marks_file_path(sub_repo->second->name)
          );
        }
      out << submodule_prefix << mark_sha->second << " " << submodule_path << "\n";
      }
    else
      {
      out << line << "\n";
      }

    if (boost::starts_with(line, data_prefix))
      {
      std::size_t length = boost::lexical_cast<std::size_t>(
          line.substr(data_prefix_length));
        
      while (length > 0)
        {
        char buf[2048];
        std::size_t num_to_read = std::min(length, sizeof(buf));
        in.read(buf, num_to_read);
        out.write(buf, num_to_read);
        length -= num_to_read;
        }
      }
    }
  }

void run()
  {
  using boost2git::AST;
  
  AST const ast = parse_rules_file(options.rules_file);
  
  RepoStore repo_store;
  BOOST_FOREACH(AST::const_reference repo_rule, ast)
    {
    Repository& repo = repo_store[repo_rule.git_repo_name];
    repo.name = repo_rule.git_repo_name;
    if (!repo_rule.submodule_info.empty())
      {
      assert(repo_rule.submodule_info.size() == 2);
      repo.submodule_in_repo = &repo_store[repo_rule.submodule_info[0]];
      repo.submodule_path = repo_rule.submodule_info[1];
      }
    }

  // Verify that the specified repository actually exists in the map
  RepoStore::iterator const p = repo_store.find(options.repo_name);
  if (p == repo_store.end())
      throw std::runtime_error("repository " + options.repo_name + " not found in ruleset");

  SubmoduleMap submodules;
  // Read all relevant marks files
  BOOST_FOREACH(Repository& repo, repo_store | boost::adaptors::map_values)
    {
    if (repo.submodule_in_repo == &p->second)
      {
        read_marks_file(repo);
        submodules[repo.submodule_path] = &repo;
      }
    }
  transform_import_stream(p->second, submodules);
  }
} // namespace fix_submodule

int main(int argc, char **argv)
  {
  using fix_submodule::options;
  namespace po = boost::program_options;
  po::options_description program_options("Allowed options");
  program_options.add_options()
    ("help,h", "produce help message")
    ("rules", po::value(&options.rules_file)->value_name("FILENAME")->required(),
      "file with the conversion rules")
    ("repo-name", po::value(&options.repo_name)->value_name("IDENTIFIER")->required(),
      "name of the repository to rewrite")
    ;
  po::variables_map variables;
  store(po::command_line_parser(argc, argv)
    .options(program_options)
    .run(), variables);
  notify(variables);
  if (variables.count("help"))
    {
    std::cout << program_options << std::endl;
    return 0;
    }

  try
    {
    fix_submodule::run();
    }
  catch (std::exception& error)
    {
    std::cerr << error.what() << std::endl;
    return -1;
    }
  }
