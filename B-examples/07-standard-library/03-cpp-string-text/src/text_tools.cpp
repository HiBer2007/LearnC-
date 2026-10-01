/**
 * text_tools.cpp —— 文本处理的实现
 *
 * 这里没有任何界面代码：不含 <windows.h>，也不打印任何东西。
 * 库里所有 std::string 承载的都是 UTF-8 字节，与源码字面量的执行字符集无关，
 * 因为核心库内部一律使用 u8"" 字面量。
 */
#include "text_tools.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace text {

namespace {

/* 自测的小工具：把每一项的结果记下来，不打印 */
class Checker {
public:
    void check(bool ok, const std::string &what, const std::string &detail = std::string())
    {
        ++result_.total;
        std::ostringstream line;
        if (ok) {
            ++result_.passed;
            line << u8"[通过] " << result_.total << ". " << what;
        } else {
            ++result_.failed;
            line << u8"[失败] " << result_.total << ". " << what;
            if (!detail.empty()) {
                line << u8"（" << detail << u8"）";
            }
        }
        result_.lines.push_back(line.str());
    }

    CheckResult take() { return result_; }

private:
    CheckResult result_;
};

bool is_space_byte(unsigned char byte)
{
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r'
           || byte == '\f' || byte == '\v';
}

std::string hex_byte(unsigned char byte)
{
    static const char digits[] = "0123456789ABCDEF";
    std::string out = "0x";
    out.push_back(digits[(byte >> 4) & 0x0F]);
    out.push_back(digits[byte & 0x0F]);
    return out;
}

/* 一段 view 是否落在 [base, base + size) 之内，用来证明零拷贝 */
bool inside(std::string_view part, std::string_view whole)
{
    if (part.empty()) {
        return part.data() >= whole.data() && part.data() <= whole.data() + whole.size();
    }
    return part.data() >= whole.data() && part.data() + part.size() <= whole.data() + whole.size();
}

}   /* namespace */

/* ---------------- 切分与拼接 ---------------- */

std::vector<std::string_view> split_view(std::string_view text, std::string_view delimiter)
{
    std::vector<std::string_view> parts;
    if (delimiter.empty()) {
        parts.push_back(text);
        return parts;
    }

    std::size_t pos = 0;
    while (true) {
        const std::size_t hit = text.find(delimiter, pos);
        if (hit == std::string_view::npos) {
            parts.push_back(text.substr(pos));
            break;
        }
        parts.push_back(text.substr(pos, hit - pos));
        pos = hit + delimiter.size();
    }
    return parts;
}

std::vector<std::string> split(std::string_view text, std::string_view delimiter)
{
    const std::vector<std::string_view> views = split_view(text, delimiter);
    std::vector<std::string> parts;
    parts.reserve(views.size());
    for (const std::string_view &view : views) {
        parts.emplace_back(view);
    }
    return parts;
}

std::vector<std::string_view> split_lines(std::string_view text)
{
    std::vector<std::string_view> lines;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t newline = text.find('\n', pos);
        const std::size_t end = (newline == std::string_view::npos) ? text.size() : newline;
        std::string_view line = text.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        lines.push_back(line);
        if (newline == std::string_view::npos) {
            break;
        }
        pos = newline + 1;
    }
    return lines;
}

std::string join(const std::vector<std::string> &parts, std::string_view separator)
{
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) {
            out.append(separator);
        }
        out.append(parts[i]);
    }
    return out;
}

/* ---------------- 修剪与大小写 ---------------- */

