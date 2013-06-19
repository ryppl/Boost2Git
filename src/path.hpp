// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef PATH_DWA2013618_HPP
# define PATH_DWA2013618_HPP

# include <boost/algorithm/string/predicate.hpp>
# include <boost/operators.hpp>
# include <utility>
# include <ostream>

// A wrapper for Git/SVN path strings.  Boost.Filesystem's path is
// inappropriate for this purpose because of
// https://svn.boost.org/trac/boost/ticket/8708.
// 
// Additionally, we normalize by stripping leading and trailing
// slashes.
struct path : boost::totally_ordered<path>
{
    path(char const* x)
        : text(trim_slashes(std::string(x)))
    {}

    path(std::string x)
        : text(trim_slashes(std::move(x)))
    {}

    bool starts_with(path const& prefix) const
    {
        return boost::starts_with(text, prefix.text) && (
            text.size() == prefix.text.size() 
            || text[prefix.text.size()] == '/');
    }

    friend bool operator==(path const& p0, path const& p1)
    {
        return p0.text == p1.text;
    }

    friend bool operator<(path const& p0, path const& p1)
    {
        return p0.text < p1.text;
    }

    friend void swap(path& p0, path& p1)
    {
        using std::swap;
        swap(p0.text, p1.text);
    }

    friend std::ostream& operator<<(std::ostream& os, path const& p)
    {
        return os << p.text;
    }

    char const* c_str() const
    {
        return text.c_str();
    }
 private:
    static std::string trim_slashes(std::string x) 
    { 
        if (!x.empty() && x.back() == '/')
            x.pop_back();
        if (!x.empty() && x.front() == '/')
            x.erase(x.begin());
        return x;
    }
 private:
    std::string text;
};

#endif // PATH_DWA2013618_HPP
