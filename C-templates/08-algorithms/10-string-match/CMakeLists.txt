# ===================================================================
#  练习模板 10 · 字符串匹配（C++17）
#
#  目标：
#     core          静态库：算法实现，本模板的 TODO 都在这里
#     app_cli       命令行验收程序，按阶段打印结果，不需要改
#
#  命令行验证（在模板目录下执行，产物在 build\mingw\bin\）：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_cli.exe
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(algo10_string_match
    VERSION 1.0.0
    DESCRIPTION "失配表的递推、KMP 的失配跳转、滚动哈希的更新与去重"
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
# 这一行必须放在 add_library / add_executable 之前
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------- 核心库
add_library(core STATIC
    src/strmatch.cpp)

target_include_directories(core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ---------------------------------------------------------------- 命令行版
add_executable(app_cli
    src/main_cli.cpp
)

target_link_libraries(app_cli PRIVATE core)

# ---------------------------------------------------------------- 编译选项
if(MSVC)
    target_compile_options(core PRIVATE /utf-8 /W4)
    target_compile_options(app_cli PRIVATE /utf-8 /W4)
else()
    target_compile_options(core PRIVATE -g -Wall -Wextra)
    target_compile_options(app_cli PRIVATE -g -Wall -Wextra)
endif()
