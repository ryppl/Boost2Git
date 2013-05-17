// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef MARKS_FILE_NAME_DWA2013516_HPP
# define MARKS_FILE_NAME_DWA2013516_HPP

# include <string>
#include <boost/range/algorithm/replace.hpp>

inline std::string marksFileName(std::string name)
  {
  boost::replace(name, '/', '_');
  return "marks-" + name;
  }

inline std::string marks_file_path(std::string repo_name)
  {
  return repo_name + "/" + marksFileName(repo_name);
  }

#endif // MARKS_FILE_NAME_DWA2013516_HPP
