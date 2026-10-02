# 索引存储：`map`、`set` 与有序

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

前两章解决了两种需求：按下标随机访问，用连续存储；在已知位置频繁插入删除，用链式存储。
还有一类需求两者都满足不了：**按键查，而且要一直保持有序。**
有序不是装饰——它换来四件具体的能力：按序遍历、取某段区间、取前驱与后继、按区间统计。
前面两种模型给不了这些：连续存储要维持有序就得在插入时搬移元素，
链式存储根本没有能支持二分的结构。于是引出第三类摆法：
**数据之外另存一张有序的表，查的时候先查表，再顺着表找到数据。**

表本身有两种组织方式：排好序的数组，或者一棵按大小组织的树。
两者查找都是 `O(log n)`，差别全在插入与删除上——
排序数组插一个元素要挪动后面所有元素，树只要改几个指针。
标准库给的 `map` 与 `set` 选了树。这笔账的结果很具体：
在一份「20 万个键、2 万次插入、2 万次删除、20 万次查找」的任务里，
排序数组的查找比 `map` 快两倍多，插入却慢一百倍，删除慢一千七百倍。

「有序」换来四种按序操作，代价则落在插入与删除上；
`map` 与 `set` 的节点形状、自定义排序准则与失效规则都围绕这一点。
三个自带的算法——`lower_bound`、`upper_bound`、`equal_range`——是「有序」在接口上的直接体现：
它们只要求区间有序，因此换成一个排好序的数组，同一套函数也照样能用。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 复现命令随程序给出；`文档` 表示引自标准草案或官方资料；`待确认` 表示尚未验证。
> 路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。
> 复杂度写成 `O(...)`。本章节实测环境是 Windows 11 + g++ 15.2.0（MinGW-w64）。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 连续存储的插入代价与失效规则 | 《09-高阶数据结构/A-01-连续存储：array 与 vector.md》第 3 节 |
| 链式存储的节点模型与地址稳定性 | 《09-高阶数据结构/A-02-链式存储：list 与 forward_list.md》第 2 节 |
| 二分查找要求区间有序 | 《04-语法/08-数组、指针与引用.md》第 2.1 节（下标与地址） |
| 类模板与比较器这类模板参数 | 《05-类与面向对象/11-模板.md》第 4 节 |
| `pair` 与结构化绑定 | 《07-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 1 节 |
| lambda 与函数对象 | 《05-类与面向对象/10-lambda 与函数对象.md》第 5 节 |
| 异常与 `catch` | 《04-语法/13-异常.md》第 1 节 |

**相邻的章节**：上一章《09-高阶数据结构/A-02-链式存储：list 与 forward_list.md》
讲的是「位置已知时插入便宜」；本章节讲的是「按键定位」。
下一章 `A-04` 会把本章的「有序」这一半划掉，只保留「快」，
用哈希表换掉搜索树，两章的实测要并排看。
本章节用到的树只讲**用起来的样子、代价与失效规则**，
树为什么不会退化、旋转在维持什么，全部留给 `A-07`。

| 本章各节 | 讲什么 |
|---|---|
| 第 1 节 | 需求的两半：有序换来哪四件能力；索引模型是什么形状；有序表用数组还是用树 |
| 第 2 节 | 模型：`map`、`set`、`multimap`、`multiset` 的关系；一个节点里有什么；键的等价由谁定 |
| 第 3 节 | 接口：按键取值与插入的几种写法、遍历与结构化绑定、自定义准则、失效规则 |
| 第 4 节 | 它自带的算法：`lower_bound`、`upper_bound`、`equal_range` 与「有序区间」这个前提 |
| 第 5 节 | 实现：为什么查找与插入都是 `O(log n)`；平衡机制在哪一章 |
| 第 6 节 | 代价：建表、查找、插入、删除、遍历的并排实测；空间账；有序的收益 |
| 第 7 节 | 怎么选：什么时候用树、什么时候退回排序数组、什么时候该去 `A-04` |

---

# 第 1 节 需求的两半：查得快，还要有序

## 1.1 前两种模型各缺一半

把需求写清楚：二十万个键，要回答三类问题——某个键在不在、按顺序把键列出来、
取键落在某个区间里的那一批。同时数据还会变，随时可能插入新键、删掉旧键。

| 模型 | 查找 | 按序取区间 | 插入 | 删除 |
|---|---|---|---|---|
| 连续存储（`vector`，不排序） | `O(n)` | 做不到 | 尾部 `O(1)` | `O(n)` |
| 连续存储（`vector`，保持有序） | `O(log n)` | **可以** | `O(n)` | `O(n)` |
| 链式存储（`list`） | `O(n)` | 做不到 | `O(1)` | `O(1)` |
| **索引存储** | `O(log n)` | **可以** | `O(log n)` | `O(log n)` |

**前两种模型都缺了同一半：它们要么查得慢，要么一改就贵。**
排序数组那一行是这一节的起点：它查找够快、也能按序取区间，
唯一的问题是每次插入或删除都要挪动后面所有元素。

## 1.2 索引模型：另存一张有序的表

索引的思路是：**把「按键排序」这件事从数据上搬到另一张表上。**
数据本身可以待在原地不动，表里只放键与「去哪里找它」的线索。

