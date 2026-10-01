/**
 * text_tools.hpp —— 文本处理逻辑，不依赖任何界面
 *
 * 约定：本文件与 text_tools.cpp 里的 std::string 一律承载 UTF-8 字节，
 * 与源码里字面量的执行字符集无关（核心库内部用 u8"" 字面量）。
 * 显示成哪种编码由界面层决定，命令行版见 src/main_cli.cpp 的 to_console_encoding。
 */
#ifndef TEXT_TOOLS_HPP
#define TEXT_TOOLS_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace text {

/* ---------------- 切分与拼接 ---------------- */

/** 按分隔符切分，分隔符可以是一段多字节文本（例如中文逗号）。
    相邻分隔符之间产生一个空段，末尾的分隔符之后也产生一个空段。
    分隔符为空时整段作为唯一的一段返回。 */
std::vector<std::string> split(std::string_view text, std::string_view delimiter);

/** 零拷贝切分：返回的每一段都指向 text 自己的缓冲区，一个字节都不复制。
    text 指向的内存必须在这些 view 的使用期间一直有效，且不能被重新分配。 */
std::vector<std::string_view> split_view(std::string_view text, std::string_view delimiter);

/** 按行切分：'\n' 分行，行尾的 '\r' 去掉；文本末尾的换行不产生空行。 */
std::vector<std::string_view> split_lines(std::string_view text);

/** 用分隔符把若干段拼起来 */
std::string join(const std::vector<std::string> &parts, std::string_view separator);

/* ---------------- 修剪与大小写 ---------------- */

/** 去掉两端的空白（空格、\t、\n、\r、\f、\v） */
std::string trim(std::string_view text);
std::string trim_left(std::string_view text);
std::string trim_right(std::string_view text);

/** ASCII 字母的大小写转换；UTF-8 多字节序列的字节原样保留 */
std::string to_upper(std::string_view text);
std::string to_lower(std::string_view text);

/* ---------------- 查找与替换 ---------------- */

/** 找出 needle 全部出现的位置（字节下标，从 0 数）；needle 为空时返回空表 */
std::vector<std::size_t> find_all(std::string_view text, std::string_view needle);

/** 把 from 全部换成 to，返回替换次数；from 为空时返回 0，文本不变 */
std::size_t replace_all(std::string &text, std::string_view from, std::string_view to);

/* ---------------- UTF-8 字节处理 ---------------- */

/** 由首字节判定位数：1、2、3、4；续字节（0x80 到 0xBF）与 0xF8 以上返回 0 */
std::size_t utf8_sequence_length(unsigned char lead_byte);

/** 字符（码点）个数：数非续字节的字节数。文本合法时与真实字符数一致 */
std::size_t utf8_char_count(std::string_view text);

/** 结构是否合法：首字节给出的长度与后续字节的 10xxxxxx 形式都对得上。
    这里不检查超长编码与代理区，只做逐字节的结构检查 */
bool utf8_is_valid(std::string_view text);

/** 按字符数截断：保留前 max_chars 个字符，不切断多字节序列 */
std::string utf8_truncate(std::string_view text, std::size_t max_chars);

/** 按字节数截断：结果不超过 max_bytes 字节，并且退到字符边界上 */
std::string utf8_truncate_bytes(std::string_view text, std::size_t max_bytes);

/* ---------------- 文件与报告 ---------------- */

/** 按二进制读入整个文件，不做换行转换；失败时 ok 置 false 并写明原因 */
std::string read_text_file(const std::string &path, bool &ok, std::string &error);

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;     /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;        /**< "16 项中 16 项通过，全部通过" */
};

/** 项目输出：对给定文本做一遍演示，返回多行 UTF-8 文本，不含结尾换行 */
std::string build_report(std::string_view sample, const std::string &source_label);

/** 逐项核对切分、修剪、替换与 UTF-8 边界处理 */
CheckResult run_self_tests();

}   /* namespace text */

#endif /* TEXT_TOOLS_HPP */
