/* main_cli.cpp —— 练习模板 12 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 rnd 里的函数，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 src/sampling.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 * 前三阶段的随机数都来自固定种子，重跑逐位相同；阶段 4 的
 * 「不设种子」那两行是唯一的例外，它专门用来说明没有固定种子时会怎样。
 */
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "sampling.hpp"

namespace {

/* 标签列的宽度：本模板所有「标签 : 值」的行都用这一个宽度 */
const int kLabelWidth = 32;

void line(const char *label, long long value)
{
    std::cout << std::left << std::setw(kLabelWidth) << label << ": " << value << "\n";
}

void line(const char *label, const std::string &text)
{
    std::cout << std::left << std::setw(kLabelWidth) << label << ": " << text << "\n";
}

std::string yes_no(bool v)
{
    return v ? "yes" : "no";
}

long long sum_counts(const std::vector<long long> &v)
{
    long long s = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        s += v[i];
    }
    return s;
}

/* 位置矩阵：行是元素，列是位置 */
void print_matrix(const std::vector<std::vector<long long>> &m)
{
    std::cout << std::left << std::setw(12) << "elem \\ pos";
    for (std::size_t p = 0; p < m.size(); ++p) {
        std::cout << std::right << std::setw(8) << p;
    }
    std::cout << "\n";

    for (std::size_t e = 0; e < m.size(); ++e) {
        std::cout << std::left << std::setw(12) << ("elem " + std::to_string(e));
        for (std::size_t p = 0; p < m[e].size(); ++p) {
            std::cout << std::right << std::setw(8) << m[e][p];
        }
        std::cout << "\n";
    }
}

/* ---------------- 阶段 1 的固定数据 ---------------- */

/* 24 KiB 的缓冲区，按 4 KiB 一页分成 6 页。
 * 这个长度不是随手挑的：rand() 的取值一共 RAND_MAX + 1 个，
 * 24576 正好是它的四分之三，于是头 8192 个余数（也就是头两页）
 * 会比其余部分多算一次。偏差因此从「千分之几」放大到整整一倍。 */
const int kBufferBytes = 24576;
const int kPageBytes = 4096;
const int kPages = kBufferBytes / kPageBytes;
const long long kDraws = 480000;

void stage1()
{
    std::cout << "=== Stage 1: a uniform integer in a range ===\n";

    line("seed", static_cast<long long>(rnd::kSeed));
    line("RAND_MAX", static_cast<long long>(RAND_MAX));
    line("buffer bytes", kBufferBytes);
    line("page bytes", kPageBytes);
    line("pages", kPages);
    line("draws per histogram", kDraws);
    line("expected per page if uniform", kDraws / kPages);

    std::mt19937 engine = rnd::make_engine();
    const rnd::Histogram u = rnd::histogram_uniform(engine, kBufferBytes, kPages, kDraws);
    const rnd::Histogram r = rnd::histogram_rand(kBufferBytes, kPages, kDraws);

    std::cout << "--- draws per page, side by side ---\n";
    std::cout << std::left << std::setw(6) << "page"
              << std::right << std::setw(16) << "byte range"
              << std::setw(14) << "uniform_int"
              << std::setw(14) << "rand_percent" << "\n";
    for (int b = 0; b < kPages; ++b) {
        const int lo = b * kPageBytes;
        const int hi = lo + kPageBytes - 1;
        std::cout << std::left << std::setw(6) << b
                  << std::right << std::setw(7) << lo << ".." << std::setw(7) << hi
                  << std::setw(14) << u.counts[static_cast<std::size_t>(b)]
                  << std::setw(14) << r.counts[static_cast<std::size_t>(b)] << "\n";
    }

    std::cout << "--- max abs deviation from the uniform expectation ("
              << (kDraws / kPages) << ") ---\n";
    line("uniform_int", rnd::max_abs_deviation(u.counts, kDraws / kPages));
    line("rand_percent", rnd::max_abs_deviation(r.counts, kDraws / kPages));

    line("uniform_int, histogram total", sum_counts(u.counts));
    line("rand_percent, histogram total", sum_counts(r.counts));
    line("uniform_int, out of range", u.out_of_range);
    line("rand_percent, out of range", r.out_of_range);
}

/* ---------------- 阶段 2 的固定数据 ---------------- */

const int kDeckSize = 6;
const long long kMatrixRounds = 240000;

void stage2()
{
    std::cout << "\n=== Stage 2: Fisher-Yates, the swap ===\n";

    line("deck size", kDeckSize);
    line("deck, sorted", rnd::join_ints(rnd::make_deck(kDeckSize)));

    std::mt19937 engine = rnd::make_engine();
    bool all_perm = true;
    for (int k = 1; k <= 3; ++k) {
        std::vector<int> deck = rnd::make_deck(kDeckSize);
        rnd::shuffle(deck, engine);
        all_perm = all_perm && rnd::is_permutation_of_deck(deck);
        line(("shuffle " + std::to_string(k)).c_str(), rnd::join_ints(deck));
    }
    line("each shuffle is a permutation", yes_no(all_perm));

    line("rounds per matrix", kMatrixRounds);
    line("expected per cell", kMatrixRounds / kDeckSize);

    const rnd::PositionMatrix mc = rnd::count_positions_correct(kDeckSize, kMatrixRounds);
    const rnd::PositionMatrix mw = rnd::count_positions_wrong(kDeckSize, kMatrixRounds);

    std::cout << "--- correct swap: element (row) x position (column) ---\n";
    print_matrix(mc.cells);

    std::cout << "--- wrong swap, whole-range partner: same rounds ---\n";
    print_matrix(mw.cells);

    line("correct, max abs deviation", rnd::max_abs_deviation(rnd::flatten_counts(mc.cells), kMatrixRounds / kDeckSize));
    line("wrong, max abs deviation", rnd::max_abs_deviation(rnd::flatten_counts(mw.cells), kMatrixRounds / kDeckSize));
    line("correct, out of range values", mc.bad_values);
    line("wrong, out of range values", mw.bad_values);
}

