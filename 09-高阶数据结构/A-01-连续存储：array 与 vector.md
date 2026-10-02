# 连续存储：`array` 与 `vector`

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

连续存储是最朴素的一种摆法：向系统要一块内存，把元素一个挨一个排进去。
它换来的东西很直接——**按下标取第 `i` 个元素不需要任何查找**，
地址由起始地址加上 `i` 乘元素大小就能算出来；一整块内存连着读，
硬件一次读取能把后面若干个元素一起带进缓存，遍历因此比任何指针结构都快。

语言给的第一个连续存储是 C 数组，它有两个毛病。
第一个是**一传参就退化成指针**：函数收到的是地址，元素个数当场丢失，
`sizeof` 从「整块的大小」变成「一个指针的大小」。
第二个是**它不是一个值**：不能整体赋值、不能整体比较、不能当返回值，
只能一个元素一个元素地搬。`std::array` 把 C 数组包成一个真正的类型，
上面那些操作全都能做，而且**一个字节都不多花**。
C 数组还有一个更硬的限制：**长度在编译期就要定下来**。
运行期才知道要放多少个元素时，需要一块能自己长大的连续内存，那就是 `std::vector`。

这一章把连续存储从形状讲到代价。先看地址是怎么算出来的、一块缓冲区由哪几个数描述，
再看 `std::array` 补上了 C 数组的哪一半、`std::vector` 补上了哪一半；
然后进入它的核心机制——容量不够时换一块更大的内存、把元素搬过去，
把「搬多少次、搬的时候是移动还是拷贝、什么时候该先 `reserve`」逐条测清楚；
最后算总账：每元素的空间、分配次数、插入的代价、缓存的收益，
以及 `vector<bool>` 这个被特化出来的例外。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 复现命令随程序给出；`文档` 表示引自标准草案或官方资料；`待确认` 表示尚未验证。
> 路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。
> 复杂度写成 `O(...)`，表示代价随规模增长的量级。
> 本章节的实测环境是 Windows 11 + g++ 15.2.0（MinGW-w64），
> 涉及标准库实现差异的地方会给出第二套标准库的对照。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 数组与指针、下标就是 `*(a + i)` | 《04-语法/08-数组、指针与引用.md》第 2.1 节 |
| 数组名退化成指针、`sizeof(arr)` 在函数里为什么变了 | 《04-语法/08-数组、指针与引用.md》第 2.5 节 |
| 越界没有运行时检查 | 《04-语法/08-数组、指针与引用.md》第 5.1 节 |
| 对象在栈上、堆上还是静态区 | 《06-更底层/01-对象在哪里：栈、堆与静态区.md》第 5 节 |
| 拷贝构造与移动构造的调用时机 | 《05-类与面向对象/05-拷贝与移动.md》第 4 节 |
| RAII：资源在析构时释放 | 《05-类与面向对象/06-RAII 与资源管理.md》第 2 节 |
| 异常与 `catch` | 《04-语法/13-异常.md》第 1 节 |
| 缓存行与访问局部性 | 《06-更底层/02-对齐、填充与缓存.md》第 3 节 |

**相邻的章节**：上一章是《09-高阶数据结构/A-00-导读：数据结构是问题的形状.md》，
它把存储模型分成连续、链式、索引三类，本章节讲第一类。
下一章《09-高阶数据结构/A-02-链式存储：list 与 forward_list.md》讲第二类，
两章的数据要对照着看：同样是「放下 n 个元素再遍历一遍」，两条路线的账完全不同。
再往后，第三类存储模型（索引）见《09-高阶数据结构/A-03-索引存储：map、set 与有序.md》第 3 节；
迭代器与失效的一般规则集中在 `A-06`，本章节只讲 `vector` 这一份。

| 本章各节 | 讲什么 |
|---|---|
| 第 1 节 | 连续存储的形状：地址由下标算出来，一块缓冲区由哪几个数描述，它赢在哪、输在哪 |
| 第 2 节 | `std::array`：C 数组的两个毛病，包裹之后一个字节都不多花，越界时 `[]` 与 `at` 的分工 |
| 第 3 节 | `std::vector`：容量、增长、搬迁、`reserve`、移动还是拷贝、插入删除、失效规则 |
| 第 4 节 | 代价：每元素空间、分配次数、边界检查、缓存收益、`vector<bool>` 的特化 |
| 第 5 节 | 怎么选：什么时候连续存储是唯一合理的选择，三个常见误用 |

---

# 第 1 节 连续存储的形状

## 1.1 地址就是一次乘法

C 数组与 `std::array` 的 `a[i]` 之所以是常数时间，靠的是一条公式：
**第 `i` 个元素的地址等于首地址加上 `i` 乘元素大小。**
下面这份程序把「下标取到的地址」与「按公式算出来的地址」逐个对照。

`C++`

```cpp
/* array_shape.cpp    编译：g++ -std=c++17 -O2 array_shape.cpp -o array_shape
 * 连续存储的形状：地址由下标算出来；C 数组退化成指针；std::array 是一个值 */
#include <array>
#include <cstddef>
#include <cstdio>

static void takes_array(int a[10]) {      // 形参写成数组，实际收到的是指针
    std::printf("  函数里 sizeof(a)            = %zu（指针的大小）\n", sizeof(a));
}

int main() {
    int a[10] = {0};
    std::printf("main 里 sizeof(a)             = %zu\n", sizeof(a));
    takes_array(a);

    std::printf("地址与公式对照（a 的首地址 + i × sizeof(int)）：\n");
    for (int i = 0; i < 4; ++i) {
        const void* by_index = &a[i];
        const void* by_formula = reinterpret_cast<const char*>(a) + i * sizeof(int);
        std::printf("  i=%d  &a[i]=%p  基址+i*4=%p  相同=%d\n",
                    i, by_index, by_formula, by_index == by_formula);
    }

    std::array<int, 3> x{1, 2, 3};
    std::array<int, 3> y{};
    y = x;                                        // 整体赋值：C 数组做不到
    std::printf("std::array 整体赋值后 y = {%d, %d, %d}\n", y[0], y[1], y[2]);
    std::printf("std::array 整体比较 x == y  = %d\n", x == y);
    std::printf("std::array 知道自己多长：x.size() = %zu\n", x.size());
    return 0;
}
```

编译这一份时，GCC 会额外给出一条警告。它值得单独看一眼，
因为这条警告说明的问题正是本章节第 2 节的起点。

`实测数据`
`Text`

```text
array_shape.cpp:8:79: warning: 'sizeof' on array function parameter 'a' will return size of 'int*' [-Wsizeof-array-argument]
array_shape.cpp:7:29: note: declared here
（为省篇幅，省略了编译器附在两条诊断下面的源码摘录）
```

`实测数据`
`Text`

```text
main 里 sizeof(a)             = 40
  函数里 sizeof(a)            = 8（指针的大小）
地址与公式对照（a 的首地址 + i × sizeof(int)）：
  i=0  &a[i]=000000ee2a9ffb90  基址+i*4=000000ee2a9ffb90  相同=1
  i=1  &a[i]=000000ee2a9ffb94  基址+i*4=000000ee2a9ffb94  相同=1
  i=2  &a[i]=000000ee2a9ffb98  基址+i*4=000000ee2a9ffb98  相同=1
  i=3  &a[i]=000000ee2a9ffb9c  基址+i*4=000000ee2a9ffb9c  相同=1
std::array 整体赋值后 y = {1, 2, 3}
std::array 整体比较 x == y  = 1
std::array 知道自己多长：x.size() = 3
```

**四个下标取到的地址与公式算出来的完全一致，而且每个相差 4 字节。**
这就是连续存储的本钱：取第 `i` 个元素是一次乘加，与 `i` 无关，也与元素总数无关。

同一份输出里还藏着两个坏消息。
`main` 里 `sizeof(a)` 是 40（10 个 `int`），进了函数就变成 8——**数组退化成了指针**，
元素个数当场丢失，编译器为此专门发了一条警告。
另外，`y = x` 这一行之所以能写，是因为 `x` 与 `y` 是 `std::array` 而不是 C 数组：
**C 数组不能整体赋值，也不能整体比较。**

