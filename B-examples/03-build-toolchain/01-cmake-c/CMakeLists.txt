# ===================================================================
#  示例 03 · CMake 工程（C 语言，多文件）
#
#  两套构建预设见同目录的 CMakePresets.json：
#     mingw-gdb    → MinGW GCC 编译，用 GDB 调试
#     msvc         → MSVC 编译，用 Visual Studio 调试器调试
#
#  命令行用法：
#     cmake --preset mingw-gdb    && cmake --build --preset mingw-gdb
#     cmake --preset msvc         && cmake --build --preset msvc-debug
#
#  VS Code 里直接按 F5，会自动完成「配置 + 编译 + 启动调试」。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(app
    VERSION 1.0.0
    DESCRIPTION "CMake 多文件工程示例（C）"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 单配置生成器（Ninja / MinGW Makefiles）默认走 Debug
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# 可执行文件统一输出到 <build>/bin。
# 多配置生成器（Visual Studio）会自动再套一层 Debug/ 子目录，所以最终是：
#     MinGW → build/mingw/bin/app.exe
#     MSVC  → build/msvc/bin/Debug/app.exe
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 生成 compile_commands.json 给智能感知用精确编译参数
# 注意：只有 Ninja / Makefile 生成器会产出，Visual Studio 生成器不会
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------
#  多文件：源文件必须显式列出
#  （MSYS2 从 2024-11 起禁用了通配符展开，别用 file(GLOB *.c)）
# ---------------------------------------------------------------
add_executable(app
    src/main.c
    src/calc.c
)

target_include_directories(app
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ---------------------------------------------------------------
#  字符集设置：中文输出不乱码的关键
#
#  源码文件是 UTF-8，但程序输出要显示在中文 Windows 的控制台里，
#  该类控制台默认按 CP936（GBK）解释字节流。
#  因此需要：源码按 UTF-8 读取，字符串常量按 GBK 写入 exe。
#
#  不设置时，gcc 与 MSVC 默认都把字符串常量写成 UTF-8，
#  printf("中文") 会显示成 "涓枃" 一类的乱码。
#  两种编译器的实测字节对照见《01-编译器/02-环境配置.md》第 9 章。
# ---------------------------------------------------------------
if(MSVC)
    # /source-charset:utf-8    源码按 UTF-8 读取
    # /execution-charset:gbk   字符串常量按 GBK 写入 exe
    # /W4                      较高的警告等级
    target_compile_options(app PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    # -g                     始终生成调试信息（Release 下也能调试）
    # -finput-charset=UTF-8  源码按 UTF-8 读取
    # -fexec-charset=GBK     字符串常量按 GBK 写入 exe
    target_compile_options(app PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()
