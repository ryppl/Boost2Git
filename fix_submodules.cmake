# Copyright 2013 David Abrahams
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt

execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${DST_REPO}"
  ERROR_VARIABLE message
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Failed to create directory ${DST_REPO}: ${message}")
endif()

execute_process(COMMAND ${GIT} init --bare
  WORKING_DIRECTORY "${DST_REPO}"
  ERROR_VARIABLE message
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Failed to initialize repository ${DST_REPO}: ${message}")
endif()

execute_process(COMMAND bash -e -o pipefail -c
  "( cd '${SRC_REPO}' && '${GIT}' fast-export --all ) | (
    '${FIX_SUBMODULE_REFS}' --rules '${RULES_FILE}' --repo-name '${SRC_REPO}' ) | (
    cd '${DST_REPO}' && '${GIT}' fast-import --quiet --force )"
  ERROR_VARIABLE message
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Failed to fix submodule refs: ${message}")
endif()
