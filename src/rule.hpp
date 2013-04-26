// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef RULE_DWA201337_HPP
# define RULE_DWA201337_HPP

# include <string>
# include <ostream>
# include <climits>
# include "AST.hpp"

struct Rule
  {
  Rule()
      : min(0)
      , max(UINT_MAX)
      , is_fallback(false)
      , repo_rule(0)
      , branch_rule(0)
      , content_rule(0)
    {
    }

  Rule(
      std::size_t min, std::size_t max,
      std::string match,
      std::string repository,
      std::string branch,
      std::string prefix,
      bool is_fallback)
      : min(min), max(max),
      match(match), repository(repository), branch(branch),
      prefix(prefix), is_fallback(is_fallback)
    {}

  std::size_t min, max;
  std::string match;
  std::string repository;
  std::string branch;
  std::string prefix;
  bool is_fallback;

  // Constituent rules in the AST
  boost2git::RepoRule const* repo_rule;
  boost2git::BranchRule const* branch_rule;
  boost2git::ContentRule const* content_rule;

  friend bool operator==(Rule const& lhs, Rule const& rhs)
    {
    return lhs.min == rhs.min
      && lhs.max == rhs.max
      && lhs.repository == rhs.repository
      && lhs.branch == rhs.branch
      && lhs.prefix == rhs.prefix
      && lhs.is_fallback == rhs.is_fallback
      && lhs.repo_rule == rhs.repo_rule
      && lhs.branch_rule == rhs.branch_rule
      && lhs.content_rule == rhs.content_rule;
    }
  };

inline std::ostream& operator<<(std::ostream& os, Rule const& r)
  {
  if (r.min != 0 || r.max != UINT_MAX)
    {
    os << "[";
    if (r.min != 0) os << r.min;
    os << ":";
    if (r.max != UINT_MAX)
        os << r.max;
    os << "] ";
    }
  os << r.repository << ".git:<" << r.branch << ">:/" << r.prefix;
  return os;
  }

#endif // RULE_DWA201337_HPP
