cmake_minimum_required(VERSION 3.15)
project(smsgora VERSION 1.0.0 LANGUAGES C
        DESCRIPTION "SMS Gora SMS Gateway client for C and C++"
        HOMEPAGE_URL "https://tekservix.com/smsgora/docs/")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(SMSGORA_TOP_LEVEL ON)
else()
  set(SMSGORA_TOP_LEVEL OFF)
endif()
option(SMSGORA_BUILD_TESTS "Build the tests" ${SMSGORA_TOP_LEVEL})
option(SMSGORA_BUILD_EXAMPLES "Build the examples" OFF)

find_package(CURL REQUIRED)

add_library(smsgora src/smsgora.c src/sha256.c)
add_library(smsgora::smsgora ALIAS smsgora)
target_include_directories(smsgora PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(smsgora PRIVATE CURL::libcurl)
set_target_properties(smsgora PROPERTIES
  C_STANDARD 99
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  C_VISIBILITY_PRESET hidden)
if(BUILD_SHARED_LIBS)
  target_compile_definitions(smsgora PUBLIC SMSGORA_SHARED)
endif()
if(MSVC)
  target_compile_options(smsgora PRIVATE /W4)
  target_compile_definitions(smsgora PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(smsgora PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS smsgora EXPORT smsgora-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT smsgora-targets NAMESPACE smsgora::
  DESTINATION ${CMAKE_INSTALL_DATADIR}/smsgora)
configure_package_config_file(cmake/smsgora-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/smsgora-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/smsgora)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/smsgora-config-version.cmake
  COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/smsgora-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/smsgora-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_DATADIR}/smsgora)
configure_file(cmake/smsgora.pc.in ${CMAKE_CURRENT_BINARY_DIR}/smsgora.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/smsgora.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(SMSGORA_BUILD_TESTS)
  enable_testing()
  add_executable(smsgora_tests tests/test_smsgora.c)
  target_link_libraries(smsgora_tests PRIVATE smsgora::smsgora)
  add_test(NAME smsgora_tests COMMAND smsgora_tests)
endif()

if(SMSGORA_BUILD_EXAMPLES)
  add_executable(smsgora_send examples/send.c)
  target_link_libraries(smsgora_send PRIVATE smsgora::smsgora)
endif()