`Text`

```text
   ┌──────────────────────────────┐        按某种规律组织起来的索引表
   │            索引表            │
   │   ┌────────┬──────────────┐  │
   │   │  键 10 │ 指向 10 那一项│  │
   │   ├────────┼──────────────┤  │
   │   │  键 30 │ 指向 30 那一项│  │
   │   ├────────┼──────────────┤  │
   │   │  键 50 │ 指向 50 那一项│  │
   │   └────────┴──────────────┘  │
   └───────────────┬──────────────┘
                   │  查找先查表，再顺着线索取数据
                   ▼
   ┌──────────────────────────────┐
   │  各项数据，放在哪里都行      │
   └──────────────────────────────┘
```

`std::map` 把这两层合成了一层：**键与值放在同一个节点里，节点之间按顺序连起来。**
这样就不需要「线索」那一列了，但键的排序关系必须由节点的连接方式维持。

## 1.3 有序表用数组还是用树

表本身怎么组织，是这一章最重要的一个取舍。

| | 排好序的数组 | 按大小组织的树 |
|---|---|---|
| 查找 | `O(log n)`，一次比较砍一半 | `O(log n)`，一次比较砍一半 |
| 插入 | **`O(n)`**，插入点之后全部后移 | `O(log n)`，改几个指针 |
| 删除 | **`O(n)`**，删除点之后全部前移 | `O(log n)`，改几个指针 |
| 内存 | 紧凑，每元素只有数据 | 每个节点多付几个指针 |
| 遍历 | 最快（一整块内存） | 慢一些（节点散落） |

**标准库选了树，理由只有一个：数据会变。**
如果一张表建好之后只读不改，排序数组是更好的选择——
它查找更快、内存更省、遍历更快。两种做法的分界线由实测给出（第 6 节）。

---

# 第 2 节 模型：`map` 与 `set`

## 2.1 四个名字的关系

| 容器 | 存什么 | 键能重复吗 |
|---|---|---|
| `std::set<Key>` | 只有键 | 不能 |
| `std::multiset<Key>` | 只有键 | 能 |
| `std::map<Key, T>` | 键与值（`pair<const Key, T>`） | 不能 |
| `std::multimap<Key, T>` | 键与值 | 能 |

**`set` 就是「值等于键」的 `map`。** 四个容器共用同一套模型与几乎同一套接口，
差别只在两点：存不存值、允不允许同键。

## 2.2 一个节点里有什么

`map` 的节点里除了一个键值对，还要存维持结构用的链接与标记。这一笔开销可以直接测出来。

`实测数据`
`Text`

```text
  map<int,int>              48
  unordered_map<int,int>    56

== 放下 10 万个 int（或键值对），每元素摊到多少堆字节 ==
  map<int,int>                 40.00 字节/元素（分配 100000 次，共 4000000 字节）
```

（这两组数字的程序在《09-高阶数据结构/A-01-连续存储：array 与 vector.md》第 4.1 小节，
那里同时给了 `list` 与 `forward_list` 的对照。）

`map<int,int>` 每元素 40 字节，其中数据只有 8 字节——**其余三十二字节是链接、
平衡用的标记与对齐填充**。`set<int>` 的元素更小，相对开销更大。
**这一笔开销无法避免：要保持 `O(log n)` 的插入，就必须在节点里存下这些信息。**
至于这些信息具体怎么用、树怎么保证不退化成一条链，见 `A-07`。

## 2.3 键的等价由比较准则决定

`文档`

> "The phrase 'equivalence of keys' means the equivalence relation imposed by the
> comparison and not the `operator==` on keys. That is, two keys `k1` and `k2`
> are considered to be equivalent if for the comparison `comp(k1, k2)` is false
> for both argument orders."
>
> —— N4659 §26.2.6/3

**这句话解释了 `map` 上一个常见的意外**：自定义比较准则之后，
两个看起来不同的键可能被判成同一个键。
比如按绝对值排序时，`-3` 与 `3` 是等价的，表里只会留下先插进去的那一个。
容器不会为此报错，也不会提醒——第 3.4 小节用一个程序把这一点摆出来。

---

# 第 3 节 接口

## 3.1 「按键取值」的几种写法

同一个动作「拿到键 `k` 对应的值」，`map` 给了五种写法，它们的语义并不一样。

`C++`

