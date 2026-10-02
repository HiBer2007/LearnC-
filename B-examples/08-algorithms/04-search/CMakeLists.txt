# ===================================================================
#  示例 08-algorithms/04-search · 查找：从线性到索引
#
#    core            纯逻辑，不含任何界面代码
#    app_cli         命令行版，链接 core
#    find_threshold  正文第 4.1.2 小节的对照程序，单文件、不链接 core
#
#  核心模块单独成静态库：命令行版与将来可能加的界面版共用同一份实现，
#  把界面文件删掉，核心库照样能编译、能自测通过。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(app
    VERSION 1.0.0
    DESCRIPTION "查找：从线性到索引"
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
    src/search_lab.cpp
)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---------------------------------------------------------------
#  字符集：全板块统一——源码按 UTF-8 读，普通窄字符串按 GBK 写进 exe，
#  以匹配中文 Windows 控制台默认的 CP936。
#  核心库所有面向人的文字都写成 u8"" 字面量（固定是 UTF-8 字节，
#  不受 /execution-charset 与 -fexec-charset 影响），
#  命令行版在输出前按控制台代码页转换一次，见 src/main_cli.cpp。
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

# ---------------------------------------------------------------
#  正文第 4.1.2 小节的对照程序：三种摆法在十种规模上的代价
#  正文里的代码块与本文件逐字相同（版权头除外），正文那张表由它跑出。
#  优化等级固定 -O2：正文的数全部出自 -O2 构建，与文件首行注释里的命令一致。
# ---------------------------------------------------------------
add_executable(find_threshold src/find_threshold.cpp)
if(MSVC)
    target_compile_options(find_threshold PRIVATE /O2 /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(find_threshold PRIVATE -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()
