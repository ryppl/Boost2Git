// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef RULE_DWA201337_HPP
# define RULE_DWA201337_HPP

# include <string>
# include <ostream>
# include <climits>
# include "AST.hpp"
#include <boost/algorithm/string/predicate.hpp>

inline std::string path_append(std::string path, std::string const& append)
{
    if (boost::ends_with(path, "/"))
    {
        if (boost::starts_with(append, "/"))
        {
            path.resize(path.size() - 1);
        }
    }
    else
    {
        if (!boost::starts_with(append, "/"))
        {
            path += '/';
        }
    }
    return path + append;
}

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

    std::string svn_path() const
    {
        return content_rule
            ? path_append(branch_rule->prefix, content_rule->prefix)
            : branch_rule->prefix;
    }

    std::string git_address() const
    {
        return repo_rule->name + ":" + branch_rule->name + ":" + prefix();
    }

    std::string prefix() const
    {
        return content_rule ? content_rule->replace : std::string();
    }
    bool is_fallback() const
    {
        return content_rule != 0 && content_rule->is_fallback;
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
    os << r.repo_rule->name << ".git:<" << r.branch_rule->name << ">:/" << r.prefix();
    return os;
}

#endif // RULE_DWA201337_HPP
