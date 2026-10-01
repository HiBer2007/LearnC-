/**
 * exporter.cpp —— 三个派生类与工厂的实现
 *
 * 每个派生类只做一件事：把自己那种格式写出来。
 * 公共的检查、对象计数、创建逻辑都在基类与工厂里。
 */
#include "exporter.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace {

/* ── 派生类里的生存期探针 ─────────────────────────────────
   每个派生类都放一个 LifetimeTag 成员。
   经基类指针 delete 时，如果基类析构函数不是虚的，
   派生类的析构函数不会跑，这个成员的析构也就不会执行，
   tag_live 便归不了零 —— 自测里正是靠它证明虚析构生效。 */
struct LifetimeTag {
    static int live;
    LifetimeTag() { ++live; }
    LifetimeTag(const LifetimeTag &) = delete;
    LifetimeTag &operator=(const LifetimeTag &) = delete;
    ~LifetimeTag() { --live; }
};

int LifetimeTag::live = 0;

/* ── CSV ───────────────────────────────────────────────── */

bool csv_needs_quote(const std::string &field)
{
    return field.find(',') != std::string::npos
        || field.find('"') != std::string::npos
        || field.find('\n') != std::string::npos
        || field.find('\r') != std::string::npos;
}

std::string csv_field(const std::string &field)
{
    if (!csv_needs_quote(field)) {
        return field;
    }
    std::string out = "\"";
    for (const char ch : field) {
        if (ch == '"') {
            out += '"';             /* 字段里的引号要双写 */
        }
        out += ch;
    }
    out += '"';
    return out;
}

class CsvExporter : public Exporter {
public:
    std::string name() const override { return "csv"; }

    std::string hint() const override
    {
        return "逗号分隔，字段含逗号或引号时加双引号";
    }

    std::string render(const Table &table) const override
    {
        std::string out;
        for (std::size_t i = 0; i < table.headers.size(); ++i) {
            out += (i == 0 ? "" : ",") + csv_field(table.headers[i]);
        }
        for (const std::vector<std::string> &row : table.rows) {
            out += "\n";
            for (std::size_t i = 0; i < row.size(); ++i) {
                out += (i == 0 ? "" : ",") + csv_field(row[i]);
            }
        }
        return out;
    }

private:
    LifetimeTag tag_;               /* 只是为了能被自测观察到 */
};

/* ── Markdown ──────────────────────────────────────────── */

std::string markdown_field(const std::string &field)
{
    std::string out;
    for (const char ch : field) {
        if (ch == '|') {
            out += "\\|";           /* 竖线是表格分隔符，要转义 */
        } else if (ch == '\n') {
            out += "<br>";          /* 表格里放不下换行 */
        } else if (ch != '\r') {
            out += ch;
        }
    }
    return out;
}

class MarkdownExporter : public Exporter {
public:
    std::string name() const override { return "markdown"; }

    std::string hint() const override
    {
        return "竖线表格，竖线转义、换行写成 <br>";
    }

    std::string render(const Table &table) const override
    {
        std::string out = "|";
        for (const std::string &header : table.headers) {
            out += " " + markdown_field(header) + " |";
        }
        out += "\n|";
        for (std::size_t i = 0; i < table.headers.size(); ++i) {
            out += " --- |";
        }
        for (const std::vector<std::string> &row : table.rows) {
            out += "\n|";
            for (const std::string &field : row) {
                out += " " + markdown_field(field) + " |";
            }
        }
        return out;
    }

private:
    LifetimeTag tag_;
};

/* ── JSON ──────────────────────────────────────────────── */

std::string json_field(const std::string &field)
{
    std::string out;
    for (const char ch : field) {
        switch (ch) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:   out += ch;     break;
        }
    }
    return out;
}

std::string json_array(const std::vector<std::string> &fields)
{
    std::string out = "[";
    for (std::size_t i = 0; i < fields.size(); ++i) {
        out += (i == 0 ? "" : ", ") + std::string("\"") + json_field(fields[i]) + "\"";
    }
    return out + "]";
}

class JsonExporter : public Exporter {
public:
    std::string name() const override { return "json"; }

    std::string hint() const override
    {
        return "带缩进的对象数组，值一律是字符串";
    }

    std::string render(const Table &table) const override
    {
        std::string out = "{\n";
        out += "  \"title\": \"" + json_field(table.title) + "\",\n";
        out += "  \"columns\": " + json_array(table.headers) + ",\n";
        out += "  \"rows\": [";
        for (std::size_t i = 0; i < table.rows.size(); ++i) {
            out += (i == 0 ? "\n" : ",\n");
            out += "    " + json_array(table.rows[i]);
        }
        out += "\n  ]\n}";
        return out;
    }

private:
    LifetimeTag tag_;
};

}   /* namespace */

/* ── 基类 ──────────────────────────────────────────────── */

std::size_t Exporter::live_count_ = 0;

Exporter::Exporter()
{
    ++live_count_;
}

Exporter::~Exporter()
{
    --live_count_;
}

std::string Exporter::render_checked(const Table &table) const
{
    if (!table.is_valid()) {
        return "表格不合法：每行的列数必须与表头一致";
    }
    if (table.row_count() == 0) {
        return "表格里没有数据行";
    }
    return render(table);           /* 这里通过虚函数分派到具体格式 */
}

int exporter_tag_live_count()
{
    return LifetimeTag::live;
}

/* ── 工厂 ──────────────────────────────────────────────── */

std::unique_ptr<Exporter> make_exporter(const std::string &format)
{
    if (format == "csv") {
        return std::make_unique<CsvExporter>();
    }
    if (format == "markdown") {
        return std::make_unique<MarkdownExporter>();
    }
    if (format == "json") {
        return std::make_unique<JsonExporter>();
    }
    return nullptr;                 /* 不认识的格式，交给调用方处理 */
}

std::vector<std::string> supported_formats()
{
    return {"csv", "markdown", "json"};
}
