# Created by Daniel Pfeifer <daniel@pfeifer-mail.de>
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 2.8.7 FATAL_ERROR)
project(Boost2Git)

enable_testing()
add_subdirectory(src)
add_subdirectory(test)

if(NOT BOOST_SVN)
  message(FATAL_ERROR "Please set BOOST_SVN to Boost's Subversion repository")
endif()

if(RAMDISK)
  set(git_repository "${RAMDISK}/conversion")
  set(svn_repository "${RAMDISK}/boost_svn")
  file(COPY "${BOOST_SVN}/" DESTINATION "${svn_repository}")
else()
  set(git_repository "${CMAKE_BINARY_DIR}/conversion")
  set(svn_repository "${BOOST_SVN}")
endif()

set(authors      "${Boost2Git_SOURCE_DIR}/authors.txt")
set(repositories "${Boost2Git_SOURCE_DIR}/repositories.txt")

# clean
set(repositories_setup "${git_repository}/_setup")
add_custom_command(OUTPUT "${repositories_setup}"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${git_repository}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${git_repository}"
  COMMAND ${CMAKE_COMMAND} -E touch "${repositories_setup}"
  DEPENDS svn2git "${authors}" "${repositories}"
  COMMENT "Cleaning temporary repositories."
  )

find_program(UNIQ uniq)
if(UNIQ)
  set(pipe_uniq "2>&1" "|" "${UNIQ}")
else()
  set(pipe_uniq)
endif()

# perform conversion
set(conversion_successful "${git_repository}/_conversion_successful")
add_custom_command(OUTPUT "${conversion_successful}"
  COMMAND
    ${CMAKE_COMMAND} -E remove "${conversion_successful}"
  COMMAND
    $<TARGET_FILE:svn2git>
    --add-metadata
#   --exit-success
    --authors "${authors}"
    --rules   "${repositories}"
    --svnrepo "${svn_repository}"
    ${pipe_uniq}
  COMMAND
    ${CMAKE_COMMAND} -E touch "${conversion_successful}"
  DEPENDS
    ${repositories_setup}
  WORKING_DIRECTORY
    "${git_repository}"
  )

find_package(Git REQUIRED)

# push all repositories
file(STRINGS "${repositories}" repo_lines REGEX "^repository")
foreach(line IN LISTS repo_lines)
  string(REGEX MATCH "^repository ([^ :]+)" match "${line}")
  string(REPLACE "\"" "" name "${CMAKE_MATCH_1}")
  if(NOT TARGET push_${name})
    add_custom_target(push_${name} ALL
      COMMAND ${CMAKE_COMMAND}
        -D "GIT=${GIT_EXECUTABLE}"
        -D "NAME=${name}"
        -P "${Boost2Git_SOURCE_DIR}/git_push.cmake"
      DEPENDS "${conversion_successful}"
      WORKING_DIRECTORY "${git_repository}/${name}"
      )
  endif()
endforeach()

add_custom_target(push_fallback ALL
  COMMAND git push --quiet --mirror git@bitbucket.org:boostorg/svn2git-fallback.git
  COMMAND git push --quiet --mirror git@github.com:boostorg/svn2git-fallback.git
  DEPENDS conversion
  WORKING_DIRECTORY "${git_repository}/svn2git-fallback"
  )
