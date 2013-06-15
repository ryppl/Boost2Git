// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "git_repository.hpp"
#include "git_executable.hpp"
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <array>

git_repository::git_repository(std::string const& git_dir)
    : created(ensure_existence(git_dir)),
      fast_import(git_dir),
      super_module(nullptr)
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
    std::array<std::string, 3> git_args = { git_executable(), "init", "--bare" };
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
