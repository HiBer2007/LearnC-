# ============================================================================
#  qt-static.cmake —— 旧名字，保留仅为兼容
# ============================================================================
#
#  本文件已改名为 qt-dynamic.cmake。Qt 路线从「自建静态版」改为
#  「用官方安装器的动态版」，因此文件名里的 static 不再合适。
#
#  保留这个文件的原因：还有若干范例工程与练习模板写着
#      include(.../工具/获取依赖/qt-static.cmake)
#  直接删掉会让它们立刻配置失败。这里只做转发，不含任何逻辑。
#
#  新写的工程请直接包含 qt-dynamic.cmake。
#  那些工程改完之后，这个文件就可以删掉。
# ============================================================================

# 旧名字用的是 QT_STATIC_ROOT。若使用者仍然设的是旧变量，搬到新名字上。
if(NOT QT_ROOT AND DEFINED QT_STATIC_ROOT AND QT_STATIC_ROOT)
    set(QT_ROOT "${QT_STATIC_ROOT}")
endif()
if(NOT QT_ROOT AND NOT DEFINED ENV{QT_ROOT} AND DEFINED ENV{QT_STATIC_ROOT})
    set(ENV{QT_ROOT} "$ENV{QT_STATIC_ROOT}")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/qt-dynamic.cmake")