```cpp
/* map_interface.cpp    编译：g++ -std=c++17 -O2 map_interface.cpp -o map_interface
 * map 的接口行为：operator[] 会插入、at 会抛、insert 不覆盖、emplace/try_emplace 的差别，
 * 以及节点式容器的地址稳定性 */
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

int main() {
    std::map<std::string, int> m;

    // 一、operator[] 找不到就插入一个默认值
    std::printf("== operator[] ==\n");
    std::printf("  空表 size=%zu\n", m.size());
    std::printf("  m[\"apple\"] 读到 %d（默认构造的 int）\n", m["apple"]);
    std::printf("  读了一次之后 size=%zu，表里确实多了一项\n", m.size());
    m["apple"] = 3;                                   // 赋值：key 已存在，直接改值
    std::printf("  赋值后 m[\"apple\"]=%d，size=%zu（没有再多一项）\n", m["apple"], m.size());

    // 二、at 找不到就抛
    std::printf("\n== at ==\n");
    try {
        (void)m.at("banana");
    } catch (const std::out_of_range& e) {
        std::printf("  m.at(\"banana\") 抛出 std::out_of_range：%s\n", e.what());
    }
    std::printf("  抛过之后 size=%zu（at 不会插入）\n", m.size());

    // 三、insert 与 emplace 不覆盖已有的值
    std::printf("\n== insert / emplace ==\n");
    const auto r1 = m.insert({"apple", 99});
    std::printf("  insert({\"apple\",99})：插入成功=%d，值仍是 %d\n", r1.second, m["apple"]);
    const auto r2 = m.emplace("apple", 77);
    std::printf("  emplace(\"apple\",77)：插入成功=%d，值仍是 %d\n", r2.second, m["apple"]);
    const auto r3 = m.insert_or_assign("apple", 42);  // C++17：有则覆盖
    std::printf("  insert_or_assign(\"apple\",42)：插入成功=%d，值变成 %d\n", r3.second, m["apple"]);
    const auto r4 = m.try_emplace("banana", 5);       // C++17：键不存在才构造值
    std::printf("  try_emplace(\"banana\",5)：插入成功=%d，值 %d\n", r4.second, m["banana"]);

    // 四、find 与 count
    std::printf("\n== find / count ==\n");
    std::printf("  find(\"apple\") 命中=%d，count(\"apple\")=%zu\n",
                m.find("apple") != m.end(), m.count("apple"));
    std::printf("  find(\"cherry\") 命中=%d，count(\"cherry\")=%zu\n",
                m.find("cherry") != m.end(), m.count("cherry"));

    // 五、节点式容器的地址稳定性
    std::printf("\n== 插入与删除不动别人 ==\n");
    std::map<int, int> t;
    for (int i = 0; i < 5; ++i) t[i] = i * i;
    const int* p = &t.begin()->second;                 // 记住第一个元素的地址
    const int first_key = t.begin()->first;
    for (int i = 100; i < 200; ++i) t[i] = i;          // 插很多项，很可能触发再平衡
    t.erase(150);
    const int* q = &t.find(first_key)->second;
    std::printf("  插入 100 项并删掉一项后：首元素 key=%d 的值地址 %p -> %p，相同=%d\n",
                first_key, static_cast<const void*>(p), static_cast<const void*>(q), p == q);
    std::printf("  最终 size=%zu\n", t.size());
    return 0;
}
```

`实测数据`
`Text`

```text
== operator[] ==
  空表 size=0
  m["apple"] 读到 0（默认构造的 int）
  读了一次之后 size=1，表里确实多了一项
  赋值后 m["apple"]=3，size=1（没有再多一项）

== at ==
  m.at("banana") 抛出 std::out_of_range：map::at
  抛过之后 size=1（at 不会插入）

== insert / emplace ==
  insert({"apple",99})：插入成功=0，值仍是 3
  emplace("apple",77)：插入成功=0，值仍是 3
  insert_or_assign("apple",42)：插入成功=0，值变成 42
  try_emplace("banana",5)：插入成功=1，值 5

== find / count ==
  find("apple") 命中=1，count("apple")=1
  find("cherry") 命中=0，count("cherry")=0

== 插入与删除不动别人 ==
  插入 100 项并删掉一项后：首元素 key=0 的值地址 0000023ca5992704 -> 0000023ca5992704，相同=1
  最终 size=104
```

| 写法 | 键不存在时 | 键已存在时 | 什么时候用 |
|---|---|---|---|
| `m[k]` | **插入一个默认构造的值** | 返回旧值的引用 | 本来就想「没有就建一个」 |
| `m.at(k)` | **抛 `std::out_of_range`** | 返回旧值的引用 | 认为键一定存在，缺了就是 bug |
| `insert({k, v})` | 插入，返回 `{迭代器, true}` | **什么都不做**，返回 `{迭代器, false}` | 只想知道「原来有没有」 |
| `emplace(args...)` | 原地构造，返回 `{迭代器, true}` | **什么都不做** | 同上，且不想先造一个临时对象 |
| `try_emplace(k, args...)` | 用 `args` 原地构造值 | **什么都不做**，而且**不构造值** | 值的构造有代价时（C++17） |
| `insert_or_assign(k, v)` | 插入 | **覆盖旧值** | 想更新或写入（C++17） |

> [!WARNING]
> **`m[k]` 是唯一会悄悄改表的读法。**
> 本想只做一次查询，却写了 `if (m[k] > 0)`，表里就会多出一项默认值——
> 之后遍历、`size()`、序列化出来的结果都会跟着变。
> **只读查询用 `find` 或 `at`。**

## 3.2 插入为什么返回一对值

`insert` 与 `emplace` 返回的是 `pair<iterator, bool>`：
迭代器指向那个键所在的元素，`bool` 说明这次到底插进去了没有。
这个返回值把「查到」与「插入」合成了一次操作——
否则调用方要写「先 `find`、没有再 `insert`」，那是两次查找。

`try_emplace` 与 `insert_or_assign` 是 C++17 补上的两个：
前者在键已存在时**连值都不构造**（省掉一次可能要付代价的构造），
后者补上了「有则覆盖」这条 `insert` 故意不做的语义。

