/**
 * hash_mini.cpp —— 两版哈希表的实现、项目输出与自测
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
 * 报告里的数字全是计数器：比较次数、走过的槽数、再哈希次数、墓碑数量。
 * 计时一个都不做——结构量重跑逐位相同，计时量做不到。
 */
#include "hash_mini.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace hmini {

void HashStats::reset()
{
    *this = HashStats();
}

HashStats &stats()
{
    static HashStats instance;
    return instance;
}

/* ================= 哈希函数 ================= */

const char *hash_kind_name(HashKind kind)
{
    switch (kind) {
    case HashKind::Mix:
        return u8"混合（好）";
    case HashKind::LowByte:
        return u8"只看低 8 位";
    case HashKind::Constant:
        return u8"永远返回 0";
    }
    return u8"未知";
}

std::size_t hash_key(int key, HashKind kind)
{
    const std::uint64_t value = static_cast<std::uint64_t>(static_cast<std::uint32_t>(key));
    switch (kind) {
    case HashKind::Mix: {
        /* 一个小的混合函数：乘法与移位交替，让键的每一位都影响低位。
           哈希表取桶号时用的是低位，因此低位必须混合得开。 */
        std::uint64_t x = value + 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        x = x ^ (x >> 31);
        return static_cast<std::size_t>(x);
    }
    case HashKind::LowByte:
        return static_cast<std::size_t>(value & 0xFFu);
    case HashKind::Constant:
        return 0;
    }
    return 0;
}

bool is_prime(std::size_t n)
{
    if (n < 2) {
        return false;
    }
    if (n % 2 == 0) {
        return n == 2;
    }
    for (std::size_t d = 3; d * d <= n; d += 2) {
        if (n % d == 0) {
            return false;
        }
    }
    return true;
}

std::size_t next_prime_at_least(std::size_t n)
{
    std::size_t candidate = n < 2 ? 2 : n;
    if (candidate % 2 == 0) {
        ++candidate;
    }
    while (!is_prime(candidate)) {
        candidate += 2;
    }
    return candidate;
}

/* ================= 链地址法 ================= */

ChainedHash::ChainedHash(HashKind kind, double max_load)
    : max_load_(max_load), kind_(kind)
{
    buckets_.assign(1, nullptr);    /* 空表的桶数记 1，第一次插入时再长 */
}

ChainedHash::~ChainedHash()
{
    for (Node *head : buckets_) {
        destroy_chain(head);
    }
}

void ChainedHash::destroy_chain(Node *head)
{
    while (head != nullptr) {
        Node *next = head->next;    /* 先记住下一个，再删当前 */
        delete head;
        head = next;
    }
}

double ChainedHash::load_factor() const noexcept
{
    return static_cast<double>(size_) / static_cast<double>(bucket_count_);
}

std::size_t ChainedHash::longest_bucket() const
{
    std::size_t longest = 0;
    for (Node *head : buckets_) {
        std::size_t length = 0;
        for (Node *p = head; p != nullptr; p = p->next) {
            ++length;
        }
        if (length > longest) {
            longest = length;
        }
    }
    return longest;
}

std::size_t ChainedHash::used_buckets() const
{
    std::size_t used = 0;
    for (Node *head : buckets_) {
        if (head != nullptr) {
            ++used;
        }
    }
    return used;
}

void ChainedHash::rehash(std::size_t new_count)
{
    std::vector<Node *> fresh(new_count, nullptr);
    for (Node *head : buckets_) {
        while (head != nullptr) {
            Node *next = head->next;                            /* 先记住，再挪 */
            const std::size_t index = hash_key(head->key, kind_) % new_count;
            head->next = fresh[index];                          /* 头插进新桶 */
            fresh[index] = head;
            head = next;
            ++stats().rehash_moves;
        }
    }
    buckets_.swap(fresh);        /* 旧桶数组在这里析构，节点还活着 */
    bucket_count_ = new_count;
    ++stats().rehashes;
}

