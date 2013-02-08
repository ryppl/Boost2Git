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

#ifndef STATS_HPP
#define STATS_HPP

#include "rules.hpp"

class Stats
  {
  public:
    static Stats *instance();
    void printStats() const;
    void ruleMatched(const Rules::Match &rule, const int rev = -1);
    void addRule(const Rules::Match &rule);
    static void init();
    ~Stats();

  private:
    Stats();
    class Private;
    Private * const d;
    static Stats *self;
    bool use;
  };

#endif /* STATS_HPP */
