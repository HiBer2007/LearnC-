# 树（二）：数据在磁盘上——B 树与 B+ 树

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

上一章把树高压到了 `1.44 log₂ n`（十万个键的上界约 24 层），实测的 AVL 十万个键只要 17 层。
那套账是按**内存**算的：一次节点访问大约几十纳秒，二十层跳下来还不到一微秒。
把同一棵树搬到磁盘上，账就完全变了——**每一层都可能是一次磁盘读取**，
而一次读取的代价是几十微秒到几毫秒，比一次内存访问贵三到五个数量级（见第 1.2 小节）。
二十层意味着二十次读取，这个数量级任何优化都救不回来。

于是需求发生了根本变化：**不再追求「比较次数少」，而是追求「层数少」。**
比较次数多几百次无所谓（内存里的比较便宜），
但每多一层就多一次 I/O，这一层必须省。
把每个节点从「装 1 个键、2 个孩子」改成「装几百个键、几百个孩子」，
树高就能从二十层降到三层——这就是多路树，B 树与 B+ 树都属于这一类。

这一章的实测对象不是磁盘：本机没有可控的磁盘 I/O 环境，
所以改用**「一个节点算一页」的模型**来数一次查找要碰多少页，
再与 `std::map`（每个节点独立分配、每层必然换一个节点）对照。
模型按「一个节点占一页」计——**这对 `std::map` 是最坏情形**：它的节点只有 40 字节，
一页装得下几十个，这条假设会把它的页数放大两个数量级（第 4.2 小节）。
数出来的是页访问次数，不是耗时。
结论是：一百万个键，`std::map` 要碰 21.4 页，B+ 树只要 3 页；
取一万个连续键，`std::map` 要碰 10001 页，B+ 树沿着叶子链走只要 43 页。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 复现命令随程序给出；`文档` 表示引自标准草案或官方资料；`待确认` 表示尚未验证。
> 路径占位符（如 `<MinGW>`、`<版本号>`）的含义见《README.md》。
> 本章节实测环境：Windows 11 + g++ 15.2.0（MinGW-w64）。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 二叉搜索树、树高与旋转 | 《09-高阶数据结构/A-07-树（一）：从搜索树到平衡.md》第 2、3 节 |
| `map` 的每元素开销与迭代器类别 | 《09-高阶数据结构/A-03-索引存储：map、set 与有序.md》第 3.5、6.2 小节 |
| 数组与下标访问 | 《04-语法/08-数组、指针与引用.md》第 2 节 |
| 文件与流的基本读写 | 《07-标准库/A-01-输入输出：stdio.md》第 4 节 |

**相邻的章节**：上一章 `A-07` 讲内存里的平衡树，本章把「一次访问很贵」
这个前提换成「一次 I/O 更贵」，结构就跟着换成多路树。
`A-09` 换另一个需求（按前缀找），引出 trie。
数据库一侧的使用（索引怎么建、怎么做范围扫描）不在本教材范围内，
本章只讲结构本身；**怎么把这棵树写对、写稳见【待补：09-高阶数据结构/B-04-手写：平衡树.md】**，
本章的程序是为了测量而写的最小实现。

| 本章各节 | 讲什么 |
|---|---|
| 第 1 节 | 需求变了：一次读一页；二叉树太深；比较与 I/O 的代价差多少个量级 |
| 第 2 节 | 多路树：一个节点放多个键；扇出与高度的关系（实测表）；节点大小向页看齐 |
| 第 3 节 | B 树与 B+ 树的差别；叶子成链为什么让范围查询快 |
| 第 4 节 | 一次查找碰多少页的实测；与 `std::map` 的对照；这是模拟的说明 |
| 第 5 节 | 数据库与文件系统为什么用它 |
| 第 6 节 | 判据：什么时候需要多路树，什么时候二叉树就够 |

---

# 第 1 节 需求变了：一次读一页

## 1.1 二叉树在磁盘上太深

本小节程序第二段测出来的数：一百万个键，`std::map` 平均每次查找要比较 21.4 次
（程序 `btree_pages.cpp`，见第 4.1 小节）。
每一次比较发生在某一个节点上，也就是**要走 21.4 个节点**。
在内存里这不算什么，二十一次跳转而已；但换成磁盘：

`Text`

```text
   内存里的树（每个节点一次内存访问）        磁盘上的树（每个节点可能是一次 I/O）

        根  20 ns                               根  约 100 微秒
        │                                       │
        ├─ 第 2 层  20 ns                       ├─ 第 2 层  约 100 微秒
        │                                       │
       ...  共 21 层                            ...  共 21 层

   合计约 0.4 微秒                             合计约 2 毫秒
```

**同样的形状，按上面这张图的取值算，代价差了五千倍。** 差在每一次访问上，不在层数上——
所以「把层数压下来」成了唯一有效的方向。

## 1.2 一次比较与一次 I/O 的差价

