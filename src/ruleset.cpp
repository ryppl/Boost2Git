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

#include <string>
#include <vector>
#include <boost/spirit/home/qi.hpp>

#include <fstream>
#include <iomanip>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/support_multi_pass.hpp>
#include <boost/spirit/include/classic_position_iterator.hpp>
#include <boost/spirit/repository/include/qi_confix.hpp>
#include <boost/spirit/repository/include/qi_iter_pos.hpp>
#include <boost/spirit/home/phoenix/bind/bind_function.hpp>
#include <boost/fusion/include/make_vector.hpp>


namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace classic = boost::spirit::classic;
namespace phoenix = boost::phoenix;

namespace boost2git
{

typedef std::istreambuf_iterator<char> BaseIterator;
typedef boost::spirit::multi_pass<BaseIterator> ForwardIterator;
typedef classic::position_iterator2<ForwardIterator> PosIterator;

static void get_line(int& line, PosIterator const& iterator)
  {
  line = iterator.get_position().line;
  }
static void set_git_ref_qualifier(boost2git::BranchRule& branch, char const* qualifier)
  {
  branch.git_ref_qualifier = qualifier;
  }
} // namespace boost2git

using namespace boost2git;
using boost::spirit::make_default_multi_pass;

// robustness: if prefix of content ends with "/",
// make sure replace ends with "/" too (unless it is empty).
void beef_up_content(ContentRule& content)
  {
  std::string const& prefix = content.prefix;
  std::string& replace = content.replace;
  if (prefix.empty() || replace.empty())
    {
    continue;
    }
  if (boost::ends_with(prefix, "/") && !boost::ends_with(replace, "/"))
    {
    replace += "/";
    }
  }

template<typename Iterator, typename Skipper>
struct RepositoryGrammar: qi::grammar<Iterator, RepoRule(), Skipper>
  {
  RepositoryGrammar() : RepositoryGrammar::base_type(repository_)
    {
    repository_
     %= (qi::matches["abstract"] >> "repository")
      > line_number_
      > string_
      > -(':' > string_ % ',')
      > '{'
      > -(qi::lit("submodule") > qi::lit("of") > string_ > ':' > string_ > ';')
      > (("minrev" > qi::uint_ > ';') | qi::attr(0))
      > (("maxrev" > qi::uint_ > ';') | qi::attr(UINT_MAX))
      > -content_[beef_up_content]
      > -branches_
      > -tags_
      > '}'
      ;
    content_
     %= qi::lit("content")
      > '{'
      > +(string_ > qi::matches['?'] > -(':' > string_) > line_number_ > ';')
      > '}'
      ;
    branches_
     %= qi::lit("branches")
      > '{'
      > +(branch_[phoenix::bind(set_git_ref_qualifier, qi::_1, "refs/heads/")])
      > '}'
      ;
    tags_
     %= qi::lit("tags")
      > '{'
      > +branch_[phoenix::bind(set_git_ref_qualifier, qi::_1, "refs/tags/")]
      > '}'
      ;
    branch_
     %= '['
      > (qi::uint_ | qi::attr(0))
      > ':'
      > (qi::uint_ | qi::attr(UINT_MAX))
      > ']'
      > string_
      > ':'
      > string_
      > line_number_
      > ';'
      ;
    string_
     %= qi::char_("a-zA-Z_") >> *qi::char_("a-zA-Z_0-9")
      | qi::lexeme['"' >> +(qi::char_ - '"') >> '"']
      ;
    line_number_ = boost::spirit::repository::qi::iter_pos
      [
      phoenix::bind(get_line, qi::_val, qi::_1)
      ];
    }
  qi::rule<Iterator, RepoRule(), Skipper> repository_;
  qi::rule<Iterator, std::vector<ContentRule>(), Skipper> content_;
  qi::rule<Iterator, std::vector<BranchRule>(), Skipper> branches_, tags_;
  qi::rule<Iterator, BranchRule(), Skipper> branch_;
  qi::rule<Iterator, std::string(), Skipper> string_;
  qi::rule<Iterator, int(), Skipper> line_number_;
  };

boost2git::RepoRule fallback_repo(
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

boost2git::BranchRule fallback_branch(
  boost::fusion::make_vector(
      0, /* std::size_t min */
      std::size_t(-1), /* std::size_t max */
      "", /* std::string prefix */
      "master", /* std::string name */
      0, /* int line */
      "refs/heads/"
    ));

boost2git::ContentRule fallback_content(
  boost::fusion::make_vector(
      "", /* std::string prefix */
      true, /* bool is_fallback */
      "", /* std::string replace */
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
    search_target.name = base_name;
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
  {
  std::ifstream file(filename.c_str());
  if (!file)
    {
    throw std::runtime_error("cannot read ruleset: " + filename);
    }

  BaseIterator in_begin(file);
  ForwardIterator fwd_begin = make_default_multi_pass(in_begin), fwd_end;
  PosIterator begin(fwd_begin, fwd_end), end;

  BOOST_AUTO(comment
    , ascii::space
    | boost::spirit::repository::confix("/*", "*/")[*(qi::char_ - "*/")]
    | boost::spirit::repository::confix("//", qi::eol)[*(qi::char_ - qi::eol)]
    );
  RepositoryGrammar<PosIterator, BOOST_TYPEOF(comment)> grammar;
  try
    {
    qi::phrase_parse(begin, end, qi::eps > +grammar, comment, ast_);
    }
  catch (const qi::expectation_failure<PosIterator>& error)
    {
    typedef classic::file_position_base<std::string> Position;
    const Position& pos = error.first.get_position();
    std::stringstream msg;
    msg << "parse error at file " << filename
        << " line " << pos.line
        << " column " << pos.column << std::endl
        << "'" << error.first.get_currentline() << "'" << std::endl
        << std::setw(pos.column) << " " << "^- here"
      ;
    throw std::runtime_error(msg.str());
    }

  BOOST_FOREACH(RepoRule const& repo_rule, ast_)
    {  
    if (repo_rule.abstract)
      {
      continue;
      }

    typedef std::vector<BranchRule const*> BranchRules;
    BranchRules branches;
    BranchRules tags;
    std::vector<ContentRule const*> content;
    collect_rule_components(ast_, repo_rule, branches, tags, content);
    
    Repository repo;
    repo.name = repo_rule.name;
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
          matches_.insert(Match(&repo_rule, branch_rule, 0));
          }
        else
          {
          BOOST_FOREACH(ContentRule const* content_rule, content)
            {
            matches_.insert(
                Match(&repo_rule, branch_rule, content_rule));
            }
          }
        }
      }
    repositories_.push_back(repo);
    }
  Repository repo;
  repo.name = fallback.repo_rule->name;
  repo.branches.insert(fallback.branch_rule);
  repositories_.push_back(repo);
  }
