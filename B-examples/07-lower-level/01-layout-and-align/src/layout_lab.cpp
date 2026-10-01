/* layout_lab.cpp —— 01-layout-and-align 的核心实现
 *
 * 编译：由 CMakeLists.txt 编成静态库 core，不直接编译这个文件。
 * 手工编译：
 *   g++ -std=c++17 -O2 -Wall -Wextra -Iinclude src/layout_lab.cpp src/main_cli.cpp -o app_cli.exe
 */
#include "layout_lab.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <thread>

namespace ll {
namespace {

/* ---------- 第 1 章用的六个对象 ---------- */

/* 有初值的全局量：落在 .data */
std::uint32_t g_with_init = 0x1234ABCDu;
/* 无初值的全局量：落在 .bss */
std::uint32_t g_without_init;
/* 字符串字面量：落在 .rdata（MinGW）或 .rodata */
const char g_literal[] = "layout-and-align";

/* 两个取地址的函数都带 noinline：
 * 否则编译器可能把「取函数地址」折叠成同一个常量，看起来像是没差别。 */
#if defined(_MSC_VER)
#  define LL_NOINLINE __declspec(noinline)
#else
#  define LL_NOINLINE __attribute__((noinline))
#endif

/* 局部量的地址当场转成整数带出来：局部量在函数返回后就没了，
 * 把它的地址当指针往外传会被 -Wdangling-pointer 判为悬垂指针；
 * 整数不涉及指针逃逸，而打印时本来也要按整数格式化。
 *
 * MSVC 仍然会报 C4172（「返回局部变量的地址」），因为它只看「&local 被带出了函数」。
 * 这里确实要的就是那个地址值，不是要去访问它，因此就地关掉这一条。 */
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4172)
#endif
LL_NOINLINE std::uintptr_t stack_address_value(void) {
    volatile std::uint32_t local = 0x5A5A5A5Au;
    return reinterpret_cast<std::uintptr_t>(
        const_cast<const volatile void *>(static_cast<const volatile void *>(&local)));
}
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

/* 堆地址同理：先把地址值取出来，再释放那块内存。这里只关心地址落在哪一段。 */
LL_NOINLINE std::uintptr_t heap_address_value(void) {
    void *p = ::operator new(16);
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(p);
    ::operator delete(p);
    return value;
}

/* 自测用的计数 */
int g_pass;
int g_fail;

void check(std::ostream &os, bool ok, const char *what) {
    if (ok) {
        ++g_pass;
        os << "  [通过] " << g_pass + g_fail << ". " << what << "\n";
    } else {
        ++g_fail;
        os << "  [失败] " << g_pass + g_fail << ". " << what << "\n";
    }
}

const layout_row k_layout[] = {
    {"stat_default", sizeof(stat_default), alignof(stat_default)},
    {"stat_packed", sizeof(stat_packed), alignof(stat_packed)},
    {"stat_aligned", sizeof(stat_aligned), alignof(stat_aligned)},
    {"counters_shared", sizeof(counters_shared), alignof(counters_shared)},
    {"counters_padded", sizeof(counters_padded), alignof(counters_padded)},
};

const offset_row k_offsets[] = {
    {"tag", offsetof(stat_default, tag), offsetof(stat_packed, tag)},
    {"value", offsetof(stat_default, value), offsetof(stat_packed, value)},
    {"flags", offsetof(stat_default, flags), offsetof(stat_packed, flags)},
};

/* 一次「两个线程各撞一个计数器」的计时。
 * Pair 决定两个计数器是挤在一行还是各占一行，其余完全相同。 */
template <typename Pair>
double bump_pair(std::uint64_t iterations, std::uint64_t *total) {
    static Pair counters; /* 放在静态区：链接器保证 alignas 生效 */

    counters.a.store(0, std::memory_order_relaxed);
    counters.b.store(0, std::memory_order_relaxed);

    auto work = [](std::atomic<std::uint64_t> &c, std::uint64_t n) {
        for (std::uint64_t i = 0; i < n; ++i) {
            c.fetch_add(1, std::memory_order_relaxed);
        }
    };

    const auto t0 = std::chrono::steady_clock::now();
    std::thread t1(work, std::ref(counters.a), iterations);
    std::thread t2(work, std::ref(counters.b), iterations);
    t1.join();
    t2.join();
    const auto t1_done = std::chrono::steady_clock::now();

    *total = counters.a.load() + counters.b.load();
    const double ns =
        std::chrono::duration<double, std::nano>(t1_done - t0).count();
    return ns / static_cast<double>(iterations);
}

} /* namespace */

