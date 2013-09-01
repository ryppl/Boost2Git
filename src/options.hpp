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

#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/detail/file_parser_error.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/cgi/detail/url_decode.ipp>
#include <vector>

namespace boost { namespace property_tree { namespace gitattributes_parser
{   
    /**
     * Determines whether the @c flags are valid for use with the gitattributes_parser.
     * @param flags value to check for validity as flags to gitattributes_parser.
     * @return true if the flags are valid, false otherwise.
     */
    inline bool validate_flags(int flags)
    {
        return flags == 0;
    }

    /** Indicates an error parsing gitattributes formatted data. */
    class gitattributes_parser_error: public file_parser_error
    {
    public:
        /**
         * Construct an @c gitattributes_parser_error
         * @param message Message describing the parser error.
         * @param filename The name of the file being parsed containing the
         *                 error.
         * @param line The line in the given file where an error was
         *             encountered.
         */
        gitattributes_parser_error(const std::string &message,
                         const std::string &filename,
                         unsigned long line)
            : file_parser_error(message, filename, line)
        {
        }
    };

    /**
     * Read gitattributes from a the given stream and translate it to a property tree.
     * @note Clears existing contents of property tree. In case of error
     *       the property tree is not modified.
     * @throw gitattributes_parser_error If a format violation is found.
     * @param stream Stream from which to read in the property tree.
     * @param[out] pt The property tree to populate.
     */
    template<class Ptree>
    void read_gitattributes(std::basic_istream<
                    typename Ptree::key_type::value_type> &stream,
                  Ptree &pt)
    {
        typedef typename Ptree::key_type::value_type Ch;
        typedef std::basic_string<Ch> Str;
        const Ch semicolon = stream.widen(';');
        const Ch hash = stream.widen('#');

        Ptree local;
        unsigned long line_no = 0;
        Str line;

        // For all lines
        while (stream.good())
        {

            // Get line from stream
            ++line_no;
            std::getline(stream, line);
            if (!stream.good() && !stream.eof())
                BOOST_PROPERTY_TREE_THROW(gitattributes_parser_error(
                    "read error", "", line_no));

            // If line is non-empty
            line = property_tree::detail::trim(line, stream.getloc());
            if (!line.empty())
            {
                // Comment or ext?
                if (line[0] == hash)
                {
                    // Ignore comments
                }
                else
                {
                    std::vector<std::string> tokens;
                    boost::split(tokens, line, boost::is_any_of(" "));
                    if(tokens.empty())
                        BOOST_PROPERTY_TREE_THROW(gitattributes_parser_error(
                            "' ' character not found in line", "", line_no));                    
                    Ptree &container = local.push_back(std::make_pair(tokens.front(), Ptree()))->second;
                    //std::cout << tokens.front() << std::endl;
                    
                    for(auto it=tokens.cbegin(); it!=tokens.cend(); ++it)
                    {
                        if(tokens.cbegin()==it) continue;
                        const Str &item=*it;
                        if(item.empty()) continue;
                        typename Str::size_type eqpos = item.find(Ch('=')), hshpos = item.find(hash);
                        Str key = property_tree::detail::trim(
                            item.substr(0, eqpos), stream.getloc());
                        Str data = (eqpos == Str::npos) ? "" : property_tree::detail::trim(
                            item.substr(eqpos + 1, (Str::npos==hshpos) ? Str::npos : (hshpos - eqpos - 1)), stream.getloc());
                        //std::cout << "  " << key << " " << data << std::endl;
                        if (container.find(key) != container.not_found())
                            BOOST_PROPERTY_TREE_THROW(gitattributes_parser_error(
                                "duplicate key name", "", line_no));
                        if (hshpos != Str::npos)
                        {
                            Str mimetype = property_tree::detail::trim(
                                item.substr(hshpos + 1, Str::npos), stream.getloc());
                            mimetype = boost::cgi::detail::url_decode(mimetype);
                            //std::cout << "  mime_type " << mimetype << std::endl;
                            container.push_back(std::make_pair("mime_type", Ptree(mimetype)));
                        }
                        container.push_back(std::make_pair(key, Ptree(data)));
                    }
                }
            }
        }

        // Swap local ptree with result ptree
        pt.swap(local);

    }

    /**
     * Read gitattributes from a the given file and translate it to a property tree.
     * @note Clears existing contents of property tree.  In case of error the
     *       property tree unmodified.
     * @throw gitattributes_parser_error In case of error deserializing the property tree.
     * @param filename Name of file from which to read in the property tree.
     * @param[out] pt The property tree to populate.
     * @param loc The locale to use when reading in the file contents.
     */
    template<class Ptree>
    void read_gitattributes(const std::string &filename, 
                  Ptree &pt,
                  const std::locale &loc = std::locale())
    {
        std::basic_ifstream<typename Ptree::key_type::value_type>
            stream(filename.c_str());
        if (!stream)
            BOOST_PROPERTY_TREE_THROW(gitattributes_parser_error(
                "cannot open file", filename, 0));
        stream.imbue(loc);
        try {
            read_gitattributes(stream, pt);
        }
        catch (gitattributes_parser_error &e) {
            BOOST_PROPERTY_TREE_THROW(gitattributes_parser_error(
                e.message(), filename, e.line()));
        }
    }

