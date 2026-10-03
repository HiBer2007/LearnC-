# 示例工程（`B-examples`）

本节提供十九个可直接运行的示例。每个示例均自带完整的 `.vscode` 配置，
单独打开该示例文件夹即可开始调试或运行。

教材正文里已经有不少几十行的短演示，因此这里的示例偏**整体演练**：
每个都是一个有明确功能目标的小项目，跨两到六个文件，
把某一章或相邻几章的知识合起来用，并且**程序自己带一组自测**，
跑完会打印「全部通过」或指出第几项不符。

> **说明**：若不清楚编译与链接是怎么回事，建议先阅读《01-编译器/01-编译与链接.md》。
> 想看在真实项目里这些知识如何应用，见《01-编译器/03-嵌入式与交叉编译.md》。
>
> 若尚不清楚调试器的用途与操作方式，建议阅读
> 《02-调试器/01-原理与使用.md》。该文说明了调试器能解决哪些 `printf` 解决不好的问题，
> 以及断点、单步、调用堆栈、数据断点等具体操作。

## 范例索引

| 示例 | 形态 | 界面 | 服务板块 / 对应章节 | 演示重点 |
|---|---|---|---|---|
| [`01-compiler/01-c-single-file`](01-compiler/01-c-single-file/) | C 单文件 | 命令行 | `04-语法`：《04-语法/08-数组、指针与引用.md》第 1、2 节、《04-语法/05-初始化.md》第 4 节 | 指针、数组、初始化 |
| [`01-compiler/02-cpp-single-file`](01-compiler/02-cpp-single-file/) | C++ 单文件 | 命令行 | `04-语法` 与 `05-类与面向对象`：《05-类与面向对象/02-类是一种类型.md》第 1 节、《05-类与面向对象/04-构造与析构.md》第 1 节 | 类、STL 容器、引用 |
| [`02-debugger/01-joint-debug`](02-debugger/01-joint-debug/) | 联合调试（Windows + Linux） | 命令行 | `02-调试器`：《02-调试器/02-跨系统调试.md》章节 | 一个窗口同时调试两个程序 |
| [`03-build-toolchain/01-cmake-c`](03-build-toolchain/01-cmake-c/) | CMake 多文件（C） | 命令行 | `01-编译器` 与 `03-构建工具链`：《01-编译器/01-编译与链接.md》章节、《03-构建工具链/01-构建工具链.md》章节 | 跨文件单步、递归堆栈 |
| [`03-build-toolchain/02-cmake-cpp`](03-build-toolchain/02-cmake-cpp/) | CMake 多文件（C++） | 命令行 | `01-编译器` 与 `03-构建工具链`：同 `01-cmake-c`，另加《05-类与面向对象/11-模板.md》第 5 节 | 同上，另加 STL 整齐打印 |
| [`03-build-toolchain/03-qt-gui`](03-build-toolchain/03-qt-gui/) | CMake 工程，Qt 最小示例（单文件） | GUI（仅 **Qt**） | `03-构建工具链`：《03-构建工具链/03-包管理.md》章节；`05-类与面向对象`：《05-类与面向对象/10-lambda 与函数对象.md》第 1 节 | 把 Qt 接进 CMake 工程：`WITH_QT` 开关、静态链接、信号与槽用 lambda 连接 |
| [`04-syntax/01-cpp-language-core`](04-syntax/01-cpp-language-core/) | C++ 单文件，命令行小工具 | 命令行 | `04-语法`：《04-语法/03-常量与 const.md》第 2、3、5 节、《04-语法/05-初始化.md》第 2、5 节、《04-语法/11-作用域、生存期与链接.md》第 3 节、《04-语法/12-编译期能力.md》第 1 至 4 节 | `const` 正确性、初始化、引用与生存期、`constexpr` |
| [`05-oop/01-cpp-class-basics`](05-oop/01-cpp-class-basics/) | CMake 多文件，值类型项目 | 命令行 + GUI（**Win32** 与 **Qt** 两份） | `05-类与面向对象`：《05-类与面向对象/04-构造与析构.md》第 1 至 3 节、《05-类与面向对象/05-拷贝与移动.md》第 2 至 5 节、《05-类与面向对象/09-运算符重载.md》第 2、3 节、《05-类与面向对象/03-成员与细节.md》第 1、2、4 节 | 自己写一个类：构造析构、拷贝移动、运算符、`const` 成员、静态成员 |
| [`05-oop/02-cpp-inheritance-polymorphism`](05-oop/02-cpp-inheritance-polymorphism/) | CMake 多文件，抽象基类体系 | 命令行 + GUI（**Win32** 与 **Qt** 两份） | `05-类与面向对象`：《05-类与面向对象/07-继承.md》第 1、3、7 节、《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 3、4、5 节 | 继承、虚函数、虚析构、抽象基类、`override`、工厂 |
| [`05-oop/03-cpp-raii-and-templates`](05-oop/03-cpp-raii-and-templates/) | CMake 多文件，小库 + 使用者 | 命令行 | `05-类与面向对象`：《05-类与面向对象/06-RAII 与资源管理.md》第 2 至 4 节、《05-类与面向对象/11-模板.md》第 2、4、5、6 节 | RAII 包装（自己写句柄类）、函数模板与类模板、一次全特化 |
| [`07-standard-library/01-c-stdlib-toolbox`](07-standard-library/01-c-stdlib-toolbox/) | CMake 多文件（C） | 命令行 + GUI（**Win32**） | `07-标准库`：《07-标准库/A-01-输入输出：stdio.md》第 4.3、5.2 小节、《07-标准库/A-02-字符串与内存：string.h.md》第 3.2、5.2 小节、《07-标准库/A-05-工具与其它：stdlib 与杂项.md》第 2 节 | 与 `02` 读同一份输入、出同一份报表的 C 版：`fgets` 逐行读、`strspn` 分词、`strtol` 三件套、`qsort` 比较函数 |
| [`07-standard-library/02-cpp-io-report`](07-standard-library/02-cpp-io-report/) | CMake 多文件（C++） | 命令行 + GUI（**Win32** 与 **Qt** 两份） | `07-标准库`：《07-标准库/B-01-输入输出：iostream.md》第 3.1、6.1、7.1 小节、《07-标准库/B-02-std-string 与 string_view.md》第 7.2 小节 | 与 `01` 做同一件事、报表逐字节相同的 C++ 版：`fstream` / `sstream` / `iomanip` |
| [`07-standard-library/03-cpp-string-text`](07-standard-library/03-cpp-string-text/) | CMake 多文件 | 命令行 | `07-标准库`：《07-标准库/B-02-std-string 与 string_view.md》第 3.2、4.4、6.2 小节 | `std::string` 切分与替换、`string_view` 零拷贝与失效、UTF-8 按字符边界截断 |
| [`07-standard-library/04-cpp-smart-pointers`](07-standard-library/04-cpp-smart-pointers/) | CMake 多文件 | 命令行 | `07-标准库`：《07-标准库/B-03-智能指针的用法.md》第 1.3、2.2、3.1 小节、《07-标准库/B-04-可调用物的包装.md》第 1 节 | `unique_ptr` 工厂、`shared_ptr` 引用计数、`weak_ptr` 断环、`std::function` 回调注册表 |
| [`07-standard-library/05-cpp-numeric-random`](07-standard-library/05-cpp-numeric-random/) | CMake 多文件 | 命令行 + GUI（**Win32**） | `07-标准库`：《07-标准库/B-05-数值.md》第 4、5 节、《07-标准库/A-03-数值、数学与随机.md》第 4 节 | `<random>` 引擎与分布、`<numeric>` 统计、直方图，以及与 `rand()` 的两条弱证据对照 |
| [`07-standard-library/06-cpp-chrono-benchmark`](07-standard-library/06-cpp-chrono-benchmark/) | CMake 多文件 | 命令行 | `07-标准库`：《07-标准库/B-06-时间：chrono.md》第 2、3 节、《07-标准库/B-10-内存与并发的基础设施.md》第 5 节 | `steady_clock` 测耗时、预热与多次测量、分位数，`atomic` 记迭代次数 |
| [`07-standard-library/07-cpp-filesystem-scan`](07-standard-library/07-cpp-filesystem-scan/) | CMake 多文件 | 命令行 + GUI（**Win32** 与 **Qt** 两份） | `07-标准库`：《07-标准库/B-07-文件系统：filesystem.md》第 1、3、5 节 | `<filesystem>` 遍历目录与属性统计、`error_code` 与异常两条错误路线 |
| [`07-standard-library/08-cpp-config-parser`](07-standard-library/08-cpp-config-parser/) | CMake 多文件 | 命令行 | `07-标准库`：《07-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 2、3、4 节、《07-标准库/B-09-工具类（下）：type_traits 与 concepts.md》第 1 节 | `tuple` / `optional` / `variant` 表示配置值，类型特征决定 `get<T>()` 能否取用 |
| [`07-standard-library/09-stdlib-capstone`](07-standard-library/09-stdlib-capstone/) | CMake 多文件，综合 | 命令行 | `07-标准库`：《07-标准库/B-01-输入输出：iostream.md》第 6、7 节、《07-标准库/B-02-std-string 与 string_view.md》第 3、6 节、《07-标准库/B-06-时间：chrono.md》第 3 节（综合前八个示例） | 读文件到出报表的整条流水线：解析、计算、计时、报表，把前八份的能力串起来 |
| [`06-lower-level/01-layout-and-align`](06-lower-level/01-layout-and-align/) | CMake 多文件（C++） | 命令行 | `06-更底层`：《06-更底层/01-对象在哪里：栈、堆与静态区.md》第 1.1、1.2、1.3 小节、《06-更底层/02-对齐、填充与缓存.md》第 2.1、3.2 小节 | 六类对象的地址、`sizeof` / `offsetof` 对照、`packed` 与 `alignas`、伪共享计时 |
| [`06-lower-level/02-volatile-and-registers`](06-lower-level/02-volatile-and-registers/) | CMake 多文件（C）+ 交叉编译 | 命令行（含 semihosting） | `06-更底层`：《06-更底层/03-寄存器、位与 volatile.md》第 1.4、2.1、2.3 小节 | 同一段轮询代码带与不带 `volatile` 的 `-O0` / `-O2` 反汇编对照，真板上 `mww` 与 SysTick 两种触发各跑一遍 |
| [`06-lower-level/03-bare-metal-boot`](06-lower-level/03-bare-metal-boot/) | 交叉编译（C + 汇编 + 链接脚本） | 命令行（含 semihosting） | `06-更底层`：《06-更底层/07-链接脚本与启动代码.md》第 1.2、2.4、2.6、3.3 小节 | 自己的链接脚本与向量表：段表、`Reset_Handler`、`.data` / `.bss`，QEMU 与真板两种跑法 |
| [`06-lower-level/04-symbols-and-linking`](06-lower-level/04-symbols-and-linking/) | CMake 多文件（C + C++） | 命令行 | `06-更底层`：《06-更底层/06-符号与链接属性.md》第 1.2、4.2、6.1 小节 | 弱符号覆盖、静态库成员粒度与产物大小、`extern "C"` 与修饰名 |
| [`06-lower-level/05-interrupt-and-atomic`](06-lower-level/05-interrupt-and-atomic/) | CMake 多文件（C++）+ 交叉编译 | 命令行（含 semihosting） | `06-更底层`：《06-更底层/10-中断、并发与内存序.md》第 2.1、2.2、3.4 小节 | 丢更新复现与三种修法（关中断 / 临界区 / 原子量）、真板上的中断延迟测量 |
| [`06-lower-level/06-binary-tools`](06-lower-level/06-binary-tools/) | CMake 多文件（C） | 命令行 | `06-更底层`：《06-更底层/13-什么时候需要下到这一层.md》第 1.1、1.4 小节、《06-更底层/09-C++ 对象布局与它的硬件代价.md》第 2.1、2.4 小节 | 把 `nm` / `objdump` / `size` / `readelf` 的输出解析成结论，PE 与 ELF 两种产物都能看 |
| [`08-algorithms/01-recursion`](08-algorithms/01-recursion/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/01-递归：把问题交给自己.md》章节 | 递归树的结构量（节点数、叶子数、树高、每层节点数）、三种错法的模拟、三种改法逐列对照 |
| [`08-algorithms/02-memo-divide-backtrack`](08-algorithms/02-memo-divide-backtrack/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/02-记忆化、分治与回溯.md》章节 | 同一道题写三遍（朴素、记忆化、递推）、二分与三分分治的合并代价、N 皇后三种剪枝档位的搜索树规模 |
| [`08-algorithms/03-varargs`](08-algorithms/03-varargs/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/03-不定参数：从 printf 到可变模板.md》章节 | `va_list` 的两个函数、默认实参提升的三行读数、可变参数模板与折叠表达式、`initializer_list` 的拷贝与移动计数 |
| [`08-algorithms/04-search`](08-algorithms/04-search/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/04-查找：从线性到索引.md》章节 | 线性（含哨兵）、二分三种边界、插值查找、手写哈希索引：比较次数、探查槽数与装载因子 |
| [`08-algorithms/05-sort-compare`](08-algorithms/05-sort-compare/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/05-排序（一）：比较排序的三种策略.md》章节 | 六种排序在四类输入形态下的比较次数、搬移次数、递归深度与稳定性 |
| [`08-algorithms/06-sort-noncompare`](08-algorithms/06-sort-noncompare/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/06-排序（二）：不比较也能排.md》章节 | 计数、基数与桶排序的读写次数、辅助数组槽数与字节数、两处退化与稳定性对照 |
| [`08-algorithms/07-search-space`](08-algorithms/07-search-space/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/07-搜索：在状态空间里找路.md》章节 | 六种搜索在同一张地图上的展开节点数、入队与出队次数、路径步数与代价对照 |
| [`08-algorithms/08-graph-structure`](08-algorithms/08-graph-structure/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/08-图上的结构性问题.md》章节 | 拓扑排序、强连通分量、割点与桥、最小生成树，每类问题各配一条独立核对路子 |
| [`08-algorithms/09-greedy-dp`](08-algorithms/09-greedy-dp/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/09-贪心与动态规划.md》章节 | 零钱兑换的贪心与 DP 对照、最长上升子序列四种实现、0/1 背包的二维表与滚动数组 |
| [`08-algorithms/10-string-match`](08-algorithms/10-string-match/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/10-字符串匹配.md》章节 | 朴素、KMP 与 Rabin–Karp 的比较次数与碰撞次数；UTF-8 按字节与按码点两种口径 |
| [`08-algorithms/11-scan-state-machine`](08-algorithms/11-scan-state-machine/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/11-扫描与状态机.md》章节 | 同一台剥 JSON 注释的状态机的三种写法（`switch`、二维转移表、函数指针表）输出逐位相同 |
| [`08-algorithms/12-random-sampling`](08-algorithms/12-random-sampling/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/12-随机与采样.md》章节 | `rand() % n` 的偏差、拒绝采样、Fisher-Yates 的正误对照、蓄水池与加权抽样 |
| [`08-algorithms/13-control`](08-algorithms/13-control/) | CMake 多文件（C++） | 命令行 | `08-一些散落的算法`：《08-一些散落的算法/13-控制：让一个量停在目标上.md》章节 | 开环漂移、开关滞回、P 的稳态误差与积分项、采样周期、抗饱和、滤波与测速、前馈与规划 |
| [`09-data-structures/01-vector-mini`](09-data-structures/01-vector-mini/) | CMake 多文件（C++） | 命令行 | `09-高阶数据结构`：《09-高阶数据结构/B-01-手写：动态数组.md》第 1 至 8 节、《09-高阶数据结构/A-06-迭代器与范围：容器与算法之间的接口.md》第 2 节 | 三指针布局与三条不变式、四档增长倍数、搬迁时拷贝与移动的分岔、搬到一半抛异常之后的收拾 |
| [`09-data-structures/02-list-mini`](09-data-structures/02-list-mini/) | CMake 多文件（C++） | 命令行 | `09-高阶数据结构`：《09-高阶数据结构/B-02-手写：链表.md》第 1 至 7 节、《09-高阶数据结构/A-06-迭代器与范围：容器与算法之间的接口.md》第 1.3、6.1 小节 | 两层节点与哨兵、插入删除只改指针、`splice` 的改指针次数与数元素步数分成两列、迭代器稳定 |
| [`09-data-structures/03-hash-mini`](09-data-structures/03-hash-mini/) | CMake 多文件（C++） | 命令行 | `09-高阶数据结构`：《09-高阶数据结构/B-03-手写：哈希表.md》第 1 至 7 节、《09-高阶数据结构/A-04-按哈希定位：unordered_map.md》第 5.3 小节 | 链地址法与开放寻址共用同一套接口、桶数增长与探测步数、三档哈希函数的对照、墓碑与「清空槽就断链」 |
| [`09-data-structures/04-rb-tree-mini`](09-data-structures/04-rb-tree-mini/) | CMake 多文件（C++） | 命令行 | `09-高阶数据结构`：《09-高阶数据结构/B-04-手写：平衡树.md》第 1 至 7 节、《09-高阶数据结构/A-07-树（一）：从搜索树到平衡.md》第 2 至 5 节 | 不变式校验器与带颜色的按层打印、插入顺序决定修复工作量、随机插删 2000 次逐步与 `std::set` 比对 |
| [`09-data-structures/05-graph-mini`](09-data-structures/05-graph-mini/) | CMake 多文件（C++） | 命令行 | `09-高阶数据结构`：《09-高阶数据结构/B-05-手写：图与并查集.md》第 1 至 4 节、《09-高阶数据结构/A-10-图：关系怎么存.md》第 5 节 | 邻接表、位图矩阵与边表共用一套回调式遍历接口、四个模板算法在三种存法上逐位相同、同一张稀疏图的操作次数对照 |
| [`09-data-structures/06-dsu-mini`](09-data-structures/06-dsu-mini/) | CMake 多文件（C++） | 命令行 | `09-高阶数据结构`：《09-高阶数据结构/B-05-手写：图与并查集.md》第 5 至 7 节、《09-高阶数据结构/A-12-不相交集合：只要连通性.md》第 4、5 节 | 五档并查集写出 974.89 倍差距、离线倒序删边、非根节点上的 `size_` 是陈旧值 |

示例路径分两级：**第一级是板块目录**（`01-compiler`、`02-debugger`、
`03-build-toolchain`、`04-syntax`、`05-oop`、`06-lower-level`、`07-standard-library`、`08-algorithms`、`09-data-structures`），第二级是示例目录。
**板块目录一律用 ASCII 名**，因为示例工程会被构建工具读取，
而 CMake 的 `file(STRINGS)` 与 Qt 的 `syncqt` 在非 ASCII 路径下会失败。



每个示例内均提供 `README.md`，写明了「先读教材哪几节」「这个项目要解决什么问题」
「做完能掌握什么」「分阶段的推进路线」「构建命令」「运行后应当看到什么（含真实输出）」
与「代码里哪几处是要点」。

> **说明**：STL 指 `std::vector` / `std::string` / `std::sort` 这类 **C++ 标准库中现成的**
> 数据结构和算法。**C 语言中没有这些内容**（C 需要自行调用 `malloc` / `free`）。
> 完整讲解见《09-高阶数据结构/A-00-导读：数据结构是问题的形状.md》第 5 节、《08-一些散落的算法/05-排序（一）：比较排序的三种策略.md》第 5 节。
>
> STL 与调试直接相关：GDB 默认无法识别 `vector` 的内部结构，
> 会将其显示为一组内部指针。让 STL 整齐显示的关键是 `setupCommands` 中的
> `-enable-pretty-printing`，以及另外两条真正使其生效的 `python` 命令
> （详见 [`01-compiler/02-cpp-single-file/README.md`](01-compiler/02-cpp-single-file/README.md) 的实测对比）。
>
> **关于 `02-debugger/01-joint-debug`**：它需要 WSL 环境。搭建方法、原理与远程调试见《02-调试器/02-跨系统调试.md》。
> 该示例演示 Windows 客户端与 Linux 服务端在同一窗口中的联合调试，
> 并已验证端到端可用。

---

## 公共约定

这一节写所有示例共用的东西。各示例自己的 `README.md` 不再重复这些内容。

### 一、必须单独打开示例文件夹

VS Code **只识别工作区根目录的 `.vscode`**。

```
正确：文件 → 打开文件夹 → 选中  B-examples\01-compiler\01-c-single-file
         → 使用的是 01 自带的 .vscode，F5 正常工作

错误：打开  B-examples  或  C相关课程  这一层文件夹
         → 使用的是上层的 .vscode，示例自带的配置不生效
```

若在**根工作区**中直接打开 `03-build-toolchain/01-cmake-c/src/main.c` 并按 F5，
使用的将是根目录下的“单文件”配置：该配置会试图把 `main.c` 当作单文件单独编译，
而 `main.c` 调用了 `calc.c` 中的函数，因此会出现**链接失败**：

```
undefined reference to `add'
```

**该现象并非配置损坏**，而是打开方式不正确。需要调试 CMake 示例时，请单独打开对应的示例文件夹。

### 二、构建命令一览

单文件示例在示例目录下直接调用编译器；CMake 示例用预设配置并构建。
下列命令中的 `<示例>` 指该示例的文件夹。

`PowerShell`

```powershell
# 单文件（在 04-syntax/01-cpp-language-core 目录下）
g++ -std=gnu++17 -g -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=GBK main.cpp -o build\gcc\main.exe

# CMake 工程（在对应示例目录下）
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 不用预设时，等价的手写命令
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build/mingw

# MSVC 路线
cmake --preset msvc
cmake --build --preset msvc-debug
```

`cmake --build --preset` **必须在示例目录下执行**：预设文件 `CMakePresets.json`
是按当前目录查找的，在仓库根目录执行会报 `File not found: .../CMakePresets.json`。

### 三、占位符

教材与示例中的路径一律用占位符，不写死某一台机器的安装位置：

| 占位符 | 指什么 |
|---|---|
| `<MinGW>` | MinGW-w64 的安装目录，`g++.exe`、`gdb.exe` 所在处 |
| `<CMake>` | CMake 的安装目录 |
| `<VS>` | Visual Studio 或生成工具的安装目录（本机可能装了多套，见仓库根 `README.md`） |
| `<工作区>` | 本仓库根目录 |
| `<版本号>` | 工具版本号，例如 `15.2.0` |

`.vscode` 的配置里不写这些占位符，而是用 VS Code 自己的变量：
`${workspaceFolder}`、`${file}`、`${fileBasenameNoExtension}`、`${fileDirname}`。

### 四、两种调试路线

每个示例均提供两条路线，按 F5 后在列表中选择：

| 路线 | 配置名里含 | 编译器 | 调试信息格式 | 路径含中文时是否可用 |
|---|---|---|---|---|
| **GDB** | `GDB ·` | MinGW `gcc` / `g++` | DWARF | **不可用**，见下 |
| **MSVC** | `MSVC ·` | MSVC `cl.exe` | PDB | 可用 |

> [!WARNING]
> 工作区路径中含中文时，GDB 无法打开可执行文件，调试无法启动。
> 原因是 GDB 在 Windows 上经 MI 协议接收文件名时期望 ANSI 代码页（936）字节，
> 而 VS Code 发送的是 UTF-8 字节。
> 该问题无法通过任何配置项解决。
> **路径全为英文时不受影响。**
>
> 表现：按 F5 后提示 `Program path ... is missing or invalid`（但文件确实存在），
> 或弹出空的"选择要终止的调试会话"列表后无反应。
>
> **路径含中文时请使用 MSVC 路线。** 若需要调试 gcc 编译的产物，
> 可安装 CodeLLDB 扩展（其 LLDB 不受该限制）。详见
> 《01-编译器/02-环境配置.md》第 1 章第 2 节。
>
> 把示例移到全英文路径后，GDB 路线即可恢复可用。

> **说明**：GDB 只识别 gcc 产出的 DWARF，Visual Studio 调试器只识别 cl 产出的 PDB，两者互不兼容。
> （唯一的例外是 LLDB，见《01-编译器/02-环境配置.md》第 1 章。）

### 五、产物位置

每个示例的产物均位于**各自的 `build/` 子目录**中，互不干扰：

```
01-compiler\01-c-single-file\build\gcc\main.exe              ← gcc 路线
01-compiler\01-c-single-file\build\msvc\main.exe             ← MSVC 路线（另有 .pdb）

03-build-toolchain\01-cmake-c\build\mingw\bin\app.exe        ← Ninja 生成器
03-build-toolchain\01-cmake-c\build\msvc\bin\Debug\app.exe   ← VS 生成器（多一层 Debug）

05-oop\01-cpp-class-basics\build\mingw\bin\app_cli.exe        ← 命令行版
05-oop\01-cpp-class-basics\build\mingw\bin\app_gui_win32.exe  ← GUI 版（Win32，默认构建）
05-oop\01-cpp-class-basics\build\mingw\bin\app_gui_qt.exe     ← GUI 版（Qt，WITH_QT=ON 才有）

07-standard-library\01-c-stdlib-toolbox\build\mingw\bin\app_cli.exe        ← 命令行版
07-standard-library\01-c-stdlib-toolbox\build\mingw\bin\app_gui_win32.exe  ← GUI 版（Win32）
07-standard-library\02-cpp-io-report\build\mingw\bin\app_gui_qt.exe        ← GUI 版（Qt，WITH_QT=ON 才有）
```

`build/` 目录可以随时删除，重新按 F5 时会自动重建；
各示例的 `.gitignore` 均已忽略它。

### 六、字符集：中文输出不乱码的前提

各示例的编译参数中均包含字符集设置：

| 编译器 | 参数 |
|---|---|
| gcc / g++ | `-finput-charset=UTF-8 -fexec-charset=GBK` |
| MSVC cl.exe | `/source-charset:utf-8 /execution-charset:gbk` |

**这两项不可省略**。缺少时 gcc 与 MSVC 都会把字符串常量按 UTF-8 写入 exe，
而中文 Windows 的控制台按 CP936（GBK）解释，中文会显示为 `涓枃` 一类的乱码。
复制本目录的编译参数到自己的工程时，请注意一并复制这两项。

原因与字节级实测对照见《01-编译器/02-环境配置.md》第 9 章第 6 节。

> **注意**：MSVC 参数**不能写成 `/utf-8`**。该选项是
> `/source-charset:utf-8` 与 `/execution-charset:utf-8` 的合并写法，
> 会把执行字符集也设为 UTF-8，中文同样会乱码。

> **注意**：GUI 示例（`05-oop/01-cpp-class-basics`、`05-oop/02-cpp-inheritance-polymorphism`）里的宽字符串不受上述窄字符集选项影响：
> Win32 版的窗口与控件一律用 `W` 结尾的宽字符版本，源码中的 `L"中文"` 由
> gcc 按 UTF-16 写入 exe；核心模块返回的窄字符串再由
> `MultiByteToWideChar(CP_ACP, ...)` 转成宽字符显示。
> Qt 版由 `QString::fromLocal8Bit` 完成同一件事。

### 七、关于 GUI 示例：界面怎么配

**六份示例带界面**，按界面种类分三档：

| 示例 | Win32 版 | Qt 版 |
|---|---|---|
| `05-oop/01-cpp-class-basics` | 有 | 有 |
| `05-oop/02-cpp-inheritance-polymorphism` | 有 | 有 |
| `07-standard-library/02-cpp-io-report` | 有 | 有 |
| `07-standard-library/07-cpp-filesystem-scan` | 有 | 有 |
| `07-standard-library/01-c-stdlib-toolbox` | 有（用 C 写） | 无 |
| `07-standard-library/05-cpp-numeric-random` | 有 | 无 |

**带 Qt 版的那几份各带两份功能相同的界面**，读者按需要挑一份：

| | Win32 版 | Qt 版 |
|---|---|---|
| 源文件 | `src/main_gui_win32.cpp`（`01-c-stdlib-toolbox` 是 `.c`） | `src/main_gui_qt.cpp` |
| 目标名 | `app_gui_win32` | `app_gui_qt` |
| 依赖 | **零** | Qt 6 Widgets |
| 平台 | 仅 Windows | 跨平台 |
| 是否默认构建 | **是** | **否**，要加 `-DWITH_QT=ON` |

| 为什么以 Win32 为默认 | 说明 |
|---|---|
| 零依赖 | 随 Windows 一起来，CMake 的 `WIN32` 关键字（MinGW 下即 `-mwindows`）直接能编，不必装 Qt、WinUI 或任何第三方库 |
| 便于分层 | 界面与逻辑分成两个目标：核心模块编成静态库，命令行版与两份界面都链接它 |
| 便于调试 | 界面出问题时先跑命令行版：逻辑对了，问题就在界面那一层 |

这几个带界面的示例目录结构相同：`core`（静态库，纯逻辑）+ `app_cli`（命令行）
+ `app_gui_win32`（`add_executable(app_gui_win32 WIN32 ...)`，另链接 `user32` 与 `gdi32`）
+ `app_gui_qt`（`add_executable(app_gui_qt WIN32 ...)`，链接 `Qt6::Widgets`）。

另有 [`03-build-toolchain/03-qt-gui`](03-build-toolchain/03-qt-gui/)：它是一个**独立的 Qt 最小示例**，
只演示「把 Qt 接进 CMake 工程」这件事（`WITH_QT` 开关、静态链接、信号与槽），只有 Qt 一份界面。
上述两个示例的 Qt 版则是**范例的 Qt 前端**，与各自的 Win32 版功能相同、共用同一个 `core`。
两者互补：先看 `03-build-toolchain/03-qt-gui` 弄清怎么接线，再看 `05-oop/01-cpp-class-basics` 与 `05-oop/02-cpp-inheritance-polymorphism` 的 `main_gui_qt.cpp` 看真实界面怎么写。

**`WITH_QT` 默认为 `OFF`**：没有 Qt 时工程照常配置、照常构建命令行版与 Win32 版，
不会报错。要看 Qt 版：

`PowerShell`

```powershell
cmake --preset mingw-gdb -DWITH_QT=ON
cmake --build --preset mingw-gdb
```

Qt 需要先自备一份（本仓库不预装、也不写死任何本机路径）：
`WITH_QT=ON` 时工程会包含 `工具/获取依赖/qt-static.cmake`，
由它指向仓库内的静态 Qt（`第三方/qt-static`，已被 `.gitignore` 排除）。
获取与构建步骤见 [`工具/获取依赖/README.md`](../工具/获取依赖/README.md)。

### 八、GUI 怎么无人值守验证

GUI 的结果只在窗口里，肉眼看不等于验证过。仓库提供了
[`工具/GUI冒烟/`](../工具/GUI冒烟/)：一个 PowerShell 脚本，
自动启动程序、按标题找到窗口、给控件发消息（点按钮、改文本、在指定坐标按下左键）、
把控件文本读回来，还可截图并统计某个颜色的像素个数。

> **注意**：它只能驱动 **Win32 标准控件**。Qt 界面里的控件不是 Win32 控件，
> 脚本读不到它们的文本，只能确认窗口存在并截图比对。

### 九、`.vscode` 的用法与配置写法

每个示例的 `.vscode/launch.json` 均带有**逐字段中文注释**，可直接作为模板使用。
如需更多变体（attach 到进程、带参数调试、CodeLLDB、跨平台），
见《01-编译器/02-环境配置.md》第 7 章的 13 套模板。

调试 MSVC 路线（`cppvsdbg`）时，产物路径里会有 `Debug` 这一层；
MinGW 路线（`cppdbg`）没有这一层。路径写错时 F5 会提示找不到程序，
按上一节的产物位置表核对即可。

如需**自行练习配置过程**，可使用同级的 [`C-templates`](../C-templates/) 文件夹。

---

## 附录：历史序号对照

重排前示例按 `01` 至 `10` 的扁平编号排列，重排后该编号已不存在。
**源文件头的注释、`.vscode` 配置里的注释与程序的真实输出（命令行横幅、窗口标题）中仍写着旧编号**，
因为这些属于示例自身的文字，本次整理没有改动。下表供对照旧笔记与旧提交信息。

| 旧编号 | 现路径 |
|---|---|
| `01` | `01-compiler/01-c-single-file` |
| `02` | `01-compiler/02-cpp-single-file` |
| `03` | `03-build-toolchain/01-cmake-c` |
| `04` | `03-build-toolchain/02-cmake-cpp` |
| `05` | `02-debugger/01-joint-debug` |
| `06` | `04-syntax/01-cpp-language-core` |
| `07` | `05-oop/01-cpp-class-basics` |
| `08` | `05-oop/02-cpp-inheritance-polymorphism` |
| `09` | `05-oop/03-cpp-raii-and-templates` |
| `10` | `03-build-toolchain/03-qt-gui` |
