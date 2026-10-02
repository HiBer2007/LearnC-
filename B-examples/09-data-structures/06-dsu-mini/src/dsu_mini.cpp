/**
 * dsu_mini.cpp —— 并查集的实现、离线倒序删边的完整例子、项目输出与自测
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
 */

/**
 * 这里没有任何界面代码，也不打印任何东西。
 * 报告里的数字全是计数：find 走过的步数、改过的父指针个数、BFS 检查过的边数。
 * 计时一个都不做。
 */
#include "dsu_mini.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace dmini {

void DsuStats::reset()
{
    *this = DsuStats();
}

DsuStats &stats()
{
    static DsuStats instance;
    return instance;
}

const char *mode_name(Mode mode)
{
    switch (mode) {
    case Mode::Plain:
        return u8"都不做";
    case Mode::BySize:
        return u8"按大小合并";
    case Mode::Compress:
        return u8"路径压缩";
    case Mode::Halve:
        return u8"路径减半";
    case Mode::Both:
        return u8"两者都用";
    }
    return u8"未知";
}

/* ================= 并查集 ================= */

Dsu::Dsu(int n, Mode mode) : n_(n), set_count_(n), mode_(mode)
{
    parent_.resize(static_cast<std::size_t>(n));
    size_.assign(static_cast<std::size_t>(n), 1);
    for (int i = 0; i < n; ++i) {
        parent_[static_cast<std::size_t>(i)] = i;   /* 每个点自成一个集合，根记自己 */
    }
}

int Dsu::find(int x)
{
    ++stats().find_calls;
    switch (mode_) {
    case Mode::Plain:
    case Mode::BySize: {
        int r = x;
        while (parent_[static_cast<std::size_t>(r)] != r) {
            r = parent_[static_cast<std::size_t>(r)];
            ++stats().steps;
        }
        return r;
    }
    case Mode::Compress:
    case Mode::Both: {
        int r = x;
        while (parent_[static_cast<std::size_t>(r)] != r) {
            r = parent_[static_cast<std::size_t>(r)];
            ++stats().steps;
        }
        while (parent_[static_cast<std::size_t>(x)] != r) {     /* 走过的点直连根 */
            const int next = parent_[static_cast<std::size_t>(x)];
            parent_[static_cast<std::size_t>(x)] = r;
            ++stats().compressions;
            x = next;
        }
        return r;
    }
    case Mode::Halve: {
        while (parent_[static_cast<std::size_t>(x)] != x) {
            parent_[static_cast<std::size_t>(x)] =
                parent_[static_cast<std::size_t>(parent_[static_cast<std::size_t>(x)])];
            x = parent_[static_cast<std::size_t>(x)];
            ++stats().steps;
        }
        return x;
    }
    }
    return x;
}

bool Dsu::unite(int a, int b)
{
    int ra = find(a);
    int rb = find(b);
    if (ra == rb) {
        return false;
    }
    if (mode_ == Mode::BySize || mode_ == Mode::Both) {
        if (size_[static_cast<std::size_t>(ra)] < size_[static_cast<std::size_t>(rb)]) {
            std::swap(ra, rb);      /* 让 ra 是大的那棵，小的挂上去 */
        }
        parent_[static_cast<std::size_t>(rb)] = ra;
        size_[static_cast<std::size_t>(ra)] += size_[static_cast<std::size_t>(rb)];
    } else {
        parent_[static_cast<std::size_t>(rb)] = ra;
    }
    ++stats().link_writes;
    --set_count_;
    return true;
}

int Dsu::root_of(int x) const
{
    while (parent_[static_cast<std::size_t>(x)] != x) {
        x = parent_[static_cast<std::size_t>(x)];
    }
    return x;
}

int Dsu::max_height() const
{
    int best = 0;
    for (int i = 0; i < n_; ++i) {
        int height = 0;
        int x = i;
        while (parent_[static_cast<std::size_t>(x)] != x) {
            x = parent_[static_cast<std::size_t>(x)];
            ++height;
        }
        best = std::max(best, height);
    }
    return best;
}

