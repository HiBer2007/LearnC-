# ===================================================================
#  示例 06-standard-library/02-cpp-io-report · iostream 报表生成器
#
#    core           纯逻辑，不含任何界面代码
#    app_cli        命令行版，链接 core
#    app_gui_win32  GUI 版（Win32 API，零依赖，默认构建）
#    app_gui_qt     GUI 版（Qt Widgets，-DWITH_QT=ON 才构建）
#
#  核心模块单独成静态库：命令行版与两份界面共用同一份实现，
#  于是三边出同一份报表，也与 01-c-stdlib-toolbox 的 C 版逐字节相同。
#  把两个界面文件删掉，core 照样能编译、能自测通过。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(cpp_io_report
    VERSION 1.0.0
    DESCRIPTION "C++ 报表生成器：iostream / iomanip / sstream / fstream 出与 C 版相同的报表"
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
    src/sales_report.cpp
)

target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---------------------------------------------------------------
#  字符集：中文输出不乱码的关键
#  源码按 UTF-8 读，窄字符串按 GBK 写进 exe，
#  以匹配中文 Windows 控制台默认的 CP936。
#
#  报表的列宽全靠这一项才与 01 的 C 版对得上：std::setw(16) 按字符数补齐，
#  而 GBK 下一个汉字正好 2 字节、也是 2 个字符，与 "%-16s" 结果相同。
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
#  GUI 版之二：Qt Widgets（默认 OFF，不装 Qt 也能构建）
#
#    WITH_QT 默认 OFF：没装 Qt 的读者照样能构建命令行版与 Win32 版，
#    配置阶段不会因为找不到 Qt 而报错。
#
#    qt-dynamic.cmake 从 -DQT_ROOT 或环境变量 QT_ROOT 取套件目录，
#    并检查那套 Qt 的编译器与当前工程是否匹配；两者都没给时它直接返回，
#    由 find_package 到系统里找。路径从本文件出发往上三层是仓库根。
#
#    Qt 目标刻意【不传任何字符集选项】：
#    Qt 6 在 MSVC 下自己会加 /utf-8，再传 /source-charset:utf-8 或
#    /execution-charset:gbk 就是重复指定，编译期直接报
#      D8016: “/source-charset:utf-8”和“/utf-8”命令行选项不兼容
#    界面上的固定文字一律写成 QStringLiteral（见 src/main_gui_qt.cpp）。
# ---------------------------------------------------------------
option(WITH_QT "构建 Qt 版界面（需要 Qt 6 Widgets）" OFF)
if(WITH_QT)
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../../工具/获取依赖/qt-dynamic.cmake")
    find_package(Qt6 COMPONENTS Widgets REQUIRED)

    # 界面文件当前没有用 Q_OBJECT，moc 不是必需的；
    # 读者若自行添加自定义信号，把它打开即可。
    set(CMAKE_AUTOMOC ON)
    add_executable(app_gui_qt WIN32 src/main_gui_qt.cpp)
    target_link_libraries(app_gui_qt PRIVATE core Qt6::Widgets)
    if(MSVC)
        target_compile_options(app_gui_qt PRIVATE /EHsc /W4)
    else()
        target_compile_options(app_gui_qt PRIVATE -g -Wall -Wextra)
    endif()

    # 构建完自动把 Qt 运行时与 MinGW 运行时拷到 exe 旁边
    qt_enable_deploy(app_gui_qt)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
