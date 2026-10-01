/* main_cli.c —— 04-symbols-and-linking 的命令行版
 *
 * 它在编译期由一个宏决定要不要引用静态库的第二个成员：
 *   -DWITH_B     引用 link_static_b.o（app_cli 这个目标）
 *   不加          不引用（app_cli_slim 这个目标，产物小 4 KiB 左右）
 * 两个目标跑的是同一份源码、同一套自测，只有这一点差别。
 *
 * 关键点：对 ll_static_b_* 的引用只出现在这里，不出现在 core 里。
 * 只要 core 里还有一句引用，链接器就会把 link_static_b.o 拉进来，
 * 那时两个目标的产物就一样大了——这一点在改这份代码时最容易踩空。
 *
 * 手工编译：
 *   gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src/main_cli.c -o main_cli.o
 *   gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src/link_override.c -o link_override.o
 *   g++ -std=c++17 -O2 -Wall -Wextra -Iinclude -c src/link_cpp_side.cpp -o link_cpp_side.o
 *   gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src/link_core.c -o link_core.o
 *   gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src/link_static_a.c -o link_static_a.o
 *   ar rcs libcore.a link_core.o link_static_a.o
 *   g++ main_cli.o link_override.o link_cpp_side.o libcore.a -o app_cli.exe
 */
#include "link_lab.h"

#include <string.h>

static const char *const k_usage =
    "用法：app_cli [选项]\n"
    "  （无选项）   打印符号解析结果，最后跑自测\n"
    "  --selftest   只跑自测\n"
    "  --help       显示这段文字\n";

/* 静态库成员 B 的那两项检查：引用与否完全由这个宏决定 */
#ifdef WITH_B
static void check_static_b(FILE *out) {
    ll_report(out, strcmp(ll_static_b_name(), "static-b") == 0,
              "成员 B 的符号解析到了 link_static_b.o");
    ll_report(out, ll_static_b_size() == 4096u, "成员 B 里那张表是 4096 字节");
}
#else
static void check_static_b(FILE *out) {
    ll_report_skip(out, "成员 B（本目标没有引用它，整个目标文件都不会被链进来）");
    ll_report_skip(out, "成员 B 里那张表的大小");
}
#endif

int main(int argc, char **argv) {
    int only_selftest = 0;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--selftest") == 0) {
            only_selftest = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            fputs(k_usage, stdout);
            return 0;
        } else {
            fprintf(stderr, "无法识别的参数：%s\n\n%s", argv[i], k_usage);
            return 2;
        }
    }

    if (only_selftest) {
        return ll_self_test(stdout, check_static_b) == 0 ? 0 : 1;
    }

    printf("示例 07-lower-level/04-symbols-and-linking · 符号与链接\n\n");

    printf("== 同名符号最后落到了哪一份定义 ==\n");
    printf("  ll_provider_name()   = %s\n", ll_provider_name());
    printf("      core 里那份弱定义的名字是 core-weak-default；\n");
    printf("      应用侧 link_override.c 给了强定义，链接器选了强的。\n\n");

    printf("== 静态库的两个成员 ==\n");
    printf("  ll_static_a_name()   = %s   表 %lu 字节\n", ll_static_a_name(),
           (unsigned long)ll_static_a_size());
#ifdef WITH_B
    printf("  ll_static_b_name()   = %s   表 %lu 字节\n", ll_static_b_name(),
           (unsigned long)ll_static_b_size());
    printf("      本目标引用了成员 B，link_static_b.o 被链进来了。\n");
#else
    printf("  ll_static_b_name()   = （没有引用，link_static_b.o 不在映像里）\n");
    printf("      静态库按目标文件取舍：没人引用那个成员，它一个字节都不进最终产物。\n");
#endif
    printf("      用 nm 看两个 exe 就能看出差别：slim 版里找不到 ll_static_b_name。\n\n");

    printf("== C 与 C++ 混编 ==\n");
    printf("  ll_cpp_name()        = %s（C++ 翻译单元，用 extern \"C\" 导出）\n", ll_cpp_name());
    printf("  ll_cpp_tag()         = %s（绕了一层 C++ 链接的函数）\n", ll_cpp_tag());
    printf("      用 nm 看这两个符号：前者就是 ll_cpp_name，后者带着 _ZN 开头的修饰名。\n");
    printf("\n");

    return ll_self_test(stdout, check_static_b) == 0 ? 0 : 1;
}