/* ==================== 第 1 章 ==================== */

address_map take_address_map() {
    address_map m{};
    m.code = reinterpret_cast<std::uintptr_t>(&take_address_map);
    m.literal = reinterpret_cast<std::uintptr_t>(g_literal);
    m.idata = reinterpret_cast<std::uintptr_t>(&g_with_init);
    m.udata = reinterpret_cast<std::uintptr_t>(&g_without_init);
    m.heap = heap_address_value();
    m.stack = stack_address_value();
    return m;
}

void print_address_map(std::ostream &os, const address_map &m) {
    const struct {
        const char *name;
        std::uintptr_t addr;
        const char *section;
    } rows[] = {
        {"函数体       ", m.code, ".text"},
        {"字符串字面量 ", m.literal, ".rdata / .rodata"},
        {"有初值全局量 ", m.idata, ".data"},
        {"无初值全局量 ", m.udata, ".bss"},
        {"new 出来的   ", m.heap, "堆"},
        {"局部量       ", m.stack, "栈"},
    };
    os << "== 六类对象各在哪一段 ==\n";
    for (const auto &r : rows) {
        os << "  " << r.name << " = 0x" << std::hex << r.addr << std::dec
           << "   " << r.section << "\n";
    }
    os << "  说明：映像那四段由链接器摆在同一个基址上，栈与堆由系统另外分配。\n"
          "        谁高谁低完全由操作系统与链接器决定，标准不做任何保证。\n"
          "        本机这一次是「栈 < 堆 < 映像」，换个平台就可能换一个次序。\n";
}

/* ==================== 第 2 章 ==================== */

const layout_row *layout_table(std::size_t *count) {
    *count = sizeof(k_layout) / sizeof(k_layout[0]);
    return k_layout;
}

void print_layout_table(std::ostream &os) {
    std::size_t n = 0;
    const layout_row *rows = layout_table(&n);
    os << "== 结构体的大小与对齐 ==\n";
    os << "  类型                sizeof  alignof\n";
    for (std::size_t i = 0; i < n; ++i) {
        os << "  " << std::left << std::setw(18) << rows[i].name << std::right
           << std::setw(6) << rows[i].size << std::setw(9) << rows[i].align
           << "\n";
    }
    os << "  stat_default  1+4+1，为了对齐 value 补 3 字节、尾部再补 3 字节 → 12\n"
          "  stat_packed   打包之后没有填充 → 6，代价是 value 变成非对齐访问\n"
          "  stat_aligned  alignas(16) 把整个结构体钉到 16 字节边界 → 16\n";
}

const offset_row *offset_table(std::size_t *count) {
    *count = sizeof(k_offsets) / sizeof(k_offsets[0]);
    return k_offsets;
}

void print_offset_table(std::ostream &os) {
    std::size_t n = 0;
    const offset_row *rows = offset_table(&n);
    os << "== 字段偏移（offsetof）==\n";
    os << "  字段    默认布局  打包之后\n";
    for (std::size_t i = 0; i < n; ++i) {
        os << "  " << std::left << std::setw(8) << rows[i].field << std::right
           << std::setw(8) << rows[i].default_off << std::setw(10)
           << rows[i].packed_off << "\n";
    }
    os << "  默认布局里 value 前面有 3 字节填充，打包之后紧挨着 tag。\n"
          "  填充不是浪费：它让每个字段落在自己对齐要求的位置上，\n"
          "  处理器取一次就能拿到；拿掉填充，取值要多走几条指令。\n";
}

/* ==================== 缓存行与伪共享 ==================== */

sharing_result measure_false_sharing(std::uint64_t iterations) {
    sharing_result r{};
    r.iterations = iterations;
    std::uint64_t total_shared = 0;
    std::uint64_t total_padded = 0;
    r.ns_shared = bump_pair<counters_shared>(iterations, &total_shared);
    r.ns_padded = bump_pair<counters_padded>(iterations, &total_padded);
    r.total = total_shared + total_padded; /* 两次相加 = 4 × iterations */
    return r;
}

