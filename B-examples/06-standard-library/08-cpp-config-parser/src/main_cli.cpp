/**
 * main_cli.cpp —— 命令行版
 *
 * 用法：
 *   app_cli                  读 data/app.ini
 *   app_cli 别的配置文件      读指定的文件
 *   app_cli --selftest       只跑自测
 *
 * 界面部分只有下面这些 std::cout；解析、取值、行号报错全在 cfg 命名空间里，
 * 与自测共用同一份实现。
 */
#include "config.hpp"

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

/* 一段里每条配置占一行：段、键、值、类型四列由列宽对齐 */
void print_section(const std::vector<cfg::FlatEntry> &flat, const std::string &section)
{
    std::cout << "[" << section << "]\n";
    bool any = false;
    for (const auto &[entry_section, key, text, type] : flat) {
        if (entry_section != section) {
            continue;
        }
        any = true;
        std::ostringstream line;
        line << "  " << std::left << std::setw(14) << key << " = " << std::setw(18) << text
             << "（" << type << "）";
        std::cout << line.str() << "\n";
    }
    if (!any) {
        std::cout << "  这一段没有条目\n";
    }
}

/* 取值示范：要什么类型、拿到什么，摆在一行里 */
template <typename T>
void print_lookup(const std::string &call, const std::optional<T> &value,
                  const std::string &note)
{
    std::ostringstream line;
    line << "  " << std::left << std::setw(44) << call << " → "
         << (value.has_value() ? cfg::to_text(cfg::Value{*value}) : std::string("没有"));
    if (!note.empty()) {
        line << "    " << note;
    }
    std::cout << line.str() << "\n";
}

}   /* namespace */

int main(int argc, char *argv[])
{
    std::string path = "data/app.ini";
    bool only_check = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--selftest") {
            only_check = true;
        } else if (arg.rfind("--", 0) == 0) {
            std::cout << "无法识别的参数：" << arg << "\n"
                      << "用法：app_cli [配置文件] [--selftest]\n";
            return 2;
        } else {
            path = arg;
        }
    }

    std::cout << "示例 06-standard-library/08-cpp-config-parser · INI 配置解析器（命令行版）\n";

    if (!only_check) {
        cfg::Config config;
        std::string error;

        std::cout << "\n== 项目输出 ==\n";
        std::cout << "配置文件：" << path << "\n";
        if (!config.load_file(path, error)) {
            std::cout << error << "\n";
        } else {
            const std::vector<std::string> &sections = config.sections();
            std::cout << "段 " << sections.size() << " 个，条目 " << config.size()
                      << " 条\n\n";

            const std::vector<cfg::FlatEntry> flat = config.flatten();
            for (std::size_t i = 0; i < sections.size(); ++i) {
                print_section(flat, sections[i]);
                std::cout << "\n";
            }

            std::cout << "取值示范（段, 键, 目标类型 → 结果）：\n";
            print_lookup("get<long long>(\"run\", \"top\")",
                         config.get<long long>("run", "top"), "");
            print_lookup("get<double>(\"run\", \"top\")",
                         config.get<double>("run", "top"), "整数放宽成浮点");
            print_lookup("get<std::string>(\"run\", \"top\")",
                         config.get<std::string>("run", "top"), "类型不匹配");
            print_lookup("get<long long>(\"app\", \"ratio\")",
                         config.get<long long>("app", "ratio"), "放宽是单向的");
            print_lookup("get<std::string>(\"app\", \"missing\")",
                         config.get<std::string>("app", "missing"), "键不存在");
            print_lookup("get_or<std::string>(\"app\", \"author\", 默认值)",
                         std::optional<std::string>(
                             config.get_or<std::string>("app", "author", "(未填写)")),
                         "缺键时给默认值");

            const std::vector<cfg::ParseError> &errors = config.errors();
            std::cout << "\n解析时记下 " << errors.size() << " 处问题：\n";
            if (errors.empty()) {
                std::cout << "  没有\n";
            }
            for (const cfg::ParseError &problem : errors) {
                std::cout << "  第 " << problem.line << " 行：" << problem.message << "\n"
                          << "      原文：" << problem.text << "\n";
            }
        }
    }

    const cfg::CheckResult result = cfg::run_self_tests();
    std::cout << "\n== 自测 ==\n";
    for (const std::string &line : result.lines) {
        std::cout << "  " << line << "\n";
    }
    std::cout << "\n  自测结果：" << result.summary() << "\n";
    return result.all_passed() ? 0 : 1;
}
