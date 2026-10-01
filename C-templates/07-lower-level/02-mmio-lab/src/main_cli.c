/* main_cli.c —— 命令行验收程序（已经写好，不需要改）
 *
 * 它按阶段调用 include/gpio_lab.h 里的接口，把寄存器内容与访问次数打印出来。
 * 阶段 3 在 Cortex-M 上跑，主机这一侧只打印指路信息。
 *
 * 编译与运行：
 *   cmake --preset mingw-gdb
 *   cmake --build --preset mingw-gdb
 *   build\mingw\bin\app_cli.exe
 */
#include "gpio_lab.h"
#include "regs.h"

#include <stdio.h>

static void print_access(const char *what, uint32_t odr)
{
    printf("  %-22s ODR = 0x%04x   reads = %u  writes = %u\n", what, odr,
           g_reg_reads[REG_ODR / 4u], g_reg_writes[REG_ODR / 4u]);
}

/* 打印某个寄存器自己的访问计数（阶段 2 用的是 BSRR 与 BRR，不是 ODR） */
static void print_access_named(const char *what, uint32_t odr,
                               const char *reg_name, uint32_t reg)
{
    printf("  %-22s ODR = 0x%04x   %s: reads = %u  writes = %u\n", what, odr,
           reg_name, g_reg_reads[reg / 4u], g_reg_writes[reg / 4u]);
}

int main(void)
{
    sim_device_reset();

    /* ---------------------------------------------------------- 阶段 1 */
    printf("=== Stage 1: read-modify-write (ODR) ===\n");
    printf("  %-22s ODR = 0x%04x\n", "reset()", sim_device_odr());

    sim_access_clear();
    gl_set_pin(PIN_OUT_1);
    print_access("set(PIN_OUT_1)", sim_device_odr());

    sim_access_clear();
    gl_set_pin(PIN_OUT_2);
    print_access("set(PIN_OUT_2)", sim_device_odr());

    sim_access_clear();
    gl_toggle_pin(PIN_OUT_1);
    print_access("toggle(PIN_OUT_1)", sim_device_odr());

    sim_access_clear();
    gl_clear_pin(PIN_OUT_2);
    print_access("clear(PIN_OUT_2)", sim_device_odr());

    /* 设备的时序：每读一次 IDR 往前走一步，第三步之后就绪位读出来是 1。
     * 每一步单独清一次计数，好让「这次读了几回」看得清楚。 */
    for (int k = 0; k < 3; ++k) {
        int v;
        sim_access_clear();
        v = gl_read_pin(PIN_READY);
        printf("  %-22s -> %d            IDR reads = %u\n", "read(PIN_READY)",
               v, g_reg_reads[REG_IDR / 4u]);
    }

    /* ---------------------------------------------------------- 阶段 2 */
    printf("\n=== Stage 2: single write (BSRR / BRR) ===\n");
    sim_device_reset();
    sim_access_clear();          /* 四次调用一起数，看的是累计值 */

    gl_set_pin_fast(PIN_OUT_1);
    print_access_named("set_fast(PIN_OUT_1)", sim_device_odr(), "BSRR", REG_BSRR);

    gl_set_pin_fast(PIN_OUT_2);
    print_access_named("set_fast(PIN_OUT_2)", sim_device_odr(), "BSRR", REG_BSRR);

    gl_clear_pin_fast(PIN_OUT_1);
    print_access_named("clear_fast(PIN_OUT_1)", sim_device_odr(), "BRR ", REG_BRR);

    gl_clear_pin_fast(PIN_OUT_2);
    print_access_named("clear_fast(PIN_OUT_2)", sim_device_odr(), "BRR ", REG_BRR);

    printf("  ODR 的累计计数：reads = %u  writes = %u（一次写完成不碰 ODR）\n",
           g_reg_reads[REG_ODR / 4u], g_reg_writes[REG_ODR / 4u]);

    /* ---------------------------------------------------------- 阶段 3 */
    printf("\n=== Stage 3: volatile on Cortex-M (QEMU) ===\n");
    printf("  这一步不在主机上跑：在 WSL 里执行 arm/run_qemu.sh\n");
    printf("  带 volatile 的版本会打印 ready；去掉 volatile 的版本第一行之后就没有输出了\n");

    /* ---------------------------------------------------------- 阶段 4 */
    printf("\n=== Stage 4: field read-modify-write (CRL) ===\n");
    sim_device_reset();

    sim_access_clear();
    gl_write_field(REG_CRL, 0x000000F0u, 4u, 0x3u);
    printf("  %-40s CRL = 0x%08x   reads = %u  writes = %u\n",
           "write_field(CRL, 0xF0, 4, 0x3)", sim_peek(REG_CRL),
           g_reg_reads[REG_CRL / 4u], g_reg_writes[REG_CRL / 4u]);
    printf("  %-40s -> 0x%08x\n", "read_field(CRL, 0xF0, 4)",
           gl_read_field(REG_CRL, 0x000000F0u, 4u));

    sim_access_clear();
    gl_write_field(REG_CRL, 0x000000F0u, 4u, 0xAu);
    printf("  %-40s CRL = 0x%08x   reads = %u  writes = %u\n",
           "write_field(CRL, 0xF0, 4, 0xA)", sim_peek(REG_CRL),
           g_reg_reads[REG_CRL / 4u], g_reg_writes[REG_CRL / 4u]);

    return 0;
}