bool ChainedHash::insert(int key)
{
    /* 先判断「插进去会不会超过上限」，超了先把桶数长起来再插 */
    if (size_ + 1 > static_cast<std::size_t>(static_cast<double>(bucket_count_) * max_load_)) {
        const std::size_t target = bucket_count_ == 1
                                       ? next_prime_at_least(13)
                                       : next_prime_at_least(bucket_count_ * 2);
        rehash(target);
    }

    const std::size_t index = hash_key(key, kind_) % bucket_count_;
    for (Node *p = buckets_[index]; p != nullptr; p = p->next) {
        ++stats().steps;
        ++stats().comparisons;
        if (p->key == key) {
            return false;               /* 键已经在表里 */
        }
    }
    Node *fresh = new Node{key, buckets_[index]};
    buckets_[index] = fresh;
    ++size_;
    return true;
}

bool ChainedHash::contains(int key) const
{
    const std::size_t index = hash_key(key, kind_) % bucket_count_;
    for (Node *p = buckets_[index]; p != nullptr; p = p->next) {
        ++stats().steps;
        ++stats().comparisons;
        if (p->key == key) {
            return true;
        }
    }
    return false;
}

bool ChainedHash::erase(int key)
{
    const std::size_t index = hash_key(key, kind_) % bucket_count_;
    Node **link = &buckets_[index];     /* 指向「指向当前节点的那个指针」 */
    while (*link != nullptr) {
        ++stats().steps;
        ++stats().comparisons;
        if ((*link)->key == key) {
            Node *dead = *link;
            *link = dead->next;         /* 前驱跳过它；头节点与中间节点同一段代码 */
            delete dead;
            --size_;
            return true;
        }
        link = &(*link)->next;
    }
    return false;
}

/* ================= 开放寻址 ================= */

OpenHash::OpenHash(HashKind kind, double max_load)
    : max_load_(max_load), kind_(kind)
{
    slots_.assign(8, Slot{});           /* 容量从 8 起步 */
}

double OpenHash::load_factor() const noexcept
{
    return static_cast<double>(size_) / static_cast<double>(slots_.size());
}

bool OpenHash::find_slot(int key, std::size_t &index) const
{
    const std::size_t capacity = slots_.size();
    std::size_t probe = hash_key(key, kind_) % capacity;
    for (std::size_t visited = 0; visited < capacity; ++visited) {
        const Slot &slot = slots_[probe];
        ++stats().steps;
        if (slot.state == State::Empty) {
            return false;               /* 空槽：后面不可能有，停下 */
        }
        if (slot.state == State::Occupied) {
            ++stats().comparisons;
            if (slot.key == key) {
                index = probe;
                return true;
            }
        }
        /* 墓碑：曾经占用、现已删除，继续往后找 */
        probe = (probe + 1) % capacity;
    }
    return false;
}

bool OpenHash::contains(int key) const
{
    std::size_t index = 0;
    return find_slot(key, index);
}

void OpenHash::insert_into(std::vector<Slot> &slots, int key) const
{
    const std::size_t capacity = slots.size();
    std::size_t probe = hash_key(key, kind_) % capacity;
    while (slots[probe].state == State::Occupied) {
        probe = (probe + 1) % capacity;
    }
    slots[probe].key = key;
    slots[probe].state = State::Occupied;
}

void OpenHash::grow(std::size_t new_capacity)
{
    std::vector<Slot> fresh(new_capacity, Slot{});
    for (const Slot &slot : slots_) {
        if (slot.state == State::Occupied) {
            insert_into(fresh, slot.key);
            ++stats().rehash_moves;
        } else if (slot.state == State::Tombstone) {
            ++stats().tombstone_clears;
        }
    }
    slots_.swap(fresh);                 /* 旧槽数组在这里析构，墓碑一并消失 */
    tombstones_ = 0;
    ++stats().rehashes;
}

