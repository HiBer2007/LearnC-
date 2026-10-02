/* disjoint_set.cpp —— 练习模板 05 的并查集（C++）
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
 * 两处优化（阶段 3 的 find 与阶段 4 的 unite）写在这个文件里，
 * 计数器（步数、树高、棵数）与两个测试工具已经给出。
 */
#include "disjoint_set.hpp"

#include <cstddef>
#include <utility>

namespace dsg {

DisjointSet::DisjointSet(int n)
    : parent_(static_cast<std::size_t>(n)), size_(static_cast<std::size_t>(n), 1), n_(n)
{
    for (int i = 0; i < n; ++i) {
        parent_[static_cast<std::size_t>(i)] = i;   /* 每个点自成一集合，根记自己 */
    }
}

/* ==================================================================
 * 阶段 3-1：路径压缩
 * ================================================================== */

/* TODO（阶段 3-1）：求 x 所在集合的根，并把**走过的这一条路压平**。
 *
 * 现在的实现是「只找根、不压平」：答案是对的，但同一条长路每问一次就要走一次。
 * 要补的是第二趟：把路上经过的每个节点直接挂到根上，这样下次问它们就是一步。
 *
 * 提示：
 *   1. 第一趟已经写好了（顺着父指针走到根），根存在 root 里；
 *   2. 第二趟从 x 出发，边走边把 parent 改成 root，直到走到 root；
 *      改之前先把下一个节点记下来，否则改完指针就找不到路了；
 *   3. 步数只在第一趟里计（走一步用的是 step_to），第二趟不记——
 *      测的是「查找要走多远」，不是「改了几个指针」；
 *   4. 只在根上更新 size_：压缩不改变集合的大小，Parent 变了但大小没变。
 *
 * 判据（见《配置步骤.md》阶段 3）：验收程序手工搭一条 2000 个节点的长链，
 *   第一次 find(0) 走 1999 步；**第二次** find(0) 应当只要 1 步。
 *   没有压缩时第二次仍然是 1999 步——这就是判据。 */
int DisjointSet::find(int x)
{
    int root = x;
    while (parent_[static_cast<std::size_t>(root)] != root) {
        root = step_to(root);
    }
    return root;    /* 占位实现：找到根就返回，没有把走过的路压平 */
}

/* ==================================================================
 * 阶段 4-1：按大小合并
 * ================================================================== */

/* TODO（阶段 4-1）：合并 a 与 b 所在的两个集合。
 *
 * 现在的实现是「谁先来谁当根」：把 rb 直接挂到 ra 下面，不看两棵树各有多大。
 * 要补的是一句比较：**让小的那棵挂到大的下面**，树就不容易长高。
 *
 * 提示：
 *   1. ra 与 rb 都是根，大小分别记在 size_[ra] 与 size_[rb] 上（只在根上有效）；
 *   2. 若 size_[ra] 比 size_[rb] 小，就把两者换一下，让 ra 始终是大的那棵；
 *      换的是「哪棵树当根」，不是元素本身；
 *   3. 挂完别忘了把大小加到新的根上；
 *   4. 秩（rank）与大小等价，不必两个都实现——本模板只用了大小。
 *
 * 判据（见《配置步骤.md》阶段 4）：按「总是把新点并到老树上」的顺序合并 2000 个点，
 *   最大树高应当是 1（占位实现是 1999，退化成一条链）。
 *   注意合并过程中的步数反而会多一点：按大小合并是**先花几步把树压平**，
 *   省下的是后面每一次查找。 */
bool DisjointSet::unite(int a, int b)
{
    const int ra = find(a);
    const int rb = find(b);
    if (ra == rb) {
        return false;
    }
    /* 占位实现：直接把 rb 挂到 ra 下面，不比较两棵树的大小 */
    parent_[static_cast<std::size_t>(rb)] = ra;
    size_[static_cast<std::size_t>(ra)] += size_[static_cast<std::size_t>(rb)];
    return true;
}

/* ------------------------------------------------------------ 量具 */

int DisjointSet::step_to(int x) const
{
    ++steps_;
    return parent_[static_cast<std::size_t>(x)];
}

int DisjointSet::root_of_unchecked(int x) const
{
    while (parent_[static_cast<std::size_t>(x)] != x) {
        x = parent_[static_cast<std::size_t>(x)];
    }
    return x;
}

int DisjointSet::size_of(int x)
{
    const int root = find(x);
    return size_[static_cast<std::size_t>(root)];
}

int DisjointSet::max_height(void) const
{
    int best = 0;
    for (int x = 0; x < n_; ++x) {
        int depth = 0;
        int cur = x;
        while (parent_[static_cast<std::size_t>(cur)] != cur) {
            cur = parent_[static_cast<std::size_t>(cur)];
            ++depth;
        }
        if (depth > best) {
            best = depth;
        }
    }
    return best;
}

int DisjointSet::distinct_roots(void) const
{
    int count = 0;
    for (int x = 0; x < n_; ++x) {
        if (parent_[static_cast<std::size_t>(x)] == x) {
            ++count;
        }
    }
    return count;
}

void DisjointSet::link_raw(int child, int root)
{
    const int child_root = root_of_unchecked(child);
    const int new_root = root_of_unchecked(root);
    if (child_root == new_root) {
        return;
    }
    parent_[static_cast<std::size_t>(child_root)] = new_root;
    size_[static_cast<std::size_t>(new_root)] += size_[static_cast<std::size_t>(child_root)];
}

} /* namespace dsg */