| 动作 | 数量级 | 说明 |
|---|---|---|
| 一次内存访问（缓存命中） | 1 到 10 纳秒 | 二叉树的一层 |
| 一次内存访问（缓存未命中） | 50 到 100 纳秒 | 节点散落时的常见情形 |
| 一次 SSD 随机读 | 约 100 微秒 | 大约是内存的 1000 倍 |
| 一次机械硬盘随机读 | 5 到 10 毫秒 | 大约是内存的 10 万倍 |

> [!WARNING]
> **存疑：上表是量级参考，不是本机实测。**
> 内存与 SSD 的延迟随机器、型号与负载变化，机械硬盘从 5 毫秒到 15 毫秒都有可能，
> 这里给不出统一取值，读者要以自己机器上的测量为准。
> 本章的结论不依赖表里取哪个具体值——它依赖的是
> 「一次 I/O 比一次内存访问贵几个数量级」这一个量级关系。

> [!IMPORTANT]
> **在磁盘上，「多比较几次」几乎不要钱，「多读一次」贵得离谱。**
> 这个交换比决定了结构的方向：宁可让每个节点大一些、多装一些键，
> 也要让树矮下去。B 树就是把这个交换做到极致的形状。

## 1.3 页：读写的最小单位

磁盘不是按字节读的，它按**页**（page，也叫块）读，常见的页大小是 4 KiB。
哪怕只想要一个字节，也得把整页读进来。于是「一个节点」最自然的做法是
**让它占满一页**：

`Text`

```text
   一页 4096 字节
   ┌──────────────────────────────────────────────────────────┐
   │  键 1  指针 1  键 2  指针 2  ...  键 k  指针 k+1         │
   └──────────────────────────────────────────────────────────┘
     ↑                     ↑
     一次 I/O 把这一整页读进来，里面的键就都能用了
```

**节点大小向页大小看齐**，这是多路树与二叉树最根本的区别：
二叉树的节点里只有 1 个键，一个节点一页就是极大的浪费；
多路树把一页塞满，一次 I/O 换来几百次比较。

---

# 第 2 节 多路树：把树压矮

## 2.1 一个节点放多个键

多路树的节点是「一排排好序的键 + 每两个键之间一个孩子」：

`Text`

```text
   一个最多 4 个键、5 个孩子的内部节点

        ┌──────┬──────┬──────┬──────┐
        │  10  │  20  │  30  │  40  │        键按大小排好
        └──┬───┴──┬───┴──┬───┴──┬───┴──┬───┘
           │      │      │      │      │
           ▼      ▼      ▼      ▼      ▼
        < 10  10~20  20~30  30~40  > 40     每个孩子负责一段区间
```

**扇出**（fanout）就是一个节点最多有几个孩子。
扇出 2 就是二叉树；扇出 300 意味着树高大约只有 `log₃₀₀ n`。

## 2.2 扇出与高度的关系

`实测数据`
`Text`

```text
=== 一、扇出与树高：一次查找要碰多少页（1000000 个键，一页 = 4096 字节）===

每个节点最多     节点估字节数  每节点占几页   树高   一次查找碰到的页数
         4               96            1     11               11.0
        16              240            1      6                6.0
        64              816            1      4                4.0
       256             3120            1      3                3.0
       336             4080            1      3                3.0
      1024            12336            4      3               12.0
```

**一百万个键，每节点最多 4 个键（扇出 5）要 11 层，最多 256 个键（扇出 257）只要 3 层。**
节点装得下一页时，「一次查找碰到的页数」就等于树高——每层一次 I/O。

表里有两处细节：

- **最后一行的 1024 个键也只要 3 层，但它装不进一页**（估算 12336 字节 ≈ 3 页；
  按 ⌈节点字节 ÷ 页大小⌉ 算，一个节点占 4 页）。
  节点太大，一次 I/O 读不完，多出来的部分要接着读，因此那一行的页数不是 3 而是 12——
  **扇出不是越大越好，它的上限由页大小决定。**
- 每节点 256 个键与 336 个键都是 3 层，但 336 更省 I/O 余量：
  节点越接近页大小，同一层能容纳的键越多，树越不容易再长高一层。

## 2.3 节点大小怎么估算

上面的「节点估字节数」按下面这个式子算，它决定了扇出取多少：

`Text`

```text
   节点字节数 ≈ (最大键数 + 1) × 指针大小 + 最大键数 × 键大小 + 头部开销

   例：最大键数 336，指针 8 字节，键 4 字节
       (336 + 1) × 8 + 336 × 4 + 40 = 2696 + 1344 + 40 = 4080 字节 ≤ 4096 ✓
```

> [!TIP]
> **实际系统里的页大小与键大小都要按真实情况取。**
> 键是 `int` 时一页能装三百多个；键是 32 字节的字符串时只能装一百个左右，
> 树高相应多一层。**这就是为什么数据库的索引列越短越好。**

---

