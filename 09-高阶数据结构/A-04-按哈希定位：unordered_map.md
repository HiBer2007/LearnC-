# 按哈希定位：`unordered_map`

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

上一章的需求有两半：查得快，还要有序。这一章把「有序」那一半划掉，
只留下六个字：**按键查，越快越好。**
顺序不再需要之后，树就不再是唯一的选择——
可以拿键算出一个位置，直接跳到那里去找，不必一路比较下去。
这就是哈希表：用一个哈希函数把键映射成桶的编号，桶里放数据。

这笔交换的收益很直接。同一份三十万个键的数据，
树里查一次要沿着指针跳二十来层，哈希表里几乎是一步到位：
本章实测的查找耗时是 129.269 ms 对 6.197 ms。
代价也清楚：**顺序没有了**——没有 `lower_bound`、不能按序遍历、
迭代出来的次序由桶的编号决定，换个插入顺序就换一个样。

哈希表还有一条别的结构没有的性质：**它的 `O(1)` 是有条件的。**
条件是哈希函数把键分散得足够均匀。一旦所有键都落进同一个桶，
查找立刻退化成在一条链上逐个比较，实测可以慢一万三千倍。
这一章会用三个哈希函数把这三种情形并排测出来，
因为「平均 `O(1)`」这句话如果不说清前提，等于没说。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 复现命令随程序给出；`文档` 表示引自标准草案或官方资料；`待确认` 表示尚未验证。
> 路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。
> 复杂度写成 `O(...)`。本章节实测环境是 Windows 11 + g++ 15.2.0（MinGW-w64）。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 索引模型与有序换来的四件能力 | 《09-高阶数据结构/A-03-索引存储：map、set 与有序.md》第 1 节 |
| `map` 的接口、失效规则与每元素开销 | 《09-高阶数据结构/A-03-索引存储：map、set 与有序.md》第 3、6 节 |
| 连续存储与链式存储的代价 | 《09-高阶数据结构/A-01-连续存储：array 与 vector.md》第 4 节、`A-02` 第 5 节 |
| 函数对象与 lambda 作模板参数 | 《05-类与面向对象/10-lambda 与函数对象.md》第 5 节 |
| 类模板与默认模板实参 | 《05-类与面向对象/11-模板.md》第 4 节 |
| `std::hash` 这类现成的哈希设施 | 《07-标准库/B-05-数值.md》第 4 节（随机与散列的基础设施在这里出现） |

**相邻的章节**：上一章《09-高阶数据结构/A-03-索引存储：map、set 与有序.md》讲有序索引，
本章节与它成对——同一份数据、同一个需求，只把「要不要顺序」改一个字，
选型就整个换掉。两章的实测表要并排看。
本章节讲的是哈希表的**用法、代价与前提**；
哈希表内部怎么处理冲突，`B-03` 会自己写一个出来。

| 本章各节 | 讲什么 |
|---|---|
| 第 1 节 | 划掉「有序」之后换了什么；桶与哈希函数的模型；平均与最坏的分界 |
| 第 2 节 | 桶数、负载因子与再哈希：插入过程中它们怎么变，`rehash` 与 `reserve` 做什么 |
| 第 3 节 | 接口：与 `map` 的差别；哈希函数与相等谓词必须成对；自定义键类型 |
| 第 4 节 | 失效规则：再哈希让迭代器失效，指针与引用仍有效；迭代次序不可依赖 |
| 第 5 节 | 代价：与 `map` 并排的时间与内存实测；哈希函数质量决定一切 |
| 第 6 节 | 怎么选：什么时候哈希表赢，什么时候必须退回有序结构 |

---

# 第 1 节 划掉「有序」之后

## 1.1 需求里少了什么、多了什么

| 需求 | 有序索引（`map`） | 哈希索引（`unordered_map`） |
|---|---|---|
| 按键查 | `O(log n)` | **平均 `O(1)`，最坏 `O(n)`** |
| 插入、删除 | `O(log n)` | **平均 `O(1)`，最坏 `O(n)`** |
| 按序遍历 | 可以 | **做不到** |
| 取区间 `[a, b)` | 可以 | **做不到** |
| 取前驱与后继 | 可以 | **做不到** |
| 迭代次序 | 由比较准则决定，稳定 | 由桶的编号决定，**不可依赖** |

**右边这一列不是「差一点的 `map`」，而是把一整类能力换掉的结果。**
标准对它的定位写得很直白。

`文档`

> "Unordered associative containers provide an ability for fast retrieval of data
> based on keys. The worst-case complexity for most operations is linear, but the
> average case is much faster."
>
> —— N4659 §26.2.7/1

**「最坏线性、平均快得多」**——这句话里有本章最重要的一个提醒：
平均不是保证。第 5.3 小节会把最坏情形测出来。

## 1.2 模型：桶数组加一个哈希函数

`Text`

