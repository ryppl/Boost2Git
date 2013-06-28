// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef RULE_DWA201337_HPP
# define RULE_DWA201337_HPP

# include <string>
# include <ostream>
# include <climits>
# include "AST.hpp"
# include <boost/algorithm/string/predicate.hpp>

struct Rule
{
    Rule(
        boost2git::RepoRule const* repo_rule,
        boost2git::BranchRule const* branch_rule,
        boost2git::ContentRule const* content_rule
    )
        : repo_rule(repo_rule),
          branch_rule(branch_rule),
          content_rule(content_rule),
          min(std::max(branch_rule->min, repo_rule->minrev)),
          max(std::min(branch_rule->max, repo_rule->maxrev))
    {}

    // Constituent rules in the AST
    boost2git::RepoRule const* repo_rule;       // never 0
    boost2git::BranchRule const* branch_rule;   // never 0
    boost2git::ContentRule const* content_rule; // can be 0
  
    std::size_t min, max;

    friend bool operator==(Rule const& lhs, Rule const& rhs)
    {
        return lhs.repo_rule == rhs.repo_rule
            && lhs.branch_rule == rhs.branch_rule
            && lhs.content_rule == rhs.content_rule
            && lhs.min == rhs.min
            && lhs.max == rhs.max;
    }

    path svn_path() const
    {
        return content_rule
            ? branch_rule->svn_path / content_rule->svn_path
            : branch_rule->svn_path;
    }

    std::string git_address() const
    {
        return git_repo_name() +  ":" + git_ref_name() + ":" + git_path().str();
    }

    std::string git_repo_name() const
    {
        return repo_rule->git_repo_name;
    }

    path git_path() const
    {
        return content_rule ? content_rule->git_path : path();
    }

    std::string git_ref_name() const
    {
        return boost2git::git_ref_name(branch_rule);
    }
};

void report_overlap(Rule const* rule0, Rule const* rule1);

inline std::ostream& operator<<(std::ostream& os, Rule const& r)
{
    if (r.min != 0 || r.max != UINT_MAX)
    {
        os << "[";
        if (r.min != 0) os << r.min;
        os << ":";
        if (r.max != UINT_MAX)
            os << r.max;
        os << "] ";
    }
    os << r.git_address();
    return os;
}

#endif // RULE_DWA201337_HPP
