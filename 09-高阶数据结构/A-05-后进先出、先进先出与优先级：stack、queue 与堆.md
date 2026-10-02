# 后进先出、先进先出与优先级：`stack`、`queue` 与堆

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

前三章讲的都是「随便访问哪一个」的容器：按下标取、按键查、按位置插。
还有一类需求恰好相反：**访问方式被限制得只剩一两个口。**
函数的调用与返回是后进先出——最后进去的那一层最先出来；
排队办事是先进先出——先到的先办；
任务调度是优先级——每次都要取出当前最紧急的那一件。
三种需求对应三个结构，它们的共同点是**别的口都封上了**。

封上口不是损失，而是收益。**能做的事变少了，需要维持的性质也跟着变少**：
只要能在一端进出，就不必支持按下标访问，也不必把元素排成一整块；
只要每次取出最值，就不必维持完整的有序——**堆只维持「最值在上面」这一条不变式**，
插入与取出都是 `O(log n)`，而它背后的存储只是一个数组。

这一章要讲清三件事：适配器为什么叫适配器（`stack` 与 `queue` 自己不存数据，
它们把底层容器的接口削成需要的几个）、堆的两种视图（同一个数组既是一串元素，
也是一棵近似完全的二叉树）、以及「只维持最值」这条思路换来的实际代价——
本章实测里，同一份「全部插入再全部取出」的任务，
用堆是 7.471 ms，用每次都维持有序的数组是 529.463 ms。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 复现命令随程序给出；`文档` 表示引自标准草案或官方资料；`待确认` 表示尚未验证。
> 路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。
> 复杂度写成 `O(...)`。本章节实测环境是 Windows 11 + g++ 15.2.0（MinGW-w64）。
> **排序算法的策略**（堆排为什么不稳、快排怎么选轴）不在这里，见 `【待补：08-一些散落的算法/】`。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 连续存储与链式存储的代价 | 《09-高阶数据结构/A-01-连续存储：array 与 vector.md》第 4 节、`A-02` 第 5 节 |
| 每次比较砍掉一半的含义 | 《09-高阶数据结构/A-03-索引存储：map、set 与有序.md》第 5 节 |
| 类模板与模板参数 | 《05-类与面向对象/11-模板.md》第 4 节 |
| 函数对象与 lambda 作比较器 | 《05-类与面向对象/10-lambda 与函数对象.md》第 5 节 |
| 函数实参的求值顺序 | 《04-语法/04-表达式与运算符.md》第 4 节 |

**相邻的章节**：上一章《09-高阶数据结构/A-04-按哈希定位：unordered_map.md》
讲的是「顺序可以不要」，本章把「元素之间的顺序」再砍一刀——
连完整的顺序都不要，只留一个最值。本章的 `stack` 与 `queue` 默认用 `deque` 作底层容器，
原因在第 2 节的实测里：链表每元素一次分配，在只从两端进出的场景下代价最大。
`deque` 自己的代价与实现细节在 `A-01` 与 `A-02` 的数据里都有对照。

| 本章各节 | 讲什么 |
|---|---|
| 第 1 节 | 三类受限需求；封上多余的口换来什么 |
| 第 2 节 | 适配器：接口是新的、存储是借的；换底层容器；`queue` 为什么不能用 `vector` |
| 第 3 节 | 堆：只维持最值在上面；数组与树两种视图；上浮与下沉；五个操作逐个走一遍 |
| 第 4 节 | `priority_queue`：它就是堆的适配器；比较器的方向；存自定义类型 |
| 第 5 节 | 代价：堆对有序数组、适配器换底层容器的实测 |
| 第 6 节 | 怎么选 |

---

# 第 1 节 三种受限的访问方式

## 1.1 三类需求

| 需求 | 允许的操作 | 结构 | 标准库里的名字 |
|---|---|---|---|
| 回退、撤销、函数调用 | 只在一端进出，后进的先出 | 栈 | `std::stack` |
| 排队、缓冲、按序处理 | 一端进、另一端出，先进的先出 | 队列 | `std::queue` |
| 每次都取最紧急的那件 | 随时放入，只取最值 | 优先队列（堆） | `std::priority_queue` |

三者的共同点：**接口只有三四个函数**。`push`（放进去）、`pop`（取出来）、
`top` 或 `front`（看一眼下一个要出来的是谁）、`empty`、`size`。
没有下标、没有查找、没有遍历、没有插入到中间。

## 1.2 限制访问换来什么

| 放弃了 | 换来了 |
|---|---|
| 按下标访问 | 不必把元素排成连续的一块，也不必按位置排序 |
| 按键查找 | 不必维护任何索引 |
| 任意位置插入 | 只在一端改动，不必搬动别的元素 |
| 遍历 | 不需要为「按顺序看一遍」提供任何保证 |

**最后一行值得单独说：`priority_queue` 不提供遍历，
因为它的元素顺序在每次操作后都可能变。** 想看全部元素，只能一个一个取出来。
这不是缺陷，而是「只维持最值」这条路线的必然结果——
容器没有为「整整齐齐排好」花过任何代价。

---

# 第 2 节 适配器：接口是新的，存储是借的

## 2.1 「适配器」这个名字的含义

