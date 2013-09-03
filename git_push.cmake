# Created by Daniel Pfeifer <daniel@pfeifer-mail.de>
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt

execute_process(COMMAND ${GIT} push --quiet --mirror git@bitbucket.org:boostorg/${NAME}.git
  RESULT_VARIABLE result
  )

if(NOT result EQUAL 0)
  # try again
  execute_process(COMMAND ${GIT} push --quiet --mirror git@bitbucket.org:boostorg/${NAME}.git
    RESULT_VARIABLE result
    )
endif()

if(NOT result EQUAL 0)
  message(FATAL_ERROR "Failed to push ${NAME} to bitbucket!")
endif()

# GitHub does not accept files bigger than 100MB. The sandbox is on Bitbucket only.
if("${NAME}" STREQUAL "sandbox")
  return()
endif()

execute_process(COMMAND ${GIT} push --quiet --mirror git@github.com:boostorg/${NAME}.git
  RESULT_VARIABLE result
  )

if(NOT result EQUAL 0)
  # try again
  execute_process(COMMAND ${GIT} push --quiet --mirror git@github.com:boostorg/${NAME}.git
    RESULT_VARIABLE result
    )
endif()

if(NOT result EQUAL 0)
  message(FATAL_ERROR "Failed to push ${NAME} to github!")
endif()