```text
   键 ──► 哈希函数 ──► 一个整数 ──► 对桶数取余 ──► 桶的编号
                                                     │
   ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┴─┬──────┐
   │ 桶 0 │ 桶 1 │ 桶 2 │ 桶 3 │ 桶 4 │ 桶 5 │ 桶 6   │ 桶 7 │
   └───┬──┴──────┴───┬──┴──────┴──────┴───┬──┴────────┴──────┘
       │              │                   │
       ▼              ▼                   ▼
   ┌────────┐    ┌────────┐          ┌────────┐   ┌────────┐
   │ 键 16  │    │ 键  9  │          │ 键  2  │──►│ 键 42  │   同一个桶里的元素串成一条链
   └────────┘    └────────┘          └────────┘   └────────┘
```

查找一个键要走三步：算哈希值、算出桶号、在那个桶里比较。
**前两步是常数时间，第三步的代价取决于桶里有多少元素**——
这就是「平均快、最坏慢」的来源：桶里元素的平均个数就是负载因子。

## 1.3 平均与最坏的分界

| 桶的分布 | 一次查找要比较几次 |
|---|---|
| 每个桶最多一个元素 | 1 |
| 平均每桶 `α` 个元素 | 约 `α / 2` |
| 全部挤在一个桶里 | `n` |

**哈希表的所有承诺都压在「桶的分布」上。** 分布由两件事决定：
哈希函数好不好，以及桶够不够多（第 2 节）。两件事都在这一章里有实测。

---

# 第 2 节 桶、负载因子与再哈希

## 2.1 三个数

| 名字 | 含义 |
|---|---|
| `bucket_count()` | 一共有多少个桶 |
| `load_factor()` | 平均每个桶里有多少元素，等于 `size() / bucket_count()` |
| `max_load_factor()` | 容器允许的负载因子上限；超过就加桶 |

`文档`

> Table 91：`b.load_factor()` —— "Returns the average number of elements per bucket."
> `b.max_load_factor()` —— "Returns a positive number that the container attempts
> to keep the load factor less than or equal to. The container automatically
> increases the number of buckets as necessary to keep the load factor below this number."
>
> —— N4659 §26.2.7 表 91

`C++`

```cpp
/* hash_internals.cpp    编译：g++ -std=c++17 -O2 hash_internals.cpp -o hash_internals
 * 桶数与负载因子随插入的变化、rehash 与 reserve 的效果、rehash 之后什么失效 */
#include <cstdio>
#include <string>
#include <unordered_map>

static void show(const char* tag, const std::unordered_map<int, int>& m) {
    std::printf("  %-22s size=%6zu 桶数=%6zu 负载因子=%.3f\n",
                tag, m.size(), m.bucket_count(), m.load_factor());
}

int main() {
    // 一、插入过程中桶数与负载因子怎么变
    std::printf("== 插入过程中的桶数与负载因子 ==\n");
    std::unordered_map<int, int> m;
    show("起始", m);
    std::size_t last_buckets = m.bucket_count();
    for (int i = 1; i <= 200000; ++i) {
        m[i] = i;
        if (m.bucket_count() != last_buckets) {
            std::printf("  插到第 %6d 个时加桶 -> ", i);
            show("", m);
            last_buckets = m.bucket_count();
        }
    }
    show("最后", m);
    std::printf("  最大负载因子 max_load_factor = %.3f\n", m.max_load_factor());

    // 二、reserve：一次把桶要够，之后不再加桶
    std::printf("\n== reserve 之后 ==\n");
    std::unordered_map<int, int> r;
    r.reserve(200000);
    show("reserve(200000)", r);
    std::size_t rb = r.bucket_count();
    int grows = 0;
    for (int i = 0; i < 200000; ++i) {
        r[i] = i;
        if (r.bucket_count() != rb) { ++grows; rb = r.bucket_count(); }
    }
    show("插满 20 万个", r);
    std::printf("  中途加桶次数 = %d\n", grows);

    // 三、rehash 之后什么失效：迭代器失效，指针与引用仍有效
    std::printf("\n== rehash 之后 ==\n");
    std::unordered_map<std::string, int> s;
    s.reserve(4);
    s["alpha"] = 1;
    s["beta"] = 2;
    const int* p = &s["alpha"];
    const std::size_t before = s.bucket_count();
    for (int i = 0; i < 1000; ++i) s["k" + std::to_string(i)] = i;   // 一定会多次 rehash
    const int* q = &s["alpha"];
    std::printf("  桶数 %zu -> %zu\n", before, s.bucket_count());
    std::printf("  元素 alpha 的地址 %p -> %p，相同=%d（指针与引用仍然有效）\n",
                static_cast<const void*>(p), static_cast<const void*>(q), p == q);

    // 四、迭代次序不可依赖
    std::printf("\n== 迭代次序 ==\n");
    std::unordered_map<int, int> a, b;
    for (int i = 0; i < 8; ++i) a[i] = i;
    for (int i = 7; i >= 0; --i) b[i] = i;          // 同样的键，相反的顺序插入
    std::printf("  升序插入后遍历：");
    for (const auto& kv : a) std::printf(" %d", kv.first);
    std::printf("\n  降序插入后遍历：");
    for (const auto& kv : b) std::printf(" %d", kv.first);
    std::printf("\n");
    return 0;
}
```

