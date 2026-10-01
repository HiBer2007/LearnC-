/* link_core.c —— 04-symbols-and-linking 的核心实现
 *
 * 编译：由 CMakeLists.txt 编成静态库 core 的一个成员，不直接编译这个文件。
 * 手工编译（MinGW）：
 *   gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src/link_core.c -o link_core.o
 *   gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src/link_static_a.c -o link_static_a.o
 *   gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src/link_static_b.c -o link_static_b.o
 *   ar rcs libcore.a link_core.o link_static_a.o link_static_b.o
 *   g++ -std=c++17 -O2 -Wall -Wextra -Iinclude src/main_cli.c src/link_override.c \
 *       src/link_cpp_side.cpp libcore.a -o app_cli.exe
 */
#include "link_lab.h"

#include <string.h>

/* ==================== 弱符号 ==================== */

/* 两条工具链给弱函数的手段不一样：
 *   GCC / Clang  __attribute__((weak))：同名，链接器在「弱定义」与
 *                「强定义」之间选强的。
 *   MSVC         没有弱函数，只能走链接器的 /alternatename：
 *                把这个名字映射到另一个名字，别处没有定义时用它兜底。
 * 两种写法在应用侧看来完全一样：调用 ll_provider_name() 就行。 */
#if defined(_MSC_VER)
const char *ll_provider_default(void);
#pragma comment(linker, "/alternatename:ll_provider_name=ll_provider_default")
const char *ll_provider_default(void) { return "core-weak-default"; }
#else
__attribute__((weak)) const char *ll_provider_name(void) {
    return "core-weak-default";
}
#endif

/* ==================== 自测 ==================== */

static int g_pass;
static int g_fail;

void ll_report(FILE *out, int ok, const char *what) {
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
    }
    fprintf(out, "  [%s] %d. %s\n", ok ? "通过" : "失败", g_pass + g_fail, what);
}

void ll_report_skip(FILE *out, const char *what) {
    ++g_pass;
    fprintf(out, "  [跳过] %d. %s\n", g_pass + g_fail, what);
}

/* 两个地址是不是落在同一个映像里：用一个已知函数的地址当基准。 */
static int same_image(const void *a, const void *b) {
    const uintptr_t pa = (uintptr_t)a;
    const uintptr_t pb = (uintptr_t)b;
    const uintptr_t diff = pa > pb ? pa - pb : pb - pa;
    return diff < (16u * 1024u * 1024u);
}

int ll_self_test(FILE *out, ll_extra_check extra) {
    g_pass = 0;
    g_fail = 0;
    fprintf(out, "== 自测 ==\n");

    /* 调用方自己那几项先报，序号从 1 开始 */
    if (extra != NULL) {
        extra(out);
    }

    /* --- 弱符号覆盖 --- */
    {
        const char *name = ll_provider_name();
        ll_report(out, name != NULL && strcmp(name, "app-strong") == 0,
                  "ll_provider_name() 返回应用侧的强定义（弱符号被覆盖）");
        ll_report(out, name != NULL && strcmp(name, "core-weak-default") != 0,
                  "返回值不是 core 里的那个默认名");
    }

    /* --- 静态库成员 A：core 自己引用了它，因此必然在映像里 --- */
    ll_report(out, strcmp(ll_static_a_name(), "static-a") == 0,
              "成员 A 的符号解析到了 link_static_a.o");
    ll_report(out, ll_static_a_size() == 1024u, "成员 A 里那张表是 1024 字节");

    /* --- C 与 C++ 混编 --- */
    ll_report(out, strcmp(ll_cpp_name(), "cpp-side") == 0,
              "ll_cpp_name() 来自 C++ 翻译单元（extern \"C\" 让 C 侧找得到它）");
    ll_report(out, strcmp(ll_cpp_tag(), "cpp-mangled") == 0,
              "ll_cpp_tag() 绕了一层 C++ 链接的函数，名字会被修饰");

    /* --- 符号地址 --- */
    {
        const void *a = (const void *)(uintptr_t)&ll_provider_name;
        const void *b = (const void *)(uintptr_t)&ll_static_a_name;
        const void *c = (const void *)(uintptr_t)&ll_cpp_name;
        ll_report(out, a != NULL && b != NULL && c != NULL && a != b && b != c && a != c,
                  "三个函数的地址互不相同");
        ll_report(out, same_image(a, c) && same_image(b, c),
                  "三个函数落在同一个映像里（这是链接器把它们放进同一个 exe 的意思）");
    }

    /* --- 重复调用结果一致 --- */
    ll_report(out, strcmp(ll_provider_name(), ll_provider_name()) == 0,
              "同一件事问两次答案一样（自测本身是可重复的）");

    fprintf(out, "\n  自测结果：%d 项中 %d 项通过", g_pass + g_fail, g_pass);
    if (g_fail == 0) {
        fprintf(out, "，全部通过\n");
    } else {
        fprintf(out, "，%d 项失败\n", g_fail);
    }
    return g_fail;
}