bool OpenHash::insert(int key)
{
    /* 墓碑也占位置，因此判断上限时要把它们算进来 */
    const std::size_t occupied = size_ + tombstones_ + 1;
    if (static_cast<double>(occupied) > static_cast<double>(slots_.size()) * max_load_) {
        std::size_t target = slots_.size();
        do {
            target *= 2;
        } while (static_cast<double>(size_ + 1) > static_cast<double>(target) * max_load_);
        grow(target);                   /* 换新表时墓碑一并消失 */
    }

    const std::size_t capacity = slots_.size();
    const std::size_t start = hash_key(key, kind_) % capacity;
    std::size_t first_tombstone = capacity;     /* capacity 表示「还没有遇到墓碑」 */

    for (std::size_t visited = 0; visited < capacity; ++visited) {
        const std::size_t probe = (start + visited) % capacity;
        Slot &slot = slots_[probe];
        ++stats().steps;
        if (slot.state == State::Occupied) {
            ++stats().comparisons;
            if (slot.key == key) {
                return false;           /* 键已经在表里 */
            }
            continue;
        }
        if (slot.state == State::Tombstone) {
            if (first_tombstone == capacity) {
                first_tombstone = probe;    /* 记下第一个墓碑，但还要继续确认键不在后面 */
            }
            continue;
        }
        /* 空槽：键确实不在表里，可以落在第一个墓碑上 */
        const std::size_t target = first_tombstone == capacity ? probe : first_tombstone;
        slots_[target].key = key;
        slots_[target].state = State::Occupied;
        ++size_;
        if (target != probe) {
            --tombstones_;              /* 复用一个墓碑位 */
        }
        return true;
    }
    return false;                       /* 表满，正常路径下不会走到 */
}

bool OpenHash::erase(int key)
{
    std::size_t index = 0;
    if (!find_slot(key, index)) {
        return false;
    }
    slots_[index].state = State::Tombstone;     /* 留墓碑，不能清空 */
    --size_;
    ++tombstones_;
    return true;
}

bool OpenHash::erase_clearing_slot(int key)
{
    std::size_t index = 0;
    if (!find_slot(key, index)) {
        return false;
    }
    slots_[index].state = State::Empty;         /* 故意写错：探测序列从这里断掉 */
    --size_;
    return true;
}

void OpenHash::rehash_clean()
{
    grow(slots_.size());
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

/** 文本占多少列：CJK 与全角标点算 2 列，其余算 1 列 */
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

std::string fixed(double value, int digits)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(digits) << value;
    return os.str();
}

std::string pad_left_fixed(double value, std::size_t width, int digits)
{
    return pad_left_text(fixed(value, digits), width);
}

/** 键都是 10 的倍数，这样「只看低位」的坏哈希才会现形 */
std::vector<int> make_keys(std::size_t count)
{
    std::vector<int> keys;
    keys.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        keys.push_back(static_cast<int>(i) * 10);
    }
    return keys;
}

constexpr std::size_t kKeyCount = 1000;

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

/* 一次「插入全部键 + 查找全部键」的账 */
struct LookupRun {
    std::size_t comparisons = 0;
    std::size_t steps = 0;
    std::size_t longest = 0;
    std::size_t bucket_count = 0;
    std::size_t capacity = 0;
    std::size_t hits = 0;
    std::size_t rehash_moves = 0;
    std::size_t rehashes = 0;
};

LookupRun run_chained(HashKind kind)
{
    const std::vector<int> keys = make_keys(kKeyCount);
    stats().reset();
    ChainedHash table(kind, 1.0);
    for (int key : keys) {
        table.insert(key);
    }
    const std::size_t rehash_moves = stats().rehash_moves;
    const std::size_t rehashes = stats().rehashes;

    stats().reset();
    std::size_t hits = 0;
    for (int key : keys) {
        if (table.contains(key)) {
            ++hits;
        }
    }

    LookupRun run;
    run.comparisons = stats().comparisons;
    run.steps = stats().steps;
    run.longest = table.longest_bucket();
    run.bucket_count = table.bucket_count();
    run.hits = hits;
    run.rehash_moves = rehash_moves;
    run.rehashes = rehashes;
    return run;
}

