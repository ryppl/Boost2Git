# Created by Daniel Pfeifer <daniel@pfeifer-mail.de>
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 2.8.7 FATAL_ERROR)
project(Boost2Git)

add_subdirectory(src)

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
  set(pipe_uniq "2>&1" "|" "${UNIQ}" "-c")
else()
  set(pipe_uniq)
endif()

find_program(SLEEP sleep)

# perform conversion
add_custom_target(conversion ALL
  COMMAND
    $<TARGET_FILE:svn2git>
    --add-metadata
    --exit-success
    --authors "${authors}"
    --rules   "${repositories}"
    --svnrepo "${svn_repository}"
    ${pipe_uniq}
  DEPENDS
    ${repositories_setup}
  WORKING_DIRECTORY
    "${git_repository}"
  )

# set var to a push command for the given boostorg repo hosted at the given host
#
# We attempt to create a command that's somewhat resilient; if it
# fails the first time it will try again in 10 seconds, provided the
# "sleep" command is present.
function(set_gitpushmirror var host repo_name)
  set(gitpushmirror_cmd
    git push --quiet --progress --mirror git@${host}:boostorg/${repo_name}.git)
  if (SLEEP)
    set(
      ${var} "(" ${gitpushmirror_cmd} "||" ${SLEEP} 10 "&&" ${gitpushmirror_cmd} ")"
      PARENT_SCOPE)
  else()
    set(${var} ${gitpushmirror_cmd} PARENT_SCOPE)
  endif()
endfunction()

# push all repositories
file(STRINGS "${repositories}" repo_lines REGEX "^repository")
foreach(line IN LISTS repo_lines)
  string(REGEX MATCH "^repository ([^ :]+)" match "${line}")
  string(REPLACE "\"" "" name "${CMAKE_MATCH_1}")
  set_gitpushmirror(bitbucket_cmd bitbucket.org ${name})
  set_gitpushmirror(github_cmd github.com ${name})
  add_custom_target(push_${name} ALL
    COMMAND ${bitbucket_cmd}
    COMMAND ${github_cmd}
    DEPENDS conversion
    WORKING_DIRECTORY "${git_repository}/${name}"
    )
endforeach()
