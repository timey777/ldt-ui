# cmake最低版本号要求
cmake_minimum_required (VERSION 3.10)
set(PROJECT_NAME examples)
set (${PROJECT_NAME}_VERSION_MAJOR 1)
set (${PROJECT_NAME}_VERSION_MINOR 0)

set (CMAKE_CXX_STANDARD 17)

set (ROOT_DIR ${CMAKE_SOURCE_DIR})
set (3RD_DIR ${ROOT_DIR}/thirdparty)
set (BUILD_DIR ${ROOT_DIR}/build)
set (CMAKE_MODULE_PATH ${3RD_DIR})

message("CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}")
message("CMAKE_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}")
# 设置MSVC编译编码（源文件编码 + 执行字符集编码，确保字符串字面量 UTF-8 一致）
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/execution-charset:utf-8>")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
file (GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
source_group_by_dir(SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
# glad
set (GLAD_DIR ${3RD_DIR}/glad)
target_include_directories (${PROJECT_NAME} PRIVATE ${GLAD_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE glad)
# ldt
set (LDT_DIR ${ROOT_DIR}/src)
target_include_directories(${PROJECT_NAME} PRIVATE ${LDT_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ldt)