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

#ifndef RULESET_HPP
#define RULESET_HPP

#include <set>
#include <string>
#include <vector>
#include <boost/algorithm/string/predicate.hpp>
#include "patrie.hpp"
#include "rule.hpp"
#include "AST.hpp"
#include "coverage.hpp"

boost2git::AST parse_rules_file(std::string filename);

class Ruleset
  {
  public:
    typedef Rule Match;
    
    struct Repository
      {
      std::string name;
      std::string submodule_in_repo;
      std::string submodule_path;
      std::set<boost2git::BranchRule const*> branches;
      };
  public:
    Ruleset(std::string const& filename);
  public:
    patrie<Rule,coverage> const& matcher() const
      {
      return matcher_;
      }
    std::vector<Repository> const& repositories() const
      {
      return repositories_;
      }
    boost2git::AST const& getAST() const
      {
      return ast_;
      }
  private:
    patrie<Rule,coverage> matcher_;
    std::vector<Repository> repositories_;
    boost2git::AST ast_;
  };

#endif /* RULESET_HPP */
