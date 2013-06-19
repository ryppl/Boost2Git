// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "git_repository.hpp"
#include "git_executable.hpp"
#include "log.hpp"

#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <array>
#include <boost/range/adaptor/map.hpp>

git_repository::git_repository(std::string const& git_dir)
    : git_dir(git_dir),
      created(ensure_existence(git_dir)),
      fast_import_(git_dir),
      super_module(nullptr),
      last_mark(0),
      current_revnum(0),
      current_ref(nullptr)
{
}

bool git_repository::ensure_existence(std::string const& git_dir)
{
    namespace process = boost::process;
    namespace fs = boost::filesystem;
    using namespace process::initializers;
    
    if (fs::exists(git_dir))
        return true;

    // Create the new repository
    fs::create_directories(git_dir);
    std::array<std::string, 4> git_args = { git_executable(), "init", "--bare", "--quiet" };
    auto git_init = process::execute(
        run_exe(git_executable()),
        set_args(git_args), 
        start_in_dir(git_dir), 
        throw_on_error());
    wait_for_exit(git_init);
    return true;
}

void git_repository::set_super_module(
    git_repository* super_module, std::string const& submodule_path)
{
    if (super_module)
    {
        if (this->super_module != nullptr)
        {
            if (this->super_module != super_module)
                throw std::runtime_error("Conflicting super-module specifications");
            if (this->submodule_path != submodule_path)
                throw std::runtime_error("Conflicting submodule path declarations");
        }
        this->super_module = super_module;
        this->submodule_path = submodule_path;
    }
}

// Close the current ref's commit.  Return false if there are any more
// modified refs; true otherwise.
bool git_repository::close_commit()
{
    // Logging
    if (Log::get_level() >= Log::Trace)
    {
        Log::trace() << "Repository " << git_dir << " changed:" << std::endl;
        for (auto r : modified_refs)
        {
            Log::trace() << "  ref " << r->name << " changed:" << std::endl;
            for (auto& d : r->pending_deletions)
                Log::trace() << "    delete " << d << std::endl;
        }
    }

    // Now that changes are written, clear all pending information
    modified_refs.clear();
    return true;

    assert(!"FIXME");
}

// I/O manipulator that sends a linefeed character with no translation
std::ostream& LF (std::ostream& stream)
{
    stream.rdbuf()->sputc('\n');
    return stream;
}

void git_repository::open_commit(int revnum)
{
    assert(!modified_refs.empty());

    current_revnum = revnum;

    current_ref = *std::prev(modified_refs.end());
    modified_refs.erase(current_ref);

    fast_import() << "commit " << current_ref->name << LF
                  << "mark :" 
                  << (current_ref->marks[revnum] = ++last_mark)
                  << LF;

    // Do any deletions required in this ref
    for (auto& p : current_ref->pending_deletions)
        fast_import() << "D " << p << LF;
    current_ref->pending_deletions.clear();
}
