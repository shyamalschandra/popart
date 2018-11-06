cmake_minimum_required(VERSION 3.4.2)

set(BUILD_DOCS OFF CACHE BOOL "Build documentation")

if (BUILD_DOCS)
  find_package(gcdoc REQUIRED)
  set(LIBS willow)

  foreach(lib ${LIBS})
    get_target_property(LIB_INCLUDE_DIRS ${lib} INTERFACE_INCLUDE_DIRECTORIES)
    set(DOXYGEN_INPUT0 ${DOXYGEN_INPUT0} ${LIB_INCLUDE_DIRS}/${lib})
  endforeach(lib)

  list(REMOVE_DUPLICATES DOXYGEN_INPUT0)
  string (REPLACE ";" " " DOXYGEN_INPUT "${DOXYGEN_INPUT0}")
  configure_file(user_guide/Doxyfile.in Doxyfile @ONLY)

  set(RELEASE "1.0.0")

  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/docs
    COMMAND cmake -E make_directory ${CMAKE_BINARY_DIR}/docs
    COMMENT "Creating build folder for 'docs'"
  )

  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/docs/latex
    COMMAND cmake -E make_directory ${CMAKE_BINARY_DIR}/docs/latex
    COMMENT "Creating build folder for 'docs/latex'"
    DEPENDS ${CMAKE_BINARY_DIR}/docs
  )

  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/docs/pdfgen
    COMMAND ${GCDOC} -v -x ${CMAKE_BINARY_DIR}/docs/Doxyfile -r ${RELEASE} -b pdf user_guide ${CMAKE_BINARY_DIR}/docs/latex/poponnx.pdf
    COMMENT "Building PDF documentation from 'user_guide' directory"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_BINARY_DIR}/docs/latex ${CMAKE_BINARY_DIR}/docs/Doxyfile
  )

  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/docs/html
    COMMAND ${GCDOC} -v -x ${CMAKE_BINARY_DIR}/docs/Doxyfile -r ${RELEASE} -b html user_guide ${CMAKE_BINARY_DIR}/docs/html
    COMMENT "Building html documentation from 'user_guide' directory"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_BINARY_DIR}/docs ${CMAKE_BINARY_DIR}/docs/Doxyfile
  )

  add_custom_target(html_zip ALL
    COMMAND zip -r ${CMAKE_BINARY_DIR}/${USER_GUIDE_HTML_NAME} . -i *
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/docs/html
    VERBATIM
    DEPENDS ${CMAKE_BINARY_DIR}/docs/html
  )

  add_custom_target(pdf_copy ALL
    COMMAND cp ${CMAKE_BINARY_DIR}/docs/latex/poponnx.pdf ${CMAKE_BINARY_DIR}/${USER_GUIDE_PDF_NAME}
    DEPENDS ${CMAKE_BINARY_DIR}/docs/pdfgen
  )

  install(DIRECTORY ${CMAKE_BINARY_DIR}/docs/html DESTINATION docs
          COMPONENT poponnx-docs)

  install(FILES ${CMAKE_BINARY_DIR}/docs/latex/poponnx.pdf DESTINATION docs/pdf
          COMPONENT poponnx-docs)

  install(DIRECTORY ${CMAKE_BINARY_DIR}/docs/html/doxygen/html
          DESTINATION docs/doxygen
          COMPONENT poponnx-docs)

  add_custom_target(docs_user_guide ALL
                    DEPENDS ${CMAKE_BINARY_DIR}/docs/pdfgen
                    ${GEN_FILES} ${CMAKE_BINARY_DIR}/docs/html)

else()
  install(FILES)
endif()
