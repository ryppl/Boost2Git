#=============================================================================
# Copyright (C) 2012 Daniel Pfeifer <daniel@pfeifer-mail.de>
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt
#=============================================================================

find_path(APR_INCLUDE_DIR apr.h
  PATH_SUFFIXES
    apr
    apr-1
    apr-1.0
  )

find_library(APR_LIBRARY
  NAMES
    apr-1
    libapr-1
  PATH_SUFFIXES
    apr
  )

if(APR_INCLUDE_DIR)
  file(STRINGS "${APR_INCLUDE_DIR}/apr_version.h" APR_VERSION
    REGEX "#define APR_(MAJOR|MINOR|PATCH)_VERSION"
    )
  string(REGEX REPLACE "#define APR_(MAJOR|MINOR|PATCH)_VERSION *" ""
    APR_VERSION "${APR_VERSION}"
    )
  string(REPLACE ";" "." APR_VERSION "${APR_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(APR
  REQUIRED_VARS APR_LIBRARY APR_INCLUDE_DIR
  VERSION_VAR APR_VERSION
  )

mark_as_advanced(
  APR_LIBRARY
  APR_INCLUDE_DIR
  )

set(APR_INCLUDE_DIRS
  ${APR_INCLUDE_DIR}
  )

set(APR_LIBRARIES
  ${APR_LIBRARY}
  )
