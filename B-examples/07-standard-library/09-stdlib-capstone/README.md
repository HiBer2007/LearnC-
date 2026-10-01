# 示例 `07-standard-library/09-stdlib-capstone` · 标准库综合流水线（命令行）

前八个示例各盯住一组头文件；这一个把它们串起来。程序读一份配置文件，
再读一份英文语料，分词计数，算出总词数、不同词数、平均词长与词长分布，
给四个阶段分别计时，最后出一份文本报表。全程只用标准库，没有第三方依赖。

报表的每一段都是一个对象，由工厂函数造出来（`std::unique_ptr` 独占），
交给一张 `std::function` 回调表保管，渲染时按登记顺序逐段调用。
词频榜在同次数时按词典序排列，分箱与排序都不依赖容器的遍历顺序，
因此同一份输入跑两遍，除耗时段外的输出逐字节相同。

界面形态已经在 01、02、05、07 给出四处，本示例把重心放在把八个能力串成一条流水线，
输出是一份文本报表。

`Text`

```text
09-stdlib-capstone/
  include/capstone.hpp   接口：配置读取、分词、统计、段落、登记表、流水线
  src/capstone.cpp       实现：以上每一件的做法，加 24 项自测
  src/main_cli.cpp       命令行版：解析参数、打印报表与自测
  data/analysis.cfg      配置：top、min_length、lowercase、bins、bar_width
  data/corpus.txt        英文语料，每行一句
  CMakeLists.txt         目标：core（静态库）、app_cli
  CMakePresets.json      mingw-gdb 与 msvc 两套预设
  .vscode/               调试与任务配置（两个调试配置）
```

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《07-标准库/B-01-输入输出：iostream.md》第 7 节 | `<fstream>` 文件 | `read_text_file()` 一次读入整份语料 |
| 《07-标准库/B-01-输入输出：iostream.md》第 6 节 | `<sstream>` 把字符串当成流 | `split_lines()` 与配置解析都靠 `istringstream` 逐行取 |
| 《07-标准库/B-02-std-string 与 string_view.md》第 3 节 | `std::string` 的查找与截取 | 配置行按 `=` 切开，行尾注释按位置截断 |
| 《07-标准库/B-02-std-string 与 string_view.md》第 6 节 | `std::string_view` | 分词、`trim`、`strip_comment` 全程不复制 |
| 《07-标准库/B-03-智能指针的用法.md》第 1 节 | `unique_ptr` 独占所有权 | 报表段落由工厂返回 `std::unique_ptr<Section>` |
| 《07-标准库/B-04-可调用物的包装.md》第 1 节 | `std::function` 把可调用物装进一个类型 | `ReportBuilder` 的回调表按登记顺序渲染各段 |
| 《07-标准库/B-05-数值.md》第 5 节 | `<numeric>` 的一批小算法 | `accumulate` 算总词长与各档计数之和 |
| 《07-标准库/B-06-时间：chrono.md》第 2 节 | `time_point` 与 `clock` | `steady_clock` 给四个阶段各计一次时 |
| 《07-标准库/B-06-时间：chrono.md》第 3 节 | 测一段代码要多久 | `PhaseTimer` 用 `duration<double, std::milli>` 换算成毫秒 |
| 《07-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 3、4 节 | `optional` 与 `variant` | 配置值用 `variant` 装，取不到就是 `nullopt` |
| 《07-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 2 节 | `tuple` 与结构化绑定 | 配置条目摊成 `(段, 键, 值)` 三元组后逐条解包 |
| 《07-标准库/B-09-工具类（下）：type_traits 与 concepts.md》第 1 节 | 类型特征与 `if constexpr` | `get<T>()` 里只给浮点开一条从整数放宽的路 |
| 《07-标准库/B-00-导读：C++ 标准库与 C 的关系.md》第 4 节 | 与 STL 的边界 | `std::map` 与 `std::sort` 属于 STL，本示例只借它们做计数与排序 |
| 《07-标准库/AB-把标准库用对.md》第 1 节 | 头文件命名规则 | 一律用 `<cstddef>`、`<charconv>` 这类 C++ 头文件与 `std::` 名字 |
| 《07-标准库/AB-把标准库用对.md》第 2 节 | 实现差异 | 输出只依赖标准规定的行为，浮点按固定精度打印 |