void print_sharing(std::ostream &os, const sharing_result &r) {
    os << "== 伪共享：两个线程各撞一个计数器 ==\n";
    os << "  迭代次数         : " << r.iterations << " × 2 个计数器\n";
    os << "  同一缓存行       : " << std::fixed << std::setprecision(3)
       << r.ns_shared << " ns/次\n";
    os << "  各占一条缓存行   : " << r.ns_padded << " ns/次\n";
    os << "  校验值 a+b       : " << std::defaultfloat << r.total << "（应当是 "
       << (r.iterations * 4) << "）\n";
    os << "  两个计数器地址相距 " << offsetof(counters_padded, b) - offsetof(counters_padded, a)
       << " 字节（同一行版本只相距 "
       << offsetof(counters_shared, b) - offsetof(counters_shared, a) << " 字节）。\n";
    os << "  同一行时两个核轮流把整行标脏，缓存行在两核之间来回搬，\n"
          "  这就是伪共享：数据本身没有共享，缓存行被共享了。\n";
}

/* ==================== 自测 ==================== */

int run_self_test(std::ostream &os) {
    g_pass = 0;
    g_fail = 0;
    os << "== 自测 ==\n";

    /* --- 地址空间 --- */
    const address_map m = take_address_map();
    check(os, m.code != 0u && m.literal != 0u && m.idata != 0u &&
                  m.udata != 0u && m.heap != 0u && m.stack != 0u,
          "六类对象的地址都取到了");
    check(os,
          m.code != m.literal && m.code != m.idata && m.udata != m.idata &&
              m.heap != m.idata && m.stack != m.heap,
          "六类对象的地址互不相同");

    /* 函数与字符串字面量同属一个映像，地址差不会太远；
     * 栈与堆由系统另外分配，不与映像比较——它们的相对位置由操作系统决定。 */
    const std::uintptr_t img_gap = m.code > m.literal ? m.code - m.literal : m.literal - m.code;
    check(os, img_gap < (16u * 1024u * 1024u),
          "函数体与字符串字面量在同一个映像里（地址差 < 16 MiB）");

    /* .data 与 .bss 都是静态区，两个地址之间的距离应当很小 */
    const std::uintptr_t static_gap = m.idata > m.udata ? m.idata - m.udata : m.udata - m.idata;
    check(os, static_gap < (64u * 1024u),
          "有初值与无初值的全局量在同一个静态区（地址差 < 64 KiB）");

    /* --- 结构体布局 --- */
    check(os, sizeof(stat_default) == 12,
          "stat_default 有 6 字节填充，sizeof 是 12");
    check(os, sizeof(stat_packed) == 6,
          "stat_packed 无填充，sizeof 是 6（1 + 4 + 1）");
    check(os, sizeof(stat_aligned) == 16,
          "stat_aligned 由 alignas(16) 撑到 16 字节");
    check(os, alignof(stat_default) == 4,
          "stat_default 的对齐是 4（由 uint32_t 决定）");
    check(os, alignof(stat_packed) == 1,
          "stat_packed 的对齐降到 1");
    check(os, alignof(stat_aligned) == 16,
          "stat_aligned 的对齐是 16");
    check(os, offsetof(stat_default, value) == 4,
          "默认布局里 value 在偏移 4");
    check(os, offsetof(stat_default, flags) == 8,
          "默认布局里 flags 在偏移 8");
    check(os, offsetof(stat_packed, value) == 1,
          "打包之后 value 在偏移 1");
    check(os, offsetof(stat_packed, flags) == 5,
          "打包之后 flags 在偏移 5");

    /* --- 缓存行 --- */
    check(os, offsetof(counters_shared, b) - offsetof(counters_shared, a) == 8,
          "不填充时两个计数器相距 8 字节，落在同一条缓存行");
    check(os, offsetof(counters_padded, b) - offsetof(counters_padded, a) == cache_line_size,
          "alignas(64) 之后两个计数器相距 64 字节，分属两条缓存行");
    check(os, sizeof(counters_padded) == 2 * cache_line_size,
          "counters_padded 的大小是两条缓存行");

    /* --- 伪共享：只校验结果正确，不断言时间 --- */
    const sharing_result r = measure_false_sharing(200000);
    check(os, r.total == 4u * 200000u,
          "两种布局下两个计数器的自增一次都没丢（结果与线程数无关）");
    check(os, r.ns_shared > 0.0 && r.ns_padded > 0.0,
          "两次计时都拿到了非零的纳秒数");

    os << "\n  自测结果：" << (g_pass + g_fail) << " 项中 " << g_pass
       << " 项通过";
    if (g_fail == 0) {
        os << "，全部通过\n";
    } else {
        os << "，" << g_fail << " 项失败\n";
    }
    return g_fail;
}

} /* namespace ll */
