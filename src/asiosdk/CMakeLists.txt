include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(asiosdk
	URL "http://www.steinberg.net/sdk_downloads/ASIOSDK2.3.2.zip"
	URL_HASH SHA1=ab4c6562884e7a13a850978fd78396110b102aee
	USES_TERMINAL_DOWNLOAD TRUE  # https://gitlab.kitware.com/cmake/cmake/issues/18238#note_440475
)
FetchContent_GetProperties(asiosdk)
if(NOT asiosdk_POPULATED)
  FetchContent_Populate(asiosdk)
endif()

add_library(asiosdk_headers INTERFACE)
target_include_directories(asiosdk_headers INTERFACE "${asiosdk_SOURCE_DIR}")

add_compile_options(/analyze-)

add_library(asiosdk_asiolist STATIC "${asiosdk_SOURCE_DIR}/host/pc/asiolist.cpp")
target_include_directories(asiosdk_asiolist PUBLIC "${asiosdk_SOURCE_DIR}/common" "${asiosdk_SOURCE_DIR}/host/pc")
target_compile_options(asiosdk_asiolist PRIVATE /W2)

add_library(asiosdk_asiodrivers STATIC "${asiosdk_SOURCE_DIR}/host/asiodrivers.cpp")
target_include_directories(asiosdk_asiodrivers PUBLIC "${asiosdk_SOURCE_DIR}/common" "${asiosdk_SOURCE_DIR}/host")
target_link_libraries(asiosdk_asiodrivers PUBLIC asiosdk_asiolist)

add_library(asiosdk_asio STATIC "${asiosdk_SOURCE_DIR}/common/asio.cpp")
target_compile_options(asiosdk_asio PRIVATE /W2)
target_link_libraries(asiosdk_asio PRIVATE asiosdk_asiodrivers)
