/* main_cli.cpp —— 练习模板 11 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 scan 里的函数，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 src/scanner.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 */
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "scanner.hpp"

namespace {

const int kWidth = 32;

/* 写死的输入：一段带注释的 JSONC，一共 9 行，行与行之间用一个换行符连起来。
 * 里面有几处专门盯住状态机的边界：
 *     "https://example.com"     字符串里的两个斜杠不是注释的开头
 *     "a\"b"                    反斜杠把后面那个引号吃掉，它不结束字符串
 *     "C:\\tmp\\log"            两个连着的反斜杠，前一个不能把后一个吃掉
 *     第 8 行的 note 值          写在字符串里的、长得像注释的一段文字
 * 另有一段跨两行的块注释，用来看换行符有没有被一起替换掉。 */
const char *const kInputLines[] = {
    "{",
    "  // service endpoint and timeout",
    "  \"url\": \"https://example.com\",",
    "  \"escaped\": \"a\\\"b\",",
    "  /* a block comment can span lines,",
    "     a \"quote\" and a // inside it do not count */",
    "  \"path\": \"C:\\\\tmp\\\\log\",",
    "  \"note\": \"/* not a comment */\"",
    "}",
};

/* 参考文本：输入剥掉注释之后应当长成的样子，与输入一行对一行。
 * 注释里的每个字符换成一个空格，换行符留在原处，因此两者长度相同。
 * 它只是验收用的基准：实现要与它逐字节相同，而不是把它抄回去。 */
const char *const kExpectedLines[] = {
    "{",
    "                                 ",
    "  \"url\": \"https://example.com\",",
    "  \"escaped\": \"a\\\"b\",",
    "                                    ",
    "                                                 ",
    "  \"path\": \"C:\\\\tmp\\\\log\",",
    "  \"note\": \"/* not a comment */\"",
    "}",
};

const std::size_t kInputLineCount = sizeof(kInputLines) / sizeof(kInputLines[0]);
const std::size_t kExpectedLineCount = sizeof(kExpectedLines) / sizeof(kExpectedLines[0]);

/* 七个喂给玩具状态机的输入。最后一个以小数点开头，
 * 第一个带减号与小数部分，中间几个分别落在「只有减号」「只有整数与小数点」
 * 「小数点出现两次」这些边界上。 */
const char *const kNumInputs[] = {
    "-12.5",
    "12",
    "-",
    "1.",
    "1.2.3",
    "12abc",
    ".5",
};

const std::size_t kNumInputCount = sizeof(kNumInputs) / sizeof(kNumInputs[0]);

/* 一段以单个反斜杠结尾的输入：一个引号、一个字母 a、一个反斜杠，到此为止。
 * 转义处理必须先问一句「后面还有没有字符」，否则就会伸手去读结尾之后那一格。 */
const std::string kTailInput = "\"a\\";

/* ---------------------------------------------------------------- 小工具 */

void line(const std::string &label, long long value)
{
    std::cout << std::left << std::setw(kWidth) << label << ": " << value << "\n";
}

void line(const std::string &label, const std::string &text)
{
    std::cout << std::left << std::setw(kWidth) << label << ": " << text << "\n";
}

std::string yes_no(bool v)
{
    return v ? "yes" : "no";
}

std::string pos_text(const scan::Pos &p)
{
    return "(" + std::to_string(p.line) + ", " + std::to_string(p.col) + ")";
}

std::string join_lines(const char *const *lines, std::size_t count)
{
    std::string s;
    for (std::size_t i = 0; i < count; ++i) {
        if (i != 0) {
            s += '\n';
        }
        s += lines[i];
    }
    return s;
}

std::vector<std::string> to_lines(const char *const *lines, std::size_t count)
{
    std::vector<std::string> v;
    v.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        v.push_back(lines[i]);
    }
    return v;
}

std::vector<std::string> split_lines(const std::string &s)
{
    std::vector<std::string> v;
    std::string cur;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') {
            v.push_back(cur);
            cur.clear();
        } else {
            cur += s[i];
        }
    }
    v.push_back(cur);
    return v;
}

/* 逐行打印，两头用竖线夹住：行尾的空格因此看得见（它被夹在中间），
 * 打印出来的每一行也不会带着行尾空白。末尾带星号的，是与参考文本不一样的行。 */
void print_framed(const std::string &title,
                  const std::vector<std::string> &lines,
                  const std::vector<std::string> &expect)
{
    std::cout << title << "\n";
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::cout << std::right << std::setw(3) << (i + 1) << "|"
                  << std::left << lines[i] << "|";
        if (i >= expect.size() || lines[i] != expect[i]) {
            std::cout << " *";
        }
        std::cout << "\n";
    }
}

std::string states_text(const scan::RunResult &r)
{
    std::string s;
    for (std::size_t i = 0; i < r.steps.size(); ++i) {
        if (i != 0) {
            s += ' ';
        }
        s += scan::state_name(r.steps[i].state);
    }
    return s.empty() ? std::string("(none)") : s;
}

void print_run(const std::string &input, const scan::RunResult &r)
{
    std::cout << "--- input: " << input << "\n";
    line("accepted", yes_no(r.accepted));
    line("consumed", static_cast<long long>(r.consumed));
    line("accepted prefix length", static_cast<long long>(r.accepted_prefix));
    line("states", states_text(r));
}

