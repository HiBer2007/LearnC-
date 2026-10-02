# ===================================================================
#  示例 06-lower-level/01-layout-and-align
#
#    core      纯逻辑：取地址、算布局、跑伪共享计时，不含任何界面
#    app_cli   命令行版，链接 core
#
#  核心模块单独成一个静态库：命令行版只是它的一层壳，
#  自己写的程序也能直接链接它去查布局。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(layout_and_align
    VERSION 1.0.0
    DESCRIPTION "布局与对齐：段地址、sizeof/offsetof、packed 与伪共享"
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

find_package(Threads REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------
#  核心模块：只依赖 C++ 标准库
# ---------------------------------------------------------------
add_library(core STATIC src/layout_lab.cpp)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(core PUBLIC Threads::Threads)

# ---------------------------------------------------------------
#  字符集：源码按 UTF-8 读，窄字符串按 GBK 写进 exe，
#  与中文 Windows 控制台默认的 CP936 对齐，中文输出才不乱码。
#
#  /wd4324：MSVC 对「因为 alignas 而产生了填充」的结构体报 C4324。
#  本示例的 stat_aligned 与 counters_padded 正是故意这么写的，
#  填充就是要观察的对象，因此关掉这一条。
# ---------------------------------------------------------------
if(MSVC)
    target_compile_options(core PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4 /wd4324)
else()
    target_compile_options(core PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()

add_executable(app_cli src/main_cli.cpp)
target_link_libraries(app_cli PRIVATE core)
if(MSVC)
    target_compile_options(app_cli PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4 /wd4324)
else()
    target_compile_options(app_cli PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()
