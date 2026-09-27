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
option(CPPRESULT_BUILD_LOGSTAT "Build the logstat example CLI" ON)

if(CPPRESULT_BUILD_LOGSTAT)
    find_package(Threads REQUIRED)

    add_library(logstat_core STATIC
        src/string_utils.cpp
        src/line_parser.cpp
        src/stats.cpp
        src/file_reader.cpp
        src/worker_pool.cpp
        src/report.cpp
        src/config.cpp
        src/cli.cpp
    )
    target_compile_features(logstat_core PUBLIC cxx_std_20)
    target_include_directories(logstat_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(logstat_core PUBLIC cppresult Threads::Threads)

    add_executable(logstat src/main.cpp)
    target_link_libraries(logstat PRIVATE logstat_core)
endif()

if(CPPRESULT_BUILD_TESTS)
    enable_testing()
    add_executable(cppresult_tests tests/result_test.cpp)
    target_link_libraries(cppresult_tests PRIVATE cppresult)
    add_test(NAME cppresult_tests COMMAND cppresult_tests)

    if(CPPRESULT_BUILD_LOGSTAT)
        add_executable(logstat_tests tests/logstat_test.cpp)
        target_link_libraries(logstat_tests PRIVATE logstat_core)
        add_test(NAME logstat_tests COMMAND logstat_tests)
        add_test(NAME logstat_sample
            COMMAND logstat --format json --threads 2 ${CMAKE_CURRENT_SOURCE_DIR}/examples/sample.log)
    endif()
endif()
