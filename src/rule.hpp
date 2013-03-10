// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef RULE_DWA201337_HPP
# define RULE_DWA201337_HPP

# include <string>

struct Rule
  {
  Rule() : min(0), max(0), is_fallback(false) {}

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

  friend bool operator==(Rule const& lhs, Rule const& rhs)
    {
    return lhs.min == rhs.min
      && lhs.max == rhs.max
      && lhs.repository == rhs.repository
      && lhs.branch == rhs.branch
      && lhs.prefix == rhs.prefix
      && lhs.is_fallback == rhs.is_fallback;
    }
  };

#endif // RULE_DWA201337_HPP
