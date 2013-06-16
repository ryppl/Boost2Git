// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef PATH_SET_DWA2013615_HPP
# define PATH_SET_DWA2013615_HPP

#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <algorithm>

typedef std::vector<boost::filesystem::path> path_set;

inline void add_path(path_set& paths, boost::filesystem::path p)
{
    auto start = std::lower_bound(paths.begin(), paths.end(), p);

    // See if a parent path is already in the set
    if (start != paths.begin() && boost::starts_with(p, *std::prev(start)))
        return;  // if so, we're done

    // Find the range of paths in the set that p subsumes
    auto finish = start;
    while (finish != paths.end() && boost::algorithm::starts_with(*finish, p))
        ++finish;

    if (start == finish)
    {
        paths.insert(start, p);
    }
    else
    {
        // see if we already have the path
        if (*start == p)
            return;
        *start = p;
        paths.erase(std::next(start), finish);
    }
}


#endif // PATH_SET_DWA2013615_HPP
