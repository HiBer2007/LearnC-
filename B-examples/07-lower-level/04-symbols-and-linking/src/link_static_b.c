/* link_static_b.c —— 静态库 core 的另一个成员（目标文件）
 *
 * 这张表比 A 的那张大四倍：引用它与否，产物大小差得很明显，
 * 正好用来说明「静态库按目标文件取舍」这一条。
 * app_cli 引用了它，app_cli_slim 没有引用。
 */
#include "link_lab.h"

static const unsigned char k_table_b[4096] = {2u};

const char *ll_static_b_name(void) { return "static-b"; }

size_t ll_static_b_size(void) { return sizeof k_table_b; }
