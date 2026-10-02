# ===================================================================
#  练习模板 02 · 内存映射 I/O 实验（C，多文件，命令行 + QEMU 交叉实验）
#
#  一个主机侧目标：app_cli
#     src/main_cli.c    命令行验收程序，已写好
#     src/gpio_lab.c    阶段 1、2、4 的 TODO
#     src/regs.c        模拟的寄存器区与访问计数（素材，不要改）
#
#  一个 Cortex-M 侧实验（在 WSL 里手动交叉编译，不经过 CMake）：
#     arm/volatile_poll.c  阶段 3：volatile 有/无两个版本
#     arm/run_qemu.sh      两个版本各编一次、各跑一次
#
#  命令行验证：
#     cmake --preset mingw-gdb
#     cmake --build --preset mingw-gdb
#     build\mingw\bin\app_cli.exe
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(mmio_lab
    VERSION 1.0.0
    DESCRIPTION "练习模板 02 · 内存映射 I/O：位操作、一次写完成、volatile 对照"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(app_cli
    src/main_cli.c
    src/gpio_lab.c
    src/regs.c
)

target_include_directories(app_cli
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(app_cli PRIVATE /utf-8 /W4)
else()
    target_compile_options(app_cli PRIVATE -g -Wall -Wextra)
endif()
