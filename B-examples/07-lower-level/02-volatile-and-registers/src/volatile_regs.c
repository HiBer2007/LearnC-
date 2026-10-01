/* volatile_regs.c —— 02-volatile-and-registers 的核心实现
 *
 * 编译：由 CMakeLists.txt 编成静态库 core，不直接编译这个文件。
 * 手工编译：
 *   gcc -std=c17 -O2 -Wall -Wextra -Iinclude src/volatile_regs.c src/main_cli.c -o app_cli.exe
 *
 * 单独看汇编（本示例的重点）：
 *   gcc -std=c17 -O0 -Wall -Wextra -Iinclude -c src/volatile_regs.c -o vr-O0.o
 *   gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src/volatile_regs.c -o vr-O2.o
 *   objdump -d --disassemble=vr_wait_volatile vr-O2.o
 *   objdump -d --disassemble=vr_wait_plain    vr-O2.o
 */
#include "volatile_regs.h"

#include <string.h>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <pthread.h>
#  include <time.h>
#  include <unistd.h>
#endif

/* ==================== 模拟的寄存器区 ==================== */

/* 状态寄存器是 volatile：设备（本示例里是一个线程）会在任意时刻改它。
 * 这一条 volatile 与 STM32 上 `*(volatile uint32_t *)0x40010808` 里的那个
 * 是同一个作用：告诉编译器「这个字会自己变，每次用都要重新读」。 */
volatile uint32_t vr_sr;      /* 状态寄存器：设备写，主循环读 */
uint32_t vr_sr_plain;         /* 同一件事的对照件，刻意不加 volatile */
volatile uint32_t vr_cr;      /* 控制寄存器 */
volatile uint32_t vr_dr;      /* 数据寄存器，带 volatile 的那一份 */
uint32_t vr_dr_plain;         /* 同一件事的对照件，刻意不加 volatile */
volatile uint32_t vr_ir;      /* 中断标志 */

/* ==================== 两种等待写法 ==================== */

uint32_t vr_wait_volatile(uint32_t spin_limit) {
    uint32_t spins = 0u;
    /* 每次都真读：编译器不能把这个读提到循环外面，
     * 因为它不知道谁会改 vr_sr。 */
    while ((vr_sr & 1u) == 0u) {
        if (++spins >= spin_limit) {
            return 0u;
        }
    }
    return spins + 1u;
}

uint32_t vr_wait_plain(uint32_t spin_limit) {
    uint32_t spins = 0u;
    /* 一模一样的三行，只少了 volatile。
     * -O2 下编译器认定「循环体里没人写它」，于是把这个读提到循环之前，
     * 循环退化成纯粹的空转；此后别人怎么改内存，这里都不会再看一眼。 */
    while ((vr_sr_plain & 1u) == 0u) {
        if (++spins >= spin_limit) {
            return 0u;
        }
    }
    return spins + 1u;
}

/* ==================== 跨平台的线程与睡眠 ==================== */

#if defined(_WIN32)
typedef HANDLE vr_thread;
typedef DWORD(WINAPI *vr_thread_fn)(LPVOID);

static vr_thread vr_thread_start(vr_thread_fn fn, void *arg) {
    return CreateThread(NULL, 0, fn, arg, 0, NULL);
}

/* 等到线程结束返回 1，超时返回 0 */
static int vr_thread_join(vr_thread t, unsigned timeout_ms) {
    const DWORD r = WaitForSingleObject(t, (DWORD)timeout_ms);
    if (r == WAIT_OBJECT_0) {
        CloseHandle(t);
        return 1;
    }
    return 0;
}

static void vr_sleep_ms(unsigned ms) { Sleep((DWORD)ms); }

static uint64_t vr_now_ms(void) {
    static LARGE_INTEGER freq;
    static int ready = 0;
    LARGE_INTEGER now;
    if (!ready) {
        QueryPerformanceFrequency(&freq);
        ready = 1;
    }
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000) / freq.QuadPart);
}
#else
typedef pthread_t vr_thread;
typedef void *(*vr_thread_fn)(void *);