## 3.3 遍历与结构化绑定

`map` 的遍历顺序**由比较准则决定，与插入顺序无关**。这是「有序」最直接的收益。

`C++`

```cpp
/* ordered_queries.cpp    编译：g++ -std=c++17 -O2 ordered_queries.cpp -o ordered_queries
 * 有序带来的四种能力：按序遍历、lower_bound/upper_bound、equal_range、multimap 的同键区间 */
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

int main() {
    // 一、遍历顺序由比较准则决定，插入顺序无关
    std::printf("== 遍历顺序 ==\n");
    std::map<int, std::string> m;
    for (int k : {50, 10, 90, 30, 70}) m[k] = "k" + std::to_string(k);
    std::printf("  插入顺序：50 10 90 30 70\n  遍历结果：");
    for (const auto& [k, v] : m) std::printf(" %d", k);
    std::printf("\n");

    // 二、lower_bound / upper_bound：区间 [30, 70)
    std::printf("\n== lower_bound 与 upper_bound ==\n");
    const auto lo = m.lower_bound(30);      // 第一个 >= 30 的位置
    const auto hi = m.upper_bound(70);      // 第一个 > 70 的位置
    std::printf("  [30, 70) 内的键：");
    for (auto it = lo; it != hi; ++it) std::printf(" %d", it->first);
    std::printf("\n");
    std::printf("  lower_bound(40) 指向 %d（第一个不小于 40 的键）\n", m.lower_bound(40)->first);
    std::printf("  upper_bound(40) 指向 %d（第一个大于 40 的键）\n", m.upper_bound(40)->first);

    // 三、equal_range：唯一键的表里最多一项
    std::printf("\n== equal_range ==\n");
    const auto er = m.equal_range(30);
    std::printf("  equal_range(30) 的距离 = %ld\n", std::distance(er.first, er.second));
    const auto miss = m.equal_range(31);
    std::printf("  equal_range(31) 是空区间=%d（两端相等）\n", miss.first == miss.second);

    // 四、multimap 允许同键，equal_range 才真正派上用场
    std::printf("\n== multimap ==\n");
    std::multimap<std::string, int> mm;
    mm.emplace("apple", 3);
    mm.emplace("banana", 5);
    mm.emplace("apple", 7);
    mm.emplace("apple", 1);
    const auto r = mm.equal_range("apple");
    std::printf("  apple 的全部值：");
    for (auto it = r.first; it != r.second; ++it) std::printf(" %d", it->second);
    std::printf("\n  count(\"apple\")=%zu，size=%zu\n", mm.count("apple"), mm.size());

    // 五、同一个区间查询改用「排序数组 + equal_range」
    std::printf("\n== 排序数组上的同一个查询 ==\n");
    std::vector<int> v;
    for (const auto& [k, val] : m) v.push_back(k);
    const auto vr = std::equal_range(v.begin(), v.end(), 30);
    std::printf("  有序 vector 的 equal_range(30) 距离 = %ld\n",
                std::distance(vr.first, vr.second));
    const auto vlo = std::lower_bound(v.begin(), v.end(), 30);
    const auto vhi = std::upper_bound(v.begin(), v.end(), 70);
    std::printf("  有序 vector 的 [30, 70)：");
    for (auto it = vlo; it != vhi; ++it) std::printf(" %d", *it);
    std::printf("\n");

    // 六、自定义比较准则：按键的绝对值排序
    std::printf("\n== 自定义比较准则 ==\n");
    struct ByAbs {
        bool operator()(int a, int b) const { return std::abs(a) < std::abs(b); }
    };
    std::map<int, int, ByAbs> byabs;
    for (int k : {5, -3, 1, -4, 2}) byabs[k] = k * 10;
    std::printf("  按键的绝对值升序：");
    for (const auto& [k, v] : byabs) std::printf(" %d", k);
    std::printf("\n");

    // 再插一个 +3：它与 -3 的绝对值相同，比较准则认为两者等价
    const auto ins = byabs.insert({3, 30});
    std::printf("  insert({3,30}) 插进去了吗=%d，表里留着的键是 %d，size=%zu\n",
                ins.second, ins.first->first, byabs.size());
    return 0;
}
```

`实测数据`
`Text`

```text
== 遍历顺序 ==
  插入顺序：50 10 90 30 70
  遍历结果： 10 30 50 70 90

== lower_bound 与 upper_bound ==
  [30, 70) 内的键： 30 50 70
  lower_bound(40) 指向 50（第一个不小于 40 的键）
  upper_bound(40) 指向 50（第一个大于 40 的键）

== equal_range ==
  equal_range(30) 的距离 = 1
  equal_range(31) 是空区间=1（两端相等）

== multimap ==
  apple 的全部值： 3 7 1
  count("apple")=3，size=4

== 排序数组上的同一个查询 ==
  有序 vector 的 equal_range(30) 距离 = 1
  有序 vector 的 [30, 70)： 30 50 70

== 自定义比较准则 ==
  按键的绝对值升序： 1 2 -3 -4 5
  insert({3,30}) 插进去了吗=0，表里留着的键是 -3，size=5
```

`文档`

