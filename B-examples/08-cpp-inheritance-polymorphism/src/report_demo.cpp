/**
 * report_demo.cpp —— 项目逻辑的实现
 *
 * 没有界面代码，也不打印。表格数据、格式信息、自测都在这里。
 */
#include "report_demo.hpp"

#include "exporter.hpp"

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace demo {

namespace {

/* 把多行文本拼起来，写期望输出时比一长串 \n 好读 */
std::string lines(std::initializer_list<const char *> parts)
{
    std::string out;
    for (const char *part : parts) {
        if (!out.empty()) {
            out += "\n";
        }
        out += part;
    }
    return out;
}

/* 自测的小工具：把每一项记下来，不打印 */
class Checker {
public:
    void check(bool ok, const std::string &what, const std::string &detail = std::string())
    {
        ++result_.total;
        std::ostringstream line;
        if (ok) {
            ++result_.passed;
            line << "[通过] " << result_.total << ". " << what;
        } else {
            ++result_.failed;
            line << "[不符] " << result_.total << ". " << what;
            if (!detail.empty()) {
                line << "（" << detail << "）";
            }
        }
        result_.lines.push_back(line.str());
    }

    CheckResult take() { return result_; }

private:
    CheckResult result_;
};

}   /* namespace */

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << total << " 项中 " << passed << " 项通过";
    if (failed == 0) {
        os << "，全部通过";
    } else {
        os << "，" << failed << " 项不符";
    }
    return os.str();
}

Table grade_table()
{
    Table table;
    table.title = "学生成绩";
    table.headers = {"姓名", "语文", "数学"};
    table.rows = {
        {"小明", "78", "92"},
        {"小红", "95", "88"},
        {"小刚", "65", "71"},
    };
    return table;
}

Table tricky_table()
{
    Table table;
    table.title = "转义对照";
    table.headers = {"名称", "备注"};
    table.rows = {
        {"逗号", "甲,乙"},
        {"引号", "说\"你好\""},
        {"竖线", "a|b"},
        {"多行", "第一行\n第二行"},
    };
    return table;
}

Table tiny_table()
{
    Table table;
    table.title = "转义测试";
    table.headers = {"名称", "备注"};
    table.rows = {
        {"逗号", "甲,乙"},
        {"引号", "说\"你好\""},
        {"竖线", "a|b"},
    };
    return table;
}

std::vector<FormatInfo> format_infos()
{
    std::vector<FormatInfo> infos;
    for (const std::string &format : supported_formats()) {
        const std::unique_ptr<Exporter> exporter = make_exporter(format);
        if (exporter == nullptr) {
            continue;
        }
        /* name() 与 hint() 都是虚函数：界面不认识具体类型，照样问得出来 */
        infos.push_back(FormatInfo{exporter->name(), exporter->hint()});
    }
    return infos;
}

std::string render(const std::string &format, const Table &table,
                   bool &ok, std::string &error)
{
    ok = true;
    error.clear();

    const std::unique_ptr<Exporter> exporter = make_exporter(format);
    if (exporter == nullptr) {
        ok = false;
        error = "没有这种格式：" + format;
        return std::string();
    }
    return exporter->render_checked(table);
}

