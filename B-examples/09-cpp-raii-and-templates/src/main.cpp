/**
 * main.cpp —— 小库的使用者
 *
 * 用法：
 *   app                 用默认文件名 raii_sample.txt
 *   app 别的名字.txt     换一个文件名
 *
 * 项目做的事：
 *   1. 用 FileHandle 写一个数据文件（每行一个数，可带注释与空行）
 *   2. 用 ScopedPath 保证程序结束时把它删掉
 *   3. 用 FileHandle 读回来，装进 FixedVector<double, 32>
 *   4. 用函数模板求和、求最大值，用全特化把结果格式化
 *   5. 跑自测，逐项核对 RAII 与模板的行为
 *
 * 用到的小库全在 include/ 下，编成静态库 mini（见 CMakeLists.txt）。
 */
#include "file_handle.hpp"
#include "fixed_vector.hpp"

#include <cstddef>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

/* 编译期就能核对：持有资源的类型不该能拷贝，移动应当不抛异常 */
static_assert(!std::is_copy_constructible<FileHandle>::value,
              "FileHandle 持有文件，不能允许拷贝");
static_assert(std::is_nothrow_move_constructible<FileHandle>::value,
              "移动只是搬指针，应当标 noexcept");
static_assert(!std::is_copy_constructible<ScopedPath>::value,
              "删文件的责任只能有一个主人");

namespace {

/* 写进数据文件的内容：注释、空行、正数、负数都有 */
const char *const kSampleLines[] = {
    "# 每行一个数，空行与 # 开头的行会被跳过",
    "3.5",
    "1.25",
    "",
    "8",
    "-2.5",
    "4.75",
};

class SelfTest {
public:
    void check(bool ok, const std::string &what, const std::string &detail = std::string())
    {
        ++total_;
        if (ok) {
            ++passed_;
            std::cout << "  [通过] " << total_ << ". " << what << "\n";
        } else {
            ++failed_;
            std::cout << "  [不符] " << total_ << ". " << what;
            if (!detail.empty()) {
                std::cout << "（" << detail << "）";
            }
            std::cout << "\n";
        }
    }

