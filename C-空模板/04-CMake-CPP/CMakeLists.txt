# ===================================================================
#  空模板 04 · CMake 工程（C++，多文件）
#
#  与模板 03（C 版）的区别：
#     LANGUAGES C → CXX
#     CMAKE_C_STANDARD → CMAKE_CXX_STANDARD
#     编译器 gcc → g++
#
#  先用命令行验证：
#      cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
#      cmake --build build
#
#  然后照《配置步骤.md》补上预设和 VS Code 配置。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(app
    VERSION 1.0.0
    DESCRIPTION "空模板 · CMake 多文件工程（C++）"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(app
    src/main.cpp
    src/util.cpp
)

target_include_directories(app
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(app PRIVATE /utf-8 /W4)
else()
    target_compile_options(app PRIVATE -g -Wall -Wextra)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
