# ===================================================================
#  练习模板 13 · 控制（PID 等）（C++17）—— 阶段 1
#
#  本阶段是一个能单独编译的小工程：被控对象、开环、开关控制。
#  后面每一个阶段的工程都是在它上面继续加东西的，因此这一份是
#  后面各阶段的起点。
#
#  目标：
#     core          静态库：被控对象与控制器，本模板的 TODO 都在这里
#     app_cli       命令行验收程序，按阶段打印结果，不需要改
#
#  命令行验证（在 13-control 目录下执行，产物在 <构建目录>\bin\）：
#     cmake -S stage1 -B %TEMP%\ctl-st1 -G Ninja -DCMAKE_CXX_COMPILER=g++
#     cmake --build %TEMP%\ctl-st1
#     %TEMP%\ctl-st1\bin\app_cli.exe
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(algo13_control_stage1
    VERSION 1.0.0
    DESCRIPTION "被控对象、开环误差与带滞回的开关控制"
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
    src/control.cpp)

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
