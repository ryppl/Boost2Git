// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef GIT_FAST_IMPORT_DWA2013614_HPP
# define GIT_FAST_IMPORT_DWA2013614_HPP

# include "log.hpp"

# include <boost/process.hpp>
# include <boost/iostreams/device/file_descriptor.hpp>
# include <boost/iostreams/stream.hpp>
# include <vector>
# include <string>

# include <iostream>

struct path;

// I/O manipulator that sends a linefeed character with no translation
inline std::ostream& LF (std::ostream& stream)
{
    stream.rdbuf()->sputc('\n');
    return stream;
}

struct git_fast_import
{
    git_fast_import(std::string const& repo_dir);
    ~git_fast_import();
    void close() { cin.close(); }

    template <class T>
    git_fast_import& operator<<(T const& x) 
    {
        if (Log::get_level() >= Log::Trace)
            std::cerr << x << std::flush;
        this->cin << x; 
        return *this;
    }

    git_fast_import& data(char const* data, std::size_t size);

    git_fast_import& commit(
        std::string const& ref_name, 
        std::size_t mark, 
        std::string const& author,
        unsigned long epoch,
        std::string const& log_message);

    git_fast_import& filedelete(path const& p);
    
    git_fast_import& filemodify_hdr(path const& p);

    git_fast_import& write_raw(char const* data, std::size_t nbytes);

    // Just writes the header for the 'data' command; you can write
    // the actual data directly to the stream.
    git_fast_import& data_hdr(std::size_t size);

    git_fast_import& checkpoint();
    git_fast_import& reset(std::string const& ref_name, int mark);

    void send_ls(std::string const& dataref_opt_path);
    std::string readline();

 private:
    static std::vector<std::string> arg_vector(std::string const& git_dir);

    boost::process::pipe inp;
    boost::process::pipe outp;
    boost::process::child process;
    boost::iostreams::stream<
        boost::iostreams::file_descriptor_sink
    > cin;
    boost::iostreams::stream<
        boost::iostreams::file_descriptor_source
    > cout;
};

#endif // GIT_FAST_IMPORT_DWA2013614_HPP
