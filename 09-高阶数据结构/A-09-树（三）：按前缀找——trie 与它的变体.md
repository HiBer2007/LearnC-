# 树（三）：按前缀找——trie 与它的变体

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

前两章的树都在回答「这个键在不在」和「哪一段键在这两个值之间」。
还有一类查询它们都答不好：**以某个前缀开头的键有哪些。**
输入框里敲「数据」两个字，希望列出「数据」「数据库」「数据中心」——
哈希表算得出「数据」这个键的哈希，却算不出「所有以数据开头的键」，
因为哈希把整个键打散成了一个数，前缀的信息在打散时就没了。

有序结构（`map`、B+ 树）能回答前缀查询，做法是先 `lower_bound` 找到起点，
再一项一项往后看。代价是每次比较都要**比较整个键**：
查一个 8 字符的前缀，每个候选都要从头比一遍。
而前缀查询真正需要的信息只有「键的前 8 个字符」这一件事。

trie 把这件事做到极致：**它不比较键，而是一个字符一个字符地往下走。**
每一层对应键的一个单位，从根走到某个节点，路径就是那个前缀。
于是「以某前缀开头的键」这个查询变成「走到那个节点，把它下面所有的词收出来」——
查询的代价只与**前缀长度**有关，与表里有多少个键无关。

代价跟着换了地方：trie 的节点很多，而且**每个节点的子节点怎么存**成了主要开销。
同一份两万条的词表下，三种存法的每键字节数差得很远：
定长数组 10387 字节、哈希表 871.9 字节、有序表 525.4 字节，
而同样数据的 `unordered_set` 只要 64.2 字节——
**最省的那版 trie（有序表）也是它的八倍以上。**
还有一处选择：**按字节建还是按码点建**。
UTF-8 里一个汉字占三个字节，按字节建的 trie 会把一个字拆成三层，
同一份中文词表的节点数因此从 40 涨到 113。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 复现命令随程序给出；`文档` 表示引自标准草案或官方资料；`待确认` 表示尚未验证。
> 路径占位符（如 `<MinGW>`）的含义见《README.md》。
> 本章节实测环境：Windows 11 + g++ 15.2.0（MinGW-w64）。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 哈希表的结构与「哈希打散了什么」 | 《09-高阶数据结构/A-04-按哈希定位：unordered_map.md》第 1 节 |
| 有序结构与 `lower_bound` | 《09-高阶数据结构/A-03-索引存储：map、set 与有序.md》第 4 节 |
| `std::string` 与字符串字面量 | 《07-标准库/B-02-std-string 与 string_view.md》第 1 节 |
| `char` 与字节的关系 | 《04-语法/02-数据类型与类型系统.md》第 2 节 |
| 树的节点与连接 | 《09-高阶数据结构/A-07-树（一）：从搜索树到平衡.md》第 2 节 |

**相邻的章节**：`A-08` 把「一次读一页」这个约束放进结构，得到多路树；
本章换一个约束（**按前缀定位**），得到的形状完全不同——
trie 的层数由键长决定，与键的个数无关。
三章的树各回答一类需求：有序查找、区间扫描、前缀枚举。
**本章的程序是为了测量而写的最小实现**：没有内存池、没有序列化、没有并发保护，
这些工程做法不在本章范围内。

| 本章各节 | 讲什么 |
|---|---|
| 第 1 节 | 需求：前缀；哈希为什么给不了；有序结构给得了但每次比整个键 |
| 第 2 节 | 模型与接口：节点的形状、插入、整词与前缀的差别、前缀枚举、删除 |
| 第 3 节 | 子节点怎么存：定长数组、哈希表、有序表三种取舍与实测对照 |
| 第 4 节 | 内存代价：与 `unordered_set`、`map` 的每键字节对照 |
| 第 5 节 | 按字节建还是按码点建：中文在 UTF-8 下的三层问题与实测 |
| 第 6 节 | 变体：压缩 trie 与 radix 树 |
| 第 7 节 | 判据：什么时候用 trie，什么时候用别的东西 |

---

# 第 1 节 需求：按前缀找

## 1.1 哈希表给不了前缀

哈希表的查找是「算一个数，跳到那个桶」。
「以数据开头」不是一个键，而是一族键，它们算出来的哈希值各不相同、分布在全表各处。
**要找出这一族，只能把全表扫一遍。**

| 需求 | 哈希表 | 有序结构 |
|---|---|---|
| `find("数据")` | 平均 `O(1)` | `O(log n)` |
| 所有以「数据」开头的键 | **做不到（只能全表扫）** | `O(log n + k)`，`k` 是结果个数 |
| 所有以「数」开头的键 | **做不到（只能全表扫）** | `O(log n + k)` |

## 1.2 有序结构能给，但每次比较整个键

`std::map` 查询前缀的做法是：`lower_bound("数据")` 找到第一个不小于它的键，
然后一直 `++` 直到键不再以「数据」开头。`A-03` 已经讲过这套写法。
代价在两处：

- `lower_bound` 要做 `log n` 次比较，每次比较的是**整个键**；
- 前缀越短，落在区间里的键越多，`++` 的趟数越多。

**能改进的地方在比较上**：查前缀时，两个键只要前几个字符不同，就已经能定下顺序，
后面的字符根本不用看——但 `std::string` 的 `<` 会一个字符一个字符地比下去，直到比出结果为止。
trie 把「比较」整个换掉：它**逐单位往下走**，走过的那条路径就是前缀。

## 1.3 trie：一层对应一个单位

`Text`