> "The fundamental property of iterators of associative containers is that they
> iterate through the containers in the non-descending order of keys where
> non-descending is defined by the comparison that was used to construct them."
>
> —— N4659 §26.2.6/11

**插入顺序是 50 10 90 30 70，遍历出来是 10 30 50 70 90。**
标准把这一条写成「关联容器迭代器的基本性质」：遍历顺序由构造时传入的比较准则决定。

结构化绑定让遍历写得最短：`for (const auto& [k, v] : m)`。
`auto&` 而不是 `auto` 是必要的——`auto` 会把每个键值对复制一份；
`const` 则可以防住「遍历时误改键」这类错误。

## 3.4 自定义排序准则

比较准则写在模板参数里，因此**它的类型必须在声明容器时写出来**。
有三种写法：

| 写法 | 例子 | 特点 |
|---|---|---|
| 用默认的 `std::less<Key>` | `std::map<int, int> m;` | 键要能用 `<` 比较 |
| 给一个函数对象类型 | `std::map<int, int, ByAbs> m;` | 类型要能默认构造 |
| 给一个 lambda（C++20 前要 `decltype`） | `std::map<int, int, decltype(cmp)> m{cmp};` | 捕获了变量的 lambda 必须传进构造函数 |

`实测数据`
`Text`

```text
  按键的绝对值升序： 1 2 -3 -4 5
  insert({3,30}) 插进去了吗=0，表里留着的键是 -3，size=5
```

`-3` 与 `3` 的绝对值相同，比较准则认为它们等价，于是**先插进去的那个留下、
后一个被当成重复键丢弃**：程序里先插的是 `-3`，再 `insert({3, 30})` 时
返回的 `bool` 是 `false`，表里留着的键仍是 `-3`，`size` 也没有从 5 变成 6。
这就是第 2.3 小节引的那句标准原文在实践中的样子。

> [!TIP]
> **写自定义准则时的三条自检**：
> 它是**严格弱序**（`comp(a,a)` 必须为 `false`，且传递性成立）；
> 它**不依赖会被改动的状态**（准则变了，容器里的顺序就不再成立，而且再也找不回来）；
> 它对**等价的键**给出「两个方向都是 `false`」，那正是容器判重的依据。

## 3.5 失效规则

`文档`

> "The insert and emplace members shall not affect the validity of iterators and
> references to the container, and the erase members shall invalidate only
> iterators and references to the erased elements."
>
> —— N4659 §26.2.6/9

**插入不动任何已有的迭代器与引用，删除只让被删掉的那一个失效。**
这条规则比 `vector` 宽松得多，原因是节点式的存储：
插入与删除改的是几个指针，其他节点连地址都没变。第 3.1 小节的程序把这一点测了出来。

`实测数据`
`Text`

```text
  插入 100 项并删掉一项后：首元素 key=0 的值地址 0000023ca5992704 -> 0000023ca5992704，相同=1
```

**插了 100 项（很可能触发了树的再平衡），首元素的值地址一个字节都没变。**
再平衡只改节点间的连接，与节点位置无关。

| 操作 | `map` / `set` | `vector` |
|---|---|---|
| 插入 | 全都不失效 | 扩容则全部失效 |
| 删除 | 只有被删的那个失效 | 删除点及之后的失效 |
| 查找、遍历 | 不失效 | 不失效 |

这一条差别在写代码时很实用：**拿着一个 `map` 的迭代器边遍历边插入是安全的**
（新插入的键可能出现在后面，取决于它与当前键的大小关系），
同样的代码换成 `vector` 只要扩容一次就全废了。

---

# 第 4 节 它自带的算法：`lower_bound`、`upper_bound`、`equal_range`

## 4.1 三个函数，三个方向

| 函数 | 返回什么 | 一句话记法 |
|---|---|---|
| `lower_bound(k)` | 第一个**不小于** `k` 的元素 | 「`k` 该插在这里」 |
| `upper_bound(k)` | 第一个**大于** `k` 的元素 | 「`k` 之后的第一个」 |
| `equal_range(k)` | 上面两个组成的一对 | 「等于 `k` 的那一段」 |

`文档`

> Table 90：`b.equal_range(k)` 等价于 `make_pair(b.lower_bound(k), b.upper_bound(k))`，
> 复杂度 logarithmic。
>
> —— N4659 §26.2.6 表 90

三个函数在 `map`、`set`、`multimap`、`multiset` 上都有，
自由函数版本 `std::lower_bound` 也能用在**任何有序区间**上。

`实测数据`
`Text`

```text
  [30, 70) 内的键： 30 50 70
  lower_bound(40) 指向 50（第一个不小于 40 的键）
  upper_bound(40) 指向 50（第一个大于 40 的键）
  equal_range(30) 的距离 = 1
  equal_range(31) 是空区间=1（两端相等）
  有序 vector 的 equal_range(30) 距离 = 1
  有序 vector 的 [30, 70)： 30 50 70
```

**`lower_bound(40)` 与 `upper_bound(40)` 都指向 `50`**，因为表里没有 `40`：
「不小于 40 的第一个」与「大于 40 的第一个」是同一个元素。
`40` 存在时这两个才会分开——那正是 `equal_range` 非空的情形。

## 4.2 「有序区间」是算法与结构之间的接口