# 第 3 节 B 树与 B+ 树

## 3.1 B 树：每个节点都存数据

B 树的规则可以概括成三条：

| 规则 | 作用 |
|---|---|
| 每个节点最多 `M - 1` 个键、`M` 个孩子 | 节点大小有上限，一页装得下 |
| 除根以外，每个节点至少半满 | 空间不浪费，树不会长得又高又瘦 |
| 所有叶子在同一层 | 树是「等高的」，查找路径长度一致 |

**B 树把数据存在所有节点里**（内部节点也有数据），
插入时如果节点满了就从中间裂开，把中间的键提到父节点——
这与上一章讲过的旋转是同一个思路的另一种做法：**结构调整只影响局部。**

## 3.2 B+ 树：数据都放在叶子上

B+ 树是 B 树的一个变体，它把两条规则改掉：

| | B 树 | B+ 树 |
|---|---|---|
| 数据存在哪 | 所有节点 | **只在叶子** |
| 内部节点存什么 | 键 + 数据 | **只有键**（当路标用） |
| 同一个键会不会出现两次 | 不会 | **会**：内部节点有一份路标，叶子有一份数据 |
| 叶子之间 | 无连接 | **串成一条链** |
| 一次查找的层数 | 可能提前命中 | 一定要走到叶子 |

**内部节点只存键，因此一页能装下更多路标**——同样的页大小，
B+ 树的扇出比 B 树更大，树更矮。多走一层到叶子，
换来的是每层能装更多键，这笔交换在页大小固定时是划算的。

## 3.3 叶子成链：范围查询快在哪

B+ 树的叶子串成一条链，于是「取 `[a, b)` 这一段」变成两步：
**先走到 `a` 所在的叶子，再顺着链一直读下去。**

`实测数据`
`Text`

```text
=== 三、范围查询：连续取 10000 个键要碰多少页 ===

B+ 树（顺着叶子链走）：取到 10000 个键，碰 43 页
std::map（迭代器逐个跳）：取到 10000 个键，碰 10001 页
```

**43 页对 10001 页，差 233 倍。** 原因是两边的「一页」含义不同：

- B+ 树的一页里装着几百个键，顺着叶子链读 43 页就覆盖了一万个键；
- `std::map` 的每个元素是独立分配的一个节点，迭代器每 `++` 一次就换一个节点——
  **取一万个键就要碰一万个节点**。

`文档`

> "iterator of an associative container is of the bidirectional iterator category."
>
> —— N4659 §26.2.6/6

`std::map` 的迭代器是双向的，只能一步步 `++`；
B+ 树的叶子链是**顺序访问**，这正是磁盘最擅长的事：
顺序读一页的代价远低于随机读一页，而叶子链上的页正好是连续的。

> [!IMPORTANT]
> **B+ 树之所以在数据库里压倒 B 树，主要不是「查找更快」，而是「范围查询更快」。**
> 它的叶子链把「取一段区间」从「一万次随机访问」变成了「四十三次顺序访问」。
> 现实中的查询大量是范围查询（时间区间、ID 区间、分页），
> 这一条就决定了两者的胜负。

## 3.4 两种树的差别值多少页

「数据存在哪」与「叶子有没有链」这两条差别各自带来多少页的差距，可以分开测。
等值查找这一侧用同一份数据各建一棵树：一百万个键、每节点最多 336 个键、
同一批十万次查询，程序里的种子与 `btree_pages.cpp` 完全一致，因此两边可以直接比。

`C++`

