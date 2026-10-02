# ===================================================================
#  练习模板 06 · 怎么选容器与结构（C++17，多文件）
#
#  目标：
#     core           题面与选型表（src/choices.cpp），六段需求已给出
#     app_cli        命令行验收程序，把表打印出来并做格式自查
#
#  这个模板没有容器要写：要写的是**判断**——六段需求各该用哪个结构、代价在哪、
#  凭什么这么判断。四栏按阶段填在 src/choices.cpp 的 kChoices 里。
#
#  命令行验证：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_cli.exe
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(ds06_choose
    VERSION 1.0.0
    DESCRIPTION "练习模板 06 · 怎么选容器与结构：六段需求与一张待填的选型表"
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

# ---------------------------------------------------------------- 题面与表
add_library(core STATIC
    src/choices.cpp
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
