# ===================================================================
#  练习模板 04 · 手写平衡树（红黑树，C++17）
#
#  目标：
#     core          接口库（INTERFACE）：只带一个 include 目录
#     app_cli       命令行验收程序，4 个阶段的结果由它打印
#
#  整个红黑树是类模板，声明、实现与 4 处 TODO 全在 include/rb_tree.hpp 里，
#  没有需要单独编译的源文件，因此 core 用 INTERFACE 库表示。
#
#  命令行验证：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_cli.exe
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(ds04_rb_tree
    VERSION 1.0.0
    DESCRIPTION "练习模板 04 · 手写红黑树：插入修复的三种情形与不变式校验器"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 单配置生成器（Ninja）没指定构建类型时默认走 Debug
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# 产物统一放到 <build>/bin 下
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 给智能感知生成精确编译参数（只有 Ninja / Makefile 生成器会产出）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------- 接口库
add_library(core INTERFACE)

target_include_directories(core
    INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ---------------------------------------------------------------- 命令行版
add_executable(app_cli
    src/main_cli.cpp
)

target_link_libraries(app_cli PRIVATE core)

# ---------------------------------------------------------------- 编译选项
if(MSVC)
    target_compile_options(app_cli PRIVATE /utf-8 /W4)
else()
    target_compile_options(app_cli PRIVATE -g -Wall -Wextra)
endif()
