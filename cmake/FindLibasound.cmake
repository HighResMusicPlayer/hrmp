# - Try to find libasound
# Once done this will define
#  LIBASOUND_FOUND        - System has libasound
#  LIBASOUND_INCLUDE_DIRS - The libasound include directories
#  LIBASOUND_LIBRARIES    - The libraries needed to use libasound

find_path(LIBASOUND_INCLUDE_DIR
  NAMES alsa/asoundlib.h
)
find_library(LIBASOUND_LIBRARY
  NAMES asound
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBASOUND_FOUND to TRUE
# if all listed variables are TRUE and the requested version matches.
find_package_handle_standard_args(Libasound REQUIRED_VARS
                                  LIBASOUND_LIBRARY LIBASOUND_INCLUDE_DIR)

if(LIBASOUND_FOUND)
  set(LIBASOUND_LIBRARIES     ${LIBASOUND_LIBRARY})
  set(LIBASOUND_INCLUDE_DIRS  ${LIBASOUND_INCLUDE_DIR})
endif()

mark_as_advanced(LIBASOUND_INCLUDE_DIR LIBASOUND_LIBRARY)
