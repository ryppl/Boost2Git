// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef MARK_SHA_MAP_DWA2013515_HPP
# define MARK_SHA_MAP_DWA2013515_HPP

# include <algorithm>
# include <vector>
# include <string>

typedef std::vector<std::pair<unsigned long, std::string> > mark_sha_map;

struct compare_marks
  {
  bool operator()(mark_sha_map::const_reference lhs, unsigned long rhs) const
    {
    return lhs.first < rhs;
    }
  };

inline mark_sha_map::iterator
find_sha_pos(mark_sha_map& m, unsigned long mark)
  {
  return std::lower_bound(m.begin(), m.end(), mark, compare_marks());
  }

inline mark_sha_map::const_iterator
find_sha_pos(mark_sha_map const& m, unsigned long mark)
  {
  return std::lower_bound(m.begin(), m.end(), mark, compare_marks());
  }

#endif // MARK_SHA_MAP_DWA2013515_HPP
