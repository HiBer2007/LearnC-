/* main_cli.c —— 命令行验收程序（已经写好，不需要改）
 *
 * 同一份源码编出两个可执行文件：
 *   app_default   只链 core，sensor_read 用库自带的默认算法
 *   app_override  再链 drivers，sensor_read 走外部驱动的实现
 *
 * 注意这里**不直接调用** sensor_sample_hook：它是外部可选实现，
 * 由库按弱符号去发现，调用方只认 sensor_read。
 *
 * 编译与运行：
 *   cmake --preset mingw-gdb
 *   cmake --build --preset mingw-gdb
 *   build\mingw\bin\app_default.exe
 *   build\mingw\bin\app_override.exe
 */
#include "sensor.h"

#include <stdio.h>

int main(void)
{
    int i;

    /* ---------------------------------------------------------- 阶段 1 */
    printf("=== Stage 1: channel check (internal linkage) ===\n");
    printf("  check(0)  = %d\n", sensor_check_channel(0));
    printf("  check(2)  = %d\n", sensor_check_channel(2));
    printf("  check(9)  = %d   (expect -1)\n", sensor_check_channel(9));
    printf("  check(-1) = %d   (expect -1)\n", sensor_check_channel(-1));

    /* ---------------------------------------------------------- 阶段 2 */
    printf("\n=== Stage 2: the second member of the static library ===\n");
    printf("  version() = %d   (expect 100)\n", sensor_version());

    /* ---------------------------------------------------------- 阶段 3、4 */
    printf("\n=== Stage 3/4: default algorithm or external driver ===\n");
    for (i = 0; i < SENSOR_CHANNELS; ++i) {
        printf("  read(%d) = %d\n", i, sensor_read(i));
    }

    printf("\n=== out of range ===\n");
    printf("  read(9) = %d   (expect -1)\n", sensor_read(9));

    printf("\n（上面若有 -1000 或 -2000，说明对应的 TODO 还没做："
           "-1000 来自 core，-2000 来自 drivers/driver_fast.c）\n");
    return 0;
}
