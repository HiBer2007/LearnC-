# ===================================================================
#  示例 06-standard-library/03-cpp-string-text · std::string 与 string_view 文本处理
#
#    core      纯逻辑，不含任何界面代码
#    app_cli   命令行版，链接 core
#
#  核心模块单独成静态库：命令行版与将来可能加的界面版共用同一份实现，
#  把界面文件删掉，核心库照样能编译、能自测通过。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(app
    VERSION 1.0.0
    DESCRIPTION "文本处理示例：切分、修剪、查找替换、string_view 零拷贝与 UTF-8 字节处理"
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

# ---------------------------------------------------------------
#  核心模块：不依赖界面，可单独编译
# ---------------------------------------------------------------
add_library(core STATIC
    src/text_tools.cpp
)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---------------------------------------------------------------
#  字符集
#
#  这两项是全板块统一的：源码按 UTF-8 读，普通窄字符串按 GBK 写进 exe，
#  以匹配中文 Windows 控制台默认的 CP936。
#
#  本示例另有约定：核心库全程按 UTF-8 处理，所有面向人的文字都写成 u8"" 字面量
#  （u8 字面量固定是 UTF-8 字节，不受 /execution-charset 与 -fexec-charset 影响），
#  因为 data/sample_text.txt 本身就是 UTF-8。
#  命令行版在输出前按控制台代码页转换一次，见 src/main_cli.cpp 的 to_console_encoding。
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

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