## 1.2 一块缓冲区、两个数

连续存储要能长大，就必须把「已经放了多少」和「这块内存能放多少」分开记。
两个数各有一个名字：**`size`（已有元素个数）与 `capacity`（缓冲区能装下的元素个数）**。

`Text`

```text
  std::vector<int> v{7, 1, 9};  对象自己只占 24 字节，元素放在堆上的缓冲区里

    ┌────┬────┬────┬────┬────┬────┐
    │ 7  │ 1  │ 9  │    │    │    │     前三格：已经放了元素（size = 3）
    └────┴────┴────┴────┴────┴────┘     六格总量：缓冲区能放多少（capacity = 6）
```

`实测数据`
`Text`

```text
  vector<int>               24
  forward_list<int>          8
  list<int>                 24
  map<int,int>              48
  unordered_map<int,int>    56
```

`vector<int>` 对象自己占 24 字节，是三个指针的大小：一个指向缓冲区开头，
一个指向最后一个元素的下一个位置，一个指向缓冲区末尾。
`size()` 是第二个指针减第一个，`capacity()` 是第三个指针减第一个——
两个都是指针相减，因此都是常数时间。
**24 字节与「放了多少个元素」无关**：放三个是 24 字节，放一百万个还是 24 字节。

> [!IMPORTANT]
> **`size` 与 `capacity` 分开记，是连续存储能「长大」的全部秘密。**
> 如果只有 `size`，那么每次追加元素都得重新申请一块刚好大的内存——
> 每追加一个元素就搬一次全部元素，总代价是 `O(n²)`。
> 多记一个 `capacity`，就可以「一次多要一点，用不完留着」，
> 把搬迁的次数从「每次追加」降到「每隔一段时间一次」。

## 1.3 连续存储赢在哪、输在哪

| 操作 | 代价 | 原因 |
|---|---|---|
| 按下标取第 `i` 个 | `O(1)` | 地址由公式算出，不需要查找 |
| 遍历一遍 | 最快 | 一整块连着，缓存与预取都能帮上忙 |
| 末尾追加（`push_back`） | `O(1)` 摊还 | 大多数时候只是把元素写进空闲格 |
| 在中间插入或删除 | `O(n)` | 插入点之后的元素全部要挪一格 |
| 扩容的那一次 | `O(n)` | 换一块更大的内存，全部元素搬过去 |
| 按值查找 | `O(n)` | 没有额外线索，只能逐个比；排好序可二分 |

**一句话判据**：用得最多的是「按下标取」和「从头到尾走一遍」，就用连续存储；
用得最多的是「在中间插进去、删掉」，就该考虑链式存储
（见《09-高阶数据结构/A-02-链式存储：list 与 forward_list.md》第 1 节）。

---

# 第 2 节 `std::array`：把 C 数组包成一个值

## 2.1 C 数组缺什么

C 数组只有两条性质：一块连续的内存，一个能算出地址的下标。
除此之外的东西一样都没有——它不知道自己的长度，不能整体赋值，
不能整体比较，也不能按值传给函数。

| 想做的事 | C 数组 | 原因 |
|---|---|---|
| 取第 `i` 个 | 可以 | 地址能算 |
| 问「你有多长」 | **不行** | 长度是编译期的信息，运行期没有地方存 |
| `b = a` 整体赋值 | **不行** | 数组名在表达式里退化成指针，`b = a` 改的是指针而不是内容 |
| `a == b` 整体比较 | **不行** | 同上，比的是地址 |
| 按值传参 | **不行** | 传的永远是首地址 |
| 从函数返回 | **不行** | 返回的只能是首地址，局部数组的地址随函数一起消失 |

`std::array` 的定位很窄：**它只补上「是一个值」这一半**，
连续存储的形状、地址公式、零额外空间全部保持原样。
长度仍然要在编译期给定——那是另一半问题，由 `std::vector` 解决。

## 2.2 `std::array` 补上了什么

`std::array<T, N>` 里唯一的数据成员就是 `T[N]` 这样一个 C 数组，
外面套一层成员函数。因为它是一个真正的类类型，上面那张表里的「不行」全部变成「可以」：

`C++`

```cpp
/* array_is_a_value.cpp
 * 编译：g++ -std=c++17 -O2 array_is_a_value.cpp -o array_is_a_value
 * std::array 是一个值：能整体赋值、整体比较、按值传参、按值返回 */
#include <array>
#include <cstdio>

using Row = std::array<int, 4>;

static int sum(Row r) {                  // 按值传参：收到的是整份副本
    int s = 0;
    for (int x : r) s += x;
    return s;
}

static Row doubled(Row r) {              // 按值返回：返回的是整份副本
    for (int& x : r) x *= 2;
    return r;
}

int main() {
    Row a{1, 2, 3, 4};
    Row b{};
    b = a;                               // 整体赋值
    std::printf("b = {%d, %d, %d, %d}\n", b[0], b[1], b[2], b[3]);
    std::printf("a == b 是 %d\n", a == b);
    std::printf("b < Row{2,0,0,0} 是 %d（按字典序整体比较）\n", b < Row{2, 0, 0, 0});
    std::printf("按值传参求和：%d\n", sum(a));
    const Row c = doubled(a);
    std::printf("按值返回后 c = {%d, %d, %d, %d}，a 没变\n", c[0], c[1], c[2], c[3]);
    std::printf("长度：a.size() = %zu，也没退化成指针：sizeof(a) = %zu\n",
                a.size(), sizeof(a));
    return 0;
}
```

`实测数据`
`Text`

```text
b = {1, 2, 3, 4}
a == b 是 1
b < Row{2,0,0,0} 是 1（按字典序整体比较）
按值传参求和：10
按值返回后 c = {2, 4, 6, 8}，a 没变
长度：a.size() = 4，也没退化成指针：sizeof(a) = 16
```

## 2.3 一个字节都不多花

包裹的代价可以是零。`std::array` 只有那一个数组成员，
没有虚函数、没有额外的计数、没有堆分配，因此 `sizeof` 与同样大小的 C 数组完全相等。

`C++`

```cpp
/* array_vs_vector.cpp    编译：g++ -std=c++17 -O2 array_vs_vector.cpp -o array_vs_vector
 * C 数组、std::array、std::vector：对象大小、遍历代价、越界时各自怎么办 */
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <vector>

template <class F>
static double best_ms(int rounds, F&& f) {
    double best = 1e300;
    for (int i = 0; i < rounds; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        f();
        const auto t1 = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    return best;
}

int main() {
    constexpr int N = 1000000;
    static int c_array[N];                                 // 放静态区，避免爆栈
    std::array<int, N>* arr = new std::array<int, N>();    // std::array 也可以放堆上
    std::vector<int> vec(N, 1);
    for (int i = 0; i < N; ++i) { c_array[i] = 1; (*arr)[i] = 1; }

    std::printf("== 对象自身大小 ==\n");
    std::printf("  int[%d]             %8zu 字节\n", N, sizeof(c_array));
    std::printf("  std::array<int,%d>  %8zu 字节（不多一个字节）\n", N, sizeof(*arr));
    std::printf("  std::vector<int>    %8zu 字节（另有堆上的缓冲区）\n", sizeof(vec));

    std::printf("\n== 同样的遍历做 200 轮，取最快的一轮 ==\n");
    volatile long long sink = 0;
    const double t_c = best_ms(200, [&] {
        long long s = 0;
        for (int i = 0; i < N; ++i) s += c_array[i];
        sink += s;
    });
    const double t_a = best_ms(200, [&] {
        long long s = 0;
        for (int x : *arr) s += x;
        sink += s;
    });
    const double t_v = best_ms(200, [&] {
        long long s = 0;
        for (int x : vec) s += x;
        sink += s;
    });
    std::printf("  C 数组      %7.4f ms\n", t_c);
    std::printf("  std::array  %7.4f ms\n", t_a);
    std::printf("  std::vector %7.4f ms\n", t_v);
    std::printf("  （sink=%lld）\n", static_cast<long long>(sink));

    std::printf("\n== 越界取第 %d 个元素 ==\n", N);
    std::printf("  C 数组的 c_array[%d] 与 std::array 的 operator[] 都不检查，\n", N);
    std::printf("  越界是未定义行为，因此这里不去执行它。\n");
    try {
        (void)vec.at(static_cast<std::size_t>(N));
    } catch (const std::out_of_range& e) {
        std::printf("  std::vector::at 抛出：%s\n", e.what());
    }
    try {
        (void)arr->at(static_cast<std::size_t>(N));
    } catch (const std::out_of_range& e) {
        std::printf("  std::array::at  抛出：%s\n", e.what());
    }
    delete arr;
    return 0;
}
```

