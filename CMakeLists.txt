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
set(ignore_rev   "${Boost2Git_SOURCE_DIR}/ignore_rev.txt")
set(repositories "${Boost2Git_SOURCE_DIR}/repositories.txt")

# clean
set(repositories_setup "${git_repository}/_setup")
add_custom_command(OUTPUT "${repositories_setup}"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${git_repository}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${git_repository}"
  COMMAND ${CMAKE_COMMAND} -E touch "${repositories_setup}"
  DEPENDS svn2git "${authors}" "${ignore_rev}" "${repositories}"
  COMMENT "Cleaning temporary repositories."
  )

# perform conversion
add_custom_target(conversion
  COMMAND
    $<TARGET_FILE:svn2git>
    --add-metadata
    --authors "${authors}"
    --ignore  "${ignore_rev}"
    --rules   "${repositories}"
    --svnrepo "${svn_repository}"
  COMMENT
    "Performing conversion."
  DEPENDS
    "${repositories_setup}"
  WORKING_DIRECTORY
    "${git_repository}"
  )

add_custom_target(submodules
  COMMAND ${CMAKE_COMMAND} 
  -D "GIT=${GIT_EXECUTABLE}"
  -D "RULES_FILE=${repositories}"
  -D "SRC_REPO=boost"
  -D "DST_REPO=boost-fixup"
  -D "FIX_SUBMODULE_REFS=$<TARGET_FILE:fix-submodule-refs>"
  -P "${Boost2Git_SOURCE_DIR}/fix_submodules.cmake"
  COMMENT
    "Fixing submodule references."
  DEPENDS
    conversion fix-submodule-refs 
    "${Boost2Git_SOURCE_DIR}/fix_submodules.cmake"
  WORKING_DIRECTORY
    "${git_repository}"
  )

# perform conversion
add_custom_target(analysis
  COMMAND
    $<TARGET_FILE:svn2git>
    --dry-run
    --coverage
    --authors "${authors}"
    --ignore  "${ignore_rev}"
    --rules   "${repositories}"
    --svnrepo "${svn_repository}"
  DEPENDS
    svn2git
  COMMENT
    "Performing analysis."
  WORKING_DIRECTORY
    "${git_repository}"
  )

find_package(Git REQUIRED)

set(push_targets )

# push all repositories
file(STRINGS "${repositories}" repo_lines REGEX "^repository")
foreach(line IN LISTS repo_lines)
  string(REGEX MATCH "^repository ([^ :]+)" match "${line}")
  string(REPLACE "\"" "" name "${CMAKE_MATCH_1}")
  if(NOT TARGET push_${name})
    list(APPEND push_targets push_${name})
    add_custom_target(push_${name} ALL
      COMMAND ${CMAKE_COMMAND}
        -D "GIT=${GIT_EXECUTABLE}"
        -D "NAME=${name}"
        -P "${Boost2Git_SOURCE_DIR}/git_push.cmake"
      DEPENDS submodules
      WORKING_DIRECTORY "${git_repository}/${name}"
      )
  endif()
endforeach()

add_custom_target(push_fallback ALL
  # Ensure that there's a master branch in the fallback repository so
  # that it can be pushed.  If rule coverage is complete, this repo
  # will be empty.  Also record the branch list in a file
  # "branch_list.txt"
  COMMAND ${CMAKE_COMMAND}
    -D "GIT=${GIT_EXECUTABLE}"
    -D "DEFAULT_CONTENT=${CMAKE_CURRENT_SOURCE_DIR}/empty-fallback-fast-import.txt"
    -P "${Boost2Git_SOURCE_DIR}/ensure_git_pushable.cmake"
  # Push the repository to github and bitbucket
  COMMAND ${CMAKE_COMMAND}
    -D "GIT=${GIT_EXECUTABLE}"
    -D "NAME=svn2git-fallback"
    -P "${Boost2Git_SOURCE_DIR}/git_push.cmake"
  # Read "branch_list.txt" and if it's non-empty, fail with a message
  # about an incomplete ruleset
  COMMAND ${CMAKE_COMMAND}
    -P "${Boost2Git_SOURCE_DIR}/fail_incomplete_ruleset.cmake"

  # Depends on push_targets so that we don't fail too early.  If the
  # ruleset is incomplete, we still want to be sure we execute all
  # other build steps.
  DEPENDS conversion ${push_targets}
  WORKING_DIRECTORY "${git_repository}/svn2git-fallback"
  )