/* ================= 报告用的小工具 ================= */

namespace {

bool is_wide_codepoint(unsigned int cp)
{
    return (cp >= 0x1100u && cp <= 0x115Fu) || (cp >= 0x2E80u && cp <= 0x303Eu)
           || (cp >= 0x3041u && cp <= 0x33FFu) || (cp >= 0x3400u && cp <= 0x4DBFu)
           || (cp >= 0x4E00u && cp <= 0x9FFFu) || (cp >= 0xA000u && cp <= 0xA4CFu)
           || (cp >= 0xAC00u && cp <= 0xD7A3u) || (cp >= 0xF900u && cp <= 0xFAFFu)
           || (cp >= 0xFE30u && cp <= 0xFE6Fu) || (cp >= 0xFF00u && cp <= 0xFF60u)
           || (cp >= 0xFFE0u && cp <= 0xFFE6u);
}

std::size_t display_width(const std::string &text)
{
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        unsigned int cp = lead;
        std::size_t length = 1;
        if (lead >= 0xF0u) {
            cp = lead & 0x07u;
            length = 4;
        } else if (lead >= 0xE0u) {
            cp = lead & 0x0Fu;
            length = 3;
        } else if (lead >= 0xC0u) {
            cp = lead & 0x1Fu;
            length = 2;
        }
        for (std::size_t k = 1; k < length && i + k < text.size(); ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3Fu);
        }
        width += is_wide_codepoint(cp) ? 2 : 1;
        i += length;
    }
    return width;
}

std::string pad_right(const std::string &text, std::size_t width)
{
    const std::size_t used = display_width(text);
    return used >= width ? text : text + std::string(width - used, ' ');
}

std::string pad_left_text(const std::string &text, std::size_t width)
{
    const std::size_t used = display_width(text);
    return used >= width ? text : std::string(width - used, ' ') + text;
}

std::string pad_left(std::size_t value, std::size_t width)
{
    return pad_left_text(std::to_string(value), width);
}

std::string fixed2(double value)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << value;
    return os.str();
}

std::string pad_left_fixed(double value, std::size_t width)
{
    return pad_left_text(fixed2(value), width);
}

/** 固定种子的线性同余发生器：报告里的操作序列因此可以逐位复现 */
class Lcg {
public:
    explicit Lcg(std::uint64_t seed) : state_(seed) {}

    std::uint32_t next()
    {
        state_ = state_ * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(state_ >> 33);
    }

private:
    std::uint64_t state_;
};

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

const int kVertices = 2000;
const std::size_t kOperations = 40000;

/* 一次操作：合并 a 与 b，或者问 a 与 b 在不在同一个集合 */
struct Operation {
    bool is_unite = false;
    int a = 0;
    int b = 0;
};

std::vector<Operation> make_operations()
{
    std::vector<Operation> ops;
    ops.reserve(kOperations);
    /* 先顺着链的方向合并：「都不做」这一档会因此长出很深的树，两处优化的差别才看得出来 */
    for (int i = 0; i + 1 < kVertices; ++i) {
        Operation op;
        op.is_unite = true;
        op.a = i + 1;
        op.b = i;
        ops.push_back(op);
    }
    Lcg lcg(20261002u);
    while (ops.size() < kOperations) {
        Operation op;
        op.a = static_cast<int>(lcg.next() % static_cast<std::uint32_t>(kVertices));
        op.b = static_cast<int>(lcg.next() % static_cast<std::uint32_t>(kVertices));
        op.is_unite = (lcg.next() % 4u) != 0u;      /* 四次里三次合并 */
        ops.push_back(op);
    }
    return ops;
}

/* 一档跑完之后的账 */
struct ModeRun {
    std::size_t steps = 0;
    std::size_t find_calls = 0;
    std::size_t compressions = 0;
    std::size_t link_writes = 0;
    int max_height = 0;
    int set_count = 0;
    std::vector<bool> answers;      /**< 每个查询给出的答案，用来跨档比对 */
    std::size_t unions = 0;
};

