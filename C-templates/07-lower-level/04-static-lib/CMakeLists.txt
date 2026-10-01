# ===================================================================
#  练习模板 04 · 静态库与符号（C，多文件，静态库 + 两个驱动）
#
#  两个静态库：
#     sensor_core     core/sensor.c + core/sensor_extra.c + core/sensor_default.c
#                     ← 三个成员，粒度是目标文件
#     sensor_driver   drivers/driver_fast.c
#                     ← 给出强定义 sensor_sample_hook，覆盖库里的兜底实现
#
#  两个可执行文件，源码是同一份 src/main_cli.c：
#     app_default   只链 core，读到的值来自库自带的默认算法
#     app_override  再链 driver，读到的值来自驱动
#
#  命令行验证：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_default.exe
#     build\mingw\bin\app_override.exe
#
#  关掉驱动（观察覆盖失败是静默的）：
#     cmake --preset mingw-gdb -DWITH_DRIVER=OFF
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(static_lib
    VERSION 1.0.0
    DESCRIPTION "练习模板 04 · 静态库：内部链接、成员粒度、弱符号与覆盖"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(WITH_DRIVER "把外部驱动链进 app_override（关掉它，库自带的算法就会顶上来，而且没有报错）" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ---------------------------------------------------------------- 静态库
add_library(sensor_core STATIC
    core/sensor.c
    core/sensor_extra.c
    core/sensor_default.c
)

# 驱动用**对象库**，不用静态库：
# 库对 sensor_sample_hook 的引用是弱引用，而链接器不会为了满足一个弱引用
# 去静态库里抽成员——实测结果是符号仍然是 U（未定义），驱动那一份根本没进来。
# 对象库的目标文件一定会进链接，强定义就在场，弱引用才指得过去。
add_library(sensor_driver OBJECT
    drivers/driver_fast.c
)

target_include_directories(sensor_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(sensor_driver PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---------------------------------------------------------------- 两个可执行文件
add_executable(app_default src/main_cli.c)
target_link_libraries(app_default PRIVATE sensor_core)

add_executable(app_override src/main_cli.c)
target_link_libraries(app_override PRIVATE sensor_core sensor_driver)

# MSVC 没有弱引用属性：core/sensor.c 里的 sensor_sample_hook 是一个普通的未定义符号，
# 这里让链接器把「没人定义」的它引到 core/sensor_default.c 的 sensor_sample_hook_default 上。
# 外部驱动一旦定义了这个名字，替身就用不上了（app_override 走的就是这一条）。
if(MSVC)
    target_link_options(app_default PRIVATE
        "/alternatename:sensor_sample_hook=sensor_sample_hook_default")
endif()

if(NOT WITH_DRIVER)
    message(STATUS "WITH_DRIVER=OFF：app_override 不再链接 drivers/driver_fast.c")
    set_target_properties(app_override PROPERTIES LINK_LIBRARIES "sensor_core")
endif()

# ---------------------------------------------------------------- 编译选项
foreach(target sensor_core sensor_driver app_default app_override)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4)
    else()
        target_compile_options(${target} PRIVATE -g -Wall -Wextra)
    endif()
endforeach()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
