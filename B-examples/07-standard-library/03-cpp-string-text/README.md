# 示例 `07-standard-library/03-cpp-string-text` · `std::string` 与 `string_view` 文本处理（纯命令行）

一个只做文本处理的命令行程序。它读一份 UTF-8 的示例文本，按分隔符切开、去掉两端的空白、
查找并替换、转换大小写，再用 `std::string_view` 把同一段文本零拷贝地切一遍，
最后按 UTF-8 的规则统计字节数与字符数，并在字符边界上做两种截断。

`std::string` 拥有内存，`std::string_view` 只是「指针 + 长度」，两者的分工是本项目的主线。
程序把这件事做成了看得见的输出：零拷贝切分出来的每一段落在原串的哪个偏移、
字符串重新分配之后先前的 view 指向了哪块内存，都在报告里。核心库另带 16 项自测。

```text
03-cpp-string-text/
  include/text_tools.hpp     文本处理的接口（不依赖界面）
  src/text_tools.cpp         实现：切分、修剪、查找替换、UTF-8、项目输出与自测
  src/main_cli.cpp           命令行版：读参数、打印、按控制台代码页转换
  data/sample_text.txt       示例文本（UTF-8，5 行，含中文与标点，第 1 行两端有空格）
  CMakeLists.txt             目标：core（静态库）与 app_cli
  CMakePresets.json          mingw-gdb 与 msvc 两套预设
  .vscode/                   两个调试配置与四个构建任务
```

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《07-标准库/B-02-std-string 与 string_view.md》第 1 节 | 从 `char[]` 到 `std::string`，它仍然是一串字节 | 全项目的出发点：`size()` 与字符数不是一回事 |
| 《07-标准库/B-02-std-string 与 string_view.md》第 3.2 小节 | 查找函数一览 | `find_all` 与 `replace_all`（`src/text_tools.cpp`） |
| 《07-标准库/B-02-std-string 与 string_view.md》第 4.2、4.4 小节 | 悬垂的指针、安全的写法 | 自测第 16 项：view 有效与失效的分界 |
| 《07-标准库/B-02-std-string 与 string_view.md》第 6.2、6.3 小节 | 它是「指针 + 长度」、零拷贝的代价 | `split_view` 与报告里的「零拷贝切分」一段 |
| 《07-标准库/B-02-std-string 与 string_view.md》第 8.1、8.2、8.3 小节 | UTF-8 是字节序列、`size()` 是字节数、截断会把字符切坏 | `utf8_char_count`、`utf8_truncate`、`utf8_truncate_bytes` 与自测第 11 至 15 项 |

## 这个项目要解决什么问题

文本处理里最常写的四件事是切分、修剪、查找替换、大小写转换，其中三件在标准库里各有一组函数，
但用法并不统一：`find` 要给起点才能找下一个，`replace` 一次只换一处，
`substr` 每次都复制一份。项目把这几件事各包成一个函数，语义在接口上写清：
`find_all` 返回全部位置，`replace_all` 返回替换次数，`from` 为空时直接拒绝而不是死循环。

另一半问题是「用 `std::string` 还是 `std::string_view`」。前者拥有一块内存，
后者只是一个指针加一个长度。项目对同一段文本提供了两套切分：
`split` 返回 `vector<std::string>`，每一段都是新的一份；
`split_view` 返回 `vector<string_view>`，每一段都指向原串的缓冲区，一个字节都不复制。
报告里会打印每一段相对于原串的偏移，这就是零拷贝的证据，也是它的代价所在：
被指向的那个 `std::string` 一旦重新分配，先前的 view 就指向了旧缓冲区。

第三件事是编码。UTF-8 的一个汉字占三个字节，于是 `size()` 返回的是字节数，
按字节截断会把一个多字节序列切成两半，产生非法字节。项目按首字节判定位数、
按字符数截断、按字节数截断并退到字符边界，三条路径都有自测。

## 做完能掌握什么

- 会用 `string_view` 做零拷贝切分，并能说清它在什么条件下失效
- 会自己写切分与替换，而不是每次都在循环里调 `substr` 拼字符串
- 会按 UTF-8 的首字节判定位数，并在字符边界上安全截断
- 会把「字节数」与「字符数」分开对待，不再用 `size()` 当作字数
- 会把核心逻辑与界面分开：`text` 命名空间不认识 `std::cout`，也不认识控制台编码

## 文件

```text
include/text_tools.hpp     接口：切分、修剪、大小写、查找替换、UTF-8、文件读取、自测
src/text_tools.cpp         实现，565 行（其中非空非注释 464 行），含项目输出与 16 项自测
src/main_cli.cpp           命令行版，143 行：解析参数、打印、UTF-8 到控制台代码页的转换
data/sample_text.txt       示例文本，UTF-8 无 BOM、LF、5 行（第 1 行两端各有 2 个空格）
CMakeLists.txt             目标 core（静态库）与 app_cli；字符集选项全板块统一
CMakePresets.json          mingw-gdb（Ninja + g++）与 msvc（Visual Studio 17 2022）两套预设
.vscode/launch.json        两个调试配置：GDB · 命令行版、MSVC · 命令行版
.vscode/tasks.json         四个构建任务与两个组合任务
```