std::string trim_left(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size()
           && is_space_byte(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    return std::string(text.substr(begin));
}

std::string trim_right(std::string_view text)
{
    std::size_t end = text.size();
    while (end > 0 && is_space_byte(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(0, end));
}

std::string trim(std::string_view text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && is_space_byte(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && is_space_byte(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

std::string to_upper(std::string_view text)
{
    std::string out(text);
    for (char &ch : out) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (byte >= 'a' && byte <= 'z') {
            ch = static_cast<char>(byte - ('a' - 'A'));
        }
    }
    return out;
}

std::string to_lower(std::string_view text)
{
    std::string out(text);
    for (char &ch : out) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (byte >= 'A' && byte <= 'Z') {
            ch = static_cast<char>(byte + ('a' - 'A'));
        }
    }
    return out;
}

/* ---------------- 查找与替换 ---------------- */

std::vector<std::size_t> find_all(std::string_view text, std::string_view needle)
{
    std::vector<std::size_t> hits;
    if (needle.empty()) {
        return hits;
    }

    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t hit = text.find(needle, pos);
        if (hit == std::string_view::npos) {
            break;
        }
        hits.push_back(hit);
        pos = hit + needle.size();
    }
    return hits;
}

std::size_t replace_all(std::string &text, std::string_view from, std::string_view to)
{
    if (from.empty()) {
        return 0;               /* 空模式会永远匹配，直接拒绝 */
    }

    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();       /* 跳过刚换上去的内容，避免换进来的串又被匹配 */
        ++count;
    }
    return count;
}

/* ---------------- UTF-8 字节处理 ---------------- */

std::size_t utf8_sequence_length(unsigned char lead_byte)
{
    if (lead_byte < 0x80) {
        return 1;               /* 0xxxxxxx：单字节，ASCII */
    }
    if ((lead_byte & 0xE0) == 0xC0) {
        return 2;               /* 110xxxxx */
    }
    if ((lead_byte & 0xF0) == 0xE0) {
        return 3;               /* 1110xxxx：常用汉字在这一档 */
    }
    if ((lead_byte & 0xF8) == 0xF0) {
        return 4;               /* 11110xxx：增补平面 */
    }
    return 0;                   /* 10xxxxxx 是续字节，0xF8 以上不是合法首字节 */
}

std::size_t utf8_char_count(std::string_view text)
{
    std::size_t count = 0;
    for (const char ch : text) {
        if ((static_cast<unsigned char>(ch) & 0xC0) != 0x80) {
            ++count;            /* 续字节不计，其余每个字节开启一个字符 */
        }
    }
    return count;
}

bool utf8_is_valid(std::string_view text)
{
    std::size_t i = 0;
    while (i < text.size()) {
        const std::size_t length = utf8_sequence_length(static_cast<unsigned char>(text[i]));
        if (length == 0 || i + length > text.size()) {
            return false;
        }
        for (std::size_t k = 1; k < length; ++k) {
            if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) {
                return false;
            }
        }
        i += length;
    }
    return true;
}

std::string utf8_truncate(std::string_view text, std::size_t max_chars)
{
    std::size_t i = 0;
    std::size_t chars = 0;
    while (i < text.size() && chars < max_chars) {
        const std::size_t length = utf8_sequence_length(static_cast<unsigned char>(text[i]));
        if (length == 0 || i + length > text.size()) {
            break;              /* 遇到不合法的序列就停在这里，不猜它的长度 */
        }
        i += length;
        ++chars;
    }
    return std::string(text.substr(0, i));
}

std::string utf8_truncate_bytes(std::string_view text, std::size_t max_bytes)
{
    if (max_bytes >= text.size()) {
        return std::string(text);
    }

    /* 从 max_bytes 往回退：退到某个字符的首字节为止，那个字符整体不要 */
    std::size_t cut = max_bytes;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) {
        --cut;
    }
    return std::string(text.substr(0, cut));
}

/* ---------------- 文件与报告 ---------------- */

std::string read_text_file(const std::string &path, bool &ok, std::string &error)
{
    ok = false;
    error.clear();

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = std::string(u8"打不开文件：") + path;
        return std::string();
    }

    std::string content;
    char chunk[4096];
    while (in.read(chunk, static_cast<std::streamsize>(sizeof chunk))) {
        content.append(chunk, sizeof chunk);
    }
    content.append(chunk, static_cast<std::size_t>(in.gcount()));
    if (in.bad()) {
        error = std::string(u8"读取中途出错：") + path;
        return std::string();
    }

    ok = true;
    return content;
}

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << total << u8" 项中 " << passed << u8" 项通过";
    if (failed == 0) {
        os << u8"，全部通过";
    } else {
        os << u8"，" << failed << u8" 项失败";
    }
    return os.str();
}