CheckResult run_self_tests()
{
    Checker c;

    /* 1–2. 工厂造得出三种格式，问出来的名字与请求一致 */
    const std::vector<std::string> formats = supported_formats();
    c.check(formats.size() == 3, "工厂一共支持 3 种格式",
            "实际 " + std::to_string(formats.size()) + " 种");
    bool names_match = true;
    for (const std::string &format : formats) {
        const std::unique_ptr<Exporter> exporter = make_exporter(format);
        if (exporter == nullptr || exporter->name() != format) {
            names_match = false;
        }
    }
    c.check(names_match, "每种格式的 name() 都与工厂认的名字一致");

    /* 3. 不认识的名字返回空指针，而不是抛异常或造一个假的 */
    c.check(make_exporter("yaml") == nullptr, "工厂对不认识的格式返回空指针");

    /* 4. hint() 三种各不相同，说明走的是各自的虚函数 */
    const std::vector<FormatInfo> infos = format_infos();
    bool hints_differ = infos.size() == 3 && infos[0].hint != infos[1].hint
                        && infos[1].hint != infos[2].hint
                        && infos[0].hint != infos[2].hint;
    c.check(hints_differ, "三种格式的 hint() 互不相同（虚函数分派生效）");

    /* 5–7. 三种格式的转义结果与期望逐字相同 */
    bool ok = false;
    std::string error;
    const Table tiny = tiny_table();

    const std::string expected_csv = lines({
        "名称,备注",
        "逗号,\"甲,乙\"",
        "引号,\"说\"\"你好\"\"\"",
        "竖线,a|b",
    });
    const std::string actual_csv = render("csv", tiny, ok, error);
    c.check(ok && actual_csv == expected_csv, "CSV：含逗号的字段加引号，字段里的引号双写",
            ok ? actual_csv : error);

    const std::string expected_markdown = lines({
        "| 名称 | 备注 |",
        "| --- | --- |",
        "| 逗号 | 甲,乙 |",
        "| 引号 | 说\"你好\" |",
        "| 竖线 | a\\|b |",
    });
    const std::string actual_markdown = render("markdown", tiny, ok, error);
    c.check(ok && actual_markdown == expected_markdown, "Markdown：竖线转义，逗号原样保留",
            ok ? actual_markdown : error);

    const std::string expected_json = lines({
        "{",
        "  \"title\": \"转义测试\",",
        "  \"columns\": [\"名称\", \"备注\"],",
        "  \"rows\": [",
        "    [\"逗号\", \"甲,乙\"],",
        "    [\"引号\", \"说\\\"你好\\\"\"],",
        "    [\"竖线\", \"a|b\"]",
        "  ]",
        "}",
    });
    const std::string actual_json = render("json", tiny, ok, error);
    c.check(ok && actual_json == expected_json, "JSON：引号与反斜杠转义，结构带缩进",
            ok ? actual_json : error);

    /* 8. 多行内容：CSV 里原样换行，Markdown 换成 <br> */
    const Table multi = tricky_table();
    const std::string csv_multi = render("csv", multi, ok, error);
    const std::string markdown_multi = render("markdown", multi, ok, error);
    c.check(csv_multi.find("\"第一行\n第二行\"") != std::string::npos,
            "CSV：字段里的换行放进引号内原样保留");
    c.check(markdown_multi.find("第一行<br>第二行") != std::string::npos,
            "Markdown：字段里的换行写成 <br>");

    /* 9. 三种格式渲染同一张表，结果两两不同 */
    const Table grade = grade_table();
    const std::string csv_grade = render("csv", grade, ok, error);
    const std::string markdown_grade = render("markdown", grade, ok, error);
    const std::string json_grade = render("json", grade, ok, error);
    c.check(csv_grade != markdown_grade && markdown_grade != json_grade
                && csv_grade != json_grade,
            "同一张表，三种格式给出三种不同的文本");

    /* 10. 非虚的 render_checked 先挡下不合法的表格 */
    Table broken;
    broken.title = "列数不一致";
    broken.headers = {"甲", "乙"};
    broken.rows = {{"1"}, {"2", "3"}};
    const std::string broken_text = render("csv", broken, ok, error);
    c.check(ok && broken_text.find("表格不合法") != std::string::npos,
            "render_checked 挡下每行列数不一致的表格", broken_text);

    Table empty;
    empty.title = "空表";
    empty.headers = {"甲"};
    const std::string empty_text = render("csv", empty, ok, error);
    c.check(ok && empty_text.find("没有数据行") != std::string::npos,
            "render_checked 挡下没有数据行的表格", empty_text);

    /* 11. 格式名不认识时给出原因，而不是崩掉 */
    const std::string bad = render("yaml", grade, ok, error);
    c.check(!ok && bad.empty() && error.find("yaml") != std::string::npos,
            "请求不支持的格式时返回原因", error);

    /* 12–13. 虚析构：经基类指针删除，派生部分也要释放干净 */
    const std::size_t live_before = Exporter::live_count();
    const int tags_before = exporter_tag_live_count();
    {
        std::vector<std::unique_ptr<Exporter>> pool;
        for (const std::string &format : formats) {
            pool.push_back(make_exporter(format));
        }
        c.check(Exporter::live_count() == live_before + 3,
                "工厂造出的三个对象都还活着",
                "live = " + std::to_string(Exporter::live_count()));
    }
    c.check(Exporter::live_count() == live_before,
            "unique_ptr 释放后基类对象计数回到原值",
            "live = " + std::to_string(Exporter::live_count()));
    c.check(exporter_tag_live_count() == tags_before,
            "派生类里的探针成员也析构了，说明虚析构生效",
            "tag = " + std::to_string(exporter_tag_live_count()));

    return c.take();
}

}   /* namespace demo */
