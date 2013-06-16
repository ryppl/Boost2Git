// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#undef NDEBUG
#include "path_set.hpp"
#include <cassert>
#include <iostream>

int main()
{
    path_set s;
    s.insert("x/foo");
    s.insert("x/foo/bar");
    s.insert("x/foo/bar/baz");
    s.insert("y/fu/baz");
    s.insert("y/fu/bar");
    s.insert("y/fu");
    s.insert("y/bar");

    path_set expected = { "x/foo", "y/bar", "y/fu" };
    assert(s == expected);
}
