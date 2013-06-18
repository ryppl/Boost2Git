/*
 *  Copyright (C) 2013 Daniel Pfeifer <daniel@pfeifer-mail.de>
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

#include "ruleset.hpp"
#include "to_string.hpp"

#include <boost/foreach.hpp>
#include <string>
#include <vector>

#include <boost/fusion/include/make_vector.hpp>

using namespace boost2git;

RepoRule fallback_repo(
  boost::fusion::make_vector(
  false, /* bool abstract */
  0, /* int line */
  "svn2git-fallback", /* std::string name */
  0, /* std::vector<std::string> bases */
  0, /* std::vector<std::string> submodule_info */
  0, /* std::size_t minrev */
  UINT_MAX, /* std::size_t maxrev */
  0, /* std::vector<boost2git::ContentRule> content */
  0, /* std::vector<boost2git::BranchRule> branch_rules */
  0 /* std::vector<boost2git::BranchRule> tag_rules */
    ));

BranchRule fallback_branch(
  boost::fusion::make_vector(
      0, /* std::size_t min */
      std::size_t(-1), /* std::size_t max */
      "", /* std::string prefix */
      "master", /* std::string name */
      0, /* int line */
      "refs/heads/"
    ));

ContentRule fallback_content(
  boost::fusion::make_vector(
      "", /* std::string prefix */
      true, /* bool is_fallback */
      "", /* std::string git_path */
      0 /* int line */
    ));

Ruleset::Match const Ruleset::fallback(
    &fallback_repo, &fallback_branch, &fallback_content);

template <class Seq1, class Seq2>
void append_addresses(Seq1& target, Seq2 const& source)
  {
  BOOST_FOREACH(typename Seq2::const_reference x, source)
    target.push_back(&x);
  }
          
static void
collect_rule_components(
    AST const& ast,
    boost2git::RepoRule const& repo_rule,
    std::vector<boost2git::BranchRule const*>& branches,
    std::vector<boost2git::BranchRule const*>& tags,
    std::vector<boost2git::ContentRule const*>& content)
  {
  boost2git::RepoRule search_target;

  BOOST_FOREACH(std::string const& base_name, repo_rule.bases)
    {
    search_target.git_repo_name = base_name;
    std::pair<AST::iterator,AST::iterator> base_range = ast.equal_range(search_target);

    if (base_range.first == base_range.second)
      {
      throw std::runtime_error(
         options.rules_file + ":" + to_string(repo_rule.line) + ": error: " +
         "base repository rule '" + base_name+ "' not found");
      }
    
    for (AST::iterator p = base_range.first; p != base_range.second; ++p)
      {
      collect_rule_components(ast, *p, branches, tags, content);
      }
    }

  append_addresses(branches, repo_rule.branch_rules);
  append_addresses(tags, repo_rule.tag_rules);
  append_addresses(content, repo_rule.content_rules);
  }

Ruleset::Ruleset(std::string const& filename)
    : ast_(parse_rules_file(filename))
  {
  BOOST_FOREACH(RepoRule const& repo_rule, ast_)
    {  
    if (repo_rule.is_abstract)
      {
      continue;
      }

    typedef std::vector<BranchRule const*> BranchRules;
    BranchRules branches;
    BranchRules tags;
    std::vector<ContentRule const*> content;
    collect_rule_components(ast_, repo_rule, branches, tags, content);
    
    Repository repo;
    repo.name = repo_rule.git_repo_name;
    if (!repo_rule.submodule_info.empty())
      {
      assert(repo_rule.submodule_info.size() == 2);
      repo.submodule_in_repo = repo_rule.submodule_info[0];
      repo.submodule_path = repo_rule.submodule_info[1];
      }

    BranchRules const* ref_rulesets[] = { &branches, &tags };
      
    BOOST_FOREACH(BranchRules const* rules, ref_rulesets)
      {
      BOOST_FOREACH(BranchRule const* branch_rule, *rules)
        {
        assert(std::strcmp(branch_rule->git_ref_qualifier, "refs/heads/") == 0
              || std::strcmp(branch_rule->git_ref_qualifier, "refs/tags/") == 0);
        
        std::size_t minrev = std::max(branch_rule->min, repo_rule.minrev);
        std::size_t maxrev = std::min(branch_rule->max, repo_rule.maxrev);
        if (minrev > maxrev)
          {
          continue;
          }
      
        repo.branches.insert(branch_rule);

        if (repo_rule.content_rules.empty())
          {
          matcher_.insert(Match(&repo_rule, branch_rule, 0));
          }
        else
          {
          BOOST_FOREACH(ContentRule const* content_rule, content)
            {
            matcher_.insert(
                Match(&repo_rule, branch_rule, content_rule));
            }
          }
        }
      }
    repositories_.push_back(repo);
    }
  Repository repo;
  repo.name = fallback.repo_rule->git_repo_name;
  repo.branches.insert(fallback.branch_rule);
  repositories_.push_back(repo);
  }

void report_overlap(Rule const* rule0, Rule const* rule1)
{
    throw std::runtime_error(
        "found duplicate rule: " + rule1->svn_path()
        + "\n"
        + options.rules_file + ":" + to_string(rule1->branch_rule->line)
        + ": error: duplicate rule branch fragment\n"
          
        + options.rules_file + ":" + to_string(rule1->content_rule->line)
        + ": error: duplicate rule content fragment\nerror: see earlier definition:\n"
          
        + options.rules_file + ":" + to_string(rule0->branch_rule->line)
        + ": error: previous branch fragment\n"
          
        + options.rules_file + ":" + to_string(rule0->content_rule->line)
        + ": error: previous content fragment");
}

