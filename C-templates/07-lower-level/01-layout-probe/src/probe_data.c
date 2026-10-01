/* probe_data.c —— 探测对象（素材，不要改）
 *
 * 六类对象在这里各有一个代表：它们的地址就是这个模板要探的东西。
 * 这些对象声明在 include/layout_probe.h 里，阶段 1 直接取地址即可。
 */
#include "layout_probe.h"

int        lp_g_data   = 0x11223344;   /* 有初值的全局：.data */
int        lp_g_bss;                   /* 无初值的全局：.bss  */
const int  lp_g_rodata = 0x55667788;   /* 只读全局：.rodata   */
const char lp_g_msg[]  = "layout-probe";

/* 一个什么也不做的函数：它的地址落在 .text 里 */
void lp_code_anchor(void)
{
    /* 空函数体是有意的：取它的地址就能看到 .text 的位置 */
}
