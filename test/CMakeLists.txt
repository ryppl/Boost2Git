#=============================================================================
# Copyright (C) 2013 Daniel Pfeifer <daniel@pfeifer-mail.de>
#=============================================================================

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(Svn2GitTests CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

find_program(SVN NAMES svn)
find_program(SVNADMIN NAMES svnadmin)

set(REPO_PATH "${CMAKE_CURRENT_BINARY_DIR}/test-repo")
set(REPO_URI "file://${REPO_PATH}")
set(WC_PATH "${CMAKE_CURRENT_BINARY_DIR}/test-wc")
set(IN_WC "${CMAKE_COMMAND}" -E chdir "${WC_PATH}")
set(LOG_MSG --username test -m)

find_package(Boost REQUIRED filesystem system)
include_directories(${Boost_INCLUDE_DIRS} ../src)

function(prepared_test)
  cmake_parse_arguments(prepared_test "" "NAME;DEPENDENCY" "" ${ARGN})
  
  # Create a preparation test that builds the dependency
  add_test(NAME prepare_${prepared_test_NAME}
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" 
    --target ${prepared_test_DEPENDENCY})

  # Declare the actual test
  add_test(NAME ${prepared_test_NAME} ${prepared_test_UNPARSED_ARGUMENTS})
  
  # Make the prepared test depend on the preparation test
  set_tests_properties(
    ${prepared_test_NAME} PROPERTIES DEPENDS prepare_${prepared_test_NAME})
endfunction()

function(executable_test)
  cmake_parse_arguments(executable_test "" "NAME" "SOURCES" ${ARGN})
  add_executable(${executable_test_NAME}_program 
    EXCLUDE_FROM_ALL ${executable_test_SOURCES})
  prepared_test(
    NAME ${executable_test_NAME} 
    DEPENDENCY ${executable_test_NAME}_program 
    COMMAND ${executable_test_NAME}_program)
endfunction()

executable_test(NAME patrie_test SOURCES patrie_test.cpp)
executable_test(NAME path_set_test SOURCES path_set_test.cpp)
target_link_libraries(path_set_test_program ${Boost_LIBRARIES})

add_custom_command(OUTPUT ${REPO_PATH}
  COMMAND "${CMAKE_COMMAND}" 
    -DCMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR} 
    -P ${CMAKE_CURRENT_SOURCE_DIR}/PrepareSVNRepo.cmake
  DEPENDS PrepareSVNRepo.cmake
)

add_custom_target(prepare_conversion_test 
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "everything"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "svn2git-fallback"
  COMMAND "${CMAKE_COMMAND}" -E 
   remove -f "log-everything" "gitlog-everything" 
             "log-svn2git-fallback" "gitlog-svn2git-fallback"
  DEPENDS svn2git ${REPO_PATH}
)

set(GIT_EXECUTABLE "${CMAKE_BINARY_DIR}/git/git")

prepared_test(NAME conversion DEPENDENCY prepare_conversion_test
   COMMAND
     $<TARGET_FILE:svn2git>
     --git     "${GIT_EXECUTABLE}"
     --extra-verbose
     --debug-rules
     --add-metadata
     --exit-success
     --authors "${CMAKE_CURRENT_LIST_DIR}/test-authors.txt"
     --rules   "${CMAKE_CURRENT_LIST_DIR}/test-repositories.txt"
    --svnrepo "${REPO_PATH}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
   )

# TODO: check output of
#
#   git log --all --pretty=format:"%s %d" --graph

set_tests_properties(
  conversion PROPERTIES FAIL_REGULAR_EXPRESSION "(^|
)[+][+] WARNING: "
  )



