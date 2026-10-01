# ===================================================================
#  练习模板 07 · filesystem 目录工具（C++17，多文件，核心 + 命令行 + 两份界面）
#
#  目标：
#     core           纯逻辑模块（src/dirscan.cpp），不含任何界面代码
#     app_cli        命令行验收程序，先跑它确认逻辑对不对
#     app_gui_win32  Win32 界面程序，用 -mwindows 链接，零第三方依赖
#     app_gui_qt     Qt Widgets 界面程序，默认不构建，见下面的 WITH_QT 开关
#
#  命令行验证：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_cli.exe
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(cpp_filesystem
    VERSION 1.0.0
    DESCRIPTION "练习模板 07 · filesystem：path、遍历、两条错误路径、增删改复制"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 单配置生成器（Ninja）没指定构建类型时默认走 Debug
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# 产物统一放到 <build>/bin 下
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------- 核心逻辑
# 这一层是练习的主体，界面与命令行都调用它，因此它不能包含任何界面代码。
add_library(core STATIC
    src/dirscan.cpp
)

target_include_directories(core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ---------------------------------------------------------------- 命令行版
add_executable(app_cli
    src/main_cli.cpp
)

target_link_libraries(app_cli PRIVATE core)
# ---------------------------------------------------------------- 界面版（Win32）
# WIN32 关键字让 MinGW 加 -mwindows，程序不带控制台窗口。
if(WIN32)
    add_executable(app_gui_win32 WIN32
        src/main_gui_win32.cpp
    )

    target_link_libraries(app_gui_win32 PRIVATE core user32 gdi32)
else()
    message(STATUS "非 Windows 平台：跳过界面版 app_gui_win32")
endif()
# ---------------------------------------------------------------- 界面版（Qt）
# 默认关闭：没有 Qt 时照常构建 core、app_cli 与 app_gui_win32。
# 打开方式见《配置步骤.md》的「界面部分怎么用」一节：
#     cmake --preset mingw-gdb -DWITH_QT=ON -DQT_ROOT=<Qt 的套件目录>
# Qt 的位置由 工具/获取依赖/qt-dynamic.cmake 决定（先看 -DQT_ROOT，再看环境变量
# QT_ROOT，都没有就交给 find_package 自己去系统里找），这里不写死任何本机路径。
option(WITH_QT "构建 Qt 版界面" OFF)
if(WITH_QT)
    set(_qt_helper "${CMAKE_CURRENT_SOURCE_DIR}/../../../工具/获取依赖/qt-dynamic.cmake")
    if(EXISTS "${_qt_helper}")
        include("${_qt_helper}")
    else()
        message(FATAL_ERROR
            "找不到 ${_qt_helper}。\n"
            "Qt 版界面要求本工程位于教材仓库内（C-templates/<板块>/<模板名>/）。\n"
            "若模板已复制到别处，请自行准备 Qt 并改为：\n"
            "  find_package(Qt6 COMPONENTS Widgets REQUIRED)\n"
            "  target_link_libraries(app_gui_qt PRIVATE core Qt6::Widgets)")
    endif()

    find_package(Qt6 COMPONENTS Widgets REQUIRED)

    add_executable(app_gui_qt WIN32
        src/main_gui_qt.cpp
    )

    target_link_libraries(app_gui_qt PRIVATE core Qt6::Widgets)

    # 链接了 Qt 的目标**不传任何字符集选项**：Qt 6 在 MSVC 下会自己加 /utf-8，
    # 这里再写 /utf-8 或 /source-charset 会报 D8016（选项冲突）。
    if(MSVC)
        target_compile_options(app_gui_qt PRIVATE /W4)
    else()
        target_compile_options(app_gui_qt PRIVATE -g -Wall -Wextra)
    endif()

    # 构建完成后把 Qt 运行时（含 platforms\qwindows.dll）与编译器运行时拷到 exe 旁边。
    # 部署产物全部落在 build/ 里，已被仓库 .gitignore 覆盖，不要提交 DLL。
    qt_enable_deploy(app_gui_qt)
endif()
# ---------------------------------------------------------------- 编译选项
foreach(target core app_cli)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4)
    else()
        target_compile_options(${target} PRIVATE -g -Wall -Wextra)
    endif()
endforeach()
if(WIN32)
    if(MSVC)
        target_compile_options(app_gui_win32 PRIVATE /utf-8 /W4)
    else()
        target_compile_options(app_gui_win32 PRIVATE -g -Wall -Wextra)
    endif()
endif()
# 给智能感知生成精确编译参数（只有 Ninja / Makefile 生成器会产出）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