`实测数据`
`Text`

```text
== 对象自身大小 ==
  int[1000000]              4000000 字节
  std::array<int,1000000>   4000000 字节（不多一个字节）
  std::vector<int>          24 字节（另有堆上的缓冲区）

== 同样的遍历做 200 轮，取最快的一轮 ==
  C 数组       0.0940 ms
  std::array   0.0940 ms
  std::vector  0.0940 ms
  （sink=600000000）

== 越界取第 1000000 个元素 ==
  C 数组的 c_array[1000000] 与 std::array 的 operator[] 都不检查，
  越界是未定义行为，因此这里不去执行它。
  std::vector::at 抛出：vector::_M_range_check: __n (which is 1000000) >= this->size() (which is 1000000)
  std::array::at  抛出：array::at: __n (which is 1000000) >= _Nm (which is 1000000)
```

**四百万字节的 C 数组与四百万字节的 `std::array` 大小完全相同，
遍历一百万次的耗时也在同一档（这一次三者都是 0.0940 ms）。**
包一层不花钱，这在 C++ 里不是特例，而是标准对有零开销意图的设施的一贯要求：
`std::array` 不许多占一个字节的空间，也不许让访问变慢。

> [!TIP]
> **想同时拿到「`std::array` 的接口」与「C 数组的布局」时，直接用它。**
> 它适合放在结构体里当定长缓冲、适合当固定大小的表、
> 也适合替换那些「长度是编译期常量」的 C 数组——编译器会继续按同样的方式优化它。

## 2.4 越界：`[]` 不检查，`at` 检查

`std::array` 与 `std::vector` 都给了两套取元素的接口：
`operator[]` 与 C 数组一样不做检查，`at` 会检查并在越界时抛 `std::out_of_range`。

`实测数据`
`Text`

```text
  std::vector::at 抛出：vector::_M_range_check: __n (which is 1000000) >= this->size() (which is 1000000)
  std::array::at  抛出：array::at: __n (which is 1000000) >= _Nm (which is 1000000)
```

**注意这两条消息里没有「哪里调用的」**：异常对象只带越界的下标与容器大小。
调试时要在抛出点附近下断点，或者自己把 `e.what()` 连同调用位置一起记下来。

> [!WARNING]
> **`operator[]` 越界是未定义行为，不是「读到随机值」这么轻。**
> 越界写可能覆盖别的对象、可能让程序在几百行之后才崩、也可能什么都不发生——
> 后一种最危险，因为它把 bug 藏起来了（越界的几种结局见《04-语法/08-数组、指针与引用.md》第 5.1 节）。

那么 `at` 的检查要花多少钱？第 3.5 小节的程序里有一项对照：
随机访问一百万个元素，`operator[]` 用了 0.502 ms，`at` 用了 0.584 ms，
两者在同一量级——在 `-O2` 下这台机器上测不出这次检查的代价。
**要不要用 `at`，取决于「越界后继续跑」的风险有多大，而不是这点开销。**
数据来自外部输入、下标由计算得出时，用 `at` 换一个能抓住的异常通常更划算。

---

# 第 3 节 `std::vector`：会自己长大的连续数组

## 3.1 三个指针撑起的容器

`std::vector` 的对象里只有三个指针：缓冲区开头、最后一个元素的下一个位置、缓冲区末尾。
`size()` 与 `capacity()` 分别是它们的差，因此都是 `O(1)`。

`C++`

```cpp
/* three_pointers.cpp
 * 编译：g++ -std=c++17 -O2 three_pointers.cpp -o three_pointers
 * size 与 capacity 的关系：容量只增不减，size 随元素增减 */
#include <cstdio>
#include <vector>

int main() {
    std::vector<int> v;
    std::printf("空 vector：            size=%zu capacity=%zu\n", v.size(), v.capacity());
    v.push_back(7);
    std::printf("push_back 一次：       size=%zu capacity=%zu\n", v.size(), v.capacity());
    v.reserve(8);
    std::printf("reserve(8)：           size=%zu capacity=%zu\n", v.size(), v.capacity());
    v.push_back(1);
    v.push_back(9);
    std::printf("再 push_back 两次：    size=%zu capacity=%zu\n", v.size(), v.capacity());
    v.pop_back();
    v.pop_back();
    std::printf("pop_back 两次：        size=%zu capacity=%zu（容量没有还回去）\n",
                v.size(), v.capacity());
    v.clear();
    std::printf("clear()：              size=%zu capacity=%zu（容量还是没有还回去）\n",
                v.size(), v.capacity());
    return 0;
}
```

`实测数据`
`Text`

```text
空 vector：            size=0 capacity=0
push_back 一次：       size=1 capacity=1
reserve(8)：           size=1 capacity=8
再 push_back 两次：    size=3 capacity=8
pop_back 两次：        size=1 capacity=8（容量没有还回去）
clear()：              size=0 capacity=8（容量还是没有还回去）
```

**删除元素不释放内存。** `pop_back` 与 `clear` 只改 `size`，缓冲区原样留着，
因为「刚删完的元素旁边很可能马上又要放新的」。想把内存真的还回去，得另说一句
（见第 3.8 小节）。

## 3.2 容量不够时：换一块更大的，把元素搬过去

`push_back` 在 `size == capacity` 时必须先扩容。扩容做的事只有三步：
要一块更大的内存、把已有元素搬过去、释放旧的那块。**元素一旦搬家，地址就变了。**

`实测数据`
`Text`

```text
== vector<int> push_back 的 capacity 变化（每次增长时打印）==
size= 1 capacity=  1  倍数=0.000  数据地址=000001ce25eb2640
size= 2 capacity=  2  倍数=2.000  数据地址=000001ce25eb2660
size= 3 capacity=  4  倍数=2.000  数据地址=000001ce25eb2640
size= 5 capacity=  8  倍数=2.000  数据地址=000001ce25eb2660
size= 9 capacity= 16  倍数=2.000  数据地址=000001ce25eb2690
size=17 capacity= 32  倍数=2.000  数据地址=000001ce25eb26e0
size=33 capacity= 64  倍数=2.000  数据地址=000001ce25eb2770
```

（这一组数字来自 `A-02` 第 5 节的程序，它同时打印了另外几种容器的对照。）

**每次增长，数据地址都换了一个。** 这一列地址是第 3.9 小节「失效」的根源：
扩容之后，原来拿到的指针、引用、迭代器指向的都是已经被释放的旧缓冲区。
至于每次涨多少倍——上表里全是 2.000，那是这一套标准库的选择，不是标准的规定。

## 3.3 增长倍数由实现决定

标准对扩容只提了两条要求：摊还常数时间，以及扩容时元素要按顺序搬过去。
**倍数本身没有规定。** 同一份源码在两套标准库上跑，得到两条完全不同的曲线。

`C++`

```cpp
/* growth_two_libs.cpp
 * 编译（GCC）：g++ -std=c++17 -O2 growth_two_libs.cpp -o growth_gcc
 * 编译（MSVC）：cl /nologo /utf-8 /std:c++17 /O2 /EHsc growth_two_libs.cpp /Fe:growth_msvc.exe
 * 同一份源码在两套标准库上的容量增长：倍数由实现定，标准不管 */
#include <cstdio>
#include <vector>

int main() {
    constexpr int N = 10000000;              // 一千万
    std::vector<int> v;
    std::size_t last = 0;
    long long grows = 0;
    long long moved = 0;
    int shown = 0;

    std::printf("size -> capacity（前 10 次增长）\n");
    for (int i = 0; i < N; ++i) {
        if (v.size() == v.capacity()) {      // 这一次 push_back 一定要扩容
            ++grows;
            moved += static_cast<long long>(v.size());
        }
        v.push_back(i);
        if (v.capacity() != last) {
            if (shown < 10) {
                ++shown;
                std::printf("  %8zu -> %8zu   倍数 %.3f\n", v.size(), v.capacity(),
                            last ? double(v.capacity()) / double(last) : 0.0);
            }
            last = v.capacity();
        }
    }
    std::printf("扩容次数 %lld，搬迁元素总数 %lld，最终 capacity %zu，"
                "空槽比例 %.1f%%，sizeof(vector<int>) %zu\n",
                grows, moved, v.capacity(),
                100.0 * (double(v.capacity()) - double(N)) / double(v.capacity()),
                sizeof(std::vector<int>));
    return 0;
}
```