`lower_bound` 这一族有一个特点：它们**同时在两个地方存在**——
容器有成员函数，算法库有自由函数。成员版本靠树往下走，
自由函数版本靠「每次砍一半」——**两者都要求同一件事：区间有序。**

这正是索引存储与算法之间的接口：**树维持有序，算法消费有序。**
把数据从 `map` 换成排序数组，`std::lower_bound` 那几行代码一个字都不用改。

| 想做的事 | 用 `map` | 用排序数组 |
|---|---|---|
| 找 `k` | `m.find(k)` | `std::binary_search` |
| 找 `k` 该插在哪 | `m.lower_bound(k)` | `std::lower_bound` |
| 取 `[a, b)` 一段 | `for (it = m.lower_bound(a); it != m.upper_bound(b); ++it)` | 同左，换成自由函数 |
| 取全部等于 `k` 的 | `m.equal_range(k)` | `std::equal_range` |

**只在「数据会变」这一件事上，两者才有分别。**

## 4.3 `multimap` 的同键区间

唯一键的表里 `equal_range` 最多返回一项，用途不明显；
在 `multimap` 里它才真正有用。

`实测数据`
`Text`

```text
  apple 的全部值： 3 7 1
  count("apple")=3，size=4
```

**三个 `apple` 按插入顺序排在一起**，`equal_range` 一次把这一段全拿到。
`multimap` 没有 `operator[]`、也没有 `at`——一个键对应多个值，
「取这个键的值」这句话本身就没有唯一答案，所以标准不提供它。

> [!IMPORTANT]
> **`multimap` 里同一个键的多个值之间的相对顺序，标准不保证。**
> 本机这一次的实测输出是插入顺序（`3 7 1`），
> 但那是实现的巧合，不要写依赖它的代码；需要固定顺序就把序号并进值里。

---

# 第 5 节 实现：为什么查找与插入都是 `O(log n)`

## 5.1 一次比较砍掉一半

树能给出 `O(log n)`，靠的是「每次比较排除一半」这条不变式：
一个节点有左右两棵子树，左子树的键全都小于它，右子树的键全都大于它。
从根往下走，每比较一次就排除了大约一半的候选。

| 键的个数 | 最多比较几次 |
|---|---|
| 1 000 | 10 |
| 1 000 000 | 20 |
| 1 000 000 000 | 30 |

**三十亿个键，三十次比较。** 这就是 `O(log n)` 的直观含义。

## 5.2 平衡机制留在 `A-07`

上面那条不变式有一个前提：**树不能长歪。**
如果每次都插进更大的键，每个新节点都挂在最右边，树就退化成了一条链，
查找从 `O(log n)` 掉到 `O(n)`。标准库的 `map` 之所以敢承诺 `O(log n)`，
是因为它在插入与删除时会做额外的工作把高度压住。

**那份额外的工作是什么、旋转在维持哪条不变式、为什么它不会把插入变慢，全部在 `A-07`。**
本章节只需要知道结论：**查找、插入、删除都是 `O(log n)`，代价是每个节点多付几个指针，
以及每次修改时的常数开销更大。**

---

# 第 6 节 代价

## 6.1 五类操作的对照

同一份任务交给三种索引做法：排序数组、`map`、哈希表（`unordered_map` 作为对照，
它的细节在下一章）。

`C++`

```cpp
/* index_cost.cpp    编译：g++ -std=c++17 -O2 index_cost.cpp -o index_cost
 * 同一个需求（按键查、按键插、按键删、按序遍历）的三种索引做法对照 */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <random>
#include <unordered_map>
#include <vector>

template <class F>
static double ms(F&& f) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main() {
    constexpr int N = 200000;      // 初始键的个数
    constexpr int Q = 200000;      // 查询次数
    constexpr int D = 20000;       // 插入与删除的次数
    std::mt19937 rng(12345);

    std::vector<int> keys(N);
    for (int i = 0; i < N; ++i) keys[i] = i * 3;           // 递增，直接可当有序数组用
    std::vector<int> queries(Q);
    for (int& q : queries) q = static_cast<int>(rng() % (N * 3));

    volatile long long sink = 0;

    std::vector<int> sv = keys;                            // 做法一：有序 vector
    std::map<int, int> mp;                                 // 做法二：搜索树
    std::unordered_map<int, int> um;                       // 做法三：哈希表（对照）

    const double t_build_vec = ms([&] { std::sort(sv.begin(), sv.end()); });
    const double t_build_map = ms([&] { for (int k : keys) mp[k] = k; });
    const double t_build_um = ms([&] { for (int k : keys) um[k] = k; });

    const double t_find_vec = ms([&] {
        long long h = 0;
        for (int q : queries) h += std::binary_search(sv.begin(), sv.end(), q);
        sink += h;
    });
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

    std::vector<int> fresh(D);                             // 一批新键
    for (int i = 0; i < D; ++i) fresh[i] = i * 3 + 1;   // 与既有键交错，插入点落在中间

    const double t_ins_vec = ms([&] {
        for (int k : fresh) {
            const auto it = std::lower_bound(sv.begin(), sv.end(), k);
            sv.insert(it, k);                              // 插入点之后的元素全部后移
        }
    });
    const double t_ins_map = ms([&] { for (int k : fresh) mp[k] = k; });
    const double t_ins_um = ms([&] { for (int k : fresh) um[k] = k; });

    const double t_del_vec = ms([&] {
        for (int k : fresh) {
            const auto it = std::lower_bound(sv.begin(), sv.end(), k);
            if (it != sv.end() && *it == k) sv.erase(it);
        }
    });
    const double t_del_map = ms([&] { for (int k : fresh) mp.erase(k); });
    const double t_del_um = ms([&] { for (int k : fresh) um.erase(k); });

    const double t_walk_vec = ms([&] { long long s = 0; for (int k : sv) s += k; sink += s; });
    const double t_walk_map = ms([&] {
        long long s = 0;
        for (const auto& kv : mp) s += kv.first;
        sink += s;
    });
    const double t_walk_um = ms([&] {
        long long s = 0;
        for (const auto& kv : um) s += kv.first;
        sink += s;
    });

    std::printf("N=%d 个键，%d 次查找，%d 次插入，%d 次删除（单位：毫秒）\n", N, Q, D, D);
    std::printf("%-12s %12s %12s %12s\n", "", "有序 vector", "map", "unordered_map");
    std::printf("%-12s %12.3f %12.3f %12.3f\n", "建表", t_build_vec, t_build_map, t_build_um);
    std::printf("%-12s %12.3f %12.3f %12.3f\n", "查找", t_find_vec, t_find_map, t_find_um);
    std::printf("%-12s %12.3f %12.3f %12.3f\n", "插入", t_ins_vec, t_ins_map, t_ins_um);
    std::printf("%-12s %12.3f %12.3f %12.3f\n", "删除", t_del_vec, t_del_map, t_del_um);
    std::printf("%-12s %12.3f %12.3f %12.3f\n", "按序遍历", t_walk_vec, t_walk_map, t_walk_um);
    std::printf("（sink=%lld）\n", static_cast<long long>(sink));
    return 0;
}
```

