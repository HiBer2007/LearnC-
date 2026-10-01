# ===================================================================
#  示例 07-lower-level/05-interrupt-and-atomic
#
#    core      纯逻辑：三种写法的对照实验与自测，不含界面
#    app_cli   命令行版，链接 core
#
#  这个工程只构建主机侧。板级那一份（board/）走 arm-none-eabi-gcc，
#  不用 CMake：交叉编译链在 WSL 里，见 board/build.sh。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(interrupt_and_atomic
    VERSION 1.0.0
    DESCRIPTION "中断与原子：丢更新复现与三种修法对照"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# CMake 给 GCC 的 Release 默认是 -O3；本示例的对照按 -O2 讲，这里对齐。
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Threads REQUIRED)

add_library(core STATIC src/race_lab.cpp)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(core PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(core PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(core PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()

add_executable(app_cli src/main_cli.cpp)
target_link_libraries(app_cli PRIVATE core)
if(MSVC)
    target_compile_options(app_cli PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(app_cli PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
