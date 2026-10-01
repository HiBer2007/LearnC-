/**
 * dir_scan.cpp —— <filesystem> 目录扫描的实现
 *
 * 这里没有任何界面代码：不包含 <windows.h>，也不打印。
 * 核心逻辑分三块：
 *   1. 遍历——同一个累加器，两条取属性的路线（error_code 版与抛异常版）
 *   2. 统计——按扩展名分类、找最大文件、记最深层级、收集最近修改的文件
 *   3. 排版——把报告摆成多行文本，界面只负责把它放进控件
 */
#include "dir_scan.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dirscan {

namespace fs = std::filesystem;

namespace {

/* ── 时间换算 ─────────────────────────────────────────── */

/* file_time_type 的起点不是 1970 年，两个时钟不能直接换算。
   取各自当前值的差，把文件时间平移到 system_clock 上。 */
std::chrono::system_clock::time_point to_system_time(fs::file_time_type when)
{
    const fs::file_time_type file_now = fs::file_time_type::clock::now();
    const std::chrono::system_clock::time_point system_now = std::chrono::system_clock::now();
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        when - file_now + system_now);
}

/* ── 扩展名归类 ───────────────────────────────────────── */

/* 没有扩展名的记 "(none)"；.TXT 与 .txt 归到同一类 */
std::string extension_key(const fs::path &p)
{
    std::string ext = p.extension().string();
    if (ext.empty()) {
        return std::string("(none)");
    }
    for (std::size_t i = 0; i < ext.size(); ++i) {
        ext[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));
    }
    return ext;
}

/* 相对扫描根的显示名；相对不出来时退回文件名 */
std::string display_name(const fs::path &root, const fs::path &p)
{
    const fs::path relative = p.lexically_relative(root);
    if (relative.empty()) {
        return p.filename().string();
    }
    return relative.string();
}

/* ── 一次遍历里拿到的条目信息 ─────────────────────────── */

/* 两条路线的差别只在「怎么把属性取出来」，取完都交成一个 EntryInfo，
   后面的统计就不必再分两套。 */
struct EntryInfo {
    fs::path path;
    bool is_directory = false;
    std::uintmax_t size = 0;
    fs::file_time_type modified{};
};

/* ── 累加器：把一条条 EntryInfo 汇成一份报告 ──────────── */

class Accumulator {
public:
    Accumulator(const fs::path &root, bool recursive, std::size_t recent_limit)
        : root_(root), recent_limit_(recent_limit)
    {
        report_.root = root.string();
        report_.recursive = recursive;
    }

    void add(const EntryInfo &info, std::size_t depth)
    {
        const std::string name = display_name(root_, info.path);

        if (info.is_directory) {
            ++report_.directory_count;
            return;                         /* 目录不参与扩展名统计 */
        }

        ++report_.file_count;
        report_.total_bytes += info.size;

        /* 同层时按名字取小的那个，报告才不会随遍历顺序变化 */
        if (depth > report_.max_depth
            || (depth == report_.max_depth
                && (report_.deepest_relative_path.empty()
                    || name < report_.deepest_relative_path))) {
            report_.max_depth = depth;
            report_.deepest_relative_path = name;
        }

        const std::string key = extension_key(info.path);
        ExtensionStat &stat = stats_[key];
        if (stat.extension.empty()) {
            stat.extension = key;
        }
        ++stat.file_count;
        stat.total_bytes += info.size;
        if (depth > stat.max_depth) {
            stat.max_depth = depth;
        }
        if (info.size > stat.largest_bytes
            || (info.size == stat.largest_bytes
                && (stat.largest_relative_path.empty()
                    || name < stat.largest_relative_path))) {
            stat.largest_bytes = info.size;
            stat.largest_relative_path = name;
        }

        RecentFile item;
        item.relative_path = name;
        item.size = info.size;
        item.modified = to_readable_time(info.modified);
        recent_.push_back(std::make_pair(info.modified, item));
    }