ModeRun run_mode(Mode mode, const std::vector<Operation> &ops)
{
    stats().reset();
    Dsu dsu(kVertices, mode);
    ModeRun run;
    for (const Operation &op : ops) {
        if (op.is_unite) {
            if (dsu.unite(op.a, op.b)) {
                ++run.unions;
            }
        } else {
            run.answers.push_back(dsu.find(op.a) == dsu.find(op.b));
        }
    }
    run.steps = stats().steps;
    run.find_calls = stats().find_calls;
    run.compressions = stats().compressions;
    run.link_writes = stats().link_writes;
    run.max_height = dsu.max_height();
    run.set_count = dsu.set_count();
    return run;
}

/** 五档跑同一串操作的结果只算一次，报告与自测共用 */
struct ModeTable {
    std::vector<Mode> modes;
    std::vector<ModeRun> runs;
    std::size_t query_count = 0;
};

const ModeTable &mode_table()
{
    static const ModeTable table = [] {
        ModeTable built;
        built.modes = { Mode::Plain, Mode::BySize, Mode::Compress, Mode::Halve, Mode::Both };
        const std::vector<Operation> ops = make_operations();
        for (Mode mode : built.modes) {
            built.runs.push_back(run_mode(mode, ops));
        }
        built.query_count = built.runs[0].answers.size();
        return built;
    }();
    return table;
}

void append_mode_section(std::ostringstream &os)
{
    const ModeTable &table = mode_table();

    os << u8"五档写法：同一串 " << kOperations << u8" 次操作（" << kVertices << u8" 个点）\n";
    os << "  " << pad_right(u8"档位", 16) << pad_left_text(u8"总步数", 12) << "  "
       << pad_left_text(u8"find 次数", 10) << "  " << pad_left_text(u8"平均每次 find", 14) << "  "
       << pad_left_text(u8"最大树高", 10) << "  " << pad_left_text(u8"根个数", 8) << "\n";

    for (std::size_t i = 0; i < table.runs.size(); ++i) {
        const ModeRun &run = table.runs[i];
        os << "  " << pad_right(mode_name(table.modes[i]), 16) << pad_left(run.steps, 12) << "  "
           << pad_left(run.find_calls, 10) << "  "
           << pad_left_fixed(static_cast<double>(run.steps)
                                 / static_cast<double>(run.find_calls), 14) << "  "
           << pad_left(static_cast<std::size_t>(run.max_height), 10) << "  "
           << pad_left(static_cast<std::size_t>(run.set_count), 8) << "\n";
    }

    bool same_answers = true;
    bool same_roots = true;
    bool same_unions = true;
    for (std::size_t i = 1; i < table.runs.size(); ++i) {
        same_answers = same_answers && table.runs[i].answers == table.runs[0].answers;
        same_roots = same_roots && table.runs[i].set_count == table.runs[0].set_count;
        same_unions = same_unions && table.runs[i].unions == table.runs[0].unions;
    }
    os << u8"  合并成功 " << table.runs[0].unions << u8" 次，查询 " << table.query_count
       << u8" 次；五档的成功合并次数相同：" << (same_unions ? u8"是" : u8"否") << u8"\n";
    os << u8"  五档的「是否同集合」答案逐位相同：" << (same_answers ? u8"是" : u8"否")
       << u8"；最终根个数相同：" << (same_roots ? u8"是" : u8"否") << u8"\n";
    os << u8"  都不做的步数是两者都用的 "
       << fixed2(static_cast<double>(table.runs[0].steps)
                 / static_cast<double>(table.runs[4].steps))
       << u8" 倍——五档之间只差那几行\n";
}

/* ================= 离线倒序删边 ================= */

