// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef FLAT_SET_UNION_DWA201376_HPP
# define FLAT_SET_UNION_DWA201376_HPP

# include <boost/container/flat_set.hpp>

template <class T, class C>
boost::container::flat_set<T>& operator |=(boost::container::flat_set<T>& lhs, C const& rhs)
{
    for (auto const& x : rhs)
        lhs.insert(x);
    return lhs;
}


#endif // FLAT_SET_UNION_DWA201376_HPP