## 这个项目要解决什么问题

- **一条流水线要能分段。** 读配置、读文本、分词、统计、计时、报表各占一个函数，
  段与段之间只传普通数据结构。任何一段出问题，都能单独拿出来跑一遍。
- **报表要能重复。** 词频榜在同次数时按词典序排，分箱的档界由整数运算算出，
  输出不含地址、时间戳、容器遍历顺序一类会变的东西。
  同一份输入跑两遍，除耗时段外逐字节相同，这一条由自测盯着。
- **耗时要分段量。** 只报一个总数说明不了问题；四段分开之后，
  一眼能看出时间花在读文件还是花在统计上。计时用 `steady_clock`，
  它只往前走，不受系统时间调整影响。
- **所有权要清楚。** 报表段落的生命周期归登记表管，回调只借用裸指针，
  因此不会漏、不会悬垂，也不需要手工 `delete`。

## 做完能掌握什么

- 会把一组标准库设施按用途分工，拼成一条能跑的流水线，而不是堆在一个函数里
- 会用 `unique_ptr` 工厂加 `std::function` 回调表做「可扩展的报表」
- 会用 `steady_clock` 与 `duration<double, std::milli>` 给多个阶段分别计时
- 会用 `accumulate` 做加权求和，并知道分箱这类整数运算怎么写才不出现空档
- 会为一个「输出要稳定」的需求设计排序与格式化规则，并用自测把它固定下来

## 文件

`Text`

```text
include/capstone.hpp   接口：Value、ConfigReader、Corpus、Stats、Section、ReportBuilder、RunResult
src/capstone.cpp       实现：配置解析、分词、统计、四个段落、流水线，加 24 项自测
src/main_cli.cpp       命令行版：解析 --selftest、--config、--input，打印报表与自测
data/analysis.cfg      配置文件：五个键分属 [analysis] 与 [report] 两个段
data/corpus.txt        英文语料：每行一句，词与标点混排
CMakeLists.txt         目标：core（静态库）、app_cli
CMakePresets.json      mingw-gdb 与 msvc 两套预设
.gitignore             忽略 build/ 与 MSVC 中间文件
.vscode/               launch.json、tasks.json、settings.json
```

`core` 是纯逻辑静态库，`app_cli` 只链接它。把 `src/main_cli.cpp` 删掉，
`core` 照样能编译、自测照样能跑。

## 阶段推进

| 阶段 | 目标 | 做什么 | 怎么验收 |
|---|---|---|---|
| **阶段 1** | 跑通流水线 | 配置、编译，运行 `app_cli.exe` | 终端里出现概览、词频前 8、词长分布、耗时四段；自测 24 项全过 |
| **阶段 2** | 改配置看变化 | 把 `data/analysis.cfg` 的 `top` 改成 3、`min_length` 改成 5 | 词频榜只剩 3 条；概览里的总词数下降，最短词长变成 5 或更大 |
| **阶段 3** | 换一份输入 | 用自己的英文文本（几十行即可）配 `--input` 运行 | 概览里的行数与输入文件一致；分箱各档之和仍等于不同词数 |
| **阶段 4** | 加一段报表 | 仿照四个工厂写一个新段落（例如「最长的十个词」），注册到登记表 | 段落数从 4 变成 5，新段落出现在计时段之前；补一条自测后 `--selftest` 项数增加且全过 |
| **阶段 5**（选做） | 遍历目录 | 把 `ifstream` 换成 `<filesystem>`，一次分析目录下所有 `.txt` | 两份以上文件的结果与逐份运行再手工相加一致；读文件那一段耗时包含遍历 |

阶段 2 与阶段 4 是重点：前者验证配置真的在起作用，后者验证报表真的可扩展。

## 构建与运行

`PowerShell`

