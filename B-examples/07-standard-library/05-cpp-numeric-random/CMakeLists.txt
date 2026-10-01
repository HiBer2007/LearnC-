# ===================================================================
#  示例 07-standard-library/05-cpp-numeric-random · 随机数与统计，命令行 + Win32
#
#    core           纯逻辑，不含任何界面代码
#    app_cli        命令行版，链接 core
#    app_gui_win32  GUI 版（Win32 API，零依赖，默认构建）
#
#  核心模块单独成静态库：命令行版与界面共用同一份实现。
#  把 src/main_gui_win32.cpp 删掉，core 照样能编译、能自测通过。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(app
    VERSION 1.0.0
    DESCRIPTION "随机数与统计示例：随机抽样、数值统计、直方图，命令行版与 Win32 版共用核心模块"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# MinGW → build/mingw/bin/*.exe；MSVC → build/msvc/bin/Debug/*.exe
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------
#  核心模块：不依赖界面，可单独编译
# ---------------------------------------------------------------
add_library(core STATIC
    src/stats_demo.cpp
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

# ---------------------------------------------------------------
#  GUI 版：Win32 API，零依赖（默认构建）
#    WIN32 关键字让链接器走图形子系统（MinGW 下即 -mwindows），
#    程序启动时不会弹出控制台窗口。
# ---------------------------------------------------------------
if(WIN32)
    add_executable(app_gui_win32 WIN32 src/main_gui_win32.cpp)
    target_link_libraries(app_gui_win32 PRIVATE core user32 gdi32)
    if(MSVC)
        target_compile_options(app_gui_win32 PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
    else()
        target_compile_options(app_gui_win32 PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
    endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
