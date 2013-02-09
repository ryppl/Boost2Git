/*
 *  Copyright (C) 2013 Daniel Pfeifer <daniel@pfeifer-mail.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "authors.hpp"
#include <boost/regex.hpp>
#include <fstream>

static boost::regex regex("(.+\\H)\\h*=\\h*(.+)");

Authors::Authors(std::string const& filename)
  {
  std::string line;
  boost::smatch match;
  std::ifstream file(filename.c_str());
  while (file)
    {
    std::getline(file, line);
    if (line.empty() || line[0] == '#')
      {
      continue;
      }
    if (regex_match(line, match, regex))
      {
      map.insert(std::make_pair(match[1], match[2]));
      }
    else
      {
      throw std::runtime_error("error in authors file: " + line);
      }
    }
  }

std::string const& Authors::operator[](std::string const& svnuser) const
  {
  typedef boost::unordered_map<std::string, std::string> map_t;
  map_t::const_iterator it = map.find(svnuser);
  if (it == map.end())
    {
    throw std::runtime_error("no user mapping for " + svnuser);
    }
  return it->second;
  }