`实测数据`
`Text`

```text
== 插入过程中的桶数与负载因子 ==
  起始                 size=     0 桶数=     1 负载因子=0.000
  插到第      1 个时加桶 ->                          size=     1 桶数=    13 负载因子=0.077
  插到第     14 个时加桶 ->                          size=    14 桶数=    29 负载因子=0.483
  插到第     30 个时加桶 ->                          size=    30 桶数=    59 负载因子=0.508
  插到第     60 个时加桶 ->                          size=    60 桶数=   127 负载因子=0.472
  插到第    128 个时加桶 ->                          size=   128 桶数=   257 负载因子=0.498
  插到第    258 个时加桶 ->                          size=   258 桶数=   541 负载因子=0.477
  插到第    542 个时加桶 ->                          size=   542 桶数=  1109 负载因子=0.489
  插到第   1110 个时加桶 ->                          size=  1110 桶数=  2357 负载因子=0.471
  插到第   2358 个时加桶 ->                          size=  2358 桶数=  5087 负载因子=0.464
  插到第   5088 个时加桶 ->                          size=  5088 桶数= 10273 负载因子=0.495
  插到第  10274 个时加桶 ->                          size= 10274 桶数= 20753 负载因子=0.495
  插到第  20754 个时加桶 ->                          size= 20754 桶数= 42043 负载因子=0.494
  插到第  42044 个时加桶 ->                          size= 42044 桶数= 85229 负载因子=0.493
  插到第  85230 个时加桶 ->                          size= 85230 桶数=172933 负载因子=0.493
  插到第 172934 个时加桶 ->                          size=172934 桶数=351061 负载因子=0.493
  最后                 size=200000 桶数=351061 负载因子=0.570
  最大负载因子 max_load_factor = 1.000
```

**两条规律看得很清楚。** 第一，桶数是按倍数涨的（13、29、59、127……大致翻倍，
再往上取一个素数），因此加桶的次数只有十几次，而元素插了二十万个。
第二，**负载因子被压在 0.5 上下**，从不接近上限 1.000——
因为容器是在「插入之后会超过上限」时提前加桶的。

**加桶这件事本身就是一次搬家**：所有元素都要按新的桶数重新算一遍位置。
标准把它叫**再哈希**（rehashing），并规定了它的代价：

`文档`

> Table 91：`a.rehash(n)` —— "Postconditions: `a.bucket_count() >= a.size() / a.max_load_factor()`
> and `a.bucket_count() >= n`. Average case linear in `a.size()`, worst case quadratic."
> `a.reserve(n)` —— "Same as `a.rehash(ceil(n / a.max_load_factor()))`."
>
> —— N4659 §26.2.7 表 91

**平均线性**：二十万个元素，加桶十几次，总共搬动的元素量与元素总数同阶，
摊到每次插入是常数——这就是「平均 `O(1)`」里那个「平均」的来历。

## 2.2 `reserve`：一次把桶数要足

`实测数据`
`Text`

```text
== reserve 之后 ==
  reserve(200000)        size=     0 桶数=202409 负载因子=0.000
  插满 20 万个       size=200000 桶数=202409 负载因子=0.988
  中途加桶次数 = 0
```

**`reserve(200000)` 之后，插满二十万个元素，一次桶都没加。**
注意负载因子最后是 0.988，非常接近上限 1.000——
`reserve` 是按 `n / max_load_factor` 要的桶，刚好够用。

> [!TIP]
> **知道要放多少个键时，先 `reserve`。** 收益与 `vector` 的 `reserve` 同理：
> 把十几次「全部元素重算位置」省掉。差别在于哈希表的搬运更贵——
> 每个元素都要重新算一次哈希与桶号，不只是挪动内存。

## 2.3 迭代次序不可依赖

`实测数据`
`Text`

```text
== 迭代次序 ==
  升序插入后遍历： 7 6 5 4 3 2 1 0
  降序插入后遍历： 0 1 2 3 4 5 6 7
```

**同样八个键，插入顺序反过来，遍历出来的顺序也反过来。**
本机这套实现的遍历顺序由桶内的链表决定，因此跟着插入顺序走；
换一套标准库、换一次 `reserve`、中间多删一个元素，结果都可能不同。

> [!WARNING]
> **不要依赖 `unordered_map` 的遍历顺序。**
> 需要固定顺序时只有两条路：换 `std::map`（按比较准则排序），
> 或者把元素先倒进一个 `vector` 再自己排序。
> 「在我机器上跑出来就是这个顺序」不是理由——重跑一次都可能变。

---

# 第 3 节 接口：与 `map` 的差别

## 3.1 哪些接口没有了