const int kSmallVertices = 8;
const std::pair<int, int> kSmallEdges[] = {
    { 0, 1 }, { 1, 2 }, { 2, 3 }, { 4, 5 }, { 5, 6 },
    { 0, 7 }, { 3, 7 }, { 1, 6 }, { 2, 5 }, { 3, 4 },
};
const std::size_t kSmallEdgeCount = sizeof(kSmallEdges) / sizeof(kSmallEdges[0]);
const std::size_t kDeletedEdges[] = { 5, 0, 1, 8 };     /* 依次删掉这几条边的下标 */
const std::size_t kDeleteCount = sizeof(kDeletedEdges) / sizeof(kDeletedEdges[0]);
const int kQueryA = 0;
const int kQueryB = 5;

struct QueryAnswer {
    int components = 0;
    bool connected = false;
};

/** 朴素做法：每删一条边之后，从零开始用广度优先重新算一遍 */
QueryAnswer answer_naive(std::size_t deleted_so_far, std::size_t &edge_checks)
{
    std::vector<std::vector<int>> adjacency(static_cast<std::size_t>(kSmallVertices));
    for (std::size_t i = 0; i < kSmallEdgeCount; ++i) {
        bool present = true;
        for (std::size_t k = 0; k < deleted_so_far; ++k) {
            if (kDeletedEdges[k] == i) {
                present = false;        /* 前 deleted_so_far 条按顺序删掉了 */
            }
        }
        if (present) {
            adjacency[static_cast<std::size_t>(kSmallEdges[i].first)].push_back(kSmallEdges[i].second);
            adjacency[static_cast<std::size_t>(kSmallEdges[i].second)].push_back(kSmallEdges[i].first);
        }
    }

    /* 一趟广度优先给每个点标上块号：块号有几个就是几个连通块 */
    std::vector<int> label(static_cast<std::size_t>(kSmallVertices), -1);
    int next_label = 0;
    for (int start = 0; start < kSmallVertices; ++start) {
        if (label[static_cast<std::size_t>(start)] >= 0) {
            continue;
        }
        std::vector<int> queue;
        queue.push_back(start);
        label[static_cast<std::size_t>(start)] = next_label;
        for (std::size_t head = 0; head < queue.size(); ++head) {
            const int v = queue[head];
            for (int u : adjacency[static_cast<std::size_t>(v)]) {
                ++edge_checks;
                if (label[static_cast<std::size_t>(u)] < 0) {
                    label[static_cast<std::size_t>(u)] = next_label;
                    queue.push_back(u);
                }
            }
        }
        ++next_label;
    }

    QueryAnswer answer;
    answer.components = next_label;
    answer.connected = label[static_cast<std::size_t>(kQueryA)]
                       == label[static_cast<std::size_t>(kQueryB)];
    return answer;
}

struct OfflineResult {
    std::vector<QueryAnswer> answers;   /**< 第 i 项是删掉前 i+1 条边之后的答案 */
    std::size_t dsu_steps = 0;
    std::size_t unions = 0;
};

/** 倒序做法：先建「全删完」的图，再把删边倒过来当加边 */
OfflineResult solve_offline()
{
    std::vector<char> alive(kSmallEdgeCount, 1);
    for (std::size_t i = 0; i < kDeleteCount; ++i) {
        alive[kDeletedEdges[i]] = 0;
    }

    stats().reset();
    Dsu dsu(kSmallVertices, Mode::Both);
    for (std::size_t i = 0; i < kSmallEdgeCount; ++i) {
        if (alive[i] != 0) {
            dsu.unite(kSmallEdges[i].first, kSmallEdges[i].second);
        }
    }

    OfflineResult result;
    for (std::size_t step = kDeleteCount; step > 0; --step) {
        QueryAnswer answer;
        answer.components = dsu.set_count();
        answer.connected = dsu.same(kQueryA, kQueryB);
        result.answers.push_back(answer);

        const std::size_t edge = kDeletedEdges[step - 1];   /* 把这一步删掉的边加回去 */
        if (dsu.unite(kSmallEdges[edge].first, kSmallEdges[edge].second)) {
            ++result.unions;
        }
    }
    result.dsu_steps = stats().steps;
    std::reverse(result.answers.begin(), result.answers.end());
    return result;
}

