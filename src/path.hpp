// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef PATH_DWA2013618_HPP
# define PATH_DWA2013618_HPP

# include <boost/algorithm/string/trim.hpp>
# include <boost/algorithm/string/predicate.hpp>
# include <boost/operators.hpp>
# include <boost/range/iterator_range.hpp>
# include <boost/iterator/iterator_facade.hpp>
# include <utility>
# include <ostream>
# include <algorithm>

// A wrapper for Git/SVN path strings.  Boost.Filesystem's path is
// inappropriate for this purpose because of
// https://svn.boost.org/trac/boost/ticket/8708.
// 
// Additionally, we normalize by stripping leading and trailing
// slashes, which is perfectly fine for this application
struct path : boost::totally_ordered<path>
{
    typedef boost::iterator_range<std::string::const_iterator> component;

    struct component_iterator
      : public boost::iterator_facade<
            component_iterator
          , component
          , std::input_iterator_tag
          , component
        >
    {
        component_iterator() {}

        component_iterator(path const& p)
            : first(p.str().begin())
            , last(p.str().end())
            , mid(std::find(first, last, '/'))
        {}

        component_iterator(std::string::const_iterator i)
            : first(i)
            , last(i)
            , mid(i)
        {}

     private:
        friend class boost::iterator_core_access;

        void increment()
        {
            first = mid;
            if (first != last)
                ++first;
            mid = std::find(first, last, '/');
        }
        
        component dereference() const
        {
            return boost::make_iterator_range(first, mid);
        }

        bool equal(component_iterator const& rhs) const
        {
            return first == rhs.first;
        }

        std::string::const_iterator first, last, mid;
    };

    path() {}

    path(char const* x)
        : text(path::trim(std::string(x)))
    {}

    path(std::string x)
        : text(path::trim(std::move(x)))
    {}

    path(path const&) = default;
    path(path&&) = default;
    path& operator=(path const&) = default;
    path& operator=(path&&) = default;

    bool starts_with(path const& prefix) const
    {
        return boost::starts_with(text, prefix.text) && (
            text.size() == prefix.text.size() 
            || text[prefix.text.size()] == '/'
            || prefix.str().empty()
        );
    }

    std::string sans_prefix(path const& prefix) const
    {
        assert(starts_with(prefix));
        return text.substr(prefix.text.size());
    }

    friend bool operator==(path const& p0, path const& p1)
    {
        return p0.text == p1.text;
    }

    friend bool operator<(path const& p0, path const& p1)
    {
        return std::lexicographical_compare(
            component_iterator(p0), component_iterator(p0.str().end()),
            component_iterator(p1), component_iterator(p1.str().end()));
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

    std::string const& str() const
    {
        return text;
    }

    friend path operator/(path lhs, path const& rhs)
    {
        if (!lhs.str().empty() && !rhs.str().empty())
            lhs.text.push_back('/');
        lhs.text += rhs.text;
        return lhs;
    }
 private:
    static std::string trim(std::string x) 
    { 
        boost::algorithm::trim_if(x, boost::is_any_of("/"));
        return x;
    }

 private:
    std::string text;
};

#endif // PATH_DWA2013618_HPP
