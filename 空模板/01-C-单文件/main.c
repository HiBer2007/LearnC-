/**
 * 空模板 01 · C 单文件
 *
 * 这个文件本身是完整的、能编译的（用命令行验证一下：
 *     gcc -g -O0 main.c -o main.exe && main.exe
 * ）。
 *
 * 缺的只是 VS Code 的配置 —— 也就是 .vscode 文件夹。
 * 请照着同目录的《配置步骤.md》把它配出来。
 *
 * 程序本身是故意写简单的：配置调试器时，别让业务逻辑分散你的注意力。
 */
#include <stdio.h>

/* 故意做成独立函数：配好之后在这里打断点，按 F11 可以单步进来 */
static int square(int x)
{
    int result = x * x;
    return result;
}

int main(void)
{
    int n = 7;
    int s = square(n);

    printf("%d 的平方是 %d\n", n, s);

    for (int i = 1; i <= 3; ++i) {
        printf("square(%d) = %d\n", i, square(i));
    }

    return 0;
}
