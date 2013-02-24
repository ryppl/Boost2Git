// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include <boost/process.hpp>
#include <iostream>

namespace process = boost::process;
using namespace boost::process::initializers;

int main(int argc, char const* argv[])
  {
  process::execute(
      run_exe(argv[1]),
      set_cmd_line("svn2git --help")
    );
  return 0;
  }
