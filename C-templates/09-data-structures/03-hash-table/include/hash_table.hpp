/* hash_table.hpp —— 练习模板 03 的核心接口（C++）
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
 * 开放寻址（线性探测）的哈希表，分成 4 个阶段。
 * 题目、验收标准与自查方法见同目录《配置步骤.md》。
 *
 *     阶段 1  probe()           探测：三种状态各自的走法，墓碑那一支在这里
 *     阶段 2  rehash()          再哈希：换更大的桶数组，把元素重新放一遍
 *     阶段 3  erase()           删除：置墓碑，不是清空
 *     阶段 4  grow_if_needed()  什么时候加桶
 *
 * 桶数组用 std::vector 存，本模板要练的是「哈希表自己的那三件事」：
 * 哈希、桶数、删除。类模板的成员函数写在使用点可见的地方，因此这个文件
 * 既有声明也有实现；hash_string 的定义在 src/hash_functions.cpp 里。
 */
#ifndef HASH_TABLE_HPP
#define HASH_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dsh {

/* 字符串的哈希：经典的 5 乘加。定义在 src/hash_functions.cpp 里 */
std::size_t hash_string(const std::string &text);

/* 默认哈希：整数做一次混合，字符串走 hash_string。
 * 混合的作用是让高位也参与——直接拿原值对桶数取余时，低位相同的键会挤在一起。 */
template <class K>
struct DefaultHash;

template <>
struct DefaultHash<int> {
    std::size_t operator()(int key) const {
        std::uint32_t x = static_cast<std::uint32_t>(key);
        x ^= x >> 16;
        x *= 0x7feb352dU;
        x ^= x >> 15;
        x *= 0x846ca68bU;
        x ^= x >> 16;
        return static_cast<std::size_t>(x);
    }
};

template <>
struct DefaultHash<std::string> {
    std::size_t operator()(const std::string &key) const { return hash_string(key); }
};

/* 坏哈希：所有键都算成 0。验收程序用它演示「哈希质量决定一切」（阶段 4） */
template <class K>
struct AllZeroHash {
    std::size_t operator()(const K &) const { return 0; }
};

template <class K, class V, class Hash = DefaultHash<K>>
class HashTable {
public:
    /* 桶的三种状态。
     * 墓碑是开放寻址独有的一档：删除**不能**把桶直接清空，否则探测序列会断，
     * 后面来找同一个键的元素会以为「这里从来没被占过」而提前停下。 */
    enum class State : unsigned char { Empty, Occupied, Tombstone };

    struct Slot {
        State state = State::Empty;
        K key{};
        V value{};
    };

    static constexpr std::size_t kInitialBuckets = 8;
    static constexpr double kMaxLoadFactor = 0.7;

    /* 开局就给 kInitialBuckets 个空桶，因此 slots_ 永远不是空的 */
    HashTable(void) : slots_(kInitialBuckets) {}

    std::size_t size(void) const { return size_; }
    std::size_t bucket_count(void) const { return slots_.size(); }
    std::size_t tombstones(void) const { return tombstones_; }

    double load_factor(void) const {
        return slots_.empty() ? 0.0
                              : static_cast<double>(size_) / static_cast<double>(slots_.size());
    }

    /* 探测步数：每读一个桶算一步。验收程序用它比较哈希函数的好坏 */
    long probe_steps(void) const { return probe_steps_; }
    void reset_probe_steps(void) { probe_steps_ = 0; }

    /* 已给出：最长的一段连续非空桶（占用与墓碑都算）。探测变慢的直接原因是它变长 */
    std::size_t longest_cluster(void) const {
        std::size_t best = 0;
        std::size_t run = 0;
        for (const Slot &s : slots_) {
            if (s.state == State::Empty) {
                run = 0;
            } else {
                ++run;
                if (run > best) {
                    best = run;
                }
            }
        }
        return best;
    }