```cpp
/* btree_lookup.cpp    编译：g++ -std=c++17 -O2 btree_lookup.cpp -o btree_lookup
 * B 树（数据存在所有节点里）的一次查找要碰多少页：
 * 与 btree_pages.cpp 里那棵 B+ 树用同一份数据、同一个扇出、同一批查询，直接对照。
 * 「一个节点算一页」仍然是模型，数出来的是页访问次数，不是耗时。 */
#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>

struct BNode {
    std::vector<int> keys;          // 内部节点与叶子都存数据
    std::vector<BNode*> kids;
    bool leaf = true;
};

class BTree {
public:
    explicit BTree(int max_keys) : max_keys_(max_keys) {}
    ~BTree() { destroy(root_); }

    void insert(int k) {
        if (!root_) root_ = new BNode{};
        Split s = insert_rec(root_, k);
        if (s.right) {                     // 根裂开了，长高一层
            BNode* r = new BNode{};
            r->leaf = false;
            r->keys.push_back(s.sep);
            r->kids.push_back(root_);
            r->kids.push_back(s.right);
            root_ = r;
        }
    }

    // 返回是否找到；pages 累加碰到的节点数；early 记录是不是在内部节点就命中了
    bool find(int k, long long& pages, bool& early) const {
        const BNode* p = root_;
        while (p) {
            ++pages;
            const auto it = std::lower_bound(p->keys.begin(), p->keys.end(), k);
            if (it != p->keys.end() && *it == k) {
                early = !p->leaf;          // 数据存在内部节点里，因此可能提前命中
                return true;
            }
            if (p->leaf) return false;
            p = p->kids[static_cast<std::size_t>(it - p->keys.begin())];
        }
        return false;
    }

    long long nodes() const { return count_nodes(root_); }
    int height() const { return root_ ? 1 + height_of(root_->kids.empty() ? nullptr : root_->kids[0]) : 0; }
    long long internal_keys() const { return internal_key_count(root_); }

private:
    struct Split {
        int sep = 0;
        BNode* right = nullptr;
    };

    Split insert_rec(BNode* p, int k) {
        const auto it = std::lower_bound(p->keys.begin(), p->keys.end(), k);
        if (it != p->keys.end() && *it == k) return {};          // 已经有了
        if (p->leaf) {
            p->keys.insert(it, k);
            if (static_cast<int>(p->keys.size()) <= max_keys_) return {};
            return split(p);
        }
        const std::size_t idx = static_cast<std::size_t>(it - p->keys.begin());
        Split s = insert_rec(p->kids[idx], k);
        if (!s.right) return {};
        p->keys.insert(p->keys.begin() + static_cast<std::ptrdiff_t>(idx), s.sep);
        p->kids.insert(p->kids.begin() + static_cast<std::ptrdiff_t>(idx) + 1, s.right);
        if (static_cast<int>(p->keys.size()) <= max_keys_) return {};
        return split(p);
    }

    // 分裂：中间的键提上去，它自己离开这个节点（B+ 树提上去的是复制品）
    Split split(BNode* p) {
        BNode* right = new BNode{};
        right->leaf = p->leaf;
        const std::size_t mid = p->keys.size() / 2;
        const int up = p->keys[mid];
        right->keys.assign(p->keys.begin() + static_cast<std::ptrdiff_t>(mid) + 1, p->keys.end());
        p->keys.resize(mid);
        if (!p->leaf) {
            right->kids.assign(p->kids.begin() + static_cast<std::ptrdiff_t>(mid) + 1, p->kids.end());
            p->kids.resize(mid + 1);
        }
        return Split{up, right};
    }

    static long long count_nodes(const BNode* p) {
        if (!p) return 0;
        long long n = 1;
        for (const BNode* c : p->kids) n += count_nodes(c);
        return n;
    }

    static long long internal_key_count(const BNode* p) {
        if (!p || p->leaf) return 0;
        long long n = static_cast<long long>(p->keys.size());
        for (const BNode* c : p->kids) n += internal_key_count(c);
        return n;
    }

    static int height_of(const BNode* p) { return p ? 1 + height_of(p->kids.empty() ? nullptr : p->kids[0]) : 0; }

    static void destroy(BNode* p) {
        if (!p) return;
        for (BNode* c : p->kids) destroy(c);
        delete p;
    }

    BNode* root_ = nullptr;
    int max_keys_;
};

int main() {
    // 与 btree_pages.cpp 完全相同的规模、扇出、种子与查询序列
    constexpr int N = 1000000;
    constexpr int Q = 100000;
    constexpr int M = 336;
    std::mt19937 rng(2024);
    std::vector<int> keys(N);
    for (int i = 0; i < N; ++i) keys[i] = i;
    std::shuffle(keys.begin(), keys.end(), rng);
    std::vector<int> queries(Q);
    for (int& q : queries) q = static_cast<int>(rng() % static_cast<unsigned>(N));

    BTree t(M);
    for (int k : keys) t.insert(k);

    long long pages = 0;
    long long early_hits = 0;
    long long hits = 0;
    for (int q : queries) {
        bool early = false;
        if (t.find(q, pages, early)) {
            ++hits;
            if (early) ++early_hits;
        }
    }

    std::printf("B 树（每节点最多 %d 个键，%d 个键）：树高 %d，共 %lld 个节点\n",
                M, N, t.height(), t.nodes());
    std::printf("  %d 次查询命中 %lld 次，共碰 %lld 页，平均每次查找碰 %.3f 页\n",
                Q, hits, pages, static_cast<double>(pages) / Q);
    std::printf("  其中 %lld 次在内部节点就命中（省掉最后那一次到叶子的页访问）\n", early_hits);
    std::printf("  内部节点里存着的键共 %lld 个，占全部键的 %.3f%%\n",
                t.internal_keys(), 100.0 * static_cast<double>(t.internal_keys()) / N);
    return 0;
}
```

`实测数据`
`Text`

```text
B 树（每节点最多 336 个键，1000000 个键）：树高 3，共 4317 个节点
  100000 次查询命中 100000 次，共碰 299580 页，平均每次查找碰 2.996 页
  其中 419 次在内部节点就命中（省掉最后那一次到叶子的页访问）
  内部节点里存着的键共 4299 个，占全部键的 0.430%
```

