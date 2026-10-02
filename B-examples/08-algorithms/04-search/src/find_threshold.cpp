/**
 * find_threshold.cpp —— 正文第 4.1.2 小节的对照程序：三种摆法在十种规模上的代价
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

/* find_threshold.cpp    编译：g++ -std=c++17 -O2 find_threshold.cpp -o find_threshold
 * 对照构建：g++ -std=c++17 -O0 find_threshold.cpp -o find_threshold_O0
 * 同一个「某数在不在」的需求，三种摆法在十种规模上的查询代价。
 * 轮数按规模反比设置：规模越大问的次数越少，让每种摆法的总工作量大致固定，
 * 否则小规模那一头会贴着时钟分辨率、大规模那一头要跑很久。 */
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <unordered_map>
#include <vector>

static std::uint64_t g_sink = 0;   // 结果落到这里，防止优化器把整段查询删掉

template <class F>
static double time_ms(F&& f) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// 线性扫描：带比较计数，返回下标，找不到返回 n
static long long linear_scan(const int* a, int n, int key, long long& cmp) {
    for (int i = 0; i < n; ++i) {
        ++cmp;
        if (a[i] == key) return i;
    }
    return n;
}

// 二分：带比较计数，返回下标，找不到返回 -1
static long long binary_probe(const int* a, int n, int key, long long& cmp) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        const int mid = lo + (hi - lo) / 2;
        ++cmp;
        if (a[mid] == key) return mid;
        if (a[mid] < key) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

int main() {
    const int sizes[] = {4, 8, 16, 32, 64, 128, 256, 1024, 65536, 1048576};
    const long long work = 4000000;      // 每种摆法每档的目标「基本操作」数
    const long long q_hash = 2000000;    // 哈希那一档每次都问满
    std::mt19937 rng(20261003);
    int bits = 0;
    // 每一档实际用到的查询次数，收尾那行报告的是这里的取值
    long long q_lin_lo = 0, q_lin_hi = 0, q_bin_lo = 0, q_bin_hi = 0;
    bool first_row = true;

    std::printf("%9s %9s %11s %11s %11s %11s %11s\n",
                "n", "每查询比较", "每查询比较", "线性", "二分", "哈希", "命中计数");
    std::printf("%9s %11s %11s %11s %11s %11s\n",
                "", "（线性）", "（二分）", "ns/查询", "ns/查询", "ns/查询");

    for (int n : sizes) {
        std::vector<int> keys(n);
        for (int i = 0; i < n; ++i) keys[i] = i * 2;      // 递增，天然有序
        std::vector<int> unsorted = keys;
        std::shuffle(unsorted.begin(), unsorted.end(), rng);

        std::unordered_map<int, int> umap;
        umap.reserve(static_cast<std::size_t>(n) * 2);
        for (int k : keys) umap[k] = k;

        bits = 1;
        while ((1 << bits) < n) ++bits;                   // 二分最多比较 bits 次

        // 三种摆法问的是同一批问题；次数按各自每一步的代价反比设置
        const long long q_lin = std::min<long long>(1000000, std::max<long long>(200, work / n));
        const long long q_bin = std::min<long long>(2000000, std::max<long long>(2000, work / bits));
        if (first_row) {
            q_lin_lo = q_lin_hi = q_lin;
            q_bin_lo = q_bin_hi = q_bin;
            first_row = false;
        }
        q_lin_lo = std::min(q_lin_lo, q_lin);
        q_lin_hi = std::max(q_lin_hi, q_lin);
        q_bin_lo = std::min(q_bin_lo, q_bin);
        q_bin_hi = std::max(q_bin_hi, q_bin);

        std::vector<int> qs(std::max(q_lin, std::max(q_bin, q_hash)));
        for (int& q : qs) q = static_cast<int>(rng() % (static_cast<unsigned>(n) * 2));

        long long cmp_lin = 0, cmp_bin = 0;
        const double t_lin = time_ms([&] {
            long long hits = 0;
            for (long long i = 0; i < q_lin; ++i)
                hits += linear_scan(unsorted.data(), n, qs[static_cast<std::size_t>(i)], cmp_lin) != n;
            g_sink += static_cast<std::uint64_t>(hits);
        });
        const double t_bin = time_ms([&] {
            long long hits = 0;
            for (long long i = 0; i < q_bin; ++i)
                hits += binary_probe(keys.data(), n, qs[static_cast<std::size_t>(i)], cmp_bin) >= 0;
            g_sink += static_cast<std::uint64_t>(hits);
        });
        const double t_hash = time_ms([&] {
            long long hits = 0;
            for (long long i = 0; i < q_hash; ++i)
                hits += umap.find(qs[static_cast<std::size_t>(i)]) != umap.end();
            g_sink += static_cast<std::uint64_t>(hits);
        });

        std::printf("%9d %11.2f %11.2f %11.3f %11.3f %11.3f %11lld\n",
                    n,
                    static_cast<double>(cmp_lin) / static_cast<double>(q_lin),
                    static_cast<double>(cmp_bin) / static_cast<double>(q_bin),
                    t_lin * 1e6 / static_cast<double>(q_lin),
                    t_bin * 1e6 / static_cast<double>(q_bin),
                    t_hash * 1e6 / static_cast<double>(q_hash),
                    static_cast<long long>(g_sink));
    }
    std::printf("查询总数（三种摆法各自）：线性 %lld 到 %lld，二分 %lld 到 %lld，哈希每档 %lld\n",
                q_lin_lo, q_lin_hi, q_bin_lo, q_bin_hi, q_hash);
    return 0;
}