```text
   存了 car、card、care、careful、cat、do、dog 的 trie（· 表示词尾）

            (根)
           /    \
          c      d
          │      │
          a      o ·
         / \     │
        r · t    g ·
        │
        e ·
        │
        f
        │
        u
        │
        l ·

   查 "care"：从根走 c → a → r → e，四步走到，看词尾标记 ✓
   查 "ca"  ：走 c → a，两步走到，但它没有词尾标记 ✗（是前缀，不是词）
   枚举前缀 "car" 开头的词：走到 r 那个节点，把它下面的词尾全收出来
```

**层数与键长成正比，与键的个数无关。**
这是 trie 与前面几种树最根本的差别：它不是靠比较缩小范围，而是靠路径直接定位。

---

# 第 2 节 模型与接口

## 2.1 节点的形状

`Text`

```text
   ┌───────────────────────────────────────────┐
   │  子节点表：单位 → 子节点                   │
   │  词尾标记：这里是不是一个完整的词          │
   └───────────────────────────────────────────┘

   「子节点表」有三种存法（第 3 节），它们决定 trie 的内存与速度；
   「词尾标记」是一个 bool，占一个字节，但它必不可少：
   没有它，就无法区分「car 是一个词」与「car 只是 card 的前缀」。
```

## 2.2 插入、整词查询、前缀查询

`C++`

```cpp
/* trie_basic.cpp    编译：g++ -std=c++17 -O2 trie_basic.cpp -o trie_basic
 * 按前缀找的最小 trie：插入、整词查询、前缀枚举、带计数的删除。
 * 这是为了说明代价而写的最小实现：只够测出代价，工程做法不在本章范围内。 */
#include <cstdio>
#include <string>
#include <vector>

class Trie {
public:
    Trie() : root_(new Node{}) {}
    ~Trie() { destroy(root_); }

    void insert(const std::string& word) {
        Node* p = root_;
        for (char ch : word) {
            Node*& slot = p->kids[static_cast<unsigned char>(ch)];
            if (!slot) slot = new Node{};
            p = slot;
        }
        p->terminal = true;
    }

    // 整词查询：走到最后一个字符后，还要看这里是不是一个词的结尾
    bool contains(const std::string& word) const {
        const Node* p = walk(word);
        return p && p->terminal;
    }

    // 前缀查询：走到最后一个字符就够了，不要求它是词尾
    bool has_prefix(const std::string& prefix) const { return walk(prefix) != nullptr; }

    // 前缀枚举：把以 prefix 开头的词全部收进 out
    void collect(const std::string& prefix, std::vector<std::string>& out) const {
        const Node* p = walk(prefix);
        if (!p) return;
        std::string cur = prefix;
        dfs(p, cur, out);
    }

    // 删除：先真的删掉这个词，再把不再需要的节点回收
    bool erase(const std::string& word) {
        if (!contains(word)) return false;
        erase_rec(root_, word, 0);
        return true;
    }

    long long nodes() const { return count(root_); }

private:
    struct Node {
        Node* kids[256] = {};        // 每个可能的字节一个位置：子节点好找，但很占地方
        bool terminal = false;
    };

    const Node* walk(const std::string& s) const {
        const Node* p = root_;
        for (char ch : s) {
            p = p->kids[static_cast<unsigned char>(ch)];
            if (!p) return nullptr;
        }
        return p;
    }

    static void dfs(const Node* p, std::string& cur, std::vector<std::string>& out) {
        if (p->terminal) out.push_back(cur);
        for (int c = 0; c < 256; ++c) {
            if (p->kids[c]) {
                cur.push_back(static_cast<char>(c));
                dfs(p->kids[c], cur, out);
                cur.pop_back();
            }
        }
    }

    bool erase_rec(Node* p, const std::string& word, std::size_t depth) {
        if (depth == word.size()) {
            p->terminal = false;
        } else {
            Node* child = p->kids[static_cast<unsigned char>(word[depth])];
            if (erase_rec(child, word, depth + 1)) {
                p->kids[static_cast<unsigned char>(word[depth])] = nullptr;
                delete child;
            }
        }
        // 自己没用了才让父节点把自己删掉：不是词尾，且一个孩子都没有
        if (p->terminal) return false;
        for (Node* c : p->kids) {
            if (c) return false;
        }
        return p != root_;
    }

    static long long count(const Node* p) {
        if (!p) return 0;
        long long n = 1;
        for (const Node* c : p->kids) n += count(c);
        return n;
    }

    static void destroy(Node* p) {
        if (!p) return;
        for (Node* c : p->kids) destroy(c);
        delete p;
    }

    Node* root_;
};

int main() {
    Trie t;
    for (const char* w : {"car", "card", "care", "careful", "cat", "dog", "do"}) t.insert(w);

    std::printf("== 整词与前缀的差别 ==\n");
    std::printf("  contains(\"car\")   = %d\n", t.contains("car"));
    std::printf("  contains(\"ca\")    = %d（ca 不是词，但它是前缀）\n", t.contains("ca"));
    std::printf("  has_prefix(\"ca\")  = %d\n", t.has_prefix("ca"));
    std::printf("  contains(\"carefu\")= %d（carefu 既不是词，也不是任何词的前缀）\n",
                t.contains("carefu"));

    std::printf("\n== 前缀枚举 ==\n");
    for (const char* p : {"car", "care", "do"}) {
        std::vector<std::string> out;
        t.collect(p, out);
        std::printf("  以 %-5s 开头：", p);
        for (const std::string& s : out) std::printf(" %s", s.c_str());
        std::printf("\n");
    }

    std::printf("\n== 删除 ==\n");
    std::printf("  删除前节点数 = %lld\n", t.nodes());
    std::printf("  erase(\"card\") = %d\n", t.erase("card"));
    std::printf("  删除后节点数 = %lld（card 那个 d 节点被回收了）\n", t.nodes());
    std::printf("  erase(\"care\") = %d\n", t.erase("care"));
    std::printf("  erase(\"care\") 再删一次 = %d\n", t.erase("care"));
    std::printf("  erase(\"card\") 再删一次 = %d（已经不在表里了）\n", t.erase("card"));
    std::printf("  删除后 contains(\"careful\") = %d（careful 还在）\n", t.contains("careful"));
    std::printf("  删除后节点数 = %lld\n", t.nodes());
    return 0;
}
```

