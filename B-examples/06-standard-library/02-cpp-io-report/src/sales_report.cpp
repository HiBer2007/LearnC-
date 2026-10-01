/**
 * sales_report.cpp —— C++ 版销售记录报表，核心逻辑
 *
 * 这里不出现任何界面代码：命令行版与两份界面版都调用本文件，
 * 因此三边算出来的报表逐字节相同，也与 01-c-stdlib-toolbox 的 C 版相同。
 *
 * 演示到的标准库（对应教材 B 段各章）：
 *   B-01  <iostream> <iomanip> <sstream> <fstream>
 *         流的状态、setw 与对齐、ostringstream 拼文本、ifstream 读文件
 *   B-02  <string>   查找、比较、substr、stol / stod 数字互转
 *   B-06  <chrono>   steady_clock 测耗时
 */
#include "sales_report.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace sales {

namespace {

/** 把文本转成 long，整个串必须都是数字才算成功 */
bool to_long(const std::string &text, long &out)
{
    try {
        std::size_t used = 0;
        const long value = std::stol(text, &used, 10);
        if (used != text.size()) {
            return false;           /* 后面还有没吃掉的字符，例如 "12x" */
        }
        out = value;
        return true;
    } catch (const std::exception &) {
        return false;               /* 不是数字，或超出 long 的取值范围 */
    }
}

/** 把文本转成 double，整个串必须都是数字才算成功 */
bool to_double(const std::string &text, double &out)
{
    try {
        std::size_t used = 0;
        const double value = std::stod(text, &used);
        if (used != text.size()) {
            return false;
        }
        out = value;
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

/** 报表第 1 段的表头与分隔线 */
const char *kHead1 = "排名  商品              订单数    数量         金额      占比\n";
const char *kRule1 = "----  ----------------  ------  ------  -----------  --------\n";

/** 报表第 2 段的表头与分隔线 */
const char *kHead2 = "商品              订单数    数量         金额      占比\n";
const char *kRule2 = "----------------  ------  ------  -----------  --------\n";

/** 报表第 3 段的表头与分隔线 */
const char *kHead3 = "排名  日期        商品                数量         金额\n";
const char *kRule3 = "----  ----------  ----------------  ------  -----------\n";

}   /* namespace */

/* ── 解析与聚合 ───────────────────────────────────────────── */

std::vector<std::string> split_fields(const std::string &line)
{
    std::istringstream stream(line);
    std::vector<std::string> fields;
    std::string field;

    /* operator>> 会跳过任意多个空白（空格、制表符、回车），
       正好是 C 版里 strspn / strcspn 手工做的事 */
    while (stream >> field) {
        fields.push_back(field);
    }
    return fields;
}

bool is_date(const std::string &text)
{
    if (text.size() != 10) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i == 4 || i == 7) {
            if (text[i] != '-') {
                return false;
            }
        } else if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }
    return true;
}

bool parse_line(const std::string &line, Record &out)
{
    const std::vector<std::string> fields = split_fields(line);

    if (fields.size() != 4) {
        return false;
    }
    if (!is_date(fields[0])) {
        return false;
    }
    if (fields[1].empty() || fields[1].size() >= kNameMax) {
        return false;
    }

    long quantity = 0;
    double price = 0.0;
    if (!to_long(fields[2], quantity) || quantity <= 0) {
        return false;
    }
    if (!to_double(fields[3], price) || price < 0.0) {
        return false;
    }

    out.date = fields[0];
    out.product = fields[1];
    out.quantity = quantity;
    out.unit_price = price;
    out.amount = static_cast<double>(quantity) * price;
    return true;
}

bool add_record(Report &report, const Record &record)
{
    Product *slot = nullptr;

    for (Product &item : report.products) {
        if (item.name == record.product) {
            slot = &item;
            break;
        }
    }

    if (slot == nullptr) {
        if (report.products.size() >= kMaxProducts) {
            return false;
        }
        report.products.push_back(Product{});
        slot = &report.products.back();
        slot->name = record.product;
    }

    slot->orders += 1;
    slot->quantity += record.quantity;
    slot->amount += record.amount;

    report.total_quantity += record.quantity;
    report.total_amount += record.amount;
    report.lines_valid += 1;

    if (report.records.size() < kMaxRecords) {
        report.records.push_back(record);
    }
    return true;
}

