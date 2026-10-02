/**
 * dsu_mini.hpp —— 并查集五档写法：两处优化各写在哪一行
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
 */

/**
 * 并查集只有一个数组：parent_[i] 是 i 的父亲，根记自己。
 * 五档写法的差别只在这两处：
 *
 *   按大小合并   unite 里「让 ra 是大的那棵」那一行
 *   路径压缩     find 里第二个 while，把走过的点直连根
 *   路径减半     find 里 parent_[x] = parent_[parent_[x]] 这一行，代码更短
 *
 * 五档对「两个点在不在同一个集合」给出的答案完全一样，变的只是步数。
 * 报告里的步数是 find 里真正走过的边数，不是公式算出来的。
 *
 * 面向人的文字一律是 u8"" 字面量，因此本头文件里的 std::string 承载 UTF-8 字节。
 */
#ifndef DSU_MINI_HPP
#define DSU_MINI_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace dmini {

/* ================= 计数 ================= */

struct DsuStats {
    std::size_t steps = 0;         /**< find 里沿着父指针走过的步数 */
    std::size_t find_calls = 0;    /**< find 被调用的次数（含 unite 内部的） */
    std::size_t compressions = 0;  /**< 路径压缩里改过的父指针个数 */
    std::size_t link_writes = 0;   /**< 合并时写父指针的次数 */

    void reset();
};

DsuStats &stats();

/* ================= 五档 ================= */

enum class Mode {
    Plain,      /**< 都不做：一路走到根，合并固定让后者挂到前者下面 */
    BySize,     /**< 只按大小合并 */
    Compress,   /**< 只做路径压缩 */
    Halve,      /**< 只做路径减半 */
    Both,       /**< 按大小合并 + 路径压缩 */
};

const char *mode_name(Mode mode);

class Dsu {
public:
    Dsu(int n, Mode mode);

    /** 找根。按档位决定要不要压缩或减半；每走一步就记一步 */
    int find(int x);

    /** 合并两个集合；本来就在一起返回 false */
    bool unite(int a, int b);

    /* ---------------- 只读观察，都不改结构、不记步 ---------------- */

    int root_of(int x) const;               /**< 一路走到根，不压缩 */
    bool same(int a, int b) const { return root_of(a) == root_of(b); }
    int set_count() const { return set_count_; }
    int max_height() const;                 /**< 整片森林的最大深度 */
    bool is_root(int x) const { return parent_[static_cast<std::size_t>(x)] == x; }
    int parent_of(int x) const { return parent_[static_cast<std::size_t>(x)]; }

    /** 集合大小。**只在根上有效**：先 find 再读 */
    int size_of(int x) const { return size_[static_cast<std::size_t>(x)]; }

    /** 直接读 size_，不做任何校正。非根节点上读到的是陈旧值——只用来演示这个坑 */
    int stale_size_of(int x) const { return size_[static_cast<std::size_t>(x)]; }

    int vertex_count() const { return n_; }
    Mode mode() const { return mode_; }

private:
    int n_ = 0;
    int set_count_ = 0;
    Mode mode_ = Mode::Plain;
    std::vector<int> parent_;
    std::vector<int> size_;
};

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;     /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;        /**< "16 项中 16 项通过，全部通过" */
};

/** 项目输出：五档对照、离线倒序删边、一处常见误用三段。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对五档的语义一致、树高、大小只在根上有效、离线倒序与朴素做法一致 */
CheckResult run_self_tests();

}   /* namespace dmini */

#endif /* DSU_MINI_HPP */