`实测数据`
`Text`

```text
== 整词与前缀的差别 ==
  contains("car")   = 1
  contains("ca")    = 0（ca 不是词，但它是前缀）
  has_prefix("ca")  = 1
  contains("carefu")= 0（carefu 既不是词，也不是任何词的前缀）

== 前缀枚举 ==
  以 car   开头： car card care careful
  以 care  开头： care careful
  以 do    开头： do dog

== 删除 ==
  删除前节点数 = 13
  erase("card") = 1
  删除后节点数 = 12（card 那个 d 节点被回收了）
  erase("care") = 1
  erase("care") 再删一次 = 0
  erase("card") 再删一次 = 0（已经不在表里了）
  删除后 contains("careful") = 1（careful 还在）
  删除后节点数 = 12
```

**`contains` 与 `has_prefix` 只差一步：走到最后一个字符之后，看不看词尾标记。**
这一条差别就是「前缀」这个需求的全部来源。

## 2.3 删除为什么要小心

删除不能直接把路径上的节点全删掉，因为那些节点可能被别的词共用：

- 删掉 `card` 之后，`car`、`care` 还要用 `c`、`a`、`r` 三个节点；
- 只有当某个节点**既不是词尾、又没有孩子**时，才能把它回收。

上面的程序按这条规则做，所以删 `card` 只回收了 `d` 那一个节点（13 → 12）；
删 `care` 之后节点数不变，因为它虽然清掉了词尾标记，
但 `careful` 还要用到那几个节点。

> [!WARNING]
> **`erase` 忘了收回节点，trie 就会一直长。**
> 一个长期运行的补全服务如果只删数据不回收节点，
> 内存会随「历史上插入过的词」单调增长，而不是随「当前表里的词」增长。

---

# 第 3 节 子节点怎么存

节点的形状里，真正决定内存与速度的是「子节点表」。三种存法各有一个明确的取舍。

## 3.1 定长数组

`Text`

```text
   Node* kids[256];        // 每个可能的字节一个位置，共 256 × 8 = 2048 字节

   好处：定位只需一次下标访问，O(1) 且常数极小
   代价：不管这个节点实际有几个孩子，都要付 2048 字节
        一份两万条的英文词表有十万个节点 → 207 MB
```

**这是用空间换时间的极端版本。** 只有当字符集很小、且节点几乎都会被用到时
（例如只存 26 个小写字母、且词表覆盖很全）才划算。

## 3.2 哈希表

`Text`

```text
   std::unordered_map<unsigned char, Node*> kids;   // 只存真正存在的边

   好处：节点里有多少条边就付多少，稀疏时很省
   代价：每次定位要算哈希、可能还要跟着链走一趟；每个节点自己也要一个对象
```

## 3.3 有序表

`Text`

```text
   std::map<unsigned char, Node*> kids;             // 边按单位排序

   好处：省内存（每个元素一个节点，没有桶数组），且能按顺序枚举孩子
         —— 前缀枚举时输出天然是字典序，不必额外排序
   代价：每次定位是 O(log d)，d 是这个节点的孩子数；d 很小，实际很快
```

## 3.4 三种存法的对照

`C++`

