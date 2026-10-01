# ===================================================================
#  示例 07-lower-level/02-volatile-and-registers
#
#    core      纯逻辑：模拟寄存器区、两种等待写法、带看门狗的探针，不含界面
#    app_cli   命令行版，链接 core
#
#  这个工程只构建主机侧。板级的那一份（board/）走 arm-none-eabi-gcc，
#  不用 CMake：交叉编译链在 WSL 里，见 board/build.sh。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(volatile_and_registers
    VERSION 1.0.0
    DESCRIPTION "寄存器与 volatile：两种等待写法的行为差别与汇编对照"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# CMake 给 GCC 的 Release 默认是 -O3；本示例的对照一律按 -O2 讲，这里对齐。
if(NOT MSVC)
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# MinGW → build/mingw/bin/*.exe
# MSVC  → build/msvc/bin/Debug/*.exe（多配置生成器自动加一层）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------
#  核心模块：只依赖 C 标准库与平台线程接口
# ---------------------------------------------------------------
add_library(core STATIC src/volatile_regs.c)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---------------------------------------------------------------
#  字符集：源码按 UTF-8 读，窄字符串按 GBK 写进 exe，
#  与中文 Windows 控制台默认的 CP936 对齐。
# ---------------------------------------------------------------
if(MSVC)
    target_compile_options(core PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(core PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()

add_executable(app_cli src/main_cli.c)
target_link_libraries(app_cli PRIVATE core)
if(MSVC)
    target_compile_options(app_cli PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(app_cli PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
