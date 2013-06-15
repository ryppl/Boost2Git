#=============================================================================
# Copyright (C) 2013 Daniel Pfeifer <daniel@pfeifer-mail.de>
#=============================================================================

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(Svn2Git CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

# Workaround for https://svn.boost.org/trac/boost/ticket/3999#comment:6
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/override_headers)

find_package(Boost REQUIRED date_time program_options regex filesystem iostreams system)
include_directories(${Boost_INCLUDE_DIRS})

find_package(APR REQUIRED)
include_directories(${APR_INCLUDE_DIRS})

find_package(SVN REQUIRED fs repos subr)
include_directories(${SVN_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/boost_process)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/boost_process)

add_definitions(
  -D_XOPEN_SOURCE
  -D_LARGEFILE_SUPPORT
  -D_LARGEFILE64_SUPPORT
  -DFUSION_MAX_VECTOR_SIZE=20
  )

set(svn2git_SOURCES
  authors.cpp
  coverage.cpp
  log.cpp
  parse_rules.cpp
  ruleset.cpp
  git_fast_import.cpp
  git_repository.cpp
  main.cpp)

add_executable(svn2git ${svn2git_SOURCES})

target_link_libraries(svn2git
  ${Boost_LIBRARIES}
  ${APR_LIBRARIES}
  ${SVN_LIBRARIES}
  )

ADD_TEST(update-svn2git "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target svn2git)

add_test(NAME help-cmd COMMAND svn2git --help)
SET_TESTS_PROPERTIES (
  help-cmd 
  PROPERTIES 
    DEPENDS update-svn2git
    PASS_REGULAR_EXPRESSION "Allowed options:")

add_executable(validate_branch
  validate_branch.cpp
  )
target_link_libraries(validate_branch
  ${APR_LIBRARIES}
  ${SVN_LIBRARIES}
  )

add_executable(fix-submodule-refs
  fix-submodule-refs.cpp
  parse_rules.cpp
  )

target_link_libraries(fix-submodule-refs
  ${Boost_LIBRARIES}
)