/* ── 排序 ─────────────────────────────────────────────────── */

void sort_products_by_amount(Report &report)
{
    std::sort(report.products.begin(), report.products.end(),
              [](const Product &a, const Product &b) {
                  if (a.amount != b.amount) {
                      return a.amount > b.amount;        /* 金额大的在前 */
                  }
                  return a.name < b.name;                /* 并列时按名称升序 */
              });
}

void sort_products_by_name(Report &report)
{
    std::sort(report.products.begin(), report.products.end(),
              [](const Product &a, const Product &b) { return a.name < b.name; });
}

void sort_records_by_amount(Report &report)
{
    std::sort(report.records.begin(), report.records.end(),
              [](const Record &a, const Record &b) {
                  if (a.amount != b.amount) {
                      return a.amount > b.amount;
                  }
                  if (a.date != b.date) {
                      return a.date < b.date;
                  }
                  return a.product < b.product;
              });
}

/* ── 读文件 ───────────────────────────────────────────────── */

Report load(const std::string &path, bool &ok, std::string &error)
{
    Report report;
    std::ifstream source(path);
    std::string line;

    ok = false;
    error.clear();

    if (!source.is_open()) {
        error = "打不开文件 " + path;
        return report;
    }

    while (std::getline(source, line)) {
        report.lines_read += 1;

        /* 空行与注释行既不算有效数据行，也不算跳过行 */
        const std::string::size_type head = line.find_first_not_of(" \t\r\n");
        if (head == std::string::npos || line[head] == '#') {
            continue;
        }

        Record record;
        if (!parse_line(line, record)) {
            report.lines_skipped += 1;
            continue;
        }
        if (!add_record(report, record)) {
            report.lines_skipped += 1;
        }
    }

    if (source.bad()) {
        error = "读取 " + path + " 的过程中出错";
        return report;
    }

    sort_products_by_amount(report);
    sort_records_by_amount(report);
    ok = true;
    return report;
}

/* ── 拼报表 ───────────────────────────────────────────────── */

