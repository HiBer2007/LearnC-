# ===================================================================
#  示例 01 · C 标准库报表工具（c-stdlib-toolbox）
#
#    core            纯逻辑，不含任何界面代码
#    app_cli         命令行版，链接 core
#    app_gui_win32   界面版（Win32 API，用 C 写），链接 core + user32 + gdi32
#
#  核心模块单独成一个静态库，好处有三条：
#    · 命令行版与界面版共用同一份实现，报表逐字节相同
#    · 界面出问题时先跑命令行版，能立刻分出是逻辑还是界面
#    · 与 02-cpp-io-report 对照时，两边的 core 做的是同一件事
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(c_stdlib_toolbox
    VERSION 1.0.0
    DESCRIPTION "C 标准库报表工具：stdio 读文本、string.h 分词、qsort 排序、time.h 计时"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# MinGW → build/mingw/bin/*.exe
# MSVC  → build/msvc/bin/Debug/*.exe（多配置生成器自动加一层）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------
#  核心模块：不依赖 GUI，可单独编译
# ---------------------------------------------------------------
add_library(core STATIC
    src/sales_report.c
)

target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---------------------------------------------------------------
#  字符集设置：中文输出不乱码的关键
#  源码按 UTF-8 读取，窄字符串按 GBK 写入 exe，
#  以匹配中文 Windows 控制台默认的 CP936。
#
#  报表的列宽全靠这一项才对齐：GBK 里一个汉字 2 字节，
#  而它在控制台上正好占 2 列，于是 %-16s 既是字节宽度也是显示宽度。
# ---------------------------------------------------------------
if(MSVC)
    target_compile_options(core PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(core PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()

# ---------------------------------------------------------------
#  命令行版
# ---------------------------------------------------------------
add_executable(app_cli src/main_cli.c)
target_link_libraries(app_cli PRIVATE core)
if(MSVC)
    target_compile_options(app_cli PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(app_cli PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()

# ---------------------------------------------------------------
#  界面版：Win32 API，零依赖
#    WIN32 关键字让链接器走图形子系统（MinGW 下即 -mwindows），
#    程序启动时不会弹出控制台窗口。
# ---------------------------------------------------------------
if(WIN32)
    add_executable(app_gui_win32 WIN32 src/main_gui_win32.c)
    target_link_libraries(app_gui_win32 PRIVATE core user32 gdi32)
    if(MSVC)
        target_compile_options(app_gui_win32 PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
    else()
        target_compile_options(app_gui_win32 PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
    endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