| | 树高 | 节点数 | 一次查找碰到的页数 | 在内部节点就命中 |
|---|---|---|---|---|
| B 树 | 3 | 4317 | **2.996** | 419 次 |
| B+ 树（第 4.1 小节同一份数据） | 3 | 4330 | 3.000 | 不可能 |

**「B 树能提前命中」这句话是对的，但它只值 0.004 页。**
原因是内部节点里存着的键只有 4299 个，占全部键的 0.430%——
**扇出越大，能提前命中的比例越低**：内部节点的键数只与节点数有关，
而总键数随数据量线性增长。
数据量再涨十倍，这个比例还会掉一个数量级。

**范围查询那一侧则是另一回事。** B+ 树顺着叶子链走，取一万个连续键碰 43 页；
B 树没有叶子链，只有两条路：

| B 树取一万个连续键的做法 | 代价 |
|---|---|
| 对每个键各查一次 | 约 `10000 × 3 = 30000` 页 |
| 自己维护一条回退用的栈，按中序一个个取 | 页数与 B+ 树接近，但要额外维护栈，而且**取完一段之后没法接着往下走** |

> [!IMPORTANT]
> **B+ 树在数据库里胜出，靠的不是等值查找更快，而是范围查询更省。**
> 实测的两组数字：等值查找上 B 树省 0.004 页（0.13%），
> 范围查询上 B+ 树省 233 倍（43 页对 10001 页）。
> 现实查询里范围条件（时间区间、ID 区间、分页、排序）占很大比重，
> 这一条就决定了两者的胜负。

---

# 第 4 节 一次查找要碰多少页

## 4.1 与 `std::map` 的对照

`C++`