    ScanReport take()
    {
        for (std::map<std::string, ExtensionStat>::const_iterator it = stats_.begin();
             it != stats_.end(); ++it) {
            report_.by_extension.push_back(it->second);
        }
        std::sort(report_.by_extension.begin(), report_.by_extension.end(),
                  [](const ExtensionStat &a, const ExtensionStat &b) {
                      if (a.total_bytes != b.total_bytes) {
                          return a.total_bytes > b.total_bytes;
                      }
                      return a.extension < b.extension;
                  });

        std::sort(recent_.begin(), recent_.end(),
                  [](const std::pair<fs::file_time_type, RecentFile> &a,
                     const std::pair<fs::file_time_type, RecentFile> &b) {
                      if (a.first != b.first) {
                          return a.first > b.first;     /* 新的在前 */
                      }
                      return a.second.relative_path < b.second.relative_path;
                  });
        for (std::size_t i = 0; i < recent_.size() && i < recent_limit_; ++i) {
            report_.recent.push_back(recent_[i].second);
        }
        return report_;
    }

private:
    fs::path root_;
    std::size_t recent_limit_ = 5;
    ScanReport report_;
    std::map<std::string, ExtensionStat> stats_;
    std::vector<std::pair<fs::file_time_type, RecentFile> > recent_;
};

/* ── 第一条路线：全部走带 error_code 的重载 ───────────── */

fs::directory_options directory_flags(const ScanOptions &options)
{
    return options.skip_permission_denied ? fs::directory_options::skip_permission_denied
                                          : fs::directory_options::none;
}

/* 第一条路线取属性：一连串带 error_code 的重载，任一步出错就把 ec 设上 */
bool read_entry_with_error_code(const fs::directory_entry &entry, EntryInfo &info,
                                std::error_code &ec)
{
    info.path = entry.path();
    info.is_directory = entry.is_directory(ec);
    if (ec) {
        return false;
    }
    info.size = info.is_directory ? 0 : entry.file_size(ec);
    if (ec) {
        return false;
    }
    info.modified = entry.last_write_time(ec);
    return !ec;
}

bool walk_with_error_code(const fs::path &root, const ScanOptions &options,
                          Accumulator &accumulator, std::error_code &ec)
{
    const fs::directory_options flags = directory_flags(options);

    if (!options.recursive) {
        /* 只走一层：directory_iterator 不往下钻 */
        fs::directory_iterator it(root, flags, ec);
        if (ec) {
            return false;
        }
        const fs::directory_iterator last;
        for (; it != last; it.increment(ec)) {
            EntryInfo info;
            if (ec || !read_entry_with_error_code(*it, info, ec)) {
                return false;
            }
            accumulator.add(info, 0);
        }
        return true;
    }

    /* 递归：depth() 在迭代器上，不在条目上 */
    fs::recursive_directory_iterator it(root, flags, ec);
    if (ec) {
        return false;
    }
    const fs::recursive_directory_iterator last;
    for (; it != last; it.increment(ec)) {
        EntryInfo info;
        if (ec || !read_entry_with_error_code(*it, info, ec)) {
            return false;                   /* 也可能是 increment 那一步失败的 */
        }
        accumulator.add(info, static_cast<std::size_t>(it.depth()));
    }
    return !ec;
}

/* ── 第二条路线：全部走会抛异常的重载 ─────────────────── */

EntryInfo entry_info_or_throw(const fs::directory_entry &entry)
{
    EntryInfo info;
    info.path = entry.path();
    info.is_directory = entry.is_directory();           /* 失败就抛 filesystem_error */
    info.size = info.is_directory ? 0 : entry.file_size();
    info.modified = entry.last_write_time();
    return info;
}

void walk_or_throw(const fs::path &root, const ScanOptions &options,
                   Accumulator &accumulator)
{
    const fs::directory_options flags = directory_flags(options);

    if (!options.recursive) {
        for (const fs::directory_entry &entry : fs::directory_iterator(root, flags)) {
            accumulator.add(entry_info_or_throw(entry), 0);
        }
        return;
    }

    for (fs::recursive_directory_iterator it(root, flags);
         it != fs::recursive_directory_iterator(); ++it) {
        accumulator.add(entry_info_or_throw(*it), static_cast<std::size_t>(it.depth()));
    }
}

/* ── 自测用的小工具 ───────────────────────────────────── */

/* 写一个恰好 bytes 字节的文件，内容全是 fill */
void write_filled_file(const fs::path &p, std::size_t bytes, char fill)
{
    std::ofstream out(p, std::ios::binary);
    out << std::string(bytes, fill);
}

