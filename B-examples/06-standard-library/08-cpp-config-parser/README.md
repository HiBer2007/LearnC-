# 示例 `06-standard-library/08-cpp-config-parser` · INI 配置解析器（命令行）

配置文件是每个工具都会遇到的第一件小事。格式看着简单，动手写起来要回答一串问题：
一个值可能是布尔、整数、小数或文本，「一个值」在代码里用什么类型装；
键可能写了也可能没写，「取不到」怎么表达，才不至于与「值就是 0」混起来；
某一行少写一个等号，程序应当指出第几行，而不是抛一个异常把整份配置作废。

本示例把这些问题收进一个只有两个文件的核心库：自己写一个 INI 风格解析器，
配置值用 `std::variant` 装，取不到用 `std::optional` 表示，
取值类型的约束交给 `<type_traits>`，遍历与解包用 `std::tuple` 加结构化绑定。
程序带 20 项自测，逐项核对类型判定、三种「取不到」与行号报错。

界面形态只有命令行：本示例的交互是「读一份配置、把结果打出来」，
控制台已经说得清楚，多余的窗口不会增加信息量。

`Text`

```text
08-cpp-config-parser/
  include/config.hpp     解析器的接口：Value、Entry、ParseError、Config、get<T>()
  src/config.cpp         解析、类型判定、取值约束，以及 20 项自测
  src/main_cli.cpp       命令行版：只负责排版与打印
  data/app.ini           示例配置：四种类型的值、注释、空行，加一处故意写错的行
  CMakeLists.txt         目标：core（静态库）、app_cli
  CMakePresets.json      mingw-gdb 与 msvc 两套预设
  .vscode/               调试与任务配置（两个调试配置）
```

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《06-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 4 节 | `variant` 表示「多选一」 | `cfg::Value` 是四种类型的选择，类型跟着值走 |
| 《06-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 3 节 | `optional` 把「可能没有值」写进类型 | `get<T>()` 返回 `std::optional<T>`，缺键就是 `nullopt` |
| 《06-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 2 节 | `tuple` 与结构化绑定 | `flatten()` 摊出四元组，打印与自测都用结构化绑定解包 |
| 《06-标准库/B-09-工具类（下）：type_traits 与 concepts.md》第 1 节 | `<type_traits>` 把类型性质写成编译期常量 | `is_same_v`、`is_integral_v`、`is_floating_point_v` 决定 `get<T>()` 能不能取 |
| 《06-标准库/B-09-工具类（下）：type_traits 与 concepts.md》第 2 节 | 类型分类 | `is_integral_v` 判定一个值算不算整数，`bool` 被单独排除 |
| 《06-标准库/B-01-输入输出：iostream.md》第 6 节 | `<sstream>` 把字符串当成流 | 逐行解析用 `std::istringstream`，取不出数不算失败 |
| 《06-标准库/B-01-输入输出：iostream.md》第 7 节 | `<fstream>` 文件 | `load_file()` 一次读完整份配置再解析 |
| 《06-标准库/B-02-std-string 与 string_view.md》第 3 节 | `std::string` 的查找与截取 | `find('=')`、`substr` 切出键与值 |
| 《06-标准库/B-02-std-string 与 string_view.md》第 6 节 | `std::string_view` | `trim` 与 `strip_comment` 收 `string_view`，切分过程不复制 |
| 《06-标准库/B-11-收尾：把标准库用对.md》第 1 节 | 头文件命名规则 | 用 `<cstdlib>` 与 `std::strtoll`，不混用 C 头文件里的全局名字 |

## 这个项目要解决什么问题

- **值的类型是值的一部分。** `variant` 让「这个值到底是布尔还是文本」有一个确定的答案，
  `std::get_if<T>` 问一次就知道，不必再存一个 `enum` 标记，也不会出现标记与内容对不上的情况。
- **「没有」与「是 0」是两件事。** `timeout` 没写与 `timeout = 0` 的含义完全不同，
  前者是 `nullopt`，后者是有值的 `0`。用 `optional` 把这条区别写进类型，调用方想忽略都难。
- **错误要能定位。** 解析器不抛异常，出错的行走一张问题清单：行号、说明、该行原文，
  同一份文件里其余合法的行照常收下。
- **约束放在编译期。** 配置值只有四种类型，`get<T>()` 里一句 `static_assert` 就挡住了
  第五种；整数可以放宽成浮点，浮点取整数不成立，这条规则由 `if constexpr` 在编译期选定分支。

## 做完能掌握什么

- 会用 `std::variant` 表示「多选一」，并用 `std::visit` 加 `if constexpr` 做分派
- 会用 `std::optional` 区分「没有值」与「有值但是零」，并知道 `value_or` 类接口的用法
- 会用 `<type_traits>` 的类型特征给模板接口加约束，把错误从运行期提到编译期
- 会用 `std::tuple` 与结构化绑定把多条记录摊平后逐条处理
- 会写一个带行号的解析器：错误收集、错误恢复、重复键覆盖都落在明确的规则上

