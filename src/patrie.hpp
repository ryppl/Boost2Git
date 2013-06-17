// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef TRIE_MAP_DWA201332_HPP
# define TRIE_MAP_DWA201332_HPP

# include "to_string.hpp"
# include "options.hpp"
# include <deque>
# include <boost/variant.hpp>
# include <vector>
# include <deque>
# include <stdexcept>
# include <boost/swap.hpp>
# include <boost/range.hpp>
# include <boost/range/iterator_range.hpp>
# include <ostream>

namespace patrie_ {
//using boost::container::vector;
using std::vector;

template <class Rule>
struct DummyCoverage
{
    void declare(Rule const& r) {}
    void match(Rule const& r, std::size_t revision) {}
};

template <class Rule, class Coverage = DummyCoverage<Rule> >
struct patrie
{
 private: // Rule transition support
    typedef std::vector<Rule const*> rule_vec;
    typedef std::pair<std::size_t, rule_vec> rev_rules;

    template <class Map>
    static auto find_transitions(Map& map, std::size_t revnum) -> decltype(map.begin())
    {
        return std::lower_bound(
            map.begin(), map.end(), revnum, 
            [](rev_rules const& lhs, std::size_t rhs)
            { return lhs.first < rhs; });
    }
    
    rule_vec& transitions(std::size_t revnum)
    {
        auto pos = find_transitions(transition_map, revnum);
        if (pos == transition_map.end() || pos->first != revnum)
            pos = transition_map.insert(pos, rev_rules(revnum, {}));
        return pos->second;
    }

 public:
    boost::iterator_range<Rule const* const*> 
    rules_in_transition(std::size_t revnum) const
    {
        auto pos = find_transitions(transition_map, revnum);

        if (pos == transition_map.end() || pos->first != revnum)
            return boost::iterator_range<Rule const* const*>(nullptr, nullptr);

        return boost::make_iterator_range(
            pos->second.data(), pos->second.data() + pos->second.size());
    }

    void insert(Rule rule_)
    {
        rules.push_back(std::move(rule_));
        Rule const& rule = rules.back();

        if (rule.min > 1)
            transitions(rule.min).push_back(&rule);
        if (rule.max < UINT_MAX)
            transitions(rule.max + 1).push_back(&rule);


        coverage.declare(rule);

        {
            insert_visitor v(&rules.back());
            std::string svn_path = rule.svn_path();
            traverse(&this->trie, svn_path.begin(), svn_path.end(), v);
        }

        {
            insert_visitor v(&rules.back(), true);
            std::string git_address = rule.git_address();
            traverse(&this->rtrie, git_address.begin(), git_address.end(), v);
        }
    }

    template <class Range>
    Rule const* longest_match(Range const& r, std::size_t revision) const
    {
        search_visitor v(revision);
        traverse(&this->trie, boost::begin(r), boost::end(r), v);
        if (v.found_rule)
            coverage.match(*v.found_rule, revision);
        return v.found_rule;
    }
  
    template <class Range, class OutputIterator>
    void git_subtree_rules(Range const& git_address, std::size_t revision, OutputIterator out) const
    {
        subtree_search_visitor<OutputIterator> v(revision, out);
        traverse(&this->rtrie, boost::begin(git_address), boost::end(git_address), v);
    }

    template <class Range, class OutputIterator>
    void svn_subtree_rules(Range const& svn_path, std::size_t revision, OutputIterator out) const
    {
        subtree_search_visitor<OutputIterator> v(revision, out);
        traverse(&this->rtrie, boost::begin(svn_path), boost::end(svn_path), v);
    }
  
 private:
    struct node
    {
        template<class Iterator>
        node(Iterator text_start, Iterator text_finish, Rule const* rule = 0)
            : text(text_start, text_finish),
              rules(rule ? 1 : 0, rule)
        {
            assert(text_start != text_finish);
        }
    
        std::string text;
        vector<node> next;
        vector<Rule const*> rules;

        friend void swap(node& l, node& r)
        {
            boost::swap(l.text, r.text);
            boost::swap(l.next, r.next);
            boost::swap(l.rules, r.rules);
        }

        friend std::ostream& print_indented(std::ostream& os, node const& n, std::string const& indent)
        {
            if (!n.rules.empty())
            {
                os << indent << n.text << "]: ";
                for (typename vector<Rule const*>::const_iterator p = n.rules.begin(); p != n.rules.end(); ++p)
                {
                    os << "{ " << **p << "} ";
                }
                os << std::endl;
            }
            return print_indented(os, n.next, indent + n.text);
        }
    };

    struct rule_rev_comparator
    {
        bool operator()(Rule const* r, std::size_t revision) const
        {
            return r->max < revision;
        }
    };

  
    struct insert_visitor
    {
        insert_visitor(Rule const* rule, bool allow_overlap = false) 
          : new_rule(rule), allow_overlap(allow_overlap) 
        {}

        // No match for *start was found in nodes
        template <class Iterator>
        void nomatch(vector<node>& nodes, typename vector<node>::iterator pos, Iterator start, Iterator finish)
        {
            nodes.insert(pos, node(start, finish, this->new_rule));
        }