`stack` 与 `queue` 自己没有存储，它们内部持有一个**底层容器**，
把它的接口削成需要的几个。标准把这件事写成了一条对底层容器的要求。

`文档`

> "Any sequence container supporting operations `back()`, `push_back()` and `pop_back()`
> can be used to instantiate `stack`. In particular, `vector` (26.3.11), `list` (26.3.10)
> and `deque` (26.3.8) can be used."
>
> —— N4659 §26.6.6/1

`文档`

> "Any sequence container supporting operations `front()`, `back()`, `push_back()` and
> `push_front()` can be used to instantiate `queue`. In particular, `list` (26.3.10)
> and `deque` (26.3.8) can be used."
>
> —— N4659 §26.6.4/1

**两条要求只差一个操作名：`stack` 要 `pop_back`，`queue` 要 `push_front`。**
这一个字决定了谁能当谁的底层容器：`vector` 有 `pop_back` 因此能给 `stack` 用，
没有 `push_front` 因此不能给 `queue` 用。默认的底层容器两者都是 `deque`。

`Text`

```text
   std::stack<int>                        std::queue<int>

   ┌───────────────────────┐              ┌───────────────────────┐
   │  只露出这几个操作     │              │  只露出这几个操作     │
   │    push  →  压进去    │              │    push  →  从尾进    │
   │    pop   →  弹出来    │              │    pop   →  从头出    │
   │    top   →  看栈顶    │              │    front →  看队头    │
   └───────────┬───────────┘              └───────────┬───────────┘
               │                                      │
               ▼                                      ▼
   ┌───────────────────────┐              ┌───────────────────────┐
   │  底层容器（默认 deque）│              │  底层容器（默认 deque）│
   │  可以是 vector、list  │              │  可以是 list，不能是  │
   │                      │              │  vector               │
   └───────────────────────┘              └───────────────────────┘
```

## 2.2 换底层容器：代价跟着走

适配器自己不存数据，因此**它的代价就是底层容器的代价**。这一点可以直接测出来。

`C++`

```cpp
/* adapters.cpp    编译：g++ -std=c++17 -O2 adapters.cpp -o adapters
 * 适配器：同一套接口换不同底层容器，代价跟着底层容器走 */
#include <chrono>
#include <cstdio>
#include <deque>
#include <list>
#include <queue>
#include <stack>
#include <vector>

template <class F>
static double ms(F&& f) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main() {
    std::printf("== 适配器对象自身大小 ==\n");
    std::printf("  stack<int>（默认底层 deque）        %zu 字节\n", sizeof(std::stack<int>));
    std::printf("  stack<int, vector<int>>             %zu 字节\n", sizeof(std::stack<int, std::vector<int>>));
    std::printf("  stack<int, list<int>>               %zu 字节\n", sizeof(std::stack<int, std::list<int>>));
    std::printf("  queue<int>（默认底层 deque）        %zu 字节\n", sizeof(std::queue<int>));
    std::printf("  queue<int, list<int>>               %zu 字节\n", sizeof(std::queue<int, std::list<int>>));

    constexpr int N = 1000000;
    std::printf("\n== 压入并弹出 %d 个 int（单位：毫秒）==\n", N);
    volatile long long sink = 0;

    const double s_deque = ms([&] {
        std::stack<int> s;
        for (int i = 0; i < N; ++i) s.push(i);
        long long acc = 0;
        while (!s.empty()) { acc += s.top(); s.pop(); }
        sink += acc;
    });
    const double s_vector = ms([&] {
        std::stack<int, std::vector<int>> s;
        for (int i = 0; i < N; ++i) s.push(i);
        long long acc = 0;
        while (!s.empty()) { acc += s.top(); s.pop(); }
        sink += acc;
    });
    const double s_list = ms([&] {
        std::stack<int, std::list<int>> s;
        for (int i = 0; i < N; ++i) s.push(i);
        long long acc = 0;
        while (!s.empty()) { acc += s.top(); s.pop(); }
        sink += acc;
    });
    const double q_deque = ms([&] {
        std::queue<int> q;
        for (int i = 0; i < N; ++i) q.push(i);
        long long acc = 0;
        while (!q.empty()) { acc += q.front(); q.pop(); }
        sink += acc;
    });
    const double q_list = ms([&] {
        std::queue<int, std::list<int>> q;
        for (int i = 0; i < N; ++i) q.push(i);
        long long acc = 0;
        while (!q.empty()) { acc += q.front(); q.pop(); }
        sink += acc;
    });

    std::printf("  stack  底层 deque   %10.3f\n", s_deque);
    std::printf("  stack  底层 vector  %10.3f\n", s_vector);
    std::printf("  stack  底层 list    %10.3f\n", s_list);
    std::printf("  queue  底层 deque   %10.3f\n", q_deque);
    std::printf("  queue  底层 list    %10.3f\n", q_list);

    // 适配器的接口只用到底层容器的几个操作，这段模板对任何满足要求的容器都能编译
    std::printf("\n== 同一段模板代码换底层容器 ==\n");
    std::stack<int, std::vector<int>> sv;
    sv.push(1);
    sv.push(2);
    std::stack<int, std::deque<int>> sd;
    sd.push(1);
    sd.push(2);
    std::printf("  vector 底层 top=%d，deque 底层 top=%d\n", sv.top(), sd.top());
    std::printf("（sink=%lld）\n", static_cast<long long>(sink));
    return 0;
}
```

