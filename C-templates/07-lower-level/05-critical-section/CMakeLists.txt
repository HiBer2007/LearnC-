# ===================================================================
#  练习模板 05 · 临界区与原子（C++，多文件，命令行）
#
#  一个目标：app_cli
#     src/main_cli.cpp   命令行验收程序，已写好
#     src/counter.cpp    阶段 1 到阶段 4 的 TODO（共享计数器与中断服务程序）
#     src/sim_irq.cpp    确定性的中断模型（素材，不要改）
#
#  命令行验证：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_cli.exe
#
#  「丢更新」这件事在真板上每次的数字都不一样，为了让练习可复现，
#  本模板把中断变成模型里两个可控的动作：request_irq() 与 poll()。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(critical_section
    VERSION 1.0.0
    DESCRIPTION "练习模板 05 · 中断与临界区：丢更新复现，三种修法与代价对照"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(app_cli
    src/main_cli.cpp
    src/counter.cpp
    src/sim_irq.cpp
)

target_include_directories(app_cli
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(app_cli PRIVATE /utf-8 /W4)
else()
    target_compile_options(app_cli PRIVATE -g -Wall -Wextra)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