```cpp
/* trie_children.cpp    编译：g++ -std=c++17 -O2 trie_children.cpp -o trie_children
 * 一个节点的子节点用三种方式存：定长数组、哈希表、有序表。
 * 同一份词表分别建三次，比较节点数、每键字节数与查找耗时；
 * 再与 unordered_set / map 的每键字节数对照。 */
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <new>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static long long g_calls = 0;
static long long g_bytes = 0;

void* operator new(std::size_t n) {
    ++g_calls;
    g_bytes += static_cast<long long>(n);
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

template <class F>
static double ms(F&& f) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// ---------- 一、定长数组：256 个位置一个不落 ----------
struct ArrayNode {
    ArrayNode* kids[256] = {};
    bool terminal = false;
};

// ---------- 二、哈希表：只存真正存在的边 ----------
struct HashNode {
    std::unordered_map<unsigned char, HashNode*> kids;
    bool terminal = false;
};

// ---------- 三、有序表：边有序，可以按字典序枚举 ----------
struct MapNode {
    std::map<unsigned char, MapNode*> kids;
    bool terminal = false;
};

template <class Node>
static void insert(Node* root, const std::string& word) {
    Node* p = root;
    for (char ch : word) {
        Node*& slot = p->kids[static_cast<unsigned char>(ch)];
        if (!slot) slot = new Node{};
        p = slot;
    }
    p->terminal = true;
}

template <class Node>
static bool contains(const Node* root, const std::string& word) {
    const Node* p = root;
    for (char ch : word) {
        auto it = p->kids.find(static_cast<unsigned char>(ch));
        if (it == p->kids.end()) return false;
        p = it->second;
    }
    return p->terminal;
}

// 数组版本的 find 与另外两种写法不同，单独给一个
static bool contains_array(const ArrayNode* root, const std::string& word) {
    const ArrayNode* p = root;
    for (char ch : word) {
        p = p->kids[static_cast<unsigned char>(ch)];
        if (!p) return false;
    }
    return p->terminal;
}

template <class Node>
static long long count_nodes(const Node* p) {
    long long n = 1;
    for (const auto& kv : p->kids) n += count_nodes(kv.second);
    return n;
}

static long long count_nodes_array(const ArrayNode* p) {
    long long n = 1;
    for (const ArrayNode* c : p->kids) {
        if (c) n += count_nodes_array(c);
    }
    return n;
}

template <class Node>
static void destroy(Node* p) {
    for (auto& kv : p->kids) destroy(kv.second);
    delete p;
}

static void destroy_array(ArrayNode* p) {
    for (ArrayNode* c : p->kids) {
        if (c) destroy_array(c);
    }
    delete p;
}

int main() {
    // 词表：500 个四字母前缀各接 40 个随机后缀，共 20000 条
    constexpr int kPrefixes = 500;
    constexpr int kPerPrefix = 40;
    std::mt19937 rng(2024);
    std::vector<std::string> words;
    words.reserve(kPrefixes * kPerPrefix);
    for (int i = 0; i < kPrefixes; ++i) {
        std::string pre(4, 'a');
        for (int j = 0; j < 4; ++j) pre[static_cast<std::size_t>(j)] = static_cast<char>('a' + rng() % 26);
        for (int j = 0; j < kPerPrefix; ++j) {
            std::string w = pre;
            const int tail = 4 + static_cast<int>(rng() % 4);
            for (int t = 0; t < tail; ++t) w.push_back(static_cast<char>('a' + rng() % 26));
            words.push_back(w);
        }
    }
    std::printf("词表：%zu 条，平均长度 %.1f 个字符\n", words.size(),
                [&] {
                    double sum = 0;
                    for (const std::string& w : words) sum += static_cast<double>(w.size());
                    return sum / static_cast<double>(words.size());
                }());

    constexpr int Q = 200000;
    std::vector<std::string> queries(Q);
    for (int i = 0; i < Q; ++i) queries[static_cast<std::size_t>(i)] = words[rng() % words.size()];

    // ---- 定长数组 ----
    g_calls = g_bytes = 0;
    ArrayNode* a_root = new ArrayNode{};
    const double a_ins = ms([&] { for (const std::string& w : words) insert(a_root, w); });
    const long long a_calls = g_calls, a_bytes = g_bytes;
    const long long a_nodes = count_nodes_array(a_root);
    volatile long long sink = 0;
    const double a_find = ms([&] {
        long long h = 0;
        for (const std::string& q : queries) h += contains_array(a_root, q) ? 1 : 0;
        sink += h;
    });
    const double a_find_ns = a_find * 1e6 / Q;

    // ---- 哈希表 ----
    g_calls = g_bytes = 0;
    HashNode* h_root = new HashNode{};
    const double h_ins = ms([&] { for (const std::string& w : words) insert(h_root, w); });
    const long long h_calls = g_calls, h_bytes = g_bytes;
    const long long h_nodes = count_nodes(h_root);
    const double h_find = ms([&] {
        long long h = 0;
        for (const std::string& q : queries) h += contains(h_root, q) ? 1 : 0;
        sink += h;
    });
    const double h_find_ns = h_find * 1e6 / Q;

    // ---- 有序表 ----
    g_calls = g_bytes = 0;
    MapNode* m_root = new MapNode{};
    const double m_ins = ms([&] { for (const std::string& w : words) insert(m_root, w); });
    const long long m_calls = g_calls, m_bytes = g_bytes;
    const long long m_nodes = count_nodes(m_root);
    const double m_find = ms([&] {
        long long h = 0;
        for (const std::string& q : queries) h += contains(m_root, q) ? 1 : 0;
        sink += h;
    });
    const double m_find_ns = m_find * 1e6 / Q;

    // ---- 对照：unordered_set 与 map ----
    g_calls = g_bytes = 0;
    std::unordered_set<std::string> us(words.begin(), words.end());
    const long long us_bytes = g_bytes;
    const double us_find = ms([&] {
        long long h = 0;
        for (const std::string& q : queries) h += us.count(q);
        sink += h;
    });

    g_calls = g_bytes = 0;
    std::map<std::string, int> mp;
    for (const std::string& w : words) mp[w] = 1;
    const long long mp_bytes = g_bytes;
    const double mp_find = ms([&] {
        long long h = 0;
        for (const std::string& q : queries) h += mp.find(q) != mp.end() ? 1 : 0;
        sink += h;
    });

    std::printf("\n%-22s %10s %14s %14s %14s\n", "", "节点数", "分配字节", "每键字节", "查找 ns/次");
    std::printf("%-22s %10lld %14lld %14.1f %14.1f\n", "trie（256 定长数组）", a_nodes, a_bytes,
                double(a_bytes) / words.size(), a_find_ns);
    std::printf("%-22s %10lld %14lld %14.1f %14.1f\n", "trie（哈希表）", h_nodes, h_bytes,
                double(h_bytes) / words.size(), h_find_ns);
    std::printf("%-22s %10lld %14lld %14.1f %14.1f\n", "trie（有序表）", m_nodes, m_bytes,
                double(m_bytes) / words.size(), m_find_ns);
    std::printf("%-22s %10s %14lld %14.1f %14.1f\n", "unordered_set<string>", "—", us_bytes,
                double(us_bytes) / words.size(), us_find * 1e6 / Q);
    std::printf("%-22s %10s %14lld %14.1f %14.1f\n", "map<string,int>", "—", mp_bytes,
                double(mp_bytes) / words.size(), mp_find * 1e6 / Q);

    std::printf("\n建表耗时（毫秒）：定长数组 %.3f，哈希表 %.3f，有序表 %.3f\n",
                a_ins, h_ins, m_ins);
    std::printf("分配次数：定长数组 %lld（节点 %lld），哈希表 %lld，有序表 %lld\n",
                a_calls, a_nodes, h_calls, m_calls);
    std::printf("（sink=%lld）\n", static_cast<long long>(sink));

    destroy_array(a_root);
    destroy(h_root);
    destroy(m_root);
    return 0;
}
```

