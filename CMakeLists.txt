cmake_minimum_required(VERSION 3.14)

set(VERSION_MAJOR "0")
set(VERSION_MINOR "15")
set(VERSION_PATCH "0")
set(VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

#
# Avoid source tree pollution
#
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

If(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not permitted. Make a separate folder for building:\nmkdir build; cd build; cmake ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.[...]")
endif(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)

project(hrmp VERSION ${VERSION_STRING} LANGUAGES  C)

set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES
  "/build/;/test/;/.cache/;/.git/;/.github/;/*.patch;/.bundle/;/_site/;/vendor/;~$;${CPACK_SOURCE_IGNORE_FILES}")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
include(CPack)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT DEFINED DOCS)
  set(DOCS TRUE)
endif()

message(STATUS "hrmp ${VERSION_STRING}")

include(CheckCCompilerFlag)
include(CheckCSourceCompiles)
include(FindPackageHandleStandardArgs)
include(GNUInstallDirs)
include(CheckIncludeFile)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: Debug Release" FORCE)
endif ()

message(STATUS "Build type is ${CMAKE_BUILD_TYPE}")
message(STATUS "System is ${CMAKE_SYSTEM_NAME}")

set(SUPPORTED_COMPILERS "GNU" "Clang" "AppleClang")

# Check for a supported compiler
if (NOT CMAKE_C_COMPILER_ID IN_LIST SUPPORTED_COMPILERS)
   message(FATAL_ERROR "Unsupported compiler ${CMAKE_C_COMPILER_ID}. Supported compilers are: ${SUPPORTED_COMPILERS}")
endif ()

CHECK_C_COMPILER_FLAG("-std=c17" COMPILER_SUPPORTS_C17)
if(NOT COMPILER_SUPPORTS_C17)
  message(FATAL_ERROR "The compiler ${CMAKE_C_COMPILER} has no C17 support. Please use a different C compiler.")
endif()

find_package(Libasound)
if (LIBASOUND_FOUND)
  message(STATUS "libasound found")
else ()
  message(FATAL_ERROR "libasound needed")
endif()

find_package(Libsndfile)
if (LIBSNDFILE_FOUND)
  message(STATUS "libsndfile found")
else ()
  message(FATAL_ERROR "libsndfile needed")
endif()

find_package(PkgConfig REQUIRED)

pkg_check_modules(OPUS REQUIRED opus)
if (OPUS_FOUND)
  message(STATUS "libopus found")
else()
  message(FATAL_ERROR "libopus needed")
endif()

pkg_check_modules(FAAD2 faad2)
if (FAAD2_FOUND)
  message(STATUS "libfaad2 found")
else()
  message(FATAL_ERROR "libfaad2 needed")
endif()

pkg_check_modules(GTK3 gtk+-3.0)
if (GTK3_FOUND)
  message(STATUS "gtk3 found")
else()
  message(FATAL_ERROR "gtk3 needed")
endif()

find_package(Rst2man)
if (RST2MAN_FOUND)
  message(STATUS "rst2man found")
else ()
  message(FATAL_ERROR "rst2man needed")
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  find_package(Libatomic)
  if (LIBATOMIC_FOUND)
    message(STATUS "libatomic found")
  else ()
    message(FATAL_ERROR "libatomic needed")
  endif()
endif()

if (DOCS)
  find_package(Doxygen)

  if (DOXYGEN_FOUND)
    message(status "Doxygen found: ${DOXYGEN_EXECUTABLE}")
  endif()

  find_package(Pandoc)
  if (PANDOC_FOUND)
    message(STATUS "pandoc found")
  else ()
    set(DOCS FALSE)
    message(STATUS "pandoc needed. The manual will be skipped.")
  endif()

  find_package(Pdflatex)
  if (PDFLATEX_FOUND)
    message(STATUS "pdflatex found")
  else ()
    set(DOCS FALSE)
    message(STATUS "pdflatex needed. The manual will be skipped.")
  endif()
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src/")

add_subdirectory(doc)
add_subdirectory(src)
