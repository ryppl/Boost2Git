// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#define BOOST_TEST_MODULE svn2git
#include <boost/test/unit_test.hpp>
#include <boost/test/framework.hpp>
#include <svn_repos.h>
#include <apr_general.h>
#include <svn_pools.h>
#include <fstream>
#include <iostream>
#include <boost/preprocessor/repetition/enum_params.hpp>
#include <boost/preprocessor/repetition/enum_trailing_params.hpp>
#include <boost/preprocessor/repetition/enum_binary_params.hpp>
#include <boost/preprocessor/repetition/enum_trailing_binary_params.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/repeat.hpp>
#include <boost/preprocessor/iteration/iterate.hpp>

namespace process = boost::process;
using namespace boost::process::initializers;
namespace unit_testing = boost::unit_test::framework;
namespace fs = boost::filesystem;

struct write_file : std::ofstream
{
    template <class T>
    write_file(T const& name) : std::ofstream(name) {}

    using std::ofstream::operator<<;
    
    std::ostream& operator<<(char const* rhs)
    {
        return *this << std::string(rhs);
    }
};

template <class P>
P as_string(P x) { return x; }

std::string as_string(fs::path x) { return x.generic_string(); }

#define BOOST_PP_ITERATION_LIMITS (0, 5)
#define BOOST_PP_FILENAME_1 "run_sync.hpp"
#include BOOST_PP_ITERATE()

BOOST_AUTO_TEST_CASE(create_repo)
  {
  fs::remove_all("test-repo");
  fs::remove_all("test-ws");

  std::cerr << "Create test repository" << std::endl;
  svnadmin("create", "test-repo");

  fs::path root = fs::current_path();
  std::string uri = "file://" + root.generic_string() + "/test-repo";
  std::cerr << "uri = " << uri << std::endl;
  
  std::cerr << "Create basic structure" << std::endl;
  svn("mkdir", "--username", "test", "-m", "create trunk", uri + "/trunk");
  svn("mkdir", "--username", "test", "-m", "create tags", uri + "/tags");
  svn("mkdir", "--username", "test", "-m", "create branches", uri + "/branches");

  std::cerr << "Checkout working copy" << std::endl;
  svn("checkout", uri, "test-ws");

  std::cerr << "Add and commit a file" << std::endl;
  fs::current_path( root/"test-ws" );

  write_file("README.txt") << "This is the README";
  svn("add", "README.txt");
  svn("commit", "--username", "test", "-m", "no message");

  write_file("README.txt") << "This is the new README";
  svn("commit", "--username", "test", "-m", "updated README");

  svn("cp", "--username", "test", "-m", "create branch", uri + "/trunk", uri + "/branches/my_branch");
  svn("cp", "--username", "test", "-m", "create tag", uri + "/trunk", uri + "/tags/my_tag");

  static char const* const* const argv = unit_testing::master_test_suite().argv;
  
  fs::current_path( root );
  run_sync(
      argv[1], "--add-metadata", "--exit-success", "--rules", argv[2], "--svnrepo", "test-repo", "--authors", argv[3]);
  
  BOOST_CHECK(fs::exists( root/"everything.git" ));
  }
