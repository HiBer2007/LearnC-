/* main_cli.cpp —— 练习模板 03 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 dsh::HashTable，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 include/hash_table.hpp 里。
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
#include <sstream>
#include <string>
#include <vector>

#include "hash_table.hpp"

namespace {

void line(const char *label, const std::string &text)
{
    std::cout << std::left << std::setw(24) << label << ": " << text << "\n";
}

void line(const char *label, long value)
{
    std::cout << std::left << std::setw(24) << label << ": " << value << "\n";
}

std::string fmt(double v)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(4) << v;
    return os.str();
}

std::string check_of(const char *problem)
{
    return problem == nullptr ? "ok" : problem;
}

/* 桶数序列：太长就只打印前 8 个 */
std::string sequence_text(const std::vector<std::size_t> &seq)
{
    std::string text = "[";
    for (std::size_t i = 0; i < seq.size() && i < 8; ++i) {
        text += (i == 0 ? "" : " ");
        text += std::to_string(seq[i]);
    }
    if (seq.size() > 8) {
        text += " ...";
    }
    text += "]";
    return text;
}

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: probe, three states ===\n";
    {
        dsh::HashTable<int, int> t;
        int inserted = 0;
        for (int i = 0; i < 5; ++i) {
            if (t.insert(i * 10, i * 100)) {
                ++inserted;
            }
        }
        int value = 0;
        const bool hit = t.find(30, value);
        const bool miss = t.find(999, value);

        line("inserted", inserted);
        line("check", check_of(t.check()));
        line("buckets", static_cast<long>(t.bucket_count()));
        line("find 30", hit ? ("hit, value " + std::to_string(value)) : std::string("miss"));
        line("find 999", miss ? "hit" : "miss");

        dsh::HashTable<std::string, int> s;
        s.insert("alpha", 7);
        int svalue = 0;
        const bool shit = s.find("alpha", svalue);
        line("string key \"alpha\"", shit ? ("hit, value " + std::to_string(svalue))
                                          : std::string("miss"));
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: rehash rebuilds the table ===\n";
    {
        dsh::HashTable<int, int> t;
        for (int i = 0; i < 5; ++i) {
            t.insert(i * 10, i * 100);
        }
        line("before rehash, buckets", static_cast<long>(t.bucket_count()));
        line("before rehash, size", static_cast<long>(t.size()));

        t.rehash(16);

        line("after rehash, buckets", static_cast<long>(t.bucket_count()));
        line("after rehash, size", static_cast<long>(t.size()));
        line("after rehash, check", check_of(t.check()));

        int found = 0;
        for (int i = 0; i < 5; ++i) {
            int value = 0;
            if (t.find(i * 10, value) && value == i * 100) {
                ++found;
            }
        }
        line("keys found after rehash", found);
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: erase leaves a tombstone ===\n";
    {
        /* 找两个落在同一个桶上的键：后一个的探测要经过前一个的槽位 */
        int first_in_bucket[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        int early = 0;
        int late = 0;
        for (int x = 1; x < 1000 && late == 0; ++x) {
            const int bucket = static_cast<int>(dsh::DefaultHash<int>{}(x) % 8);
            if (first_in_bucket[bucket] == 0) {
                first_in_bucket[bucket] = x;
            } else {
                early = first_in_bucket[bucket];
                late = x;
            }
        }

        dsh::HashTable<int, int> t;
        t.insert(early, early * 100);
        t.insert(late, late * 100);

        line("two keys in one bucket", std::to_string(early) + " and " + std::to_string(late));
        const bool removed = t.erase(early);
        line("erase the earlier key", removed ? 1 : 0);
        line("after erase, size", static_cast<long>(t.size()));
        line("after erase, tombstones", static_cast<long>(t.tombstones()));
        line("after erase, check", check_of(t.check()));

        int value = 0;
        const bool still = t.find(late, value);
        line("find the later key", still ? ("hit, value " + std::to_string(value))
                                        : std::string("miss"));
        line("erase it a second time", t.erase(early) ? 1 : 0);

        t.rehash(16);
        line("tombstones after rehash", static_cast<long>(t.tombstones()));
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: when to add buckets ===\n";
    {
        dsh::HashTable<int, int> t;
        std::vector<std::size_t> seq;
        seq.push_back(t.bucket_count());
        std::size_t last = t.bucket_count();
        int inserted = 0;
        for (int i = 0; i < 200; ++i) {
            if (t.insert(i * 3 + 1, i)) {
                ++inserted;
            }
            if (t.bucket_count() != last) {
                last = t.bucket_count();
                seq.push_back(last);
            }
        }

        line("inserted", inserted);
        line("bucket sequence", sequence_text(seq));
        line("bucket changes", static_cast<long>(seq.size()) - 1);
        line("buckets", static_cast<long>(t.bucket_count()));
        line("load factor", fmt(t.load_factor()));
        line("check", check_of(t.check()));

        /* 同一批键、同一批查找，只换哈希函数 */
        dsh::HashTable<int, int> good;
        dsh::HashTable<int, int, dsh::AllZeroHash<int> > bad;
        for (int i = 0; i < 200; ++i) {
            good.insert(i * 3 + 1, i);
            bad.insert(i * 3 + 1, i);
        }

        good.reset_probe_steps();
        int good_hits = 0;
        for (int i = 0; i < 200; ++i) {
            int value = 0;
            if (good.find(i * 3 + 1, value)) {
                ++good_hits;
            }
        }
        const long good_steps = good.probe_steps();

        bad.reset_probe_steps();
        int bad_hits = 0;
        for (int i = 0; i < 200; ++i) {
            int value = 0;
            if (bad.find(i * 3 + 1, value)) {
                ++bad_hits;
            }
        }
        const long bad_steps = bad.probe_steps();

        line("200 lookups, default hash", std::to_string(good_hits) + " hits, " +
                                             std::to_string(good_steps) + " probe steps");
        line("200 lookups, all-zero hash", std::to_string(bad_hits) + " hits, " +
                                              std::to_string(bad_steps) + " probe steps");
        line("longest cluster, default", static_cast<long>(good.longest_cluster()));
        line("longest cluster, all-zero", static_cast<long>(bad.longest_cluster()));
    }

    return 0;
}