/* 把修改时间往前推 seconds_ago 秒，让「最近修改」的次序是确定的 */
void age_file(const fs::path &p, int seconds_ago)
{
    const fs::file_time_type when =
        fs::file_time_type::clock::now() - std::chrono::seconds(seconds_ago);
    fs::last_write_time(p, when);
}

/* 把报告里的一项按扩展名找出来，找不到返回 nullptr */
const ExtensionStat *find_extension(const ScanReport &report, const std::string &key)
{
    for (std::size_t i = 0; i < report.by_extension.size(); ++i) {
        if (report.by_extension[i].extension == key) {
            return &report.by_extension[i];
        }
    }
    return nullptr;
}

std::string number(std::uintmax_t value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

/* 自测的小工具：把每一项的结果记下来，不打印 */
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
            line << "[失败] " << result_.total << ". " << what;
            if (!detail.empty()) {
                line << "（" << detail << "）";
            }
        }
        result_.lines.push_back(line.str());
    }

    CheckResult take() { return std::move(result_); }

private:
    CheckResult result_;
};

}   /* namespace */

/* ── 对外接口 ─────────────────────────────────────────── */

std::string to_readable_time(fs::file_time_type when)
{
    const std::chrono::system_clock::time_point point = to_system_time(when);
    const std::time_t seconds = std::chrono::system_clock::to_time_t(point);

    std::tm broken_down{};
#if defined(_WIN32)
    /* MinGW-w64 与 MSVC 都提供 localtime_s（参数顺序是 tm* 在前） */
    if (localtime_s(&broken_down, &seconds) != 0) {
        return std::string();
    }
#else
    if (localtime_r(&seconds, &broken_down) == nullptr) {
        return std::string();
    }
#endif

    char buffer[32];
    if (std::strftime(buffer, sizeof buffer, "%Y-%m-%d %H:%M:%S", &broken_down) == 0) {
        return std::string();
    }
    return std::string(buffer);
}

fs::path join_paths(const fs::path &base, const fs::path &name)
{
    return base / name;                     /* operator/ 会自己补分隔符 */
}

fs::path relative_of(const fs::path &base, const fs::path &child)
{
    return child.lexically_relative(base);
}

ScanReport scan_with_error_code(const fs::path &root, const ScanOptions &options,
                                std::error_code &ec)
{
    ec.clear();
    Accumulator accumulator(root, options.recursive, options.recent_limit);
    if (!walk_with_error_code(root, options, accumulator, ec)) {
        /* 出错时给一份空报告：根目录记着，统计全是零 */
        Accumulator empty(root, options.recursive, options.recent_limit);
        return empty.take();
    }
    return accumulator.take();
}

ScanReport scan_or_throw(const fs::path &root, const ScanOptions &options)
{
    Accumulator accumulator(root, options.recursive, options.recent_limit);
    walk_or_throw(root, options, accumulator);
    return accumulator.take();
}

std::string format_report(const ScanReport &report)
{
    std::ostringstream os;
    os << "扫描根目录：" << report.root
       << (report.recursive ? "（递归，含子目录）" : "（非递归，只一层）") << "\n";
    os << "  文件 " << report.file_count << " 个，目录 " << report.directory_count
       << " 个，合计 " << report.total_bytes << " 字节\n";
    if (!report.deepest_relative_path.empty()) {
        os << "  最深层级：第 " << report.max_depth << " 层，例如 "
           << report.deepest_relative_path << "\n";
    }

    os << "  按扩展名分类（总字节多的在前）：\n";
    for (std::size_t i = 0; i < report.by_extension.size(); ++i) {
        const ExtensionStat &stat = report.by_extension[i];
        os << "    " << std::left << std::setw(9) << stat.extension << std::right
           << std::setw(3) << stat.file_count << " 个"
           << std::setw(9) << stat.total_bytes << " 字节"
           << "   最大 " << stat.largest_relative_path
           << "（" << stat.largest_bytes << " 字节）"
           << "   最深 第 " << stat.max_depth << " 层\n";
    }

    os << "  最近修改的 " << report.recent.size() << " 个文件：\n";
    for (std::size_t i = 0; i < report.recent.size(); ++i) {
        const RecentFile &item = report.recent[i];
        os << "    " << item.modified << std::setw(9) << item.size
           << " 字节   " << item.relative_path << "\n";
    }
    return os.str();
}

