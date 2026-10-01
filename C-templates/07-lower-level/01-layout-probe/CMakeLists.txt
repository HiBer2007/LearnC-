# ===================================================================
#  练习模板 01 · 布局探针（C，多文件，命令行）
#
#  一个目标：app_cli
#     src/main_cli.c       命令行验收程序，已写好
#     src/layout_probe.c   阶段 1、2 的 TODO
#     src/shapes.c         阶段 3、4 的 TODO
#     src/probe_data.c     探测对象（素材，不要改）
#
#  命令行验证：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_cli.exe
#
#  阶段 2 与阶段 4 要看两次运行之间的差别，因此这个程序可以反复运行。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(layout_probe
    VERSION 1.0.0
    DESCRIPTION "练习模板 01 · 布局探针：各段地址、段间距、结构体布局、对齐自检"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 单配置生成器（Ninja）没指定构建类型时默认走 Debug
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# 产物统一放到 <build>/bin 下
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(app_cli
    src/main_cli.c
    src/layout_probe.c
    src/shapes.c
    src/probe_data.c
)

target_include_directories(app_cli
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# MSVC 与 GCC 两条路线都要零警告：
# MSVC 加 /utf-8（源码是 UTF-8 的，注释里有中文），GCC 加 -Wall -Wextra。
if(MSVC)
    target_compile_options(app_cli PRIVATE /utf-8 /W4)
else()
    target_compile_options(app_cli PRIVATE -g -Wall -Wextra)
endif()

# 给智能感知生成精确编译参数（只有 Ninja / Makefile 生成器会产出）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