LookupRun run_open(HashKind kind)
{
    const std::vector<int> keys = make_keys(kKeyCount);
    stats().reset();
    OpenHash table(kind, 0.7);
    for (int key : keys) {
        table.insert(key);
    }
    const std::size_t rehash_moves = stats().rehash_moves;
    const std::size_t rehashes = stats().rehashes;

    stats().reset();
    std::size_t hits = 0;
    for (int key : keys) {
        if (table.contains(key)) {
            ++hits;
        }
    }

    LookupRun run;
    run.comparisons = stats().comparisons;
    run.steps = stats().steps;
    run.capacity = table.capacity();
    run.hits = hits;
    run.rehash_moves = rehash_moves;
    run.rehashes = rehashes;
    return run;
}

void append_growth_section(std::ostringstream &os)
{
    os << u8"链地址法：桶数怎么长（插入 " << kKeyCount << u8" 个键，键都是 10 的倍数）\n";
    os << u8"  插入第几个键时加桶  新桶数  加桶后的负载因子\n";

    stats().reset();
    ChainedHash table(HashKind::Mix, 1.0);
    std::size_t inserted = 0;
    std::size_t grown = 0;
    for (int key : make_keys(kKeyCount)) {
        ++inserted;
        const std::size_t before = table.bucket_count();
        table.insert(key);
        if (table.bucket_count() != before) {
            ++grown;
            os << "  " << pad_left(inserted, 18) << "  " << pad_left(table.bucket_count(), 6)
               << "  " << pad_left_fixed(table.load_factor(), 16, 2) << "\n";
        }
    }
    os << u8"  最终：" << table.size() << u8" 个键，" << table.bucket_count() << u8" 个桶，负载因子 "
       << fixed(table.load_factor(), 3) << u8"，用到的桶 " << table.used_buckets() << u8" 个\n";
    os << u8"  最长的一条链 " << table.longest_bucket() << u8" 个节点，加桶 " << grown
       << u8" 次，再哈希重新放过 " << stats().rehash_moves << u8" 个元素\n";
    os << u8"  每次加桶都把桶数往上取到下一个素数，因此桶数序列是 13 29 59 127 …\n";
}

