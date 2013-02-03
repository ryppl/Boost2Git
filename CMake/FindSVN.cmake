#=============================================================================
# Copyright (C) 2012 Daniel Pfeifer <daniel@pfeifer-mail.de>
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt
#=============================================================================

#find_package(APR)

find_path(SVN_INCLUDE_DIR svn_version.h
  PATH_SUFFIXES
    subversion
    subversion-1
  )

mark_as_advanced(SVN_INCLUDE_DIR)

if(SVN_INCLUDE_DIR)
  file(STRINGS "${SVN_INCLUDE_DIR}/svn_version.h" SVN_VERSION
    REGEX "#define SVN_VER_(MAJOR|MINOR|PATCH) "
    )
  string(REGEX REPLACE "#define SVN_VER_(MAJOR|MINOR|PATCH) *" ""
    SVN_VERSION "${SVN_VERSION}"
    )
  string(REPLACE ";" "." SVN_VERSION "${SVN_VERSION}")
endif()

set(SVN_INCLUDE_DIRS
  ${SVN_INCLUDE_DIR}
  ${APR_INCLUDE_DIR}
  )

set(SVN_LIBRARIES)
set(required_vars)

foreach(library IN LISTS SVN_FIND_COMPONENTS)
  string(TOUPPER "SVN_${library}_LIBRARY" LIBRARY)
  find_library(${LIBRARY} NAMES libsvn_${library}-1 svn_${library}-1)
  list(APPEND SVN_LIBRARIES ${${LIBRARY}})
  list(APPEND required_vars ${LIBRARY})
  mark_as_advanced(${LIBRARY})
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SVN
  REQUIRED_VARS ${required_vars} SVN_INCLUDE_DIR
  VERSION_VAR SVN_VERSION
  )