`实测数据`
`Text`

```text
== 适配器对象自身大小 ==
  stack<int>（默认底层 deque）        80 字节
  stack<int, vector<int>>             24 字节
  stack<int, list<int>>               24 字节
  queue<int>（默认底层 deque）        80 字节
  queue<int, list<int>>               24 字节

== 压入并弹出 1000000 个 int（单位：毫秒）==
  stack  底层 deque        3.850
  stack  底层 vector       2.448
  stack  底层 list        58.918
  queue  底层 deque        7.047
  queue  底层 list        56.561

== 同一段模板代码换底层容器 ==
  vector 底层 top=2，deque 底层 top=2
（sink=2499997500000）
```

| 底层容器 | `stack` | `queue` | 原因 |
|---|---|---|---|
| `deque`（默认） | 3.850 | 7.047 | 分段连续，两端都是 `O(1)` |
| `vector` | **2.448** | 用不了 | 尾插尾删最快，但没有头删 |
| `list` | 58.918 | 56.561 | 每元素一次堆分配，这条老账在这里最贵 |

**`stack` 用 `vector` 是最快的**（2.448 对 3.850），因为它的进出都在同一端，
而 `vector` 在尾部的追加与删除没有分段管理的开销。
**`queue` 只能用 `deque` 或 `list`**，而 `deque` 比 `list` 快 8 倍——
原因与前一章讲过的完全一样：链表每放一个元素就要一次堆分配。

> [!TIP]
> **适配器的选择顺序是「先看需要哪些操作，再看代价」。**
> 只在一端进出的用 `vector`；两端都要进出的用 `deque`；
> 只有需要「在中间插入删除」时才轮到 `list`，而那种需求本来就不该用适配器。

## 2.3 `queue` 为什么不能用 `vector`

标准的那句要求只是纸面上的，编译器会把它变成一个具体的报错。

`C++`

```cpp
/* err_queue_vector.cpp   queue 不能用 vector 作底层容器：pop 需要 pop_front */
#include <queue>
#include <vector>
int main() {
    std::queue<int, std::vector<int>> q;      // vector 没有 pop_front
    q.push(1);
    q.pop();
    return 0;
}
```

`实测数据`
`Text`

```text
<MinGW>/include/c++/bits/stl_queue.h: In instantiation of 'void std::queue<_Tp, _Sequence>::pop()
  [with _Tp = int; _Sequence = std::vector<int>]':
<源文件>:7:10:   required from here
<MinGW>/include/c++/bits/stl_queue.h:360:11: error: 'class std::vector<int>' has no member named 'pop_front'
  360 |         c.pop_front();
      |         ~~^~~~~~~~~
（省略了包含链的前几行；编译器打印的路径已换成占位符）
```

**报错发生在 `q.pop()` 这一行，而不是声明容器那一行。** 原因是模板成员按需实例化：
`std::queue<int, std::vector<int>>` 这个类型本身没有错，
直到有人调用需要 `pop_front` 的成员，编译器才去检查并失败。
这也是读模板报错时的一条经验：**先看 "required from here" 指向哪一行自己写的代码。**

## 2.4 底层默认用 `deque` 的原因

`deque` 的形状是**分段连续**：元素分成若干段，每段内部是一块连续内存，
段与段之间靠一张「段指针表」索引。

`Text`

```text
   std::deque<int>（中间那几格是当前用到的元素）

        段指针表                    各段缓冲区（每段内部连续）
      ┌──────────┐
      │  段 0    ┼────► ┌────┬────┬────┬────┐
      ├──────────┤      │    │    │ 2  │ 3  │
      │  段 1    ┼──┐   └────┴────┴────┴────┘
      ├──────────┤  │
      │  段 2    ┼┐ │   ┌────┬────┬────┬────┐
      ├──────────┤│ └──►│ 4  │ 5  │ 6  │    │
      │  段 3    ┼┘    └────┴────┴────┴────┘
      └──────────┘
                    两头留了空段，因此 push_front 与 push_back 都是 O(1)
```

**它与 `vector` 的差别只有一条：`vector` 是一整块，`deque` 是一块一块拼起来的。**
多出来的那张段指针表让「在前面插入」不必搬走全部元素——
只要在最前面那段还有空位，或者再挂一段新的。

| 操作 | `deque` | `vector` |
|---|---|---|
| 尾部追加 | `O(1)` 摊还 | `O(1)` 摊还 |
| **头部追加** | **`O(1)` 摊还** | `O(n)`（要挪全部元素） |
| 中间插入 | `O(n)`，而且要判断往哪一头挪 | `O(n)`，挪后半段 |
| 按下标访问 | `O(1)`，但要先查段表再算偏移 | `O(1)`，一次乘加 |
| 对象自身大小 | 80 字节（段表与两个迭代器） | 24 字节（三个指针） |

`实测数据`
`Text`

```text
== 在头部插入 n 个元素，单位：毫秒 ==
         n       vector        deque         list vector+reverse
   1000000    34742.979        1.949       41.435        1.957

== 在中间（1/2 处）插入 n 个元素，单位：毫秒 ==
         n       vector        deque         list
    100000    111.584      372.268        3.828

  sizeof(deque<int>) = 80
```