    namespace detail
    {
        template<class Ptree>
        void check_dupes(const Ptree &pt)
        {
            if(pt.size() <= 1)
                return;
            const typename Ptree::key_type *lastkey = 0;
            typename Ptree::const_assoc_iterator it = pt.ordered_begin(),
                                                 end = pt.not_found();
            lastkey = &it->first;
            for(++it; it != end; ++it) {
                if(*lastkey == it->first)
                    BOOST_PROPERTY_TREE_THROW(gitattributes_parser_error(
                        "duplicate key", "", 0));
                lastkey = &it->first;
            }
        }
    }

#if 0 // unfinished
    /**
     * Translates the property tree to gitattributes and writes it the given output
     * stream.
     * @pre @e pt cannot have data in its root.
     * @pre @e pt cannot have keys both data and children.
     * @pre @e pt cannot be deeper than two levels.
     * @pre There cannot be duplicate keys on any given level of @e pt.
     * @throw gitattributes_parser_error In case of error translating the property tree to
     *                         gitattributes or writing to the output stream.
     * @param stream The stream to which to write the gitattributes representation of the 
     *               property tree.
     * @param pt The property tree to tranlsate to gitattributes and output.
     * @param flags The flags to use when writing the gitattributes file.
     *              No flags are currently supported.
     */
    template<class Ptree>
    void write_gitattributes(std::basic_ostream<
                       typename Ptree::key_type::value_type
                   > &stream,
                   const Ptree &pt,
                   int flags = 0)
    {
        using detail::check_dupes;

        typedef typename Ptree::key_type::value_type Ch;
        typedef std::basic_string<Ch> Str;

        BOOST_ASSERT(validate_flags(flags));
        (void)flags;

        if (!pt.data().empty())
            BOOST_PROPERTY_TREE_THROW(gitattributes_parser_error(
                "ptree has data on root", "", 0));
        check_dupes(pt);

        for (typename Ptree::const_iterator it = pt.begin(), end = pt.end();
             it != end; ++it)
        {
            check_dupes(it->second);
            if (it->second.empty()) {
                stream << it->first << Ch('=')
                    << it->second.template get_value<
                        std::basic_string<Ch> >()
                    << Ch('\n');
            } else {
                if (!it->second.data().empty())
                    BOOST_PROPERTY_TREE_THROW(gitattributes_parser_error(
                        "mixed data and children", "", 0));
                stream << Ch('[') << it->first << Ch(']') << Ch('\n');
                for (typename Ptree::const_iterator it2 = it->second.begin(),
                         end2 = it->second.end(); it2 != end2; ++it2)
                {
                    if (!it2->second.empty())
                        BOOST_PROPERTY_TREE_THROW(gitattributes_parser_error(
                            "ptree is too deep", "", 0));
                    stream << it2->first << Ch('=')
                        << it2->second.template get_value<
                            std::basic_string<Ch> >()
                        << Ch('\n');
                }
            }
        }

    }

    /**
     * Translates the property tree to gitattributes and writes it the given file.
     * @pre @e pt cannot have data in its root.
     * @pre @e pt cannot have keys both data and children.
     * @pre @e pt cannot be deeper than two levels.
     * @pre There cannot be duplicate keys on any given level of @e pt.
     * @throw info_parser_error In case of error translating the property tree
     *                          to gitattributes or writing to the file.
     * @param filename The name of the file to which to write the gitattributes
     *                 representation of the property tree.
     * @param pt The property tree to tranlsate to gitattributes and output.
     * @param flags The flags to use when writing the gitattributes file.
     *              The following flags are supported:
     * @li @c skip_gitattributes_validity_check -- Skip check if ptree is a valid gitattributes. The
     *     validity check covers the preconditions but takes <tt>O(n log n)</tt>
     *     time.
     * @param loc The locale to use when writing the file.
     */
    template<class Ptree>
    void write_gitattributes(const std::string &filename,
                   const Ptree &pt,
                   int flags = 0,
                   const std::locale &loc = std::locale())
    {
        std::basic_ofstream<typename Ptree::key_type::value_type>
            stream(filename.c_str());
        if (!stream)
            BOOST_PROPERTY_TREE_THROW(gitattributes_parser_error(
                "cannot open file", filename, 0));
        stream.imbue(loc);
        try {
            write_gitattributes(stream, pt, flags);
        }
        catch (gitattributes_parser_error &e) {
            BOOST_PROPERTY_TREE_THROW(gitattributes_parser_error(
                e.message(), filename, e.line()));
        }
    }
#endif

} } }

namespace boost { namespace property_tree
{
    using gitattributes_parser::gitattributes_parser_error;
    using gitattributes_parser::read_gitattributes;
#if 0 // unfinished
    using gitattributes_parser::write_gitattributes;
#endif
} }

struct Options
  {
  bool add_metadata;
  bool add_metadata_notes;
  bool dry_run;
  bool debug_rules;
  bool coverage;
  int commit_interval;
  bool svn_branches;
  std::string rules_file;
  std::string git_executable;
  std::string gitattributes_text;
  boost::property_tree::ptree gitattributes_tree;
  };

extern Options options;

#endif /* OPTIONS_HPP */
