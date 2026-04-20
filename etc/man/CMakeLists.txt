# Uses either a2x from asciidoc or asciidoctor to compile the man page.

include(GNUInstallDirs)

# Use the last modification time of the man page source so the rendered date
# reflects the current content instead of a hand-maintained constant.
file(TIMESTAMP "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.1.adoc"
     MAN_DATE "%Y-%m-%d" UTC)

configure_file("${PROJECT_NAME}.1.adoc" "${PROJECT_NAME}.1.adoc" @ONLY)

find_program (ASCIIDOC_COMPILER a2x)
if (NOT ASCIIDOC_COMPILER)
  find_program (ASCIIDOCTOR_COMPILER asciidoctor)
  if(NOT ASCIIDOCTOR_COMPILER)
    message(WARNING
      "Could not find asciidoc (a2x) or asciidoctor. "
      "Skipping man page generation. "
      "Install asciidoctor (or asciidoc) to build the man page.")
    return()
  endif()
endif()

if(ASCIIDOC_COMPILER)
  message(STATUS "Using asciidoc compiler at ${ASCIIDOC_COMPILER}")

  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.1"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS "${PROJECT_NAME}.1.adoc"
    COMMAND "${ASCIIDOC_COMPILER}"
    ARGS
    "--doctype=manpage"
    "--format=manpage"
    "--no-xmllint"
    "${PROJECT_NAME}.1.adoc")
endif()

if(ASCIIDOCTOR_COMPILER)
  message(STATUS "Using asciidoctor compiler at ${ASCIIDOCTOR_COMPILER}")

  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.1"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS "${PROJECT_NAME}.1.adoc"
    COMMAND "${ASCIIDOCTOR_COMPILER}"
    ARGS "--backend=manpage" "--doctype=manpage" "${PROJECT_NAME}.1.adoc")
endif()

add_custom_target(man ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.1")

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