`实测数据`
`Text`

```text
词表：20000 条，平均长度 9.5 个字符

                        节点数   分配字节   每键字节  查找 ns/次
trie（256 定长数组）     101041      207740296        10387.0          123.4
trie（哈希表）        101041       17438864          871.9          549.9
trie（有序表）        101041       10508216          525.4          255.9
unordered_set<string>         —        1284840           64.2           21.4
map<string,int>               —        1440000           72.0          116.0

建表耗时（毫秒）：定长数组 96.499，哈希表 13.352，有序表 8.846
分配次数：定长数组 101041（节点 101041），哈希表 283635，有序表 202081
（sink=1000000）
```

| 存法 | 节点数 | 每键字节 | 查找 ns/次 | 建表毫秒 | 特点 |
|---|---|---|---|---|---|
| 定长数组 | 101041 | **10387.0** | **123.4** | 96.499 | 定位最快，内存最贵 |
| 哈希表 | 101041 | 871.9 | 549.9 | 13.352 | 稀疏时省内存，定位要算哈希 |
| 有序表 | 101041 | **525.4** | 255.9 | **8.846** | 最省内存，输出天然有序 |
| `unordered_set<string>` | —— | 64.2 | 21.4 | —— | 只有等值查找 |
| `map<string,int>` | —— | 72.0 | 116.0 | —— | 有序，但给不了前缀枚举 |

三条结论：

- **节点数三者完全一样**（101041），因为结构相同，差别只在「子节点表怎么存」；
- **定长数组的每键 10387 字节**：十万个节点每个 2048 字节的指针数组，
  合计 207 MB——**一份两万条的词表吃掉两百兆内存**；
- **有序表的查找比哈希表还快**（255.9 对 549.9 ns），
  因为每个节点的孩子数很少（平均一两条边），
  `log d` 实际上只有一两次比较，而哈希表要为每个节点算一次哈希、还要处理桶。

> [!IMPORTANT]
> **trie 的取舍几乎全在「子节点表」这一处。**
> 字符集小且稠密（26 个小写字母、键几乎覆盖全部组合）用定长数组；
> 稀疏且要求速度用哈希表；**要求省内存或要求按字典序输出用有序表**。
> 三者可以按节点混用：根节点孩子多、用数组；深处节点孩子少、用有序表。

---

# 第 4 节 内存代价

`实测数据`
`Text`

```text
trie（有序表）        101041       10508216          525.4          255.9
unordered_set<string>         —        1284840           64.2           21.4
map<string,int>               —        1440000           72.0          116.0
```

**同一份两万条的词表：`unordered_set` 每键 64.2 字节，trie 每键 525.4 字节。**
差了八倍。这笔钱花在哪儿，可以从节点数直接算出来：

`Text`

```text
   两万条词，平均 9.5 个字符 → 十万个节点
   每个节点无论存法如何，都要有：一个 bool、若干指针、以及容器的头部开销
   有序表方案下每个节点约 104 字节（10508216 ÷ 101041）

   对比：unordered_set 每个键一个节点，两万个节点，每键 64.2 字节
```

**trie 的节点数由「不同前缀有多少个」决定，不由「键有多少个」决定。**
前缀共享得多（词表里大量词有共同前缀）时它省，
前缀几乎不共享（随机字符串）时它最贵——
每个键贡献自己的全部字符那么多节点。

> [!TIP]
> **判断该不该用 trie，先估一个数：所有键的不同前缀总数。**
> 如果这个数远大于键的个数（比如随机 ID），trie 的内存会失控；
> 如果接近键的个数乘平均长度（自然语言、代码标识符、URL），
> 那么前缀共享会让它比想象中省，**再加上前缀查询这个别人给不了的能力，就值了**。

---

# 第 5 节 按字节建还是按码点建

## 5.1 标准把字符串定义成元素的序列

`文档`

> "A string-literal that begins with `u8`, such as `u8"asdf"`, is a UTF-8 string literal."
> "Ordinary string literals and UTF-8 string literals are also referred to as narrow string
> literals. A narrow string literal has type 'array of `n` `const char`', where `n` is the size
> of the string as defined below."
> "For a UTF-8 string literal, each successive element of the object representation has the
> value of the corresponding **code unit** of the UTF-8 encoding of the string."
>
> —— N4659 §5.13.5/7、§5.13.5/8、§5.13.5/9

`文档`

> "**`sizeof(char)`, `sizeof(signed char)` and `sizeof(unsigned char)` are 1.**"
>
> —— N4659 §8.3.3/1

`文档`