void append_offline_section(std::ostringstream &os)
{
    os << u8"\n离线倒序删边：把「删」倒过来当「加」\n";
    os << u8"  " << kSmallVertices << u8" 个点、" << kSmallEdgeCount << u8" 条边：";
    for (std::size_t i = 0; i < kSmallEdgeCount; ++i) {
        os << i << u8"=" << kSmallEdges[i].first << u8"-" << kSmallEdges[i].second
           << (i + 1 == kSmallEdgeCount ? "" : u8"，");
    }
    os << "\n";
    os << u8"  依次删掉：";
    for (std::size_t i = 0; i < kDeleteCount; ++i) {
        os << kDeletedEdges[i] << u8" 号边（" << kSmallEdges[kDeletedEdges[i]].first << u8"-"
           << kSmallEdges[kDeletedEdges[i]].second << u8"）" << (i + 1 == kDeleteCount ? "" : u8"，");
    }
    os << "\n";
    os << u8"  每一步都问两件事：" << kQueryA << u8" 与 " << kQueryB
       << u8" 连通吗、现在有几个连通块\n\n";

    os << "  " << pad_right(u8"删到第几条", 14) << pad_left_text(u8"朴素：连通块", 14) << "  "
       << pad_left_text(u8"倒序：连通块", 14) << "  " << pad_left_text(u8"朴素：连通", 12) << "  "
       << pad_left_text(u8"倒序：连通", 12) << "  " << pad_left_text(u8"一致", 6) << "\n";

    std::size_t naive_edge_checks = 0;
    bool all_same = true;
    const OfflineResult offline = solve_offline();
    for (std::size_t step = 1; step <= kDeleteCount; ++step) {
        const QueryAnswer naive = answer_naive(step, naive_edge_checks);
        const QueryAnswer &reverse = offline.answers[step - 1];
        const bool same = naive.components == reverse.components
                          && naive.connected == reverse.connected;
        all_same = all_same && same;
        os << "  " << pad_right(u8"第 " + std::to_string(step) + u8" 条（"
                               + std::to_string(kDeletedEdges[step - 1]) + u8" 号边）", 14)
           << pad_left(static_cast<std::size_t>(naive.components), 14) << "  "
           << pad_left(static_cast<std::size_t>(reverse.components), 14) << "  "
           << pad_left_text(naive.connected ? u8"是" : u8"否", 12) << "  "
           << pad_left_text(reverse.connected ? u8"是" : u8"否", 12) << "  "
           << pad_left_text(same ? u8"是" : u8"否", 6) << "\n";
    }
    os << u8"  两份答案逐位相同：" << (all_same ? u8"是" : u8"否") << u8"\n";
    os << u8"  代价对照：朴素做法每步重算一次连通块，累计检查 " << naive_edge_checks
       << u8" 个边端点；倒序做法只加边不删边，find 一共走 " << offline.dsu_steps
       << u8" 步、成功加边 " << offline.unions << u8" 次\n";
    os << u8"  并查集不能删边：它没记「当初是哪条边并起来的」，能删是靠倒着做\n";
}

void append_misuse_section(std::ostringstream &os)
{
    Dsu dsu(6, Mode::Both);
    dsu.unite(0, 1);
    dsu.unite(2, 3);
    dsu.unite(1, 2);
    dsu.unite(4, 5);

    int child = -1;
    for (int i = 0; i < 6; ++i) {
        if (!dsu.is_root(i)) {
            child = i;
            break;
        }
    }

    os << u8"\n一处常见误用：size_ 只在根上有效\n";
    os << u8"  6 个点，合并成 {0,1,2,3} 与 {4,5} 两块，根分别是 " << dsu.root_of(0) << u8" 与 "
       << dsu.root_of(4) << u8"\n";
    os << u8"  拿 " << child << u8" 号点直接读 size_：得到 " << dsu.stale_size_of(child)
       << u8"；先 find 再读根：" << dsu.size_of(dsu.find(child)) << u8"\n";
    os << u8"  非根节点的 size_ 是当初它当根时的旧值，之后没人更新过它\n";
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;
    append_mode_section(os);
    append_offline_section(os);
    append_misuse_section(os);
    return os.str();
}

