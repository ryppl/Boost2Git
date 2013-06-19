// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "git_fast_import.hpp"
#include "git_executable.hpp"
#include <boost/iostreams/device/file_descriptor.hpp>

using namespace boost::process::initializers;
using namespace boost::process;
namespace iostreams = boost::iostreams;

git_fast_import::git_fast_import(std::string const& git_dir)
    : outp(boost::process::create_pipe()),
      process(
          boost::process::execute(
              run_exe(git_executable()),
              set_args(arg_vector(git_dir)),
              bind_stdin(iostreams::file_descriptor_source(outp.source, iostreams::close_handle)),
#if defined(BOOST_POSIX_API)
              close_fd(outp.sink),
#endif
              throw_on_error())),
      cin(iostreams::file_descriptor_sink(outp.sink, iostreams::close_handle))
{
}

git_fast_import::~git_fast_import()
{
    // Note: this might not be enough to avoid waiting forever for
    // process exit if there are other subprocesses whose input
    // streams are still open.
    close();
    wait_for_exit(process);
}

std::vector<std::string> 
git_fast_import::arg_vector(std::string const& git_dir)
{
    return { git_executable(), "fast-import", "--quiet" };
}
