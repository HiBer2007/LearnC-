/**
 * main_cli.cpp —— 练习模板 07 的命令行验收程序（C++）
 *
 * 这个文件**不需要改**：它按 4 个阶段调用 dirscan.cpp 里的函数，
 * 把结果打印成《配置步骤.md》里的验收输出。你的实现写在 src/dirscan.cpp 里。
 *
 * 构建与运行（**在模板目录下**执行，数据目录是相对路径 data/tree）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 阶段的划分：
 *     阶段 1  path 的拆分与拼装
 *     阶段 2  遍历目录树与属性统计
 *     阶段 3  error_code 与异常两条错误路线
 *     阶段 4  创建、复制、改名、删除
 */
#include <filesystem>
#include <iostream>
#include <string>

#include "dirscan.hpp"

namespace {

void print_stats(const char *label, const ds::DirStats &s)
{
    std::cout << label << "\n";
    std::cout << "  files          : " << s.files << "\n";
    std::cout << "  dirs           : " << s.dirs << "\n";
    std::cout << "  bytes          : " << s.bytes << "\n";
    std::cout << "  by extension   :\n";
    for (const auto &kv : s.by_extension) {
        std::cout << "    " << kv.first << " : " << kv.second << "\n";
    }
}

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: path parts ===\n";
    {
        const std::string input = "data/tree/sub/report.txt";
        const ds::PathParts parts = ds::split_path(input);

        std::cout << "input            : " << input << "\n";
        std::cout << "filename         : " << parts.filename << "\n";
        std::cout << "stem             : " << parts.stem << "\n";
        std::cout << "extension        : " << parts.extension << "\n";
        std::cout << "parent           : " << parts.parent << "\n";
        std::cout << "generic          : " << parts.generic << "\n";
        std::cout << "replace ext      : " << ds::swap_extension(input, ".md") << "\n";
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: iterate a directory tree ===\n";
    {
        const std::string root = "data/tree";
        std::cout << "root             : " << root << "\n";
        print_stats("non-recursive:", ds::scan(root, false));
        print_stats("recursive:", ds::scan(root, true));
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: two error paths ===\n";
    {
        const std::string missing = "data/no_such_dir";
        std::cout << "missing path     : " << missing << "\n";

        std::error_code ec;
        const ds::DirStats a = ds::scan_ec(missing, true, ec);
        std::cout << "error_code scan  : files = " << a.files
                  << ", ec = " << ec.value()
                  << " (category " << ec.category().name() << ")\n";
        std::cout << "ec nonzero       : " << (ec ? "yes" : "no") << "\n";

        bool threw = false;
        std::string what;
        int  code = 0;
        try {
            (void)ds::scan_throw(missing, true);
        } catch (const std::filesystem::filesystem_error &e) {
            threw = true;
            code = e.code().value();
            what = e.what();
        }
        std::cout << "exception catch  : threw = " << (threw ? "yes" : "no")
                  << ", code = " << code << "\n";
        std::cout << "what() length    : " << what.size()
                  << " bytes (localized text, not printed)\n";
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: create, copy, rename, remove ===\n";
    {
        const std::string root = "build/fs_demo";
        const ds::TreeResult r = ds::build_tree(root);

        std::cout << "demo root        : " << root << "\n";
        std::cout << "created dirs     : " << r.created_dirs << "\n";
        std::cout << "copied           : " << r.copied << "\n";
        std::cout << "renamed          : " << r.renamed << "\n";
        std::cout << "files now        : " << r.files_now << "\n";
        std::cout << "bytes            : " << r.bytes << "\n";
        std::cout << "remove_all count : " << r.removed << "\n";

        std::error_code ec;
        std::cout << "left on disk     : "
                  << (std::filesystem::exists(root, ec) ? "yes" : "no") << "\n";
    }

    return 0;
}
