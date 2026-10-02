# ===================================================================
#  练习模板 01 · C 标准库综合（C，多文件，核心 + 命令行 + Win32 界面）
#
#  三个目标：
#     core           纯逻辑模块（src/textstats.c），不含任何界面代码
#     app_cli        命令行验收程序，先跑它确认逻辑对不对
#     app_gui_win32  Win32 界面程序，用 -mwindows 链接，零第三方依赖
#
#  命令行验证：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_cli.exe
#     build\mingw\bin\app_gui_win32.exe
#
#  数据文件按相对路径 data/sample.txt 打开，因此**在模板目录下运行**。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(c_stdlib_toolbox
    VERSION 1.0.0
    DESCRIPTION "练习模板 01 · C 标准库综合：读文件、词频、qsort、计时、报表"
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

# 给智能感知生成精确编译参数（只有 Ninja / Makefile 生成器会产出）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------- 核心逻辑
# 这一层是练习的主体，界面与命令行都调用它，因此它不能包含任何界面代码。
add_library(core STATIC
    src/textstats.c
)

target_include_directories(core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ---------------------------------------------------------------- 命令行版
add_executable(app_cli
    src/main_cli.c
)

target_link_libraries(app_cli PRIVATE core)

# ---------------------------------------------------------------- 界面版（Win32）
# WIN32 关键字让 MinGW 加 -mwindows，程序不带控制台窗口。
if(WIN32)
    add_executable(app_gui_win32 WIN32
        src/main_gui_win32.c
    )

    target_link_libraries(app_gui_win32 PRIVATE core user32 gdi32)
else()
    message(STATUS "非 Windows 平台：跳过界面版 app_gui_win32")
endif()

# ---------------------------------------------------------------- 编译选项
# MSVC 会把 ISO C 的 fopen / strerror 一类函数报成 C4996（说它们「不安全」）。
# 那是 MSVC 自己的扩展检查，不是 C 标准的要求；本工程按 ISO C 的写法来，
# 因此关掉这一类弃用警告，好让「零警告」这个验收口径对两条路线都成立。
if(MSVC)
    target_compile_definitions(core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

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
