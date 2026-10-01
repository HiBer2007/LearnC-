/**
 * main_cli.cpp —— 命令行版
 *
 * 用法：
 *   app_cli                          读默认的 data/sales.txt
 *   app_cli <文件>                    读指定文件
 *   app_cli <文件> --out <文件>        同时把报表原样写进另一个文件
 *   app_cli --selftest               只跑内置自测
 *
 * 输出与 01-c-stdlib-toolbox 的 C 版逐字节相同：
 *   01 用 printf / fputs，这里用 std::cout，两边都写同一份报表文本。
 *   计时写在标准错误上，因此标准输出里只有报表本身。
 */
#include "sales_report.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

/* 相对路径：必须在示例目录下运行，或用 .vscode 里配置的 cwd */
const char *kDefaultInput = "data/sales.txt";

int run_selftest()
{
    const sales::CheckResult result = sales::run_self_tests();

    std::cout << "== 自测 ==\n";
    for (const std::string &line : result.lines) {
        std::cout << "  " << line << "\n";
    }
    std::cout << "\n  自测结果：" << result.summary() << "\n";
    return result.all_passed() ? 0 : 1;
}

}   /* namespace */

int main(int argc, char *argv[])
{
    std::string input = kDefaultInput;
    std::string output;
    bool only_check = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--selftest") {
            only_check = true;
        } else if (arg == "--out" && i + 1 < argc) {
            i += 1;
            output = argv[i];
        } else if (arg.size() > 1 && arg[0] == '-') {
            std::cout << "不认识的选项：" << arg << "\n";
            std::cout << "用法：app_cli [文件] [--out 文件] [--selftest]\n";
            return 2;
        } else {
            input = arg;
        }
    }

    if (only_check) {
        return run_selftest();
    }

    std::cout << "示例 07-standard-library/02-cpp-io-report · iostream 报表生成器（命令行版）\n\n";

    const double started = sales::now_ms();

    bool ok = false;
    std::string error;
    const sales::Report report = sales::load(input, ok, error);
    if (!ok) {
        std::cerr << "读取失败：" << error << "\n";
        return 2;
    }
    const std::string text = sales::format_report(report, input);

    const double finished = sales::now_ms();

    std::cout << text;

    if (!output.empty()) {
        /* 用二进制模式写：换行原样保留成 LF，方便与 01 的输出逐字节对比 */
        std::ofstream sink(output, std::ios::binary);
        if (!sink) {
            std::cerr << "写不了 " << output << "\n";
            return 2;
        }
        sink << text;
        sink.close();
        std::cerr << "[输出] 报表已写入 " << output << "\n";
    }

    std::cerr << "[计时] 读文件并生成报表 " << std::fixed << std::setprecision(3)
              << (finished - started) << " 毫秒\n";
    return 0;
}
