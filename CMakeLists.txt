cmake_minimum_required(VERSION 3.20)

project(cppresult VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(cppresult INTERFACE)
add_library(cppresult::cppresult ALIAS cppresult)

target_compile_features(cppresult INTERFACE cxx_std_20)
target_include_directories(cppresult INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(TARGETS cppresult EXPORT cppresultTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT cppresultTargets
    FILE cppresultTargets.cmake
    NAMESPACE cppresult::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppresult
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cppresultConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cppresultConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppresult
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cppresultConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cppresultConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cppresultConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppresult
)

option(CPPRESULT_BUILD_TESTS "Build cppresult tests" ON)

if(CPPRESULT_BUILD_TESTS)
    enable_testing()
    add_executable(cppresult_tests tests/result_test.cpp)
    target_link_libraries(cppresult_tests PRIVATE cppresult)
    add_test(NAME cppresult_tests COMMAND cppresult_tests)
endif()
