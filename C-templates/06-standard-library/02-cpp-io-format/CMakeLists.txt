# ===================================================================
#  练习模板 02 · C++ 输入输出与格式化（C++17，多文件）
#
#  两个目标：
#     core      纯逻辑模块（src/report.cpp），不含任何界面代码
#     app_cli   命令行验收程序，也是本模板唯一的驱动
#
#  命令行验证：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_cli.exe
#
#  数据文件按相对路径 data/sample.txt 打开，因此**在模板目录下运行**。
#  这份数据与 01-c-stdlib-toolbox 里的逐字节相同，两份程序出的报表也应当逐字节相同。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(cpp_io_format
    VERSION 1.0.0
    DESCRIPTION "练习模板 02 · C++ 输入输出与格式化：iostream、iomanip、sstream、fstream"
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
add_library(core STATIC
    src/report.cpp
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
# MSVC 会把 std::strerror 一类 C 函数报成 C4996（说它们「不安全」）。
# 那是 MSVC 自己的扩展检查，不是标准的要求；本工程按 ISO C++ 的写法来，
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

# 给智能感知生成精确编译参数（只有 Ninja / Makefile 生成器会产出）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