```cpp
/* btree_pages.cpp    编译：g++ -std=c++17 -O2 btree_pages.cpp -o btree_pages
 * 磁盘上的一次查找要碰多少页：用一个最小的 B+ 树数页访问次数，
 * 再与 std::map（每层一个节点、每个节点一页）对照。
 * 注意：本机没有可控的磁盘 I/O，这里把「一个节点算一页」当作模型来数，
 * 数出来的是一次查找触碰的页数，不是耗时。 */
#include <algorithm>
#include <cstdio>
#include <map>
#include <random>
#include <vector>

static constexpr int kPageBytes = 4096;

// 一个节点估多少字节：孩子指针数组 + 键数组 + 头部
static long long node_bytes(int max_keys) {
    return static_cast<long long>(max_keys + 1) * 8 + static_cast<long long>(max_keys) * 4 + 40;
}

// ------------------------- 最小 B+ 树 -------------------------
struct Node {
    bool leaf = true;
    std::vector<int> keys;        // 叶子：数据键；内部：分隔键（keys[i] 是 kids[i+1] 的最小键）
    std::vector<Node*> kids;      // 内部节点的孩子
    Node* next = nullptr;         // 叶子链：范围查询靠它
};

class BPlus {
public:
    explicit BPlus(int max_keys) : max_keys_(max_keys) {}
    ~BPlus() { destroy(root_); }

    void insert(int k) {
        if (!root_) root_ = new Node{};
        Split s = insert_rec(root_, k);
        if (s.right) {
            Node* r = new Node{};
            r->leaf = false;
            r->keys.push_back(s.sep);
            r->kids.push_back(root_);
            r->kids.push_back(s.right);
            root_ = r;
        }
    }

    // 返回是否找到；pages 累加这次查找碰到的节点数
    bool find(int k, long long& pages) const {
        const Node* p = root_;
        while (p) {
            ++pages;                                  // 读入一页
            if (p->leaf) return std::binary_search(p->keys.begin(), p->keys.end(), k);
            p = p->kids[child_index(p, k)];
        }
        return false;
    }

    // 从 start 开始取 count 个键，返回碰到的页数，取到的个数写入 got
    long long range_pages(int start, int count, long long& got) const {
        long long pages = 0;
        got = 0;
        const Node* p = root_;
        while (p && !p->leaf) {
            ++pages;
            p = p->kids[child_index(p, start)];
        }
        if (!p) return pages;
        ++pages;                                       // 第一个叶子
        while (p && got < count) {
            for (int k : p->keys) {
                if (k >= start && got < count) { ++got; }
            }
            if (got >= count) break;
            if (p->next) ++pages;                      // 顺着叶子链读下一页
            p = p->next;
        }
        return pages;
    }

    long long nodes() const { return count_nodes(root_); }
    int height() const { return height_of(root_); }
    int max_keys() const { return max_keys_; }

private:
    struct Split {
        int sep = 0;
        Node* right = nullptr;
    };

    static int child_index(const Node* p, int k) {
        // 找最后一个满足 keys[i-1] <= k 的孩子
        const auto it = std::upper_bound(p->keys.begin(), p->keys.end(), k);
        return static_cast<int>(it - p->keys.begin());
    }

    Split insert_rec(Node* p, int k) {
        if (p->leaf) {
            const auto it = std::lower_bound(p->keys.begin(), p->keys.end(), k);
            if (it != p->keys.end() && *it == k) return {};
            p->keys.insert(it, k);
            if (static_cast<int>(p->keys.size()) <= max_keys_) return {};
            // 叶子分裂：右半搬走，左边留在原节点
            Node* right = new Node{};
            right->leaf = true;
            const int mid = static_cast<int>(p->keys.size()) / 2;
            right->keys.assign(p->keys.begin() + mid, p->keys.end());
            p->keys.resize(static_cast<std::size_t>(mid));
            right->next = p->next;                     // 维护叶子链
            p->next = right;
            return Split{right->keys.front(), right};
        }
        const int idx = child_index(p, k);
        Split s = insert_rec(p->kids[static_cast<std::size_t>(idx)], k);
        if (!s.right) return {};
        p->keys.insert(p->keys.begin() + idx, s.sep);
        p->kids.insert(p->kids.begin() + idx + 1, s.right);
        if (static_cast<int>(p->keys.size()) <= max_keys_) return {};
        // 内部节点分裂：中间的键提上去，不算在两边
        Node* right = new Node{};
        right->leaf = false;
        const int mid = static_cast<int>(p->keys.size()) / 2;
        const int up = p->keys[static_cast<std::size_t>(mid)];
        right->keys.assign(p->keys.begin() + mid + 1, p->keys.end());
        right->kids.assign(p->kids.begin() + mid + 1, p->kids.end());
        p->keys.resize(static_cast<std::size_t>(mid));
        p->kids.resize(static_cast<std::size_t>(mid) + 1);
        return Split{up, right};
    }

    static long long count_nodes(const Node* p) {
        if (!p) return 0;
        long long n = 1;
        for (const Node* c : p->kids) n += count_nodes(c);
        return n;
    }

    static int height_of(const Node* p) { return p ? 1 + height_of(p->kids.empty() ? nullptr : p->kids[0]) : 0; }

    static void destroy(Node* p) {
        if (!p) return;
        for (Node* c : p->kids) destroy(c);
        delete p;
    }

    Node* root_ = nullptr;
    int max_keys_;
};

// ------------------------- 对照：std::map -------------------------
struct CountingLess {
    bool operator()(int a, int b) const {
        ++calls;
        return a < b;
    }
    static long long calls;
};
long long CountingLess::calls = 0;

int main() {
    constexpr int N = 1000000;
    constexpr int Q = 100000;
    std::mt19937 rng(2024);
    std::vector<int> keys(N);
    for (int i = 0; i < N; ++i) keys[i] = i;
    std::shuffle(keys.begin(), keys.end(), rng);
    std::vector<int> queries(Q);
    for (int& q : queries) q = static_cast<int>(rng() % static_cast<unsigned>(N));

    std::printf("=== 一、扇出与树高：一次查找要碰多少页（%d 个键，一页 = %d 字节）===\n\n",
                N, kPageBytes);
    std::printf("每个节点最多     节点估字节数  每节点占几页   树高   一次查找碰到的页数\n");

    for (int m : {4, 16, 64, 256, 336, 1024}) {
        BPlus t(m);
        for (int k : keys) t.insert(k);
        long long pages = 0;
        for (int q : queries) t.find(q, pages);
        const double per = static_cast<double>(pages) / Q;
        // 模型：一个节点占 ⌈节点字节 / 页大小⌉ 页；装得下一页时就是 1 页
        const long long per_node = (node_bytes(m) + kPageBytes - 1) / kPageBytes;
        std::printf("%10d %16lld %12lld %6d %18.1f\n", m, node_bytes(m),
                    per_node, t.height(), per * static_cast<double>(per_node));
    }

    std::printf("\n=== 二、与 std::map 对照：同样是 %d 个键 ===\n\n", N);
    {
        std::map<int, int, CountingLess> m;
        for (int k : keys) m[k] = k;
        CountingLess::calls = 0;
        volatile long long sink = 0;
        long long hits = 0;
        for (int q : queries) hits += m.find(q) != m.end() ? 1 : 0;
        sink += hits;
        std::printf("std::map：平均每次查找比较 %.1f 次（每个节点一页，就是碰 %.1f 页）\n",
                    static_cast<double>(CountingLess::calls) / Q,
                    static_cast<double>(CountingLess::calls) / Q);
    }
    {
        BPlus t(336);
        for (int k : keys) t.insert(k);
        long long pages = 0;
        for (int q : queries) t.find(q, pages);
        std::printf("B+ 树（每个节点最多 336 个键）：树高 %d，平均每次查找碰 %.1f 页，共 %lld 个节点\n",
                    t.height(), static_cast<double>(pages) / Q, t.nodes());
    }

    std::printf("\n=== 三、范围查询：连续取 %d 个键要碰多少页 ===\n\n", 10000);
    {
        BPlus t(336);
        for (int k : keys) t.insert(k);
        long long got = 0;
        const long long pages = t.range_pages(N / 3, 10000, got);
        std::printf("B+ 树（顺着叶子链走）：取到 %lld 个键，碰 %lld 页\n", got, pages);
    }
    {
        std::map<int, int> m;
        for (int k : keys) m[k] = k;
        auto it = m.lower_bound(N / 3);
        long long taken = 0;
        long long pages = 1;                      // 定位到起点算一页
        for (; it != m.end() && taken < 10000; ++it, ++taken) ++pages;   // 每个元素一个节点
        std::printf("std::map（迭代器逐个跳）：取到 %lld 个键，碰 %lld 页\n", taken, pages);
    }
    return 0;
}
```

