/* list_stats.hpp —— 练习模板 02 的节点与元素计数（C++）
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
 * 计数发生在节点的建立与销毁两处（list.hpp 里的 create_node 与 destroy_node）。
 * 验收程序用它判断三件事：节点有没有漏掉销毁、size() 与实际是否一致、
 * splice 有没有偷偷构造或析构元素。
 *
 * 定义在 src/list_stats.cpp 里，这个文件不需要改。
 */
#ifndef LIST_STATS_HPP
#define LIST_STATS_HPP

struct ListStats {
    static long nodes_created;          /* 建立过多少个节点（含哨兵） */
    static long nodes_destroyed;        /* 销毁过多少个节点 */
    static long values_constructed;     /* 构造过多少个元素（含哨兵里那一个） */
    static long values_destroyed;       /* 析构过多少个元素 */

    static void reset(void);

    /* 还没销毁的节点数：正常收尾时应当是 0 */
    static long nodes_alive(void);
};

#endif /* LIST_STATS_HPP */