```powershell
# 在 07-standard-library/09-stdlib-capstone 目录下：配置 + 编译
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 运行（程序按 data/analysis.cfg 与 data/corpus.txt 这两个相对路径找文件，
# 因此要在示例目录下运行）
build\mingw\bin\app_cli.exe

# 换配置或换输入
build\mingw\bin\app_cli.exe --config data\analysis.cfg --input data\corpus.txt

# 只跑自测
build\mingw\bin\app_cli.exe --selftest
```

不想用预设时，等价的手写命令是：

`PowerShell`

```powershell
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build/mingw
```

用 VS Code 打开本文件夹后按 `F5`，有两个配置可选：`GDB · 命令行版`、`MSVC · 命令行版`。
调试路线的选择与产物位置见 [`../../README.md`](../../README.md)。

配置与编译的实测结果（MinGW-w64 g++ 15.2.0，Ninja，Debug）：

`实测数据`

| 步骤 | 命令 | 结果 |
|---|---|---|
| 配置 | `cmake --preset mingw-gdb` | 退出码 0，生成 `build/mingw` 下的 Ninja 工程 |
| 编译 | `cmake --build --preset mingw-gdb` | 退出码 0，四个编译与链接步骤，`-Wall -Wextra` 无警告 |
| 产物 | — | `build\mingw\bin\app_cli.exe` |

编译的完整输出：

`实测数据`
`Text`

```text
[1/4] Building CXX object CMakeFiles/app_cli.dir/src/main_cli.cpp.obj
[2/4] Building CXX object CMakeFiles/core.dir/src/capstone.cpp.obj
[3/4] Linking CXX static library libcore.a
[4/4] Linking CXX executable bin\app_cli.exe
```

## 运行后应当看到什么

`build\mingw\bin\app_cli.exe` 的完整输出：

`实测数据`
`Text`

```text
示例 07-standard-library/09-stdlib-capstone · 标准库综合流水线（命令行版）

== 项目输出 ==
-- 概览 --
  输入文件  : data/corpus.txt（48 行）
  配置      : analysis.top = 8，analysis.min_length = 3，analysis.lowercase = true，analysis.bins = 5，report.bar_width = 40
  总词数    : 420
  不同词数  : 225
  平均词长  : 5.095
  最短/最长 : 3 / 14
-- 词频前 8（不同词共 225 个）--
    1. the                42
    2. that               14
    3. and                13
    4. not                 7
    5. clock               6
    6. report              6
    7. text                6
    8. every               5
-- 词长分布（5 档，按不同词统计）--
  [ 3,  5]   109  ########################################
  [ 6,  7]    69  #########################
  [ 8, 10]    43  ###############
  [11, 12]     2  #
  [13, 14]     2  #
-- 耗时（steady_clock，毫秒）--
  读文件    : 0.085
  分词      : 0.162
  统计      : 0.296
  报表      : 0.038
  合计      : 0.581

没有问题。

== 自测 ==
  [通过] 1. 配置 analysis.top 读成整数 3
  [通过] 2. min_length 与 bins 都读到了
  [通过] 3. lowercase = true 读成布尔真
  [通过] 4. 只写键名 bar_width 也能取到 report 段里的值
  [通过] 5. 配置条目按 (段, 键, 值) 结构化绑定解包，共 5 条，行尾注释没有进值
  [通过] 6. 缺键返回 nullopt，类型不符也返回 nullopt
  [通过] 7. 越界的 top 与 bins 改回默认值，并各记一条问题
  [通过] 8. 标点与数字都是分隔符，词统一转成小写
  [通过] 9. 样例文本按 min_length = 3 过滤后剩 13 个词次
  [通过] 10. min_length 改成 5 之后只剩 still 一个词，过滤确实生效
  [通过] 11. 总词数 13、不同词数 6
  [通过] 12. 平均词长等于 41 / 13（按词次加权）
  [通过] 13. 关掉 lowercase 后 The 与 the 分开算，不同词从 6 变成 8
  [通过] 14. 词频第一名是 the，出现 4 次
  [通过] 15. 词频前三名依次是 the、sat、cat（同为 2 次的按词典序）
  [通过] 16. 各档计数之和等于不同词数
  [通过] 17. 档数取 min(bins, 词长跨度)，这里是 3 档，覆盖 3 到 5
  [通过] 18. 工厂造出 4 个段落；render() 按登记进度补渲染，不重算已渲染的段落
  [通过] 19. 段落名按登记顺序为 概览、词频、词长分布、耗时
  [通过] 20. 正文里四段的出现顺序与登记顺序一致
  [通过] 21. 同一份输入跑两遍，概览、词频、词长分布逐字节相同，只有耗时段不同
  [通过] 22. 四个计时字段都非负，读文件那一段正是传进来的 0.25 毫秒
  [通过] 23. 读不到的文件返回 false、给出原因，并把文本清空
  [通过] 24. 空输入仍然出齐四段，ok 为假并给出说明

  自测结果：24 项中 24 项通过，全部通过
```

