# ===================================================================
#  示例 06-lower-level/04-symbols-and-linking
#
#    core           静态库：link_core.o + link_static_a.o + link_static_b.o
#    app_cli        引用成员 B，正常版本
#    app_cli_slim   同一份源码，不引用成员 B —— 用来对照静态库的取舍粒度
#
#  静态库的粒度是「目标文件」：哪个成员没人引用，它一个字节都不进最终映像。
#  两个可执行目标跑的是同一套自测，只在 WITH_B 这一个宏上不同。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(symbols_and_linking
    VERSION 1.0.0
    DESCRIPTION "符号与链接：弱符号覆盖、静态库成员粒度、C 与 C++ 混编"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

if(NOT MSVC)
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# MinGW → build/mingw/bin/*.exe
# MSVC  → build/msvc/bin/Debug/*.exe（多配置生成器自动加一层）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------
#  静态库：三个成员，每个成员一个目标文件
# ---------------------------------------------------------------
add_library(core STATIC
    src/link_core.c
    src/link_static_a.c
    src/link_static_b.c
)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---------------------------------------------------------------
#  字符集：源码按 UTF-8 读，窄字符串按 GBK 写进 exe。
#  这一组选项 gcc 与 g++ 都接受，因此整段照写即可。
# ---------------------------------------------------------------
if(MSVC)
    target_compile_options(core PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(core PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()

# ---------------------------------------------------------------
#  两个命令行目标：同一份源码，只差 WITH_B 这一个宏
# ---------------------------------------------------------------
set(app_sources
    src/main_cli.c
    src/link_override.c
    src/link_cpp_side.cpp
)

add_executable(app_cli ${app_sources})
target_link_libraries(app_cli PRIVATE core)
target_compile_definitions(app_cli PRIVATE WITH_B=1)
# 目标里既有 C 又有 C++ 源文件，用 g++ / cl 来链接，C++ 运行时才会被带上
set_target_properties(app_cli PROPERTIES LINKER_LANGUAGE CXX)

add_executable(app_cli_slim ${app_sources})
target_link_libraries(app_cli_slim PRIVATE core)
set_target_properties(app_cli_slim PROPERTIES LINKER_LANGUAGE CXX)

foreach(tgt app_cli app_cli_slim)
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
    else()
        target_compile_options(${tgt} PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
    endif()
endforeach()
