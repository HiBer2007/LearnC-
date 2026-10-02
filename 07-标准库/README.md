# 标准库

本板块收录 **C 与 C++ 的标准库**——但不是它的全部。

**它不追求覆盖整个标准库。** 本板块建立在一条判断上：

> [!IMPORTANT]
> **标准库的价值不在「有多少个函数」，而在「常用的那些用得对不对」。**
> 一个 `snprintf` 用错长度参数，比不会用一个冷门头文件严重得多。
> **因此本板块把常用的部分讲透，不常用的只给一张表。**

---

## 一、为什么分成 A、B 两段

**C++ 的标准库几乎全量包含了 C 的标准库。**

`<cstdio>` 就是 `<stdio.h>` 加一层 `std::` 前缀，`printf`、`fopen`、`memcpy` 一个不少。
既然这样，**先把 C 那半边讲清，再讲 C++ 多出来的是什么**，比混在一起讲省力得多：

| 段 | 讲什么 | 章号 | 篇数 |
|---|---|---|---|
| **A 段** | **C 标准库**：`<stdio.h>`、`<string.h>`、`<math.h>`、`<time.h>`、`<stdlib.h>` 等 | `A-00` … `A-05` | 6 |
| **B 段** | **C++ 标准库**：`<iostream>`、`std::string`、`<memory>`、`<chrono>`、`<filesystem>` 等 | `B-00` … `B-11` | 12 |

**两段是成对的**，每一处对照都能落回对面：

| 同一件事 | A 段 | B 段 |
|---|---|---|
| 往屏幕上写字 | `<stdio.h>` 的 `printf` | `<iostream>` 的 `<<` |
| 处理一段文本 | `char[]` 与 `<string.h>` | `std::string` 与 `string_view` |
| 算个数、取个随机数 | `<math.h>`、`rand` | `<cmath>`、`<random>` |
| 记时间、测耗时 | `<time.h>` 的 `time`、`clock` | `<chrono>` |
| 读写文件、遍历目录 | `<stdio.h>` 与平台 API | `<filesystem>` |

**先读 A 再读 B**。反过来读也能读懂，但会反复遇到「这个 C++ 里对应什么」的问题。

> [!NOTE]
> **A 段与 B 段各有一篇导读**（`A-00`、`B-00`）。
> 两篇导读分别奠定各自那一段的读法，**不是可跳过的开场白**：
> A-00 讲清「C 标准库是怎么组织的、查什么、哪些坑是设计遗留」，
> B-00 讲清「C++ 标准库在 C 之上加了什么、为什么加、与 STL 的边界在哪」。

**最后一篇用 `END-` 开头**（`END-把标准库用对.md`）：
它讲的是两段共用的东西——同一个需求该用哪个头文件、该选 C 的还是 C++ 的、边界在哪，
因此它既不属于 A 段，也不属于 B 段。**`END-` 既是含义也是排序**：它表示「到这里结束」，
而在文件名列表里，`E` 排在 `A`、`B` 之后、`README.md` 的 `R` 之前——
正好落在两段正文的末尾、板块索引的前面，一眼能看出这是收束全板块的一篇。

---

## 二、本板块不装什么

| 内容 | 去哪 |
|---|---|
| `std::vector`、`std::map`、`std::sort`、迭代器 | **`09-高阶数据结构`**——它们是数据结构与算法 |
| CRT 的实现、ABI、内存布局、系统调用 | **`06-更底层`** |
| 类、模板、lambda、异常这些**语言机制** | 已在 `04-语法`、`05-类与面向对象` 讲过，这里只讲**怎么用标准库里的现成件** |

**一条按用途划的线**：`std::string` 技术上是个容器，但它归本板块——
因为它是**处理文本**的工具，与输入输出放在一起讲更顺。

---

## 三、文档索引

### A 段：C 标准库