void append_open_section(std::ostringstream &os)
{
    os << u8"\n开放寻址：线性探测、墓碑与重整（同样的 " << kKeyCount << u8" 个键）\n";

    stats().reset();
    OpenHash table(HashKind::Mix, 0.7);
    for (int key : make_keys(kKeyCount)) {
        table.insert(key);
    }
    os << u8"  插入 " << table.size() << u8" 个键：容量 " << table.capacity() << u8"，共探测 "
       << stats().steps << u8" 次，平均每次插入 " << fixed(static_cast<double>(stats().steps) / 1000.0, 2)
       << u8" 次\n";

    stats().reset();
    std::size_t hits = 0;
    for (int key : make_keys(kKeyCount)) {
        if (table.contains(key)) {
            ++hits;
        }
    }
    const std::size_t hit_probes = stats().steps;
    os << u8"  查找 " << kKeyCount << u8" 次（全部命中 " << hits << u8" 次）：共探测 "
       << hit_probes << u8" 次，平均 " << fixed(static_cast<double>(hit_probes) / 1000.0, 2)
       << u8" 次\n";

    /* 删掉最前面的 700 个键，全留成墓碑 */
    const std::size_t deleted_target = 700;
    std::size_t deleted = 0;
    for (std::size_t i = 0; i < deleted_target; ++i) {
        if (table.erase(static_cast<int>(i) * 10)) {
            ++deleted;
        }
    }

    stats().reset();
    std::size_t alive_hits = 0;
    std::size_t alive_count = 0;
    for (std::size_t i = deleted_target; i < kKeyCount; ++i) {
        ++alive_count;
        if (table.contains(static_cast<int>(i) * 10)) {
            ++alive_hits;
        }
    }
    const std::size_t probes_with_tombstones = stats().steps;

    stats().reset();
    for (std::size_t i = deleted_target; i < kKeyCount; ++i) {
        table.contains(static_cast<int>(i) * 10 + 5);    /* 一定不在表里 */
    }
    const std::size_t miss_with_tombstones = stats().steps;

    /* 下面这些数要在插新键之前记下来：插完就变了 */
    const std::size_t size_after_delete = table.size();
    const std::size_t tombstones_after_delete = table.tombstones();
    const std::size_t capacity_at_compare = table.capacity();

    /* 对照实验：两张表容量相同、内容相同，只有一张留着 700 个墓碑 */
    OpenHash clean_table(HashKind::Mix, 0.7);
    for (int key : make_keys(kKeyCount)) {
        clean_table.insert(key);
    }
    for (std::size_t i = 0; i < deleted_target; ++i) {
        clean_table.erase(static_cast<int>(i) * 10);
    }
    clean_table.rehash_clean();

    const std::size_t new_keys = 300;
    stats().reset();
    for (std::size_t i = 0; i < new_keys; ++i) {
        table.insert(20000 + static_cast<int>(i) * 10);
    }
    const std::size_t insert_with_tombstones = stats().steps;

    stats().reset();
    for (std::size_t i = 0; i < new_keys; ++i) {
        clean_table.insert(20000 + static_cast<int>(i) * 10);
    }
    const std::size_t insert_after_clean = stats().steps;

    os << u8"  删掉 " << deleted << u8" 个键之后：剩下 " << size_after_delete << u8" 个键，墓碑 "
       << tombstones_after_delete << u8" 个\n";
    os << u8"  再查那 " << alive_count << u8" 个还在的键（命中 " << alive_hits << u8" 次）：共探测 "
       << probes_with_tombstones << u8" 次，平均 "
       << fixed(static_cast<double>(probes_with_tombstones)
                / static_cast<double>(alive_count), 2) << u8" 次\n";
    os << u8"  查一个不在表里的键（" << alive_count << u8" 次）：共探测 " << miss_with_tombstones
       << u8" 次，平均 "
       << fixed(static_cast<double>(miss_with_tombstones) / static_cast<double>(alive_count), 2)
       << u8" 次——墓碑不会加长它，探测照样一格一格走过\n";
    os << u8"  对照：两张表都是容量 " << capacity_at_compare << u8"、" << size_after_delete
       << u8" 个键，各再插 " << new_keys << u8" 个新键\n";
    os << u8"    留着 700 个墓碑的那张：共探测 " << insert_with_tombstones << u8" 次，平均 "
       << fixed(static_cast<double>(insert_with_tombstones)
                / static_cast<double>(new_keys), 2) << u8" 次\n";
    os << u8"    重整清干净的那张：共探测 " << insert_after_clean << u8" 次，平均 "
       << fixed(static_cast<double>(insert_after_clean) / static_cast<double>(new_keys), 2)
       << u8" 次\n";
    os << u8"  墓碑占着位置：插入要绕过它们找空槽，负载因子也把它们算在内，表会更早地长大\n";
}

void append_bad_hash_section(std::ostringstream &os)
{
    os << u8"\n坏哈希对照：同一份 " << kKeyCount << u8" 个键、同一套 " << kKeyCount
       << u8" 次查找\n";
    os << u8"  链地址法          桶数    最长链  查找比较次数  平均每次  命中\n";
    const HashKind kinds[3] = { HashKind::Mix, HashKind::LowByte, HashKind::Constant };
    for (HashKind kind : kinds) {
        const LookupRun run = run_chained(kind);
        os << "  " << pad_right(hash_kind_name(kind), 18) << pad_left(run.bucket_count, 8) << "  "
           << pad_left(run.longest, 6) << "  " << pad_left(run.comparisons, 12) << "  "
           << pad_left_fixed(static_cast<double>(run.comparisons) / 1000.0, 8, 2) << "  "
           << pad_left(run.hits, 4) << "\n";
    }

    os << u8"\n  开放寻址          容量    查找探测次数  平均每次  命中\n";
    for (HashKind kind : kinds) {
        const LookupRun run = run_open(kind);
        os << "  " << pad_right(hash_kind_name(kind), 18) << pad_left(run.capacity, 8) << "  "
           << pad_left(run.steps, 12) << "  "
           << pad_left_fixed(static_cast<double>(run.steps) / 1000.0, 8, 2) << "  "
           << pad_left(run.hits, 4) << "\n";
    }
    os << u8"  三种哈希下命中次数完全一样：哈希只决定快慢，不决定对错\n";
    os << u8"  键都是 10 的倍数，「只看低 8 位」只有 26 种取值，于是挤在少数桶里\n";
}