std::string build_demo_output(const fs::path &root)
{
    std::ostringstream os;
    ScanOptions options;
    std::error_code ec;

    /* 1. 递归扫一遍 */
    const ScanReport full = scan_with_error_code(root, options, ec);
    if (ec) {
        os << "扫描 " << root.string() << " 失败：error_code = " << ec.value()
           << "（" << ec.message() << "）\n";
    } else {
        os << format_report(full);
    }

    /* 2. 同一个根，非递归再扫一遍，两种迭代器对照 */
    ScanOptions flat_options;
    flat_options.recursive = false;
    std::error_code flat_ec;
    const ScanReport flat = scan_with_error_code(root, flat_options, flat_ec);
    os << "\n两种遍历对照：\n";
    os << "  directory_iterator            只走一层，看到 " << flat.file_count
       << " 个文件、" << flat.directory_count << " 个目录、" << flat.total_bytes << " 字节\n";
    os << "  recursive_directory_iterator  连子目录，看到 " << full.file_count
       << " 个文件、" << full.directory_count << " 个目录、" << full.total_bytes << " 字节\n";

    /* 3. path 拼装与 lexically_relative，两者都是纯文本运算 */
    os << "\npath 拼装：\n";
    os << "  root / \"sub\" / \"deep\"            = "
       << join_paths(join_paths(root, "sub"), "deep").string() << "\n";
    fs::path appended = root;
    appended += "sub/deep";
    os << "  root += \"sub/deep\"（不补分隔符）  = " << appended.string() << "\n";
    if (!full.deepest_relative_path.empty()) {
        const fs::path deepest = join_paths(root, full.deepest_relative_path);
        os << "  lexically_relative(" << deepest.string() << ", " << root.string()
           << ") = " << relative_of(root, deepest).string() << "\n";
    }
    os << "  lexically_relative(\"a\", \"a/b/../c\") = "
       << relative_of("a", "a/b/../c").string() << "（纯文本，不化简 ..）\n";

    /* 4. 同一个不存在的路径，两条路线各走一次 */
    const fs::path missing = join_paths(root, "no_such_subdir");
    os << "\n不存在的路径：" << missing.string() << "\n";

    std::error_code missing_ec;
    const ScanReport nothing = scan_with_error_code(missing, options, missing_ec);
    os << "  error_code 版本：不抛异常，报告里 " << nothing.file_count
       << " 个文件；ec = " << missing_ec.value()
       << "（" << missing_ec.category().name() << "），消息：" << missing_ec.message() << "\n";

    os << "  异常版本：";
    try {
        const ScanReport thrown = scan_or_throw(missing, options);
        os << "没有抛异常，报告里 " << thrown.file_count << " 个文件\n";
    } catch (const fs::filesystem_error &e) {
        /* filesystem_error 是 system_error 的派生类，
           除 what() 之外还带两个路径与一个错误码 */
        os << "抛出 filesystem_error\n";
        os << "    what()         = " << e.what() << "\n";
        os << "    code().value() = " << e.code().value() << "\n";
        os << "    path1()        = " << e.path1().string() << "\n";
        os << "    path2()        = " << e.path2().string() << "\n";
    }

    os << "  取舍：两条路线报的是同一批错误码，区别在于失败怎么交出来。\n";
    os << "        「文件不存在」属于预期结果，用 error_code 版，调用方按返回值分支；\n";
    os << "        权限不足、磁盘故障这类意外用异常版，错误信息自带路径，不会被忽略。\n";
    os << "        带 error_code 的重载只把文件系统错误转成错误码，别的异常照样抛，\n";
    os << "        因此外层该有的 try 不能因为用了 error_code 版就省掉。\n";
    return os.str();
}

