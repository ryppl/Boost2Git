// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "patrie.hpp"
#include <boost/foreach.hpp>
#undef NDEBUG
#include <cassert>

int main()
  {
  Ruleset::Match rules[4];
  rules[0].match = "abrasives";
  rules[1].match = "abracadabra";
  rules[2].match = "abra";
  rules[3].match = "abrahams";
  
  patrie p;
  BOOST_FOREACH(Ruleset::Match const& m, rules)
    p.insert(&m);

  {
  char test[] = "abracadaver";
  assert(p.longest_match(test, test+sizeof(test)-1) == rules + 2);
  }

  {
  char test[] = "quantico";
  assert(p.longest_match(test, test+sizeof(test)-1) == 0);
  }

  {
  char test[] = "abrahamson";
  assert(p.longest_match(test, test+sizeof(test)-1) == rules + 3);
  }

  };
