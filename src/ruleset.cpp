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

#include <map>
#include <string>
#include <vector>
#include <boost/fusion/adapted/struct/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/home/qi.hpp>

typedef std::map<std::string, std::string> Dictionary;
typedef std::pair<std::string, std::string> DictEntry;

#include <fstream>
#include <iomanip>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/support_multi_pass.hpp>
#include <boost/spirit/include/classic_position_iterator.hpp>
#include <boost/spirit/repository/include/qi_confix.hpp>

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace classic = boost::spirit::classic;

struct BranchRule
  {
  std::size_t min;
  std::size_t max;
  std::string prefix;
  std::string name;
  };

struct RepoRule
  {
  bool abstract;
  std::string name;
  std::string parent;
  std::size_t minrev;
  Dictionary content;
  std::vector<BranchRule> branches;
  std::vector<BranchRule> tags;
  };

BOOST_FUSION_ADAPT_STRUCT(BranchRule,
  (std::size_t, min)
  (std::size_t, max)
  (std::string, prefix)
  (std::string, name)
  )

BOOST_FUSION_ADAPT_STRUCT(RepoRule,
  (bool, abstract)
  (std::string, name)
  (std::string, parent)
  (std::size_t, minrev)
  (Dictionary, content)
  (std::vector<BranchRule>, branches)
  (std::vector<BranchRule>, tags)
  )

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

template<typename Iterator, typename Skipper>
struct RepositoryGrammar: qi::grammar<Iterator, RepoRule(), Skipper>
  {
  RepositoryGrammar() : RepositoryGrammar::base_type(repository_)
    {
    repository_
     %= (qi::matches["abstract"] >> "repository")
      > string_
      > -(':' > string_) // TODO: make sure abstract parent exists and copy branches!
      > '{'
      > (("start_from" > qi::uint_ > ';') | qi::attr(0))
      > -content_
      > -branches_
      > -tags_
      > '}'
      ;
    content_
     %= qi::lit("content")
      > '{'
      > *(string_ > -(':' > string_) > ';')
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
      > ';'
      ;
    string_
     %= qi::char_("a-zA-Z_") >> *qi::char_("a-zA-Z_0-9")
      | qi::lexeme['"' >> +(qi::char_ - '"') >> '"']
      ;
    }
  qi::rule<Iterator, RepoRule(), Skipper> repository_;
  qi::rule<Iterator, Dictionary(), Skipper> content_;
  qi::rule<Iterator, std::vector<BranchRule>(), Skipper> branches_, tags_;
  qi::rule<Iterator, BranchRule(), Skipper> branch_;
  qi::rule<Iterator, std::string(), Skipper> string_;
  };

void inherit(RepoRule& repo_rule, std::vector<RepoRule> const& result)
  {
  if (repo_rule.parent.empty() || repo_rule.parent == repo_rule.name)
    {
    return;
    }
  BOOST_FOREACH(const RepoRule& other, result)
    {
    if (other.name == repo_rule.parent)
      {
      repo_rule.branches.insert(
          repo_rule.branches.end(),
          other.branches.begin(),
          other.branches.end());
      repo_rule.tags.insert(
          repo_rule.tags.end(),
          other.tags.begin(),
          other.tags.end());
      return;
      }
    }
  }

Ruleset::Ruleset(std::string const& filename)
  {
  std::ifstream file(filename.c_str());
  if (!file)
    {
    throw std::runtime_error("cannot read ruleset: " + filename);
    }

  typedef std::istreambuf_iterator<char> BaseIterator;
  BaseIterator in_begin(file);

  typedef boost::spirit::multi_pass<BaseIterator> ForwardIterator;
  ForwardIterator fwd_begin = boost::spirit::make_default_multi_pass(in_begin);
  ForwardIterator fwd_end;

  typedef classic::position_iterator2<ForwardIterator> PosIterator;
  PosIterator begin(fwd_begin, fwd_end);
  PosIterator end;

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

    BOOST_FOREACH(BranchRule const& branch_rule, repo_rule.branches)
      {
      if (branch_rule.max < repo_rule.minrev)
        {
        continue;
        }
      repo.branches.insert(branch_rule.name);

      match.min = std::min(branch_rule.min, repo_rule.minrev);
      match.max = branch_rule.max;
      match.branch = branch_rule.name;

      if (repo_rule.content.empty())
        {
        match.match = branch_rule.prefix;
        match.prefix.clear();
        matches_.push_back(match);
        continue;
        }
      BOOST_FOREACH(DictEntry const& content, repo_rule.content)
        {
        match.match = branch_rule.prefix + content.first;
        match.prefix = content.second;
        matches_.push_back(match);
        }
      }
    repositories_.push_back(repo);
    }
  }
