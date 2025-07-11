# man target
add_custom_target(man ALL)

# man page definitions
set(HRMP_SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/man/hrmp.1.rst")
set(HRMP_DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/hrmp.1")
set(HRMP_CONF_SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/man/hrmp.conf.5.rst")
set(HRMP_CONF_DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/hrmp.conf.5")


# hrmp.1
add_custom_command(
  OUTPUT ${HRMP_DST_FILE}
  COMMAND ${RST2MAN_EXECUTABLE} ${HRMP_SRC_FILE} ${HRMP_DST_FILE}
  DEPENDS ${HRMP_SRC_FILE}
  COMMENT "Generating man page: hrmp.1"
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
  DEPENDS ${HRMP_DST_FILE} ${HRMP_CONF_DST_FILE}
)

# Make 'man' depend on 'manpages'
add_dependencies(man manpages)

#
# Install configuration and documentation
#
install(FILES etc/hrmp.conf DESTINATION share/doc/hrmp/etc)

install(DIRECTORY . DESTINATION share/doc/hrmp FILES_MATCHING PATTERN "*.md" PATTERN "etc" EXCLUDE PATTERN "man" EXCLUDE PATTERN "manual" EXCLUDE)

install(FILES ${HRMP_DST_FILE} DESTINATION share/man/man1)
install(FILES ${HRMP_CONF_DST_FILE} DESTINATION share/man/man5)

#
# Generate manual
#
if(generation)
  # Files
  FILE(GLOB PREDS "manual/0?-*.md")
  FILE(GLOB POSTDS "manual/9?-*.md")
  FILE(GLOB TDS "tutorial/??_*.md")
  FILE(GLOB UMDS_PRE "manual/user-0?-*.md")
  FILE(GLOB UMDS_POST "manual/user-1?-*.md")
  FILE(GLOB DMDS "manual/dev-??-*.md")
  FILE(GLOB AG_EN "manual/advanced/??-*.md")

  # Manuals definitions
  set(MANUAL_OUTPUT_DIR "${CMAKE_BINARY_DIR}/doc")
  get_filename_component(PARENT_DIR ${CMAKE_BINARY_DIR} DIRECTORY)
  set(IMAGE_DIR "${PARENT_DIR}/doc/images")

  add_custom_target(
    copy_images
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${IMAGE_DIR} ${CMAKE_BINARY_DIR}/images
    COMMENT "Copy images"
  )

  add_custom_command(
    OUTPUT ${MANUAL_OUTPUT_DIR}/hrmp-user-guide.pdf
    COMMAND ${PANDOC_EXECUTABLE} -o ${MANUAL_OUTPUT_DIR}/hrmp-user-guide.pdf --from markdown --template eisvogel --listings -N --toc ${PREDS} ${UMDS_PRE} ${TDS} ${UMDS_POST} ${POSTDS}
    DEPENDS ${UMDS}
    COMMENT "Generating User Guide PDF documentation"
  )
  add_custom_command(
    OUTPUT ${MANUAL_OUTPUT_DIR}/hrmp-dev-guide.pdf
    COMMAND ${PANDOC_EXECUTABLE} -o ${MANUAL_OUTPUT_DIR}/hrmp-dev-guide.pdf --from markdown --template eisvogel --listings -N --toc ${PREDS} ${DMDS} ${POSTDS}
    DEPENDS ${DMDS}
    COMMENT "Generating Developer Guide PDF documentation"
  )

  add_custom_command(
    OUTPUT ${MANUAL_OUTPUT_DIR}/hrmp-user-guide.html
    COMMAND ${PANDOC_EXECUTABLE} -o ${MANUAL_OUTPUT_DIR}/hrmp-user-guide.html -s -f markdown-smart -N --toc -t html5 ${PREDS} ${UMDS_PRE} ${TDS} ${UMDS_POST} ${POSTDS}
    DEPENDS ${UMDS}
    COMMENT "Generating User Guide HTML documentation"
  )
  add_custom_command(
    OUTPUT ${MANUAL_OUTPUT_DIR}/hrmp-dev-guide.html
    COMMAND ${PANDOC_EXECUTABLE} -o ${MANUAL_OUTPUT_DIR}/hrmp-dev-guide.html -s -f markdown-smart -N --toc -t html5 ${PREDS} ${DMDS} ${POSTDS}
    DEPENDS ${DMDS}
    COMMENT "Generating Developer Guide HTML documentation"
  )

  add_custom_command(
    OUTPUT ${MANUAL_OUTPUT_DIR}/hrmp-advanced.pdf
    COMMAND ${PANDOC_EXECUTABLE} -o ${MANUAL_OUTPUT_DIR}/hrmp-advanced.pdf --from markdown --template eisvogel --listings -N --toc ${AG_EN}
    DEPENDS ${AG_EN}
    COMMENT "Generating Advanced PDF documentation"
  )

  add_custom_command(
    OUTPUT ${MANUAL_OUTPUT_DIR}/hrmp-advanced.html
    COMMAND ${PANDOC_EXECUTABLE} -o ${MANUAL_OUTPUT_DIR}/hrmp-advanced.html -s -f markdown-smart -N --toc -t html5 ${AG_EN}
    DEPENDS ${AG_EN}
    COMMENT "Generating Advanced HTML documentation"
  )

  add_custom_target(
    all_docs ALL
    DEPENDS copy_images ${MANUAL_OUTPUT_DIR}/hrmp-user-guide.pdf ${MANUAL_OUTPUT_DIR}/hrmp-user-guide.html ${MANUAL_OUTPUT_DIR}/hrmp-dev-guide.pdf ${MANUAL_OUTPUT_DIR}/hrmp-dev-guide.html ${MANUAL_OUTPUT_DIR}/hrmp-advanced.pdf ${MANUAL_OUTPUT_DIR}/hrmp-advanced.html
  )

  install(FILES ${MANUAL_OUTPUT_DIR}/hrmp-user-guide.pdf DESTINATION share/doc/hrmp/manual)
  install(FILES ${MANUAL_OUTPUT_DIR}/hrmp-user-guide.html DESTINATION share/doc/hrmp/manual)
  install(FILES ${MANUAL_OUTPUT_DIR}/hrmp-dev-guide.pdf DESTINATION share/doc/hrmp/manual)
  install(FILES ${MANUAL_OUTPUT_DIR}/hrmp-dev-guide.html DESTINATION share/doc/hrmp/manual)
  install(FILES ${MANUAL_OUTPUT_DIR}/hrmp-advanced.pdf DESTINATION share/doc/hrmp/manual)
  install(FILES ${MANUAL_OUTPUT_DIR}/hrmp-advanced.html DESTINATION share/doc/hrmp/manual)
endif()

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
      ${DOXYGEN_VERSION} STREQUAL "1.13.2")
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