`实测数据`
`Text`

```text
N=200000 个键，200000 次查找，20000 次插入，20000 次删除（单位：毫秒）
             有序 vector          map unordered_map
建表              0.918       12.103        7.202
查找             14.962       40.849        3.625
插入            175.879        1.658        0.583
删除           2415.484        1.343        0.411
按序遍历          0.119        2.239        1.143
（sink=179999300622）
```

这张表回答了第 1.3 小节那个取舍：

| 对比 | 数字 | 说明 |
|---|---|---|
| 查找：排序数组对 `map` | 14.962 对 40.849 | **排序数组快 2.7 倍**，因为二分只碰连续内存 |
| 插入：排序数组对 `map` | 175.879 对 1.658 | **`map` 快 106 倍**，因为数组要挪走后面全部元素 |
| 删除：排序数组对 `map` | 2415.484 对 1.343 | **`map` 快 1798 倍**，删除要挪的更多 |
| 遍历：排序数组对 `map` | 0.119 对 2.239 | 排序数组快 19 倍，缓存友好带来的优势 |

「建表」这一行的三列口径并不相同：`vector` 那一列只计了 `std::sort`
（那 20 万个元素是提前拷好的，而且输入已经有序，是排序的最好情况），
`map` 与 `unordered_map` 那两列计的是二十万次插入连同全部节点分配。

**分界线不在「会不会变」，而在「改多少次」。** 这份任务里改了两万次，
删除时数组要挪动的元素总量是二十亿次量级（平均每次挪一半），
规模一大就无法接受；但如果只改几次，`map` 那一列 12.103 ms 的建表开销
比数组多出来的搬移更贵，只读或几乎只读时排序数组反而全面更优。
**改动次数与表规模同量级时，树才稳赢。**

> [!IMPORTANT]
> **「`map` 查找是 `O(log n)`」这句话不足以选型。**
> 排序数组的查找也是 `O(log n)`，而且常数更小；
> `map` 真正换来的是**改数据时也是 `O(log n)`**。
> 判断依据是「这张表会不会变」，不是「查找复杂度是多少」。

## 6.2 空间

`实测数据`
`Text`

```text
  map<int,int>                 40.00 字节/元素（分配 100000 次，共 4000000 字节）
  map<int,int>              48
```

**每个键值对 40 字节，其中数据 8 字节。** `map` 的空间开销因此是数据本身的五倍——与 `list` 一样，
因为两者都是「一个元素一次分配、每次分配都要带链接信息」。

## 6.3 有序换来的东西

| 能力 | 靠什么实现 | 代价 |
|---|---|---|
| 按序遍历 | 中序遍历 | 无（就是遍历） |
| 取区间 `[a, b)` | `lower_bound` + `upper_bound` | `O(log n)` |
| 取前驱与后继 | 迭代器 `--` 与 `++` | 摊还 `O(1)`（单次最坏 `O(log n)`；本机节点里有父指针） |
| 判重 | 比较准则判等价 | 每次插入多几次比较 |
| 每元素空间 | 链接与平衡标记 | 40 字节对 8 字节 |

**「取前驱后继是摊还 `O(1)`」这一条需要单独说明：** 树是有序的，
因此「比 `k` 小的最大的键」这类查询不用重新查一遍，从 `lower_bound` 处往前后挪一步就行；
一次 `++` 最坏要沿父指针往上走几层（`O(log n)`），走完整棵树摊下来每步才是常数。