`实测数据`
`Text`

```text
=== 一、扇出与树高：一次查找要碰多少页（1000000 个键，一页 = 4096 字节）===

每个节点最多     节点估字节数  每节点占几页   树高   一次查找碰到的页数
         4               96            1     11               11.0
        16              240            1      6                6.0
        64              816            1      4                4.0
       256             3120            1      3                3.0
       336             4080            1      3                3.0
      1024            12336            4      3               12.0

=== 二、与 std::map 对照：同样是 1000000 个键 ===

std::map：平均每次查找比较 21.4 次（每个节点一页，就是碰 21.4 页）
B+ 树（每个节点最多 336 个键）：树高 3，平均每次查找碰 3.0 页，共 4330 个节点

=== 三、范围查询：连续取 10000 个键要碰多少页 ===

B+ 树（顺着叶子链走）：取到 10000 个键，碰 43 页
std::map（迭代器逐个跳）：取到 10000 个键，碰 10001 页
```

> [!WARNING]
> **下表里的倍数建立在模型的两条假设上：一、一个节点算一页；二、每读一个节点算一次 I/O。**
> 第一条对 B+ 树是设计目标（它的节点本来就按页做），
> 对 `std::map` 却是**最坏情形**：它的节点只有 40 字节，一页装得下几十个，
> 真实分配器也常把先后申请的节点放在相邻的页里。
> 因此下表里的 7.1 倍与 233 倍，有相当一部分来自这条**定义**，
> 而不是两者在实现上的差距；换成「按字节数折算页数」，`std::map` 那一列会小得多。
> 现实里决定胜负的是「一次 I/O 比一次内存访问贵几个数量级」这个量级关系，
> 它不因为这条假设而改变。

| 对照 | `std::map` | B+ 树（扇出 337） | 倍数 |
|---|---|---|---|
| 一次查找碰到的页数 | 21.4 | **3.0** | 7.1 |
| 取一万个连续键碰到的页数 | 10001 | **43** | 233 |
| 一百万个键的节点总数 | 约 100 万 | **4330** | 231 |

**最后一行解释了第一行**：`std::map` 每个键一个节点，B+ 树四千多个节点装下了一百万个键。
节点少、每页装得多，页访问次数自然就少。

## 4.2 这是模拟，不是磁盘实测

> [!WARNING]
> **上面所有「页数」都是模型数出来的，不是真在磁盘上跑出来的。**
> 模型有两条假设：**一个节点占一页**、**每读一个节点就算一次 I/O**。
> 第一条对 `std::map` 尤其不利——一个 40 字节的节点算一整页，
> 页数因此被放大两个数量级。
> 现实里还有缓存（页可能已经在内存里）、预读（顺序访问会提前读下一页）、
> 日志与写放大（写比读更复杂）等因素，真实数字会与上面不同。
> 本机也没有可控的磁盘 I/O 环境，无法把假设换成实测。

`待确认`

要得到真实数字，需要一台能控制缓存状态与 I/O 调度的机器，
并直接对块设备做随机读与顺序读，测出各自的延迟分布。
本机没有这样的环境，因此本章只给页访问次数这个与实现无关的量。
**结论（多路树把页访问次数从与 `log₂ n` 同阶降到与 `log_M n` 同阶）稳定；
具体的毫秒数给不出。**

---

# 第 5 节 数据库与文件系统为什么用它

| 场景 | 为什么是多路树 |
|---|---|
| 数据库索引 | 数据在磁盘上，一次查询要尽量少的 I/O；范围扫描（`BETWEEN`、分页）靠叶子链 |
| 文件系统的目录索引 | 目录项按名字排序，一次查找只碰几个页；创建文件时要能快速插入 |
| 键值存储 | 有序遍历与范围查询是常见需求，哈希表给不了 |
| 内存里的有序容器 | **不需要**：内存访问快，二叉树甚至跳表都够用 |

关联容器的复杂度要求（N4659 §26.2.6 表 90）的原文引在
《09-高阶数据结构/A-07-树（一）：从搜索树到平衡.md》第 5.3 小节。

