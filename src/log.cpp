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

#include "log.hpp"
#include <stdexcept>

namespace Log
{

static Level level = Log::Info;

static std::size_t revision;
static std::size_t revision_reported;
static std::ostream dummy(0);
static std::size_t num_errors = 0;

void set_level(Level value)
  {
  level = value;
  }

static void check_revision()
  {
  if (revision == revision_reported)
    {
    return;
    }
  std::cout << "\nRevision " << revision << std::endl;
  revision_reported = revision;
  }

void set_revision(std::size_t rev)
  {
  if ((revision % 1000) == 0)
    {
    check_revision();
    }
  revision = rev;
  }

std::ostream& error()
  {
  ++num_errors;
  //if (num_errors > 100)
  //  {
  //  throw std::runtime_error("Too many errors, skipping.");
  //  }
  check_revision();
  return std::cerr << "++ ERROR: ";
  }

std::ostream& trace()
  {
  if (level < Log::Trace)
    {
    return dummy;
    }
  check_revision();
  return std::cout << "-- ";
  }

std::ostream& debug()
  {
  if (level < Log::Debug)
    {
    return dummy;
    }
  check_revision();
  return std::cout << "-- ";
  }

std::ostream& info()
  {
  if (level < Log::Info)
    {
    return dummy;
    }
  check_revision();
  return std::cout << "-- ";
  }

std::ostream& warn()
  {
  check_revision();
  return std::cout << "++ WARNING: ";
  }

int result()
  {
  if (num_errors == 0)
    {
    return EXIT_SUCCESS;
    }
  std::cerr << "\n" << num_errors << " Errors occured!" << std::endl;
  return EXIT_FAILURE;
  }

} // namespace Log