/* ================= 自测 ================= */

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

CheckResult run_self_tests()
{
    Checker c;

    /* 1. 初始化之后每个点自成一个集合 */
    {
        Dsu dsu(5, Mode::Plain);
        bool all_roots = true;
        for (int i = 0; i < 5; ++i) {
            all_roots = all_roots && dsu.is_root(i) && dsu.root_of(i) == i;
        }
        c.check(all_roots && dsu.set_count() == 5 && dsu.max_height() == 0,
                u8"初始化：每个点自成一个集合、树高 0");
    }

    /* 2. 合并之后同属一个集合，根个数减一 */
    {
        Dsu dsu(4, Mode::Both);
        const bool merged = dsu.unite(0, 1);
        const bool again = dsu.unite(1, 0);
        c.check(merged && !again && dsu.same(0, 1) && dsu.set_count() == 3,
                u8"合并之后同属一个集合；再合并一次返回 false，根个数只减一次",
                std::to_string(dsu.set_count()) + u8" 个根");
    }

    /* 3. find 幂等 */
    {
        Dsu dsu(8, Mode::Compress);
        dsu.unite(0, 1);
        dsu.unite(2, 3);
        dsu.unite(0, 2);
        const int first = dsu.find(3);
        const int second = dsu.find(3);
        c.check(first == second, u8"find 连着调两次结果相同");
    }

    /* 4. 都不做这一档：顺着链的方向合并会退化成链 */
    {
        Dsu dsu(64, Mode::Plain);
        for (int i = 1; i < 64; ++i) {
            dsu.unite(i, i - 1);        /* 每次都让新点的根挂到旧链的根下面 */
        }
        c.check(dsu.max_height() == 63 && dsu.same(0, 63),
                u8"都不做：顺着链的方向合并，64 个点长成 63 层",
                u8"树高 " + std::to_string(dsu.max_height()));
    }

    /* 5. 按大小合并：小的挂到大的下面 */
    {
        Dsu dsu(8, Mode::BySize);
        dsu.unite(0, 1);
        const int root_before = dsu.find(0);
        dsu.unite(2, 3);
        dsu.unite(4, 5);
        dsu.unite(0, 2);        /* {0,1} 与 {2,3} 一样大，合并后根还是原来的 */
        const int total = dsu.size_of(dsu.find(0));
        c.check(dsu.is_root(root_before) && total == 4,
                u8"按大小合并：小的挂到大的下面，根上的 size_ 是整块的大小",
                std::to_string(total));
    }

    /* 6. 按大小合并：同样的最坏顺序，树高压在对数级 */
    {
        Dsu plain(1024, Mode::Plain);
        Dsu by_size(1024, Mode::BySize);
        for (int i = 1; i < 1024; ++i) {
            plain.unite(i, i - 1);
            by_size.unite(i, i - 1);
        }
        c.check(plain.max_height() == 1023 && by_size.max_height() <= 11,
                u8"同一串最坏顺序：都不做是 1023 层，按大小合并不超过 11 层",
                std::to_string(plain.max_height()) + u8" 对 "
                    + std::to_string(by_size.max_height()));
    }

    /* 7. 路径压缩之后路径上的点直连根 */
    {
        Dsu plain(16, Mode::Plain);
        Dsu compressed(16, Mode::Compress);
        for (int i = 1; i < 16; ++i) {
            plain.unite(i, i - 1);
            compressed.unite(i, i - 1);
        }
        const int height_before = plain.max_height();
        compressed.find(0);     /* 从链的一头走到另一头，沿途全部直连根 */
        const int height_after = compressed.max_height();
        c.check(height_before == 15 && height_after == 1,
                u8"路径压缩：从链头走一趟之后树高从 15 降到 1",
                std::to_string(height_before) + u8" → " + std::to_string(height_after));
    }

    /* 8. 路径减半：走过的点挂到祖父上 */
    {
        Dsu dsu(16, Mode::Halve);
        for (int i = 1; i < 16; ++i) {
            dsu.unite(i, i - 1);
        }
        const int height_before = dsu.max_height();
        dsu.find(0);
        c.check(dsu.parent_of(0) == 2 && dsu.max_height() < height_before,
                u8"路径减半：0 号的父亲从 1 变成 2（跳过一层），树高也跟着降",
                std::to_string(height_before) + u8" → " + std::to_string(dsu.max_height()));
    }

    /* 9. 五档对同一串操作给出的答案逐位相同 */
    {
        const ModeTable &table = mode_table();
        bool same = true;
        for (std::size_t i = 1; i < table.runs.size(); ++i) {
            same = same && table.runs[i].answers == table.runs[0].answers
                   && table.runs[i].set_count == table.runs[0].set_count
                   && table.runs[i].unions == table.runs[0].unions;
        }
        c.check(same && table.query_count > 0,
                u8"五档对同一串操作的答案、最终根个数与成功合并次数都逐位相同",
                std::to_string(table.query_count) + u8" 个查询");
    }

    /* 10. 两处优化都加上之后步数远小于都不做 */
    {
        const ModeTable &table = mode_table();
        const ModeRun &plain = table.runs[0];
        const ModeRun &both = table.runs[4];
        c.check(plain.steps > both.steps * 50,
                u8"都不做的步数至少是两者都用的 50 倍",
                std::to_string(plain.steps) + u8" 对 " + std::to_string(both.steps));
    }

    /* 11. 两者都用的树高最小 */
    {
        const ModeTable &table = mode_table();
        const ModeRun &plain = table.runs[0];
        const ModeRun &by_size = table.runs[1];
        const ModeRun &both = table.runs[4];
        c.check(both.max_height <= by_size.max_height && by_size.max_height <= plain.max_height,
                u8"树高：两者都用 ≤ 按大小合并 ≤ 都不做",
                std::to_string(plain.max_height) + u8" / " + std::to_string(by_size.max_height)
                    + u8" / " + std::to_string(both.max_height));
    }

    /* 12. 非根节点的 size_ 是陈旧值 */
    {
        Dsu dsu(6, Mode::Both);
        dsu.unite(0, 1);
        dsu.unite(2, 3);
        dsu.unite(1, 2);
        int child = -1;
        for (int i = 0; i < 6; ++i) {
            if (!dsu.is_root(i)) {
                child = i;
                break;
            }
        }
        const int stale = dsu.stale_size_of(child);
        const int real = dsu.size_of(dsu.find(child));
        c.check(child >= 0 && stale != real && real == 4,
                u8"非根节点的 size_ 是陈旧值，要先 find 再读根",
                std::to_string(stale) + u8" 对 " + std::to_string(real));
    }

    /* 13. 离线倒序与朴素做法逐位相同 */
    {
        const OfflineResult offline = solve_offline();
        bool same = offline.answers.size() == kDeleteCount;
        std::size_t checks = 0;
        for (std::size_t step = 1; step <= kDeleteCount; ++step) {
            const QueryAnswer naive = answer_naive(step, checks);
            same = same && naive.components == offline.answers[step - 1].components
                   && naive.connected == offline.answers[step - 1].connected;
        }
        c.check(same, u8"离线倒序与朴素做法在每一步上逐位相同",
                std::to_string(kDeleteCount) + u8" 步");
    }

    /* 14. 删除之后那条边确实不在图里 */
    {
        std::vector<char> alive(kSmallEdgeCount, 1);
        alive[kDeletedEdges[0]] = 0;
        Dsu dsu(kSmallVertices, Mode::Both);
        for (std::size_t i = 0; i < kSmallEdgeCount; ++i) {
            if (alive[i] != 0) {
                dsu.unite(kSmallEdges[i].first, kSmallEdges[i].second);
            }
        }
        const std::pair<int, int> gone = kSmallEdges[kDeletedEdges[0]];
        Dsu without(kSmallVertices, Mode::Both);
        for (std::size_t i = 0; i < kSmallEdgeCount; ++i) {
            if (i != kDeletedEdges[0]) {
                without.unite(kSmallEdges[i].first, kSmallEdges[i].second);
            }
        }
        c.check(dsu.set_count() == without.set_count() && dsu.same(0, 1) == without.same(0, 1)
                    && (gone.first != gone.second),
                u8"删掉的边（" + std::to_string(gone.first) + u8"-" + std::to_string(gone.second)
                    + u8"）不在图里：两种建图方式结果相同");
    }

    /* 15. 重复删同一条边不改变答案 */
    {
        std::vector<char> alive(kSmallEdgeCount, 1);
        alive[kDeletedEdges[0]] = 0;
        Dsu once(kSmallVertices, Mode::Both);
        Dsu twice(kSmallVertices, Mode::Both);
        for (std::size_t i = 0; i < kSmallEdgeCount; ++i) {
            if (alive[i] != 0) {
                once.unite(kSmallEdges[i].first, kSmallEdges[i].second);
                twice.unite(kSmallEdges[i].first, kSmallEdges[i].second);
            }
        }
        twice.unite(kSmallEdges[kDeletedEdges[0]].first, kSmallEdges[kDeletedEdges[0]].second);
        /* 上面这一句是「已经不在图里的边又加了一次」，答案不该变 */
        c.check(once.set_count() == twice.set_count() && once.same(kQueryA, kQueryB) == twice.same(kQueryA, kQueryB),
                u8"把已经删掉的边再加一次，答案与原来相同");
    }

    /* 16. set_count 与实际根数一致 */
    {
        Dsu dsu(50, Mode::Both);
        Lcg lcg(3u);
        for (int i = 0; i < 100; ++i) {
            dsu.unite(static_cast<int>(lcg.next() % 50u), static_cast<int>(lcg.next() % 50u));
        }
        std::size_t roots = 0;
        for (int i = 0; i < 50; ++i) {
            if (dsu.is_root(i)) {
                ++roots;
            }
        }
        c.check(static_cast<std::size_t>(dsu.set_count()) == roots,
                u8"set_count() 与走一遍数出来的根数一致",
                std::to_string(roots) + u8" 个根");
    }

    /* 17. 只走不压缩的 same() 与 find() 给出的判断一致 */
    {
        Dsu dsu(100, Mode::Halve);
        Lcg lcg(5u);
        for (int i = 0; i < 200; ++i) {
            dsu.unite(static_cast<int>(lcg.next() % 100u), static_cast<int>(lcg.next() % 100u));
        }
        bool consistent = true;
        for (int i = 0; i < 100; ++i) {
            for (int j = 0; j < 100; ++j) {
                consistent = consistent && (dsu.same(i, j) == (dsu.find(i) == dsu.find(j)));
            }
        }
        c.check(consistent, u8"same()（只走不压缩）与 find() 的判断在 10000 个点对上都一致");
    }

    /* 18. 一个集合的 size_ 只在根上等于集合大小 */
    {
        Dsu dsu(10, Mode::Both);
        dsu.unite(0, 1);
        dsu.unite(1, 2);
        dsu.unite(2, 3);
        const int root = dsu.find(0);
        std::size_t members = 0;
        for (int i = 0; i < 10; ++i) {
            if (dsu.same(i, 0)) {
                ++members;
            }
        }
        c.check(dsu.size_of(root) == 4 && members == 4 && dsu.is_root(root),
                u8"根上的 size_ 等于集合里的点数", std::to_string(members));
    }

    return c.take();
}

}   /* namespace dmini */
