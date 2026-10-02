# ===================================================================
#  示例 06-lower-level/06-binary-tools
#
#    core      纯逻辑：跑工具、解析四种输出、给结论、自测
#    app_cli   命令行版，链接 core
#
#  工具路径在配置阶段找一次，用编译定义传给 core：
#  程序里不写死任何本机路径，换一台机器重新配置即可。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(binary_tools
    VERSION 1.0.0
    DESCRIPTION "把 nm / objdump / size / readelf 的输出读成结论"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

if(NOT MSVC)
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------
#  找工具：先看编译器同目录（MinGW 的 binutils 就在 gcc 旁边），
#  再退回 PATH。MSVC 那边没有 size/readelf，找不到就留空，
#  程序会打印「没找到」并跳过对应的那一段。
# ---------------------------------------------------------------
get_filename_component(_cc_dir "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(BT_OBJDUMP_EXE NAMES objdump HINTS "${_cc_dir}")
find_program(BT_NM_EXE      NAMES nm      HINTS "${_cc_dir}")
find_program(BT_SIZE_EXE    NAMES size    HINTS "${_cc_dir}")
find_program(BT_READELF_EXE NAMES readelf HINTS "${_cc_dir}")

foreach(_v BT_OBJDUMP_EXE BT_NM_EXE BT_SIZE_EXE BT_READELF_EXE)
    if(${_v})
        message(STATUS "${_v} = ${${_v}}")
    else()
        message(STATUS "${_v} = （没找到）")
    endif()
endforeach()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------
#  核心模块
# ---------------------------------------------------------------
add_library(core STATIC
    src/bin_tools.c
    src/tool_fixtures.c
)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# 路径里的反斜杠在 C 字符串里是转义字符，这里统一换成正斜杠再传进去
function(bt_slash out_var in_path)
    if(NOT in_path)
        set(${out_var} "" PARENT_SCOPE)
    else()
        string(REPLACE "\\" "/" _p "${in_path}")
        set(${out_var} "${_p}" PARENT_SCOPE)
    endif()
endfunction()

bt_slash(_objdump "${BT_OBJDUMP_EXE}")
bt_slash(_nm      "${BT_NM_EXE}")
bt_slash(_size    "${BT_SIZE_EXE}")
bt_slash(_readelf "${BT_READELF_EXE}")

target_compile_definitions(core PUBLIC
    BT_OBJDUMP="${_objdump}"
    BT_NM="${_nm}"
    BT_SIZE="${_size}"
    BT_READELF="${_readelf}"
)

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
