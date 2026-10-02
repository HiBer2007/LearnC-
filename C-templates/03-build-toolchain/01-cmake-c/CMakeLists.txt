# ===================================================================
#  空模板 03 · CMake 工程（C，多文件）
#
#  这个 CMakeLists.txt 是完整可用的，先用命令行验证它能编译：
#      cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc
#      cmake --build build
#
#  缺的是【预设文件】和【VS Code 配置】，请照着《配置步骤.md》补上。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(app
    VERSION 1.0.0
    DESCRIPTION "空模板 · CMake 多文件工程（C）"
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

# 给智能感知生成精确编译参数
# 注意：只有 Ninja / Makefile 生成器会产出，Visual Studio 生成器不会
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(app
    src/main.c
    src/util.c
)

target_include_directories(app
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(app PRIVATE /utf-8 /W4)
else()
    target_compile_options(app PRIVATE -g -Wall -Wextra)
endif()
