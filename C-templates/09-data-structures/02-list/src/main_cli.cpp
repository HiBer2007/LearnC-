/* main_cli.cpp —— 练习模板 02 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 dsl::List，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 include/list.hpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 */
#include <cstddef>
#include <iomanip>
#include <string>
#include <iostream>

#include "list.hpp"
#include "list_stats.hpp"

namespace {

/* 所有输出都走这一个函数，标签统一占 18 列 */
void line(const char *label, const std::string &text)
{
    std::cout << std::left << std::setw(22) << label << ": " << text << "\n";
}

void line(const char *label, long value)
{
    std::cout << std::left << std::setw(22) << label << ": " << value << "\n";
}

/* check() 正常才遍历：链断了的时候硬走会读到乱七八糟的东西 */
std::string values_of(dsl::List<int> &list)
{
    if (list.check() != nullptr) {
        return "check failed, values not read";
    }
    std::string text = "[";
    for (dsl::List<int>::iterator it = list.begin(); it != list.end(); ++it) {
        if (text.size() > 1) {
            text += " ";
        }
        text += std::to_string(*it);
    }
    text += "]";
    return text;
}

std::string check_of(dsl::List<int> &list)
{
    const char *problem = list.check();
    return problem == nullptr ? "ok" : problem;
}

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: insert links the new node ===\n";
    {
        ListStats::reset();
        dsl::List<int> a;
        a.push_back(3);                     /* 尾插就是「在 end() 之前插入」 */
        a.insert(a.begin(), 2);             /* 插到头之前 */
        a.push_back(5);
        dsl::List<int>::iterator it = a.begin();
        ++it;
        ++it;                               /* 指向 5 */
        a.insert(it, 4);                    /* 插到中间 */

        line("size", static_cast<long>(a.size()));
        line("check", check_of(a));
        line("values", values_of(a));
        line("nodes alive", ListStats::nodes_alive());
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: erase unlinks and destroys ===\n";
    {
        ListStats::reset();
        dsl::List<int> b;
        for (int i = 1; i <= 5; ++i) {
            b.push_back(i);
        }
        const long destroyed_before = ListStats::nodes_destroyed;

        b.erase(b.begin());                 /* 删头 */
        dsl::List<int>::iterator it = b.begin();
        ++it;
        it = b.erase(it);                   /* 删中间那个，返回它的后继 */
        it = b.end();
        --it;
        b.erase(it);                        /* 删尾 */

        line("size", static_cast<long>(b.size()));
        line("check", check_of(b));
        line("values", values_of(b));
        line("nodes destroyed", ListStats::nodes_destroyed - destroyed_before);
        line("nodes alive", ListStats::nodes_alive());
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: clear and destructor order ===\n";
    {
        ListStats::reset();
        {
            dsl::List<int> c;
            for (int i = 0; i < 200; ++i) {
                c.push_back(i);
            }
            c.clear();
            line("after clear, size", static_cast<long>(c.size()));
            line("after clear, check", check_of(c));
            line("nodes alive", ListStats::nodes_alive());

            for (int i = 0; i < 3; ++i) {
                c.push_back(i);             /* 清空之后还能继续用 */
            }
            line("push after clear", static_cast<long>(c.size()));
        }
        line("alive after scope", ListStats::nodes_alive());
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: splice moves the whole chain ===\n";
    {
        ListStats::reset();
        dsl::List<int> dst;
        dst.push_back(1);
        dst.push_back(2);
        dsl::List<int> src;
        for (int i = 3; i <= 6; ++i) {
            src.push_back(i);
        }

        const int *address_before = &*src.begin();
        const long constructed_before = ListStats::values_constructed;
        const long destroyed_before = ListStats::values_destroyed;

        dst.splice(dst.end(), src);

        line("dst size", static_cast<long>(dst.size()));
        line("src size", static_cast<long>(src.size()));
        line("dst check", check_of(dst));
        line("dst values", values_of(dst));
        line("src values", values_of(src));

        bool address_kept = false;
        if (dst.size() == 6 && src.size() == 0) {
            dsl::List<int>::iterator it = dst.begin();
            ++it;
            ++it;                           /* 指向从 src 搬过来的第一个节点 */
            address_kept = (&*it == address_before);
        }
        line("address kept", address_kept ? "yes" : "no");
        line("constructed in splice", ListStats::values_constructed - constructed_before);
        line("destroyed in splice", ListStats::values_destroyed - destroyed_before);
    }

    return 0;
}