（程序与完整输出见《09-高阶数据结构/A-02-链式存储：list 与 forward_list.md》第 5 节。）

**头部插一百万次：`deque` 1.949 ms，`vector` 34742.979 ms。**
中间插入时 `deque` 反而最慢（372.268 ms），因为它要判断往哪一头挪得更少，
判断与挪动的常数都比 `vector` 大。

这两条数字合起来解释了本章的两件事：**`queue` 默认用 `deque`**，
因为它的进出正好落在 `deque` 最便宜的两端；**`queue` 不能换成 `vector`**，
因为 `vector` 没有头部插入与删除（第 2.3 小节那个报错就是这一条）。

> [!TIP]
> **要按下标随机访问、又只在两端增删，`deque` 是最合适的一档。**
> 它比 `vector` 多了头部操作与那张段表，代价是随机访问稍慢、对象大一些。
> 需要「一整块连续内存」时（例如要交给只认指针与长度的 C 接口）才回到 `vector`。

---

# 第 3 节 堆：只维持「最值在上面」

## 3.1 需求：反复取最值，但不要求全序

需求是这样：不断有任务进来，每次处理时都要取出**当前最紧急**的那一件。
数据量很大，进出都很频繁。

最朴素的做法是维护一个始终有序的数组：取出最值就是取第一个元素，`O(1)`；
但每次插入都要把后面所有元素挪一格，`O(n)`。
第 5.1 小节会看到，二十万个元素进出一次，这样做要 529 ms。

**换个角度想：真的需要「全部有序」吗？**
每次只取一个（最小的或最大的），其余元素的相对顺序从来没人看。
于是可以把要求降到最低：**只保证「最值在第一个位置」**，
其余元素之间随便怎么排都行。这一条弱得多的性质，就是堆。

## 3.2 两种视图：数组与树

堆的存储是一个数组，元素之间没有指针。
「哪个是哪个的孩子」这件事由**下标算出来**，标准把它写成了定义。

`文档`

> "A heap is a particular organization of elements in a range between two random
> access iterators `[a, b)` such that: —(1.1) With `N = b - a`, for all `i`, `0 < i < N`,
> `comp(a[⌊(i−1)/2⌋], a[i])` is false. —(1.2) `*a` may be removed by `pop_heap()`,
> or a new element added by `push_heap()`, in `O(log N)` time."
>
> —— N4659 §28.7.7/1

`a[⌊(i−1)/2⌋]` 就是「`i` 的父节点」。把它翻成中文：**每个元素都不比它的父节点更优先。**
两条性质合起来说明：最值在 `a[0]`，而调整一次只要 `O(log N)`。

`Text`

```text
   同一个数组，两种读法（下面这份数据取自本章程序的输出）：

   下标：   0    1    2    3    4    5    6    7
          ┌────┬────┬────┬────┬────┬────┬────┬────┐
   数组： │ 9  │ 6  │ 4  │ 1  │ 5  │ 3  │ 2  │ 1  │
          └────┴────┴────┴────┴────┴────┴────┴────┘

   树形：            9                ← 下标 0，最值
                  ┌─┴─┐
                  6   4              ← 下标 1、2（父都是 0）
                ┌─┴┐ ┌┴─┐
                1  5 3  2            ← 下标 3、4、5、6
              ┌─┘
              1                      ← 下标 7（父是 3）

   父子关系不用指针，靠下标算：i 的父是 (i-1)/2，左孩子 2i+1，右孩子 2i+2
```

**这棵树是「近似完全」的**：除了最后一层，每一层都填满，最后一层从左往右填。
正因为形状这么规整，才能用下标代替指针——
**一份数据，两种视图，指针一个都不需要。**

## 3.3 上浮与下沉

维持堆性质只需要两个动作：

| 动作 | 什么时候用 | 怎么做 |
|---|---|---|
| **上浮** | 新元素放到数组末尾之后 | 与父比较，比父更优先就交换，一直到不比父优先为止 |
| **下沉** | 第一个元素被取走之后 | 把末尾元素搬到第一个位置，再与两个孩子里更优先的那个比较、交换，一直到不比孩子差为止 |

两个动作都沿着树的一层一层走，因此是 `O(log n)`。
**「把末尾元素搬到堆顶再下沉」是 `pop_heap` 的关键一步**：
它保住「近似完全」的形状，代价只是一次下沉。

## 3.4 五个操作各自做什么

`C++`