## 文件

`Text`

```text
include/config.hpp     接口：Value、Entry、ParseError、Config::get<T> / get_or、自测入口
src/config.cpp         实现：字面量识别、逐行解析、行号报错、20 项自测
src/main_cli.cpp       命令行版：读参数、排版打印、返回码
data/app.ini           示例配置：四类值、两种注释、空行，第 27 行故意少写一个等号
CMakeLists.txt         目标：core（静态库）、app_cli
CMakePresets.json      mingw-gdb 与 msvc 两套预设
.gitignore             忽略 build/ 与 MSVC 中间文件
.vscode/               launch.json、tasks.json、settings.json
```

`core` 是纯逻辑静态库，`app_cli` 只链接它。把 `src/main_cli.cpp` 删掉，
`core` 照样能编译、自测照样能跑，这是「界面与逻辑分开」的判据。

## 阶段推进

| 阶段 | 目标 | 做什么 | 怎么验收 |
|---|---|---|---|
| **阶段 1** | 跑通解析与打印 | 配置、编译，运行 `app_cli.exe` | 终端里出现 4 个段、12 条配置，以及「第 27 行」那条报错；自测 20 项全过 |
| **阶段 2** | 看清类型判定 | 把 `data/app.ini` 里的 `ratio = 0.75` 改成 `ratio = 1`，重新运行 | `ratio` 那一行的类型从 `double` 变成 `long long`，「放宽是单向的」那一行的结果不变 |
| **阶段 3** | 自己写一份配置 | 新写一份 `data/my.ini`，故意漏掉一个键、把某个整数键写成小数，用 `app_cli.exe data/my.ini` 运行 | 打印出的段与条目数对得上文件；漏掉的键在「取值示范」里显示「没有」，类型不符的那一行也是「没有」 |
| **阶段 4**（选做） | 给解析器加一项能力 | 例如支持 `键: 值` 的冒号写法，或给值加上转义序列 | 为新规则补自测项，`--selftest` 项数增加且全部通过；原有 20 项一条不动 |

阶段 2 是重点：值还是那个值，类型变了，取法就要跟着变，这正是 `variant` 想让人看见的事。

## 构建与运行

`PowerShell`

```powershell
# 在 06-standard-library/08-cpp-config-parser 目录下：配置 + 编译
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 运行（程序按 data/app.ini 这个相对路径找配置，因此要在示例目录下运行）
build\mingw\bin\app_cli.exe

# 读另一份配置
build\mingw\bin\app_cli.exe data\my.ini

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
[2/4] Building CXX object CMakeFiles/core.dir/src/config.cpp.obj
[3/4] Linking CXX static library libcore.a
[4/4] Linking CXX executable bin\app_cli.exe
```

## 运行后应当看到什么

`build\mingw\bin\app_cli.exe` 的完整输出：

`实测数据`
`Text`

```text
示例 06-standard-library/08-cpp-config-parser · INI 配置解析器（命令行版）

== 项目输出 ==
配置文件：data/app.ini
段 4 个，条目 12 条

[app]
  name           = text-analyzer     （string）
  version        = 1.40              （double）
  debug          = false             （bool）

[run]
  top            = 8                 （long long）
  min_length     = 3                 （long long）
  lowercase      = true              （bool）
  ratio          = 0.75              （double）
  timeout        = 30                （long long）
  window         = 1280x720          （string）
  motto          = stay # simple     （string）

[paths]
  input          = data/corpus.txt   （string）
  output         = data/report.txt   （string）

[broken]
  这一段没有条目

取值示范（段, 键, 目标类型 → 结果）：
  get<long long>("run", "top")                 → 8
  get<double>("run", "top")                    → 8.00    整数放宽成浮点
  get<std::string>("run", "top")               → 没有    类型不匹配
  get<long long>("app", "ratio")               → 没有    放宽是单向的
  get<std::string>("app", "missing")           → 没有    键不存在
  get_or<std::string>("app", "author", 默认值) → (未填写)    缺键时给默认值

解析时记下 1 处问题：
  第 27 行：既不是 [段]，也没有 = 号
      原文：missing_equals

== 自测 ==
  [通过] 1. 解析出 app、run 两个段，顺序与文件一致
  [通过] 2. flatten() 摊出 9 条 (段, 键, 值, 类型) 四元组，结构化绑定取得出首尾两条
  [通过] 3. retries = 3 认成整数
  [通过] 4. ratio = 0.75 认成浮点
  [通过] 5. debug = false 认成布尔
  [通过] 6. window = 1024x768 整体读不成数，落进字符串
  [通过] 7. 引号里的 # 被保留，引号本身去掉
  [通过] 8. timeout 后面的 `; 秒` 是注释，没有进入值
  [通过] 9. empty = 取到有值的空字符串，与缺键不是一回事
  [通过] 10. 缺键返回 nullopt
  [通过] 11. 缺段返回 nullopt
  [通过] 12. 整数键按字符串取返回 nullopt（类型不匹配）
  [通过] 13. 重复键以最后一次为准，且整数能放宽成浮点
  [通过] 14. 浮点键按整数取返回 nullopt（放宽是单向的）
  [通过] 15. get_or 在缺键时给出默认值
  [通过] 16. is_integral_v 判定：整数算数、bool 不算；type_name 与值一致
  [通过] 17. 四处笔误各记一行，行号为 1、4、5、6
  [通过] 18. 出错行前后的合法行照常收下
  [通过] 19. CRLF 行尾解析结果相同，行尾的 \r 被去掉
  [通过] 20. 空文本解析出 0 条、0 错

  自测结果：20 项中 20 项通过，全部通过
```

