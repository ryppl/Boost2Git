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

#include <string>
#include <vector>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/fusion/adapted/struct/define_struct.hpp>
#include <boost/spirit/home/qi.hpp>

#include <fstream>
#include <iomanip>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/support_multi_pass.hpp>
#include <boost/spirit/include/classic_position_iterator.hpp>
#include <boost/spirit/repository/include/qi_confix.hpp>
#include <boost/spirit/repository/include/qi_iter_pos.hpp>
#include <boost/spirit/home/phoenix/bind/bind_function.hpp>

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

} // namespace boost2git

using namespace boost2git;
using boost::spirit::make_default_multi_pass;

BOOST_FUSION_DEFINE_STRUCT((boost2git), ContentRule,
  (std::string, prefix)
  (bool, is_fallback)
  (std::string, replace)
  (int, line)
  )

BOOST_FUSION_DEFINE_STRUCT((boost2git), BranchRule,
  (std::size_t, min)
  (std::size_t, max)
  (std::string, prefix)
  (std::string, name)
  (int, line)
  )

BOOST_FUSION_DEFINE_STRUCT((boost2git), RepoRule,
  (bool, abstract)
  (int, line)
  (std::string, name)
  (std::string, parent)
  (std::size_t, minrev)
  (std::size_t, maxrev)
  (std::vector<ContentRule>, content)
  (std::vector<BranchRule>, branch_rules)
  (std::vector<BranchRule>, tag_rules)
  )

template<typename Iterator, typename Skipper>
struct RepositoryGrammar: qi::grammar<Iterator, RepoRule(), Skipper>
  {
  RepositoryGrammar() : RepositoryGrammar::base_type(repository_)
    {
    repository_
     %= (qi::matches["abstract"] >> "repository")
      > line_number_
      > string_
      > -(':' > string_) // TODO: make sure abstract parent exists and copy branches!
      > '{'
      > (("minrev" > qi::uint_ > ';') | qi::attr(0))
      > (("maxrev" > qi::uint_ > ';') | qi::attr(UINT_MAX))
      > -content_
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
      > +branch_
      > '}'
      ;
    tags_
     %= qi::lit("tags")
      > '{'
      > +branch_
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

static void inherit(RepoRule& repo_rule, std::vector<RepoRule> const& result)
  {
  if (repo_rule.parent.empty() || repo_rule.parent == repo_rule.name)
    {
    return;
    }
  BOOST_FOREACH(const RepoRule& other, result)
    {
    if (other.name == repo_rule.parent)
      {
      repo_rule.branch_rules.insert(
          repo_rule.branch_rules.end(),
          other.branch_rules.begin(),
          other.branch_rules.end());
      repo_rule.tag_rules.insert(
          repo_rule.tag_rules.end(),
          other.tag_rules.begin(),
          other.tag_rules.end());
      return;
      }
    }
  }

static std::string path_append(std::string path, std::string const& append)
  {
  if (boost::ends_with(path, "/"))
    {
    if (boost::starts_with(append, "/"))
      {
      path.resize(path.size() - 1);
      }
    }
  else
    {
    if (!boost::starts_with(append, "/"))
      {
      path += '/';
      }
    }
  return path + append;
  }

Ruleset::Match Ruleset::fallback(
  0,
  UINT_MAX,
  "/",
  "svn2git-fallback",
  "refs/heads/master",
  "",
  true
  );

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
  std::vector<RepoRule> result;
  RepositoryGrammar<PosIterator, BOOST_TYPEOF(comment)> grammar;
  try
    {
    qi::phrase_parse(begin, end, qi::eps > +grammar, comment, result);
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
  BOOST_FOREACH(RepoRule& repo_rule, result)
    {
    inherit(repo_rule, result);
    if (repo_rule.abstract)
      {
      continue;
      }

    Repository repo;
    repo.name = repo_rule.name;

    Match match;
    match.repository = repo_rule.name;
    match.repo_line = repo_rule.line;

    typedef std::pair<std::vector<BranchRule>*, char const*> RulesAndPrefix;
    RulesAndPrefix const ref_rulesets[] =
      {
      std::make_pair(&repo_rule.branch_rules, "heads"),
      std::make_pair(&repo_rule.tag_rules, "tags")
      };
      
    BOOST_FOREACH(RulesAndPrefix const& rules_and_prefix, ref_rulesets)
      {
      BOOST_FOREACH(BranchRule const& branch_rule, *rules_and_prefix.first)
        {
        if (branch_rule.max < repo_rule.minrev)
          {
          continue;
          }
      
        std::string const& ref_name = qualify_ref(branch_rule.name, rules_and_prefix.second);
        repo.branches.insert(ref_name);

        match.branch = ref_name;
        match.branch_line = branch_rule.line;
        match.min = std::max(branch_rule.min, repo_rule.minrev);
        match.max = std::min(branch_rule.max, repo_rule.maxrev);
        if (match.min > match.max)
          {
          continue;
          }

        if (repo_rule.content.empty())
          {
          match.match = branch_rule.prefix;
          match.prefix.clear();
          match.is_fallback = false;
          matches_.insert(match);
          continue;
          }
        BOOST_FOREACH(ContentRule const& content, repo_rule.content)
          {
          match.match = path_append(branch_rule.prefix, content.prefix);
          match.prefix = content.replace;
          match.is_fallback = content.is_fallback;
          match.content_line = content.line;
          matches_.insert(match);
          }
        }
      }
    repositories_.push_back(repo);
    }
  Repository repo;
  repo.name = fallback.repository;
  repo.branches.insert(fallback.branch);
  repositories_.push_back(repo);
  }
