#
# Add files for hrmp
#
FILE(GLOB SOURCE_FILES CONFIGURE_DEPENDS "libhrmp/*.c")
FILE(GLOB HEADER_FILES CONFIGURE_DEPENDS "include/*.h")

set(SOURCES ${SOURCE_FILES} ${HEADER_FILES})

add_compile_options(-DHAVE_LINUX)
add_compile_options(-D_POSIX_C_SOURCE=200809L)

check_include_file("execinfo.h" HAVE_EXECINFO_H)
if (HAVE_EXECINFO_H)
  add_compile_options(-DHAVE_EXECINFO_H)
endif()

#
# Include directories
#
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${LIBASOUND_INCLUDE_DIRS}
  ${LIBSNDFILE_INCLUDE_DIR}
  ${OPUS_INCLUDE_DIRS}
  ${FAAD2_INCLUDE_DIRS}
)

#
# Library directories
#
link_libraries(
  ${LIBATOMIC_LIBRARY}
  ${LIBASOUND_LIBRARIES}
  ${LIBSNDFILE_LIBRARY}
  ${OPUS_LIBRARIES}
  ${FAAD2_LIBRARIES}
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")

#
# Compile options
#
add_compile_options(-g)
add_compile_options(-Wall)
add_compile_options(-std=c17)
add_compile_options(-D__USE_ISOC11)
add_compile_options(-D_GNU_SOURCE)
add_compile_options(-Wno-deprecated)
add_compile_options(-Wno-deprecated-declarations)
add_compile_options(-Wsign-compare)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wstrict-prototypes)
endif()

# Debug
if (CMAKE_BUILD_TYPE MATCHES Debug)
  add_compile_options(-O0)
  add_compile_options(-DDEBUG)

  if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fsanitize=address)

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=address")
  endif()

  check_c_compiler_flag(-fno-omit-frame-pointer HAS_NO_OMIT_FRAME_POINTER)
  if (HAS_NO_OMIT_FRAME_POINTER)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer")
  endif()
endif()

# Release
if (CMAKE_BUILD_TYPE MATCHES Release OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
  add_compile_options(-O2)
  add_compile_options(-DNDEBUG)
endif (CMAKE_BUILD_TYPE MATCHES Release OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)

check_c_compiler_flag(-Wformat HAS_FORMAT)
if (HAS_FORMAT)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wformat")
endif()

check_c_compiler_flag(-Wformat-security HAS_FORMAT_SECURITY)
if (HAS_FORMAT_SECURITY)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wformat-security")
endif()

check_c_compiler_flag(-fstack-protector-strong HAS_STACKPROTECTOR_STRONG)
if (HAS_STACKPROTECTOR_STRONG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
else()
  check_c_compiler_flag(-fstack-protector HAS_STACKPROTECTOR)
  if (HAS_STACKPROTECTOR)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
  endif()
endif()

check_c_compiler_flag(-rdynamic HAS_DYNAMIC)
if (HAS_DYNAMIC)
  set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic")
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
endif()

check_c_compiler_flag(-fPIC HAS_PIC)
if (HAS_PIC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif()

include(CheckPIESupported)
check_pie_supported()

check_c_compiler_flag(-Wl,-z,relro HAS_RELRO)
if (HAS_RELRO)
  set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,relro")
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro")
endif()

check_c_compiler_flag(-Wl,-z,now HAS_NOW)
if (HAS_NOW)
  set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,now")
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,now")
endif()

#
# Build libhrmp
#
add_library(hrmp SHARED ${SOURCES})
set_target_properties(hrmp PROPERTIES LINKER_LANGUAGE C VERSION ${VERSION_STRING}
                           SOVERSION ${VERSION_MAJOR})
target_link_libraries(hrmp PUBLIC)

install(TARGETS hrmp DESTINATION ${CMAKE_INSTALL_LIBDIR}/)

#
# Build hrmp
#
add_executable(hrmp-bin main.c ${RESOURCE_OBJECT})
if (CMAKE_C_LINK_PIE_SUPPORTED)
  set_target_properties(hrmp-bin PROPERTIES LINKER_LANGUAGE C OUTPUT_NAME hrmp POSITION_INDEPENDENT_CODE TRUE)
else()
  set_target_properties(hrmp-bin PROPERTIES LINKER_LANGUAGE C OUTPUT_NAME hrmp POSITION_INDEPENDENT_CODE FALSE)
endif()
target_link_libraries(hrmp-bin hrmp)

install(TARGETS hrmp-bin DESTINATION ${CMAKE_INSTALL_BINDIR})

#
# GTK-based UI for hrmp
#
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

set(HRMP_UI_RESOURCE_XML ${CMAKE_CURRENT_SOURCE_DIR}/hrmp-ui.gresource.xml)

add_custom_command(
  OUTPUT hrmp-ui-resources.c
  COMMAND glib-compile-resources
          --target=hrmp-ui-resources.c
          --generate-source
          --sourcedir=${CMAKE_SOURCE_DIR}
          ${HRMP_UI_RESOURCE_XML}
  DEPENDS ${HRMP_UI_RESOURCE_XML} ${CMAKE_SOURCE_DIR}/LICENSE ${CMAKE_SOURCE_DIR}/AUTHORS ${CMAKE_SOURCE_DIR}/doc/images/logo-transparent-256.png ${CMAKE_SOURCE_DIR}/doc/images/logo-transparent-16.png
)

add_executable(hrmp-ui
  ui.c
  hrmp-ui-resources.c
)

set_source_files_properties(hrmp-ui-resources.c PROPERTIES GENERATED TRUE)

target_include_directories(hrmp-ui PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_libraries(hrmp-ui PRIVATE hrmp ${GTK3_LIBRARIES})
target_compile_options(hrmp-ui PRIVATE ${GTK3_CFLAGS_OTHER})
target_compile_definitions(hrmp-ui PRIVATE HRMP_GTK_VERSION_STRING="${VERSION_STRING}")

install(TARGETS hrmp-ui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/hrmp-ui.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

foreach(size 16 24 32 48 64 96 128 256 512)
  install(FILES ${CMAKE_SOURCE_DIR}/doc/images/logo-transparent-${size}.png
          DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${size}x${size}/apps
          RENAME hrmp-ui.png)
endforeach()