static vr_thread vr_thread_start(vr_thread_fn fn, void *arg) {
    pthread_t t;
    if (pthread_create(&t, NULL, fn, arg) != 0) {
        memset(&t, 0, sizeof t);
    }
    return t;
}

static int vr_thread_join(vr_thread t, unsigned timeout_ms) {
    /* 主机侧只在 Windows 上实测过；这里的写法是等价的保守版本。 */
    (void)timeout_ms;
    return pthread_join(t, NULL) == 0 ? 1 : 0;
}

static void vr_sleep_ms(unsigned ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)((ms % 1000u) * 1000000u);
    nanosleep(&ts, NULL);
}

static uint64_t vr_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}
#endif

/* ==================== 探针 ==================== */

typedef struct {
    int use_volatile;
    uint32_t spin_limit;
    uint32_t spins;
} vr_worker_arg;

#if defined(_WIN32)
static DWORD WINAPI vr_worker(LPVOID p)
#else
static void *vr_worker(void *p)
#endif
{
    vr_worker_arg *a = (vr_worker_arg *)p;
    a->spins = a->use_volatile ? vr_wait_volatile(a->spin_limit)
                               : vr_wait_plain(a->spin_limit);
#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

vr_probe_result vr_probe_flag(int use_volatile, unsigned delay_ms, unsigned timeout_ms) {
    vr_probe_result r;
    vr_worker_arg arg;
    vr_thread worker;
    const uint64_t t0 = vr_now_ms();

    memset(&r, 0, sizeof r);
    r.use_volatile = use_volatile;

    /* 两个状态位都先清零，两次调用互不影响 */
    vr_sr = 0u;
    *(volatile uint32_t *)&vr_sr_plain = 0u;

    arg.use_volatile = use_volatile;
    arg.spin_limit = 400000000u; /* 够大：-O2 下 plain 版本会一直空转到这个上限 */
    arg.spins = 0u;

    worker = vr_thread_start(vr_worker, &arg);
    if (worker == (vr_thread)0) {
        return r;
    }

    vr_sleep_ms(delay_ms);

    /* 「设备」把状态位写上。写的是 volatile 左值，
     * 因此这个写不会被优化掉；编译器只是不知道它什么时候发生。 */
    vr_sr = 1u;
    *(volatile uint32_t *)&vr_sr_plain = 1u;

    /* 等结果。超时也算一种结果：它说明循环没出来。 */
    if (vr_thread_join(worker, timeout_ms)) {
        r.timed_out = 0;
    } else {
        r.timed_out = 1;
    }
    r.spins = arg.spins;
    r.escaped = (arg.spins > 0u) ? 1 : 0;
    r.waited_ms = (unsigned)(vr_now_ms() - t0);
    return r;
}

/* ==================== 重复读同一个字 ==================== */

uint32_t vr_sum_reads(int use_volatile, uint32_t n) {
    uint32_t sum = 0u;
    uint32_t i;
    if (use_volatile) {
        /* 每次都真读：n 次读就是 n 次内存访问 */
        for (i = 0u; i < n; ++i) {
            sum += vr_dr;
        }
    } else {
        /* 编译器看到 vr_dr_plain 在循环体里没被写过，
         * 于是折成「读一次再乘 n」——读的次数从 n 次降到 1 次。 */
        for (i = 0u; i < n; ++i) {
            sum += vr_dr_plain;
        }
    }
    return sum;
}

void vr_dr_set(uint32_t value) {
    vr_dr = value;
    vr_dr_plain = value;
}

/* ==================== 寄存器表 ==================== */

const vr_reg_info *vr_reg_table(size_t *count) {
    static const vr_reg_info k_regs[VR_REG_COUNT] = {
        {"VR_CR", (uintptr_t)&vr_cr, 0x00000000u, "读写"},
        {"VR_SR", (uintptr_t)&vr_sr, 0x00000000u, "只读，设备改"},
        {"VR_DR", (uintptr_t)&vr_dr, 0x12345678u, "读写"},
        {"VR_IR", (uintptr_t)&vr_ir, 0x00000000u, "写 1 清位"},
    };
    *count = (size_t)VR_REG_COUNT;
    return k_regs;
}

void vr_print_reg_table(FILE *out) {
    size_t n = 0u;
    size_t i;
    const vr_reg_info *regs = vr_reg_table(&n);
    fprintf(out, "== 模拟的寄存器区 ==\n");
    fprintf(out, "  寄存器  地址                复位值      访问\n");
    for (i = 0u; i < n; ++i) {
        fprintf(out, "  %-6s  0x%016llx  0x%08lx  %s\n", regs[i].name,
                (unsigned long long)regs[i].addr, (unsigned long)regs[i].reset,
                regs[i].access);
    }
    fprintf(out, "  这四个字和普通全局量住在同一段内存里，没有任何特殊之处；\n");
    fprintf(out, "  「寄存器」这三个字说的是硬件语义：它们的值由设备改，程序看不见改的时机。\n");
}

/* ==================== 自测 ==================== */

static int g_pass;
static int g_fail;

static void check(FILE *out, int ok, const char *what) {
    if (ok) {
        ++g_pass;
        fprintf(out, "  [通过] %d. %s\n", g_pass + g_fail, what);
    } else {
        ++g_fail;
        fprintf(out, "  [失败] %d. %s\n", g_pass + g_fail, what);
    }
}

int vr_self_test(FILE *out) {
    vr_probe_result v;
    vr_probe_result p;

    g_pass = 0;
    g_fail = 0;
    fprintf(out, "== 自测 ==\n");

    /* --- 模拟寄存器 --- */
    {
        size_t n = 0u;
        const vr_reg_info *regs = vr_reg_table(&n);
        check(out, n == (size_t)VR_REG_COUNT, "寄存器表有 4 个寄存器");
        check(out, regs[1].addr != 0u && regs[1].addr != regs[0].addr,
              "VR_SR 的地址与 VR_CR 不同");
        check(out, (regs[0].addr % 4u) == 0u, "VR_CR 是 4 字节对齐的");
        check(out, regs[2].reset == 0x12345678u, "VR_DR 的复位值在表里写着");
    }

    /* --- 带 volatile 的等待：任何优化等级下都必须读得到 --- */
    v = vr_probe_flag(1, 50u, 10000u);
    check(out, v.timed_out == 0, "带 volatile：等待循环没有卡死");
    check(out, v.escaped == 1,
          "带 volatile：另一个线程改了状态位，循环读到了并跳出来");

    /* --- 不带 volatile 的等待：结果取决于优化等级 --- */
    p = vr_probe_flag(0, 50u, 10000u);
    check(out, p.timed_out == 0, "不带 volatile：等待循环也在自旋上限内结束");
#if defined(__OPTIMIZE__)
    check(out, p.escaped == 0,
          "不带 volatile（本档开了优化）：状态位改了，循环却一直读的是旧值");
    check(out, v.escaped != p.escaped,
          "两种写法的结果不同，差别只在那一个关键字");
#else
    check(out, p.escaped == 1,
          "不带 volatile（本档 -O0，未开优化）：循环每次都真读，因此结果与带 volatile 时相同");
    fprintf(out,
            "  [说明] 本档没有开优化，两种写法看不出差别。\n"
            "         用 -O2 编一遍就能看到：Release 预设或 tools/asm-compare.ps1。\n");
#endif

    /* --- 重复读一个字：结果相同，读的次数不同 --- */
    vr_dr_set(0x00000001u);
    check(out, vr_sum_reads(1, 100u) == 100u,
          "带 volatile 读 100 次，得到 100 次真实读的和");
    check(out, vr_sum_reads(0, 100u) == 100u,
          "不带 volatile 读 100 次，结果相同——正因为结果相同，这类错才不容易被发现");
    fprintf(out,
            "  [说明] 结果的差别在这里看不出来，差别在「读了几次」：\n"
            "         不带 volatile 的版本在 -O2 下只读一次再乘 100。\n"
            "         看汇编用 tools/asm-compare.ps1，或 objdump -d --disassemble=vr_sum_reads。\n");

    fprintf(out, "\n  自测结果：%d 项中 %d 项通过", g_pass + g_fail, g_pass);
    if (g_fail == 0) {
        fprintf(out, "，全部通过\n");
    } else {
        fprintf(out, "，%d 项失败\n", g_fail);
    }
    return g_fail;
}
