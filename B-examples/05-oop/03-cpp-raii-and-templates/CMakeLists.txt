# ===================================================================
#  示例 09 · 小库与使用者（RAII + 模板）
#
#    mini   小库：RAII 的文件句柄、定容容器模板、几个函数模板与全特化
#    app    使用者：链接 mini，写数据文件、读回、统计、自测
#
#  库与使用者分开的三个理由：
#    · 库那部分不打印任何东西，也不认识 main
#    · 使用者只通过头文件里的接口用它，改实现不必重新编译使用者
#    · 静态库可以原样拿到别的工程里去用
# ===================================================================
cmake_minimum_required(VERSION 3.16)

project(app
    VERSION 1.0.0
    DESCRIPTION "RAII 与模板示例：一个小库加一个使用者"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # 本示例要用 fopen / fclose 演示 RAII。
    # MSVC 把它们标成「不安全」并给 C4996，这里是刻意使用，因此关掉该警告。
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------
#  小库：RAII 包装 + 模板。模板在头文件里，全特化在 .cpp 里
# ---------------------------------------------------------------
add_library(mini STATIC
    src/file_handle.cpp
    src/fixed_vector.cpp
)

target_include_directories(mini PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(MSVC)
    target_compile_options(mini PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(mini PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()

# ---------------------------------------------------------------
#  使用者：只包含 mini 的头文件，链接 mini
# ---------------------------------------------------------------
add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE mini)
if(MSVC)
    target_compile_options(app PRIVATE /source-charset:utf-8 /execution-charset:gbk /W4)
else()
    target_compile_options(app PRIVATE -g -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
