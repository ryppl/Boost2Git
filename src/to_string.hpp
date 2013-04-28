// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef TO_STRING_DWA2013427_HPP
# define TO_STRING_DWA2013427_HPP

# include <sstream>
# include <string>

template <class T>
std::string to_string(T const& x)
  {
  std::stringstream s;
  s << x;
  return s.str();
  }

#endif // TO_STRING_DWA2013427_HPP
