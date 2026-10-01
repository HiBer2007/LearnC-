/**
 * 示例 02 · C++ 单文件调试
 * 主题：类、STL 容器、引用、范围 for
 *
 * ── 怎么用 ─────────────────────────────────────────────
 *   1. 用 VS Code 打开【本文件夹】（文件 → 打开文件夹）
 *   2. 在行号左边点一下打断点
 *   3. 按 F5，选：
 *        「GDB · 调试当前 .cpp 文件」     → g++ 编译 + GDB 调试
 *        「VS2022 · 调试当前 .cpp 文件」  → cl 编译 + VS2022 调试器
 *
 * ── 为什么要用 g++ 而不是 gcc？ ──────────────────────────
 *   gcc 编译 .cpp 时确实按 C++ 编译，但【不会自动链接 libstdc++】，
 *   于是 std::cout / std::vector 全会报 undefined reference。
 *   所以 C++ 必须用 g++。（MSVC 的 cl.exe 没这个问题，它按扩展名判断）
 *
 * ── 建议的断点位置 ──────────────────────────────────────
 *   第 54 行  std::vector<Student> list = {...};   看 vector 里的元素
 *   第 63 行  std::sort(list.begin(), list.end(),  对比排序前后
 *   第 77 行  for (const Student &s : list)        看引用变量 s
 */
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

/* ── 一个简单的类，用来看调试器怎么展开对象 ── */
class Student {
public:
    Student(std::string name, int score)
        : name_(std::move(name)), score_(score) {}

    const std::string &name() const { return name_; }
    int score() const { return score_; }

    /* 加分，用来演示「在调试器里改值会真的影响程序」 */
    void add_bonus(int bonus) { score_ += bonus; }

private:
    std::string name_;
    int score_;
};

static void print_all(const std::vector<Student> &list)
{
    for (const Student &s : list) {
        std::cout << "  " << s.name() << "\t" << s.score() << "\n";
    }
}

int main()
{
    std::cout << "== 1. 构造对象列表 ==\n";
    std::vector<Student> list = {
        {"小明", 78},
        {"小红", 92},
        {"小刚", 65},
        {"小美", 88},
    };
    print_all(list);

    std::cout << "\n== 2. 按分数从高到低排序 ==\n";
    std::sort(list.begin(), list.end(),
              [](const Student &a, const Student &b) {
                  return a.score() > b.score();
              });
    print_all(list);

    std::cout << "\n== 3. 给前三名各加 5 分 ==\n";
    for (std::size_t i = 0; i < 3 && i < list.size(); ++i) {
        list[i].add_bonus(5);
    }
    print_all(list);

    std::cout << "\n== 4. 用引用做统计 ==\n";
    int total = 0;
    for (const Student &s : list) {
        total += s.score();
    }
    const double avg = static_cast<double>(total) / list.size();
    std::cout << "  总分 " << total
              << "，平均 " << avg << "\n";

    return 0;
}
