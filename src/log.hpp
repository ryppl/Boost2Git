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

#ifndef LOG_HPP
#define LOG_HPP

#include <iostream>

namespace Log
{

enum Level
  {
  Warning,
  Info,
  Debug,
  Trace
  };

void set_level(Level value);
void set_revision(std::size_t value);

std::ostream& error();
std::ostream& trace();
std::ostream& debug();
std::ostream& info();
std::ostream& warn();

int result();

} // namespace Log

#endif /* LOG_HPP */
