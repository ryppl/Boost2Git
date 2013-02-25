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

add_custom_target(test-svn-repo 
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${REPO_PATH}"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${WC_PATH}"
  COMMAND "${SVNADMIN}" create test-repo
  COMMAND "${SVN}" mkdir ${LOG_MSG} "create trunk/" ${REPO_URI}/trunk
  COMMAND "${SVN}" mkdir ${LOG_MSG} "create tags/" ${REPO_URI}/tags
  COMMAND "${SVN}" mkdir ${LOG_MSG} "create branches/" ${REPO_URI}/branches
  COMMAND "${SVN}" checkout "${REPO_URI}" test-wc
  COMMAND echo "This is the README" > "${WC_PATH}/README.txt"
  COMMAND ${IN_WC} "${SVN}" add README.txt
  COMMAND ${IN_WC} "${SVN}" commit ${LOG_MSG} "first commit" 
  COMMAND echo "This is the NEW README" > "${WC_PATH}/README.txt"
  COMMAND ${IN_WC} "${SVN}" commit ${LOG_MSG} "updated README" 
  COMMAND "${SVN}" cp ${LOG_MSG} "create branch" "${REPO_URI}/trunk" "${REPO_URI}/branches/branch1"
  COMMAND "${SVN}" cp ${LOG_MSG} "create tag" "${REPO_URI}/trunk" "${REPO_URI}/tags/tag1"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "everything"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "svn2git-fallback"
)

add_test(NAME prepare
  COMMAND "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target test-svn-repo)

add_test(NAME conversion 
  COMMAND
    $<TARGET_FILE:svn2git>
    --extra-verbose
    --debug-rules
    --add-metadata
    --exit-success
    --authors "${CMAKE_CURRENT_LIST_DIR}/test-authors.txt"
    --rules   "${CMAKE_CURRENT_LIST_DIR}/test-repositories.txt"
    --svnrepo "${REPO_PATH}"
  )

# TODO: check output of
#
#   git log --all --pretty=format:"%s" --graph --decorate=full

set_tests_properties(conversion PROPERTIES 
  DEPENDS prepare
  FAIL_REGULAR_EXPRESSION "(^|
)[+][+] WARNING: "
  )