`core` 是纯逻辑的静态库，删掉 `src/main_cli.cpp` 它照样能编译、能自测通过。
命令行版只做三件事：读参数、把 core 给的文本摆出来、把 UTF-8 换成本地编码。

## 阶段推进

| 阶段 | 目标 | 做什么 | 怎么验收 |
|---|---|---|---|
| **阶段 1** | 跑通命令行版 | 配置、编译，运行 `build\mingw\bin\app_cli.exe` | 看到「项目输出」与「16 项中 16 项通过，全部通过」 |
| **阶段 2** | 看懂零拷贝 | 读 `split_view` 的实现，对照报告里每段的偏移 | 能说清 `views[i].data()` 落在 `sample.data()` 的哪一段之内 |
| **阶段 3** | 自己加一个函数 | 加 `std::size_t count_words(std::string_view)`（按空白切分并计数），并补一条自测 | 新自测项通过；报告里能看到它的结果 |
| **阶段 4** | 换一份数据 | 把自己的 `.txt` 交给 `app_cli <文件路径>`，观察各行统计与截断结果 | 换成含日文或 emoji 的文本时，`结构合法` 仍为「是」 |

阶段 3 是重点：新加的解析函数必须对多字节分隔符也成立，写完后自测会告诉你答案。

## 构建与运行

`PowerShell`

```powershell
# 在 07-standard-library/03-cpp-string-text 目录下：配置 + 编译
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 运行：默认读 data/sample_text.txt（工作目录必须是示例目录）
build\mingw\bin\app_cli.exe

# 换一个文件
build\mingw\bin\app_cli.exe <文件路径>

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
调试路线的选择、`.vscode` 的用法与产物位置见 [`../../README.md`](../../README.md)。

## 运行后应当看到什么

`build\mingw\bin\app_cli.exe`（不带参数，工作目录为示例目录）：

`实测数据`
`Text`

```text
示例 07-standard-library/03-cpp-string-text · std::string 与 string_view 文本处理（命令行版）

== 项目输出 ==
源文件：data/sample_text.txt（字节 533，字符 233，行 5）
  按字节算比按字符算大：UTF-8 里一个汉字占 3 个字节

各行统计（字节 / 字符）
  第 1 行：108 / 58
  第 2 行：104 / 52
  第 3 行：105 / 35
  第 4 行：102 / 42
  第 5 行：109 / 41

修剪（第 1 行两端各留了两个空格）
  修剪前 108 字节，修剪后 104 字节，少了 4 个空白字节
  修剪后：标准库把字符串分成两层：std::string 拥有内存，std::string_view 只是指针与长度。

大小写（只动 ASCII 字母，汉字与符号原样保留）
  转大写：标准库把字符串分成两层：STD::STRING 拥有内存，STD::STRING_VIEW 只是指针与长度。

切分：按「，」切第 1 行（修剪后），得到 2 段
  [0] 标准库把字符串分成两层：std::string 拥有内存（60 字节）
  [1] std::string_view 只是指针与长度。（41 字节）

零拷贝切分：同一行换成 string_view，段数 2
  第 1 段起点在原文的偏移 0，长度 60 字节
  第 2 段起点在原文的偏移 63，长度 41 字节
  这些 view 一个字节都没有复制：偏移 0 到 104 之间就是原来那块内存

查找与替换
  「std::string」出现 2 次，字节位置 36 63
  换成 string：2 处，长度 104 → 94 字节

UTF-8 首字节判定位数
  U+0041 'A' 首字节 0x41，共 1 字节
  U+00E9 首字节 0xC3，共 2 字节
  U+4E2D 首字节 0xE4，共 3 字节
  U+1F642 首字节 0xF0，共 4 字节

按字符与按字节截断（都在第 1 行修剪后的文本上做）
  按字符截断到 20 个字符：104 → 44 字节，20 个字符，结构合法 是
    标准库把字符串分成两层：std::str
  按字节截断到 40 字节：实际 40 字节，结构合法 是
    标准库把字符串分成两层：std:
  直接取前 40 个字节（不推荐）：结构合法 否，末尾的多字节序列被切成两半

string_view 不拥有内存
  指向的 string 还活着时：取它的前几个字符得到 标准库把字符串分成两
  给那个 string 追加 100 个字节后：缓冲区地址变了，先前的 view 还指着旧缓冲区，再解引用就是悬垂访问

