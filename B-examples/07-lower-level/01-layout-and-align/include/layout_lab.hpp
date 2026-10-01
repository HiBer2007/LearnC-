/* layout_lab.hpp —— 示例 07-lower-level/01-layout-and-align 的核心接口
 *
 * 这个头文件管两件事：
 *   第 1 章  六类对象在进程地址空间里各占哪一段
 *   第 2 章  结构体的对齐与填充、缓存行上的伪共享
 *
 * 核心库只依赖 C++ 标准库，不认识任何界面；命令行版把它的结果打成表。
 * 对齐与填充这两件事在 C 与 C++ 里规则相同，本示例用 C++ 写，
 * 原因是伪共享那一段要用 std::thread 起两个线程。
 */
#ifndef LAYOUT_LAB_HPP
#define LAYOUT_LAB_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iosfwd>

namespace ll {

/* ==================== 第 1 章：六类对象的地址 ==================== */

/* 六类对象的地址。存成整数而不是指针，有两个原因：
 *   · 栈上的局部量在函数返回后就没了，把它的地址当指针传出去，
 *     编译器会给出 -Wdangling-pointer 警告——存整数不涉及指针逃逸；
 *   · 打印时本来就要按整数格式化。 */
struct address_map {
    std::uintptr_t code;    /* 函数体的地址      → .text */
    std::uintptr_t literal; /* 字符串字面量的地址 → .rdata / .rodata */
    std::uintptr_t idata;   /* 有初值的全局量     → .data */
    std::uintptr_t udata;   /* 无初值的全局量     → .bss */
    std::uintptr_t heap;    /* new 出来的         → 堆 */
    std::uintptr_t stack;   /* 函数里的局部量     → 栈 */
};

address_map take_address_map();
void print_address_map(std::ostream &os, const address_map &m);

/* ==================== 第 2 章：对齐与填充 ==================== */

/* 同一个三字段结构体的三种写法，用来对照 sizeof 与 offsetof。
 *
 * MSVC 的打包靠 #pragma pack，GCC 与 Clang 靠 __attribute__((packed))，
 * 因此这两种写法包在宏里：LL_PACK_PUSH / LL_PACK_POP 把 pragma 圈起来，
 * LL_PACKED 是 GCC 那边挂在 struct 上的属性。MSVC 下 LL_PACKED 展开为空。 */
#if defined(_MSC_VER)
#  define LL_PACK_PUSH __pragma(pack(push, 1))
#  define LL_PACK_POP __pragma(pack(pop))
#  define LL_PACKED
#else
#  define LL_PACK_PUSH
#  define LL_PACK_POP
#  define LL_PACKED __attribute__((packed))
#endif

struct stat_default {
    std::uint8_t tag;
    std::uint32_t value;
    std::uint8_t flags;
};

LL_PACK_PUSH
struct LL_PACKED stat_packed {
    std::uint8_t tag;
    std::uint32_t value;
    std::uint8_t flags;
};
LL_PACK_POP

struct alignas(16) stat_aligned {
    std::uint8_t tag;
    std::uint32_t value;
    std::uint8_t flags;
};

struct layout_row {
    const char *name;  /* 类型的写法 */
    std::size_t size;  /* sizeof */
    std::size_t align; /* alignof */
};

const layout_row *layout_table(std::size_t *count);
void print_layout_table(std::ostream &os);

struct offset_row {
    const char *field;        /* 字段名 */
    std::size_t default_off;  /* 默认布局里的偏移 */
    std::size_t packed_off;   /* 打包之后的偏移 */
};

const offset_row *offset_table(std::size_t *count);
void print_offset_table(std::ostream &os);

/* ==================== 第 2 章：缓存行与伪共享 ==================== */

/* x86-64 的一级数据缓存行是 64 字节；示例按这个宽度做对齐。 */
inline constexpr std::size_t cache_line_size = 64;

struct counters_shared { /* 两个计数器挤在同一行 */
    std::atomic<std::uint64_t> a;
    std::atomic<std::uint64_t> b;
};

struct counters_padded { /* 两个计数器各自占一行 */
    alignas(cache_line_size) std::atomic<std::uint64_t> a;
    alignas(cache_line_size) std::atomic<std::uint64_t> b;
};

struct sharing_result {
    std::uint64_t iterations; /* 每个线程自增多少次 */
    double ns_shared;         /* 「同一行」版本每次自增的纳秒数 */
    double ns_padded;         /* 「各占一行」版本 */
    std::uint64_t total;      /* 校验值：两个计数器之和，应当是 2 × iterations */
};

sharing_result measure_false_sharing(std::uint64_t iterations);
void print_sharing(std::ostream &os, const sharing_result &r);

/* ==================== 自测 ==================== */

/* 跑完全部检查，把结果写到 os；返回失败的项数（0 表示全部通过）。 */
int run_self_test(std::ostream &os);

} /* namespace ll */

#endif /* LAYOUT_LAB_HPP */