/* ---------------- 阶段 3 的固定数据 ---------------- */

const int kStreamSize = 16;
const std::size_t kReservoirSize = 4;
const long long kSampleRounds = 60000;

void stage3()
{
    std::cout << "\n=== Stage 3: the reservoir replacement rule ===\n";

    line("stream elements", kStreamSize);
    line("reservoir size k", static_cast<long long>(kReservoirSize));

    /* 三次抽样共用一台引擎：同一个种子的第一抽固定不变，
     * 而引擎一直往下走，三次拿到的是三个不同的样本。 */
    const std::vector<int> stream = rnd::make_stream(kStreamSize);
    std::mt19937 engine = rnd::make_engine();
    for (int run = 1; run <= 3; ++run) {
        const std::vector<int> pool = rnd::reservoir_sample(stream, kReservoirSize, engine);
        line(("sample run " + std::to_string(run) + ", sorted").c_str(),
             rnd::join_ints(rnd::sorted_copy(pool)));
        line(("sample run " + std::to_string(run) + ", pool size").c_str(),
             static_cast<long long>(pool.size()));
    }

    const rnd::SampleStats st = rnd::count_selected(kStreamSize, kReservoirSize, kSampleRounds);

    line("rounds", st.rounds);
    line("expected hits per element", st.rounds * static_cast<long long>(kReservoirSize) / kStreamSize);

    std::cout << "--- times each element was selected (" << kStreamSize << " elements, k = "
              << kReservoirSize << ", " << st.rounds << " rounds) ---\n";
    std::cout << std::left << std::setw(8) << "element";
    for (int e = 0; e < kStreamSize; ++e) {
        std::cout << std::right << std::setw(6) << e;
    }
    std::cout << "\n";
    std::cout << std::left << std::setw(8) << "hits";
    for (int e = 0; e < kStreamSize; ++e) {
        std::cout << std::right << std::setw(6) << st.hits[static_cast<std::size_t>(e)];
    }
    std::cout << "\n";

    long long lo = st.hits.empty() ? 0 : st.hits[0];
    long long hi = lo;
    for (std::size_t i = 1; i < st.hits.size(); ++i) {
        if (st.hits[i] < lo) {
            lo = st.hits[i];
        }
        if (st.hits[i] > hi) {
            hi = st.hits[i];
        }
    }

    line("hits total", sum_counts(st.hits));
    line("hits min", lo);
    line("hits max", hi);
    line("hits, max abs deviation",
         rnd::max_abs_deviation(st.hits, st.rounds * static_cast<long long>(kReservoirSize) / kStreamSize));
    line(("runs with pool size != " + std::to_string(st.expected_size)).c_str(), st.bad_size);
    line("runs with duplicate elements", st.duplicates);
}

/* ---------------- 阶段 4 ---------------- */

void stage4()
{
    std::cout << "\n=== Stage 4: a fixed seed, and what it buys ===\n";

    line("seed", static_cast<long long>(rnd::kSeed));

    /* 两台独立的引擎，同一个种子，跑同一批操作 */
    std::mt19937 a = rnd::make_engine();
    std::mt19937 b = rnd::make_engine();
    const std::vector<std::uint32_t> da = rnd::raw_draws(a, 6);
    const std::vector<std::uint32_t> db = rnd::raw_draws(b, 6);
    line("engine A, 6 raw draws", rnd::join_u32(da));
    line("engine B, 6 raw draws", rnd::join_u32(db));
    line("engine A == engine B", yes_no(rnd::same_u32(da, db)));

    /* 再各取一千个：上面那个「相同」不是前几个数的巧合 */
    const std::vector<std::uint32_t> da2 = rnd::raw_draws(a, 1000);
    const std::vector<std::uint32_t> db2 = rnd::raw_draws(b, 1000);
    line("engine A == engine B, 1000 more", yes_no(rnd::same_u32(da2, db2)));

    /* 对照：种子来自 random_device，两次构造给出两串不同的数 */
    std::mt19937 ua = rnd::make_unseeded_engine();
    std::mt19937 ub = rnd::make_unseeded_engine();
    const std::vector<std::uint32_t> ua6 = rnd::raw_draws(ua, 6);
    const std::vector<std::uint32_t> ub6 = rnd::raw_draws(ub, 6);
    line("unseeded A, 6 raw draws", rnd::join_u32(ua6));
    line("unseeded B, 6 raw draws", rnd::join_u32(ub6));
    line("unseeded A == unseeded B", yes_no(rnd::same_u32(ua6, ub6)));
}

} /* namespace */

int main()
{
    stage1();
    stage2();
    stage3();
    stage4();
    return 0;
}
