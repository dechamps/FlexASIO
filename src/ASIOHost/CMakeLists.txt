set(ASIOSDK ../ASIOSDK2.3.2)
include_directories("${ASIOSDK}/common")

add_library(ASIOHost_asiolist STATIC "${ASIOSDK}/host/pc/asiolist.cpp")
target_include_directories(ASIOHost_asiolist PUBLIC "${ASIOSDK}/host/pc")

add_library(ASIOHost_asiodrivers STATIC "${ASIOSDK}/host/asiodrivers.cpp")
target_include_directories(ASIOHost_asiodrivers PUBLIC "${ASIOSDK}/host")
target_link_libraries(ASIOHost_asiodrivers PUBLIC ASIOHost_asiolist)

add_library(ASIOHost_asio STATIC "${ASIOSDK}/common/asio.cpp")
target_link_libraries(ASIOHost_asio PRIVATE ASIOHost_asiodrivers)
