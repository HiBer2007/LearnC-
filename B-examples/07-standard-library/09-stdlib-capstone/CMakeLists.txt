# ===================================================================
#  示例 07-standard-library/09-stdlib-capstone · 标准库综合流水线
#
#    core     纯逻辑，不含任何界面代码
#    app_cli  命令行版，链接 core
#
#  本示例不做 GUI：界面形态已经在 01、02、05、07 给出四处，
#  这里把重心放在把八个能力串成一条流水线，输出是一份文本报表。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(app
    VERSION 1.0.0
    DESCRIPTION "标准库综合流水线：读配置、读文本、分词计数、统计、计时、出报表"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# MinGW → build/mingw/bin/*.exe
# MSVC  → build/msvc/bin/Debug/*.exe（多配置生成器自动加一层）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------
#  核心模块：不依赖界面，可单独编译
# ---------------------------------------------------------------
add_library(core STATIC
    src/capstone.cpp
)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---------------------------------------------------------------
#  字符集：中文输出不乱码的关键（源码按 UTF-8 读，窄字符串按 GBK 写进 exe，
#  以匹配中文 Windows 控制台默认的 CP936）
#  报表里的文本统计只认 ASCII 字母，因此执行字符集不影响统计结果。
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