CheckResult run_self_tests()
{
    Checker c;

    /* 自测自带一棵临时目录树：建、写、扫、断言、清理，不碰仓库里的文件 */
    std::error_code ec;
    const fs::path base = fs::temp_directory_path(ec);
    c.check(!ec && !base.empty(), "取得系统临时目录",
            ec ? ec.message() : base.string());
    if (ec) {
        return c.take();
    }

    const fs::path root = join_paths(base, "dirscan_selftest");
    std::error_code cleanup_ec;
    fs::remove_all(root, cleanup_ec);       /* 上一次留下的残骸先清掉 */

    ec.clear();
    const bool created = fs::create_directories(root / "sub" / "deep", ec);
    if (!created || ec) {
        c.check(false, "在临时目录下建起三层目录树并写进 6 个文件",
                "建目录失败：" + ec.message());
        return c.take();
    }

    write_filled_file(root / "alpha.txt", 100, 'a');
    write_filled_file(root / "bravo.log", 40, 'b');
    write_filled_file(root / "noext", 7, 'n');
    write_filled_file(root / "sub" / "charlie.txt", 300, 'c');
    write_filled_file(root / "sub" / "deep" / "delta.bin", 1000, 'd');
    write_filled_file(root / "sub" / "deep" / "echo.TXT", 20, 'e');

    age_file(root / "alpha.txt", 10);
    age_file(root / "bravo.log", 50);
    age_file(root / "noext", 20);
    age_file(root / "sub" / "charlie.txt", 30);
    age_file(root / "sub" / "deep" / "delta.bin", 5);
    age_file(root / "sub" / "deep" / "echo.TXT", 70);

    bool all_written = true;
    for (const char *name : {"alpha.txt", "bravo.log", "noext", "sub/charlie.txt",
                             "sub/deep/delta.bin", "sub/deep/echo.TXT"}) {
        if (!fs::is_regular_file(root / name)) {
            all_written = false;
        }
    }
    c.check(all_written,
            "在临时目录下建起三层目录树，写进 6 个文件：四种扩展名归类、大小与层级都不同",
            "根目录 " + root.string());

    ScanOptions recursive_options;
    recursive_options.recent_limit = 3;

    ScanOptions flat_options;
    flat_options.recursive = false;
    flat_options.recent_limit = 3;

    std::error_code scan_ec;
    const ScanReport flat = scan_with_error_code(root, flat_options, scan_ec);
    c.check(!scan_ec && flat.file_count == 3 && flat.directory_count == 1,
            "非递归扫描只走一层",
            "文件 " + std::to_string(flat.file_count) + " 个，目录 "
                + std::to_string(flat.directory_count) + " 个");

    std::error_code full_ec;
    const ScanReport full = scan_with_error_code(root, recursive_options, full_ec);
    c.check(!full_ec && full.file_count == 6 && full.directory_count == 2,
            "递归扫描把子目录里的文件也算进来",
            "文件 " + std::to_string(full.file_count) + " 个，目录 "
                + std::to_string(full.directory_count) + " 个");

    c.check(full.total_bytes == 1467, "总字节数与写进去的一致",
            "合计 " + number(full.total_bytes) + " 字节，期望 1467");

    const ExtensionStat *txt = find_extension(full, ".txt");
    c.check(txt != nullptr && txt->file_count == 3 && txt->total_bytes == 420,
            ".txt 一类合在一起，.TXT 也归进来（大小写不敏感）",
            txt == nullptr ? "报告里没有 .txt 这一类"
                           : "文件 " + std::to_string(txt->file_count) + " 个，字节 "
                                 + number(txt->total_bytes));

    const ExtensionStat *none = find_extension(full, "(none)");
    c.check(none != nullptr && none->file_count == 1 && none->total_bytes == 7,
            "没有扩展名的文件归到 (none) 一类",
            none == nullptr ? "报告里没有 (none) 这一类"
                            : "文件 " + std::to_string(none->file_count) + " 个，字节 "
                                  + number(none->total_bytes));

    const ExtensionStat *bin = find_extension(full, ".bin");
    c.check(bin != nullptr && bin->largest_bytes == 1000
                && fs::path(bin->largest_relative_path)
                       == fs::path("sub") / "deep" / "delta.bin",
            "最大文件落在 .bin 这一类里，是 1000 字节的 delta.bin",
            bin == nullptr ? "报告里没有 .bin 这一类"
                           : bin->largest_relative_path + "（"
                                 + number(bin->largest_bytes) + " 字节）");

    c.check(full.max_depth == 2
                && fs::path(full.deepest_relative_path)
                       == fs::path("sub") / "deep" / "delta.bin",
            "最深层级是第 2 层，最深文件是 sub/deep/delta.bin",
            "第 " + std::to_string(full.max_depth) + " 层："
                + full.deepest_relative_path);

    bool recent_ok = full.recent.size() == 3;
    if (recent_ok) {
        recent_ok = fs::path(full.recent[0].relative_path) == fs::path("sub") / "deep" / "delta.bin"
                    && fs::path(full.recent[1].relative_path) == fs::path("alpha.txt")
                    && fs::path(full.recent[2].relative_path) == fs::path("noext");
    }
    c.check(recent_ok, "最近修改的三个文件按时间从新到旧，最新的是 5 秒前的 delta.bin",
            full.recent.empty() ? "列表是空的" : full.recent[0].relative_path);

    bool shape_ok = !full.recent.empty() && full.recent[0].modified.size() == 19;
    if (shape_ok) {
        const std::string &stamp = full.recent[0].modified;
        shape_ok = stamp[4] == '-' && stamp[7] == '-' && stamp[10] == ' '
                   && stamp[13] == ':' && stamp[16] == ':';
    }
    c.check(shape_ok, "last_write_time 换算成了 YYYY-MM-DD HH:MM:SS 形状的可读时间",
            full.recent.empty() ? "列表是空的" : full.recent[0].modified);

    c.check(fs::file_size(root / "sub" / "deep" / "delta.bin") == 1000,
            "file_size 读回来的大小与写进去的字节数一致",
            "delta.bin " + number(fs::file_size(root / "sub" / "deep" / "delta.bin"))
                + " 字节");

    const fs::path joined = join_paths(join_paths(root, "sub"), "deep");
    fs::path appended = root;
    appended += "sub";
    c.check(joined == root / "sub" / "deep" && appended.string() == root.string() + "sub",
            "operator/ 会补分隔符，+= 直接把字符接上去",
            "root / \"sub\" == " + joined.string() + "，root += \"sub\" == " + appended.string());

    const fs::path relative = relative_of(root, root / "sub" / "deep" / "delta.bin");
    c.check(relative == fs::path("sub") / "deep" / "delta.bin",
            "lexically_relative 把子路径变回相对路径",
            relative.string());

    c.check(relative_of("a", "a/b/../c") == fs::path("b") / ".." / "c",
            "lexically_relative 是纯文本运算，不化简 ..",
            relative_of("a", "a/b/../c").string());

    const fs::path missing = join_paths(root, "no_such_subdir");
    std::error_code missing_ec;
    const ScanReport nothing = scan_with_error_code(missing, recursive_options, missing_ec);
    c.check(static_cast<bool>(missing_ec) && missing_ec.value() != 0
                && nothing.file_count == 0,
            "不存在的路径：error_code 版本给出错误码，不抛异常",
            "ec = " + std::to_string(missing_ec.value()) + "，报告里 "
                + std::to_string(nothing.file_count) + " 个文件");

    bool caught = false;
    int thrown_code = 0;
    bool threw_filesystem_error = false;
    try {
        const ScanReport thrown = scan_or_throw(missing, recursive_options);
        thrown_code = static_cast<int>(thrown.file_count);
    } catch (const fs::filesystem_error &e) {
        caught = true;
        threw_filesystem_error = true;
        thrown_code = e.code().value();
    } catch (const std::exception &) {
        caught = true;
    }
    c.check(caught && threw_filesystem_error && thrown_code == missing_ec.value(),
            "不存在的路径：异常版本抛出 filesystem_error，错误码与上一条相同",
            "抛出 " + std::string(threw_filesystem_error ? "filesystem_error" : "别的异常")
                + "，code = " + std::to_string(thrown_code));

    std::error_code remove_ec;
    fs::remove_all(root, remove_ec);
    c.check(!remove_ec && !fs::exists(root), "自测建的临时目录树清理干净",
            "清理之后 " + root.string() + " 还在吗："
                + (fs::exists(root) ? "在" : "不在"));

    return c.take();
}

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << total << " 项中 " << passed << " 项通过";
    if (failed == 0) {
        os << "，全部通过";
    } else {
        os << "，" << failed << " 项失败";
    }
    return os.str();
}

}   /* namespace dirscan */
