# ===================================================================
#  空模板 09 · CMake 工程（C++，多文件，核心 + 命令行 + 界面）
#
#  三个目标：
#     core      纯逻辑模块（src/file_guard.cpp），不含任何界面代码
#     app_cli   命令行验收程序，先跑它确认逻辑对不对
#     app_gui_win32  Win32 界面程序，用 -mwindows 链接，零第三方依赖
#     app_gui_qt     Qt Widgets 界面程序，默认不构建，见下面的 WITH_QT 开关
#
#  命令行验证：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_cli.exe
#     build\mingw\bin\app_gui_win32.exe
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(app
    VERSION 1.0.0
    DESCRIPTION "空模板 09 · RAII 与异常：文件句柄包装（核心 + 命令行 + Win32 界面）"
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
    src/file_guard.cpp
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
# WIN32 关键字让 MinGW 加 -mwindows，程序不带控制台窗口
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
# 打开方式见《配置步骤.md》的「两份界面」一节：
#     cmake --preset mingw-gdb -DWITH_QT=ON -DQT_ROOT=<Qt 的套件目录>
# Qt 的位置由 工具/获取依赖/qt-dynamic.cmake 决定（先看 -DQT_ROOT，再看环境变量
# QT_ROOT，都没有就交给 find_package 自己去系统里找），这里不写死任何本机路径。
option(WITH_QT "构建 Qt 版界面" OFF)
if(WITH_QT)
    set(_qt_helper "${CMAKE_CURRENT_SOURCE_DIR}/../../工具/获取依赖/qt-dynamic.cmake")
    if(EXISTS "${_qt_helper}")
        include("${_qt_helper}")
    else()
        message(FATAL_ERROR
            "找不到 ${_qt_helper}。\n"
            "Qt 版界面要求本工程位于教材仓库内（C-空模板/<模板名>/）。\n"
            "若模板已复制到别处，请自行准备 Qt 并改为：\n"
            "  find_package(Qt6 COMPONENTS Widgets REQUIRED)\n"
            "  target_link_libraries(app_gui_qt PRIVATE core Qt6::Widgets)")
    endif()

    find_package(Qt6 COMPONENTS Widgets REQUIRED)

    add_executable(app_gui_qt WIN32
        src/main_gui_qt.cpp
    )

    target_link_libraries(app_gui_qt PRIVATE core Qt6::Widgets)

    # MinGW：把 GCC 运行时静态链进来，exe 就不再依赖 libgcc_s_seh-1.dll 与
    # libstdc++-6.dll（libwinpthread-1.dll 仍由下面的部署步骤拷到 exe 旁边）。
    if(MINGW)
        target_link_options(app_gui_qt PRIVATE -static-libgcc -static-libstdc++)
    endif()

    # 链接了 Qt 的目标**不传任何字符集选项**：Qt 6 在 MSVC 下会自己加 /utf-8，
    # 这里再写 /utf-8 或 /source-charset 会报 D8016（选项冲突）。
    if(MSVC)
        target_compile_options(app_gui_qt PRIVATE /W4)
    else()
        target_compile_options(app_gui_qt PRIVATE -g -Wall -Wextra)
    endif()

    # ------------------------------------------------------------ 部署 Qt 运行时
    # 动态版 Qt 的程序在运行期要能找到 DLL，否则会报「找不到 Qt6Core.dll」一类错误。
    # 下面这一步在构建完成后调用 windeployqt，把 Qt 的 DLL 与
    # plugins\platforms\qwindows.dll 拷到 exe 旁边，拷完双击 exe 即可运行。
    # 目标机器仍需要对应的编译器运行库（windeployqt 默认不部署它）。
    # 部署产物全部落在 build/ 里，已被仓库 .gitignore 覆盖，不要提交。
    #
    # 待 工具/获取依赖/qt-dynamic.cmake 里的 qt_enable_deploy(<target>) 封装就绪后，
    # 这一段可以换成一行 qt_enable_deploy(app_gui_qt)。
    option(WITH_QT_DEPLOY "构建后把 Qt 运行时部署到 exe 旁边" ON)
    if(WITH_QT_DEPLOY)
        # windeployqt 的位置：优先用 Qt 自己导出的目标，取不到再从 Qt6_DIR 推。
        # Qt6_DIR 形如 <套件>/lib/cmake/Qt6，向上三级就是套件目录。
        if(TARGET Qt6::windeployqt)
            set(_qt_deploy_tool Qt6::windeployqt)
        else()
            get_filename_component(_qt_kit_dir "${Qt6_DIR}/../../.." ABSOLUTE)
            set(_qt_deploy_tool "${_qt_kit_dir}/bin/windeployqt${CMAKE_EXECUTABLE_SUFFIX}")
        endif()

        if(TARGET Qt6::windeployqt OR EXISTS "${_qt_deploy_tool}")
            add_custom_command(TARGET app_gui_qt POST_BUILD
                COMMAND "${_qt_deploy_tool}"
                        --no-translations
                        --no-system-d3d-compiler
                        --no-opengl-sw
                        "$<TARGET_FILE:app_gui_qt>"
                COMMENT "部署 Qt 运行时到 $<TARGET_FILE_DIR:app_gui_qt>"
                VERBATIM)
        else()
            message(WARNING
                "找不到 windeployqt，跳过 Qt 运行时部署：${_qt_deploy_tool}\n"
                "程序仍能构建，但运行时会报缺少 Qt 的 DLL。")
        endif()

        # MinGW：把编这个 exe 的那套 MinGW 运行时 DLL 拷到 exe 旁边。
        # 必须与编译器同源：不同 GCC 版本的 libstdc++-6.dll 混用会出怪问题。
        # 路径从 CMAKE_CXX_COMPILER 推，不写死任何本机路径；
        # copy_if_different 在目标已存在且内容一致时会跳过。
        if(MINGW)
            get_filename_component(_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
            foreach(_runtime libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
                if(EXISTS "${_mingw_bin}/${_runtime}")
                    add_custom_command(TARGET app_gui_qt POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${_mingw_bin}/${_runtime}"
                                "$<TARGET_FILE_DIR:app_gui_qt>/${_runtime}"
                        COMMENT "部署 MinGW 运行时：${_runtime}"
                        VERBATIM)
                endif()
            endforeach()
        endif()
    endif()
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