std::string build_report(std::string_view sample, const std::string &source_label)
{
    std::ostringstream os;
    const std::vector<std::string_view> lines = split_lines(sample);

    os << u8"源文件：" << source_label << u8"（字节 " << sample.size()
       << u8"，字符 " << utf8_char_count(sample) << u8"，行 " << lines.size() << u8"）\n";
    os << u8"  按字节算比按字符算大：UTF-8 里一个汉字占 3 个字节\n";
    if (lines.empty()) {
        return os.str();
    }

    os << u8"\n各行统计（字节 / 字符）\n";
    for (std::size_t i = 0; i < lines.size(); ++i) {
        os << u8"  第 " << (i + 1) << u8" 行：" << lines[i].size() << u8" / "
           << utf8_char_count(lines[i]) << "\n";
    }

    const std::string_view first = lines[0];
    const std::string trimmed_first = trim(first);
    os << u8"\n修剪（第 1 行两端各留了两个空格）\n";
    os << u8"  修剪前 " << first.size() << u8" 字节，修剪后 " << trimmed_first.size()
       << u8" 字节，少了 " << (first.size() - trimmed_first.size()) << u8" 个空白字节\n";
    os << u8"  修剪后：" << trimmed_first << "\n";

    os << u8"\n大小写（只动 ASCII 字母，汉字与符号原样保留）\n";
    os << u8"  转大写：" << to_upper(trimmed_first) << "\n";

    const std::vector<std::string> parts = split(trimmed_first, u8"，");
    os << u8"\n切分：按「，」切第 1 行（修剪后），得到 " << parts.size() << u8" 段\n";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        os << u8"  [" << i << u8"] " << parts[i] << u8"（" << parts[i].size() << u8" 字节）\n";
    }

    const std::vector<std::string_view> views = split_view(trimmed_first, u8"，");
    os << u8"\n零拷贝切分：同一行换成 string_view，段数 " << views.size() << u8"\n";
    for (std::size_t i = 0; i < views.size(); ++i) {
        os << u8"  第 " << (i + 1) << u8" 段起点在原文的偏移 "
           << static_cast<std::size_t>(views[i].data() - trimmed_first.data())
           << u8"，长度 " << views[i].size() << u8" 字节\n";
    }
    os << u8"  这些 view 一个字节都没有复制：偏移 0 到 " << trimmed_first.size()
       << u8" 之间就是原来那块内存\n";

    const std::string needle = u8"std::string";
    const std::vector<std::size_t> hits = find_all(trimmed_first, needle);
    os << u8"\n查找与替换\n";
    os << u8"  「" << needle << u8"」出现 " << hits.size() << u8" 次，字节位置";
    for (const std::size_t hit : hits) {
        os << " " << hit;
    }
    os << "\n";
    std::string replaced(trimmed_first);
    const std::size_t replaced_count = replace_all(replaced, needle, u8"string");
    os << u8"  换成 string：" << replaced_count << u8" 处，长度 " << trimmed_first.size()
       << u8" → " << replaced.size() << u8" 字节\n";

    os << u8"\nUTF-8 首字节判定位数\n";
    struct Probe {
        const char *label;
        std::string_view bytes;
    };
    const Probe probes[] = {
        { u8"U+0041 'A'", u8"A" },
        { u8"U+00E9", u8"\u00E9" },
        { u8"U+4E2D", u8"\u4E2D" },
        { u8"U+1F642", u8"\U0001F642" },
    };
    for (const Probe &probe : probes) {
        const unsigned char lead = static_cast<unsigned char>(probe.bytes[0]);
        os << u8"  " << probe.label << u8" 首字节 " << hex_byte(lead) << u8"，共 "
           << utf8_sequence_length(lead) << u8" 字节\n";
    }

    os << u8"\n按字符与按字节截断（都在第 1 行修剪后的文本上做）\n";
    const std::string by_chars = utf8_truncate(trimmed_first, 20);
    os << u8"  按字符截断到 20 个字符：" << trimmed_first.size() << u8" → "
       << by_chars.size() << u8" 字节，" << utf8_char_count(by_chars) << u8" 个字符，结构合法 "
       << (utf8_is_valid(by_chars) ? u8"是" : u8"否") << "\n";
    os << u8"    " << by_chars << "\n";
    const std::string by_bytes = utf8_truncate_bytes(trimmed_first, 40);
    os << u8"  按字节截断到 40 字节：实际 " << by_bytes.size() << u8" 字节，结构合法 "
       << (utf8_is_valid(by_bytes) ? u8"是" : u8"否") << "\n";
    os << u8"    " << by_bytes << "\n";
    const std::string_view naive = trimmed_first.substr(0, 40);
    os << u8"  直接取前 40 个字节（不推荐）：结构合法 "
       << (utf8_is_valid(naive) ? u8"是" : u8"否") << u8"，末尾的多字节序列被切成两半\n";

    os << u8"\nstring_view 不拥有内存\n";
    std::string owner(trimmed_first);
    const std::string_view borrowed(owner);
    os << u8"  指向的 string 还活着时：取它的前几个字符得到 "
       << utf8_truncate(split_view(borrowed, u8"，")[0], 10) << u8"\n";
    const char *before = borrowed.data();
    owner.append(100, '#');
    os << u8"  给那个 string 追加 100 个字节后：缓冲区地址"
       << (before == owner.data() ? u8"没变" : u8"变了")
       << u8"，先前的 view 还指着旧缓冲区，再解引用就是悬垂访问\n";
    return os.str();
}