> "The class template `basic_string` describes objects that can store a sequence consisting of
> a varying number of arbitrary char-like objects with the first element of the sequence at
> position zero. ... **A `basic_string` is a contiguous container**."
>
> —— N4659 §24.3.2/1、§24.3.2/3

**三句话连起来就是结论**：UTF-8 字面量是 `const char` 的数组，
`char` 的大小是 1，而 `std::string` 就是一串连续的 `char`。
**每个元素是一个字节，也就是 UTF-8 编码的一个码元，不是一个字符。**
一个汉字在 UTF-8 里占三个字节，因此在 `std::string` 里它占**三个元素**。

`Text`

```text
   std::string s = "数据";

   内存里的字节： E6 95 B0  E6 8D AE
                 └──┬──┘  └──┬──┘
                  「数」     「据」
                 3 个元素   3 个元素

   s.size() == 6      ← 不是 2
   s[0] 是 0xE6 这个字节，不是「数」这个字
```

## 5.2 两种建法的实测

`C++`

```cpp
/* trie_utf8.cpp    编译：g++ -std=c++17 -O2 trie_utf8.cpp -o trie_utf8
 * 同一份中文词表，按字节建一棵 trie、按码点再建一棵：
 * 节点数、内存、以及「拿半个字当前缀」时两边各自的反应。 */
#include <cstdio>
#include <cstdlib>
#include <map>
#include <new>
#include <string>
#include <vector>

static long long g_bytes = 0;
static long long g_calls = 0;

void* operator new(std::size_t n) {
    ++g_calls;
    g_bytes += static_cast<long long>(n);
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

// 子节点用有序表存：既能枚举，又能直接看到节点个数
struct Node {
    std::map<unsigned int, Node*> kids;      // 键是「一个单位」：字节或码点
    bool terminal = false;
};

// 把 UTF-8 串切成码点（这里够用：不处理非法序列与代理对）
static std::vector<unsigned int> to_codepoints(const std::string& s) {
    std::vector<unsigned int> out;
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        unsigned int cp = 0;
        int extra = 0;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else { cp = c & 0x07; extra = 3; }
        for (int k = 0; k < extra && i + 1 < s.size(); ++k) {
            ++i;
            cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
        }
        ++i;
        out.push_back(cp);
    }
    return out;
}

static void insert_units(Node* root, const std::vector<unsigned int>& units) {
    Node* p = root;
    for (unsigned int u : units) {
        Node*& slot = p->kids[u];
        if (!slot) slot = new Node{};
        p = slot;
    }
    p->terminal = true;
}

static void collect(const Node* p, std::vector<unsigned int>& cur,
                    std::vector<std::vector<unsigned int>>& out) {
    if (p->terminal) out.push_back(cur);
    for (const auto& kv : p->kids) {
        cur.push_back(kv.first);
        collect(kv.second, cur, out);
        cur.pop_back();
    }
}

static long long nodes(const Node* p) {
    long long n = 1;
    for (const auto& kv : p->kids) n += nodes(kv.second);
    return n;
}

static void destroy(Node* p) {
    for (auto& kv : p->kids) destroy(kv.second);
    delete p;
}

int main() {
    const std::vector<std::string> words = {
        "数据", "数据库", "数据结构", "数据中心", "数据流", "数据挖掘",
        "数组", "数值", "数字", "数组越界",
        "算法", "算力", "算式",
        "计算机", "计算", "计算机网络",
        "程序设计", "程序", "程序员",
        "网络", "网络协议", "网卡",
        "字符串", "字符", "字节", "字节序",
    };
    std::printf("词表：%zu 条中文词\n", words.size());

    // ---- 按字节建 ----
    g_bytes = g_calls = 0;
    Node* by_byte = new Node{};
    for (const std::string& w : words) {
        std::vector<unsigned int> units;
        for (char ch : w) units.push_back(static_cast<unsigned char>(ch));
        insert_units(by_byte, units);
    }
    const long long byte_nodes = nodes(by_byte);
    const long long byte_bytes = g_bytes;

    // ---- 按码点建 ----
    g_bytes = g_calls = 0;
    Node* by_cp = new Node{};
    for (const std::string& w : words) insert_units(by_cp, to_codepoints(w));
    const long long cp_nodes = nodes(by_cp);
    const long long cp_bytes = g_bytes;

    std::printf("\n%-14s %10s %12s %14s\n", "", "节点数", "分配字节", "每词字节");
    std::printf("%-14s %10lld %12lld %14.1f\n", "按字节建", byte_nodes, byte_bytes,
                double(byte_bytes) / words.size());
    std::printf("%-14s %10lld %12lld %14.1f\n", "按码点建", cp_nodes, cp_bytes,
                double(cp_bytes) / words.size());
    std::printf("汉字在 UTF-8 里占 3 个字节，因此按字节建的树要多走三层\n");

    // ---- 前缀「数」：按字节建走 3 步，按码点建走 1 步 ----
    const std::string prefix = "数";
    std::printf("\n== 前缀「数」：两边都能命中 ==\n");
    {
        const Node* p = by_byte;
        int steps = 0;
        bool ok = true;
        for (char ch : prefix) {
            auto it = p->kids.find(static_cast<unsigned char>(ch));
            if (it == p->kids.end()) { ok = false; break; }
            p = it->second;
            ++steps;
        }
        std::printf("  按字节建：走 %d 步%s（一个字 = 3 个字节 = 3 层）\n", steps, ok ? "命中" : "失败");
    }
    {
        const Node* p = by_cp;
        int steps = 0;
        bool ok = true;
        for (unsigned int u : to_codepoints(prefix)) {
            auto it = p->kids.find(u);
            if (it == p->kids.end()) { ok = false; break; }
            p = it->second;
            ++steps;
        }
        std::printf("  按码点建：走 %d 步%s（一个字 = 1 个码点 = 1 层）\n", steps, ok ? "命中" : "失败");
    }

    // ---- 前缀只给一半：4 个字节，「数」之后再接「据」的第一个字节 0xE6 ----
    std::string half = "数";
    half.push_back(static_cast<char>(0xE6));
    std::printf("\n== 前缀只给一半：\"数\" 加 0xE6（「据」的第一个字节）==\n");
    {
        const Node* p = by_byte;
        bool ok = true;
        for (char ch : half) {
            auto it = p->kids.find(static_cast<unsigned char>(ch));
            if (it == p->kids.end()) { ok = false; break; }
            p = it->second;
        }
        if (!ok) {
            std::printf("  按字节建：走不到（没有这条边）\n");
        } else {
            std::vector<unsigned int> cur;
            for (char ch : half) cur.push_back(static_cast<unsigned char>(ch));
            std::vector<std::vector<unsigned int>> out;
            collect(p, cur, out);
            std::printf("  按字节建：命中 %zu 条——半截字节也被当成了一条合法的路径\n", out.size());
            std::printf("  返回的都是「数」开头、第二个字的第一个字节恰好是 0xE6 的词：\n");
            for (const auto& u : out) {
                std::string raw;
                for (unsigned int b : u) raw.push_back(static_cast<char>(b));
                std::printf("    %s\n", raw.c_str());
            }
        }
    }
    {
        const std::vector<unsigned int> cps = to_codepoints(half);
        const Node* p = by_cp;
        bool ok = true;
        for (unsigned int u : cps) {
            auto it = p->kids.find(u);
            if (it == p->kids.end()) { ok = false; break; }
            p = it->second;
        }
        std::printf("  按码点建：这 4 个字节只解出 %zu 个码点（最后那个字节落单）——%s\n",
                    cps.size(), ok ? "命中" : "走不到");
    }

    destroy(by_byte);
    destroy(by_cp);
    return 0;
}
```

