# ===================================================================
#  示例 07 · 值类型项目（IntVector）—— 一个核心模块，两个可执行目标
#
#    core     纯逻辑，不含任何界面代码
#    app_cli  命令行版，链接 core
#    app_gui  GUI 版（Win32 API），链接 core + user32 + gdi32
#
#  核心模块单独成一个静态库，好处有三条：
#    · 命令行版与 GUI 版共用同一份实现，不会出现两套逻辑
#    · GUI 出问题时可以先跑命令行版，确认是逻辑还是界面
#    · 核心模块可以被别的程序直接链接
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(app
    VERSION 1.0.0
    DESCRIPTION "值类型项目示例：自己写一个类，命令行版与 GUI 版共用核心模块"
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
#  核心模块：不依赖 GUI，可单独编译
# ---------------------------------------------------------------
add_library(core STATIC
    src/int_vector.cpp
    src/vector_demo.cpp
)

target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---------------------------------------------------------------
#  字符集设置：中文输出不乱码的关键（与示例 03、04 相同）
#  源码按 UTF-8 读取，窄字符串按 GBK 写入 exe，
#  以匹配中文 Windows 控制台默认的 CP936。
#  GUI 里的宽字符串不受这一项影响（宽字符用 -fwide-exec-charset）。
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
#  GUI 版之一：Win32 API，零依赖（默认构建）
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

# ---------------------------------------------------------------
#  GUI 版之二：Qt Widgets，跨平台（默认不构建）
#
#    WITH_QT 默认 OFF：没装 Qt 的读者照样能构建命令行版与 Win32 版，
#    配置阶段不会因为找不到 Qt 而报错。
#    要用 Qt 版时：
#      $env:QT_ROOT = '<Qt 套件目录>'      # 或 -DQT_ROOT=<目录>
#      cmake -S . -B build/qt -G "Visual Studio 17 2022" -A x64 -DWITH_QT=ON
#      cmake --build build/qt --config Release
#    生成器必须与套件匹配（msvc2022_64 套件用 "Visual Studio 17 2022" 即可），
#    原因见 README 的「Qt 版怎么构建」。
#    构建后会自动调用 windeployqt，把 Qt 的 DLL 拷到 exe 旁边；
#    不需要时用 -DWITH_QT_DEPLOY=OFF 关掉。
#    Qt 从哪来见 工具/获取依赖/README.md。
#
#    qt-dynamic.cmake 从 -DQT_ROOT 或环境变量 QT_ROOT 取路径，
#    并检查那套 Qt 的编译器与当前工程是否匹配；
#    两个都没给时它直接返回，由 find_package 到系统里找。
# ---------------------------------------------------------------
option(WITH_QT "构建 Qt 版界面（需要 Qt 6 Widgets）" OFF)
if(WITH_QT)
    # Qt 套件的接入与编译器匹配检查都在这个文件里：
    # 它读 -DQT_ROOT 或环境变量 QT_ROOT，指向套件目录（编译器那一层，
    # 例如 <Qt>/6.11.1/msvc2022_64），并核对套件与本工程的编译器是否一致。
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../工具/获取依赖/qt-dynamic.cmake")
    find_package(Qt6 COMPONENTS Widgets REQUIRED)
    # 界面文件当前没有用 Q_OBJECT，moc 不是必需的；
    # 读者若自行添加自定义信号，把它打开即可。
    set(CMAKE_AUTOMOC ON)
    add_executable(app_gui_qt WIN32 src/main_gui_qt.cpp)
    target_link_libraries(app_gui_qt PRIVATE core Qt6::Widgets)

    # 这个目标刻意【不传任何字符集选项】：
    #   Qt 6 在 MSVC 下会自动给目标加 /utf-8，而 /utf-8 已经同时定下
    #   源字符集与执行字符集，再传 /source-charset:utf-8 或
    #   /execution-charset:gbk 就是重复指定，编译期直接报
    #     D8016: “/source-charset:utf-8”和“/utf-8”命令行选项不兼容
    #   Qt 的字符串走 QString，本来也不依赖窄字符串的执行字符集；
    #   界面上的固定文字一律写成 QStringLiteral（见 src/main_gui_qt.cpp）。
    if(MSVC)
        target_compile_options(app_gui_qt PRIVATE /EHsc /W4)
    else()
        target_compile_options(app_gui_qt PRIVATE -g -Wall -Wextra)
    endif()

    # MinGW 目标把 GCC 运行时静态链进 exe：这样 exe 自己不再依赖
    # libgcc_s_seh-1.dll 与 libstdc++-6.dll，分发时少两个文件。
    # 注意 Qt 自己的 DLL（Qt6Core.dll 等）仍然依赖这两个 DLL，
    # 因此部署时照样要拷，见下面的部署段。
    if(MINGW)
        target_link_options(app_gui_qt PRIVATE -static-libgcc -static-libstdc++)
    endif()

    # ---------------------------------------------------------------
    #  构建后把 Qt 运行时拷到可执行文件旁边
    #
    #  动态版 Qt 的程序启动时要能找到 Qt6Core.dll、Qt6Gui.dll、
    #  Qt6Widgets.dll 与 platforms\qwindows.dll；找不到就报缺 DLL。
    #  windeployqt 会把它们复制到 exe 同目录，构建完双击即可运行。
    #
    #  静态版 Qt 的程序自带 Qt，不需要这一步，因此检测到静态套件就跳过。
    #
    #  待 工具/获取依赖/qt-dynamic.cmake 提供 qt_enable_deploy() 封装之后，
    #  下面这一段应换成一句 qt_enable_deploy(app_gui_qt)，
    #  开关名沿用 WITH_QT_DEPLOY（默认 ON）。
    # ---------------------------------------------------------------
    option(WITH_QT_DEPLOY "构建后把 Qt 运行时拷到可执行文件旁边" ON)
    if(WITH_QT_DEPLOY AND NOT (DEFINED _qt_shared AND NOT _qt_shared))
        find_program(WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt windeployqt6
            HINTS "${QT_ROOT}/bin" "${Qt6_DIR}/../../../bin")
        if(WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET app_gui_qt POST_BUILD
                # --no-translations 不拷 Qt 自带的 .qm 翻译文件（本示例用不到）
                # 不传 --no-compiler-runtime：编译器运行时是要拷的
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations
                        "$<TARGET_FILE:app_gui_qt>"
                COMMENT "部署 Qt 运行时到 $<TARGET_FILE_DIR:app_gui_qt>"
                VERBATIM)
        else()
            message(WARNING
                "没有找到 windeployqt，Qt 运行时不会自动部署。"
                "把 ${QT_ROOT}/bin 加进 PATH 后重新配置，或手动运行 windeployqt。")
        endif()

        # MinGW 目标还要带上 GCC 的运行时 DLL：libgcc_s_seh-1.dll 等不在
        # Qt 的部署范围内，windeployqt 不会管它们，缺了就报找不着 DLL。
        #
        # 必须与编译这个 exe 的编译器**同源**：不同 GCC 版本的
        # libstdc++-6.dll 混用会出怪问题。因此这里从 CMAKE_CXX_COMPILER
        # 所在目录取，不写死任何本机路径。
        # copy_if_different：文件已存在且一致时自动跳过。
        if(MINGW)
            get_filename_component(_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
            foreach(_runtime_dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
                if(EXISTS "${_mingw_bin}/${_runtime_dll}")
                    add_custom_command(TARGET app_gui_qt POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${_mingw_bin}/${_runtime_dll}"
                                "$<TARGET_FILE_DIR:app_gui_qt>/${_runtime_dll}"
                        COMMENT "复制 MinGW 运行时 ${_runtime_dll}"
                        VERBATIM)
                else()
                    message(STATUS "编译器目录里没有 ${_runtime_dll}，跳过")
                endif()
            endforeach()
        endif()
    endif()
endif()
