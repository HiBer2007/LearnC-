/**
 * dir_scan.hpp —— 目录扫描的核心逻辑，不依赖任何界面
 *
 * 命令行版（src/main_cli.cpp）、Win32 版（src/main_gui_win32.cpp）与
 * Qt 版（src/main_gui_qt.cpp）都链接这个模块。这里的函数不打印任何东西，
 * 只负责遍历、统计、格式化并返回字符串：显示成什么样子由各自的界面决定。
 *
 * 两条错误处理路线都在这里，而且是分开写的：
 *   scan_with_error_code()  用带 std::error_code 的重载，不抛异常
 *   scan_or_throw()         用会抛异常的重载，失败时抛 filesystem_error
 * 两条路线对同一棵树给出同样的统计结果，区别只在失败怎么交出来。
 */
#ifndef DIR_SCAN_HPP
#define DIR_SCAN_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dirscan {

/** 一种扩展名的汇总。没有扩展名的文件归到键 "(none)" 下 */
struct ExtensionStat {
    std::string extension;                  /**< ".txt" 这样带点的形式，已转小写 */
    std::size_t file_count = 0;             /**< 这一类的文件数 */
    std::uintmax_t total_bytes = 0;         /**< 这一类加起来的字节数 */
    std::string largest_relative_path;      /**< 这一类里最大的那个文件，相对扫描根 */
    std::uintmax_t largest_bytes = 0;       /**< 上面那个文件的字节数 */
    std::size_t max_depth = 0;              /**< 这一类里最深的文件在第几层 */
};

/** 最近修改的文件里的一条 */
struct RecentFile {
    std::string relative_path;              /**< 相对扫描根 */
    std::uintmax_t size = 0;                /**< 字节数 */
    std::string modified;                   /**< 已换算成可读时间，形如 2026-10-01 15:40:12 */
};

/** 一次扫描的全部结果。纯数据，不含路径以外的任何状态 */
struct ScanReport {
    std::string root;                       /**< 扫描根，按调用者给的形式原样记下 */
    bool recursive = true;                  /**< 这一次走的是递归还是只一层 */
    std::size_t file_count = 0;             /**< 普通文件数 */
    std::size_t directory_count = 0;        /**< 子目录数，不含根自己 */
    std::uintmax_t total_bytes = 0;         /**< 所有普通文件加起来的字节数 */
    std::size_t max_depth = 0;              /**< 最深的文件在第几层，根下是 0 */
    std::string deepest_relative_path;      /**< 最深处的一个文件，相对扫描根 */
    std::vector<ExtensionStat> by_extension;/**< 按总字节从多到少排好 */
    std::vector<RecentFile> recent;         /**< 按修改时间从新到旧，条数不超过 recent_limit */
};

/** 扫描的可选项 */
struct ScanOptions {
    bool recursive = true;                  /**< true 走 recursive_directory_iterator */
    std::size_t recent_limit = 5;           /**< 保留几个最近修改的文件 */
    bool skip_permission_denied = true;     /**< 没权限的子目录跳过，不中断整次扫描 */
};

/**
 * 把 last_write_time 的值换算成可读时间。
 *
 * file_time_type 的起点不是 1970 年，因此不能直接 to_time_t。
 * C++17 没有 clock_cast，只能取两个时钟当前值的差，把文件时间平移到
 * system_clock 上再格式化。
 */
std::string to_readable_time(std::filesystem::file_time_type when);

/** 拼路径：用 operator/ ，它会自己补分隔符。纯文本运算，不查磁盘 */
std::filesystem::path join_paths(const std::filesystem::path &base,
                                 const std::filesystem::path &name);

/** 求相对路径：走 lexically_relative，同样不查磁盘、不解析符号链接 */
std::filesystem::path relative_of(const std::filesystem::path &base,
                                  const std::filesystem::path &child);

/**
 * 第一条路线：带 std::error_code 的重载。
 * 出错时不抛异常，错误码写进 ec，返回一份空报告（根目录仍然记着）。
 */
ScanReport scan_with_error_code(const std::filesystem::path &root,
                                const ScanOptions &options,
                                std::error_code &ec);

/**
 * 第二条路线：会抛异常的重载。
 * 失败时抛 std::filesystem::filesystem_error，由调用方 catch。
 */
ScanReport scan_or_throw(const std::filesystem::path &root,
                         const ScanOptions &options);

/** 把一份报告摆成多行文本。界面与命令行共用同一份排版 */
std::string format_report(const ScanReport &report);

/**
 * 演示模式的全部输出：递归扫描、非递归对照、path 拼装与 lexically_relative、
 * 以及同一个不存在的路径在两条路线下的表现。命令行版与两份界面共用它。
 */
std::string build_demo_output(const std::filesystem::path &root);

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;         /**< 每项一行，已含 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;            /**< "18 项中 18 项通过，全部通过" */
};

/**
 * 逐项核对遍历、统计、时间换算与两条错误路线。
 * 自测自己建一棵临时目录树（std::filesystem::temp_directory_path() 下），
 * 跑完删掉，不依赖仓库里的任何文件。
 */
CheckResult run_self_tests();

}   /* namespace dirscan */

#endif /* DIR_SCAN_HPP */