CheckResult run_self_tests()
{
    Checker c;

    /* 1. 基本切分 */
    const std::vector<std::string> parts = split(u8"a,b,c", u8",");
    c.check(parts.size() == 3 && parts[0] == "a" && parts[1] == "b" && parts[2] == "c",
            u8"split 把 a,b,c 切成 3 段", join(parts, "|"));

    /* 2. 相邻分隔符之间的空段要保留，否则会丢字段 */
    const std::vector<std::string> empties = split(u8"a,,b,", u8",");
    c.check(empties.size() == 4 && empties[1].empty() && empties[3].empty(),
            u8"连续分隔符与末尾分隔符都产生空段", join(empties, "|"));

    /* 3. 多字节分隔符：中文逗号 */
    const std::vector<std::string> cjk = split(u8"甲，乙，丙", u8"，");
    c.check(cjk.size() == 3 && cjk[0] == u8"甲" && cjk[2] == u8"丙",
            u8"分隔符本身是多字节的 UTF-8 文本", join(cjk, "|"));

    /* 4. 零拷贝：view 与拆分结果一致，且都落在原文的缓冲区内 */
    const std::string source = u8"alpha,beta,gamma";
    const std::vector<std::string_view> views = split_view(source, u8",");
    bool inside_all = true;
    for (const std::string_view &view : views) {
        inside_all = inside_all && inside(view, source);
    }
    c.check(views.size() == 3 && views[1] == "beta" && inside_all
                && views[0].data() == source.data(),
            u8"split_view 的每一段都指向原串自己的缓冲区，没有复制",
            u8"段数 " + std::to_string(views.size()));

    /* 5. 修剪：两端去干净，中间的空格保留 */
    const std::string padded = std::string("  x y\t\r\n");
    c.check(trim(padded) == "x y" && trim_left(padded) == "x y\t\r\n"
                && trim_right(padded) == "  x y",
            u8"trim 去掉两端空白，中间的空格不动", trim(padded));

    /* 6. 大小写只作用于 ASCII 字母 */
    const std::string mixed = u8"a中B";
    c.check(to_upper(mixed) == u8"A中B" && to_lower(mixed) == u8"a中b"
                && to_upper(mixed).size() == mixed.size(),
            u8"大小写转换不碰多字节序列", to_upper(mixed));

    /* 7. 全部替换：换上去的内容不会被再次匹配 */
    std::string repeat = "aaaa";
    const std::size_t repeat_count = replace_all(repeat, "aa", "b");
    c.check(repeat == "bb" && repeat_count == 2,
            u8"replace_all 替换全部并返回次数", repeat);

    /* 8. 空模式必须被拒绝，否则循环不会结束 */
    std::string untouched = "abc";
    c.check(replace_all(untouched, "", "x") == 0 && untouched == "abc",
            u8"from 为空时返回 0，文本不变", untouched);

    /* 9. 查找全部位置 */
    const std::vector<std::size_t> hits = find_all("one two one", "one");
    c.check(hits.size() == 2 && hits[0] == 0 && hits[1] == 8,
            u8"find_all 给出全部出现位置", std::to_string(hits.size()) + u8" 处");

    /* 10. 拼接 */
    const std::vector<std::string> words{ "a", "b", "c" };
    c.check(join(words, "-") == "a-b-c" && join(words, "").size() == 3,
            u8"join 用分隔符拼回", join(words, "-"));

    /* 11. 字节数与字符数不是一回事 */
    const std::string chinese = u8"中文abc";
    c.check(chinese.size() == 9 && utf8_char_count(chinese) == 5,
            u8"size() 是字节数，utf8_char_count 才是字符数",
            std::to_string(chinese.size()) + u8" 字节 / "
                + std::to_string(utf8_char_count(chinese)) + u8" 字符");

    /* 12. 首字节判定位数：四档各验一次 */
    c.check(utf8_sequence_length(static_cast<unsigned char>(u8"A"[0])) == 1
                && utf8_sequence_length(static_cast<unsigned char>(u8"\u00E9"[0])) == 2
                && utf8_sequence_length(static_cast<unsigned char>(u8"\u4E2D"[0])) == 3
                && utf8_sequence_length(static_cast<unsigned char>(u8"\U0001F642"[0])) == 4,
            u8"首字节 0xxxxxxx 到 11110xxx 分别判为 1 到 4 字节");

    /* 13. 续字节与非法首字节 */
    c.check(utf8_sequence_length(0x80) == 0 && utf8_sequence_length(0xBF) == 0
                && utf8_sequence_length(0xF8) == 0,
            u8"续字节与 0xF8 以上不当作首字节");

    /* 14. 按字符截断，不切断多字节序列 */
    const std::string cut2 = utf8_truncate(u8"中文测试", 2);
    c.check(cut2 == u8"中文" && cut2.size() == 6 && utf8_is_valid(cut2),
            u8"utf8_truncate 按字符数截断，汉字完整", cut2);

    /* 15. 按字节截断要退到字符边界 */
    const std::string mixed_bytes = u8"a中b";
    const std::string cut3 = utf8_truncate_bytes(mixed_bytes, 3);
    const std::string cut4 = utf8_truncate_bytes(mixed_bytes, 4);
    c.check(mixed_bytes.size() == 5 && cut3 == "a" && cut3.size() == 1
                && cut4 == u8"a中" && cut4.size() == 4 && utf8_is_valid(cut4),
            u8"utf8_truncate_bytes 退到字符边界上",
            std::to_string(cut3.size()) + u8" / " + std::to_string(cut4.size()) + u8" 字节");

    /* 16. string_view 的生命期：主人活着时有效，重新分配之后就悬垂 */
    std::string owner = u8"第一段，第二段";
    const std::string_view borrowed(owner);
    const bool valid_while_alive = borrowed.size() == owner.size()
                                   && borrowed.data() == owner.data()
                                   && borrowed.substr(0, 3) == u8"第";
    const char *old_buffer = borrowed.data();
    owner.append(200, '#');
    const bool buffer_moved = old_buffer != owner.data();
    c.check(valid_while_alive && buffer_moved,
            u8"view 在主人活着且未重新分配时有效，重新分配后指向旧缓冲区",
            buffer_moved ? u8"缓冲区地址已改变" : u8"缓冲区地址没变");

    return c.take();
}

}   /* namespace text */
