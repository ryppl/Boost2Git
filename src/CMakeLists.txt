#=============================================================================
# Copyright (C) 2013 Daniel Pfeifer <daniel@pfeifer-mail.de>
#=============================================================================

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(Svn2Git CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

find_package(Boost REQUIRED date_time program_options regex)
include_directories(${BoostINCLUDE_DIRS})

find_package(APR REQUIRED)
include_directories(${APR_INCLUDE_DIRS})

find_package(SVN REQUIRED fs repos subr)
include_directories(${SVN_INCLUDE_DIRS})

find_package(Qt4 REQUIRED QtCore)
include(${QT_USE_FILE})

add_definitions(
  -D_XOPEN_SOURCE
  -D_LARGEFILE_SUPPORT
  -D_LARGEFILE64_SUPPORT
  )

if(${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
  set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++")
  set(CMAKE_CXX_LINK_FLAGS "-std=c++11 -stdlib=libc++")
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
    add_definitions(-Doverride)
  endif()
endif()

add_executable(svn2git
  apr_init.hpp
  apr_pool.hpp
  authors.cpp
  authors.hpp
  log.cpp
  log.hpp
  logging_process.hpp
  repository.cpp
  repository.h
  ruleset.cpp
  ruleset.hpp
  svn_revision.cpp
  svn_revision.hpp
  svn.cpp
  svn.h
  main.cpp
  )
target_link_libraries(svn2git
  ${Boost_LIBRARIES}
  ${APR_LIBRARIES}
  ${SVN_LIBRARIES}
  ${QT_LIBRARIES}
  )
