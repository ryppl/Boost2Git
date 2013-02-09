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

#ifndef AUTHORS_HPP
#define AUTHORS_HPP

#include <boost/unordered_map.hpp>

class Authors
  {
  public:
    explicit Authors(std::string const& filename);
    std::string const& operator[](std::string const& svnuser) const;
  private:
    boost::unordered_map<std::string, std::string> map;
  };

#endif /* AUTHORS_HPP */