| 文档 | 内容 | 适合什么时候读 |
|---|---|---|
| [A-00-导读：C 标准库](<A-00-导读：C 标准库.md>) | **C 标准库是怎么组织的**（标准的一部分、`<xxx.h>` 的划分）、**怎么查**、实现定义与未定义在库里的表现、哪些坑是设计遗留 | **读 A 段任何一章之前** |
| [A-01-输入输出：stdio](A-01-输入输出：stdio.md) | 流与缓冲、`printf` 家族的格式化细节、`scanf` 的坑、`fopen` 与文件读写定位、文本与二进制模式、`errno` 与 `perror` | 要读写文件或打印时 |
| [A-02-字符串与内存：string.h](A-02-字符串与内存：string.h.md) | `strlen`/`strcpy`/`strcmp`/`strstr` 一族、`mem*` 系列（**重叠区域为什么必须 `memmove`**）、溢出的真实后果、`strncpy` 的坑与 `snprintf` 的正确用法、数字转换 | 处理 `char[]` 时 |
| [A-03-数值、数学与随机](A-03-数值、数学与随机.md) | `<math.h>` 常用函数、`<float.h>` 与 `<limits.h>` 的极值、`<stdint.h>` 定宽整数与格式宏、`rand` 为什么弱 | 要算数、要随机数时 |
| [A-04-时间与日期：time.h](A-04-时间与日期：time.h.md) | `time`/`clock`/`difftime`、`localtime` 与 `gmtime`、`strftime`、`timespec_get`、**为什么测耗时不该用 `clock()`** | 要记时间或格式化日期时 |
| [A-05-工具与其它：stdlib 与杂项](<A-05-工具与其它：stdlib 与杂项.md>) | `malloc` 一族、`qsort`/`bsearch`、`exit`/`atexit`、`getenv`/`system`、`<assert.h>`、`<ctype.h>`、`<errno.h>`、`<setjmp.h>`、`<signal.h>` | 要用到那些「零散但常用」的头文件时 |

### B 段：C++ 标准库

| 文档 | 内容 | 适合什么时候读 |
|---|---|---|
| [B-00-导读：C++ 标准库与 C 的关系](<B-00-导读：C++ 标准库与 C 的关系.md>) | `<cstdio>` 与 `<stdio.h>` 的区别、`std::` 里的那些 C 名字、**与 STL 的边界**、版本标注习惯 | **读 B 段任何一章之前** |
| [B-01-输入输出：iostream](B-01-输入输出：iostream.md) | 四个标准流、`<<` 与 `>>`、`<iomanip>` 格式化、**流的状态**、`getline`、`<sstream>`、`<fstream>`、**与 `printf` 的取舍与实测** | 要用 C++ 方式读写时 |
| [B-02-std-string 与 string_view](<B-02-std-string 与 string_view.md>) | 构造与容量（**SSO 实测**）、修改与查找、**失效规则**、与 C 字符串互操作、`string_view` 的用途与悬垂风险、数字互转、**编码现实** | 处理文本时 |
| [B-03-智能指针的用法](B-03-智能指针的用法.md) | `unique_ptr` 的删除器与工厂、`shared_ptr` 的控制块与引用计数、**循环引用与 `weak_ptr`**、`make_*` 的取舍、怎么选 | 要管对象生存期时 |
| [B-04-可调用物的包装](B-04-可调用物的包装.md) | `std::function` 的用法与代价、`bind` 与 lambda 的取舍、成员函数绑定、`reference_wrapper`、`invoke` | 要存一个「待会儿再调」的东西时 |
| [B-05-数值](B-05-数值.md) | `<limits>`、`<cmath>`、**`<random>` 的引擎与分布**、`<numeric>`、浮点比较与容差 | 要算数、要随机数时 |
| [B-06-时间：chrono](B-06-时间：chrono.md) | `duration`/`time_point`/`clock`、**`steady_clock` 与 `system_clock` 的区别**、测耗时、`sleep_for` | 要测耗时或做定时时 |
| [B-07-文件系统：filesystem](B-07-文件系统：filesystem.md) | `path` 与编码现实、遍历目录、属性与时间戳、增删改复制、**`error_code` 与异常两条路径** | 要操作文件与目录时 |
| [B-08-工具类（上）：pair、tuple、optional、variant、any](B-08-工具类（上）：pair、tuple、optional、variant、any.md) | 结构化绑定、`tuple` 的取用、**`optional` 表达「可能没有」**、`variant` 与 `visit`、`any` 的边界 | 要返回多个值或表示「多选一」时 |
| [B-09-工具类（下）：type_traits 与 concepts](<B-09-工具类（下）：type_traits 与 concepts.md>) | `<type_traits>` 的分类与常用件、`typeid` 与 `type_info` 的边界、**C++20 标准概念库** | 写模板要做约束时 |
| [B-10-内存与并发的基础设施](B-10-内存与并发的基础设施.md) | 分配器与对齐、`<mutex>`/`<atomic>`/`<condition_variable>` 的**接口用法** | 要加锁或用原子量时 |
| [END-把标准库用对](END-把标准库用对.md) | 头文件命名规则、实现差异、**什么时候不该用标准库**、一页速查 | **最后读** |

---

## 四、建议阅读顺序

`Text`