== 自测 ==
  [通过] 1. split 把 a,b,c 切成 3 段
  [通过] 2. 连续分隔符与末尾分隔符都产生空段
  [通过] 3. 分隔符本身是多字节的 UTF-8 文本
  [通过] 4. split_view 的每一段都指向原串自己的缓冲区，没有复制
  [通过] 5. trim 去掉两端空白，中间的空格不动
  [通过] 6. 大小写转换不碰多字节序列
  [通过] 7. replace_all 替换全部并返回次数
  [通过] 8. from 为空时返回 0，文本不变
  [通过] 9. find_all 给出全部出现位置
  [通过] 10. join 用分隔符拼回
  [通过] 11. size() 是字节数，utf8_char_count 才是字符数
  [通过] 12. 首字节 0xxxxxxx 到 11110xxx 分别判为 1 到 4 字节
  [通过] 13. 续字节与 0xF8 以上不当作首字节
  [通过] 14. utf8_truncate 按字符数截断，汉字完整
  [通过] 15. utf8_truncate_bytes 退到字符边界上
  [通过] 16. view 在主人活着且未重新分配时有效，重新分配后指向旧缓冲区

  自测结果：16 项中 16 项通过，全部通过
```

`build\mingw\bin\app_cli.exe --selftest`（只跑自测，没有项目输出）：

`实测数据`
`Text`

```text
示例 07-standard-library/03-cpp-string-text · std::string 与 string_view 文本处理（命令行版）

== 自测 ==
  [通过] 1. split 把 a,b,c 切成 3 段
  [通过] 2. 连续分隔符与末尾分隔符都产生空段
  [通过] 3. 分隔符本身是多字节的 UTF-8 文本
  [通过] 4. split_view 的每一段都指向原串自己的缓冲区，没有复制
  [通过] 5. trim 去掉两端空白，中间的空格不动
  [通过] 6. 大小写转换不碰多字节序列
  [通过] 7. replace_all 替换全部并返回次数
  [通过] 8. from 为空时返回 0，文本不变
  [通过] 9. find_all 给出全部出现位置
  [通过] 10. join 用分隔符拼回
  [通过] 11. size() 是字节数，utf8_char_count 才是字符数
  [通过] 12. 首字节 0xxxxxxx 到 11110xxx 分别判为 1 到 4 字节
  [通过] 13. 续字节与 0xF8 以上不当作首字节
  [通过] 14. utf8_truncate 按字符数截断，汉字完整
  [通过] 15. utf8_truncate_bytes 退到字符边界上
  [通过] 16. view 在主人活着且未重新分配时有效，重新分配后指向旧缓冲区

  自测结果：16 项中 16 项通过，全部通过
```

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `text_tools.hpp` 的 `split_view` 声明 | 注释里写明返回值指向 `text` 自己的缓冲区，这是调用的前提 |
| `text_tools.cpp` 的 `split_view` | 只用 `find` 与 `substr`，`substr` 在 `string_view` 上是「再取一段视图」，不复制字节 |
| `text_tools.cpp` 的 `replace_all` | `pos += to.size()` 跳过刚换上去的内容，换入串里再含原串也不会死循环；`from` 为空直接返回 0 |
| `text_tools.cpp` 的 `utf8_sequence_length` | 用首字节的高位模式判位数，续字节与 `0xF8` 以上一律返回 0 |
| `text_tools.cpp` 的 `utf8_char_count` | 数非续字节的字节数，一行循环解决，与「字节数」明确区分 |
| `text_tools.cpp` 的 `utf8_truncate_bytes` | 从 `max_bytes` 往回退到某个首字节为止，宁可少一个字符也不切坏序列 |
| `text_tools.cpp` 的 `Checker` | 自测项在库里攒成文本行，谁来显示、显示到哪由界面层决定 |
| `text_tools.cpp` 的 `read_text_file` | 按二进制读入，不做换行转换；空文件也不会误判成失败 |
| `main_cli.cpp` 的 `to_console_encoding` | 全项目唯一的编码转换点：控制台是 65001 就原样输出，是 936 就转 GBK |
| `main_cli.cpp` 的 `print_line` | 所有输出都走这一个函数，编码问题只可能出现在一处 |
| `CMakeLists.txt` 的字符集段 | 核心库内部一律用 `u8""` 字面量，因此 `-fexec-charset=GBK` 只影响非 u8 字面量 |

## 已知问题

- 只处理 UTF-8 输入。GBK 文本要先转成 UTF-8，否则 `utf8_is_valid` 会给出「否」
- `utf8_is_valid` 只做结构检查：首字节给出的长度与后续字节的 `10xxxxxx` 形式对得上就算合法，
  不检查超长编码（overlong）与代理区
- `utf8_char_count` 数的是码点个数，不是显示宽度。组合字符（如 `e` 加音调符号）与
  带修饰符的 emoji 会被算成多个字符
- `to_upper` 与 `to_lower` 只作用于 ASCII 字母。`std::toupper` 一次只看一个字节，
  对多字节序列没有意义
- 控制台代码页是 936 时，转换函数对 GBK 表示不了的字符（例如 emoji）会写成 `?`。
  为此报告里只打印码点编号 `U+1F642`，不打印字符本身
- 默认数据文件按相对路径 `data/sample_text.txt` 打开，运行目录必须是示例目录；
  在别处运行时用 `app_cli <文件路径>` 指定
- 示例文本以 UTF-8 无 BOM、LF 保存，第 1 行两端各留了 2 个空格。用编辑器改写时留意这两处空白，
  被自动去掉之后报告里的「修剪前 108 字节，修剪后 104 字节」会跟着变
