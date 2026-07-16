cmake_minimum_required(VERSION 3.10)
project(ldt_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Include paths ──
set(ROOT_DIR ${CMAKE_SOURCE_DIR})
set(3RD_DIR ${ROOT_DIR}/thirdparty)
set(BUILD_DIR ${ROOT_DIR}/build)
set(LDT_SRC_DIR ${ROOT_DIR}/src)

# MSVC UTF-8
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/execution-charset:utf-8>")

# ── Include ldt headers ──
include_directories(${LDT_SRC_DIR})

# Need the generated root_directory.h
set(LDT_CONFIG_DIR "${CMAKE_BINARY_DIR}/generated-config")
file(MAKE_DIRECTORY "${LDT_CONFIG_DIR}")
configure_file(
    "${ROOT_DIR}/configuration/root_directory.h.in"
    "${LDT_CONFIG_DIR}/root_directory.h"
    @ONLY
)
include_directories(${LDT_CONFIG_DIR})

# ── Third-party includes (needed by ldt headers) ──
if(WIN32)
    set(GLFW_DIR "${3RD_DIR}/glfw")
    include_directories(${GLFW_DIR}/include)
endif()
set(GLAD_DIR "${3RD_DIR}/glad")
include_directories(${GLAD_DIR}/include)
set(FREETYPE_DIR "${3RD_DIR}/freetype")
include_directories(${FREETYPE_DIR}/include)
# nlohmann/json (header-only)
include_directories(${3RD_DIR})

# ── Header-only test framework ──
add_library(ldt_test_framework INTERFACE)
target_include_directories(ldt_test_framework INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/ldt_test_framework
)

# ── Collect all unit test sources from cases/unit/ ──
file(GLOB_RECURSE TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cases/unit/*.cpp
)

# ── Copy golden test data to build directory ──
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cases/golden DESTINATION ${CMAKE_BINARY_DIR}/cases/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cases/README.md DESTINATION ${CMAKE_BINARY_DIR}/cases/)

# ── Test runner ──
add_executable(ldt_test_runner
    ${CMAKE_CURRENT_SOURCE_DIR}/ldt_test_runner/main.cpp
    ${TEST_SOURCES}
)

target_link_libraries(ldt_test_runner PRIVATE
    ldt_test_framework
    ldt
)

# ── Custom target to run tests ──
add_custom_target(run_tests
    COMMAND ldt_test_runner
    DEPENDS ldt_test_runner
    USES_TERMINAL
)
