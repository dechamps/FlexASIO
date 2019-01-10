cmake_minimum_required(VERSION 3.11)
project(dechamps_cpputil LANGUAGES)

add_library(find INTERFACE)
target_include_directories(find INTERFACE "$<INSTALL_INTERFACE:include>")

add_library(string INTERFACE)
target_include_directories(string INTERFACE "$<INSTALL_INTERFACE:include>")

install(FILES find.h string.h DESTINATION include/dechamps_cpputil)
install(TARGETS find string EXPORT dechamps_cpputil-config)
install(EXPORT dechamps_cpputil-config NAMESPACE dechamps_cpputil:: DESTINATION lib/cmake/dechamps_cpputil)
