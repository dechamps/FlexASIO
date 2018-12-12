set(ASIOSDK ../ASIOSDK2.3.2)
add_library(ASIOHost STATIC "${ASIOSDK}/common/asio.cpp" "${ASIOSDK}/host/asiodrivers.cpp" "${ASIOSDK}/host/pc/asiolist.cpp")
target_include_directories(ASIOHost PRIVATE "${ASIOSDK}/common" "${ASIOSDK}/host" "${ASIOSDK}/host/pc")