`实测数据`
`Text`

```text
== GCC 15.2.0（libstdc++），g++ -std=c++17 -O2 ==
size -> capacity（前 10 次增长）
         1 ->        1   倍数 0.000
         2 ->        2   倍数 2.000
         3 ->        4   倍数 2.000
         5 ->        8   倍数 2.000
         9 ->       16   倍数 2.000
        17 ->       32   倍数 2.000
        33 ->       64   倍数 2.000
        65 ->      128   倍数 2.000
       129 ->      256   倍数 2.000
       257 ->      512   倍数 2.000
扩容次数 25，搬迁元素总数 16777215，最终 capacity 16777216，空槽比例 40.4%，sizeof(vector<int>) 24
```

`实测数据`
`Text`

```text
== MSVC 19.44（Microsoft STL），cl /utf-8 /std:c++17 /O2 ==
size -> capacity（前 10 次增长）
         1 ->        1   倍数 0.000
         2 ->        2   倍数 2.000
         3 ->        3   倍数 1.500
         4 ->        4   倍数 1.333
         5 ->        6   倍数 1.500
         7 ->        9   倍数 1.500
        10 ->       13   倍数 1.444
        14 ->       19   倍数 1.462
        20 ->       28   倍数 1.474
        29 ->       42   倍数 1.500
扩容次数 41，搬迁元素总数 23917332，最终 capacity 11958657，空槽比例 16.4%，sizeof(vector<int>) 24
```

**两套标准库的对象都是 24 字节（三个指针），增长策略却完全不同：**
一套每次翻倍，另一套每次乘 1.5。随之而来的两个数字也分道扬镳——
放一千万个 `int`，翻倍的那套只扩容 25 次、最终浪费 40.4% 的槽位；
乘 1.5 的那套扩容 41 次、浪费 16.4%。

> [!IMPORTANT]
> **「`vector` 扩容是翻倍」这句话不准确**：准确的说法是
> 「`vector` 扩容的倍数由实现定，常见取值有 2 与 1.5，两者都满足摊还常数时间」。
> 需要确定行为时不要推理倍数，直接测。

`待确认`

1.5 与 2 这两个取值背后的完整取舍（内存复用、分配器行为、增长上限）
没有查到权威出处，本机也无法判定哪一套更优。
可以确定的是：两者的搬迁总次数都在 `O(n)` 量级，摊还到每个元素都是常数次。

## 3.4 搬迁的总量：摊还常数是什么意思

看两条曲线的「搬迁元素总数」：放一千万个元素，
翻倍的那套共搬了 16777215 次，乘 1.5 的那套搬了 23917332 次。

| | 扩容次数 | 搬迁元素总数 | 平均每个元素被搬 | 最终空槽比例 |
|---|---|---|---|---|
| 翻倍（libstdc++） | 25 | 16777215 | **1.68 次** | 40.4% |
| 乘 1.5（Microsoft STL） | 41 | 23917332 | **2.39 次** | 16.4% |

**这就是摊还常数的含义**：单次 `push_back` 大多数时候只写一个元素，
偶尔一次要搬走全部元素；把那些「偶尔」平摊到每个元素上，
每个元素负担的搬迁次数是个常数——1.68 或 2.39，与元素总数无关。
`n` 个元素的 `push_back` 总代价是 `O(n)`，不是 `O(n²)`。

> [!TIP]
> **倍数越大，搬迁越少、浪费越多。** 翻倍的那套每元素只搬 1.68 次，
> 代价是最终可能空着一小半内存；乘 1.5 的那套搬得更多，内存利用率更高。
> 两条路都成立，选哪条是实现的事；写代码的人只要记住
> **「扩容会发生、扩容时元素会搬家」**，并据此决定要不要先 `reserve`。

## 3.5 `reserve`：把搬迁次数压到零

既然扩容的代价是「搬走全部元素」，那么在知道大概要放多少个元素时，
**提前把容量要够**就能把这次搬家省掉。

`C++`

```cpp
/* vector_internals.cpp    编译：g++ -std=c++17 -O2 vector_internals.cpp -o vector_internals
 * vector 扩容：分配次数、元素搬迁次数的精确计数，以及 reserve 的耗时对照。
 * 每项取 5 轮中的最小值，降低调度噪声。 */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <vector>

static long long g_alloc = 0;
static long long g_alloc_bytes = 0;
static long long g_reloc = 0;          // 扩容时被搬走的元素个数

void* operator new(std::size_t n) {
    ++g_alloc;
    g_alloc_bytes += static_cast<long long>(n);
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

struct Tracked {
    int v;
    Tracked(int x = 0) : v(x) {}
    Tracked(const Tracked& o) : v(o.v) { ++g_reloc; }               // 拷贝也算搬迁
    Tracked(Tracked&& o) noexcept : v(o.v) { ++g_reloc; }           // 移动也算
    Tracked& operator=(const Tracked&) = default;
    Tracked& operator=(Tracked&&) = default;
};

template <class F>
static double best_ms(int rounds, F&& f) {
    double best = 1e300;
    for (int i = 0; i < rounds; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        f();
        const auto t1 = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    return best;
}

int main() {
    constexpr int N = 100000;

    std::printf("== 精确计数：插入 10 万个 Tracked 对象 ==\n");
    {
        g_alloc = g_alloc_bytes = g_reloc = 0;
        std::vector<Tracked> v;
        for (int i = 0; i < N; ++i) v.push_back(Tracked(i));
        std::printf("  不 reserve：堆分配 %lld 次、共 %lld 字节，元素搬迁 %lld 次，"
                    "最终 capacity=%zu\n",
                    g_alloc, g_alloc_bytes, g_reloc, v.capacity());
    }
    {
        g_alloc = g_alloc_bytes = g_reloc = 0;
        std::vector<Tracked> v;
        v.reserve(N);
        for (int i = 0; i < N; ++i) v.push_back(Tracked(i));
        std::printf("  reserve   ：堆分配 %lld 次、共 %lld 字节，元素搬迁 %lld 次，"
                    "最终 capacity=%zu\n",
                    g_alloc, g_alloc_bytes, g_reloc, v.capacity());
    }

    std::printf("\n== 耗时对照（每项 5 轮取最小，单位毫秒）==\n");
    std::printf("  %10s %14s %14s %8s\n", "n", "不 reserve", "reserve", "倍数");
    for (int n : {100000, 1000000, 10000000}) {
        const double a = best_ms(5, [&] {
            std::vector<int> v;
            for (int i = 0; i < n; ++i) v.push_back(i);
        });
        const double b = best_ms(5, [&] {
            std::vector<int> v;
            v.reserve(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) v.push_back(i);
        });
        std::printf("  %10d %14.3f %14.3f %7.2fx\n", n, a, b, a / b);
    }

    std::printf("\n== 边界检查的代价：随机访问 100 万次（N=100 万）==\n");
    std::vector<int> v(1000000, 7);
    std::vector<std::size_t> idx(1000000);
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = (i * 7919) % v.size();
    volatile long long sink = 0;
    const double t_sub = best_ms(5, [&] {
        long long s = 0;
        for (std::size_t i = 0; i < idx.size(); ++i) s += v[idx[i]];
        sink += s;
    });
    const double t_at = best_ms(5, [&] {
        long long s = 0;
        for (std::size_t i = 0; i < idx.size(); ++i) s += v.at(idx[i]);
        sink += s;
    });
    std::printf("  operator[]  %8.3f ms\n", t_sub);
    std::printf("  at          %8.3f ms\n", t_at);
    std::printf("  （sink=%lld）\n", static_cast<long long>(sink));

    std::printf("\n== at 越界 ==\n");
    try {
        std::vector<int> small(3, 0);
        (void)small.at(5);
    } catch (const std::out_of_range& e) {
        std::printf("  捕获 std::out_of_range：%s\n", e.what());
    }

    std::printf("\n== shrink_to_fit ==\n");
    std::vector<int> s;
    s.reserve(1000);
    for (int i = 0; i < 10; ++i) s.push_back(i);
    std::printf("  reserve(1000) 后填 10 个：size=%zu capacity=%zu\n", s.size(), s.capacity());
    s.shrink_to_fit();
    std::printf("  shrink_to_fit 后：         size=%zu capacity=%zu\n", s.size(), s.capacity());
    return 0;
}
```