```cpp
/* heap_shape.cpp    编译：g++ -std=c++17 -O2 heap_shape.cpp -o heap_shape
 * 堆的两种视图：同一份数据既是一个数组，也是一棵近似完全的二叉树 */
#include <algorithm>
#include <cstdio>
#include <vector>

static void dump(const char* tag, const std::vector<int>& v) {
    std::printf("%-16s 数组：", tag);
    for (int x : v) std::printf(" %d", x);
    std::printf("\n");
}

static void dump_tree(const std::vector<int>& v) {
    std::printf("                 树形：");
    std::size_t i = 0;
    std::size_t level = 1;
    while (i < v.size()) {
        std::printf("\n                   ");
        for (std::size_t j = 0; j < level && i < v.size(); ++j, ++i) std::printf("%5d", v[i]);
        level *= 2;
    }
    std::printf("\n");
}

int main() {
    std::vector<int> v{3, 1, 4, 1, 5, 9, 2, 6};
    dump("原始", v);

    std::make_heap(v.begin(), v.end());
    dump("make_heap 之后", v);
    dump_tree(v);

    std::printf("\n  下标关系：i 的父是 (i-1)/2，左孩子 2i+1，右孩子 2i+2\n");
    for (std::size_t i = 1; i < 4; ++i) {
        std::printf("    i=%zu 的值 %d，父下标 %zu 的值 %d\n",
                    i, v[i], (i - 1) / 2, v[(i - 1) / 2]);
    }

    std::printf("\n== push_heap ==\n");
    v.push_back(7);
    dump("push_back(7)", v);
    std::push_heap(v.begin(), v.end());
    dump("push_heap 之后", v);

    std::printf("\n== pop_heap ==\n");
    std::pop_heap(v.begin(), v.end());   // 最大值被换到末尾
    dump("pop_heap 之后", v);
    const int top = v.back();
    v.pop_back();
    std::printf("  取出的最大值 = %d，剩下 %zu 个\n", top, v.size());

    std::printf("\n== sort_heap ==\n");
    std::sort_heap(v.begin(), v.end());
    dump("sort_heap 之后", v);
    std::printf("  排好序之后它已经不是一个堆了：堆的性质被排序破坏\n");

    std::printf("\n== 用 is_heap 验证 ==\n");
    std::vector<int> h{9, 5, 4, 1, 1, 3, 2};
    std::vector<int> noth{1, 2, 3};
    std::printf("  {9,5,4,1,1,3,2} 是堆？%d\n", std::is_heap(h.begin(), h.end()));
    std::printf("  {1,2,3} 是堆？%d\n", std::is_heap(noth.begin(), noth.end()));
    return 0;
}
```

`实测数据`
`Text`

```text
原始           数组： 3 1 4 1 5 9 2 6
make_heap 之后 数组： 9 6 4 1 5 3 2 1
                 树形：
                       9
                       6    4
                       1    5    3    2
                       1

  下标关系：i 的父是 (i-1)/2，左孩子 2i+1，右孩子 2i+2
    i=1 的值 6，父下标 0 的值 9
    i=2 的值 4，父下标 0 的值 9
    i=3 的值 1，父下标 1 的值 6

== push_heap ==
push_back(7)     数组： 9 6 4 1 5 3 2 1 7
push_heap 之后 数组： 9 7 4 6 5 3 2 1 1

== pop_heap ==
pop_heap 之后  数组： 7 6 4 1 5 3 2 1 9
  取出的最大值 = 9，剩下 8 个

== sort_heap ==
sort_heap 之后 数组： 1 1 2 3 4 5 6 7
  排好序之后它已经不是一个堆了：堆的性质被排序破坏

== 用 is_heap 验证 ==
  {9,5,4,1,1,3,2} 是堆？1
  {1,2,3} 是堆？0
```

| 操作 | 做什么 | 复杂度 |
|---|---|---|
| `make_heap` | 把一个普通区间整理成堆 | `O(n)` |
| `push_heap` | 把**已经在末尾**的新元素上浮到位 | `O(log n)` |
| `pop_heap` | 把堆顶换到末尾，并让剩下的重新成堆 | `O(log n)` |
| `sort_heap` | 反复 `pop_heap`，把一个堆排成升序 | `O(n log n)` |
| `is_heap` | 检查一个区间是否满足堆性质 | `O(n)` |

`文档`

> `push_heap` —— "Complexity: At most `log(last - first)` comparisons."
> `pop_heap` —— "Complexity: At most `2 log(last - first)` comparisons."
> `make_heap` —— "Complexity: At most `3(last - first)` comparisons."
> `sort_heap` —— "Complexity: At most `N log N` comparisons, where `N = last - first`."
>
> —— N4659 §28.7.7.1/3、§28.7.7.2/3、§28.7.7.3/3、§28.7.7.4/3

三条容易被绊住的地方：

- **`push_heap` 不是「插入」**：它要求新元素已经在 `last - 1` 这个位置上，
  它只负责把它上浮到位。所以要写 `v.push_back(x); std::push_heap(v.begin(), v.end());` 两步。
- **`pop_heap` 也不删除**：它把堆顶换到末尾，元素的个数一个没少。
  要真的丢掉它，得自己 `pop_back()`。
- **`sort_heap` 之后就再也不是堆了**：排序输出的是升序，
  而升序数组的第一个元素是最小值，堆要求第一个是最大值（默认比较器下）。
  `is_heap` 对 `{1,2,3}` 返回 0，就是这个原因。

---

# 第 4 节 `priority_queue`

## 4.1 它是堆的适配器

`priority_queue` 与 `stack`、`queue` 是同一类东西：**适配器**。
标准把它的每个操作都写成「底层容器的操作加一次堆调整」。

`文档`

