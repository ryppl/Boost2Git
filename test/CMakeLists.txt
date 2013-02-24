#=============================================================================
# Copyright (C) 2013 Daniel Pfeifer <daniel@pfeifer-mail.de>
#=============================================================================

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(Svn2GitTests CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

find_package(Boost REQUIRED system unit_test_framework filesystem)
include_directories(${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/boost.process ${Boost_INCLUDE_DIR})

find_package(APR REQUIRED)
find_package(SVN REQUIRED fs repos subr)
include_directories(${APR_INCLUDE_DIRS} ${SVN_INCLUDE_DIRS})


# This is required when linking against a dynamic Boost unit test
# framework; otherwise we get a link error looking for "main"
add_definitions(-DBOOST_TEST_DYN_LINK)

add_executable(
  test-driver EXCLUDE_FROM_ALL
  test-driver.cpp)

target_link_libraries(test-driver 
  ${Boost_LIBRARIES}
  ${APR_LIBRARIES} ${SVN_LIBRARIES}
)

find_program(SVN NAMES svn)
find_program(SVNADMIN NAMES svnadmin)

add_test(update-driver "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target test-driver)

add_test(NAME svn2git COMMAND 
  test-driver $<TARGET_FILE:svn2git> 
  ${CMAKE_CURRENT_LIST_DIR}/test-repositories.txt 
  ${CMAKE_CURRENT_LIST_DIR}/test-authors.txt 
  ${SVN} ${SVNADMIN})

set_tests_properties(
  svn2git
  PROPERTIES
    DEPENDS test-update-driver)
