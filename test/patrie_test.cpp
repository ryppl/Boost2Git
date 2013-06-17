// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#undef NDEBUG
#include "patrie.hpp"
#include <boost/fusion/adapted/struct/define_struct.hpp>
#include <cassert>

namespace patrie_test {

struct Rule
{
    std::string match;
    std::string git_address_;
    int min;
    int max;

    std::string svn_path() const { return match; }
    std::string git_address() const { return git_address_; }
    void report_overlap() const {}
};

bool operator==(Rule const& lhs, Rule const& rhs)
{
    return lhs.match == rhs.match && lhs.min == rhs.min && lhs.max == rhs.max;
}

void report_overlap(Rule const* rule0, Rule const* rule1) 
{
    assert(!"should never get here");
}

}

int main()
{
    using patrie_test::Rule;
    Rule rules[5]
    = {
        {"abrasives",   "a:b:foo/bar", 1, 3}, // 0
        {"abracadabra", "a:b:baz",     1, 3}, // 1
        {"abra",        "a:b:fubar",   1, 3}, // 2
        {"abrahams",    "a:b:fu/bar",  1, 1}, // 3
        {"abracadabra", "a:b:fu/bar",  4, 5}  // 4
    };

    patrie<Rule> p;
    for(auto const& m: rules)
        p.insert(m);

    {
        std::string test = "abracadaver";
        assert(*p.longest_match(test, 1) == rules[2]);
        assert(*p.longest_match(test, 3) == rules[2]);
        assert(p.longest_match(test, 4) == 0);
    }

    {
        std::string test = "abracadabra";
        assert(*p.longest_match(test, 3) == rules[1]);
        assert(*p.longest_match(test, 4) == rules[4]);
        assert(p.longest_match(test, 9) == 0);
    }

    {
        std::string test = "quantico";
        assert(p.longest_match(test, 6) == 0);
    }

    {
        std::string test = "abrahamson";
        assert(*p.longest_match(test, 1) == rules[3]);
        assert(*p.longest_match(test, 2) == rules[2]);
        assert(p.longest_match(test, 5) == 0);
    }
};
