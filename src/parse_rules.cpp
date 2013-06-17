// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include "parse_rules.hpp"

#include <boost/spirit/home/qi.hpp>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/support_multi_pass.hpp>
#include <boost/spirit/include/classic_position_iterator.hpp>
#include <boost/spirit/repository/include/qi_confix.hpp>
#include <boost/spirit/repository/include/qi_iter_pos.hpp>
#include <boost/spirit/home/phoenix/bind/bind_function.hpp>
#include <boost/algorithm/string/predicate.hpp>

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
// make sure git_path ends with "/" too (unless it is empty).
static void beef_up_content(std::vector<ContentRule> &content_vector)
  {
  BOOST_FOREACH(ContentRule &content, content_vector)
    {
    std::string const& prefix = content.svn_path;
    std::string& git_path = content.git_path;
    if (prefix.empty() || git_path.empty())
      {
      continue;
      }
    if (boost::ends_with(prefix, "/") && !boost::ends_with(git_path, "/"))
      {
      git_path += "/";
      }
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
      > -content_[phoenix::bind(beef_up_content, qi::_1)]
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

AST parse_rules_file(std::string filename)
  {
  AST ast;
  
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
    qi::phrase_parse(begin, end, qi::eps > +grammar, comment, ast);
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
  return ast;
  }