`实测数据`
`Text`

```text
== 精确计数：插入 10 万个 Tracked 对象 ==
  不 reserve：堆分配 18 次、共 1048572 字节，元素搬迁 231071 次，最终 capacity=131072
  reserve   ：堆分配 1 次、共 400000 字节，元素搬迁 100000 次，最终 capacity=100000

== 耗时对照（每项 5 轮取最小，单位毫秒）==
           n    不 reserve        reserve   倍数
      100000          0.305          0.064    4.77x
     1000000          2.098          1.436    1.46x
    10000000         28.084         11.649    2.41x

== 边界检查的代价：随机访问 100 万次（N=100 万）==
  operator[]     0.502 ms
  at             0.584 ms
  （sink=70000000）

== at 越界 ==
  捕获 std::out_of_range：vector::_M_range_check: __n (which is 5) >= this->size() (which is 3)

== shrink_to_fit ==
  reserve(1000) 后填 10 个：size=10 capacity=1000
  shrink_to_fit 后：         size=10 capacity=10
```

**不 `reserve` 时：18 次堆分配、1048572 字节、元素被搬了 231071 次；
`reserve(100000)` 之后：1 次分配、400000 字节、搬迁 100000 次。**
搬迁次数从 23 万降到 10 万——那 10 万次是 `std::vector<Tracked> v;` 构造
`Tracked(i)` 临时对象时产生的，不是扩容引起的。
内存占用也从 1048572 降到 400000 字节，正好省掉了那 131072 个空槽。

耗时上的收益随规模变化：

| n | 不 `reserve` | `reserve` | 倍数 |
|---|---|---|---|
| 100000 | 0.305 ms | 0.064 ms | 4.77x |
| 1000000 | 2.098 ms | 1.436 ms | 1.46x |
| 10000000 | 28.084 ms | 11.649 ms | 2.41x |

**三次里 `reserve` 都更快，倍数在一到五倍之间波动。**
倍数不稳的原因是那条「不 `reserve`」的曲线里掺着分配器的行为：
扩容时要向系统要一块更大的内存，而分配器拿到的新块未必正好合适，
它可能要从别处挪一块出来。**结论（先 `reserve` 更好）稳定，具体倍数不稳定。**

> [!TIP]
> **知道要放多少个元素时，先 `reserve`。** 三种场合收益最明显：
> 要放的元素个数已知（读文件前先数行数、协议里带长度字段）；
> 元素本身很大或拷贝很贵（`Tracked` 这种）；
> 在性能敏感的循环里反复 `push_back`，而循环外面就能估出上界。
> **估错了也不要紧**：估少了只是多扩容一次，估多了浪费一些内存。

> [!CAUTION]
> **`reserve` 之后不要用旧的下标或迭代器去写。**
> `reserve` 只改容量，不改 `size`：`v.reserve(100)` 之后
> `v[0] = 1` 写的是一块「已分配但没有元素」的内存，是未定义行为。
> 要用 `push_back` 或 `resize`，让 `size` 跟上去。

## 3.6 搬的时候是移动还是拷贝

扩容要「把已有元素搬到新缓冲区」。搬有两种做法：拷贝构造（原对象留着，按内容再造一份）
与移动构造（把资源直接接过来，原对象随即销毁）。
移动显然更快，但有一个条件：**移动过程不能抛异常。**

`C++`

```cpp
/* move_or_copy.cpp    编译：g++ -std=c++17 -O2 move_or_copy.cpp -o move_or_copy
 * 扩容时元素是被搬走还是被逐字节复制：看移动构造有没有 noexcept */
#include <cstdio>
#include <utility>
#include <vector>

struct NoThrowMove {
    int v;
    char pad[64];
    explicit NoThrowMove(int x = 0) : v(x), pad{} {}
    NoThrowMove(const NoThrowMove& o) : v(o.v), pad{} { ++copies; }
    NoThrowMove(NoThrowMove&& o) noexcept : v(o.v), pad{} { ++moves; }
    static int copies, moves;
};
int NoThrowMove::copies = 0;
int NoThrowMove::moves = 0;

struct ThrowMove {
    int v;
    char pad[64];
    explicit ThrowMove(int x = 0) : v(x), pad{} {}
    ThrowMove(const ThrowMove& o) : v(o.v), pad{} { ++copies; }
    ThrowMove(ThrowMove&& o) : v(o.v), pad{} { ++moves; }        // 没有 noexcept
    static int copies, moves;
};
int ThrowMove::copies = 0;
int ThrowMove::moves = 0;

template <class T>
static void run(const char* name) {
    T::copies = 0;
    T::moves = 0;
    std::vector<T> v;
    for (int i = 0; i < 1000; ++i) v.push_back(T(i));
    std::printf("%-14s 1000 次 push_back：拷贝构造 %d 次，移动构造 %d 次，capacity=%zu\n",
                name, T::copies, T::moves, v.capacity());
}

int main() {
    run<NoThrowMove>("noexcept 移动");
    run<ThrowMove>("无 noexcept");
    return 0;
}
```

`实测数据`
`Text`

```text
noexcept 移动 1000 次 push_back：拷贝构造 0 次，移动构造 2023 次，capacity=1024
无 noexcept   1000 次 push_back：拷贝构造 1023 次，移动构造 1000 次，capacity=1024
```

**两个结构体只差 `noexcept` 一个词，扩容时的行为就分岔了。**
第一行里扩容搬了 1023 次，走的全是移动构造；第二行里同样的 1023 次搬迁，
走的是拷贝构造——`moves` 只有 1000 次，那 1000 次是构造 `T(i)` 临时对象产生的。

原因是 `vector` 要对扩容给一个强保证：**扩容失败时容器必须保持原样。**
如果搬了一半才发现某个元素的移动构造抛了异常，原缓冲区已经被改动过、
新缓冲区又没建完，两份都不完整。拷贝构造没有这个问题——拷贝失败时原对象毫发无损。
因此标准库的选择是：**移动构造标了 `noexcept` 就用移动，否则退回拷贝。**

> [!TIP]
> **自己写的类如果要放进容器，移动构造与移动赋值都加上 `noexcept`。**
> 这不是可选的优化：少了它，`vector` 扩容会退化成深拷贝，
> 元素越大、拷贝越贵，差距越明显（见《05-类与面向对象/05-拷贝与移动.md》第 4 节）。

## 3.7 在中间插入与删除

连续存储最贵的一类操作是「在中间插入或删除」：
插入点之后的所有元素都要往后或往前挪一格。标准对这件事的说法很直接。

`文档`

> "Complexity: The complexity is linear in the number of elements inserted
> plus the distance to the end of the vector."
>
> —— N4659 §26.3.11.5

**「插入的元素个数加上插入点到末尾的距离」**，与容器总大小无关——
但这已经足够贵：如果每次都插在开头，`n` 次插入的总代价是 `O(n²)`。

`实测数据`
`Text`

```text
== 在中间（1/2 处）插入 n 个元素，单位：毫秒 ==
         n       vector        deque         list
      1000        0.035        0.052        0.047
     10000        0.777        3.413        0.408
    100000      111.584      372.268        3.828

== 在头部插入 n 个元素，单位：毫秒 ==
         n       vector        deque         list vector+reverse
      1000        0.031        0.002        0.072        0.007
     10000        1.334        0.030        0.569        0.070
    100000      250.982        0.248        3.998        0.328
   1000000    34742.979        1.949       41.435        1.957
```