void append_wrong_erase_section(std::ostringstream &os)
{
    os << u8"\n删除：墓碑与「直接清空」的差别\n";

    const std::size_t del = 700;
    const std::vector<int> keys = make_keys(kKeyCount);

    OpenHash right(HashKind::Mix, 0.7);
    OpenHash wrong(HashKind::Mix, 0.7);
    for (int key : keys) {
        right.insert(key);
        wrong.insert(key);
    }
    for (std::size_t i = 0; i < del; ++i) {
        right.erase(keys[i]);
        wrong.erase_clearing_slot(keys[i]);
    }

    std::size_t right_found = 0;
    std::size_t wrong_found = 0;
    std::size_t remaining = 0;
    for (std::size_t i = del; i < keys.size(); ++i) {
        ++remaining;
        if (right.contains(keys[i])) {
            ++right_found;
        }
        if (wrong.contains(keys[i])) {
            ++wrong_found;
        }
    }

    os << u8"  删掉最前面的 " << del << u8" 个键，再查剩下的 " << remaining << u8" 个\n";
    os << u8"  留墓碑（正确）：找到 " << right_found << u8" 个，没找到 " << (remaining - right_found)
       << u8" 个\n";
    os << u8"  直接清空（错误）：找到 " << wrong_found << u8" 个，没找到 "
       << (remaining - wrong_found) << u8" 个\n";
    os << u8"  清空的那一格让探测提前停下，后面的键就被挡在序列之外\n";
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;
    append_growth_section(os);
    append_open_section(os);
    append_bad_hash_section(os);
    append_wrong_erase_section(os);
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
    const std::vector<int> keys = make_keys(kKeyCount);

    /* 1. 链地址法：重复插入被拒绝 */
    {
        ChainedHash table;
        const bool first = table.insert(10);
        const bool second = table.insert(10);
        c.check(first && !second && table.size() == 1,
                u8"链地址法：重复插入返回 false，size 不变",
                std::to_string(table.size()) + u8" 个键");
    }

    /* 2. 链地址法：插入过的都找得到，没插入的找不到 */
    {
        ChainedHash table;
        for (int key : keys) {
            table.insert(key);
        }
        std::size_t hits = 0;
        for (int key : keys) {
            if (table.contains(key)) {
                ++hits;
            }
        }
        std::size_t misses = 0;
        for (int key : keys) {
            if (!table.contains(key + 5)) {
                ++misses;               /* 键都是 10 的倍数，加 5 一定不在表里 */
            }
        }
        c.check(hits == keys.size() && misses == keys.size(),
                u8"链地址法：1000 个键全部找得到，另外 1000 个不存在的键全部找不到",
                std::to_string(hits) + u8" / " + std::to_string(misses));
    }

    /* 3. 链地址法：删除之后再找就找不到了 */
    {
        ChainedHash table;
        for (int key : keys) {
            table.insert(key);
        }
        const bool erased = table.erase(500);
        const bool again = table.erase(500);
        c.check(erased && !again && !table.contains(500) && table.size() == keys.size() - 1,
                u8"链地址法：删掉一个键之后找不到它，再删一次返回 false",
                std::to_string(table.size()) + u8" 个键");
    }

    /* 4. 链地址法：负载因子始终不超上限 */
    {
        ChainedHash table(HashKind::Mix, 1.0);
        double peak = 0.0;
        for (int key : keys) {
            table.insert(key);
            if (table.load_factor() > peak) {
                peak = table.load_factor();
            }
        }
        c.check(peak <= 1.0, u8"链地址法：负载因子始终不超过上限 1.0", fixed(peak, 3));
    }

    /* 5. 链地址法：桶数取的是素数 */
    {
        ChainedHash table;
        bool all_prime = true;
        std::size_t last = table.bucket_count();
        for (std::size_t i = 1; i <= 2000; ++i) {
            table.insert(static_cast<int>(i) * 10);
            if (table.bucket_count() != last) {
                last = table.bucket_count();
                all_prime = all_prime && is_prime(last);
            }
        }
        c.check(all_prime, u8"链地址法：每次加桶之后桶数都是素数",
                std::to_string(table.bucket_count()) + u8" 个桶");
    }

    /* 6. 链地址法：再哈希之后所有键仍然找得到 */
    {
        ChainedHash table;
        for (int key : keys) {
            table.insert(key);
        }
        const std::size_t before = table.bucket_count();
        std::size_t hits = 0;
        for (int key : keys) {
            if (table.contains(key)) {
                ++hits;
            }
        }
        c.check(before > 1 && hits == keys.size(),
                u8"链地址法：加桶与再哈希之后 1000 个键仍然全部找得到",
                std::to_string(before) + u8" 个桶");
    }

    /* 7. 坏哈希不影响正确性 */
    {
        ChainedHash table(HashKind::Constant, 1.0);
        for (int key : keys) {
            table.insert(key);
        }
        std::size_t hits = 0;
        for (int key : keys) {
            if (table.contains(key)) {
                ++hits;
            }
        }
        c.check(hits == keys.size() && table.longest_bucket() == keys.size(),
                u8"坏哈希照样正确：所有键挤在一个桶里，1000 个键仍然全部找得到",
                std::to_string(hits) + u8" 次命中，最长链 "
                    + std::to_string(table.longest_bucket()));
    }

    /* 8. 坏哈希的代价：比较次数差两个数量级 */
    {
        const LookupRun good = run_chained(HashKind::Mix);
        const LookupRun bad = run_chained(HashKind::Constant);
        c.check(good.comparisons * 100 < bad.comparisons,
                u8"坏哈希的查找比较次数至少是好哈希的 100 倍",
                std::to_string(good.comparisons) + u8" 对 " + std::to_string(bad.comparisons));
    }

    /* 9. 两种实现的命中结果逐位一致 */
    {
        ChainedHash chained;
        OpenHash open;
        for (int key : keys) {
            chained.insert(key);
            open.insert(key);
        }
        bool same = chained.size() == open.size();
        for (int key : keys) {
            same = same && chained.contains(key) == open.contains(key);
        }
        c.check(same && chained.size() == keys.size(),
                u8"两种实现：1000 个键的命中结果逐位一致",
                std::to_string(chained.size()) + u8" / " + std::to_string(open.size()));
    }

    /* 10. 开放寻址：插入、查找、删除的基本语义 */
    {
        OpenHash table;
        const bool first = table.insert(10);
        const bool second = table.insert(10);
        const bool found = table.contains(10);
        const bool erased = table.erase(10);
        c.check(first && !second && found && erased && !table.contains(10) && table.size() == 0,
                u8"开放寻址：插入、重复插入、查找、删除都对");
    }

    /* 11. 开放寻址：负载因子不超上限 */
    {
        OpenHash table(HashKind::Mix, 0.7);
        double peak = 0.0;
        for (int key : keys) {
            table.insert(key);
            if (table.load_factor() > peak) {
                peak = table.load_factor();
            }
        }
        c.check(peak <= 0.7, u8"开放寻址：负载因子始终不超过上限 0.7", fixed(peak, 3));
    }

    /* 12. 开放寻址：墓碑不影响正确性 */
    {
        OpenHash table;
        for (int key : keys) {
            table.insert(key);
        }
        for (std::size_t i = 0; i < 700; ++i) {
            table.erase(keys[i]);
        }
        std::size_t hits = 0;
        for (std::size_t i = 700; i < keys.size(); ++i) {
            if (table.contains(keys[i])) {
                ++hits;
            }
        }
        std::size_t deleted_gone = 0;
        for (std::size_t i = 0; i < 700; ++i) {
            if (!table.contains(keys[i])) {
                ++deleted_gone;
            }
        }
        c.check(hits == 300 && deleted_gone == 700 && table.tombstones() == 700,
                u8"开放寻址：删掉 700 个之后，剩下 300 个全找得到、删掉的 700 个全找不到",
                std::to_string(hits) + u8" / " + std::to_string(deleted_gone));
    }

    /* 13. 开放寻址：墓碑占着位置，插入要绕过它们找空槽 */
    {
        OpenHash with_tombstones;
        OpenHash cleaned;
        for (int key : keys) {
            with_tombstones.insert(key);
            cleaned.insert(key);
        }
        for (std::size_t i = 0; i < 700; ++i) {
            with_tombstones.erase(keys[i]);
            cleaned.erase(keys[i]);
        }
        cleaned.rehash_clean();

        stats().reset();
        for (std::size_t i = 0; i < 300; ++i) {
            with_tombstones.insert(20000 + static_cast<int>(i) * 10);
        }
        const std::size_t with = stats().steps;

        stats().reset();
        for (std::size_t i = 0; i < 300; ++i) {
            cleaned.insert(20000 + static_cast<int>(i) * 10);
        }
        const std::size_t without = stats().steps;

        c.check(with_tombstones.tombstones() > 0 && cleaned.tombstones() == 0 && with > without,
                u8"开放寻址：同样容量、同样键数，有墓碑的那张表插入要多探测",
                std::to_string(with) + u8" 对 " + std::to_string(without));
    }

    /* 14. 直接清空删除位会断掉探测序列 */
    {
        OpenHash table;
        for (int key : keys) {
            table.insert(key);
        }
        for (std::size_t i = 0; i < 700; ++i) {
            table.erase_clearing_slot(keys[i]);
        }
        std::size_t lost = 0;
        for (std::size_t i = 700; i < keys.size(); ++i) {
            if (!table.contains(keys[i])) {
                ++lost;
            }
        }
        c.check(lost > 0, u8"直接清空删除位会挡断探测序列：剩下的键里有找不到的",
                std::to_string(lost) + u8" 个找不到");
    }

    /* 15. 插入顺序不影响结果 */
    {
        ChainedHash forward;
        ChainedHash backward;
        for (int key : keys) {
            forward.insert(key);
        }
        for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
            backward.insert(*it);
        }
        bool same = forward.size() == backward.size();
        for (int key : keys) {
            same = same && forward.contains(key) && backward.contains(key);
        }
        c.check(same, u8"插入顺序不影响结果：正序与倒序插出来的表一样");
    }

    /* 16. 空表 */
    {
        ChainedHash chained;
        OpenHash open;
        c.check(!chained.contains(10) && chained.size() == 0 && !open.contains(10)
                    && open.size() == 0 && open.capacity() == 8,
                u8"空表：查找一律返回 false，开放寻址的初始容量是 8");
    }

    /* 17. 删除全部键之后表回到空 */
    {
        ChainedHash chained;
        OpenHash open;
        for (int key : keys) {
            chained.insert(key);
            open.insert(key);
        }
        std::size_t chained_gone = 0;
        std::size_t open_gone = 0;
        for (int key : keys) {
            if (!chained.erase(key)) {
                ++chained_gone;
            }
            if (!open.erase(key)) {
                ++open_gone;
            }
        }
        bool none_left = true;
        for (int key : keys) {
            none_left = none_left && !chained.contains(key) && !open.contains(key);
        }
        c.check(chained_gone == 0 && open_gone == 0 && none_left && chained.size() == 0
                    && open.size() == 0,
                u8"把 1000 个键全删掉：两边都回到空，一个都不剩");
    }

    /* 18. 哈希值与判等一致：相等的键哈希值一定相同 */
    {
        bool consistent = true;
        for (int key : keys) {
            consistent = consistent && hash_key(key, HashKind::Mix) == hash_key(key, HashKind::Mix)
                          && hash_key(key, HashKind::Constant) == 0;
        }
        c.check(consistent, u8"同一个键每次算出的哈希值相同，与判等一致");
    }

    /* 19. 线性探测在好哈希下的平均探测次数不多 */
    {
        OpenHash table(HashKind::Mix, 0.7);
        for (int key : keys) {
            table.insert(key);
        }
        stats().reset();
        for (int key : keys) {
            table.contains(key);
        }
        const double average = static_cast<double>(stats().steps) / 1000.0;
        c.check(average < 3.0, u8"好哈希下线性探测平均每次查找不超过 3 次",
                fixed(average, 2) + u8" 次");
    }

    return c.take();
}

}   /* namespace hmini */
