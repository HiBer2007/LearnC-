/* link_static_a.c —— 静态库 core 的一个成员（目标文件）
 *
 * 它带来的那张表会被链进任何一个引用了 ll_static_a_name 的可执行文件。
 * 静态库的粒度就是这个文件：没人引用这里的符号，整个目标文件都不会进最终映像。
 */
#include "link_lab.h"

/* 一张只读表：用来让「成员有没有被链进来」在产物大小上看得见 */
static const unsigned char k_table_a[1024] = {1u};

const char *ll_static_a_name(void) { return "static-a"; }

size_t ll_static_a_size(void) { return sizeof k_table_a; }
