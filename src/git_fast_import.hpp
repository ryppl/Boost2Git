// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef GIT_FAST_IMPORT_DWA2013614_HPP
# define GIT_FAST_IMPORT_DWA2013614_HPP

#include <boost/process.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include <vector>
#include <string>

struct git_fast_import
{
    git_fast_import(std::string const& repo_dir);
    ~git_fast_import();
    void close() { stdin.close(); }

 private:
    static std::vector<std::string> arg_vector(std::string const& git_dir);

    boost::process::pipe outp;
    boost::process::child process;
    boost::iostreams::stream<
        boost::iostreams::file_descriptor_sink
    > stdin;
};

#endif // GIT_FAST_IMPORT_DWA2013614_HPP