---

# 第 7 节 怎么选

## 7.1 判据

| 你的情况 | 选择 | 理由 |
|---|---|---|
| 按键查，还要按序遍历或取区间 | `std::map` / `std::set` | 有序是它的本职 |
| 按键查，且同一键会有多个值 | `std::multimap` / `std::multiset` | `equal_range` 一次取全 |
| 数据建好之后只读，不再增删 | **排序数组 + 二分** | 查找更快、内存更省、遍历更快 |
| 只要「查得快」，不需要有序 | **下一章的 `std::unordered_map`** | 平均 `O(1)` |
| 键是很小的整数、范围有限 | 直接用数组当表 | 连比较都省了 |
| 要按区间做统计、求最值 | `std::map` + `lower_bound` | 有序区间上还能套通用算法 |

**一条实用的分界线**：先问「这张表会不会变」。
不会变，排序数组更好；会变，用树；不需要顺序，用哈希。

## 7.2 三个常见误用

**误用一：用 `m[k]` 做只读查询。**
键不存在时会插入一项默认值，表的内容与 `size()` 都会变（第 3.1 小节）。
只读查询用 `find` 或 `at`。

**误用二：以为遍历顺序是插入顺序。**
`map` 的顺序由比较准则决定。想按插入顺序遍历，
需要另外存一个序号，或者改用「`vector` + 下标」的组合。

**误用三：给比较准则留下可变的依赖。**
比较准则一旦与容器里已有的键不一致，元素就再也找不回来了。
准则要只依赖键本身，且必须稳定。

> [!NOTE]
> **它的形状是「按比较准则组织起来的一棵有序结构」，因此查找、插入、删除都是 `O(log n)`，
> 并且天然支持按序遍历与区间查询；
> 它的代价是每个元素多付几个指针、每次修改的常数更大；
> 如果数据只读，排好序的数组在每一项上都更好；如果不需要顺序，就该换哈希表。**

---

# 术语表

| 术语 | 含义 |
|---|---|
| **关联容器** | 按键而不是按位置访问的容器；`map`、`set`、`multimap`、`multiset` |
| **索引** | 数据之外另存的一张表，用来快速定位数据 |
| **比较准则** | 决定键的先后与等价关系的函数对象；默认是 `std::less<Key>` |
| **严格弱序** | 比较准则必须满足的性质：`comp(a, a)` 为假，且传递 |
| **等价** | 比较准则认为「谁也不小于谁」的两个键；容器判重的依据 |
| **前驱／后继** | 有序序列里紧邻在某个键之前／之后的元素 |
| **`lower_bound`** | 第一个不小于给定键的元素 |
| **`upper_bound`** | 第一个大于给定键的元素 |
| **`equal_range`** | 区间 `[lower_bound, upper_bound)`，即全部等价键 |
| **节点** | 树里独立分配的一块内存，存键、值、链接与平衡标记 |
| **退化** | 树长成一条链，查找从 `O(log n)` 掉到 `O(n)` |

# 附录 A 复现本章节实测

**环境**：Windows 11，g++ 15.2.0（MinGW-w64）。
所有程序的编译命令均为 `g++ -std=c++17 -O2 <源文件> -o <可执行文件>`，
程序首行注释里也写了同一条命令。

| 本章节的数字 | 程序 | 在哪一小节 |
|---|---|---|
| 五种取值写法的行为、地址稳定性 | `map_interface.cpp` | 第 3.1、3.5 小节 |
| 遍历顺序、`lower_bound`/`upper_bound`/`equal_range`、`multimap`、自定义准则 | `ordered_queries.cpp` | 第 3.3、3.4、4.1、4.3 小节 |
| 建表／查找／插入／删除／遍历的并排对照 | `index_cost.cpp` | 第 6.1 小节 |
| 每元素堆字节与容器大小 | 见《09-高阶数据结构/A-01-连续存储：array 与 vector.md》第 4.1 小节 | 第 2.2、6.2 小节 |

**几点复现说明**：

- `index_cost.cpp` 里「删除」那一行要跑两秒上下，是排序数组逐次搬移的必然结果；
- 地址与计时这两类数字每次运行都会变，正文引用的是其中一次的输出；
- `map` 与 `set` 的节点开销由实现决定，本章节的数据来自 libstdc++，换一套标准库数字会变；
  迭代器是双向迭代器、`--` 必须可用，这一条则由标准规定
  （N4659 §26.2.6/6：`iterator of an associative container is of the bidirectional
  iterator category`），不是实现自由。

`待确认`

计时数据来自这一台机器。同一份 `index_cost.cpp` 重跑数次后观察到的区间：
排序数组的删除在 1900 到 2600 ms 之间，有序数组的查找在 13.1 到 15.0 ms 之间，
`map` 的查找在 33.2 到 49.2 ms 之间，`unordered_map` 的查找在 3.6 到 11.4 ms 之间，
按序遍历排序数组在 0.06 到 0.12 ms 之间、`map` 在 1.6 到 2.2 ms 之间。
**结论（排序数组改数据要挪走全部元素，因此数量级更差）稳定，逐位数字不稳定。**
