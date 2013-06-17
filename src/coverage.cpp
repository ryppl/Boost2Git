// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include "coverage.hpp"
#include "rule.hpp"
#include "options.hpp"
#include <boost/foreach.hpp>

#include <string>
#include <set>
#include <map>
#include <iostream>
#include <cassert>

typedef std::map<
  boost2git::BranchRule const*,
  std::set<boost2git::RepoRule const*>
  > branch_repositories;

static branch_repositories declared;
static branch_repositories matched;

void coverage::declare(Rule const& r)
  {
  if (!options.coverage)
    return;
  declared[r.branch_rule].insert(r.repo_rule);
  }

void coverage::match(Rule const& r, std::size_t revision)
  {
  if (!options.coverage)
    return;
  // std::cout << "** Matching: " << r << " in " << declared.size() << " declared rules" << std::endl;
  assert(declared.find(r.branch_rule) != declared.end());
  matched[r.branch_rule].insert(r.repo_rule);
  // std::cout << "** match count: " << matched[r.branch_rule].size() << std::endl;
  }

struct project1st
  {
  template <class T, class U>
  T const& operator()(std::pair<T,U> const& x) const
    {
    return x.first;
    }
  };

static double utilization(boost2git::BranchRule const* r)
  {
    std::set<boost2git::RepoRule const*> const& matches = matched[r];
    double nmatched = matches.size();
    return nmatched / declared[r].size();
  }

struct less_utilized
  {
  bool operator()(boost2git::BranchRule const* lhs, boost2git::BranchRule const* rhs) const
    {
    return utilization(lhs) < utilization(rhs);
    }
  };

void coverage::report()
  {
  std::vector<boost2git::BranchRule const*> by_utilization;
  
  std::transform(declared.begin(), declared.end(), std::back_inserter(by_utilization), project1st());
  std::sort(by_utilization.begin(), by_utilization.end(), less_utilized());
  
  BOOST_FOREACH(boost2git::BranchRule const* b, by_utilization)
    {
    std::size_t nmatches = matched[b].size();
    int percentage = utilization(b) * 100 + 0.5;
    if (percentage >= 70)
        continue;
    
    std::cout << options.rules_file << ":" << b->line << ": warning:"
              << b->svn_path << " ==> " << git_ref_name(b) 
              << " utilization " << percentage << "% ("
              << nmatches << " repositories)" << std::endl;

    if (nmatches < 10)
      {
      BOOST_FOREACH(boost2git::RepoRule const* r, matched[b])
        {
        std::cout << options.rules_file << ":" << r->line << ": see " << r->git_repo_name << std::endl;
        }
      }
      std::cout << std::endl;
    }
  }
