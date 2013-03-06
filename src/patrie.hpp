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
    this->traverse(rule->match.begin(), rule->match.end(), insert_visitor(rule));
    }

  template <class Iterator>
  Ruleset::Match const* longest_match(Iterator start, Iterator finish)
    {
    search_visitor searcher;
    this->traverse(start, finish, searcher);
    return searcher.found_rule;
    }
  
private:
  struct node
    {
    node(std::string const& text, Ruleset::Match const* rule)
        : text(text), rule(rule)
      {}
    
    std::string text;
    vector<node> next;
    Ruleset::Match const*rule;

    friend void swap(node& l, node& r)
      {
      boost::swap(l.text, r.text);
      boost::swap(l.next, r.next);
      boost::swap(l.rule, r.rule);
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
      nodes.insert(pos, node(std::string(start, finish), this->new_rule));
      }

    // We matched up through position c in node n
    template <class Iterator>
    void partial_match(node& n, std::string::iterator c, Iterator start, Iterator finish)
      {
      // split the node
      vector<node> temp;
      boost::swap(n.next, temp); // extract its set of next nodes
      
      // prepare a new node with the node's unmatched text and rule
      n.next.push_back(
          node(std::string(c, n.text.end()), n.rule));
      
      // the next nodes of the tail node are those of the original node
      boost::swap(n.next.back().next, temp);
      // no rule is fully matched at the split point
      n.rule = 0;
      // add a new node with the unmatched search text and rule
      n.next.push_back(node(std::string(start, finish), this->new_rule));

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
      
      // Otherwise, we had better have found a node created by
      // splitting, that doesn't correspond to a rule.  Otherwise,
      // we've tried to insert two rules with the same match string.
      assert(!n.next.empty());
      assert(!n.rule);
      n.rule = this->new_rule;
      }

    Ruleset::Match const* new_rule;
    };

  struct search_visitor
    {
    search_visitor() : found_rule(0) {}

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
      found_rule = n.rule;
      }

    Ruleset::Match const* found_rule;
    };
  
  struct node_comparator
    {
    bool operator()(node const& lhs, char rhs)
      {
      return lhs.text[0] < rhs;
      }
    };
  
  template <class Iterator, class Visitor>
  void traverse(Iterator start, Iterator finish, Visitor visitor)
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
      
      ++start;
        
      // Look for the first difference between [start, finish) and
      // the node's text
      std::string::iterator c = n->text.begin();
      while (++c != n->text.end() && start != finish)
        {
        if (*c != *start)
          {
          visitor.partial_match(*n, c, start, finish);
          return;
          }
        ++start;
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
