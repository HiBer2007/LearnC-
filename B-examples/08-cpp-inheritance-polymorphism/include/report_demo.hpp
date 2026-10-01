/**
 * report_demo.hpp —— 项目逻辑，不依赖任何界面
 *
 * 命令行版（src/main_cli.cpp）与 GUI 版（src/main_gui.cpp）都链接它。
 * 这里的函数只负责算，不打印：表格从哪来、结果怎么显示，由界面决定。
 */
#ifndef REPORT_DEMO_HPP
#define REPORT_DEMO_HPP

#include "table.hpp"

#include <string>
#include <vector>

namespace demo {

/** 演示用的学生成绩表 */
Table grade_table();

/** 含逗号、引号、竖线的表格，用来对比三种格式各自的转义写法 */
Table tricky_table();

/** 自测用的最小表格 */
Table tiny_table();

/** 从虚函数问出来的格式信息，界面据此画按钮或面板 */
struct FormatInfo {
    std::string name;
    std::string hint;
};

/** 依次问工厂要一个导出器，再问它 name() 与 hint() */
std::vector<FormatInfo> format_infos();

/** 按格式名渲染表格。名字不认识时 ok 置 false 并写明原因 */
std::string render(const std::string &format, const Table &table,
                   bool &ok, std::string &error);

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    int total = 0;
    int passed = 0;
    int failed = 0;
    std::vector<std::string> lines;

    bool all_passed() const { return failed == 0; }
    std::string summary() const;
};

/** 逐项核对工厂、虚函数分派、三种格式的转义与虚析构 */
CheckResult run_self_tests();

}   /* namespace demo */

#endif /* REPORT_DEMO_HPP */
