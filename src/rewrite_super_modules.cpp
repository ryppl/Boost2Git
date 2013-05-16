// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include "rewrite_super_modules.hpp"
#include "mark_sha_map.hpp"
#include <boost/fusion/adapted/struct/define_struct.hpp>
#include "repository.h"
#include "log.hpp"
#include <boost/foreach.hpp>
#include <set>
#include <map>
#include <QString>
#include <fstream>
#include <boost/process.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <QProcess>

QString marksFileName(std::string name);

static void close(QProcess& fastImport)
  {
  if (fastImport.state() != QProcess::NotRunning)
    {
    fastImport.write("checkpoint\n");
    fastImport.waitForBytesWritten(-1);
    fastImport.closeWriteChannel();
    if (!fastImport.waitForFinished())
      {
      fastImport.terminate();
      if (!fastImport.waitForFinished(200))
        {
        Log::warn() << "git-fast-import did not die" << std::endl;
        }
      }
    }
  }

mark_sha_map Repository::readMarksFile() const
  {
  mark_sha_map ret;
  std::string marks_file_name = this->name + "/" + marksFileName(this->name).toStdString();
  
  std::ifstream marks(marks_file_name.c_str());
  marks.exceptions( std::ifstream::badbit );
  char colon, newline;
  std::pair<unsigned long, std::string> mark_sha;
  while (marks >> colon >> mark_sha.first >> mark_sha.second)
    {
    if (colon != ':')
        throw std::runtime_error("Expected colon in marks file!");
    marks.read(&newline, 1);
    if (newline != '\n')
        throw std::runtime_error("Expected newline in marks file!");

    // Insert the mapping.  All kinds of efficiencies are possible
    // here, but I was more interested in getting the logic right.
    mark_sha_map::iterator where = find_sha_pos(ret, mark_sha.first);
    
    // Make sure we're not mapping the same mark twice.
    if (where != ret.end() && where->first == mark_sha.first)
        throw std::runtime_error("Duplicate mark mapping!");
    
    ret.insert(where, mark_sha);
    }
  return ret;
  }

void rewrite_super_modules(std::vector<Repository*> const& repositories)
  {

  Log::info() << "rewriting super-modules in " << boost::filesystem::current_path() << std::endl;
  //
  // Setup: Locate all super/sub-modules, index and read all submodule
  // mark files
  //
  std::set<Repository*> super_modules;
  
  // Map from (super-module, path) to submodule
  std::map<std::pair<Repository*, std::string>, Repository*> submodules;

  // Map from repository to a mark/sha map.
  std::map<Repository*, mark_sha_map> mark2sha;
    
  BOOST_FOREACH(Repository* r, repositories)
    {
    if (r->submodule_in_repo)
      {
      submodules[make_pair(r->submodule_in_repo, r->submodule_path)] = r;
      super_modules.insert(r->submodule_in_repo);
      mark2sha[r] = r->readMarksFile();
      }
    }

  std::string git_exe = boost::process::search_path("git");
  std::vector<std::string> git_args(3);
  git_args[0] = "git";

  BOOST_FOREACH(Repository* super, super_modules)
    {
    using namespace boost::process;
    using boost::process::pipe;
    using namespace boost::process::initializers;
    using namespace boost::iostreams;
    namespace fs = boost::filesystem;
    
    // Rename the source repository
    std::string const import_repo_name = super->name + "-rewrite";
    fs::rename(super->name, import_repo_name);

    // Create the new repository
    fs::create_directory(super->name);
    git_args[1] = "init";
    git_args[2] = "--bare";
    child git_init = execute(
        run_exe(git_exe),
        set_args(git_args), start_in_dir(super->name), throw_on_error());
    wait_for_exit(git_init);

    pipe inp = create_pipe();
    git_args[1] = "fast-export";
    git_args[2] = "--all";
    child git_fast_export = execute(
        run_exe(git_exe),
        set_args(git_args),
        start_in_dir(import_repo_name),
        bind_stdout(file_descriptor_sink(inp.sink, close_handle)),
        throw_on_error());
    
    stream<file_descriptor_source> in(
        file_descriptor_source(inp.source, close_handle));
    in.exceptions( std::istream::badbit );


#ifdef USE_BOOST_PROCESS
    pipe outp = create_pipe();
    git_args[1] = "fast-import";
    git_args[2] = "--quiet";
    child git_fast_import = execute(
        run_exe(git_exe),
        set_args(git_args),
        start_in_dir(super->name),
        bind_stdin(file_descriptor_source(outp.source, close_handle)),
        throw_on_error());

    stream<file_descriptor_sink> out(
        file_descriptor_sink(outp.sink, close_handle));
    out.exceptions( std::istream::failbit | std::istream::badbit );
#else
    QProcess git_fast_import;
    git_fast_import.setWorkingDirectory(QString::fromStdString(super->name));
    git_fast_import.setProcessChannelMode(QProcess::MergedChannels);

    git_fast_import.start("git", QStringList() << "fast-import" << "--quiet");
#endif
    
    char const submodule_prefix[] = "M 160000 ";
    std::size_t const submodule_prefix_length = std::strlen(submodule_prefix);
    std::size_t const sha_length = 40;

    char const data_prefix[] = "data ";
    std::size_t const data_prefix_length = std::strlen(data_prefix);

    std::string line;

#if defined(BOOST_POSIX_API)
      // signal(SIGCHLD, SIG_IGN);
#endif
  
    while (getline(in, line))
      {
#ifndef USE_BOOST_PROCESS
      std::stringstream out;
#endif 
      if (boost::starts_with(line, submodule_prefix))
        {
        unsigned long mark = boost::lexical_cast<unsigned long>(
            line.substr(submodule_prefix_length, sha_length));
        std::string submodule_path = line.substr(submodule_prefix_length + sha_length + 1);

        Repository* sub_repo = submodules[make_pair(super,submodule_path)];
        assert(sub_repo);
        
        mark_sha_map::iterator const p = find_sha_pos(mark2sha[sub_repo], mark);
        assert(p->first == mark);
        out << submodule_prefix << p->second << " " << submodule_path << "\n";
        }
      else
        {
        out << line << "\n";
        }

#ifndef USE_BOOST_PROCESS
      git_fast_import.write(out.str().c_str(), out.str().size());
#endif 
      if (boost::starts_with(line, data_prefix))
        {
        std::size_t length = boost::lexical_cast<std::size_t>(
            line.substr(data_prefix_length));
        
        while (length > 0)
          {
          char buf[2048];
          std::size_t num_to_read = std::min(length, sizeof(buf));
          in.read(buf, num_to_read);
#ifdef USE_BOOST_PROCESS
          out.write(buf, num_to_read);
#else
          git_fast_import.write(buf, num_to_read);
#endif 
          length -= num_to_read;
          }
        }
      }
    std::cout << "finishing supermodule..." << std::flush;
#ifdef USE_BOOST_PROCESS
    wait_for_exit(git_fast_import);
#else
    close(git_fast_import);
#endif 
    std::cout << "done." << std::endl << std::flush;
    }
  }

