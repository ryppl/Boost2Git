// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "git_fast_import.hpp"
#include "git_executable.hpp"
#include "path.hpp"
#include "options.hpp"
#include "marks_file_name.hpp"

#include <boost/iostreams/device/file_descriptor.hpp>
#include <numeric>

using namespace boost::process::initializers;
using namespace boost::process;
namespace iostreams = boost::iostreams;

git_fast_import::git_fast_import(std::string const& git_dir)
    : inp(boost::process::create_pipe()),
      outp(boost::process::create_pipe()),
      process(
          options.dry_run ? boost::optional<boost::process::child>()
          : boost::process::execute(
              run_exe(git_executable()),
              set_env(std::vector<std::string>({"GIT_DIR="+git_dir})),
              set_args(arg_vector(git_dir)),
              bind_stdout(iostreams::file_descriptor_sink(inp.sink, iostreams::close_handle)),
              bind_stdin(iostreams::file_descriptor_source(outp.source, iostreams::close_handle)),
#if defined(BOOST_POSIX_API)
              close_fd(outp.sink),
              close_fd(inp.source),
#endif
              throw_on_error())),
      cin(iostreams::file_descriptor_sink(outp.sink, iostreams::close_handle)),
      cout(iostreams::file_descriptor_source(inp.source, iostreams::close_handle))
{
}

git_fast_import::~git_fast_import()
{
    // Note: this might not be enough to avoid waiting forever for
    // process exit if there are other subprocesses whose input
    // streams are still open.
    close();
    if (process)
        wait_for_exit(*process);
}

std::vector<std::string> 
git_fast_import::arg_vector(std::string const& git_dir)
{
    return 
    { 
        git_executable(), "fast-import", "--quiet", "--force", 
        "--export-marks=" + marks_file_path(git_dir) 
    };
}

git_fast_import& git_fast_import::write_raw(char const* data, std::size_t nbytes)
{
#if 0
    if (Log::get_level() >= Log::Trace)
    {
        std::cerr.write(data, std::min(nbytes, (std::size_t)100));
        if (nbytes > 100)
            std::cerr << "...";
        std::cerr << std::endl;
    }
#endif 
    if (!options.dry_run)
        cin.write(data, nbytes);
    return *this;
}

// Just writes the header.  
git_fast_import& git_fast_import::data_hdr(std::size_t size)
{
    return *this << "data " << size << LF;
}

git_fast_import& git_fast_import::data(char const* data, std::size_t size)
{
    return data_hdr(size).write_raw(data, size) << LF;
}

git_fast_import& git_fast_import::commit(
    std::string const& ref_name, 
    std::size_t mark, 
    std::string const& author,
    unsigned long epoch,
    std::string const& log_message)
{
    *this << "commit " << ref_name << LF
          << "mark :" << mark << LF
          << "committer " << author << " " << epoch << " +0000" << LF;
    return data(log_message.data(), log_message.size());
}

git_fast_import& git_fast_import::filedelete(path const& p)
{
    return *this << "D " << p << LF;
}

git_fast_import& git_fast_import::filemodify_hdr(path const& p, unsigned long mode)
{
    return *this << "M " << std::oct << mode << std::dec << " inline " << p << LF;
}

git_fast_import& git_fast_import::checkpoint()
{
    return *this << "checkpoint" << LF << LF;
}

void git_fast_import::send_ls(std::string const& dataref_opt_path)
{
    *this << "ls " << dataref_opt_path << LF;
    cin << std::flush;
}

std::string git_fast_import::readline()
{
    std::string result;
    std::getline(cout, result);
    return result;
}

git_fast_import& git_fast_import::reset(std::string const& ref_name, int mark = -1)
{
    *this << "reset " << ref_name << LF;
    if (mark >= 0)
        *this << "from :" << mark << LF;
    return *this << LF;
}
