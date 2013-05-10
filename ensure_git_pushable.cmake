# Created by Dave Abrahams <dave@boostpro.com>
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt

execute_process(COMMAND ${GIT} branch -v RESULT_VARIABLE result OUTPUT_FILE branch_list.txt)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "Failed to get branch list -- not a git repository?")
endif()

file(READ branch_list.txt branch_list)

if(NOT branch_list)
  execute_process(COMMAND ${GIT} fast-import --quiet INPUT_FILE ${DEFAULT_CONTENT})
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed import default repository content")
  endif()
endif()