`--selftest` 只跑自测，没有「项目输出」一段：

`实测数据`
`Text`

```text
示例 06-standard-library/08-cpp-config-parser · INI 配置解析器（命令行版）

== 自测 ==
  [通过] 1. 解析出 app、run 两个段，顺序与文件一致
  [通过] 2. flatten() 摊出 9 条 (段, 键, 值, 类型) 四元组，结构化绑定取得出首尾两条
  [通过] 3. retries = 3 认成整数
  [通过] 4. ratio = 0.75 认成浮点
  [通过] 5. debug = false 认成布尔
  [通过] 6. window = 1024x768 整体读不成数，落进字符串
  [通过] 7. 引号里的 # 被保留，引号本身去掉
  [通过] 8. timeout 后面的 `; 秒` 是注释，没有进入值
  [通过] 9. empty = 取到有值的空字符串，与缺键不是一回事
  [通过] 10. 缺键返回 nullopt
  [通过] 11. 缺段返回 nullopt
  [通过] 12. 整数键按字符串取返回 nullopt（类型不匹配）
  [通过] 13. 重复键以最后一次为准，且整数能放宽成浮点
  [通过] 14. 浮点键按整数取返回 nullopt（放宽是单向的）
  [通过] 15. get_or 在缺键时给出默认值
  [通过] 16. is_integral_v 判定：整数算数、bool 不算；type_name 与值一致
  [通过] 17. 四处笔误各记一行，行号为 1、4、5、6
  [通过] 18. 出错行前后的合法行照常收下
  [通过] 19. CRLF 行尾解析结果相同，行尾的 \r 被去掉
  [通过] 20. 空文本解析出 0 条、0 错

  自测结果：20 项中 20 项通过，全部通过
```

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `config.hpp` 的 `using Value = std::variant<...>` | 四种类型按顺序列出，`variant` 自己保证同一时刻只有一种有效 |
| `config.hpp` 的 `get<T>()` | `static_assert` 挡住第五种类型，`std::get_if<T>` 精确匹配，`if constexpr` 里只给浮点开一条从整数放宽的路 |
| `config.hpp` 的 `detail::is_config_value_v` | 用 `is_same_v` 与 `remove_cv_t` 拼出的编译期常量，是 `static_assert` 的依据 |
| `config.cpp` 的 `parse_literal` | `strtoll` 与 `strtod` 都要求读到末尾才算数，`1024x768` 因此落进字符串而不是被截成 `1024` |
| `config.cpp` 的 `strip_comment` | 注释符前必须有空白、且不在双引号里，值里的 `#` 不会被误伤 |
| `config.cpp` 的 `parse` | 出错只记一行错误并继续，`errors_` 里的行号就是文本里的行号 |
| `config.cpp` 的 `put` | 同一个键写两次时原地覆盖，条目表里不会出现重复键 |
| `config.cpp` 的 `is_integer_value` | 用 `is_integral_v` 加 `is_same_v` 排除 `bool`，判定与类型表一致 |
| `config.cpp` 的 `run_self_tests` | 20 项自测；每项都有说明与失败时的期望值、实际值 |
| `main_cli.cpp` 的 `print_section` | 用结构化绑定逐条解包四元组，界面里没有一行解析逻辑 |

## 已知问题

- **配置文件里的非 ASCII 字节按原样保留。** 解析器不转换编码，
  而本示例的窄字符串按 GBK 写入 exe，因此数据文件里的值用英文写，
  避免控制台代码页把 UTF-8 字节显示成乱码；要支持别的编码，得在读取时显式转码。
- 值不支持多行写法与转义序列，双引号只负责「引号里的注释符不算注释」这一件事。
- 同一个键写两次时后写的覆盖先写的，覆盖这件事不记进问题清单。
- 键出现在任何 `[段]` 之前会被判为错误，本示例不支持「无段」的全局键。
- 行尾注释的注释符前面必须有空白，`a#b` 这种写法里 `#` 算值的一部分。
- `get<T>()` 传入第四种以外的类型是编译错误，不是运行期错误，这一点与「取不到」不是一类问题。
