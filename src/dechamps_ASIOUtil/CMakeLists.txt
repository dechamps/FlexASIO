cmake_minimum_required(VERSION 3.11)
project(dechamps_ASIOUtil)

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

add_library(asiosdk_ginclude INTERFACE)
target_include_directories(asiosdk_ginclude INTERFACE "$<INSTALL_INTERFACE:include>")

add_library(asiosdk_asioh INTERFACE)
target_include_directories(asiosdk_asioh INTERFACE "$<INSTALL_INTERFACE:include>")

add_library(asiosdk_asiosys INTERFACE)
target_include_directories(asiosdk_asiosys INTERFACE "$<INSTALL_INTERFACE:include>")

add_library(asiosdk_iasiodrv INTERFACE)
target_include_directories(asiosdk_iasiodrv INTERFACE "$<INSTALL_INTERFACE:include>")

add_library(asiosdk_asiolist STATIC "${asiosdk_SOURCE_DIR}/host/pc/asiolist.cpp")
target_include_directories(asiosdk_asiolist
	PUBLIC "$<BUILD_INTERFACE:${asiosdk_SOURCE_DIR}/host/pc>"
	PRIVATE "$<BUILD_INTERFACE:${asiosdk_SOURCE_DIR}/common>"
	INTERFACE "$<INSTALL_INTERFACE:include>"
)

add_library(asiosdk_asiodrivers STATIC "${asiosdk_SOURCE_DIR}/host/asiodrivers.cpp")
target_link_libraries(asiosdk_asiodrivers PRIVATE asiosdk_asiolist)
target_include_directories(asiosdk_asiodrivers
	PRIVATE "$<BUILD_INTERFACE:${asiosdk_SOURCE_DIR}/common>"
	INTERFACE "$<INSTALL_INTERFACE:include>"
)

add_library(asiosdk_asio STATIC "${asiosdk_SOURCE_DIR}/common/asio.cpp")
target_link_libraries(asiosdk_asio PRIVATE asiosdk_asiodrivers)
target_include_directories(asiosdk_asio
	PRIVATE "$<BUILD_INTERFACE:${asiosdk_SOURCE_DIR}/host>"
	PRIVATE "$<BUILD_INTERFACE:${asiosdk_SOURCE_DIR}/host/pc>"
	INTERFACE "$<INSTALL_INTERFACE:include>"
)

install(FILES "${asiosdk_SOURCE_DIR}/host/ginclude.h" "${asiosdk_SOURCE_DIR}/common/asio.h" "${asiosdk_SOURCE_DIR}/common/asiosys.h" "${asiosdk_SOURCE_DIR}/common/iasiodrv.h" DESTINATION include/dechamps_ASIOUtil/asiosdk)
install(TARGETS asiosdk_ginclude asiosdk_asioh asiosdk_asiosys asiosdk_iasiodrv asiosdk_asiolist asiosdk_asiodrivers asiosdk_asio EXPORT dechamps_ASIOUtil-config ARCHIVE DESTINATION lib/dechamps_ASIOUtil)
install(EXPORT dechamps_ASIOUtil-config NAMESPACE dechamps_ASIOUtil:: DESTINATION lib/cmake/dechamps_ASIOUtil)
