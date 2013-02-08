#=============================================================================
# Copyright (C) 2013 Daniel Pfeifer <daniel@pfeifer-mail.de>
#=============================================================================

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(Svn2Git CXX)

list(APPEND CMAKE_MODULE_PATH "${Svn2Git_SOURCE_DIR}/CMake")

find_package(APR REQUIRED)
include_directories(${APR_INCLUDE_DIRS})

find_package(SVN REQUIRED fs repos subr)
include_directories(${SVN_INCLUDE_DIRS})

find_package(Qt4 REQUIRED QtCore)
include(${QT_USE_FILE})

# git --no-pager show --pretty=oneline --no-notes | head -1 | cut -b-40
add_definitions(
  -DVER=\"unknown\"
  -D_XOPEN_SOURCE
  -D_LARGEFILE_SUPPORT
  -D_LARGEFILE64_SUPPORT
  )

add_executable(svn2git
  src/apr_pool.hpp
  src/CommandLineParser.cpp
  src/CommandLineParser.h
  src/logging_process.hpp
  src/repository.cpp
  src/repository.h
  src/rules_list.cpp
  src/rules_list.hpp
  src/rules.cpp
  src/rules.hpp
  src/stats.cpp
  src/stats.hpp
  src/svn_revision.cpp
  src/svn_revision.hpp
  src/svn.cpp
  src/svn.h
  src/main.cpp
  )
target_link_libraries(svn2git
  ${APR_LIBRARIES}
  ${SVN_LIBRARIES}
  ${QT_LIBRARIES}
  )
