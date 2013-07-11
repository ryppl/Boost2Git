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

    path_set s1;
    s1.insert("/website/public_html/live/");
    s1.insert("/website/");
    s1.insert("/website/public_html/beta/");
    s1.insert("/website/public_html/beta/");
    path_set expected1 = { "website" };
    for (auto p : s1)
        std::cout << p << std::endl;
    assert(s1 == expected1);

    path_set s2;
    s2.insert("a.txt/bb");
    s2.insert("a");
    s2.insert("a/b");
    s2.insert("x");
    s2.insert("x.txt/yy");
    s2.insert("x/y");
    path_set expected2 = { "a", "a.txt/bb", "x", "x.txt/yy" };
    assert(s2 == expected2);
}