（完整的程序与另外两种容器的数据见《09-高阶数据结构/A-02-链式存储：list 与 forward_list.md》第 5 节。）

两次插入都插了十万次：中间插入 111.584 ms，头部插入 250.982 ms，
而链式存储只要 3.828 与 3.998 ms。**但同一张表里还有一列更值得看**：
「先 `push_back` 再 `reverse`」在头部插入的场景下只花了 0.328 ms，
比链表还快十几倍。

> [!IMPORTANT]
> **「`vector` 中间插入慢」这句话只在「真的每次都要插在中间」时成立。**
> 如果元素的到达顺序可以调整，先追加、最后统一排一次序或翻一次面，
> 往往比直接用链表更快——因为整批操作的代价从 `O(n²)` 降到了 `O(n log n)` 或 `O(n)`。
> 判断依据是**访问模式**，不是「`vector` 插入慢」这条结论。

## 3.8 容量只增不减

`pop_back`、`erase`、`clear` 都只改元素、不改容量。
要把多出来的内存还回去，`std::vector` 提供了一个请求：

`文档`

> "Effects: shrink_to_fit is a non-binding request to reduce capacity() to size().
> [Note: The request is non-binding to allow latitude for
> implementation-specific optimizations. —end note]"
>
> —— N4659 §26.3.11.3

**注意 `non-binding`（非强制）。** 实现可以不理它。本机这一套是理会的：

`实测数据`
`Text`

```text
== shrink_to_fit ==
  reserve(1000) 后填 10 个：size=10 capacity=1000
  shrink_to_fit 后：         size=10 capacity=10
```

从 1000 降到 10，元素一个没动。这一次收缩本身是一次扩容的反向操作：
它要申请一块新内存、把元素搬过去、释放旧的，因此代价是 `O(n)`，
**不要频繁调用它**。常见的正确用法是「一批数据装完、以后只读」时调一次。

> [!TIP]
> **「交换一个空的 `vector`」是另一条释放内存的路子**：
> `std::vector<int>().swap(v);` 把 `v` 与一个临时空容器交换，
> 内存随临时对象析构而释放，且不受 `shrink_to_fit` 非强制性的影响。
> 两种做法的效果在本机一致，选哪一种看代码的可读性。

## 3.9 什么时候会失效

「失效」指的是：容器结构变化之后，之前拿到的指针、引用、迭代器不再可用。
`std::vector` 的规则由扩容决定。

`文档`

> "Remarks: Causes reallocation if the new size is greater than the old capacity.
> Reallocation invalidates all the references, pointers, and iterators
> referring to the elements in the sequence. If no reallocation happens,
> all the iterators and references before the insertion point remain valid."
>
> —— N4659 §26.3.11.5

`文档`

> "Effects: Invalidates iterators and references at or after the point of the erase."
>
> —— N4659 §26.3.11.5

| 操作 | 失效范围 |
|---|---|
| `push_back`、`insert`、`emplace`，**且发生了扩容** | **全部**失效（元素都换了地址） |
| `push_back`、`insert`、`emplace`，**没有扩容** | 插入点**之后**的失效；之前的仍然有效 |
| `reserve`、`shrink_to_fit`、`resize` 变大 | 发生了再分配就全部失效 |
| `erase`、`pop_back` | 删除点**及之后**的失效 |
| `clear` | 全部失效 |
| 只读操作（`[]`、`at`、`size`、遍历） | 不失效 |

**为什么扩容会让全部失效**：前面那一列数据地址已经说明了——
元素搬到了新缓冲区，旧缓冲区被释放，原来指向它的指针全部悬垂。
使用悬垂指针是未定义行为，可能读到旧值、可能崩、也可能什么都不发生。

> [!CAUTION]
> **下面这段代码是错的**：
> `for (auto it = v.begin(); it != v.end(); ++it) { if (bad(*it)) v.push_back(x); }`
> `push_back` 一旦触发扩容，`it` 与 `v.end()` 同时失效，循环的下一步就在访问已释放的内存。
> 正确做法有三种：先 `reserve` 到不会扩容的容量；
> 用**下标**循环并每轮重新读 `v.size()`；
> 或者把要加的元素先收进另一个容器，循环结束后再合并。

---

# 第 4 节 代价

## 4.1 空间：每元素 4 字节

同一件事——放下十万个 `int`——不同的结构付出的堆内存完全不同。

`C++`

```cpp
/* sizes_and_allocs.cpp    编译：g++ -std=c++17 -O2 sizes_and_allocs.cpp -o sizes_and_allocs
 * 连续存储与节点式存储的空间账：容器对象多大、每元素摊到多少堆字节、分配了几次。
 * 用替换全局 operator new/delete 的办法计数；容器用完加 asm 屏障，防止整段被优化掉。 */
#include <cstdio>
#include <cstdlib>
#include <forward_list>
#include <list>
#include <map>
#include <new>
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
static void clobber(C& c) {
    asm volatile("" : : "r"(&c) : "memory");
}

static void reset() { g_calls = 0; g_bytes = 0; }
static void report(const char* what) {
    std::printf("  %-24s %9.2f 字节/元素（分配 %lld 次，共 %lld 字节）\n",
                what, double(g_bytes) / 100000.0, g_calls, g_bytes);
}

int main() {
    std::printf("== 容器对象自身大小（sizeof，64 位）==\n");
    std::printf("  %-24s %3zu\n", "vector<int>", sizeof(std::vector<int>));
    std::printf("  %-24s %3zu\n", "forward_list<int>", sizeof(std::forward_list<int>));
    std::printf("  %-24s %3zu\n", "list<int>", sizeof(std::list<int>));
    std::printf("  %-24s %3zu\n", "map<int,int>", sizeof(std::map<int, int>));
    std::printf("  %-24s %3zu\n", "unordered_map<int,int>", sizeof(std::unordered_map<int, int>));

    constexpr int M = 100000;
    std::printf("\n== 放下 10 万个 int（或键值对），每元素摊到多少堆字节 ==\n");
    { reset(); std::vector<int> c; c.reserve(M);
      for (int i = 0; i < M; ++i) c.push_back(i); clobber(c); report("vector<int>（先 reserve）"); }
    { reset(); std::vector<int> c;
      for (int i = 0; i < M; ++i) c.push_back(i); clobber(c); report("vector<int>（不 reserve）"); }
    { reset(); std::list<int> c;
      for (int i = 0; i < M; ++i) c.push_back(i); clobber(c); report("list<int>"); }
    { reset(); std::forward_list<int> c;
      for (int i = 0; i < M; ++i) c.push_front(i); clobber(c); report("forward_list<int>"); }
    { reset(); std::map<int, int> c;
      for (int i = 0; i < M; ++i) c[i] = i; clobber(c); report("map<int,int>"); }
    { reset(); std::unordered_map<int, int> c;
      for (int i = 0; i < M; ++i) c[i] = i; clobber(c); report("unordered_map<int,int>"); }
    return 0;
}
```

`实测数据`
`Text`

```text
== 容器对象自身大小（sizeof，64 位）==
  vector<int>               24
  forward_list<int>          8
  list<int>                 24
  map<int,int>              48
  unordered_map<int,int>    56

== 放下 10 万个 int（或键值对），每元素摊到多少堆字节 ==
  vector<int>（先 reserve）      4.00 字节/元素（分配 1 次，共 400000 字节）
  vector<int>（不 reserve）     10.49 字节/元素（分配 18 次，共 1048572 字节）
  list<int>                    24.00 字节/元素（分配 100000 次，共 2400000 字节）
  forward_list<int>            16.00 字节/元素（分配 100000 次，共 1600000 字节）
  map<int,int>                 40.00 字节/元素（分配 100000 次，共 4000000 字节）
  unordered_map<int,int>       43.26 字节/元素（分配 100014 次，共 4326480 字节）
```

**一个 `int` 是 4 字节，`vector` 每元素就摊到 4.00 字节——没有一分钱的管理开销。**
链式存储每元素要 24 字节（多出两个指针与对齐填充），
比数据本身多付了五倍。这张表还说明了另一件事：
**「不 `reserve`」的那一行是 10.49 字节**，多出来的 6.49 字节全是扩容留下的空槽与旧块。

