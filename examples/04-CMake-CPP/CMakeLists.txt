# ===================================================================
#  示例 04 · CMake 工程（C++，多文件）
#
#  和示例 03 的区别：
#     · LANGUAGES 从 C 改成 CXX
#     · 标准用 CMAKE_CXX_STANDARD
#     · MinGW 预设的编译器从 gcc 改成 g++
#       （gcc 编译 .cpp 不会链接 libstdc++，会报 undefined reference）
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(app
    VERSION 1.0.0
    DESCRIPTION "CMake 多文件工程示例（C++）"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# MinGW → build/mingw/bin/app.exe
# MSVC  → build/msvc/bin/Debug/app.exe（多配置生成器自动加一层）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(app
    src/main.cpp
    src/calc.cpp
)

target_include_directories(app
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ---------------------------------------------------------------
#  字符集设置：中文输出不乱码的关键（与示例 03 相同）
#  源码按 UTF-8 读取，字符串常量按 GBK 写入 exe，
#  以匹配中文 Windows 控制台默认的 CP936。
# ---------------------------------------------------------------
if(MSVC)
    target_compile_options(app PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(app PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
