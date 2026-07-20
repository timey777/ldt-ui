cmake_minimum_required(VERSION 3.10)
project(hello_world)

set(CMAKE_CXX_STANDARD 17)

# LDT 根目录
set(LDT_ROOT ${CMAKE_SOURCE_DIR})
set(3RD_DIR ${LDT_ROOT}/thirdparty)

# 源文件
file(GLOB SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# 编译选项
target_compile_options(${PROJECT_NAME} PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>"
    "$<$<CXX_COMPILER_ID:MSVC>:/execution-charset:utf-8>"
)

# LDT 头文件目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${LDT_ROOT}/src
    ${LDT_ROOT}/build/src/generated-config
)

# 第三方头文件
target_include_directories(${PROJECT_NAME} PRIVATE
    ${3RD_DIR}/glfw/include
    ${3RD_DIR}/glad/include
    ${3RD_DIR}/freetype/include
)

# 链接 LDT 库及其依赖
target_link_libraries(${PROJECT_NAME} PRIVATE ldt)
