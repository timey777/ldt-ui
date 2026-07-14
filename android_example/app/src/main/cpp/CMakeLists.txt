cmake_minimum_required(VERSION 3.22.1)
project(PureOpenGL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

get_filename_component(LDT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../" ABSOLUTE)

set(LDT_PLATFORM_ANDROID ON CACHE BOOL "" FORCE)
set(LDT_ENABLE_RENDERING ON CACHE BOOL "" FORCE)
set(LDT_BUILD_SHARED OFF CACHE BOOL "" FORCE)

add_subdirectory("${LDT_ROOT}/src" "${CMAKE_BINARY_DIR}/ldt")

add_library(native-lib SHARED
    android_dsl_scene.cpp
    native-lib.cpp
)

target_include_directories(native-lib PRIVATE
    "${LDT_ROOT}/src"
)

target_link_libraries(native-lib
    ldt
    android
    log
    EGL
    GLESv3
)
