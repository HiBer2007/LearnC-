/**
 * dirscan.hpp —— 练习模板 07 的核心接口（C++）
 *
 * 接口已经定好，src/main_cli.cpp 与两份界面都调用它们。
 * 你要做的是在 src/dirscan.cpp 里把标了 TODO 的函数实现出来。
 *
 * 四个阶段的对应关系：
 *     阶段 1   split_path、swap_extension      path 的拆分与拼装
 *     阶段 2   scan                            directory_iterator 遍历
 *     阶段 3   scan_ec、scan_throw             两条错误处理路线
 *     阶段 4   build_tree                      增删改复制
 */
#ifndef DIRSCAN_HPP
#define DIRSCAN_HPP

#include <cstdint>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ds {

/* ==================================================================
 * 阶段 1：path 的拆分与拼装
 * ================================================================== */

struct PathParts {
    std::string input;       /* 原样保留的输入 */
    std::string filename;    /* 文件名（含扩展名），如 report.txt */
    std::string stem;        /* 去掉扩展名的部分，如 report */
    std::string extension;   /* 扩展名（含点），如 .txt */
    std::string parent;      /* 父目录，用 generic_string 输出，斜杠是 / */
    std::string generic;     /* 整个路径的 generic_string 形式 */
};

/* 用 std::filesystem::path 拆开一个路径。输入为空时各项都为空串。 */
PathParts split_path(const std::string &p);

/* 换扩展名：新扩展名要带上点（例如 ".md"）。 */
std::string swap_extension(const std::string &p, const std::string &new_ext);

/* ==================================================================
 * 阶段 2：遍历与统计
 * ================================================================== */

struct DirStats {
    long files = 0;
    long dirs  = 0;
    long long bytes = 0;
    /* 扩展名（小写，含点）到文件个数，按扩展名升序排列；没有扩展名的记为 "(none)" */
    std::vector<std::pair<std::string, long>> by_extension;
};

/* 统计一棵目录树。recursive 为真时用 recursive_directory_iterator，
 * 否则只用 directory_iterator。目录不存在时返回全 0（不抛异常）。 */
DirStats scan(const std::string &root, bool recursive);

/* ==================================================================
 * 阶段 3：两条错误处理路线
 * ================================================================== */

/* 不抛异常的版本：把错误码交给调用方，函数本身只返回统计结果。
 * 这是唯一一个「出错也返回正常类型」的函数，因此调用方必须先看 ec。 */
DirStats scan_ec(const std::string &root, bool recursive, std::error_code &ec);

/* 抛异常的版本：目录不存在时让 std::filesystem::filesystem_error 逃出去，
 * 由调用方 try/catch 接住。 */
DirStats scan_throw(const std::string &root, bool recursive);

/* ==================================================================
 * 阶段 4：增删改复制
 * ================================================================== */

struct TreeResult {
    long created_dirs = 0;   /* create_directories 建了几层 */
    long files_now    = 0;   /* 建好之后树里有几个普通文件 */
    long long bytes   = 0;   /* 树里文件的总字节数 */
    long copied       = 0;   /* copy_file 是否成功（1 表示成功） */
    long renamed      = 0;   /* rename 是否成功（1 表示成功） */
    std::uintmax_t removed = 0;  /* remove_all 的返回值：删掉了几个条目 */
};

/* 在 root 下做一遍完整的增删改复制：
 *   1. create_directories(root / "sub")
 *   2. 写一个 root/a.txt，内容固定为 "filesystem demo\n"
 *   3. copy_file 到 root/sub/copy.txt
 *   4. rename 成 root/sub/renamed.txt
 *   5. 统计 files_now 与 bytes（只数普通文件）
 *   6. remove_all(root)，把返回值记进 removed
 * 每一步失败都要写进对应的字段（用 0 表示没成功），不要把异常抛给调用方。 */
TreeResult build_tree(const std::string &root);

} /* namespace ds */

#endif /* DIRSCAN_HPP */
