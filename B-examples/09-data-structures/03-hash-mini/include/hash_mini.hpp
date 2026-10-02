/**
 * hash_mini.hpp —— 哈希表两版：链地址法与开放寻址
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
 * 两版共用同一套接口与同一批计数：
 *
 *   insert(key)     插入；键已经在表里返回 false
 *   contains(key)   查找
 *   erase(key)      删除；删到了返回 true
 *
 * 差别只在冲突怎么办：
 *
 *   链地址法  桶是一段连续内存，冲突的键挂成一条链；删除就是摘节点。
 *   开放寻址  所有键都在槽数组里，冲突时按线性探测往后找；删除必须留墓碑，
 *             因为直接把槽清空会挡断后面那些键的探测序列。
 *
 * 哈希函数由一个开关切换（HashKind），用来对照「坏哈希有多坏」：
 * 三种哈希下表的正确性完全一样，变的只是比较次数。
 *
 * 面向人的文字一律是 u8"" 字面量，因此本头文件里的 std::string 承载 UTF-8 字节。
 */
#ifndef HASH_MINI_HPP
#define HASH_MINI_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace hmini {

/* ================= 计数 ================= */

/** 全局计数，按「先 reset、再做一件事、读增量」的方式用 */
struct HashStats {
    std::size_t comparisons = 0;     /**< 键与键比较的次数 */
    std::size_t steps = 0;           /**< 走过的节点或槽的个数 */
    std::size_t rehashes = 0;        /**< 再哈希（换桶数组）的次数 */
    std::size_t rehash_moves = 0;    /**< 再哈希时重新放过的元素个数 */
    std::size_t tombstone_clears = 0;/**< 重整时清掉的墓碑个数 */

    void reset();
};

HashStats &stats();

/* ================= 哈希函数 ================= */

/** 哈希开关。三者对判等的语义没有影响，只影响键落到哪个桶 */
enum class HashKind {
    Mix,        /**< 混合：键的每一位都参与，分布均匀 */
    LowByte,    /**< 只看低 8 位：键有规律时会挤在少数桶里 */
    Constant,   /**< 永远返回 0：所有键挤进同一个桶，退化成链表 */
};

const char *hash_kind_name(HashKind kind);

/** 算一个键的哈希值。size_t 的取值，取桶号时再对桶数取余 */
std::size_t hash_key(int key, HashKind kind);

/** 不小于 n 的下一个素数；链地址法的桶数取素数 */
std::size_t next_prime_at_least(std::size_t n);

/** n 是不是素数 */
bool is_prime(std::size_t n);

/* ================= 链地址法 ================= */

class ChainedHash {
public:
    explicit ChainedHash(HashKind kind = HashKind::Mix, double max_load = 1.0);

    ~ChainedHash();
    ChainedHash(const ChainedHash &) = delete;
    ChainedHash &operator=(const ChainedHash &) = delete;

    bool insert(int key);
    bool contains(int key) const;
    bool erase(int key);

    std::size_t size() const noexcept { return size_; }
    std::size_t bucket_count() const noexcept { return bucket_count_; }
    double load_factor() const noexcept;
    std::size_t longest_bucket() const;
    std::size_t used_buckets() const;
    HashKind hash_kind() const noexcept { return kind_; }

private:
    struct Node {
        int key;
        Node *next;
    };

    void rehash(std::size_t new_count);
    static void destroy_chain(Node *head);

    std::vector<Node *> buckets_;
    std::size_t size_ = 0;
    std::size_t bucket_count_ = 1;
    double max_load_ = 1.0;
    HashKind kind_ = HashKind::Mix;
};

/* ================= 开放寻址 ================= */

class OpenHash {
public:
    explicit OpenHash(HashKind kind = HashKind::Mix, double max_load = 0.7);

    bool insert(int key);
    bool contains(int key) const;
    bool erase(int key);

    /** 清掉墓碑、把元素重新排一遍 */
    void rehash_clean();

    /** 故意写错的一种删除：把槽直接清空。
        它会让探测序列断掉，后面那些键就找不到了——只用来演示，正常代码里不要用。 */
    bool erase_clearing_slot(int key);

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return slots_.size(); }
    std::size_t tombstones() const noexcept { return tombstones_; }
    double load_factor() const noexcept;
    HashKind hash_kind() const noexcept { return kind_; }

private:
    enum class State : unsigned char { Empty, Occupied, Tombstone };

    struct Slot {
        int key = 0;
        State state = State::Empty;
    };

    bool find_slot(int key, std::size_t &index) const;
    void grow(std::size_t new_capacity);
    void insert_into(std::vector<Slot> &slots, int key) const;

    std::vector<Slot> slots_;
    std::size_t size_ = 0;
    std::size_t tombstones_ = 0;
    double max_load_ = 0.7;
    HashKind kind_ = HashKind::Mix;
};

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;     /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;        /**< "16 项中 16 项通过，全部通过" */
};

/** 项目输出：桶数增长、开放寻址的墓碑、坏哈希对照、删错位置的后果四段。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对两版的插入、查找、删除、再哈希与坏哈希下的正确性 */
CheckResult run_self_tests();

}   /* namespace hmini */

#endif /* HASH_MINI_HPP */