**标准库给的是对数复杂度，这一点在内存里完全够用。**
多路树出现的理由不是「对数不够好」，而是「对数的底太小」：
二叉树的底是 2，`log₂ 1000000 ≈ 20`；多路树的底是扇出，
`log₃₃₇ 1000000 ≈ 2.4`。**同样是「对数」，一次是 20 次 I/O，一次是 3 次。**

> [!NOTE]
> 本节只有一句结论：
> **B 树与 B+ 树不是「更好的二叉树」，而是「把页这个约束放进结构里」的结果——
> 需求变了，最优形状就跟着变。**

---

# 第 6 节 怎么选

| 你的情况 | 选择 |
|---|---|
| 数据在磁盘上、要按键查找或范围扫描 | **B+ 树**（数据库与文件系统的选择） |
| 数据在内存里、要按键有序 | `std::map`（`A-03`）或哈希表（`A-04`） |
| 数据在磁盘上、只要等值查找 | 哈希索引也可以，但范围查询会变得很贵 |
| 要理解 B 树怎么写对 | 【待补：09-高阶数据结构/B-04-手写：平衡树.md】 |

> [!NOTE]
> 本章的结论归纳成三句话：
> **数据在磁盘上时，代价的单位从「比较次数」换成了「I/O 次数」，
> 于是结构的目标从「树尽量矮」变成「每层尽量宽」；
> 多路树把一页塞满一个节点，一百万个键只要三层，
> 而同样数据的 `std::map` 要碰 21.4 页；
> B+ 树把数据都放在叶子并串成链，范围查询因此从一万次随机访问变成四十三次顺序访问。**

---

> [!TIP]
> **配套演示**：《A-教学素材/演示工具/数据结构演示/》里的「B 树」场景能看见一个节点装满之后从中间裂开、父节点多出一个键的全过程。
> 它的 `--selftest` 输出（C#／.NET Framework 4.8）给出：阶 4、12 个键：节点 8 个、树高 3、分裂 5 次；删两个键后节点 7 个、合并 1 次、借用 1 次。
> **这组数字与本章的 C++ 实测不是同一套工具链，两组不要放进同一张表里比。**

# 术语表

| 术语 | 含义 |
|---|---|
| **页** | 磁盘读写的最小单位，常见 4 KiB |
| **扇出** | 一个节点最多有几个孩子 |
| **多路树** | 一个节点有多个键与多个孩子的搜索树 |
| **B 树** | 所有节点都存数据、所有叶子同层的多路搜索树 |
| **B+ 树** | 数据只在叶子、内部节点只存路标、叶子串成链的变体 |
| **分裂** | 节点满了以后从中间拆成两个，并往父节点提一个键 |
| **顺序访问** | 连续读相邻的数据，磁盘最擅长；与随机访问相对 |
| **页访问次数** | 一次操作碰到多少个页；本章用它代替耗时来做对照 |

# 附录 A 复现本章节实测

**环境**：Windows 11，g++ 15.2.0（MinGW-w64）。
编译命令：`g++ -std=c++17 -O2 btree_pages.cpp -o btree_pages`
与 `g++ -std=c++17 -O2 btree_lookup.cpp -o btree_lookup`。

| 本章节的数字 | 程序 | 在哪一小节 |
|---|---|---|
| 扇出与树高、节点字节估算 | `btree_pages.cpp` 第一段 | 第 2.2、2.3 小节 |
| B 树的一次查找页数（与 B+ 树对照） | `btree_lookup.cpp` | 第 3.4 小节 |
| 与 `std::map` 的页访问对照 | `btree_pages.cpp` 第二段 | 第 4.1 小节 |
| 范围查询的页访问对照 | `btree_pages.cpp` 第三段 | 第 3.3、4.1 小节 |
| 一百万个键时 `std::map` 的比较次数（21.4） | `btree_pages.cpp` 第二段 | 第 1.1、4.1 小节 |

**几点复现说明**：

- 程序里的一百万个键与十万次查询都是固定种子生成的，重跑结果一致；
- 「节点估字节数」按上面的式子估算，不是 `sizeof` 的结果
  （节点用的是 `std::vector`，实际占用会随容量增长策略变化）；
- 「每节点占几页」按下式算：装得下一页记 1 页，装不下按 ⌈节点字节 ÷ 4096⌉ 记
  （1024 那一行因此记 4 页，一次查找碰到 3 个节点，共 12 页）；
- **页数是数出来的，不是测出来的**，这一点在第 4.2 小节已经写明；
- 树高、节点数、页访问次数都是结构量，重跑稳定。

`待确认`

程序里的 B+ 树是为了测量页访问次数而写的**最小实现**：
只做了插入、查找与范围扫描，没有做删除，也没有处理并发与持久化。
**怎么把它写对、写稳（删除时的合并与借位、异常安全、内存管理）见【待补：09-高阶数据结构/B-04-手写：平衡树.md】。**
另外，「一个节点一页」这个模型忽略了缓存与预读，
真实系统里页访问的实际代价要用块设备上的 I/O 实测才能确定。