| 接口 | `map` / `set` | `unordered_map` / `unordered_set` |
|---|---|---|
| `operator[]`、`at` | 有 | 有（语义相同） |
| `insert`、`emplace`、`try_emplace`、`insert_or_assign` | 有 | 有 |
| `find`、`count`、`equal_range` | 有 | 有（`equal_range` 只对同键容器有意义） |
| `lower_bound`、`upper_bound` | 有 | **没有** |
| 迭代器类别 | 双向（能 `--`） | **前向（只能 `++`）** |
| 桶相关的接口 | 无 | `bucket_count`、`bucket`、`load_factor`、`rehash`、`reserve` |
| 哈希与相等谓词 | 无 | `hash_function()`、`key_eq()` |

**少了三个接口，少的是一整类用法。** 没有 `lower_bound`，
「取 `[a, b)`」这件事在这里根本写不出来——不是慢，是做不到。
迭代器只能 `++` 也不能忽略：任何需要往回走的算法都不能用在它上面。

## 3.2 哈希函数与相等谓词必须成对

`unordered_map` 的模板参数里有四个东西：

`Text`

```text
std::unordered_map<Key, T, Hash, KeyEqual, Allocator>
                            ▲     ▲
                            │     └── 判等：两个键算不算同一个
                            └──────── 哈希：把键变成一个整数
```

**这两个参数必须互相配合**：哈希函数认为「同一个键」的两个对象，
哈希值必须相同。标准把这条约束写在模板参数的要求里。

`文档`

> "Each unordered associative container is parameterized by `Key`, by a function
> object type `Hash` that meets the Hash requirements (20.5.3.4) and acts as a hash
> function for argument values of type `Key`, and by a binary predicate `Pred` that
> induces an equivalence relation on values of type `Key`."
>
> —— N4659 §26.2.7/3

违反这条约束会怎样，用一个程序演示最清楚。

`C++`

```cpp
/* custom_key.cpp    编译：g++ -std=c++17 -O2 custom_key.cpp -o custom_key
 * 自定义键类型：哈希函数与相等谓词必须成对，否则容器的行为不可预料 */
#include <cstdio>
#include <functional>
#include <unordered_set>

struct Point {
    int x;
    int y;
};

// 一、正确的一对：相等看 (x, y)，哈希也由 (x, y) 决定
struct HashByXY {
    std::size_t operator()(const Point& p) const {
        return std::hash<int>()(p.x * 31 + p.y);
    }
};
struct EqByXY {
    bool operator()(const Point& a, const Point& b) const {
        return a.x == b.x && a.y == b.y;
    }
};

// 二、不匹配的一对：相等只看 x，哈希却用了 x 与 y
struct EqByX {
    bool operator()(const Point& a, const Point& b) const { return a.x == b.x; }
};

int main() {
    std::printf("== 正确的一对 ==\n");
    std::unordered_set<Point, HashByXY, EqByXY> s;
    const auto a1 = s.insert({1, 2});
    std::printf("  insert({1,2}) 第一次：插入成功=%d，size=%zu\n", a1.second, s.size());
    const auto a2 = s.insert({1, 2});
    std::printf("  insert({1,2}) 第二次：插入成功=%d，size=%zu\n", a2.second, s.size());
    const auto a3 = s.insert({3, 4});
    std::printf("  insert({3,4})：插入成功=%d，size=%zu\n", a3.second, s.size());
    std::printf("  find({1,2}) 命中=%d；find({2,1}) 命中=%d\n",
                s.find({1, 2}) != s.end(), s.find({2, 1}) != s.end());

    std::printf("\n== 不匹配的一对 ==\n");
    std::printf("  相等谓词只看 x，哈希看 x 与 y：{1,2} 与 {1,3}「相等」，哈希却不同\n");
    std::unordered_set<Point, HashByXY, EqByX> broken;
    const auto b1 = broken.insert({1, 2});
    std::printf("  插入 {1,2}：成功=%d，size=%zu\n", b1.second, broken.size());
    std::printf("  find({1,3}) 命中=%d（按谓词应当命中）\n", broken.find({1, 3}) != broken.end());
    const auto b2 = broken.insert({1, 3});
    std::printf("  再插入 {1,3}：成功=%d，size=%zu（按谓词本该仍是 1）\n", b2.second, broken.size());
    std::printf("  容器不检查这对约束，违反了它属于未定义行为，这里只是把症状打出来\n");
    return 0;
}
```

`实测数据`
`Text`

```text
== 正确的一对 ==
  insert({1,2}) 第一次：插入成功=1，size=1
  insert({1,2}) 第二次：插入成功=0，size=1
  insert({3,4})：插入成功=1，size=2
  find({1,2}) 命中=1；find({2,1}) 命中=0

== 不匹配的一对 ==
  相等谓词只看 x，哈希看 x 与 y：{1,2} 与 {1,3}「相等」，哈希却不同
  插入 {1,2}：成功=1，size=1
  find({1,3}) 命中=0（按谓词应当命中）
  再插入 {1,3}：成功=1，size=2（按谓词本该仍是 1）
  容器不检查这对约束，违反了它属于未定义行为，这里只是把症状打出来
```

