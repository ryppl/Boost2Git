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

#ifndef APR_POOL_HPP
#define APR_POOL_HPP

#include <apr_general.h>
#include <svn_pools.h>

class AprPool
  {
  public:
    AprPool(apr_pool_t *parent = 0)
      {
      pool = svn_pool_create(parent);
      }
    ~AprPool()
      {
      if (pool)
          svn_pool_destroy(pool);
      }

    AprPool(AprPool const&) = delete;
    void operator=(AprPool const&) = delete;

    AprPool(AprPool&& rhs) 
      { 
      pool = rhs.pool; 
      rhs.pool = 0; 
      }

    AprPool& operator=(AprPool&& rhs) 
      { 
      if (pool) 
        svn_pool_destroy(pool); 
      pool = rhs.pool; 
      rhs.pool = 0; 
      return *this;
      }

    AprPool make_subpool() const
      { return AprPool(data()); }

    void clear()
      {
      svn_pool_clear(pool);
      }

    operator apr_pool_t*() const
      {
      return pool;
      }
    apr_pool_t* data() const
      {
      return pool;
      }
  private:
    apr_pool_t *pool;
  };

#endif /* APR_POOL_HPP */
