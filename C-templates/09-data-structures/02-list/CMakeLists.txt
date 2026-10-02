# ===================================================================
#  练习模板 02 · 手写链表（C++17，多文件）
#
#  目标：
#     core           节点与元素的计数器（src/list_stats.cpp），不含界面代码
#     app_cli        命令行验收程序，4 个阶段的结果由它打印
#
#  链表本身是类模板，实现与 4 处 TODO 都在 include/list.hpp 里，
#  由 app_cli 直接引入，因此不单独编译成库。
#
#  命令行验证：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_cli.exe
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(ds02_list
    VERSION 1.0.0
    DESCRIPTION "练习模板 02 · 手写链表：哨兵、插入删除的指针改动与析构顺序"
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

# ---------------------------------------------------------------- 计数器
add_library(core STATIC
    src/list_stats.cpp
)

target_include_directories(core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ---------------------------------------------------------------- 命令行版
add_executable(app_cli
    src/main_cli.cpp
)

target_link_libraries(app_cli PRIVATE core)

# ---------------------------------------------------------------- 编译选项
foreach(target core app_cli)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4)
    else()
        target_compile_options(${target} PRIVATE -g -Wall -Wextra)
    endif()
endforeach()
