// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef GITATTRIBUTES_PARSER_DWA201393_HPP
# define GITATTRIBUTES_PARSER_DWA201393_HPP

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/detail/file_parser_error.hpp>
#include <boost/cgi/detail/url_decode.ipp>
#include <boost/algorithm/string.hpp>

#include <string>
#include <vector>

namespace gitattributes_parser
{   
    namespace property_tree = boost::property_tree;
    using file_parser_error = property_tree::file_parser_error;

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
}


#endif // GITATTRIBUTES_PARSER_DWA201393_HPP
