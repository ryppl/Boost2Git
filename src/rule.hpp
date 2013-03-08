// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef RULE_DWA201337_HPP
# define RULE_DWA201337_HPP

# include <string>

struct Rule
  {
  std::size_t min, max;
  std::string match;
  std::string repository;
  std::string branch;
  std::string prefix;
  bool is_fallback;
  };

#endif // RULE_DWA201337_HPP