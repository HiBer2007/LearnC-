/**
 * dirscan.cpp —— 练习模板 07 的核心逻辑（<filesystem> 目录工具）
 *
 * 4 个阶段的实现都写在这个文件里。骨架给的是占位实现：
 * 能编译、能运行、结果明显不对（空字符串、全 0）。
 * 各阶段的任务、验收标准与自查方法见同目录《配置步骤.md》。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 数据目录按相对路径 data/tree 使用，因此**在模板目录下运行**。
 */
#include "dirscan.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace ds {

/* ==================================================================
 * 阶段 1：path 的拆分与拼装
 * ================================================================== */

/* TODO（阶段 1-1）：用 std::filesystem::path 拆开一个路径。
 *
 * 提示：
 *   1. fs::path p(input)；
 *   2. filename() 是文件名（含扩展名），stem() 去掉扩展名，extension() 是扩展名；
 *   3. parent_path() 与整体路径都要用 generic_string() 输出：
 *      Windows 下默认的 string() 会把斜杠写成反斜杠，generic_string() 统一成 '/'，
 *      这样同一份验收输出在 Windows 与 Linux 上一致；
 *   4. 输入为空时返回全空的 PathParts（提前返回）。
 *
 * 验收：data/tree/sub/report.txt 拆出 report.txt / report / .txt / data/tree/sub。 */
PathParts split_path(const std::string &p)
{
    (void)p;
    return PathParts{};   /* 占位实现：全空 */
}

/* TODO（阶段 1-2）：换扩展名。新扩展名要带上点。
 *
 * 提示：fs::path p(input); p.replace_extension(new_ext); return p.generic_string();
 *       replace_extension 会把原来的扩展名整段换掉（含点），不会拼接。 */
std::string swap_extension(const std::string &p, const std::string &new_ext)
{
    (void)p;
    (void)new_ext;
    return "(TODO stage 1-2)";
}

/* ==================================================================
 * 阶段 2：遍历与统计
 * ================================================================== */

/* TODO（阶段 2-1）：统计一棵目录树。
 *
 * 提示：
 *   1. 目录不存在时直接返回全 0，不要抛异常：
 *      std::error_code ec; if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return {};
 *   2. 遍历用 fs::directory_iterator(root) 或 fs::recursive_directory_iterator(root)；
 *      为了让目录不存在时不抛，构造时可以传 error_code：
 *      for (const auto &entry : fs::directory_iterator(root, ec)) { ... }
 *   3. is_regular_file() 才算文件（目录、符号链接分开数）；
 *   4. 文件大小用 fs::file_size(entry.path(), ec)；
 *   5. 扩展名用 path::extension()，转小写后统计；没有扩展名的记为 "(none)"；
 *   6. 最后把 by_extension 按名字排序（std::sort + lambda）。
 *
 * 验收：对 data/tree 递归统计得 3 个文件、1 个目录、总字节数与《配置步骤.md》一致。 */
DirStats scan(const std::string &root, bool recursive)
{
    (void)root;
    (void)recursive;
    return DirStats{};
}

/* ==================================================================
 * 阶段 3：两条错误处理路线
 * ================================================================== */

/* TODO（阶段 3-1）：不抛异常的版本。
 *
 * 提示：
 *   1. 与 scan 的区别是「错误往哪去」：这里把 error_code 交给调用方；
 *   2. fs::directory_iterator(root, ec) 这种带 error_code 的构造不会抛异常，
 *      但 ec 被置位后循环体一次都不会执行，因此函数返回全 0；
 *   3. 调用方必须先判断 ec（见 main_cli.cpp）；ec 为 0 表示成功。
 *
 * 验收：对一个不存在的目录，ec 非 0、返回全 0、程序不崩。 */
DirStats scan_ec(const std::string &root, bool recursive, std::error_code &ec)
{
    (void)root;
    (void)recursive;
    ec.clear();
    return DirStats{};
}

/* TODO（阶段 3-2）：抛异常的版本。
 *
 * 提示：
 *   1. 用不带 error_code 的构造与调用：fs::directory_iterator(root)；
 *      目录不存在时它抛 std::filesystem::filesystem_error；
 *   2. 函数本身不写 try/catch——「抛」就是这条路线的方式；
 *   3. 调用方 catch (const fs::filesystem_error &e) 之后可以取 e.code() 与 e.what()。
 *
 * 验收：对一个不存在的目录，调用方接住异常并打印 code 与 what。 */
DirStats scan_throw(const std::string &root, bool recursive)
{
    (void)root;
    (void)recursive;
    return DirStats{};
}

/* ==================================================================
 * 阶段 4：增删改复制
 * ================================================================== */

/* TODO（阶段 4-1）：在 root 下走一遍创建、复制、改名、统计、删除。
 *
 * 提示：
 *   1. fs::create_directories(root / "sub") 会一次建出多层，返回是否新建；
 *   2. 写文件可以用 std::ofstream(root / "a.txt", std::ios::binary)，内容固定；
 *   3. fs::copy_file(a, b, fs::copy_options::overwrite_existing)；
 *   4. fs::rename(b, c)；
 *   5. 统计时只数 is_regular_file，字节数用 fs::file_size；
 *   6. fs::remove_all(root) 返回删掉的条目总数（文件与目录都算）；
 *   7. 每一步失败都要把对应字段留成 0，不要把异常放出去。
 *      （练习：想一想哪些调用会抛、哪些要传 error_code 才不会抛。）
 *
 * 验收：files_now = 2、bytes 与《配置步骤.md》一致、removed = 3、运行后目录不残留。 */
TreeResult build_tree(const std::string &root)
{
    (void)root;
    return TreeResult{};   /* 占位实现：全 0 */
}

} /* namespace ds */