**第一段里的每一步都能对上**：同一个键插两次，第二次的 `bool` 是 0、`size` 不变；
换一个键插进去，`size` 变成 2；`find({2,1})` 命中 0，因为 `(2,1)` 与 `(1,2)` 是两个不同的键。

> [!TIP]
> **写这类演示程序时，把「改容器的调用」与「读容器状态的调用」分成两条语句。**
> `printf` 的实参求值顺序在 C 与 C++ 里都是未指定的：
> 写成 `printf("%d %zu", s.insert(x).second, s.size())` 时，
> `s.size()` 可能先被求值，于是打印出来的 `size` 是插入**之前**的。
> 求值顺序的三类情形见《04-语法/04-表达式与运算符.md》第 4 节。

> [!CAUTION]
> **`Hash` 与 `KeyEqual` 不一致的容器行为是未定义行为。**
> 第二段里 `find({1,3})` 找不到一个「按谓词与它相等」的元素，
> 而 `{1,2}` 与 `{1,3}` 又都能插进去——这不是「实现不够聪明」，
> 而是程序违反了容器对这两个参数的要求，标准没有规定它该怎样。
> **写自定义键类型时的自检**：任何两个被判为相等的键，哈希值必须相同。

## 3.3 自定义键类型的三种写法

| 写法 | 例子 | 什么时候用 |
|---|---|---|
| 特化 `std::hash` | `template <> struct std::hash<Point> {...};` | 这个类型在本程序里普遍要用作键 |
| 传哈希函数对象 | `std::unordered_map<Point, int, PointHash> m;` | 只在这一处用 |
| 用 lambda（要写 `decltype`） | `auto h = [](const Point& p){...};`<br>`std::unordered_map<Point, int, decltype(h)> m{0, h};` | 哈希逻辑需要捕获外部状态 |

键类型还必须能用 `==` 比较（默认的 `KeyEqual` 是 `std::equal_to<Key>`），
或者自己给一个相等谓词。**这两件事必须同时成立**——
只给哈希不给相等（或者反过来），只要两者的判断不一致，就落进上一小节那个坑里。

---

# 第 4 节 失效规则

`文档`

> "The elements of an unordered associative container are organized into buckets.
> Keys with the same hash code appear in the same bucket. The number of buckets is
> automatically increased as elements are added to an unordered associative container,
> so that the average number of elements per bucket is kept below a bound.
> **Rehashing invalidates iterators**, changes ordering between elements, and changes
> which buckets elements appear in, but **does not invalidate pointers or references
> to elements**."
>
> —— N4659 §26.2.7/9

这段话把迭代器与指针分开处理，是哈希表独有的一条规则：

| 操作 | 迭代器 | 指针与引用 |
|---|---|---|
| 插入（未触发再哈希） | 不失效 | 不失效 |
| 插入（**触发再哈希**） | **全部失效** | **仍然有效** |
| 删除 | 只有被删的那个失效 | 只有被删的那个失效 |
| `rehash`、`reserve` | **全部失效** | **仍然有效** |

**为什么指针有效、迭代器失效？** 因为元素本身没有搬家：
再哈希改的是「哪个元素挂在哪个桶上」，节点自己的地址没变。
而迭代器在本机实现里记住的是「当前桶 + 桶内的位置」，
桶一变，它记的那套坐标就作废了。

`实测数据`
`Text`

```text
== rehash 之后 ==
  桶数 5 -> 1741
  元素 alpha 的地址 0000028436c826a8 -> 0000028436c826a8，相同=1（指针与引用仍然有效）
```

**桶数从 5 涨到 1741，`alpha` 的地址一个字节都没动。**

> [!WARNING]
> **「指针有效」只对元素本身成立，对迭代器不成立。**
> 常见错误是：一边用范围 `for` 遍历 `unordered_map`，一边往里插元素。
> 只要中途触发一次再哈希，那个范围 `for` 用的迭代器就全废了。
> 需要边遍历边插入时，先把要插的键攒进另一个容器，遍历结束再插进去。

---

# 第 5 节 代价

## 5.1 与 `map` 并排：时间、内存、桶

`C++`