    int report() const
    {
        std::cout << "\n  自测结果：" << total_ << " 项中 " << passed_ << " 项通过";
        if (failed_ == 0) {
            std::cout << "，全部通过\n";
        } else {
            std::cout << "，" << failed_ << " 项不符\n";
        }
        return failed_ == 0 ? 0 : 1;
    }

private:
    int total_ = 0;
    int passed_ = 0;
    int failed_ = 0;
};

/* 写数据文件。函数返回时 handle 析构，文件自动关闭 */
void write_sample(const std::string &path)
{
    FileHandle handle(path, "w");
    for (const char *line : kSampleLines) {
        handle.write_line(line);
    }
}

/* 读数据文件，返回有效数据行。skipped 记下跳过了几行 */
std::vector<std::string> read_sample(const std::string &path, int &skipped)
{
    std::vector<std::string> kept;
    skipped = 0;
    FileHandle handle(path, "r");

    std::string line;
    while (handle.read_line(line)) {
        if (line.empty() || line[0] == '#') {
            ++skipped;
            continue;
        }
        kept.push_back(line);
    }
    return kept;
}

/* 把文本行转成 double，失败时 ok 置 false */
std::vector<double> parse_numbers(const std::vector<std::string> &lines, bool &ok)
{
    std::vector<double> values;
    ok = true;
    for (const std::string &line : lines) {
        std::istringstream is(line);
        double value = 0.0;
        char extra = '\0';
        if (!(is >> value) || (is >> extra)) {
            ok = false;
            return values;
        }
        values.push_back(value);
    }
    return values;
}

int run_self_tests(const std::string &scratch)
{
    std::cout << "\n== 自测 ==\n";
    SelfTest t;

    /* 自测自己产生的临时文件，结束时一并删掉。
       它声明在最前面，因此最后析构，保证前面那些句柄都已关闭。 */
    ScopedPath scratch_guard(scratch);

    /* 1–3. 打开、写、读 */
    const std::size_t live_before = FileHandle::live_count();
    const std::size_t open_before = FileHandle::open_count();
    {
        FileHandle writer(scratch, "w");
        t.check(writer.is_open() && FileHandle::open_count() == open_before + 1
                    && FileHandle::live_count() == live_before + 1,
                "打开文件后，对象计数与打开计数都加一");

        writer.write_line("第一行");
        writer.write_line("第二行");
        writer.write_line("第三行");
    }
    t.check(FileHandle::open_count() == open_before,
            "离开作用域后文件已关闭，不必手写 fclose");

    {
        FileHandle reader(scratch, "r");
        std::string line;
        const bool first = reader.read_line(line);
        const bool first_ok = first && line == "第一行";
        bool all_ok = first_ok;
        int count = first ? 1 : 0;
        while (reader.read_line(line)) {
            ++count;
        }
        t.check(all_ok && count == 3, "读回三行，内容与写入一致",
                "读到 " + std::to_string(count) + " 行");
        t.check(!reader.read_line(line), "读到文件末尾时返回 false");
    }

    /* 4–5. 移动：句柄换主人，源对象变成空句柄 */
    {
        FileHandle source(scratch, "r");
        FileHandle moved(std::move(source));
        t.check(moved.is_open() && !source.is_open(),
                "移动构造后源对象不再持有文件");
        FileHandle target;
        target = std::move(moved);
        t.check(target.is_open() && !moved.is_open(),
                "移动赋值后同样，且目标接过了文件");
        t.check(FileHandle::open_count() == open_before + 1,
                "两次移动都没有多打开文件，打开计数始终为一");
    }

    /* 6. 提前关闭，析构时再关一次也无害 */
    {
        FileHandle handle(scratch, "r");
        handle.close();
        t.check(!handle.is_open() && FileHandle::open_count() == open_before,
                "提前 close() 之后计数回落，析构时不会重复关闭");
    }

    /* 7. 抛异常时照样释放 */
    bool caught = false;
    try {
        FileHandle handle(scratch, "r");
        throw std::runtime_error("故意抛出，看看文件会不会漏关");
    } catch (const std::runtime_error &) {
        caught = true;
    }
    t.check(caught && FileHandle::open_count() == open_before,
            "抛异常离开作用域时，文件照样被关闭");

    /* 8–9. ScopedPath：析构时删文件。
       注意声明顺序：creator 在后、guard 在前，于是 creator 先析构（关文件），
       guard 后析构（删文件）。反过来写，Windows 上会删不掉还开着的文件。 */
    const std::string temporary = scratch + ".scoped";
    {
        ScopedPath guard(temporary);
        FileHandle creator(temporary, "w");
        creator.write_line("临时文件");
        t.check(guard.exists() && file_exists(temporary), "作用域内文件确实存在");
    }
    t.check(!file_exists(temporary), "离开作用域后文件被删掉了");

    {
        ScopedPath guard(temporary);
        FileHandle creator(temporary, "w");
        creator.write_line("要留下的文件");
        guard.release();
    }
    t.check(file_exists(temporary), "release() 之后文件被留下");
    std::remove(temporary.c_str());         /* 自己收拾干净 */

    /* 10. 定容容器满了就拒绝，不越界 */
    FixedVector<int, 3> small;
    const bool first_ok = small.push_back(1);
    const bool second_ok = small.push_back(2);
    const bool third_ok = small.push_back(3);
    const bool fourth_ok = small.push_back(4);
    t.check(first_ok && second_ok && third_ok && !fourth_ok && small.size() == 3,
            "FixedVector<int, 3> 第 4 次 push_back 被拒绝，长度仍是 3");

    /* 11–12. 同一个函数模板对两种容器都成立 */
    FixedVector<double, 8> fixed;
    std::vector<double> dynamic;
    for (int i = 1; i <= 5; ++i) {
        fixed.push_back(static_cast<double>(i));
        dynamic.push_back(static_cast<double>(i));
    }
    t.check(sum_of(fixed) == sum_of(dynamic) && sum_of(fixed) == 15.0,
            "sum_of 对 FixedVector 与 std::vector 得到相同结果",
            to_text(sum_of(fixed)));
    t.check(max_of(fixed) == 5.0 && max_of(dynamic) == 5.0,
            "max_of 对两种容器同样成立");

    /* 13–15. 通用模板与两个全特化 */
    t.check(to_text(42) == "42", "to_text<int> 走通用模板", to_text(42));
    t.check(to_text(3.14159265) == "3.142",
            "to_text<double> 走全特化，固定三位小数", to_text(3.14159265));
    t.check(to_text(true) == "是" && to_text(false) == "否",
            "to_text<bool> 走全特化，输出是或否，不是 1 和 0");

    /* 16. 范围 for 靠的是 begin/end */
    FixedVector<int, 4> looped;
    for (int i = 1; i <= 4; ++i) {
        looped.push_back(i);
    }
    int walked = 0;
    for (const int value : looped) {
        walked += value;
    }
    t.check(walked == 10, "FixedVector 能用范围 for 遍历", std::to_string(walked));

    return t.report();
}

}   /* namespace */

