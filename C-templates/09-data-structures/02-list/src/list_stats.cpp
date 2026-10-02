/* list_stats.cpp —— 练习模板 02 的计数器定义（C++）
 *
 * 版权所有 (C) 2026 HiBer2007，保留所有权利。
 *
 * 本程序是《C 与 C++》教材的一部分，采用与教材文档相同的授权：
 * CC BY-NC-ND 4.0 加附加条款。全文见仓库根目录的 LICENSE 与 许可附加条款.md。
 *
 * 允许在保留本声明的前提下查看、编译、运行本程序用于学习；
 * 不允许二次分发，不允许商用，不允许演绎（修改后再分发），不允许移除署名。
 *
 * 本程序不提供任何担保。
 *
 * ------------------------------------------------------------------
 * 这个文件是计数器，不是练习内容，不需要改。
 */
#include "list_stats.hpp"

long ListStats::nodes_created = 0;
long ListStats::nodes_destroyed = 0;
long ListStats::values_constructed = 0;
long ListStats::values_destroyed = 0;

void ListStats::reset(void)
{
    nodes_created = 0;
    nodes_destroyed = 0;
    values_constructed = 0;
    values_destroyed = 0;
}

long ListStats::nodes_alive(void)
{
    return nodes_created - nodes_destroyed;
}
