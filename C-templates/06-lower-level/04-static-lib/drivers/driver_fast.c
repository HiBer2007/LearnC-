/* driver_fast.c —— 阶段 4 的 TODO：外部驱动
 *
 * 它给出一个**强**定义 sensor_sample_hook，库里的弱引用就会指向它，
 * sensor_read 于是走驱动这一份算法。
 */
#include "sensor.h"

/* 这个函数与 core/sensor.c 里的那个同名同签名（那边也是 static）。
 * 两处都是内部链接，所以链接器看到的是两个互不干扰的局部符号——
 * 用 nm 看，两边都是小写 t。
 * 阶段 4-1：实现成「合法通道原样返回，越界返回 -1」。 */
static int strip_channel(int ch)
{
    /* TODO 4-1：占位实现 */
    (void)ch;
    return -2000;
}

/* 阶段 4-2
 * 采样钩子的驱动实现：本模板要求返回 ch * 100 + 7
 * （通道 0 读到 7、通道 1 读到 107、通道 2 读到 207……），
 * 越界返回 -1。先用本文件自己的 strip_channel 检查通道号。
 *
 * 这个名字不写在 include/sensor.h 里：它是「外部可选实现」，
 * 由 core/sensor.c 自己按弱符号去发现。 */
int sensor_sample_hook(int ch)
{
    /* TODO 4-2：占位实现（先原样转给 strip_channel，好让这一版也能编译过） */
    return strip_channel(ch);
}
