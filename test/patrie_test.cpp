// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "patrie.hpp"
#include <boost/foreach.hpp>
#undef NDEBUG
#include <cassert>

int main()
  {
  Rule rules[5];
  rules[0].match = "abrasives";
  rules[0].min = 1; rules[0].max = 3;
  rules[1].match = "abracadabra";
  rules[1].min = 1; rules[1].max = 3;
  rules[2].match = "abra";
  rules[2].min = 1; rules[2].max = 3;
  rules[3].match = "abrahams";
  rules[3].min = rules[3].max = 1;
  rules[4].match = "abracadabra";
  rules[4].min = 4; rules[4].max = 5;

  patrie p;
  BOOST_FOREACH(Rule const& m, rules)
    p.insert(&m);

  {
  char test[] = "abracadaver";
  assert(p.longest_match(test, test+sizeof(test)-1, 1) == rules + 2);
  assert(p.longest_match(test, test+sizeof(test)-1, 3) == rules + 2);
  assert(p.longest_match(test, test+sizeof(test)-1, 4) == 0);
  }

  {
  char test[] = "abracadabra";
  assert(p.longest_match(test, test+sizeof(test)-1, 3) == rules + 1);
  assert(p.longest_match(test, test+sizeof(test)-1, 4) == rules + 4);
  assert(p.longest_match(test, test+sizeof(test)-1, 9) == 0);
  }

  {
  char test[] = "quantico";
  assert(p.longest_match(test, test+sizeof(test)-1, 6) == 0);
  }

  {
  char test[] = "abrahamson";
  assert(p.longest_match(test, test+sizeof(test)-1, 1) == rules + 3);
  assert(p.longest_match(test, test+sizeof(test)-1, 2) == rules + 2);
  assert(p.longest_match(test, test+sizeof(test)-1, 5) == 0);
  }

  };
