// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define PUSH_BACK_ARG(z, n, _) \
    args.push_back(BOOST_PP_CAT(a, n));

#define STREAM(z, n, _)  << ", " << BOOST_PP_CAT(a, n)

#define n BOOST_PP_ITERATION()

template <class Exe BOOST_PP_ENUM_TRAILING_PARAMS(n, class A) >
void run_sync(Exe const& exe BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n, A, const& a))
  {
  std::string exe_str = as_string(exe);
  fs::path exe_path = exe_str[0] == '/' || exe_str[0] == '.'
    ? fs::path(exe_str) : process::search_path(exe_str);

  std::vector<std::string> args(1, exe_str);
  BOOST_PP_REPEAT(n, PUSH_BACK_ARG, ~)

  std::cerr << "++ in " << fs::current_path() << std::endl;
  std::cerr << "++ process::execute(" << exe_path BOOST_PP_REPEAT(n, STREAM, ~)
  << ")" << std::endl << std::flush;

  wait_for_exit(process::execute(
     run_exe(exe_path), set_args(args), throw_on_error() ));
  }
