# ===================================================================
#  练习模板 05 · random 与 numeric（C++17，多文件，核心 + 命令行 + Win32 界面）
#
#  目标：
#     core           纯逻辑模块（src/stats.cpp），不含任何界面代码
#     app_cli        命令行验收程序，先跑它确认逻辑对不对
#     app_gui_win32  Win32 界面程序，用 -mwindows 链接，零第三方依赖
#
#  命令行验证：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_cli.exe
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(cpp_numeric_random
    VERSION 1.0.0
    DESCRIPTION "练习模板 05 · random 与 numeric：引擎、分布、统计量与直方图"
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

# ---------------------------------------------------------------- 核心逻辑
# 这一层是练习的主体，界面与命令行都调用它，因此它不能包含任何界面代码。
add_library(core STATIC
    src/stats.cpp
)

target_include_directories(core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ---------------------------------------------------------------- 命令行版
add_executable(app_cli
    src/main_cli.cpp
)

target_link_libraries(app_cli PRIVATE core)
# ---------------------------------------------------------------- 界面版（Win32）
# WIN32 关键字让 MinGW 加 -mwindows，程序不带控制台窗口。
if(WIN32)
    add_executable(app_gui_win32 WIN32
        src/main_gui_win32.cpp
    )

    target_link_libraries(app_gui_win32 PRIVATE core user32 gdi32)
else()
    message(STATUS "非 Windows 平台：跳过界面版 app_gui_win32")
endif()
# ---------------------------------------------------------------- 编译选项
foreach(target core app_cli)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4)
    else()
        target_compile_options(${target} PRIVATE -g -Wall -Wextra)
    endif()
endforeach()
if(WIN32)
    if(MSVC)
        target_compile_options(app_gui_win32 PRIVATE /utf-8 /W4)
    else()
        target_compile_options(app_gui_win32 PRIVATE -g -Wall -Wextra)
    endif()
endif()
# 给智能感知生成精确编译参数（只有 Ninja / Makefile 生成器会产出）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
