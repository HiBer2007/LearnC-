# ============================================================================
#  qt-dynamic.cmake —— 把范例工程接到一套**动态版** Qt 上
# ============================================================================
#
#  用法（范例工程的 CMakeLists.txt）：
#
#      option(WITH_QT "构建 Qt 版界面" OFF)
#      if(WITH_QT)
#        include(${CMAKE_CURRENT_SOURCE_DIR}/../../工具/获取依赖/qt-dynamic.cmake)
#        find_package(Qt6 COMPONENTS Widgets REQUIRED)
#      endif()
#
#  本文件只做三件事：
#    1. 找出 Qt 的安装套件目录
#    2. 检查它与当前工程用的是不是同一条工具链
#    3. 把 CMAKE_PREFIX_PATH 与 Qt6_DIR 指向它
#
#  它**不调用 find_package**，是否找、找哪些组件由调用方决定。
#
#  ---------------------------------------------------------------------------
#  Qt 的目录从哪来
#  ---------------------------------------------------------------------------
#  按优先级依次尝试：
#
#    1. CMake 变量  -DQT_ROOT=<路径>
#    2. 环境变量    QT_ROOT
#    3. 都不给      —— 本文件不做任何事，交给 find_package 自己去系统里找
#
#  路径要指向**套件目录**，也就是编译器那一层，例如
#  <Qt>\6.11.1\mingw_64，而不是 <Qt>\6.11.1。各套件的区别见
#  《工具/获取依赖/README.md》第三节。
#
#  **仓库里不写死任何本机路径。** 谁把 Qt 装在哪里，与别人无关。
#
#  ---------------------------------------------------------------------------
#  一条硬性匹配规则
#  ---------------------------------------------------------------------------
#  **Qt 的套件是用哪个编译器编的，范例就必须用哪个编译器构建。**
#  C++ 没有跨编译器的稳定 ABI：MSVC 编出来的 .lib 不能被 MinGW 链接，
#  反之亦然。混用会在链接期报出一大片未解析符号。
#  本文件从目录名与 qconfig.pri 判断套件属于哪条工具链，不符时直接报错。
#
#  ---------------------------------------------------------------------------
#  动态版与静态版在构建期的区别
#  ---------------------------------------------------------------------------
#  动态版没有静态版那些额外约束：Debug 与 Release 都能配，
#  也不要求某个特定的生成器版本（msvc2022_64 套件用 Visual Studio 17 2022
#  生成器即可）。动态版额外要注意的是**运行期**要能找到 DLL，
#  见 README 第 3.4 小节。
# ============================================================================

# ============================================================================
#  部署：让 exe 自己带着 Qt 跑
# ============================================================================
#
#  动态版的程序启动时要能找到 Qt6Core.dll、Qt6Gui.dll、Qt6Widgets.dll
#  以及 platforms/qwindows.dll。找不到就直接起不来——进程还在，界面不出现。
#  平台插件那一个最容易漏，漏了会报
#      This application failed to start because no Qt platform plugin
#      could be initialized
#
#  下面这个函数把这件事接到构建上：构建完成后自动把 Qt 的运行时拷到 exe 旁边，
#  于是双击 exe 就能跑，拷到别的目录也能跑。
#
#  用法（在 add_executable 与 target_link_libraries 之后调用一次）：
#
#      qt_enable_deploy(app_gui_qt)
#
#  三个开关：
#
#      -DWITH_QT_DEPLOY=OFF                    关掉自动部署（默认 ON）
#      -DQT_DEPLOY_COMPILER_RUNTIME=OFF        不拷编译器运行时（默认 ON，见下）
#      -DQT_MINGW_STATIC_GCC_RUNTIME=ON        把 GCC 运行时静态链进 exe（默认 OFF）
#
#  ---------------------------------------------------------------------------
#  MinGW 版的 exe 还需要三个 DLL
#  ---------------------------------------------------------------------------
#  用 MinGW 编出来的 exe，除 Qt 的 DLL 之外还依赖三个运行库：
#
#      libgcc_s_seh-1.dll     异常处理运行时（32 位 MinGW 上叫 libgcc_s_dw2-1.dll）
#      libstdc++-6.dll        C++ 标准库
#      libwinpthread-1.dll    线程与互斥支持
#
#  这三个是**编译器**的东西，不是 Qt 的东西，因此 windeployqt 未必会带上。
#  少了它们的症状是程序根本起不来，报缺 libgcc_s_seh-1.dll 之类。
#
#  本函数默认（QT_DEPLOY_COMPILER_RUNTIME=ON）会显式从
#  **编这个工程的那个编译器**的 bin 目录把这三个拷过来：
#
#      get_filename_component(_cxx_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
#
#  **运行时必须与编译器同源。** 从 Qt 自带的 Tools\mingw1310_64 里拷一份
#  给 GCC 15.2 编出来的 exe 用，属于混用不同版本的 libstdc++，
#  轻则行为古怪，重则崩溃。
#
#  ---------------------------------------------------------------------------
#  另一条路：把 GCC 运行时静态链进 exe
#  ---------------------------------------------------------------------------
#  也可以让 exe 根本不依赖这三个 DLL：
#
#      qt_link_static_gcc_runtime(app_gui_qt)      # 等价于全局打开下面的开关
#
#  或对全部目标生效：
#
#      -DQT_MINGW_STATIC_GCC_RUNTIME=ON
#
#  代价与取舍见 README 第 3.5 小节：exe 会变大，且进程里可能同时存在
#  两份 libstdc++（exe 里一份静态的，Qt 的 DLL 用一份动态的）。
#  因此本文件把它做成**可选**，默认走「拷 DLL」那条。
# ============================================================================

