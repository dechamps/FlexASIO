set(ASIOSDK ../ASIOSDK2.3.2)
include_directories("${ASIOSDK}/common")
add_compile_options(/analyze-)

add_library(ASIOHost_asiolist STATIC "${ASIOSDK}/host/pc/asiolist.cpp")
target_include_directories(ASIOHost_asiolist PUBLIC "${ASIOSDK}/host/pc")
target_compile_options(ASIOHost_asiolist PRIVATE /W2)

add_library(ASIOHost_asiodrivers STATIC "${ASIOSDK}/host/asiodrivers.cpp")
target_include_directories(ASIOHost_asiodrivers PUBLIC "${ASIOSDK}/host")
target_link_libraries(ASIOHost_asiodrivers PUBLIC ASIOHost_asiolist)

add_library(ASIOHost_asio STATIC "${ASIOSDK}/common/asio.cpp")
target_compile_options(ASIOHost_asio PRIVATE /W2)
target_link_libraries(ASIOHost_asio PRIVATE ASIOHost_asiodrivers)
