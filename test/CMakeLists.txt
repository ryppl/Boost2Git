#=============================================================================
# Copyright (C) 2013 Daniel Pfeifer <daniel@pfeifer-mail.de>
#=============================================================================

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(Svn2GitTests CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

find_package(Boost REQUIRED system)
include_directories(${CMAKE_CURRENT_LIST_DIR}/boost.process ${Boost_INCLUDE_DIR})

add_executable(
  test-driver EXCLUDE_FROM_ALL
  test-driver.cpp)
target_link_libraries(test-driver ${Boost_LIBRARIES})

ADD_TEST(test-update-driver "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target test-driver)

add_test(NAME rules COMMAND test-driver ../src/svn2git)
set_tests_properties(
  rules
  PROPERTIES
    DEPENDS test-update-drivers
    PASS_REGULAR_EXPRESSION "ran driver")

add_test(NAME help-cmd2 COMMAND svn2git --help)
SET_TESTS_PROPERTIES (
  help-cmd2
  PROPERTIES 
    DEPENDS test-update-svn2git
    PASS_REGULAR_EXPRESSION "Allowed options:")