std::string format_report(const Report &report, const std::string &path)
{
    std::ostringstream out;
    std::vector<Product> by_name = report.products;      /* 第二段要按名称另排一遍 */
    const double total = report.total_amount;
    const double average = report.total_quantity > 0
        ? total / static_cast<double>(report.total_quantity)
        : 0.0;

    /* 第二段要按名称升序，而 report.products 已按金额排过，因此排这份副本 */
    std::sort(by_name.begin(), by_name.end(),
              [](const Product &a, const Product &b) { return a.name < b.name; });

    /* 左上角的概况。label 把名称补到 16 列，与 C 版的 "%-16s" 一致 */
    const auto label = [&out](const char *name) {
        out << std::left << std::setw(16) << name << std::right << ": ";
    };

    out << "==================== 销售记录报表 ====================\n";
    label("输入文件");
    out << path << "\n";
    label("读取行数");
    out << report.lines_read << "\n";
    label("有效数据行");
    out << report.lines_valid << "\n";
    label("跳过行数");
    out << report.lines_skipped << "\n";
    label("商品种类");
    out << report.products.size() << "\n";
    label("总数量");
    out << report.total_quantity << "\n";
    label("总金额");
    out << std::fixed << std::setprecision(2) << total << "\n";
    label("平均单价");
    out << std::fixed << std::setprecision(3) << average << "\n";

    /* 第 1 段：按金额降序 */
    out << "\n[1] 按金额降序\n";
    out << kHead1 << kRule1;
    for (std::size_t i = 0; i < report.products.size(); ++i) {
        const Product &item = report.products[i];
        const double share = total > 0.0 ? item.amount * 100.0 / total : 0.0;
        out << std::right << std::setw(4) << (i + 1)
            << "  " << std::left << std::setw(16) << item.name
            << "  " << std::right << std::setw(6) << item.orders
            << "  " << std::setw(6) << item.quantity
            << "  " << std::fixed << std::setprecision(2) << std::setw(11) << item.amount
            << "  " << std::setw(7) << share << "%\n";
    }
    out << std::left << std::setw(4) << "" << "  " << std::setw(16) << "合计"
        << "  " << std::right << std::setw(6) << report.lines_valid
        << "  " << std::setw(6) << report.total_quantity
        << "  " << std::fixed << std::setprecision(2) << std::setw(11) << total
        << "  " << std::setw(7) << 100.0 << "%\n";

    /* 第 2 段：按商品名升序 */
    out << "\n[2] 按商品名升序\n";
    out << kHead2 << kRule2;
    for (const Product &item : by_name) {
        const double share = total > 0.0 ? item.amount * 100.0 / total : 0.0;
        out << std::left << std::setw(16) << item.name
            << "  " << std::right << std::setw(6) << item.orders
            << "  " << std::setw(6) << item.quantity
            << "  " << std::fixed << std::setprecision(2) << std::setw(11) << item.amount
            << "  " << std::setw(7) << share << "%\n";
    }

    /* 第 3 段：单笔金额最高的前三笔 */
    out << "\n[3] 单笔金额最高的前三笔\n";
    out << kHead3 << kRule3;
    const std::size_t limit = std::min<std::size_t>(3, report.records.size());
    for (std::size_t i = 0; i < limit; ++i) {
        const Record &item = report.records[i];
        out << std::right << std::setw(4) << (i + 1)
            << "  " << std::left << std::setw(10) << item.date
            << "  " << std::setw(16) << item.product
            << "  " << std::right << std::setw(6) << item.quantity
            << "  " << std::fixed << std::setprecision(2) << std::setw(11) << item.amount
            << "\n";
    }

    out << "\n====================== 报表结束 ======================\n";
    return out.str();
}

/* ── 计时 ─────────────────────────────────────────────────── */

double now_ms()
{
    const auto mark = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(mark).count();
}

/* ── 内置自测 ─────────────────────────────────────────────── */

namespace {

struct Checker {
    CheckResult result;

    void check(bool ok, const std::string &title)
    {
        result.total += 1;
        if (ok) {
            result.passed += 1;
        } else {
            result.failed += 1;
        }
        std::ostringstream line;
        line << (ok ? "[通过] " : "[不通过] ") << result.total << ". " << title;
        result.lines.push_back(line.str());
    }
};

bool near(double value, double expected)
{
    const double diff = value - expected;
    return (diff < 0 ? -diff : diff) < 0.005;
}

}   /* namespace */

std::string CheckResult::summary() const
{
    std::ostringstream out;
    out << total << " 项中 " << passed << " 项通过，"
        << (failed == 0 ? "全部通过" : "有未通过项");
    return out.str();
}