`--selftest` 只跑自测，各级输出与参数检查不参与：

`实测数据`
`Text`

```text
示例 07-standard-library/09-stdlib-capstone · 标准库综合流水线（命令行版）

== 自测 ==
  [通过] 1. 配置 analysis.top 读成整数 3
  [通过] 2. min_length 与 bins 都读到了
  [通过] 3. lowercase = true 读成布尔真
  [通过] 4. 只写键名 bar_width 也能取到 report 段里的值
  [通过] 5. 配置条目按 (段, 键, 值) 结构化绑定解包，共 5 条，行尾注释没有进值
  [通过] 6. 缺键返回 nullopt，类型不符也返回 nullopt
  [通过] 7. 越界的 top 与 bins 改回默认值，并各记一条问题
  [通过] 8. 标点与数字都是分隔符，词统一转成小写
  [通过] 9. 样例文本按 min_length = 3 过滤后剩 13 个词次
  [通过] 10. min_length 改成 5 之后只剩 still 一个词，过滤确实生效
  [通过] 11. 总词数 13、不同词数 6
  [通过] 12. 平均词长等于 41 / 13（按词次加权）
  [通过] 13. 关掉 lowercase 后 The 与 the 分开算，不同词从 6 变成 8
  [通过] 14. 词频第一名是 the，出现 4 次
  [通过] 15. 词频前三名依次是 the、sat、cat（同为 2 次的按词典序）
  [通过] 16. 各档计数之和等于不同词数
  [通过] 17. 档数取 min(bins, 词长跨度)，这里是 3 档，覆盖 3 到 5
  [通过] 18. 工厂造出 4 个段落；render() 按登记进度补渲染，不重算已渲染的段落
  [通过] 19. 段落名按登记顺序为 概览、词频、词长分布、耗时
  [通过] 20. 正文里四段的出现顺序与登记顺序一致
  [通过] 21. 同一份输入跑两遍，概览、词频、词长分布逐字节相同，只有耗时段不同
  [通过] 22. 四个计时字段都非负，读文件那一段正是传进来的 0.25 毫秒
  [通过] 23. 读不到的文件返回 false、给出原因，并把文本清空
  [通过] 24. 空输入仍然出齐四段，ok 为假并给出说明

  自测结果：24 项中 24 项通过，全部通过
```

