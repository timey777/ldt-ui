cmake_minimum_required(VERSION 3.10)
set(PROJECT_NAME ldt)
set(${PROJECT_NAME}_VERSION_MAJOR 1)
set(${PROJECT_NAME}_VERSION_MINOR 0)

set(CMAKE_CXX_STANDARD 17)

get_filename_component(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(3RD_DIR "${ROOT_DIR}/thirdparty")
set(BUILD_DIR "${ROOT_DIR}/build")
set(CMAKE_MODULE_PATH "${3RD_DIR}")

option(LDT_BUILD_SHARED "Build ldt as a shared library" OFF)
option(LDT_LINK_DEPENDENCIES_STATIC "Force third-party dependencies to build/link statically into ldt" ON)
option(LDT_PLATFORM_ANDROID "Build ldt for Android" OFF)

message("ROOT_DIR=${ROOT_DIR}")
message("CMAKE_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}")

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/execution-charset:utf-8>")

set(LDT_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated-config")
file(MAKE_DIRECTORY "${LDT_CONFIG_DIR}")
configure_file(
    "${ROOT_DIR}/configuration/root_directory.h.in"
    "${LDT_CONFIG_DIR}/root_directory.h"
    @ONLY
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${LDT_CONFIG_DIR})

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)
if(COMMAND source_group_by_dir)
    source_group_by_dir(SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR})
endif()

set(LDT_DESKTOP_ONLY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/application.h
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/Window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/Window.h
)

if(LDT_PLATFORM_ANDROID)
    list(REMOVE_ITEM SOURCE_FILES ${LDT_DESKTOP_ONLY_SOURCES})
endif()

if(LDT_BUILD_SHARED)
    set(LDT_LIB_TYPE SHARED)
else()
    set(LDT_LIB_TYPE STATIC)
endif()

add_library(${PROJECT_NAME} ${LDT_LIB_TYPE} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LDT_CONFIG_DIR}
)

if(NOT LDT_PLATFORM_ANDROID)
    set(GLFW_DIR "${3RD_DIR}/glfw")
    target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW_DIR}/include)
    if(NOT TARGET glfw)
        add_subdirectory(${GLFW_DIR} ${CMAKE_BINARY_DIR}/thirdparty_glfw)
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
endif()

if(LDT_ENABLE_RENDERING)
    if(NOT LDT_PLATFORM_ANDROID)
        set(GLAD_DIR "${3RD_DIR}/glad")
        target_include_directories(${PROJECT_NAME} PRIVATE ${GLAD_DIR}/include)
        if(NOT TARGET glad)
            add_library(glad ${GLAD_DIR}/src/glad.c)
        endif()
        target_link_libraries(${PROJECT_NAME} PRIVATE glad)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE GLESv3 EGL)
    endif()

    set(FREETYPE_DIR "${3RD_DIR}/freetype")
    target_include_directories(${PROJECT_NAME} PRIVATE ${FREETYPE_DIR}/include)
    if(NOT TARGET freetype)
        add_subdirectory(${FREETYPE_DIR} ${CMAKE_BINARY_DIR}/thirdparty_freetype)
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE freetype)

    target_compile_definitions(${PROJECT_NAME} PRIVATE LDT_ENABLE_RENDERING)
endif()

if(LDT_BUILD_SHARED)
    target_compile_definitions(${PROJECT_NAME} PRIVATE LDT_EXPORTS)
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC LDT_STATIC)
endif()