`实测数据`
`Text`

```text
词表：26 条中文词

                节点数 分配字节   每词字节
按字节建          113        14096          542.2
按码点建           40         4664          179.4
汉字在 UTF-8 里占 3 个字节，因此按字节建的树要多走三层

== 前缀「数」：两边都能命中 ==
  按字节建：走 3 步命中（一个字 = 3 个字节 = 3 层）
  按码点建：走 1 步命中（一个字 = 1 个码点 = 1 层）

== 前缀只给一半："数" 加 0xE6（「据」的第一个字节）==
  按字节建：命中 6 条——半截字节也被当成了一条合法的路径
  返回的都是「数」开头、第二个字的第一个字节恰好是 0xE6 的词：
    数据
    数据中心
    数据库
    数据挖掘
    数据流
    数据结构
  按码点建：这 4 个字节只解出 2 个码点（最后那个字节落单）——走不到
```

## 5.3 半个字当前缀会怎样

**最后一段是两种建法分歧最大的地方**：

- **按字节建的表对「半个字」也照样命中**，返回 6 条。
  这些词确实都以「数」开头，但它们是靠「第二个字节是 0xE6」筛出来的，
  与「第二个字是什么」没有关系——**如果输入的半个字节换一个值，
  筛出来的就是另一批毫不相干的词**；
- **按码点建的表对同样的输入直接走不到**：那 4 个字节解不出第二个完整的码点。

两边都不算「错」，但它们对「前缀」的定义不同：

| | 按字节建 | 按码点建 |
|---|---|---|
| 一层对应什么 | 一个字节 | 一个码点（一个字符） |
| 汉字占几层 | 3 层 | 1 层 |
| 节点数（同一份词表） | 113 | 40 |
| 每词字节 | 542.2 | 179.4 |
| 「数」这一个字 | 3 层 | 1 层 |
| 前缀能不能切在字中间 | **能，而且会给出结果** | 不能，直接走不到 |
| 删除一个词要动几层 | 键长 × 3 | 键长 |

> [!CAUTION]
> **要按「字」做前缀，就必须按码点建；按字节建的表根本不知道「字」是什么。**
> 按字节建并不是永远错——**只要你的「前缀」定义就是字节前缀**
> （例如按 URL 路径、按协议字段做分发），它反而更简单也更快。
> 错的是**需求说「字」、实现按「字节」**：
> 那时删除、统计、按字高亮、按字分页都会在字中间断开，
> 而且因为两边都能跑出结果，问题不会自己暴露。

> [!TIP]
> **判断办法只有一句：用户敲进来的「一个字」在实现里是几个元素。**
> 中文、日文、emoji 在 UTF-8 里都是多个字节；
> 只要界面上按字符计数（长度限制、光标位置、高亮），
> 底层就必须按码点处理。
> 中文在 C 风格字符串与 `std::string` 里各占几个字节、字节值长什么样，
> 见《04-语法/10-字符串.md》第 1.3 小节。

---

# 第 6 节 变体：压缩 trie 与 radix 树

上面那种「一个单位一层」的 trie 有两个明显的浪费：
**只有一个孩子的节点会串成一条长链**（例如 `careful` 那条 `f → u → l`），
以及**每个节点都要存一份子节点表**。

**压缩 trie**（也叫 radix 树、PATRICIA 树）把只有一个孩子的连续节点合并成一个节点，
节点里存一段字符串而不是一个单位：

`Text`