## 4.2 分配次数：比字节数更值钱

同一张表里的「分配次数」一列，差别比字节数更悬殊：

| 放十万个元素 | 分配次数 | 每元素字节 |
|---|---|---|
| `vector<int>`（先 `reserve`） | **1** | 4.00 |
| `vector<int>`（不 `reserve`） | 18 | 10.49 |
| `list<int>` | **100000** | 24.00 |
| `map<int,int>` | 100000 | 40.00 |
| `unordered_map<int,int>` | 100014 | 43.26 |

**链式容器每放一个元素就要一次堆分配。** 一次分配的成本不止是那几十个字节：
分配器要在自己的数据结构里找一块合适的空闲块、可能要切分、要更新记账信息，
释放时还要合并相邻空块。这些动作都比「把 4 个字节写进缓冲区」贵得多，
而且**分配次数与元素个数是 1:1**，没有摊还可言。

> [!IMPORTANT]
> **连续存储最大的优势不是省内存，而是省分配。**
> 一次分配换一整块缓冲区的做法，把「每次追加」的成本降到了「写几个字节」，
> 这是 `vector` 在绝大多数场景下都比链式容器快的根本原因。
> 分配器的完整接口与自定义池见 `A-13`；堆本身的行为见
> 《06-更底层/01-对象在哪里：栈、堆与静态区.md》第 5 节。

## 4.3 随机访问与边界检查

第 3.5 小节的程序里有一项对照：对一百万个元素做一百万次随机下标访问，
`operator[]` 用了 **0.502 ms**，`at` 用了 **0.584 ms**。

**两者在同一量级。** `at` 多做的是一次比较与一个不太可能跳转的分支，
在 `-O2` 下这台机器上只测到十几个百分点的差别，而同样一批数据重跑时这个差别还会变号。
要不要用 `at` 的判断标准因此不是性能，而是「越界了以后怎么办」——见第 2.4 小节。

## 4.4 缓存带来的优势

同样遍历一百万个 `int`，连续存储与链式存储的每元素耗时随规模变化。

`C++`

```cpp
/* cache_scale.cpp    编译：g++ -std=c++17 -O2 cache_scale.cpp -o cache_scale
 * 规模对遍历速度的影响：每元素纳秒数随数据量变化。
 * 每个规模固定约 1e9 次元素访问，避免小规模测出来是 0。 */
#include <chrono>
#include <cstdio>
#include <list>
#include <vector>

int main() {
    std::printf("%12s %12s %14s %14s %12s\n",
                "元素数", "字节数", "vector ns/元素", "list ns/元素", "倍数");
    for (long long n : {1000LL, 10000LL, 100000LL, 1000000LL, 10000000LL}) {
        std::vector<int> v(static_cast<std::size_t>(n), 1);
        std::list<int> l(static_cast<std::size_t>(n), 1);
        const long long rounds = 1000000000LL / n;      // 让总访问量大致相同

        volatile long long sink = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (long long r = 0; r < rounds; ++r) {
            long long s = 0;
            for (int x : v) s += x;
            sink += s;
        }
        const auto t1 = std::chrono::steady_clock::now();
        const long long list_rounds = rounds > 20 ? 20 : rounds;
        for (long long r = 0; r < list_rounds; ++r) {
            long long s = 0;
            for (int x : l) s += x;
            sink += s;
        }
        const auto t2 = std::chrono::steady_clock::now();

        const double tv = std::chrono::duration<double, std::nano>(t1 - t0).count() / (double(rounds) * double(n));
        const double tl = std::chrono::duration<double, std::nano>(t2 - t1).count() / (double(list_rounds) * double(n));
        std::printf("%12lld %9lld KB %14.3f %14.3f %11.2fx\n", n, n * 4 / 1024, tv, tl, tl / tv);
        (void)sink;
    }
    return 0;
}
```

`实测数据`
`Text`

```text
   元素数    字节数 vector ns/元素 list ns/元素       倍数
        1000         3 KB          0.197          1.400        7.10x
       10000        39 KB          0.194          2.000       10.29x
      100000       390 KB          0.196          2.318       11.82x
      1000000      3906 KB          0.191          3.841       20.07x
     10000000     39062 KB          0.380          5.332       14.02x
```

**这一次的数据里，连续存储的每元素耗时几乎不随规模变化（0.191 到 0.197 ns），
一直到 39 MB 才升到 0.380 ns。**
链式存储则一路涨：从 1.400 ns 涨到 5.332 ns，因为每个节点散落在堆上，
访问下一个节点要先取出它的地址、再去那块内存取数据，
**硬件预取没有规律可循，每次都可能等一次内存。**

`实测数据`
`Text`

```text
sizeof(vector<int>)=24
sizeof(deque<int>)=80
sizeof(list<int>)=24
sizeof(forward_list<int>)=8
vector         10 轮求和     1.30 ms  （acc=10000000）
deque          10 轮求和     2.76 ms  （acc=10000000）
list           10 轮求和    38.86 ms  （acc=10000000）
forward_list   10 轮求和    45.83 ms  （acc=10000000）
```

一百万个 `int`，同样求和十轮：`vector` 1.30 ms，`list` 38.86 ms，
**相差约 30 倍**。这个倍数不是算法差异（两边都是 `O(n)` 的线性遍历），
而是内存布局差异：一整块内存能让缓存行与预取器满负荷工作，
一串散落的节点不能（机制见《06-更底层/02-对齐、填充与缓存.md》第 3 节）。

> [!IMPORTANT]
> **在「遍历」与「随机访问」这两类操作上，连续存储的优势是结构性的，不是常数因子的差距。**
> 选容器时，先数一数「遍历」与「按下标取」占多大比重；
> 只要它们占主导，链式存储省下的那点插入代价几乎不可能补回来。

## 4.5 `vector<bool>`：一个特化的例外

`std::vector<bool>` 与其他 `vector` 不是同一个东西：
它把每个 `bool` 压缩成**一个二进制位**，为此放弃了「`[]` 返回引用」这条性质。

`C++`

```cpp
/* vector_bool.cpp    编译：g++ -std=c++17 -O2 vector_bool.cpp -o vector_bool
 * vector<bool> 的位压缩：直接用堆分配的字节数对照 vector<char> */
#include <cstdio>
#include <cstdlib>
#include <new>
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
static void clobber(C& c) {
    asm volatile("" : : "r"(&c) : "memory");
}

int main() {
    constexpr std::size_t N = 100000000;      // 一亿个布尔值

    g_calls = g_bytes = 0;
    {
        std::vector<bool> vb(N, true);
        clobber(vb);
        std::printf("vector<bool>  一亿个：堆分配 %lld 次、共 %lld 字节 = %.1f MB，"
                    "capacity=%zu（单位是位）\n",
                    g_calls, g_bytes, double(g_bytes) / 1024.0 / 1024.0, vb.capacity());
    }

    g_calls = g_bytes = 0;
    {
        std::vector<char> vc(N, 1);
        clobber(vc);
        std::printf("vector<char>  一亿个：堆分配 %lld 次、共 %lld 字节 = %.1f MB，"
                    "capacity=%zu（单位是元素）\n",
                    g_calls, g_bytes, double(g_bytes) / 1024.0 / 1024.0, vc.capacity());
    }

    std::printf("\nsizeof(vector<bool>)  = %zu\n", sizeof(std::vector<bool>));
    std::printf("sizeof(vector<char>)  = %zu\n", sizeof(std::vector<char>));

    std::vector<bool> vb(10, false);
    auto proxy = vb[3];                             // 代理对象，不是 bool&
    proxy = true;
    std::printf("vector<bool> 没有 data()；&vb[0] 的类型是 vector<bool>::reference*\n");
    std::printf("通过代理写入后 vb[3]=%d\n", static_cast<int>(vb[3]));
    return 0;
}
```

`实测数据`
`Text`

