/**
 * main.c —— 空模板 03 的主程序
 *
 * 先用命令行确认这个工程本身能编译：
 *     cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc
 *     cmake --build build
 *     build\bin\app.exe
 *
 * 能跑通之后再照《配置步骤.md》配 VS Code。
 */
#include <stdio.h>

#include "util.h"

int main(void)
{
    for (int i = 1; i <= 5; ++i) {
        printf("square(%d) = %d\n", i, square(i));   /* <- F11 会跳进 util.c */
    }

    printf("factorial(6) = %lld\n", factorial(6));   /* <- 反复 F11 看调用堆栈 */

    return 0;
}
