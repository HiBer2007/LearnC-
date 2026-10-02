# ===================================================================
#  示例 07-standard-library/06-cpp-chrono-benchmark · <chrono> 小基准，命令行
#
#    core      纯逻辑，不含任何界面代码
#    app_cli   命令行版，链接 core
#
#  本示例没有图形界面：结果是一张表，命令行里看最合适。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(app
    VERSION 1.0.0
    DESCRIPTION "小基准工具示例：预热、多次测量、分位数统计，用 atomic 计数迭代次数"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# MinGW → build/mingw/bin/*.exe；MSVC → build/msvc/bin/Debug/*.exe
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------
#  核心模块：不依赖界面，可单独编译
# ---------------------------------------------------------------
add_library(core STATIC
    src/benchmark.cpp
)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---------------------------------------------------------------
#  字符集：中文输出不乱码的关键（源码按 UTF-8 读，窄字符串按 GBK 写进 exe，
#  以匹配中文 Windows 控制台默认的 CP936）
# ---------------------------------------------------------------
if(MSVC)
    target_compile_options(core PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(core PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()

# ---------------------------------------------------------------
#  命令行版
# ---------------------------------------------------------------
add_executable(app_cli src/main_cli.cpp)
target_link_libraries(app_cli PRIVATE core)
if(MSVC)
    target_compile_options(app_cli PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(app_cli PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()
