# ===================================================================
#  示例 10 · Qt 界面（动态版 Qt）
#
#  这个示例只做一件事：把 Qt 界面路线跑通。
#  Qt 是可选的，WITH_QT 默认为 OFF；不打开时本工程什么都不构建，
#  也不会因为找不到 Qt 而报错。
#
#  为什么没有 Win32 版：主线 GUI 示例（07、08）与练习模板一律给
#  「Win32 + Qt」两份界面；本示例的用途是演示 Qt 接入本身，
#  因此只有 Qt 一份。两条路线的取舍见
#  《工具/获取依赖/README.md》第二节。
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(qt_gui
    VERSION 1.0.0
    DESCRIPTION "Qt 界面示例（动态版 Qt）"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------
#  WITH_QT 默认 OFF
#
#  不打开时不做任何与 Qt 有关的事。
#  打开时才去找 Qt；找不到就报错退出，而不是悄悄降级。
#
#  qt-dynamic.cmake 从 -DQT_ROOT 或环境变量 QT_ROOT 取 Qt 的套件目录，
#  并检查它的编译器与当前工程是否匹配。
# ---------------------------------------------------------------
option(WITH_QT "构建 Qt 版界面" OFF)

if(WITH_QT)
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../工具/获取依赖/qt-dynamic.cmake")
    find_package(Qt6 COMPONENTS Widgets REQUIRED)

    # WIN32 关键字让链接器走图形子系统，启动时不弹出控制台窗口。
    add_executable(app_gui_qt WIN32 src/main.cpp)
    target_link_libraries(app_gui_qt PRIVATE Qt6::Widgets)

    # 这里刻意不加任何字符集选项。
    # MSVC 下 Qt 6 会自动给目标加 /utf-8，再补 /source-charset:utf-8
    # 或 /execution-charset:gbk 就是重复指定，编译期直接报
    #   D8016: “/source-charset:utf-8”和“/utf-8”命令行选项不兼容
    # MinGW 下同理不要加 -fexec-charset=GBK。
    # Qt 的字符串走 QString，本来也不依赖窄字符串的执行字符集。

    # ---------------------------------------------------------
    #  动态版特有：把 Qt 的运行时拷到 exe 旁边
    #
    #  不加这一步也能跑，但必须从 Qt 的 bin 目录启动，
    #  或者先把 <Qt>\bin 加进 PATH。加了之后双击 exe 就能跑，
    #  整个目录拷到别的机器上也还能跑。
    #
    #  函数的定义在 qt-dynamic.cmake 里，用法与开关见
    #  《工具/获取依赖/README.md》第 3.5 小节。
    #
    #    -DWITH_QT_DEPLOY=OFF             关掉自动部署（默认 ON）
    #    -DQT_DEPLOY_COMPILER_RUNTIME=ON  连 C/C++ 运行库一起拷（默认 OFF）
    # ---------------------------------------------------------
    qt_enable_deploy(app_gui_qt)
else()
    message(STATUS "WITH_QT=OFF，Qt 版界面不构建。")
    message(STATUS "要构建它：-DWITH_QT=ON 并设好 QT_ROOT，见同目录 README.md。")
endif()