option(WITH_QT_DEPLOY "构建后自动把 Qt 运行时部署到可执行文件旁边" ON)
option(QT_DEPLOY_COMPILER_RUNTIME "部署时把编译器运行时也拷到可执行文件旁" ON)
option(QT_MINGW_STATIC_GCC_RUNTIME "MinGW：把 GCC 运行时静态链进可执行文件" OFF)

# ----------------------------------------------------------------------------
#  qt_link_static_gcc_runtime(target)
#
#  让 MinGW 编出来的 exe 不再依赖 libgcc_s_*.dll 与 libstdc++-6.dll。
#  必须在 add_executable 之后调用。
# ----------------------------------------------------------------------------
function(qt_link_static_gcc_runtime target)
    if(NOT MINGW)
        message(STATUS
            "qt_link_static_gcc_runtime：当前编译器是 ${CMAKE_CXX_COMPILER_ID}，"
            "不是 MinGW，本函数不做任何事。")
        return()
    endif()
    target_link_options(${target} PRIVATE -static-libgcc -static-libstdc++)
    message(STATUS "${target}：GCC 运行时改为静态链接（-static-libgcc -static-libstdc++）。")
endfunction()

function(qt_enable_deploy target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "qt_enable_deploy：没有名为 ${target} 的目标。"
            "请在 add_executable 之后调用它。")
    endif()

    if(NOT WITH_QT)
        # 没开 Qt 路线时这个函数不该被调用；静默跳过，免得成了绊脚石。
        return()
    endif()

    if(NOT WITH_QT_DEPLOY)
        message(STATUS "WITH_QT_DEPLOY=OFF：${target} 不自动部署 Qt 运行时，"
            "运行时需要自己把 Qt 的 bin 目录加进 PATH。")
        return()
    endif()

    # 静态版不需要部署：Qt 的代码已经在 exe 里了。
    if(DEFINED _qt_shared AND NOT _qt_shared)
        message(STATUS "${target}：这套 Qt 是静态版，不需要部署。")
        return()
    endif()

    # ---------------------------------------------------------------- 找 windeployqt
    # 其一：Qt 6.3 起提供的导入目标，最可靠。
    if(TARGET Qt6::windeployqt)
        set(_wd "$<TARGET_FILE:Qt6::windeployqt>")
    else()
        # 其二：从 Qt6_DIR 反推。<Qt>/lib/cmake/Qt6 往上三级就是 <Qt>。
        set(_wd "")
        if(Qt6_DIR)
            get_filename_component(_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
            set(_cand "${_qt_prefix}/bin/windeployqt${CMAKE_EXECUTABLE_SUFFIX}")
            if(EXISTS "${_cand}")
                set(_wd "${_cand}")
            endif()
        endif()
        # 其三：退回 QT_ROOT。
        if(NOT _wd AND QT_ROOT)
            set(_cand "${QT_ROOT}/bin/windeployqt${CMAKE_EXECUTABLE_SUFFIX}")
            if(EXISTS "${_cand}")
                set(_wd "${_cand}")
            endif()
        endif()
    endif()

    if(NOT _wd)
        message(WARNING
            "qt_enable_deploy：找不到 windeployqt，${target} 不会自动部署。\n"
            "运行前请把 Qt 的 bin 目录加进 PATH，或手工把 Qt 的 DLL 与\n"
            "platforms 目录拷到可执行文件旁边。")
        return()
    endif()

    # ---------------------------------------------------------------- 静态链接 GCC 运行时
    # 打开全局开关时，先把这个目标改成静态链接 GCC 运行时，
    # 后面就不必再拷 libgcc / libstdc++ 了。
    if(QT_MINGW_STATIC_GCC_RUNTIME)
        qt_link_static_gcc_runtime(${target})
    endif()

    # ---------------------------------------------------------------- 第一步：Qt 自己那部分
    # 不传 --no-compiler-runtime：让 windeployqt 按它自己的判断处理。
    # 编译器运行时由下面第二步显式负责，那一步的来源更可靠。
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${_wd}" --no-translations "$<TARGET_FILE:${target}>"
        COMMENT "部署 Qt 运行时到 $<TARGET_FILE_DIR:${target}>"
        VERBATIM)

    # ---------------------------------------------------------------- 第二步：编译器运行时
    # 显式从「编这个工程的那个编译器」的 bin 目录拷。
    # 这三个 DLL 是 MinGW 的，不是 Qt 的，windeployqt 未必带上；
    # 少了它们，程序在没装编译器的机器上根本起不来。
    #
    # 已经静态链接 GCC 运行时的目标不需要这一步。
    if(QT_DEPLOY_COMPILER_RUNTIME AND NOT (MINGW AND QT_MINGW_STATIC_GCC_RUNTIME))
        if(MINGW)
            get_filename_component(_cxx_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
            set(_mingw_runtime_dlls
                libgcc_s_seh-1.dll      # 异常处理，64 位 SEH
                libgcc_s_dw2-1.dll      # 异常处理，32 位 DWARF
                libstdc++-6.dll         # C++ 标准库
                libwinpthread-1.dll)    # 线程与互斥
            foreach(_dll IN LISTS _mingw_runtime_dlls)
                if(EXISTS "${_cxx_bin}/${_dll}")
                    # copy_if_different：已经一致就不写盘，避免每次构建都动文件。
                    add_custom_command(TARGET ${target} POST_BUILD
                        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                                "${_cxx_bin}/${_dll}" "$<TARGET_FILE_DIR:${target}>"
                        COMMENT "拷贝编译器运行时 ${_dll}"
                        VERBATIM)
                endif()
            endforeach()
        else()
            message(STATUS
                "${target}：编译器是 ${CMAKE_CXX_COMPILER_ID}，"
                "运行库不在 Qt 的 bin 里。分发到没装运行库的机器时，"
                "请让目标机器安装对应的运行库（MSVC 即 VC 运行库）。")
        endif()
    endif()
endfunction()

# ============================================================================
#  下面是接线：找到 Qt 并把 CMake 指过去
# ============================================================================

set(QT_ROOT "" CACHE PATH "Qt 安装的套件目录（留空则读环境变量 QT_ROOT）")

if(NOT QT_ROOT)
    set(QT_ROOT "$ENV{QT_ROOT}")
endif()

if(NOT QT_ROOT)
    # 没有指定就什么都不做，把选择权交回 find_package。
    return()
endif()

get_filename_component(QT_ROOT "${QT_ROOT}" ABSOLUTE)

# ---------------------------------------------------------------- 目录检查
if(NOT EXISTS "${QT_ROOT}")
    message(FATAL_ERROR
        "QT_ROOT 指向的目录不存在：\n  ${QT_ROOT}\n"
        "请改成你那套 Qt 的**套件目录**，例如 <Qt>/6.11.1/mingw_64。")
endif()

if(NOT EXISTS "${QT_ROOT}/lib/cmake/Qt6/Qt6Config.cmake")
    message(FATAL_ERROR
        "${QT_ROOT} 存在，但其中没有 lib/cmake/Qt6/Qt6Config.cmake，\n"
        "它不是一套 Qt 的套件目录。常见错误是指到了版本号那一层——\n"
        "请再往里进一层，指到具体套件，例如 <Qt>/6.11.1/mingw_64。")
endif()

# ---------------------------------------------------------------- 读 qconfig.pri
# Qt 安装自带的配置总结，动态还是静态、什么版本、哪个编译器编的，都在里面。
set(_qt_qconfig_file "${QT_ROOT}/mkspecs/qconfig.pri")
if(EXISTS "${_qt_qconfig_file}")
    file(READ "${_qt_qconfig_file}" _qt_qconfig)

    if(_qt_qconfig MATCHES "QT_CONFIG \\+=.*shared")
        set(_qt_shared TRUE)
    elseif(_qt_qconfig MATCHES "QT_CONFIG \\+=.*static")
        set(_qt_shared FALSE)
    endif()

    if(_qt_qconfig MATCHES "QT_VERSION = ([0-9.]+)")
        set(_qt_version "${CMAKE_MATCH_1}")
    endif()
    if(_qt_qconfig MATCHES "QT_ARCH = ([A-Za-z0-9_]+)")
        set(_qt_arch "${CMAKE_MATCH_1}")
    endif()

    if(_qt_qconfig MATCHES "QT_MSVC_MAJOR_VERSION = ([0-9]+)")
        set(_qt_built_with_msvc TRUE)
    else()
        set(_qt_built_with_msvc FALSE)
    endif()
endif()

# ---------------------------------------------------------------- 套件判断
# 目录名是 Qt 安装器给套件起的名字，直接标出了目标工具链。
get_filename_component(_qt_kit "${QT_ROOT}" NAME)

set(_qt_kit_is_msvc FALSE)
if(_qt_kit MATCHES "^msvc")
    set(_qt_kit_is_msvc TRUE)
elseif(_qt_kit MATCHES "^(llvm-)?mingw")
    set(_qt_kit_is_msvc FALSE)
elseif(DEFINED _qt_built_with_msvc)
    # 目录名不是常见形态时，退回到 qconfig.pri 的判断。
    set(_qt_kit_is_msvc ${_qt_built_with_msvc})
endif()

# ---------------------------------------------------------------- 编译器匹配
if(_qt_kit_is_msvc AND NOT MSVC)
    message(FATAL_ERROR
        "编译器不匹配。\n"
        "  套件 ${_qt_kit}（Qt ${_qt_version}）是 MSVC 编的，\n"
        "  而当前工程用的是 ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}。\n"
        "C++ 没有跨编译器的稳定 ABI，MSVC 的 .lib 不能被 MinGW 链接。\n"
        "两个办法：改用 MSVC 生成器配置本工程，或把 QT_ROOT 换成 MinGW 套件\n"
        "（例如 <Qt>/<版本>/mingw_64）。")
endif()

if(NOT _qt_kit_is_msvc AND MSVC)
    message(FATAL_ERROR
        "编译器不匹配。\n"
        "  套件 ${_qt_kit}（Qt ${_qt_version}）是 MinGW 编的，\n"
        "  而当前工程用的是 MSVC。\n"
        "两个办法：改用 MinGW 生成器配置本工程，或把 QT_ROOT 换成 MSVC 套件\n"
        "（例如 <Qt>/<版本>/msvc2022_64）。")
endif()

# ---------------------------------------------------------------- 交给 find_package
# find_package 先查 Qt6_DIR，再沿 CMAKE_PREFIX_PATH 找。两个都设上，
# 免得环境里另一套 Qt 抢先被找到。
list(PREPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
set(Qt6_DIR "${QT_ROOT}/lib/cmake/Qt6" CACHE PATH "Qt6 的 CMake 包目录" FORCE)

set(_qt_kind "未知")
if(DEFINED _qt_shared)
    if(_qt_shared)
        set(_qt_kind "动态版")
    else()
        set(_qt_kind "静态版")
    endif()
endif()

message(STATUS "Qt ${_qt_version}（${_qt_arch}，${_qt_kind}，套件 ${_qt_kit}）：${QT_ROOT}")

if(DEFINED _qt_shared AND NOT _qt_shared)
    message(STATUS
        "提示：这套 Qt 是静态版。静态版编出来的程序是单文件，但受 LGPLv3 的"
        "重链接义务约束，且 Debug/Release 必须与 Qt 一致。见 README 第六节。")
endif()
