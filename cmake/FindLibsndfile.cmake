# - Try to find libSndfile
# Once done this will define
#  LIBSNDFILE_FOUND       - System has libsndfile
#  LIBSNDFILE_INCLUDE_DIR - The libsndfile include directory
#  LIBSNDFILE_LIBRARY     - The library needed to use libsndfile

find_path(LIBSNDFILE_INCLUDE_DIR
  NAMES sndfile.h
)
find_library(LIBSNDFILE_LIBRARY
  NAMES sndfile
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBSNDFILE_FOUND to TRUE
# if all listed variables are TRUE and the requested version matches.
find_package_handle_standard_args(Libsndfile REQUIRED_VARS
                                  LIBSNDFILE_LIBRARY LIBSNDFILE_INCLUDE_DIR)

if(LIBSNDFILE_FOUND)
  set(LIBSNDFILE_LIBRARY     ${LIBSNDFILE_LIBRARY})
  set(LIBSNDFILE_INCLUDE_DIR ${LIBSNDFILE_INCLUDE_DIR})
endif()

mark_as_advanced(LIBSNDFILE_INCLUDE_DIR LIBSNDFILE_LIBRARY)