```cpp
/* hash_vs_tree.cpp    编译：g++ -std=c++17 -O2 hash_vs_tree.cpp -o hash_vs_tree
 * 同一份数据放进 map 与 unordered_map：时间、内存、桶数并排对照 */
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <new>
#include <random>
#include <unordered_map>
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

template <class C>
static void clobber(C& c) { asm volatile("" : : "r"(&c) : "memory"); }

template <class F>
static double ms(F&& f) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main() {
    constexpr int N = 300000;
    constexpr int Q = 300000;
    std::mt19937 rng(2024);
    std::vector<int> keys(N);
    for (int i = 0; i < N; ++i) keys[i] = i * 2;
    std::vector<int> queries(Q);
    for (int& q : queries) q = static_cast<int>(rng() % (N * 2));

    volatile long long sink = 0;
    std::map<int, int> mp;
    std::unordered_map<int, int> um;

    g_calls = g_bytes = 0;
    const double t_build_map = ms([&] { for (int k : keys) mp[k] = k; });
    const long long map_calls = g_calls, map_bytes = g_bytes;

    g_calls = g_bytes = 0;
    const double t_build_um = ms([&] { for (int k : keys) um[k] = k; });
    const long long um_calls = g_calls, um_bytes = g_bytes;
    clobber(mp);
    clobber(um);

    const double t_find_map = ms([&] {
        long long h = 0;
        for (int q : queries) h += mp.find(q) != mp.end();
        sink += h;
    });
    const double t_find_um = ms([&] {
        long long h = 0;
        for (int q : queries) h += um.find(q) != um.end();
        sink += h;
    });
    const double t_walk_map = ms([&] { long long s = 0; for (const auto& kv : mp) s += kv.first; sink += s; });
    const double t_walk_um = ms([&] { long long s = 0; for (const auto& kv : um) s += kv.first; sink += s; });

    std::printf("N=%d 个键，%d 次查找（一半命中）\n", N, Q);
    std::printf("%-26s %12s %12s\n", "", "map", "unordered_map");
    std::printf("%-26s %12.3f %12.3f\n", "建表（毫秒）", t_build_map, t_build_um);
    std::printf("%-26s %12.3f %12.3f\n", "查找（毫秒）", t_find_map, t_find_um);
    std::printf("%-26s %12.3f %12.3f\n", "遍历一遍（毫秒）", t_walk_map, t_walk_um);
    std::printf("%-26s %12lld %12lld\n", "建表时的堆分配次数", map_calls, um_calls);
    std::printf("%-26s %12lld %12lld\n", "建表时的堆字节", map_bytes, um_bytes);
    std::printf("%-26s %12.2f %12.2f\n", "每元素堆字节",
                double(map_bytes) / N, double(um_bytes) / N);
    std::printf("%-26s %12s %12zu\n", "桶数（只有无序容器有）", "无", um.bucket_count());
    std::printf("%-26s %12s %12.3f\n", "负载因子", "无", um.load_factor());
    std::printf("%-26s %12zu %12zu\n", "容器对象自身大小", sizeof(mp), sizeof(um));
    std::printf("（sink=%lld）\n", static_cast<long long>(sink));
    return 0;
}
```

`实测数据`
`Text`

```text
N=300000 个键，300000 次查找（一半命中）
                                    map unordered_map
建表（毫秒）               18.919       12.076
查找（毫秒）              129.269        6.197
遍历一遍（毫秒）          2.669        1.098
建表时的堆分配次数       300000       300015
建表时的堆字节          12000000     10334968
每元素堆字节                40.00        34.45
桶数（只有无序容器有）          无       351061
负载因子                        无        0.855
容器对象自身大小             48           56
（sink=179999700432）
```

**查找快了 21 倍**（129.269 对 6.197 ms）。这就是「划掉有序」换来的东西。

内存这一栏有一个反直觉的结果：**`unordered_map` 每元素 34.45 字节，比 `map` 的 40.00 还少。**
原因是两者的节点结构不同：`map` 的节点要存三个指针加颜色标记（40 字节），
`unordered_map` 的节点只要一个后继指针加键值对（本机是 24 字节），
多出来的开销是桶数组。

> [!IMPORTANT]
> **「`unordered_map` 比 `map` 省内存」这句话取决于元素个数，不能一概而论。**
> 桶数组的开销要按元素个数摊薄：元素越多，每元素的字节数越接近节点本身的大小；
> 元素少的时候，桶数组那笔固定开销占比更大，结论就会反过来。
> 本机两个规模的实测对照与成因分析见附录 A。

## 5.2 遍历也更快

`实测数据`
`Text`

```text
遍历一遍（毫秒）          2.669        1.098
```

**哈希表的遍历比树快一倍多**，原因与连续存储对链式存储那条一样：
哈希表的节点虽然也是分散的，但桶数组本身是连续的，
遍历的访问模式比「沿树跳来跳去」更规整。

## 5.3 哈希函数的质量决定一切

`C++`