int main(int argc, char *argv[])
{
    const std::string path = argc > 1 ? argv[1] : "raii_sample.txt";
    std::cout << "示例 09 · 小库与使用者（RAII + 模板）\n";
    std::cout << "数据文件：" << path << "\n";

    /* 这一行就决定了：不管后面怎么退出，文件都会被删掉 */
    ScopedPath guard(path);

    std::cout << "\n== 1. 写出数据文件 ==\n";
    write_sample(path);
    std::cout << "  写入 " << (sizeof(kSampleLines) / sizeof(kSampleLines[0]))
              << " 行（含注释与空行）\n";

    std::cout << "\n== 2. 读回并解析 ==\n";
    int skipped = 0;
    const std::vector<std::string> lines = read_sample(path, skipped);
    bool ok = false;
    const std::vector<double> values = parse_numbers(lines, ok);
    if (!ok) {
        std::cout << "  数据文件格式不对，读取失败\n";
        return 2;
    }
    std::cout << "  跳过 " << skipped << " 行（注释与空行），留下 "
              << values.size() << " 个有效数据\n";
    for (const double value : values) {
        std::cout << "    " << to_text(value) << "\n";
    }

    std::cout << "\n== 3. 装进 FixedVector<double, 32> 并统计 ==\n";
    FixedVector<double, 32> numbers;
    bool room_enough = true;
    for (const double value : values) {
        if (!numbers.push_back(value)) {
            room_enough = false;
        }
    }
    if (!room_enough) {
        std::cout << "  数据太多，容器装不下（最多 " << numbers.capacity() << " 个）\n";
        return 3;
    }
    const double total = sum_of(numbers);
    const double maximum = max_of(numbers);
    std::cout << "  个数 " << numbers.size()
              << "，总和 " << to_text(total)
              << "，最大 " << to_text(maximum)
              << "，平均 " << to_text(total / static_cast<double>(numbers.size()))
              << "\n";

    std::cout << "\n== 4. 模板与全特化 ==\n";
    std::cout << "  to_text<int>(42)           = " << to_text(42) << "\n";
    std::cout << "  to_text<double>(3.14159265) = " << to_text(3.14159265)
              << "  ← 全特化，固定三位小数\n";
    std::cout << "  to_text<bool>(true)        = " << to_text(true)
              << "  ← 全特化，不是 1\n";

    std::cout << "\n== 5. 退出前的状态 ==\n";
    std::cout << "  活着的句柄对象 " << FileHandle::live_count()
              << " 个，打开着的文件 " << FileHandle::open_count() << " 个\n";

    const int result = run_self_tests(path + ".check");

    std::cout << "\n== 6. 收尾 ==\n";
    std::cout << "  main 即将返回，ScopedPath 会删掉 " << guard.path() << "\n";
    return result;
}