> `push(x)` —— "Effects: As if by: `c.push_back(std::move(x)); push_heap(c.begin(), c.end(), comp);`"
> `pop()` —— "Effects: As if by: `pop_heap(c.begin(), c.end(), comp); c.pop_back();`"
>
> —— N4659 §26.6.5.3

**这两行等价写法把它讲透了**：`priority_queue` 就是「一个 `vector` 加一组堆操作」，
外面套一层接口。它的模板参数也印证了这一点。

`文档`

> "Any sequence container with random access iterator and supporting operations
> `front()`, `push_back()` and `pop_back()` can be used to instantiate `priority_queue`.
> In particular, `vector` (26.3.11) and `deque` (26.3.8) can be used. Instantiating
> `priority_queue` also involves supplying a function or function object for making
> priority comparisons; the library assumes that the function or function object
> defines a strict weak ordering (28.7)."
>
> —— N4659 §26.6.5/1

**注意「random access iterator」这一条**：堆要靠下标算父子关系，
因此底层容器必须能随机访问——`list` 用不了。
默认的底层容器是 `vector`（不是 `deque`），因为堆只在尾部追加、在尾部删除。

## 4.2 比较器的方向

`C++`

```cpp
/* priority_queue_cmp.cpp    编译：g++ -std=c++17 -O2 priority_queue_cmp.cpp -o priority_queue_cmp
 * priority_queue 的比较器语义：默认是大顶堆，换成 greater 才是小顶堆 */
#include <algorithm>
#include <cstdio>
#include <functional>
#include <queue>
#include <string>
#include <vector>

struct Task {
    int priority;      // 数字越大越紧急
    std::string name;
};

// 比较器：让优先级大的排在堆顶
struct ByPriority {
    bool operator()(const Task& a, const Task& b) const {
        return a.priority < b.priority;      // 「a 的优先级低于 b」→ a 排在后面
    }
};

int main() {
    std::printf("== 默认：大顶堆 ==\n");
    std::priority_queue<int> big;
    for (int x : {3, 1, 4, 1, 5, 9, 2, 6}) big.push(x);
    std::printf("  依次取出：");
    while (!big.empty()) { std::printf(" %d", big.top()); big.pop(); }
    std::printf("\n  默认比较器是 less<int>，堆顶是最大值\n");

    std::printf("\n== 换成 greater：小顶堆 ==\n");
    std::priority_queue<int, std::vector<int>, std::greater<int>> small;
    for (int x : {3, 1, 4, 1, 5, 9, 2, 6}) small.push(x);
    std::printf("  依次取出：");
    while (!small.empty()) { std::printf(" %d", small.top()); small.pop(); }
    std::printf("\n  比较器写成 greater<int>，堆顶变成最小值\n");

    std::printf("\n== 自定义比较器：按任务的优先级 ==\n");
    std::priority_queue<Task, std::vector<Task>, ByPriority> tasks;
    tasks.push({2, "回邮件"});
    tasks.push({5, "修线上故障"});
    tasks.push({1, "整理文档"});
    tasks.push({4, "评审代码"});
    std::printf("  按处理顺序列出：\n");
    while (!tasks.empty()) {
        const Task t = tasks.top();
        tasks.pop();
        std::printf("    优先级 %d  %s\n", t.priority, t.name.c_str());
    }

    std::printf("\n== 比较器的方向与 sort 一致 ==\n");
    std::vector<int> v{3, 1, 4, 1, 5};
    std::sort(v.begin(), v.end());                       // 升序
    std::printf("  sort 默认（less）：");
    for (int x : v) std::printf(" %d", x);
    std::printf("\n  priority_queue 默认（less）：堆顶是最大值，取出顺序是降序\n");
    std::printf("  两者用的是同一个 less，方向看起来相反，原因是「堆顶是最后一个」\n");
    return 0;
}
```

`实测数据`
`Text`

```text
== 默认：大顶堆 ==
  依次取出： 9 6 5 4 3 2 1 1
  默认比较器是 less<int>，堆顶是最大值

== 换成 greater：小顶堆 ==
  依次取出： 1 1 2 3 4 5 6 9
  比较器写成 greater<int>，堆顶变成最小值

== 自定义比较器：按任务的优先级 ==
  按处理顺序列出：
    优先级 5  修线上故障
    优先级 4  评审代码
    优先级 2  回邮件
    优先级 1  整理文档

== 比较器的方向与 sort 一致 ==
  sort 默认（less）： 1 1 3 4 5
  priority_queue 默认（less）：堆顶是最大值，取出顺序是降序
  两者用的是同一个 less，方向看起来相反，原因是「堆顶是最后一个」
```

**比较器的语义是一致的，看起来相反的是「取出的顺序」。**
`less` 的意思是「排在前面的更小」，排序时最小的排在第一；
而堆规定「排在前面的先被取出」，于是用 `less` 时被先取出的反而是最大的。
**要小顶堆就写 `greater`，要按任务的紧急程度排序就写「谁的优先级更低」。**

> [!TIP]
> **写自定义比较器时记住一句话：`comp(a, b)` 为真表示 `a` 应该排在 `b` 后面。**
> 想要「优先级数字大的先出」，就写 `a.priority < b.priority`；
> 想要「编号小的先出」，就写 `a.id > b.id`。这个方向与 `sort` 的直觉相反，
> 但只要盯住「谁排在后面」这一个判断，就不用每次现推。

