# Created by Daniel Pfeifer <daniel@pfeifer-mail.de>
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 2.8.7 FATAL_ERROR)
project(Boost2Git)

# Enable C++11
if(${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
  if(APPLE)
    set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++")
    set(CMAKE_CXX_LINK_FLAGS "-std=c++11 -stdlib=libc++")
  else()
    set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libstdc++")
    set(CMAKE_CXX_LINK_FLAGS "-std=c++11 -stdlib=libstdc++")
  endif()
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL GNU)
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(-std=c++11 HAVE_STD11)
  if(HAVE_STD11)
    set(CMAKE_CXX_FLAGS -std=c++11)
  else()
    check_cxx_compiler_flag(-std=c++0x HAVE_STD0X)
    if(HAVE_STD0X)
      set(CMAKE_CXX_FLAGS -std=c++0x)
    endif()
  endif()
endif()

enable_testing()
add_subdirectory(src)
add_subdirectory(test)

if(NOT BOOST_SVN)
  message(STATUS "BOOST_SVN is not set. Disabling targets for conversion.")
  return()
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

set(GIT_EXECUTABLE "${CMAKE_BINARY_DIR}/git/git")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/git" DESTINATION "${CMAKE_BINARY_DIR}" PATTERN .git EXCLUDE)
add_custom_command( 
  OUTPUT "${GIT_EXECUTABLE}"
  COMMAND make template_dir="${CMAKE_BINARY_DIR}/git/templates"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/git"
  COMMENT "Updating Git"
  )

# perform conversion
add_custom_target(conversion
  COMMAND
    $<TARGET_FILE:svn2git>
    --add-metadata
    --git     "${GIT_EXECUTABLE}"
    --authors "${authors}"
    --rules   "${repositories}"
    --svnrepo "${svn_repository}"
  COMMENT
    "Performing conversion."
  DEPENDS
    "${repositories_setup}"
    "${GIT_EXECUTABLE}"
  WORKING_DIRECTORY
    "${git_repository}"
  )

add_custom_target(submodules
  COMMAND ${CMAKE_COMMAND} 
  -D "GIT=${GIT_EXECUTABLE}"
  -D "RULES_FILE=${repositories}"
  -D "SRC_REPO=${git_repository}/boost"
  -D "DST_REPO=${git_repository}/boost-fixup"
  -D "FIX_SUBMODULE_REFS=$<TARGET_FILE:fix-submodule-refs>"
  -P "${Boost2Git_SOURCE_DIR}/fix_submodules.cmake"
  COMMENT
    "Fixing submodule references."
  DEPENDS
    conversion fix-submodule-refs 
    "${Boost2Git_SOURCE_DIR}/fix_submodules.cmake"
    "${GIT_EXECUTABLE}"
  WORKING_DIRECTORY
    "${git_repository}"
  )

# perform conversion
add_custom_target(analysis
  COMMAND
    $<TARGET_FILE:svn2git>
    --dry-run
    --coverage
    --git     "${GIT_EXECUTABLE}"
    --authors "${authors}"
    --rules   "${repositories}"
    --svnrepo "${svn_repository}"
  DEPENDS
    svn2git
  COMMENT
    "Performing analysis."
  WORKING_DIRECTORY
    "${git_repository}"
  )

set(push_targets )

# push all repositories
file(STRINGS "${repositories}" repo_lines REGEX "^repository")
foreach(line IN LISTS repo_lines)
  string(REGEX MATCH "^repository ([^ :]+)" match "${line}")
  string(REPLACE "\"" "" name "${CMAKE_MATCH_1}")
  if(name STREQUAL boost)
    set(repo_name boost-fixup)
  else()
    set(repo_name ${name})
  endif()
  if(NOT TARGET push_${name})
    list(APPEND push_targets push_${name})
    add_custom_target(push_${name} ALL
      COMMAND ${Boost2Git_SOURCE_DIR}/post-conversion-cleanup
      COMMAND ${CMAKE_COMMAND}
        -D "GIT=${GIT_EXECUTABLE}"
        -D "NAME=${name}"
        -P "${Boost2Git_SOURCE_DIR}/git_push.cmake"
      DEPENDS
        submodules
        ${Boost2Git_SOURCE_DIR}/post-conversion-cleanup
      WORKING_DIRECTORY "${git_repository}/${repo_name}"
      )
  endif()
endforeach()
