// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef COVERAGE_DWA2013428_HPP
# define COVERAGE_DWA2013428_HPP

# include <cstdlib>

struct Rule;

struct coverage
{
    static void declare(Rule const& r);
    static void match(Rule const& r, std::size_t revision);
    static void report();
};

#endif // COVERAGE_DWA2013428_HPP