## 4.3 存自定义类型

上例中的 `Task` 没有定义 `operator<`，照样能进 `priority_queue`——
因为比较器是模板参数，容器从来不用 `operator<`。
这与 `std::sort` 不同：`sort` 默认要求元素能用 `<` 比较。

| 容器或算法 | 默认怎么比较 | 自定义方式 |
|---|---|---|
| `std::priority_queue<T>` | `std::less<T>`，要求 `T` 能用 `<` | 第二个模板参数传比较器 |
| `std::sort` | `std::less<T>`，要求 `T` 能用 `<` | 第三个实参传比较器 |
| `std::map` / `std::set` | `std::less<Key>`，要求键能用 `<` | 第三个模板参数传比较准则 |

---

# 第 5 节 代价

## 5.1 堆对有序数组

同一个任务：二十万个随机数，全部放进去，再全部按从小到大取出来。
四种做法各自的耗时如下。

`C++`

```cpp
/* heap_vs_sorted.cpp    编译：g++ -std=c++17 -O2 heap_vs_sorted.cpp -o heap_vs_sorted
 * 反复「插入 + 取最小」：自己维护有序数组、用堆、以及先全部收齐再排序 */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <queue>
#include <random>
#include <vector>

template <class F>
static double ms(F&& f) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main() {
    constexpr int N = 200000;
    std::mt19937 rng(99);
    std::vector<int> data(N);
    for (int& x : data) x = static_cast<int>(rng() % 1000000);
    volatile long long sink = 0;

    // 做法一：每次插入都维持数组有序（插入排序的做法），取最小直接取头部
    const double t_sorted_arr = ms([&] {
        std::vector<int> v;
        v.reserve(N);
        long long acc = 0;
        for (int x : data) {
            const auto it = std::upper_bound(v.begin(), v.end(), x);
            v.insert(it, x);
        }
        for (int i = 0; i < N; ++i) acc += v[i];      // 顺序取完
        sink += acc;
    });

    // 做法二：堆
    const double t_heap = ms([&] {
        std::priority_queue<int, std::vector<int>, std::greater<int>> pq;
        long long acc = 0;
        for (int x : data) pq.push(x);
        while (!pq.empty()) { acc += pq.top(); pq.pop(); }
        sink += acc;
    });

    // 做法三：先全部收齐，最后排一次序
    const double t_batch = ms([&] {
        std::vector<int> v = data;
        long long acc = 0;
        std::sort(v.begin(), v.end());
        for (int i = 0; i < N; ++i) acc += v[i];
        sink += acc;
    });

    // 做法四：用堆操作自己走一遍（make_heap 加逐个 pop_heap）
    const double t_heap_manual = ms([&] {
        std::vector<int> v = data;
        std::make_heap(v.begin(), v.end(), std::greater<int>());
        long long acc = 0;
        std::size_t n = v.size();
        while (n > 0) {
            std::pop_heap(v.begin(), v.begin() + n, std::greater<int>());
            acc += v[n - 1];
            --n;
        }
        sink += acc;
    });

    std::printf("N=%d 个随机数，全部插入后再全部取出（单位：毫秒）\n", N);
    std::printf("  %-34s %10.3f\n", "有序数组（每次插入都搬移）", t_sorted_arr);
    std::printf("  %-34s %10.3f\n", "堆（priority_queue）", t_heap);
    std::printf("  %-34s %10.3f\n", "堆（make_heap + 逐个 pop_heap）", t_heap_manual);
    std::printf("  %-34s %10.3f\n", "先全部收齐再 sort 一次", t_batch);
    std::printf("  （sink=%lld）\n", static_cast<long long>(sink));
    return 0;
}
```

`实测数据`
`Text`

```text
N=200000 个随机数，全部插入后再全部取出（单位：毫秒）
  有序数组（每次插入都搬移）    529.463
  堆（priority_queue）                 8.916
  堆（make_heap + 逐个 pop_heap）      7.471
  先全部收齐再 sort 一次          8.453
  （sink=400511087216）
```

| 做法 | 每次插入的代价 | 总代价 | 实测 |
|---|---|---|---|
| 有序数组 | `O(n)` 搬移 | `O(n²)` | 529.463 ms |
| 堆 | `O(log n)` 上浮 | `O(n log n)` | **8.916 ms** |
| 堆（手动） | 同上 | `O(n log n)` | 7.471 ms |
| 先收齐再排序 | —— | `O(n log n)` | 8.453 ms |

**堆比「每次都维持有序」快六十到七十倍**，而它的总代价与「先全部收齐再排一次序」在同一档。
这三行说明的是同一件事：**要的只是「每次取最值」，那就别付「维持全序」的钱。**
排序一次的代价与堆的 `n` 次插入加 `n` 次取出同阶，都是 `O(n log n)`，
因此两者可比；而「每次插入都维持有序」是 `O(n²)`，规模一大就垮。

## 5.2 适配器的代价

`实测数据`
`Text`

```text
== 压入并弹出 1000000 个 int（单位：毫秒）==
  stack  底层 deque        3.850
  stack  底层 vector       2.448
  stack  底层 list        58.918
  queue  底层 deque        7.047
  queue  底层 list        56.561
```