    /* 已给出：检查桶的状态与两个计数是否自洽 */
    const char *check(void) const {
        std::size_t occupied = 0;
        std::size_t tombstones = 0;
        for (const Slot &s : slots_) {
            if (s.state == State::Occupied) {
                ++occupied;
            } else if (s.state == State::Tombstone) {
                ++tombstones;
            }
        }
        if (occupied != size_) {
            return "occupied slots differ from size()";
        }
        if (tombstones != tombstones_) {
            return "tombstone count differs from the slots marked Tombstone";
        }
        if (size_ + tombstones_ > slots_.size()) {
            return "size + tombstones exceeds bucket_count";
        }
        return nullptr;
    }

    /* 已给出：插入。键已经在表里就只更新值，返回 false */
    bool insert(const K &key, const V &value) {
        grow_if_needed();                       /* 阶段 4-1：先加桶，再插 */
        bool found = false;
        const std::size_t idx = locate(key, found);
        if (found) {
            slots_[idx].value = value;
            return false;
        }
        if (idx >= slots_.size()) {
            return false;                       /* 一圈都是占用或墓碑，没有位置 */
        }
        slots_[idx].state = State::Occupied;
        slots_[idx].key = key;
        slots_[idx].value = value;
        ++size_;
        return true;
    }

    /* 已给出：查找 */
    bool find(const K &key, V &out) const {
        bool found = false;
        const std::size_t idx = locate(key, found);
        if (!found) {
            return false;
        }
        out = slots_[idx].value;
        return true;
    }

    bool erase(const K &key);                   /* 阶段 3-1 */
    void rehash(std::size_t new_count);         /* 阶段 2-1 */

private:
    /* 阶段 1-1：在被探测的那张表里找 key，找到时通过 found 告知，返回桶号 */
    std::size_t probe(const std::vector<Slot> &table, const K &key, bool &found) const;

    /* 已给出：在当前表里探测 */
    std::size_t locate(const K &key, bool &found) const { return probe(slots_, key, found); }

    /* 已给出：读一个桶，顺便记一次探测步数。探测循环里请用它，别直接写 table[i] */
    const Slot &at(const std::vector<Slot> &table, std::size_t i) const {
        ++probe_steps_;
        return table[i];
    }

    /* 阶段 4-1：加桶的时机 */
    void grow_if_needed(void);

    std::vector<Slot> slots_;
    std::size_t size_ = 0;
    std::size_t tombstones_ = 0;
    Hash hash_{};
    mutable long probe_steps_ = 0;
};

/* ==================================================================
 * 阶段 1-1：探测
 * ================================================================== */

/* TODO（阶段 1-1）：在 table 里找 key。
 *
 * 找到时把 found 置 true 并返回它所在的桶号；没找到时 found 置 false，
 * 返回「这个键该放进哪个桶」。返回 table.size() 表示一圈下来都没有位置。
 *
 * 要探测的表由参数给进来而不是固定用 slots_：再哈希（阶段 2）要把元素
 * 放进新表里，用的就是这个函数。读桶请用 at(table, i)，它顺便记探测步数。
 *
 * 三种状态各自的走法（这是本阶段的核心）：
 *
 * | 遇到 | 查找时 | 插入时 |
 * |---|---|---|
 * | 空（Empty） | 停下，说明后面不可能有 | 可以放这里 |
 * | 占用（Occupied） | 比较键，相等就是找到 | 不能放 |
 * | **墓碑（Tombstone）** | **继续往后找** | 不能放（本模板只在再哈希时清墓碑） |
 *
 * 漏掉墓碑那一支的后果只在删过元素之后才出现：探测提前停在墓碑上，
 * 于是「本来找得到的键变成找不到」。阶段 3 的验收就是专门判这一条的。
 *
 * 提示：
 *   1. 起点是 hash_(key) 对桶数取余；
 *   2. 线性探测就是每次往后挪一格，挪到头要绕回 0 号桶；
 *   3. 最多走 table.size() 步——走满一圈还没有空位，说明表满了；
 *   4. 桶数是 0 的表要单独处理，否则取余会除以 0。
 *
 * 判据（见《配置步骤.md》阶段 1）：插进 5 个键之后 check() 正常、5 个键都找得到、
 *   没插过的键找不到；占位实现（永远返回 0 号桶）会让 check() 报
 *   「occupied slots differ from size()」，并且所有查找都落空。 */
