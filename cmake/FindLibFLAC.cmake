# - Try to find libFLAC
# Once done this will define
#  LIBFLAC_FOUND        - System has libFLAC
#  LIBFLAC_INCLUDE_DIRS - The libFLAC include directories
#  LIBFLAC_LIBRARIES    - The libraries needed to use libFLAC

find_path(LIBFLAC_INCLUDE_DIR
  NAMES FLAC/all.h
)
find_library(LIBFLAC_LIBRARY
  NAMES FLAC
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBFLAC_FOUND to TRUE
# if all listed variables are TRUE and the requested version matches.
find_package_handle_standard_args(LibFLAC REQUIRED_VARS
                                  LIBFLAC_LIBRARY LIBFLAC_INCLUDE_DIR)

if(LIBFLAC_FOUND)
  set(LIBFLAC_LIBRARIES     ${LIBFLAC_LIBRARY})
  set(LIBFLAC_INCLUDE_DIRS  ${LIBFLAC_INCLUDE_DIR})
endif()

mark_as_advanced(LIBFLAC_INCLUDE_DIR LIBFLAC_LIBRARY)
