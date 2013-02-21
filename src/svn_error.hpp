/*
 *  Copyright (C) 2007  Thiago Macieira <thiago@kde.org>
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

#ifndef SVN_ERROR_HPP
#define SVN_ERROR_HPP

#include <exception>

class SvnError: public std::exception
  {
  public:
    explicit SvnError(svn_error_t* err) : err(err)
      {
      }
  private:
    char const* what() const throw ()
      {
      return err->message ? err->message : "No Message";
      }
  private:
    svn_error_t* err;
  };

inline void check_svn(svn_error_t* err)
  {
  if (err)
    {
    throw SvnError(err);
    }
  }

#endif /* SVN_ERROR_HPP */
