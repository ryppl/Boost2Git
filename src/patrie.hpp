// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef TRIE_MAP_DWA201332_HPP
# define TRIE_MAP_DWA201332_HPP

# include <ruleset.hpp>
# include <deque>
# include <boost/variant.hpp>
//# include <boost/container/vector.hpp>
# include <vector>
# include <boost/swap.hpp>

namespace patrie_ {
//using boost::container::vector;
using std::vector;

struct patrie
  {
  
public:
  void insert(Ruleset::Match const* rule)
    {
    insert_visitor v(rule);
    this->traverse(rule->match.begin(), rule->match.end(), v);
    }

  template <class Iterator>
  Ruleset::Match const* longest_match(Iterator start, Iterator finish, std::size_t revision)
    {
    search_visitor v(revision);
    this->traverse(start, finish, v);
    return v.found_rule;
    }
  
private:
  struct node
    {
    template<class Iterator>
    node(Iterator text_start, Iterator text_finish, Ruleset::Match const* rule = 0)
        : text(text_start, text_finish),
        rules(rule ? 1 : 0, rule)
      {}
    
    std::string text;
    vector<node> next;
    vector<Ruleset::Match const*> rules;

    friend void swap(node& l, node& r)
      {
      boost::swap(l.text, r.text);
      boost::swap(l.next, r.next);
      boost::swap(l.rules, r.rules);
      }
    };

  struct rule_rev_comparator
    {
    bool operator()(Ruleset::Match const* r, std::size_t revision) const
      {
      return r->max < revision;
      }
    };

  
  typedef vector<node>::iterator node_iterator;
  struct insert_visitor
    {
    insert_visitor(Ruleset::Match const* rule) : new_rule(rule) {}

    // No match for *start was found in nodes
    template <class Iterator>
    void nomatch(vector<node>& nodes, node_iterator pos, Iterator start, Iterator finish)
      {
      nodes.insert(pos, node(start, finish, this->new_rule));
      }

    // We matched up through position c in node n
    template <class Iterator>
    void partial_match(node& n, std::string::iterator c, Iterator start, Iterator finish)
      {
      // split the node
      vector<node> save_next;
      boost::swap(n.next, save_next); // extract its set of next nodes
      
      // prepare a new node with the node's unmatched text
      n.next.push_back( node(c, n.text.end()) );

      // chop that part off of the original node
      n.text.erase(c, n.text.end());
      
      // the next nodes and rules of the tail node are those of the original node
      boost::swap(n.next.back().next, save_next);
      boost::swap(n.next.back().rules, n.rules);
      
      // add a new node with the unmatched search text and rule
      n.next.push_back(node(start, finish, this->new_rule));

      // make sure the two splits are properly sorted
      if (n.next[0].text[0] > n.next[1].text[0])
          boost::swap(n.next[0], n.next[1]);
      }

    // We matched all of node n
    template <class Iterator>
    void full_match(node& n, Iterator start, Iterator finish)
      {
      // If we haven't consumed the entire search text, just keep looking.
      if (start != finish)
          return;

      vector<Ruleset::Match const*>::iterator p
        = std::lower_bound(n.rules.begin(), n.rules.end(), new_rule->max, rule_rev_comparator());

      assert(p == n.rules.end() || (*p)->min > new_rule->max || !"found overlapping rules!");
      n.rules.insert(p, this->new_rule);
      }

    Ruleset::Match const* new_rule;
    };

  struct search_visitor
    {
    search_visitor(std::size_t revision)
        : revision(revision), found_rule(0) {}

    // No match for *start was found in nodes
    template <class Iterator>
    void nomatch(vector<node> const& nodes, node_iterator pos, Iterator start, Iterator finish)
      {
      }

    // We matched up through position c in node n
    template <class Iterator>
    void partial_match(node const &n, std::string::const_iterator c, Iterator start, Iterator finish)
      {
      }

    // We matched all of node n
    template <class Iterator>
    void full_match(node const& n, Iterator start, Iterator finish)
      {
      vector<Ruleset::Match const*>::const_iterator p
        = std::lower_bound(n.rules.begin(), n.rules.end(), revision, rule_rev_comparator());
      
      if (p != n.rules.end() && (*p)->min <= revision)
        {
        found_rule = *p;
        }
      }

    std::size_t revision;
    Ruleset::Match const* found_rule;
    };
  
  struct node_comparator
    {
    bool operator()(node const& lhs, char rhs) const
      {
      return lhs.text[0] < rhs;
      }
    };
  
  template <class Iterator, class Visitor>
  void traverse(Iterator start, Iterator finish, Visitor& visitor)
    {
    vector<node> *nodes = &this->trie;
    
    while (start != finish)
      {
      // look for the node beginning with *start
      vector<node>::iterator n
        = std::lower_bound(
            nodes->begin(), nodes->end(), *start, node_comparator());
      
      if (n == nodes->end() || n->text[0] != *start)
        {
          visitor.nomatch(*nodes, n, start, finish);
          return;
        }

      // Look for the first difference between [start, finish) and
      // the node's text
      std::string::iterator c = n->text.begin();
      ++c;
      ++start;

      while (c != n->text.end())
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
  vector<node> trie;
  };
}
using patrie_::patrie;

#endif // TRIE_MAP_DWA201332_HPP
