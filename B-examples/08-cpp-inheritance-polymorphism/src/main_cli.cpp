/**
 * main_cli.cpp —— 命令行版
 *
 * 用法：
 *   app_cli                    用 markdown 渲染学生成绩表
 *   app_cli csv                用 csv 渲染
 *   app_cli json --tricky      渲染含逗号、引号、竖线、换行的表格
 *   app_cli --check            只跑自测
 *
 * 界面部分只有这里的 std::cout；算的部分全在 demo 命名空间，
 * 与 GUI 版共用同一份实现（src/report_demo.cpp）。
 */
#include "report_demo.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

void print_table(const Table &table, const std::string &format)
{
    bool ok = false;
    std::string error;
    const std::string text = demo::render(format, table, ok, error);
    if (!ok) {
        std::cout << "渲染失败：" << error << "\n";
        return;
    }
    std::cout << "--- " << format << " ---\n" << text << "\n";
}

}   /* namespace */

int main(int argc, char *argv[])
{
    std::cout << "示例 08 · 导出器：抽象基类、工厂与虚析构（命令行版）\n";

    std::string format = "markdown";
    bool tricky = false;
    bool only_check = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--check") {
            only_check = true;
        } else if (arg == "--tricky") {
            tricky = true;
        } else {
            format = arg;
        }
    }

    if (!only_check) {
        /* 界面不认识具体类型，问到的名字与说明都来自虚函数 */
        const std::vector<demo::FormatInfo> infos = demo::format_infos();
        std::cout << "\n可选格式（来自虚函数 name() 与 hint()）：\n";
        for (const demo::FormatInfo &info : infos) {
            std::cout << "  " << info.name << "：" << info.hint << "\n";
        }
        if (format == "yaml") {
            std::cout << "\n故意要一个不支持的格式：\n";
        }

        std::cout << "\n== 项目输出 ==\n";
        print_table(tricky ? demo::tricky_table() : demo::grade_table(), format);
        if (!tricky) {
            std::cout << "\n（加上 --tricky 换成含转义字符的表格）\n";
        }
    }

    const demo::CheckResult result = demo::run_self_tests();
    std::cout << "\n== 自测 ==\n";
    for (const std::string &line : result.lines) {
        std::cout << "  " << line << "\n";
    }
    std::cout << "\n  自测结果：" << result.summary() << "\n";
    return result.all_passed() ? 0 : 1;
}