```cpp
/* hash_quality.cpp    编译：g++ -std=c++17 -O2 hash_quality.cpp -o hash_quality
 * 哈希函数的好坏直接决定 unordered_map 是 O(1) 还是 O(n) */
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// 坏哈希：所有键都落到同一个桶里
struct BadHash {
    std::size_t operator()(int) const { return 0; }
};

// 也不好的哈希：只按末位数字分桶，键又全是 10 的倍数
struct LowBitsHash {
    std::size_t operator()(int k) const { return static_cast<std::size_t>(k % 16); }
};

template <class F>
static double ms(F&& f) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main() {
    constexpr int N = 100000;
    constexpr int Q = 100000;
    std::mt19937 rng(7);
    std::vector<int> keys(N);
    for (int i = 0; i < N; ++i) keys[i] = i * 10;      // 都是 10 的倍数
    std::vector<int> queries(Q);
    for (int& q : queries) q = static_cast<int>(rng() % (N * 10));

    volatile long long sink = 0;

    std::unordered_map<int, int> good;                 // 默认哈希
    std::unordered_map<int, int, LowBitsHash> low;     // 只看末四位，且键低位全为 0
    std::unordered_map<int, int, BadHash> bad;         // 全落一个桶

    const double t_build_good = ms([&] { for (int k : keys) good[k] = k; });
    const double t_build_low = ms([&] { for (int k : keys) low[k] = k; });
    const double t_build_bad = ms([&] { for (int k : keys) bad[k] = k; });

    const double t_find_good = ms([&] {
        long long h = 0;
        for (int q : queries) h += good.find(q) != good.end();
        sink += h;
    });
    const double t_find_low = ms([&] {
        long long h = 0;
        for (int q : queries) h += low.find(q) != low.end();
        sink += h;
    });
    const double t_find_bad = ms([&] {
        long long h = 0;
        for (int q : queries) h += bad.find(q) != bad.end();
        sink += h;
    });

    std::printf("N=%d 个键（都是 10 的倍数），%d 次查找\n", N, Q);
    std::printf("%-16s %12s %12s %14s\n", "", "默认哈希", "只看末四位", "全部一个桶");
    std::printf("%-16s %12zu %12zu %14zu\n", "桶数", good.bucket_count(),
                low.bucket_count(), bad.bucket_count());
    std::printf("%-16s %12.3f %12.3f %14.3f\n", "建表（毫秒）",
                t_build_good, t_build_low, t_build_bad);
    std::printf("%-16s %12.3f %12.3f %14.3f\n", "查找（毫秒）",
                t_find_good, t_find_low, t_find_bad);
    std::printf("  最长的桶长度：默认 %zu，只看末四位 %zu，全部一个桶 %zu\n",
                good.bucket_count() ? [&] {
                    std::size_t mx = 0;
                    for (std::size_t i = 0; i < good.bucket_count(); ++i)
                        if (good.bucket_size(i) > mx) mx = good.bucket_size(i);
                    return mx;
                }() : 0,
                [&] {
                    std::size_t mx = 0;
                    for (std::size_t i = 0; i < low.bucket_count(); ++i)
                        if (low.bucket_size(i) > mx) mx = low.bucket_size(i);
                    return mx;
                }(),
                [&] {
                    std::size_t mx = 0;
                    for (std::size_t i = 0; i < bad.bucket_count(); ++i)
                        if (bad.bucket_size(i) > mx) mx = bad.bucket_size(i);
                    return mx;
                }());
    std::printf("（sink=%lld）\n", static_cast<long long>(sink));
    return 0;
}
```

`实测数据`
`Text`

```text
N=100000 个键（都是 10 的倍数），100000 次查找
                 默认哈希 只看末四位 全部一个桶
桶数                 172933       172933         172933
建表（毫秒）        5.656     3437.830      10981.326
查找（毫秒）        1.571     3966.114      21807.775
  最长的桶长度：默认 1，只看末四位 12500，全部一个桶 100000
（sink=30372）
```

| 哈希函数 | 最长的桶 | 查找（毫秒） | 相对默认 |
|---|---|---|---|
| 默认（`std::hash<int>`） | 1 | 1.571 | 1 倍 |
| 只看末四位（键的低位全是 0） | 12500 | 3966.114 | **2524 倍** |
| 全部返回 0 | 100000 | 21807.775 | **13882 倍** |

**同一份数据、同一个容器、同一个查找语句，只换哈希函数，慢了将近一万四千倍。**
后两种情况下每个桶里都是一条长链，查找退化成了线性搜索——
这就是标准里那句 "the worst-case complexity for most operations is linear" 的实际样子。

> [!IMPORTANT]
> **「`unordered_map` 是 `O(1)`」的完整说法是：
> 哈希函数把键均匀分散时，平均 `O(1)`；分布退化时，最坏 `O(n)`。**
> 标准库给基本类型和 `std::string` 的默认哈希对常见数据够用；
> **真正需要警惕的是自定义键**——把几个字段简单地相加、
> 或者取某个低位字段当哈希值，都会撞上上面第二行那种情形。

---

# 第 6 节 怎么选

## 6.1 判据

| 你的情况 | 选择 | 理由 |
|---|---|---|
| 只按键查，不需要顺序 | `std::unordered_map` | 平均 `O(1)`，比树快一个量级 |
| 需要按序遍历、取区间、取前驱后继 | `std::map` | 哈希表给不了这些 |
| 需要「按键有序地输出结果」 | `std::map`，或者查完再排序 | 见下面的提示 |
| 键的分布可能很集中 | 先检查哈希函数，或者仍用 `std::map` | 分布退化时哈希表最坏 `O(n)` |
| 数据量很小（几十个键） | 直接线性查找 | 哈希的常数开销还不值得 |
| 键是范围有限的小整数 | 直接用数组 | 一次乘加就定位，比哈希还快 |

