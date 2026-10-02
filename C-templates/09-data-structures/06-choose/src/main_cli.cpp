/* main_cli.cpp —— 练习模板 06 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它把六段需求与你填的表打印出来，再做一遍机械自查。
 *
 * 自查只看两件事：填了没有、依据字段的格式对不对。
 * **它不判断你选得对不对**——那由《配置步骤.md》里的自查表管。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 题面与答案里都有中文：控制台代码页不是 65001 时先执行 chcp 65001，
 * 否则看到的是乱码（不影响判据的几个数字）。
 */
#include <iomanip>
#include <iostream>

#include "choices.hpp"

namespace {

void line(const char *label, int value, int total)
{
    std::cout << std::left << std::setw(26) << label << ": " << value << " of " << total << "\n";
}

} /* namespace */

int main()
{
    std::cout << "=== choice table: six needs ===\n\n";
    for (int i = 0; i < dsc::kNeedCount; ++i) {
        dsc::print_row(i);
    }

    const dsc::Report report = dsc::check_table();
    std::cout << "=== self check (format only) ===\n";
    line("structure filled", report.structure_filled, dsc::kNeedCount);
    line("cost filled", report.cost_filled, dsc::kNeedCount);
    line("reason filled", report.reason_filled, dsc::kNeedCount);
    line("source filled", report.source_filled, dsc::kNeedCount);
    line("sources in the right form", report.sources_ok, dsc::kNeedCount);
    line("rows complete", report.rows_complete, dsc::kNeedCount);

    return 0;
}
