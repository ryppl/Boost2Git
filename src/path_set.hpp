// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef PATH_SET_DWA2013615_HPP
# define PATH_SET_DWA2013615_HPP

#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <algorithm>

class path_set
{
    typedef std::vector<boost::filesystem::path> storage;
 public:

    path_set() {}

    path_set(std::initializer_list<boost::filesystem::path> const& x)
        : paths(x)
    {}

    friend bool operator==(path_set const& lhs, path_set const& rhs)
    { return lhs.paths == rhs.paths; }

    typedef storage::const_iterator const_iterator;
    typedef const_iterator iterator;

    const_iterator begin() { return paths.begin(); }
    const_iterator end() { return paths.begin(); }

    const_iterator insert(const_iterator _, boost::filesystem::path p)
    {
        return insert(std::move(p));
    }

    const_iterator insert(boost::filesystem::path p)
    {
        auto start = std::lower_bound(paths.begin(), paths.end(), p);

        // See if a parent path is already in the set
        if (start != paths.begin() && boost::starts_with(p, *std::prev(start)))
            return start;  // if so, we're done

        // Find the range of paths in the set that p subsumes
        auto finish = start;
        while (finish != paths.end() && boost::algorithm::starts_with(*finish, p))
            ++finish;

        if (start == finish)
        {
            return paths.insert(start, p);
        }
        else if (*start != p) // don't bother if we already have the path
        {
            *start = p;
            paths.erase(std::next(start), finish);
        }
        return start;
    }
 private:
    storage paths;
};

#endif // PATH_SET_DWA2013615_HPP