```text
A 段（C 标准库）
  先弄清这一段的读法          A-00        ← 必读
      ↓
  怎么读写、怎么打印          A-01
      ↓
  怎么处理一段文本            A-02        ← A 段的重心
      ↓
  怎么算、怎么取随机数        A-03
      ↓
  怎么记时间                  A-04
      ↓
  那些零散但常用的头文件      A-05
      ↓
B 段（C++ 标准库）
  弄清 C++ 在 C 之上加了什么   B-00        ← 必读
      ↓
  换个方式读写                B-01        ← 与 A-01 对照
      ↓
  换个方式处理文本            B-02        ← 与 A-02 对照
      ↓
  对象谁来释放                B-03
      ↓
  「待会儿再调」的东西怎么存  B-04
      ↓
  算数与随机                  B-05        ← 与 A-03 对照
      ↓
  时间与耗时                  B-06        ← 与 A-04 对照
      ↓
  文件与目录                  B-07        ← 与 A-01 对照
      ↓
  「可能没有」与「多选一」    B-08
      ↓
  给模板加约束                B-09
      ↓
  加锁与原子量                B-10
      ↓
  收尾：用对                   B-11        ← 最后读
```

**A 段与 B 段之间可以来回读**：读到 B-01 的 `<<` 时回头看 A-01 的 `printf`，
两边的取舍才看得清。**本板块的对照是双向的，每章都给了对面的位置。**

---

## 五、怎么用这个板块

| 你的处境 | 从哪开始 |
|---|---|
| 刚开始学，按顺序读 | `A-00` → … → `A-05` → `B-00` → … → `B-11` |
| 只想把 C 的那半套用对 | A 段六篇 |
| 已经在写 C++，想弄清某个头文件 | 查第三节的索引，直接跳到那一章 |
| 要做一个读写文件、算一算、输出报表的小工具 | 先读 `A-01`、`A-02`、`A-04`，再读 `B-01`、`B-02`、`B-06` |
| 想动手练 | 见第六节 |

---

## 六、配套示例与练习

**本板块的配套件是全书最多的**：每个主题簇都有**一个可运行的示例**（`B-examples/`）
与**一个分阶段的练习模板**（`C-templates/`），而且 C 段与 C++ 段的示例**做同一件事**，
读者可以直接对比两种写法。

| 主题簇 | 示例 | 练习 |
|---|---|---|
| C 段综合 | [`B-examples/07-standard-library/01-c-stdlib-toolbox/`](../B-examples/07-standard-library/01-c-stdlib-toolbox/) | [`C-templates/07-standard-library/01-c-stdlib-toolbox/`](../C-templates/07-standard-library/01-c-stdlib-toolbox/) |
| C++ 输入输出 | [`02-cpp-io-report`](../B-examples/07-standard-library/02-cpp-io-report/) | [`02-cpp-io-format`](../C-templates/07-standard-library/02-cpp-io-format/) |
| `std::string` | [`03-cpp-string-text`](../B-examples/07-standard-library/03-cpp-string-text/) | [`03-cpp-string-text`](../C-templates/07-standard-library/03-cpp-string-text/) |
| 智能指针与可调用物 | [`04-cpp-smart-pointers`](../B-examples/07-standard-library/04-cpp-smart-pointers/) | [`04-cpp-smart-pointers`](../C-templates/07-standard-library/04-cpp-smart-pointers/) |
| 数值与随机 | [`05-cpp-numeric-random`](../B-examples/07-standard-library/05-cpp-numeric-random/) | [`05-cpp-numeric-random`](../C-templates/07-standard-library/05-cpp-numeric-random/) |
| 时间与基准 | [`06-cpp-chrono-benchmark`](../B-examples/07-standard-library/06-cpp-chrono-benchmark/) | [`06-cpp-chrono-benchmark`](../C-templates/07-standard-library/06-cpp-chrono-benchmark/) |
| 文件系统 | [`07-cpp-filesystem-scan`](../B-examples/07-standard-library/07-cpp-filesystem-scan/) | [`07-cpp-filesystem`](../C-templates/07-standard-library/07-cpp-filesystem/) |
| 工具类 | [`08-cpp-config-parser`](../B-examples/07-standard-library/08-cpp-config-parser/) | [`08-cpp-config-parser`](../C-templates/07-standard-library/08-cpp-config-parser/) |
| **毕业练习** | [`09-stdlib-capstone`](../B-examples/07-standard-library/09-stdlib-capstone/) | [`09-stdlib-capstone`](../C-templates/07-standard-library/09-stdlib-capstone/) |

每个示例都**内置自测**（跑完打印「N 项通过」），不靠肉眼看输出。

---

## 七、本板块的约定

- **每个代码块上方标语言**（`` `C` `` / `` `C++` `` / `` `Text` `` …）；
- **数据**：跑出来的数据都用 `` `实测数据` `` 独立成行标出，且都是真跑出来的；
- **标准条文**用 `` `文档` `` 引英文原文并给出草案章节号（C 用 N3220，C++ 用 N4659）；
- **不写本机路径**，用 `<MinGW>`、`<Qt>` 这类占位符；
- 本板块的代码示例**以 C 与 C++ 成对给出**为主——这是「先 C 后 C++」的落点。
