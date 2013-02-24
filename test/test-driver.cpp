// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include <boost/process.hpp>
#include <iostream>
#define BOOST_TEST_MODULE svn2git
#include <boost/test/unit_test.hpp>
#include <boost/test/framework.hpp>
#include <svn_repos.h>
#include <apr_general.h>
#include <svn_pools.h>

namespace process = boost::process;
using namespace boost::process::initializers;
namespace unit_testing = boost::unit_test::framework;

BOOST_AUTO_TEST_CASE(create_repo)
  {
  apr_status_t err = apr_initialize();
  BOOST_CHECK(err == APR_SUCCESS);
  
  svn_repos_t* repos;
  apr_pool_t* pool = svn_pool_create(0);
  BOOST_CHECK(pool != 0);
  
  svn_repos_create(
      &repos
      , "test-repo"
      , (char const*)0 // unused_1
      , (char const*)0 // unused_2
      , (apr_hash_t*)0 // config
      , (apr_hash_t*)0 // fs_config
      , pool);
  }

BOOST_AUTO_TEST_CASE(help)
  {
  process::execute(
      run_exe(unit_testing::master_test_suite().argv[1]),
      set_cmd_line("svn2git --help")
    );
  }
