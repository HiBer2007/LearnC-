/* link_lab.h —— 示例 07-lower-level/04-symbols-and-linking 的核心接口
 *
 * 这个头文件同时给 C 与 C++ 两个翻译单元使用，因此整个接口包在
 * extern "C" 里：C++ 侧按 C 的规则生成符号名，C 侧才找得到它。
 */
#ifndef LINK_LAB_H
#define LINK_LAB_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 弱符号钩子 ==================== */
/* 声明有了，core 里给一份弱定义；应用侧如果再给一份强定义，
 * 链接器选强的那一份。真启动文件里的那些 weak 别名是同一个机制。 */
const char *ll_provider_name(void);

/* ==================== 静态库的两个成员 ==================== */
/* 分别住在 link_static_a.c 与 link_static_b.c 里。
 * 静态库的粒度是目标文件：没人引用哪个成员，它就不会被链进来。 */
const char *ll_static_a_name(void);
size_t ll_static_a_size(void);
const char *ll_static_b_name(void);
size_t ll_static_b_size(void);

/* ==================== C++ 翻译单元提供的那一份 ==================== */
/* 定义在 link_cpp_side.cpp 里，用 extern "C" 导出。
 * 同一份源码里还有一个 C++ 链接的函数，它的名字会被修饰。 */
const char *ll_cpp_name(void);
const char *ll_cpp_tag(void);

/* ==================== 自测 ==================== */
/* 记一项结果，供调用方在自测里插入自己的检查项。
 * 计数器在 core 里，汇总与「全部通过」那句话由 ll_self_test 打印。 */
void ll_report(FILE *out, int ok, const char *what);
void ll_report_skip(FILE *out, const char *what);

/* 额外检查项的钩子；传 NULL 表示没有。 */
typedef void (*ll_extra_check)(FILE *out);

/* 跑完全部检查。extra 里的项会计入同一个总数。 */
int ll_self_test(FILE *out, ll_extra_check extra);

#ifdef __cplusplus
}
#endif

#endif /* LINK_LAB_H */
