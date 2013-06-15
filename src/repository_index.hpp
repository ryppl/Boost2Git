// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef REPOSITORY_INDEX_DWA2013614_HPP
# define REPOSITORY_INDEX_DWA2013614_HPP

# include "git_repository.hpp"
# include <map>

typedef std::map<std::string, git_repository> repository_index;

inline git_repository* 
ensure_repository(repository_index& index, std::string const& name)
{
    if (name.empty())
        return 0;

    auto p = index.find(name);
    if (p == index.end())
    {
        p = index.emplace_hint(
            p, std::piecewise_construct, 
            std::make_tuple(name), std::make_tuple(name));
    }
    return &p->second;
};

#endif // REPOSITORY_INDEX_DWA2013614_HPP