## 6.2 一条经验

**「查完之后要不要按顺序拿出来」是这两章之间最实用的一个判据。**
要顺序，就用 `map`：贵一点，但不用再排一次序。
不要顺序，就用 `unordered_map`：省下的时间是一个数量级。

如果既要快又要顺序，还有第三条路：**用 `unordered_map` 查，
把结果倒进 `vector` 再排序**。查询次数远多于排序次数时，
这条路的整体代价可能比全程用 `map` 更低。

> [!NOTE]
> 哈希索引这一章收成三句话：
> **它的形状是「桶数组加哈希函数」，因此平均 `O(1)`，代价是失去全部顺序能力；
> 它的 `O(1)` 有前提——哈希函数要把键分散开，分布退化时最坏是 `O(n)`；
> 挑它的判据是「查完不需要按顺序拿」，需要顺序就回到上一章的树。**

---

# 术语表

| 术语 | 含义 |
|---|---|
| **哈希函数** | 把键映射成一个整数的函数；容器再对它取余得到桶号 |
| **桶** | 哈希表里的一格，存放落进这一格的元素 |
| **负载因子** | `size() / bucket_count()`，平均每个桶里的元素个数 |
| **最大负载因子** | 容器允许的负载因子上限，默认是 1.0；超过就加桶 |
| **再哈希** | 桶数变化后，所有元素按新的桶数重新算位置 |
| **`reserve`** | 一次把桶要够，避免后续的再哈希 |
| **相等谓词** | 判断两个键算不算同一个的函数对象；默认是 `std::equal_to<Key>` |
| **冲突** | 两个不同的键算出了同一个桶号 |
| **前向迭代器** | 只能 `++`、不能 `--` 的迭代器；无序容器的迭代器属于这一类 |

# 附录 A 复现本章节实测

**环境**：Windows 11，g++ 15.2.0（MinGW-w64）。
所有程序的编译命令均为 `g++ -std=c++17 -O2 <源文件> -o <可执行文件>`。

| 本章节的数字 | 程序 | 在哪一小节 |
|---|---|---|
| 桶数与负载因子的变化、`reserve`、指针与引用、迭代次序 | `hash_internals.cpp` | 第 2.1、2.2、2.3、4 节 |
| 哈希函数与相等谓词成对 | `custom_key.cpp` | 第 3.2、3.3 小节 |
| 与 `map` 的时间与内存对照 | `hash_vs_tree.cpp` | 第 5.1、5.2 小节 |
| 三种哈希函数的对照 | `hash_quality.cpp` | 第 5.3 小节 |
| 每元素堆字节随规模的变化 | `hash_vs_tree.cpp` 与《09-高阶数据结构/A-01-连续存储：array 与 vector.md》第 4.1 小节的程序 | 附录 A 的表 |

### A.1 每元素堆字节随元素个数怎么变

`实测数据`
`Text`

```text
         键的个数     map 每元素堆字节    unordered_map 每元素堆字节
          100000            40.00                   43.26
          300000            40.00                   34.45
```

**`map` 那一列不随规模变化**（它的开销全在节点里，每个节点固定 40 字节）；
**`unordered_map` 那一列从 43.26 降到 34.45**，因为它的开销分成两笔：
节点本身大约 24 字节，剩下的是桶数组，而桶数组的字节数被元素个数一除就变小了。

`待确认`

上表两个规模各自只测了一次。「每元素堆字节」随元素个数单调下降并趋于节点大小，
这个趋势成立；具体在哪个规模上 `unordered_map` 开始比 `map` 省，
本机没有逐点扫描过，读者若要据此选型，应在自己的数据规模上各测一次。

**几点复现说明**：

- `hash_internals.cpp` 会打印十几次加桶，属正常输出，不要以为是死循环；
- `hash_quality.cpp` 的后两组要跑十几秒，那是退化之后的真实代价；
- 桶数的增长策略、初始桶数、节点大小都由实现决定，本章数据来自 libstdc++；
- 地址与计时这两类数字每次运行都会变，正文引用的是其中一次的输出；
- 计时数字每次重跑都会浮动，下面给出观察到的区间。

`待确认`

同一份程序重跑数次后观察到的区间：
`hash_vs_tree.cpp` 里 `map` 的查找在 39 到 130 ms 之间，
`unordered_map` 的查找在 3.6 到 7.3 ms 之间（比值在 11 到 21 倍之间浮动）；
`hash_quality.cpp` 里「全部一个桶」的查找在 21 到 24 秒之间。
**结论（哈希查找比树快一个量级；分布退化后慢三个数量级）稳定，逐位数字不稳定。**