void print_steps(const scan::RunResult &r, std::size_t max_steps)
{
    const std::size_t n = r.steps.size() < max_steps ? r.steps.size() : max_steps;
    std::cout << "steps:\n";
    for (std::size_t i = 0; i < n; ++i) {
        std::cout << "  [" << r.steps[i].index << "] '" << r.steps[i].ch
                  << "' -> " << scan::state_name(r.steps[i].state) << "\n";
    }
    if (r.steps.size() > n) {
        std::cout << "  ... (" << (r.steps.size() - n) << " more)\n";
    }
}

} /* namespace */

int main()
{
    const std::string text = join_lines(kInputLines, kInputLineCount);
    const std::vector<std::string> expected =
        to_lines(kExpectedLines, kExpectedLineCount);

    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: one pass, line and column ===\n";
    {
        const std::size_t off_newline = text.find('\n');
        const std::size_t off_url = text.find("https");
        const std::size_t off_escape = text.find("a\\\"b");
        const std::size_t off_block = text.find("/*");
        const std::size_t off_last = text.size() - 1;
        const std::size_t off_end = text.size();

        std::vector<std::size_t> offsets;
        offsets.push_back(0);
        offsets.push_back(off_newline);
        offsets.push_back(off_url);
        offsets.push_back(off_escape);
        offsets.push_back(off_block);
        offsets.push_back(off_last);
        offsets.push_back(off_end);

        line("input lines", static_cast<long long>(kInputLineCount));
        line("input chars", static_cast<long long>(text.size()));
        line("offset of first newline", static_cast<long long>(off_newline));
        line("offset of \"https\"", static_cast<long long>(off_url));
        line("offset of the escaped quote", static_cast<long long>(off_escape));
        line("offset of the block comment", static_cast<long long>(off_block));
        line("offset of the last character", static_cast<long long>(off_last));
        line("offset one past the last", static_cast<long long>(off_end));

        std::vector<scan::Pos> marks;
        scan::ScanStats st;
        scan::scan_lines(text, offsets, marks, st);

        line("scan chars", st.chars);
        line("scan lines", st.lines);
        line("scan newlines", st.newlines);
        line("scan tabs", st.tabs);
        line("scan max col", st.max_col);

        for (std::size_t i = 0; i < offsets.size(); ++i) {
            line("mark " + std::to_string(offsets[i]), pos_text(marks[i]));
        }
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: the same machine, written with switch ===\n";
    {
        for (std::size_t i = 0; i < kNumInputCount; ++i) {
            const std::string in = kNumInputs[i];
            const scan::RunResult r = scan::run_switch(in);
            print_run(in, r);
            if (i == 0) {
                print_steps(r, 12);
            }
        }
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: the same machine, driven by a table ===\n";
    {
        const scan::TableShape sh = scan::table_shape();
        line("table states", sh.states);
        line("table classes", sh.classes);
        line("table cells", sh.cells);
        line("table dead cells", scan::table_dead_cells());

        bool all_same = true;
        for (std::size_t i = 0; i < kNumInputCount; ++i) {
            const std::string in = kNumInputs[i];
            const scan::RunResult a = scan::run_switch(in);
            const scan::RunResult b = scan::run_table(in);
            const bool same = scan::same_states(a, b);
            all_same = all_same && same;

            print_run(in, b);
            line("same as the switch version", yes_no(same));
        }
        line("all inputs identical", yes_no(all_same));
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: stripping JSON comments ===\n";
    {
        const std::string out = scan::strip_json_comments(text);
        const std::string naive = scan::strip_naive(text);

        line("input lines", static_cast<long long>(kInputLineCount));
        line("input chars", static_cast<long long>(text.size()));
        line("output chars", static_cast<long long>(out.size()));
        line("lengths equal", yes_no(out.size() == text.size()));
        line("chars that differ from input",
             static_cast<long long>(scan::count_differ(text, out)));
        line("occurrences of \"//\" in output",
             static_cast<long long>(scan::count_substring(out, "//")));
        line("same as expected text", yes_no(out == join_lines(kExpectedLines,
                                                              kExpectedLineCount)));

        print_framed("output (' *' marks a line that differs from the expected one):",
                     split_lines(out), expected);

        line("naive chars", static_cast<long long>(naive.size()));
        line("occurrences of \"//\" in naive",
             static_cast<long long>(scan::count_substring(naive, "//")));
        line("naive same as expected text",
             yes_no(naive == join_lines(kExpectedLines, kExpectedLineCount)));

        print_framed("naive output (' *' marks a line that differs from the expected one):",
                     split_lines(naive), expected);

        const std::string tail_out = scan::strip_json_comments(kTailInput);
        std::cout << "--- tail input: one backslash at the very end ---\n";
        line("tail input chars", static_cast<long long>(kTailInput.size()));
        line("tail input escaped", scan::escape_for_print(kTailInput));
        line("tail output chars", static_cast<long long>(tail_out.size()));
        line("tail output escaped", scan::escape_for_print(tail_out));
        line("tail lengths equal", yes_no(tail_out.size() == kTailInput.size()));
        line("tail output same as input", yes_no(tail_out == kTailInput));
    }

    return 0;
}
