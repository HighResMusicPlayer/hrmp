if (RST2MAN_FOUND)
  # man target
  add_custom_target(man ALL)

  # man page definitions
  set(HRMP_SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/man/hrmp.1.rst")
  set(HRMP_DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/hrmp.1")
  set(HRMP_UI_SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/man/hrmp-ui.1.rst")
  set(HRMP_UI_DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/hrmp-ui.1")
  set(HRMP_CONF_SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/man/hrmp.conf.5.rst")
  set(HRMP_CONF_DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/hrmp.conf.5")

  # hrmp.1
  add_custom_command(
    OUTPUT ${HRMP_DST_FILE}
    COMMAND ${RST2MAN_EXECUTABLE} ${HRMP_SRC_FILE} ${HRMP_DST_FILE}
    DEPENDS ${HRMP_SRC_FILE}
    COMMENT "Generating man page: hrmp.1"
  )

  # hrmp-ui.1
  add_custom_command(
    OUTPUT ${HRMP_UI_DST_FILE}
    COMMAND ${RST2MAN_EXECUTABLE} ${HRMP_UI_SRC_FILE} ${HRMP_UI_DST_FILE}
    DEPENDS ${HRMP_UI_SRC_FILE}
    COMMENT "Generating man page: hrmp-ui.1"
  )

  # hrmp.conf.5
  add_custom_command(
    OUTPUT ${HRMP_CONF_DST_FILE}
    COMMAND ${RST2MAN_EXECUTABLE} ${HRMP_CONF_SRC_FILE} ${HRMP_CONF_DST_FILE}
    DEPENDS ${HRMP_CONF_SRC_FILE}
    COMMENT "Generating man page: hrmp.conf.5"
  )

  # Group man page outputs into a target
  add_custom_target(manpages ALL
    DEPENDS ${HRMP_DST_FILE} ${HRMP_UI_DST_FILE} ${HRMP_CONF_DST_FILE}
  )

  # Make 'man' depend on 'manpages'
  add_dependencies(man manpages)

  #
  # Install configuration and documentation
  #
  install(DIRECTORY . DESTINATION share/doc/hrmp FILES_MATCHING PATTERN "*.md" PATTERN "etc" EXCLUDE PATTERN "man" EXCLUDE PATTERN "manual" EXCLUDE)

  install(FILES ${HRMP_DST_FILE} DESTINATION share/man/man1)
  install(FILES ${HRMP_UI_DST_FILE} DESTINATION share/man/man1)
  install(FILES ${HRMP_CONF_DST_FILE} DESTINATION share/man/man5)
endif()

if (DOCS)
  #
  # Generate manual
  #

  # Files
  FILE(GLOB MAN_EN "manual/??-*.md")
  FILE(GLOB MAN_IMAGES "manual/images/*")

  # Manuals definitions
  set(MANUAL_OUTPUT_DIR "${CMAKE_BINARY_DIR}/doc")
  set(MANUAL_BUILD_IMAGES_DIR "${MANUAL_OUTPUT_DIR}/manual/images")
  get_filename_component(PARENT_DIR ${CMAKE_BINARY_DIR} DIRECTORY)

  set(MANUAL_TITLE_LOGO_PNG "${MANUAL_BUILD_IMAGES_DIR}/logo-transparent-512.png")

  add_custom_command(
    OUTPUT ${MANUAL_TITLE_LOGO_PNG}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MANUAL_BUILD_IMAGES_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/doc/images/logo-transparent-512.png ${MANUAL_TITLE_LOGO_PNG}
    DEPENDS ${CMAKE_SOURCE_DIR}/doc/images/logo-transparent-512.png
    COMMENT "Copying manual logo"
  )

  add_custom_command(
    OUTPUT ${MANUAL_OUTPUT_DIR}/hrmp-en.pdf
    COMMAND ${PANDOC_EXECUTABLE} -o ${MANUAL_OUTPUT_DIR}/hrmp-en.pdf --from markdown --template eisvogel --listings -N --toc --resource-path=${CMAKE_CURRENT_SOURCE_DIR}/manual -V titlepage-logo=${MANUAL_TITLE_LOGO_PNG} -V logo-width=120mm ${MAN_EN}
    DEPENDS ${MAN_EN} ${MAN_IMAGES} ${MANUAL_TITLE_LOGO_PNG}
    COMMENT "Generating English PDF documentation"
  )

  add_custom_command(
    OUTPUT ${MANUAL_OUTPUT_DIR}/hrmp-en.html
    COMMAND ${PANDOC_EXECUTABLE} -o ${MANUAL_OUTPUT_DIR}/hrmp-en.html -s -f markdown-smart -N --toc -t html5 --resource-path=${CMAKE_CURRENT_SOURCE_DIR}/manual ${MAN_EN}
    DEPENDS ${MAN_EN} ${MAN_IMAGES}
    COMMENT "Generating English HTML documentation"
  )

  add_custom_target(
    all_docs ALL
    DEPENDS ${MANUAL_OUTPUT_DIR}/hrmp-en.pdf ${MANUAL_OUTPUT_DIR}/hrmp-en.html
  )

  install(FILES ${MANUAL_OUTPUT_DIR}/hrmp-en.pdf DESTINATION share/doc/hrmp/manual)
  install(FILES ${MANUAL_OUTPUT_DIR}/hrmp-en.html DESTINATION share/doc/hrmp/manual)
  install(DIRECTORY ${MANUAL_BUILD_IMAGES_DIR} DESTINATION share/doc/hrmp/manual)

  #
  # Generate API docs
  #
  if (DOXYGEN_FOUND)
    add_custom_target(api ALL)

    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_MAN NO)
    set(DOXYGEN_WARN_AS_ERROR FAIL_ON_WARNINGS)
    set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.doxygen)

    doxygen_add_docs(doxygen ${PROJECT_SOURCE_DIR}/src/include/)

    if (${DOXYGEN_VERSION} STREQUAL "1.10.0" OR
        ${DOXYGEN_VERSION} STREQUAL "1.11.0" OR
        ${DOXYGEN_VERSION} STREQUAL "1.12.0" OR
        ${DOXYGEN_VERSION} STREQUAL "1.13.0" OR
        ${DOXYGEN_VERSION} STREQUAL "1.13.1" OR
        ${DOXYGEN_VERSION} STREQUAL "1.13.2" OR
        ${DOXYGEN_VERSION} STREQUAL "1.14.0" OR
        ${DOXYGEN_VERSION} STREQUAL "1.15.0" OR
        ${DOXYGEN_VERSION} STREQUAL "1.16.0" OR
        ${DOXYGEN_VERSION} STREQUAL "1.16.1")
      add_custom_command(
        TARGET api
        COMMAND ${DOXYGEN_EXECUTABLE} -q ${DOXYFILE_OUT}
        COMMENT "Generating API documentation"
        POST_BUILD
      )
    else()
      add_custom_command(
        TARGET api
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
        COMMENT "Generating API documentation"
        POST_BUILD
      )
    endif()
  endif()
endif()