报表里的耗时会随机器状态变化，其余各行逐字节稳定；把两次运行的输出保存下来，
删掉耗时段再比较，可以自己验证这一条。

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `capstone.cpp` 的 `PhaseTimer` | 构造即开始计时，`duration<double, std::milli>` 直接给出毫秒浮点值 |
| `capstone.cpp` 的 `parse_literal` | `from_chars` 要求读到末尾才算数，`1280x720` 因此落进字符串 |
| `capstone.cpp` 的 `ConfigReader::find` | 键名带点时按「段.键」精确匹配，不带点时全文件按键名找 |
| `capstone.cpp` 的 `tokenize` | 只把 ASCII 字母当词字符，`flush` 里同时做 `min_length` 过滤与小写规范化 |
| `capstone.cpp` 的 `compute_stats` | 词频榜的排序带第二关键字（词典序），这是输出稳定的原因 |
| `capstone.cpp` 的 `compute_stats` 分箱段 | 档数取 `min(bins, 词长跨度)`，档界用向上取整算出，各档不重不漏 |
| `capstone.cpp` 的四个 `make_*_section` | 工厂返回 `std::unique_ptr<Section>`，构造细节留在实现文件里 |
| `capstone.cpp` 的 `ReportBuilder::add_section` | 回调按值捕获裸指针，段落所有权同时交给 `owned_` |
| `capstone.cpp` 的 `ReportBuilder::render` | 已渲染的段落留在缓存里，后加入的段落接着渲染，重复调用不重算 |
| `capstone.cpp` 的 `run_text` | 计时段落最后登记，因此它的数字只统计它登记之前的动作 |
| `capstone.cpp` 的 `run` | 读文件、按行切开、计时三件事按顺序做，读不到文件时直接返回空报表 |
| `capstone.cpp` 的 `run_self_tests` | 24 项自测；含「同一份输入跑两遍，除耗时外逐字节相同」一条 |

## 前序示例与本示例的对照

| 前序示例 | 用到的能力 | 在本示例里的位置 |
|---|---|---|
| `01-c-stdlib-toolbox` | `<stdio.h>` 读文本、`<string.h>` 分词、`qsort` 排序、`<time.h>` 计时、报表输出 | 同样的一条流水线，C++ 写法：`read_text_file`、`tokenize`、`compute_stats` 里的排序、`PhaseTimer`、四个段落 |
| `02-cpp-io-report` | `iostream`、`iomanip`、`sstream`、`fstream` 出同一份报表 | `read_text_file` 与 `split_lines` 用 `<fstream>` 与 `<sstream>`；各段落用 `setw`、`setprecision` 排版 |
| `03-cpp-string-text` | `std::string` 切分、查找、替换，`string_view` 零拷贝 | `strip_comment`、`trim` 收 `string_view`；`ConfigReader::parse` 按 `=` 与注释符截取 |
| `04-cpp-smart-pointers` | `unique_ptr` 工厂、`shared_ptr`、`weak_ptr`、`std::function` 回调注册表 | 四个段落工厂返回 `unique_ptr`，`ReportBuilder` 的回调表按登记顺序渲染 |
| `05-cpp-numeric-random` | `<numeric>` 统计、直方图输出 | `compute_stats` 用 `accumulate` 加权求和，`HistogramSection` 按计数画条形 |
| `06-cpp-chrono-benchmark` | `<chrono>` 多次测量、分位数、`atomic` 计数 | `PhaseTimer` 给四段各测一次；本示例不做多次测量，耗时只作参考 |
| `07-cpp-filesystem-scan` | `<filesystem>` 遍历目录、`error_code` 与异常两条路径 | 本示例只用 `<fstream>` 读两个固定路径，不遍历目录；要扩展见阶段 5 |
| `08-cpp-config-parser` | `variant`/`optional`/`tuple` 表示配置值，`<type_traits>` 做约束 | `ConfigReader` 用同一套思路读 `data/analysis.cfg`，`get<T>()` 只给浮点留放宽的路 |

## 已知问题

- 分词只认 ASCII 字母：中文、数字、下划线都算分隔符，因此本示例的语料是纯英文。
  要按别的规则切词，改 `is_word_char` 一处即可。
- 语料整份读进内存再按行切开，文件很大时占用与文件同量级；
  要流式处理得改成边读边分词。
- 词长分箱按**不同词**统计（各档之和等于不同词数），与「词次」口径不同；
  要按词次统计，把 `compute_stats` 分箱段里的 `+= 1` 换成 `+= item.second`。
- 耗时段的数字每次运行都不同，报表因此不是完全逐字节稳定；
  自测只断言除耗时段外一致。
- 输入只支持一个文件，没有目录遍历与多文件合并，这两件事见 `07-cpp-filesystem-scan`。
- 配置值只认布尔、整数、小数、字符串四种，与 `08-cpp-config-parser` 相同；
  容器类型的配置项不在本示例范围内。