        // We matched up through position c in node n
        template <class Iterator>
        void partial_match(node& n, std::string::const_iterator c, Iterator start, Iterator finish)
        {
            // split the node
            vector<node> save_next;
            boost::swap(n.next, save_next); // extract its set of next nodes
      
            // prepare a new node with the node's unmatched text
            std::string::const_iterator end_ = n.text.end();
            n.next.push_back( node(c, end_) );

            // chop that part off of the original node
            n.text.erase(n.text.begin() + (c - n.text.begin()), n.text.end());
      
            // the next nodes and rules of the tail node are those of the original node
            boost::swap(n.next.back().next, save_next);
            boost::swap(n.next.back().rules, n.rules);
      
            if (start != finish)
            {
                // add a new node with the unmatched search text and rule
                n.next.push_back(node(start, finish, this->new_rule));
        
                // make sure the two splits are properly sorted
                if (n.next[0].text[0] > n.next[1].text[0])
                    boost::swap(n.next[0], n.next[1]);
            }
            else
            {
                this->full_match(n, start, finish);
            }
        }

        // We matched all of node n
        template <class Iterator>
        void full_match(node& n, Iterator start, Iterator finish)
        {
            // If we haven't consumed the entire search text, just keep looking.
            if (start != finish)
                return;

            typename vector<Rule const*>::iterator p
                = std::lower_bound(n.rules.begin(), n.rules.end(), new_rule->max, rule_rev_comparator());

            if (!(p == n.rules.end() || (*p)->min > new_rule->max))
            {
                if (!allow_overlap)
                    report_overlap(*p, new_rule);
            }
            n.rules.insert(p, this->new_rule);
        }

        Rule const* new_rule;
        bool allow_overlap;
    };

    struct search_visitor_base
    {
        search_visitor_base(std::size_t revision)
            : revision(revision) {}

        // No match for *start was found in nodes
        template <class Iterator>
        void nomatch(
            vector<node> const& nodes, typename vector<node>::const_iterator pos,
            Iterator start, Iterator finish)
        {
        }

        // We matched up through position c in node n
        template <class Iterator>
        void partial_match(node const &n, std::string::const_iterator c, Iterator start, Iterator finish)
        {
        }

        std::size_t revision;
    };
  
    struct search_visitor : search_visitor_base
    {
        search_visitor(std::size_t revision)
            : search_visitor_base(revision), found_rule(0) {}

        // We matched all of node n
        template <class Iterator>
        void full_match(node const& n, Iterator start, Iterator finish)
        {
            typename vector<Rule const*>::const_iterator p
                = std::lower_bound(n.rules.begin(), n.rules.end(), this->revision, rule_rev_comparator());
      
            if (p != n.rules.end() && (*p)->min <= this->revision)
            {
                found_rule = *p;
            }
        }

        Rule const* found_rule;
    };
  
    // Finds all Rules at paths beneath the prefix being sought
    template <class OutputIterator>
    struct subtree_search_visitor : search_visitor_base
    {
        subtree_search_visitor(std::size_t revision, OutputIterator out)
            : search_visitor_base(revision), out(out) {}

        // We matched all of node n
        template <class Iterator>
        void full_match(node const& n, Iterator start, Iterator finish, bool slash_required = true)
        {
            // If we used up the entire input, continue to explore
            // rules to find anything that maps into a subtree
            if (start == finish)
            {
                if (auto r = n.find_rule(this->revision))
                    *out++ = r;
                
                // Make sure we're only finding subtrees by requiring
                // a slash at the boundary between the full match and
                // everything else.
                slash_required = slash_required && n.text[n.text.size() - 1] != '/';
                for (auto const& n1 : n.next)
                {
                    if (!slash_required || n1.text[0] == '/')
                        full_match(n1, start, finish, false);
                }
            }
        }

        OutputIterator out;
    };
  
    struct node_comparator
    {
        bool operator()(node const& lhs, char rhs) const
        {
            return lhs.text[0] < rhs;
        }
    };
  
    friend std::ostream& operator<<(std::ostream& os, patrie const& data)
    {
        return print_indented(os, data.trie, "[");
    }
  
    friend std::ostream& print_indented(std::ostream& os, vector<node> const& data, std::string const& indent)
    {
        for(typename vector<node>::const_iterator p = data.begin(); p != data.end(); ++p)
        {
            print_indented(os, *p, indent);
        }
        return os;
    }
  
    template <class Nodes, class Iterator, class Visitor>
    static void traverse(Nodes* nodes, Iterator start, Iterator finish, Visitor& visitor)
    {
        while (start != finish)
        {
            // look for the node beginning with *start
            typename boost::range_iterator<Nodes>::type n
                = std::lower_bound(
                    nodes->begin(), nodes->end(), *start, node_comparator());
      
            if (n == nodes->end() || n->text[0] != *start)
            {
                visitor.nomatch(*nodes, n, start, finish);
                return;
            }

            // Look for the first difference between [start, finish) and
            // the node's text
            std::string::const_iterator c = n->text.begin(), e = n->text.end();
            ++c;
            ++start;

            while (c != e)
            {
                if (start == finish || *c != *start)
                {
                    visitor.partial_match(*n, c, start, finish);
                    return;
                }
                ++start;
                ++c;
            }

            visitor.full_match(*n, start, finish);
            nodes = &n->next;
        }
    }

 private: // data members
    std::deque<Rule> rules;
    vector<node> trie;
    vector<node> rtrie;
    mutable Coverage coverage;
    std::vector<rev_rules> transition_map;
};
}
using patrie_::patrie;

#endif // TRIE_MAP_DWA201332_HPP