```text
   普通 trie：            c → a → r → e → f → u → l
   压缩后：              "careful"（一个节点存整段）

   分支的地方才拆节点：
                        (根)
                       /    \
                    "car"    "dog"
                   /  |  \
                "d"  "e"  "t"
                     │
                   "ful"
```

合并之后**节点数只与分支数有关**，与键的总长度无关；
代价是节点里要存变长的字符串，比较时要处理「一段不匹配」的情形
（可能需要把一个节点再拆开）。现实中的实现（例如路由表、IP 前缀匹配）
多用这个变体。**本章不展开它**——它要处理分裂与合并的边界，属于实现层面的细节。

---

# 第 7 节 怎么选

| 你的需求 | 选择 | 理由 |
|---|---|---|
| 按前缀枚举、自动补全 | **trie** | 前缀是它的一等公民 |
| 只要等值查找 | `unordered_set` / `unordered_map` | 每键 64.2 字节对 525.4 字节 |
| 要按序遍历、也要前缀查询 | `std::map` + `lower_bound` | 前缀查询能写出来，但每次比较整个键 |
| 前缀查询很少、内存很紧 | `std::map` | 为偶尔一次的查询付八倍内存不值得 |
| 键是随机字符串（前缀几乎不共享） | **不要用 trie** | 节点数会接近键的总长度 |
| 要按「字符」做前缀，键含中文 | trie + **按码点建** | 按字节建会把字拆开 |
| 前缀总量很大、节点大量单链 | 压缩 trie / radix 树 | 合并单链节点，节点数只与分支数有关 |

> [!NOTE]
> 本章的结论归纳成三句话：
> **trie 把「比较键」换成「沿路径走」，因此前缀查询的代价只与前缀长度有关，
> 与表里有多少个键无关；
> 它的代价在内存上——每个节点的子节点表怎么存决定了每键字节数，
> 本机实测从 525.4（有序表）到 10387.0（定长数组），是哈希表的八倍以上；
> 建之前先定一件事：一层对应一个字节还是一个字符——
> 中文在 UTF-8 里占三个字节，按字节建会把一个字拆成三层。**

---

> [!TIP]
> **配套演示**：《A-教学素材/演示工具/数据结构演示/》里的「trie」场景能看见同一批中文词按码点建与按 UTF-8 字节建，两棵树的形状差多少。
> 它的 `--selftest` 输出（C#／.NET Framework 4.8）给出：中文 5 个词：按码点 8 个节点、深 2 层；按 UTF-8 字节 21 个节点、深 6 层。
> **这组数字与本章的 C++ 实测不是同一套工具链，两组不要放进同一张表里比。**

# 术语表

| 术语 | 含义 |
|---|---|
| **trie** | 按键的逐单位路径组织的树；也叫前缀树、字典树 |
| **前缀** | 某个键开头的一段；不要求它本身是一个键 |
| **词尾标记** | 节点上的一个标记，表示「从根走到这里是一个完整的键」 |
| **扇出／分支数** | 一个节点有多少个子节点 |
| **码点** | 一个字符的编号；`char32_t` 能直接存一个码点 |
| **码元** | 编码的最小单位；UTF-8 的码元是一个字节 |
| **UTF-8** | 一种变长编码；ASCII 占 1 字节，汉字通常占 3 字节 |
| **压缩 trie** | 把只有一个孩子的连续节点合并成一个节点的变体；也叫 radix 树 |

# 附录 A 复现本章节实测

**环境**：Windows 11，g++ 15.2.0（MinGW-w64）。
编译命令：`g++ -std=c++17 -O2 <源文件> -o <可执行文件>`。

| 本章节的数字 | 程序 | 在哪一小节 |
|---|---|---|
| 整词与前缀的差别、前缀枚举、删除回收 | `trie_basic.cpp` | 第 2.2、2.3 小节 |
| 三种子节点存法的节点数／每键字节／查找耗时 | `trie_children.cpp` | 第 3.4 小节 |
| 与 `unordered_set`、`map` 的内存对照 | `trie_children.cpp` | 第 3.4、4 节 |
| 按字节建与按码点建的节点数、内存、前缀步数 | `trie_utf8.cpp` | 第 5.2、5.3 小节 |

**几点复现说明**：

- 两份程序都用 `operator new` 计数得出「分配字节」，
  它包含容器自身与节点的开销，是**实际向堆申请的字节数**；
- `trie_children.cpp` 的词表是程序按固定种子生成的（500 个四字母前缀各接 40 个随机后缀），
  重跑结果一致；真实词表的前缀共享程度不同，节点数会跟着变；
- 节点数与每键字节是结构量，重跑稳定；查找耗时每次重跑会浮动；
- 内存数字随标准库实现变化：本机是 libstdc++，`std::map` 与 `std::unordered_map`
  的节点大小见《09-高阶数据结构/A-03-索引存储：map、set 与有序.md》第 6.2 小节。

`待确认`

- 本章的程序是为了测量而写的最小实现：没有内存池、没有序列化、
  没有并发保护；这些工程做法不在本章范围内；
- 「按码点建」的切分函数只处理合法 UTF-8，
  遇到非法字节序列或代理对时给出的是残缺值（第 5.2 小节的实测里就能看到这一点），
  生产中应当用成熟库做解码；
- 真实词表（自然语言、URL、代码标识符）上前缀共享比合成词表更强，
  trie 的相对内存开销会比上面低。本机没有可比对的真实词表，
  因此**「trie 是哈希表的八倍」这个倍数只对本章这份合成词表成立**。
