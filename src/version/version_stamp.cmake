include("${CMAKE_CURRENT_LIST_DIR}/version.cmake")
configure_file("${CMAKE_CURRENT_LIST_DIR}/version_stamp.in.h" "${OUTPUT_HEADER_FILE}" @ONLY ESCAPE_QUOTES)