```text
vector<bool>  一亿个：堆分配 1 次、共 12500000 字节 = 11.9 MB，capacity=100000000（单位是位）
vector<char>  一亿个：堆分配 1 次、共 100000000 字节 = 95.4 MB，capacity=100000000（单位是元素）

sizeof(vector<bool>)  = 40
sizeof(vector<char>)  = 24
vector<bool> 没有 data()；&vb[0] 的类型是 vector<bool>::reference*
通过代理写入后 vb[3]=1
```

**一亿个布尔值，8 倍差距**：12.5 MB 对 95.4 MB。省下来的空间是实打实的，
代价写在最后三行里：

- `vector<bool>` **没有 `data()`**，因此不能用 `memcpy` 之类的接口；
- `vb[0]` 不是 `bool&`，而是一个**代理对象**：它记录「哪一位」，
  赋值时去改那一位。因此 `auto& r = vb[0];` 编译不过，
  `bool* p = &vb[0];` 也编译不过；
- 代理对象与迭代器都不是真正的引用，**标准库的很多算法用不了**，
  这也正是「`vector<bool>` 是不是容器」在委员会里争论多年的原因。

> [!WARNING]
> **不要用 `vector<bool>` 当「位数组」以外的用途。**
> 需要「按位存一大批标志」时它是合适的；需要「能取地址的元素数组」时换成
> `std::vector<char>`、`std::deque<bool>`，或者自己写一个位数组。
> 一个具体的坑：`std::vector<bool>` 的 `[]` 返回临时代理对象，
> 对 `const` 的 `vector<bool>` 取 `[]` 得到的是 `bool` 值而非引用，
> 模板代码里一旦依赖「元素能取地址」就会编译失败。

---

# 第 5 节 怎么选

## 5.1 判据

| 你的情况 | 选择 | 理由 |
|---|---|---|
| 长度是编译期常量 | `std::array` 或 C 数组 | 零开销，能放栈上或结构体里 |
| 长度运行期才知道，之后基本不改 | `std::vector` + 一次 `reserve` | 一次分配，之后只有写内存 |
| 主要是按下标取、遍历、末尾追加 | `std::vector` | 这三件事上它最快 |
| 要频繁在中间插入删除，且位置已经拿到 | 考虑 `std::list`（见 `A-02`） | 插入是 `O(1)` |
| 要频繁在中间插入删除，但位置靠下标给 | 仍先用 `std::vector` | 找位置那一步链表也是 `O(n)`，而遍历它更慢 |
| 一批数据先收齐再处理 | `std::vector` + 最后统一排序或反转 | 把 `O(n²)` 的插入变成一次批量操作 |
| 一批布尔标志 | `std::vector<bool>` | 8 倍空间，但元素不可取地址 |
| 需要「元素地址稳定」 | `std::vector` 加一层间接（存指针），或者 `std::deque` | `vector` 扩容会换地址 |

## 5.2 三个常见误用

**误用一：在循环里 `push_back`，同时又拿着迭代器。**
扩容会让旧迭代器全部失效（第 3.9 小节）。要么先 `reserve`，
要么改成下标循环，要么把新增元素攒到另一个容器里最后合并。

**误用二：把 `vector` 当队列用，从头部 `erase(begin())`。**
每次 `erase` 都要把后面所有元素挪一格，`n` 次出队就是 `O(n²)`。
要从两端进出就用 `std::deque`，要先进先出就用 `std::queue`（见 `A-05`）。

**误用三：把 `vector` 的引用或指针长期存下来。**
只要之后还会 `push_back`，那些引用随时可能悬垂。
需要「地址稳定的元素」时，存下标比存指针安全；
或者改用 `std::deque` / `std::list` 这类不搬元素的结构。

> [!NOTE]
> 连续存储这一章到此为止，收成三句话：
> **它的形状是「一块缓冲区加两个数」，因此按下标取是 `O(1)`、遍历最快、每元素不花额外空间；
> 它的代价是中间插入要挪动、扩容要搬家、搬家会让所有指针失效；
> 想少付这笔代价，就在知道数量时先 `reserve`，在能批量处理时不要逐个插到中间去。**

---

# 术语表

| 术语 | 含义 |
|---|---|
| **连续存储** | 所有元素挤在一块内存里，按固定偏移排列，地址可由下标算出 |
| **缓冲区** | 容器向堆申请的那一整块内存，元素就放在里面 |
| **`size`** | 已经放了几个元素 |
| **`capacity`** | 缓冲区能放下几个元素 |
| **扩容** | `size` 追上 `capacity` 时，另申请一块更大的缓冲区并把元素搬过去 |
| **搬迁** | 扩容时把元素从旧缓冲区搬到新缓冲区，一次移动或一次拷贝 |
| **摊还常数** | 单次操作偶尔很贵（扩容），把这一次的代价分摊到多次操作上算出的平均值 |
| **`reserve`** | 提前把容量要够，避免后续的扩容 |
| **`shrink_to_fit`** | 请求把容量降到与 `size` 相同；标准写明这是非强制请求 |
| **失效** | 容器结构变化后，原先拿到的指针、引用、迭代器不再可用 |
| **退化** | 数组名在表达式里变成指向首元素的指针，长度信息随之丢失 |
| **代理对象** | 不是真正的引用、但用起来像引用的中间对象；`vector<bool>` 的 `[]` 返回它 |

# 附录 A 复现本章节实测

**环境**：Windows 11，g++ 15.2.0（MinGW-w64）；涉及两套标准库对照的两组数字里，
MSVC 侧是 19.44（`cl /utf-8 /std:c++17 /O2`）。
所有 GCC 侧程序的编译命令均为 `g++ -std=c++17 -O2 <源文件> -o <可执行文件>`，
程序首行注释里也写了同一条命令。

| 本章节的数字 | 程序 | 在哪一小节 |
|---|---|---|
| 地址公式、退化、编译器警告 | `array_shape.cpp` | 第 1.1 小节 |
| `array` 是一个值 | `array_is_a_value.cpp` | 第 2.2 小节 |
| 对象大小与遍历耗时 | `array_vs_vector.cpp` | 第 2.3、2.4 小节 |
| `size` 与 `capacity` 的关系 | `three_pointers.cpp` | 第 3.1 小节 |
| 两套标准库的增长曲线 | `growth_two_libs.cpp` | 第 3.3、3.4 小节 |
| 分配次数、搬迁次数、`reserve` 耗时、边界检查、`shrink_to_fit` | `vector_internals.cpp` | 第 3.5 小节 |
| 移动还是拷贝 | `move_or_copy.cpp` | 第 3.6 小节 |
| 每元素堆字节与分配次数 | `sizes_and_allocs.cpp` | 第 4.1、4.2 小节 |
| 每元素纳秒数与遍历总耗时 | `cache_scale.cpp`，遍历总耗时来自 `A-02` 附录的程序 | 第 4.4 小节 |
| `vector<bool>` 的位压缩 | `vector_bool.cpp` | 第 4.5 小节 |
| 中间插入与头部插入的耗时 | 见《09-高阶数据结构/A-02-链式存储：list 与 forward_list.md》第 5 节 | 第 3.7 小节 |

**几点复现说明**：

- `vector_internals.cpp` 与 `cache_scale.cpp` 在大规模那几行上要跑几十秒到一分钟，属正常；
- 地址与计时这两类数字每次运行都会变：地址由系统决定，
  计时受机器负载影响；正文引用的是其中一次的输出，读者重跑不必追求逐位一致；
- `cache_scale.cpp` 的每元素纳秒数对负载格外敏感：同一份程序在不同时刻跑，
  `vector` 那一列在 0.19 到 0.56 ns 之间、`list` 那一列在 1.4 到 11.1 ns 之间都出现过。
  **结论（两者差一个量级）稳定，具体倍数不稳定**；
- `growth_two_libs.cpp` 的 MSVC 侧要加 `/utf-8`，否则源码里的中文在代码页 936 下会被误读，
  程序打印出来的中文会变成乱码。

`待确认`

本章节的计时数据都来自这一台机器（Windows 11 + g++ 15.2.0）。
换编译器、换标准库、换 CPU 都会让绝对值变化；
其中「`vector` 比 `list` 遍历快一个量级」这一条在本机各规模上都成立，
但具体倍数随数据规模与缓存大小而变，读者应以自己机器上重跑的结果为准。
