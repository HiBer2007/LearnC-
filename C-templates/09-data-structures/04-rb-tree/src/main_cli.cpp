/* main_cli.cpp —— 练习模板 04 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 dsr::RBTree，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 include/rb_tree.hpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 */
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "rb_tree.hpp"

namespace {

using Tree = dsr::RBTree<int, int>;
using Color = dsr::RBTree<int, int>::Color;

void line(const char *label, const std::string &text)
{
    std::cout << std::left << std::setw(34) << label << ": " << text << "\n";
}

void line(const char *label, long value)
{
    std::cout << std::left << std::setw(34) << label << ": " << value << "\n";
}

std::string check_of(const Tree &tree)
{
    const char *problem = tree.check();
    return problem == nullptr ? "ok" : problem;
}

/* 手搭一棵合法的红黑树（用 insert_raw 与 repaint，不经过修复）：
 *
 *              B:40
 *           /        \
 *         B:20       B:60
 *        /    \     /    \
 *      R:10  R:30  R:50  R:70
 *
 * 四条不变式：根是黑的；红节点的孩子都是黑的；四条路径的黑节点个数都是 3；搜索树顺序成立。
 */
void build_valid_tree(Tree &tree)
{
    tree.insert_raw(40, 0);
    tree.repaint(40, Color::Black);
    tree.insert_raw(20, 0);
    tree.repaint(20, Color::Black);
    tree.insert_raw(60, 0);
    tree.repaint(60, Color::Black);
    tree.insert_raw(10, 0);
    tree.insert_raw(30, 0);
    tree.insert_raw(50, 0);
    tree.insert_raw(70, 0);
}

void print_tree(const char *title, const Tree &tree)
{
    std::cout << title << "\n";
    tree.print(std::cout);
}

/* 依次插入，返回校验失败的次数 */
int insert_all(Tree &tree, const int *keys, std::size_t count)
{
    int failed = 0;
    for (std::size_t i = 0; i < count; ++i) {
        tree.insert(keys[i], keys[i] * 10);
        if (tree.check() != nullptr) {
            ++failed;
        }
    }
    return failed;
}

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: the invariant checker ===\n";
    {
        Tree t;
        build_valid_tree(t);
        line("hand-built tree, size", static_cast<long>(t.size()));
        line("hand-built tree, check", check_of(t));
        line("hand-built tree, black height", static_cast<long>(t.black_height()));
        print_tree("hand-built tree:", t);

        Tree d1;
        build_valid_tree(d1);
        d1.repaint(40, Color::Red);             /* 把根染红 */
        line("broken: root is red", check_of(d1));

        Tree d2;
        build_valid_tree(d2);
        d2.repaint(20, Color::Red);             /* 黑节点染红，孩子就是红的 */
        line("broken: red child of red node", check_of(d2));

        Tree d3;
        build_valid_tree(d3);
        d3.repaint(10, Color::Black);           /* 一片叶子由红变黑，两条路的黑节点数不同 */
        line("broken: black heights differ", check_of(d3));

        Tree d4;
        build_valid_tree(d4);
        d4.set_key(10, 99);                     /* 左子树里冒出比父亲大的键 */
        line("broken: search order", check_of(d4));
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: case 3, uncle is black and node is the outer child ===\n";
    {
        const int keys[] = {30, 20, 10};        /* 一路往左，最后一个是外侧孩子 */
        Tree t;
        const int failed = insert_all(t, keys, 3);
        line("size", static_cast<long>(t.size()));
        line("check", check_of(t));
        line("black height", static_cast<long>(t.black_height()));
        line("failed checks while inserting", failed);
        print_tree("after inserting 30, 20, 10:", t);
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: case 2, uncle is black and node is the inner child ===\n";
    {
        const int keys[] = {30, 10, 20};        /* 折线：先要拉直，再落进情形 3 */
        Tree t;
        const int failed = insert_all(t, keys, 3);
        line("size", static_cast<long>(t.size()));
        line("check", check_of(t));
        line("black height", static_cast<long>(t.black_height()));
        line("failed checks while inserting", failed);
        print_tree("after inserting 30, 10, 20:", t);
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: case 1, uncle is red ===\n";
    {
        const int keys[] = {20, 10, 30, 5};     /* 叔叔是红的：双红要被推到祖父那一层 */
        Tree t;
        const int failed = insert_all(t, keys, 4);
        line("size", static_cast<long>(t.size()));
        line("check", check_of(t));
        line("black height", static_cast<long>(t.black_height()));
        line("failed checks while inserting", failed);
        print_tree("after inserting 20, 10, 30, 5:", t);
    }
    {
        /* 一路递增：每一层都落在外侧，主要走情形 3 */
        Tree t;
        int failed = 0;
        for (int i = 1; i <= 64; ++i) {
            t.insert(i, i);
            if (t.check() != nullptr) {
                ++failed;
            }
        }
        line("1..64 ascending, failed checks", failed);
        line("1..64 ascending, black height", static_cast<long>(t.black_height()));
    }
    {
        /* 固定种子的伪随机顺序：三种情形都会走到。键先查重，保证互不相同 */
        std::vector<int> keys;
        std::uint32_t x = 12345U;
        while (keys.size() < 64) {
            x = x * 1103515245U + 12345U;
            const int key = static_cast<int>((x >> 16) % 100000U);
            bool duplicate = false;
            for (std::size_t i = 0; i < keys.size(); ++i) {
                if (keys[i] == key) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                keys.push_back(key);
            }
        }

        Tree t;
        int failed = 0;
        for (std::size_t i = 0; i < keys.size(); ++i) {
            t.insert(keys[i], static_cast<int>(i));
            if (t.check() != nullptr) {
                ++failed;
            }
        }
        line("random 64 keys, failed checks", failed);
        line("random 64 keys, size", static_cast<long>(t.size()));
        line("random 64 keys, black height", static_cast<long>(t.black_height()));
    }

    return 0;
}
