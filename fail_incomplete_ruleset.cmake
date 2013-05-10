# Copyright Dave Abrahams 2013
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt

file(READ branch_list.txt branch_list)

if(branch_list)
  message(FATAL_ERROR "
error: incomplete ruleset (fallback repository has commits)!
see http://github.com/boostorg/svn2git-fallback
see http://bitbucket.org/boostorg/svn2git-fallback")
endif()