CheckResult run_self_tests()
{
    Checker checker;

    /* 1–3：分词 */
    {
        const std::vector<std::string> fields =
            split_fields("2026-01-05  keyboard  3  199.00");
        checker.check(fields.size() == 4 && fields[0] == "2026-01-05"
                          && fields[3] == "199.00",
                      "split_fields 把一行拆成 4 个字段");
    }
    {
        const std::vector<std::string> fields =
            split_fields("2026-01-05\tkeyboard\t 3\t199.00");
        checker.check(fields.size() == 4 && fields[1] == "keyboard"
                          && fields[2] == "3",
                      "split_fields 把制表符与连续空白都当作分隔");
    }
    checker.check(split_fields("a b c d e").size() == 5,
                  "split_fields 拆出 5 个字段，交给调用方判长度");

    /* 4–5：解析正常行 */
    {
        Record record;
        checker.check(parse_line("2026-01-05  keyboard  3  199.00", record)
                          && record.quantity == 3
                          && near(record.unit_price, 199.00)
                          && record.product == "keyboard",
                      "parse_line 解析正常行，数量与单价都对");
        checker.check(near(record.amount, 597.00),
                      "parse_line 同时算出金额 3 × 199.00 = 597.00");
    }

    /* 6–10：各类坏行都该被拒 */
    {
        Record record;
        checker.check(!parse_line("2026-01-20  mouse  1", record),
                      "parse_line 拒绝只有 3 个字段的行");
        checker.check(!parse_line("2026-01-19  usb-hub  -  49.90", record),
                      "parse_line 拒绝数量不是数字的行");
        checker.check(!parse_line("2026-01-19  webcam  2  abc", record),
                      "parse_line 拒绝单价不是数字的行");
        checker.check(!parse_line("2026-01-19  webcam  0  329.00", record)
                          && !parse_line("2026-01-19  webcam  -2  329.00", record),
                      "parse_line 拒绝数量为 0 或负数的行");
        checker.check(!parse_line("2026/01/05  keyboard  3  199.00", record)
                          && !parse_line("20260105  keyboard  3  199.00", record),
                      "parse_line 只认 YYYY-MM-DD 形式的日期");
    }

    /* 11–14：聚合。手算得出的答案写在断言里。
       keyboard 7 × 199.00 = 1393.00
       usb-hub  7 × 199.00 = 1393.00   （与 keyboard 金额并列，用来验并列规则）
       monitor  2 × 899.00 = 1798.00
       mouse   12 ×  59.50 =  714.00                                */
    Report report;
    {
        const char *const lines[] = {
            "2026-01-05  keyboard  3  199.00",
            "2026-01-07  keyboard  4  199.00",
            "2026-01-06  monitor   2  899.00",
            "2026-01-09  mouse    12   59.50",
            "2026-01-11  usb-hub   7  199.00"
        };
        for (const char *text : lines) {
            Record record;
            if (parse_line(text, record)) {
                (void)add_record(report, record);
            }
        }
    }

    checker.check(report.products.size() == 4,
                  "add_record 把 5 条记录归成 4 种商品，同名商品并成一行");
    checker.check(report.products[0].orders == 2 && report.products[0].quantity == 7,
                  "add_record 累加订单数（keyboard 2 笔）与数量（7 件）");
    checker.check(near(report.products[0].amount, 1393.00),
                  "add_record 累加金额（keyboard 1393.00）");
    checker.check(near(report.total_amount, 5298.00) && report.total_quantity == 28,
                  "add_record 累加总量 28 与总额 5298.00");

    /* 15–17：排序 */
    sort_products_by_amount(report);
    checker.check(report.products.front().name == "monitor"
                      && report.products.back().name == "mouse",
                  "sort_products_by_amount 把金额最大的排到最前");
    checker.check(near(report.products[1].amount, report.products[2].amount)
                      && report.products[1].name == "keyboard"
                      && report.products[2].name == "usb-hub",
                  "金额并列时按名称升序，输出顺序稳定");

    {
        Report by_name = report;
        sort_products_by_name(by_name);
        checker.check(by_name.products[0].name == "keyboard"
                          && by_name.products[1].name == "monitor"
                          && by_name.products[2].name == "mouse"
                          && by_name.products[3].name == "usb-hub",
                      "按名称升序排出来是 keyboard、monitor、mouse、usb-hub");
    }

    /* 18–20：报表文本 */
    sort_records_by_amount(report);
    const std::string text = format_report(report, "data/sales.txt");

    checker.check(text.rfind("==================== 销售记录报表", 0) == 0,
                  "format_report 的头一行是报表标题");
    checker.check(text.find("monitor") != std::string::npos
                      && text.find("合计") != std::string::npos
                      && text.find("[3] 单笔金额最高的前三笔") != std::string::npos,
                  "format_report 里有商品名、合计行与第三段标题");
    checker.check(text.find("报表结束") != std::string::npos
                      && !text.empty()
                      && text[text.size() - 1] == '\n',
                  "format_report 以「报表结束」收尾且末尾有换行");

    return checker.result;
}

}   /* namespace sales */
