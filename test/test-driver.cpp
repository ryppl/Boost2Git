// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include <boost/process.hpp>
#include <iostream>
#define BOOST_TEST_MODULE svn2git
#include <boost/test/unit_test.hpp>
#include <boost/test/framework.hpp>

namespace process = boost::process;
using namespace boost::process::initializers;
namespace unit_testing = boost::unit_test::framework;

BOOST_AUTO_TEST_CASE(help)
  {
  process::execute(
      run_exe(unit_testing::master_test_suite().argv[1]),
      set_cmd_line("svn2git --help")
    );
  }