一百万个 `int` 压入再弹出：`stack` 用 `vector` 只要 2.448 ms，
用 `list` 要 58.918 ms，**相差 24 倍**。
适配器没有任何自己的开销，这个倍数就是底层容器的倍数——
《09-高阶数据结构/A-02-链式存储：list 与 forward_list.md》第 5.3 小节讲过的那笔账（每元素一次堆分配）在这里原样重现。

> [!IMPORTANT]
> **适配器把「该用哪个容器」这个问题推给了调用方。**
> 它自己不选，默认给一个 `deque`；选错了不会报错，只会慢。
> 判断依据只有一条：**这套操作在底层容器上是 `O(1)` 还是更贵。**
> 栈与队列的进出都限制在两端，因此只在这两端是 `O(1)` 的容器才合适。

---

# 第 6 节 怎么选

| 你的需求 | 选择 | 理由 |
|---|---|---|
| 后进先出，只在一端进出 | `std::stack`，底层用 `vector` | 尾部追加删除最快，实测 2.448 ms |
| 先进先出 | `std::queue`（默认 `deque`） | `vector` 没有头删，用不了 |
| 每次都取最值 | `std::priority_queue` | 只维持最值在上面，`O(log n)` |
| 先全部收齐、之后只读 | 直接排序一个 `vector` | 与堆同阶，代码更简单 |
| 既要取最值、又要能遍历或删除任意元素 | `std::map` / `std::set` | 堆不提供遍历与任意删除 |
| 数据量很小（几十个） | 直接线性查找 | 堆的常数开销还不值得 |

**两条容易忽略的边界**：

- **`priority_queue` 只能取堆顶，不能删除中间某个元素，也不能改已入队元素的优先级。**
  这两种需求都要退回 `std::map`（键是优先级，值是任务）。
- **要「取最小的同时在相同优先级下按先进先出」，堆做不到。**
  堆只比较一个准则，相同优先级的元素之间没有顺序保证——
  需要时把入队序号并进比较准则里。

> [!NOTE]
> 这一章收成三句话：
> **适配器的接口只有几个操作，存储完全借自底层容器，因此选型就是选底层容器；
> 堆只维持「最值在上面」这一条不变式，用数组存、靠下标算父子，插入与取出都是 `O(log n)`；
> 只取最值就别维持全序——这是一条贯穿本书的取舍：需要什么性质，就只付那份代价。**

---

# 术语表

| 术语 | 含义 |
|---|---|
| **适配器** | 不自己存数据、把底层容器的接口削成几个操作的包装类 |
| **后进先出** | 最后放进去的最先取出来；栈的性质 |
| **先进先出** | 最先放进去的最先取出来；队列的性质 |
| **堆** | 用数组表示的近似完全二叉树，满足「每个元素不比父更优先」 |
| **近似完全二叉树** | 除最后一层外每层填满、最后一层从左往右填的二叉树 |
| **上浮** | 新元素与父比较并交换，直到位置合适 |
| **下沉** | 堆顶元素与孩子中更优先的那个交换，直到位置合适 |
| **优先队列** | 每次取出优先级最高元素的容器；堆是它的常见实现 |
| **严格弱序** | 比较器必须满足的性质：`comp(a, a)` 为假，且传递 |

# 附录 A 复现本章节实测

**环境**：Windows 11，g++ 15.2.0（MinGW-w64）。
所有程序的编译命令均为 `g++ -std=c++17 -O2 <源文件> -o <可执行文件>`。

| 本章节的数字 | 程序 | 在哪一小节 |
|---|---|---|
| 适配器的大小与换底层容器的代价 | `adapters.cpp` | 第 2.2、5.2 小节 |
| `queue` 不能用 `vector` | `err_queue_vector.cpp`（**故意编不过**） | 第 2.3 小节 |
| 堆的两种视图与五个操作 | `heap_shape.cpp` | 第 3.2、3.4 小节 |
| 比较器的方向 | `priority_queue_cmp.cpp` | 第 4.2 小节 |
| 堆对有序数组 | `heap_vs_sorted.cpp` | 第 5.1 小节 |

**几点复现说明**：

- `adapters.cpp` 与 `heap_vs_sorted.cpp` 各要跑几秒，属正常；
- `heap_shape.cpp` 打印的树形是按层缩进的结果，
  「每一层一行」的排法只在元素个数不超过一层容量时最整齐；
- 堆的调整细节（怎么选孩子、什么时候停）由实现决定，
  但**打印出来的数组必须满足第 3.2 小节引的那条性质**，这一点可以用 `is_heap` 复核；
- 地址与计时这两类数字每次运行都会变；本机两轮运行观察到的计时区间见下。

`待确认`

同一份程序重跑数次后观察到的区间：
`heap_vs_sorted.cpp` 里「有序数组」那一行在 460 到 560 ms 之间，
三种堆做法都在 7 到 10 ms 之间；
`adapters.cpp` 里 `stack` 用 `vector` 在 1.9 到 3.0 ms 之间、用 `list` 在 43 到 65 ms 之间。
**结论（只维持最值比维持全序便宜一个数量级；适配器的代价等于底层容器的代价）稳定，
逐位数字不稳定。**
