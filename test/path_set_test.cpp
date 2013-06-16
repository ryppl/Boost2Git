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
    add_path(s, "x/foo");
    add_path(s, "x/foo/bar");
    add_path(s, "x/foo/bar/baz");
    add_path(s, "y/fu/baz");
    add_path(s, "y/fu/bar");
    add_path(s, "y/fu");
    add_path(s, "y/bar");

    path_set expected = { "x/foo", "y/bar", "y/fu" };
    assert(s == expected);
}