template <class K, class V, class H>
std::size_t HashTable<K, V, H>::probe(const std::vector<Slot> &table, const K &key, bool &found) const
{
    (void)table;
    (void)key;
    found = false;
    return 0;   /* 占位实现：永远说「没找到」，并且总是给出 0 号桶 */
}

/* ==================================================================
 * 阶段 2-1：再哈希
 * ================================================================== */

/* TODO（阶段 2-1）：把桶数组换成 new_count 个，把所有活元素重新放一遍。
 *
 * 三件事：
 *   1. 建一张 new_count 个空桶的新表，把每个**占用**的桶按 key 重新探测一次放进去。
 *      墓碑不搬——它们在这次搬迁里被清掉，这正是「定期重整」的做法；
 *   2. 换过来之后，桶数、墓碑数这些计数都要跟着对；
 *   3. **节点（元素）本身不重新构造**：桶换了位置，元素的值只是被搬到新表里。
 *
 * 提示：可以复用阶段 1 的 probe()——把新表当参数传进去，
 *   这一次探测的起点自然变成 hash_(key) % new_count。
 *
 * 判据（见《配置步骤.md》阶段 2）：显式 rehash(16) 之后桶数从 8 变成 16，
 *   原来找得到的键一个不少；阶段 3 做完之后再跑一次，
 *   墓碑数会从某个正数归零。占位实现什么都不做，桶数不会变。 */
template <class K, class V, class H>
void HashTable<K, V, H>::rehash(std::size_t new_count)
{
    (void)new_count;
    /* 占位实现：什么都不做，桶数组原样不动 */
}

/* ==================================================================
 * 阶段 3-1：删除
 * ================================================================== */

/* TODO（阶段 3-1）：把 key 对应的桶标成墓碑，并让两个计数跟上。
 *
 * 为什么不能标成「空」：探测到了空位就停下，而删除留下的洞后面可能还挂着
 * 因为冲突而挪过来的元素——那些元素会因此永远找不到。
 *
 * 要做的事：
 *   1. 先探测找到它；找不到就返回 false，什么也不改；
 *   2. 找到之后把那个桶的状态改成墓碑，元素个数减一、墓碑数加一；
 *   3. 桶里原来的键与值可以不清理（状态决定一切）；想清掉也行，
 *      好处是让键值持有的资源早点释放。
 *
 * 判据（见《配置步骤.md》阶段 3）：删掉两个落在同一个桶上的键里靠前的那个之后，
 *   靠后的那个**仍然找得到**（它的探测要经过墓碑），
 *   重复删同一个键返回 false；再跑一次 rehash，墓碑数归零。 */
template <class K, class V, class H>
bool HashTable<K, V, H>::erase(const K &key)
{
    (void)key;
    /* 占位实现：什么都不做，永远说「没有这个键」 */
    return false;
}

/* ==================================================================
 * 阶段 4-1：什么时候加桶
 * ================================================================== */

/* TODO（阶段 4-1）：在插入之前判断「插进去会不会超过上限」，超了就先加桶。
 *
 * 本模板的限额已经给定：kMaxLoadFactor 是 0.7，桶数不够时翻倍。
 * 你要写的是这个判断本身，注意三点：
 *   1. **插入之前判断**：插完再判断会短暂超限，桶里的探测链更长；
 *   2. 判断里要算上墓碑：墓碑也占着探测链，`size_ + tombstones_ + 1`
 *      才是「插进去之后一共要占多少个桶」；
 *   3. 加桶是成倍的（翻倍），不是加一格——每次只加一格会让插入退化成
 *      每次都重排全表。
 *
 * 判据（见《配置步骤.md》阶段 4）：连续插入 200 个键，桶数序列是
 *   8、16、32、64、128、256、512（6 次加桶），负载因子始终不超过 0.7，
 *   200 个键一个不少。占位实现从不加桶，表在 8 个桶装满之后就再也插不进去了。 */
template <class K, class V, class H>
void HashTable<K, V, H>::grow_if_needed(void)
{
    /* 占位实现：从不加桶 */
}

} /* namespace dsh */

#endif /* HASH_TABLE_HPP */
