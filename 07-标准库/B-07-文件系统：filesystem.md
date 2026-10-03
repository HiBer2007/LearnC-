# 文件系统：`<filesystem>`

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**C 的标准库里没有「目录」这个概念。** `<stdio.h>` 能打开、读、写、删除文件，
但列目录要 `dirent.h` 或 `FindFirstFile`——**那是平台 API，不是标准的一部分**。
于是同一件「扫描一个目录」的事，在 Windows 与 Linux 上要写两套代码。

**`<filesystem>`（C++17）把这一块补上了**，而且不只是一个「列目录」的函数：
它给出了 `path` 这个类型来装路径，给出了「路径的哪一段是什么」的现成答案，
给出了遍历、查询属性、创建删除改名复制的全套操作，
还给出了**两条错误处理路线**——抛异常的与收 `error_code` 的。

C 那一半在《07-标准库/A-01-输入输出：stdio.md》——`<stdio.h>` 能打开、读、写一个文件，
却没有「目录」这一层。**`<filesystem>` 补的正是这一层**，代价是同一件事多出一套写法：
C 的 `remove` 成功返回 `0`，`fs::remove` 成功返回真，**两者的返回值语义正好相反**，
混起来用是这一块最常见的一类错误。
本章节的数字都在本机实际运行验证过：**g++ 15.2.0（MinGW-w64），`-std=c++17`，Windows 11**；
**本仓库的路径本身就含中文**，正好用来实测编码那一节，
没有验证过的推断写成 `待确认`。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 复现环境与命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 窄字符与宽字符、UTF-8 与 UTF-16 的区别 | 《04-语法/02-数据类型与类型系统.md》第 2.2 小节 |
| `std::string` 与 `std::wstring` | 《04-语法/10-字符串.md》第 5 节 |
| 异常与错误码的分工 | 《04-语法/13-异常.md》第 5 节 |
| RAII：把「还回去」的动作写进析构函数 | 《05-类与面向对象/06-RAII 与资源管理.md》第 2 节 |
| `FILE` 与流、`fopen` 的基本用法 | 《07-标准库/A-01-输入输出：stdio.md》第 1 节 |
| `errno` 与 `strerror` 的错误报告方式 | 《07-标准库/A-05-工具与其它：stdlib 与杂项.md》第 7 节 |

| 本章节讲什么 | 在哪一节 |
|---|---|
| `path`：拆解、拼接、分隔符差异 | 第 1 节 |
| 编码现实：宽字符、`u8path`、中文路径 | 第 2 节 |
| 遍历目录与查询属性 | 第 3 节 |
| 创建、删除、改名、复制 | 第 4 节 |
| 抛异常与 `error_code` 两条路线 | 第 5 节 |
| 与 `stdio` 的逐件事对照 | 第 6 节 |
| 速查表与配套件 | 第 7 节 |

**相邻的章节**：C 侧的输入输出与文件读写见《07-标准库/A-01-输入输出：stdio.md》，
本章节第 6 节把同一件事的两种写法摆在一起；
`path` 里装的是字符串，字符串本身的用法见《07-标准库/B-02-std-string 与 string_view.md》。

---

# 第 1 节 `path`：路径是一个类型

## 1.1 路径的构成

`path` 把一个路径拆成若干段，**每一段都有名字**。这一套名字来自 POSIX 的
路径语法，Windows 的盘符被当作「根名字」装了进去：

`Text`

```text
   C:\work\case\report.tar.gz
   │  │    │    │      │      │
   │  │    │    │      │      └── extension()   .gz
   │  │    │    │      └───────── stem()        report.tar
   │  │    │    └──────────────── filename()    report.tar.gz
   │  │    └───────────────────── parent_path() C:\work\case
   │  └────────────────────────── 目录部分
   └───────────────────────────── root_name()   C:
```

`C++`

```cpp
/* path_parts.cpp    编译：g++ -std=c++17 path_parts.cpp -o path_parts */
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

void show(const char *tag, const fs::path &p) {
    std::printf("%-16s 原文        ：[%s]\n", tag, p.string().c_str());
    std::printf("%-16s root_name   ：[%s]  root_directory：[%s]  is_absolute：%d\n", "",
                p.root_name().string().c_str(), p.root_directory().string().c_str(), (int)p.is_absolute());
    std::printf("%-16s relative    ：[%s]\n", "", p.relative_path().string().c_str());
    std::printf("%-16s parent      ：[%s]  filename：[%s]\n", "",
                p.parent_path().string().c_str(), p.filename().string().c_str());
    std::printf("%-16s stem        ：[%s]  extension：[%s]\n\n", "",
                p.stem().string().c_str(), p.extension().string().c_str());
}

int main() {
    show(u8"Windows 风格", "C:\\work\\case\\report.tar.gz");
    show(u8"反斜杠与正斜杠混用", "C:/work\\case/report.txt");
    show(u8"POSIX 风格", "/home/me/case/report.tar.gz");
    show(u8"相对路径", "sub/dir/file.txt");
    show(u8"只有文件名", "file.txt");
    show(u8"点开头", ".gitignore");
    show(u8"末尾带分隔符", "C:/work/case/");
    show(u8"空路径", "");

    // 拼接：/ 补分隔符，+= 直接接字符串
    fs::path dir = "out";
    fs::path plus = dir;
    plus += "a.txt";
    std::printf("dir                       = [%s]\n", dir.string().c_str());
    std::printf("dir / \"a.txt\"             = [%s]\n", (dir / "a.txt").string().c_str());
    std::printf("dir += \"a.txt\"            = [%s]   <- 直接接字符串，没有分隔符\n", plus.string().c_str());
    std::printf("path(\"out/\") / \"a.txt\"    = [%s]\n", (fs::path("out/") / "a.txt").string().c_str());

    // 比较是逐字符的，Windows 上也是
    std::printf("\npath(\"A\") == path(\"a\")     ：%d\n", (int)(fs::path("A") == fs::path("a")));
    std::printf("lexically_normal(\"a/./b/../c\") = [%s]\n",
                fs::path("a/./b/../c").lexically_normal().string().c_str());

    // 改扩展名、去掉文件名
    fs::path f = "archive.tar.gz";
    std::printf("\nreplace_extension(\".zip\") = [%s]\n", f.replace_extension(".zip").string().c_str());
    fs::path g = "/tmp/dir/file";
    g.remove_filename();
    std::printf("remove_filename()          = [%s]\n", g.string().c_str());

    // 逐段遍历
    std::printf("\n逐段拆开 C:\\work\\case\\report.tar.gz：");
    for (const auto &part : fs::path("C:\\work\\case\\report.tar.gz"))
        std::printf(" [%s]", part.string().c_str());
    std::printf("\n");
    return 0;
}
```

`实测数据`
`Text`

```text
Windows 风格     原文        ：[C:\work\case\report.tar.gz]
                 root_name   ：[C:]  root_directory：[\]  is_absolute：1
                 relative    ：[work\case\report.tar.gz]
                 parent      ：[C:\work\case]  filename：[report.tar.gz]
                 stem        ：[report.tar]  extension：[.gz]

反斜杠与正斜杠混用 原文        ：[C:/work\case/report.txt]
                 root_name   ：[C:]  root_directory：[/]  is_absolute：1
                 (中间各行略)

POSIX 风格       原文        ：[/home/me/case/report.tar.gz]
                 root_name   ：[]  root_directory：[/]  is_absolute：0
                 (中间各行略)

只有文件名       原文        ：[file.txt]
                 parent      ：[]  filename：[file.txt]
                 stem        ：[file]  extension：[.txt]

点开头           原文        ：[.gitignore]
                 stem        ：[.gitignore]  extension：[]

末尾带分隔符      原文        ：[C:/work/case/]
                 parent      ：[C:/work/case]  filename：[]

dir                       = [out]
dir / "a.txt"             = [out\a.txt]
dir += "a.txt"            = [outa.txt]   <- 直接接字符串，没有分隔符
path("out/") / "a.txt"    = [out/a.txt]

path("A") == path("a")     ：0
lexically_normal("a/./b/../c") = [a\c]

replace_extension(".zip") = [archive.tar.zip]
remove_filename()          = [/tmp/dir/]

逐段拆开 C:\work\case\report.tar.gz： [C:] [\] [work] [case] [report.tar.gz]
```

**这张表里有六处值得逐个说明。**

**第一，拆解是纯文本操作，不去碰磁盘。** `stem()`、`extension()` 只看字符串，
因此 `report.tar.gz` 的扩展名是 `.gz`、主干是 `report.tar`——
**它不知道「.tar.gz」是一个整体**。要双层扩展名须自行处理。

**第二，`.gitignore` 的 `extension()` 是空串。** 前导点的含义是「隐藏文件」，
不是扩展名，因此整个 `.gitignore` 都是主干。

**第三，末尾带分隔符时 `filename()` 是空串。**
`C:/work/case/` 的 `parent_path()` 反而是 `C:/work/case`，
这一处的行为容易看错：**路径末尾的分隔符会被 `parent_path` 吃掉**。

**第四，POSIX 风格的 `/home/me/...` 在 Windows 上 `is_absolute()` 是 0。**
Windows 要求「根名字」才算绝对路径，而 `/home` 没有盘符。
**同一段代码在 Linux 上是绝对路径、在 Windows 上是相对路径**，
这是跨平台代码里最常见的路径错误来源。

**第五，`/` 会补分隔符，`+=` 不会。** `dir / "a.txt"` 得到 `out\a.txt`
（补的是平台的首选分隔符），`dir += "a.txt"` 得到 `outa.txt`——
**没有分隔符，拼出来的是一个不存在的新名字**。要拼路径就用 `/`。

**第六，路径比较是逐字符的，Windows 上也不例外。**
`path("A") == path("a")` 是假，尽管 Windows 的文件系统不区分大小写。
**要按文件系统语义比较，用 `fs::equivalent(a, b)`**（它要访问磁盘）。

## 1.2 分隔符：Windows 与 POSIX 的差异

`path` 在 Windows 上**同时接受 `\` 与 `/` 作为分隔符**，
在上面的表里 `C:/work\case/report.txt` 被正确拆开了。
**首选分隔符**（`path::preferred_separator`）在 Windows 上是 `\`、
在 POSIX 上是 `/`，`/` 运算符拼出来的结果用的是它。

| 事项 | Windows | POSIX |
|---|---|---|
| 接受的输入分隔符 | `\` 与 `/` | 只有 `/` |
| `preferred_separator` | `\` | `/` |
| 绝对路径的判据 | 要有根名字（盘符）或根目录 | 以 `/` 开头即可 |
| 大小写 | 文件系统不敏感，**`path` 的比较敏感** | 都敏感 |
| 保留字符 | `< > : " / \| ? *` 在文件名里非法 | 只有 `/` 与 `\0` |

> [!TIP]
> **写跨平台代码时，源码里一律用 `/` 拼路径。**
> Windows 收 `/`，POSIX 只认 `/`，因此这样写两边都能跑；
> 需要显示给用户看时再调 `make_preferred()` 换成平台习惯的分隔符。

---

# 第 2 节 编码现实

## 2.1 Windows 上的 `path` 是宽字符

**同一个 `fs::path`，在不同平台上内部存的东西不一样。**
在 Windows 上，`path::value_type` 是 `wchar_t`（两个字节，UTF-16）；
在 POSIX 上是 `char`。**这一点决定了字符串怎么进出 `path`。**

`C++`

```cpp
/* path_enc.cpp    编译：g++ -std=c++17 path_enc.cpp -o path_enc */
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <locale>
#include <string>
#include <type_traits>

namespace fs = std::filesystem;

// 把一段字节按十六进制打出来，看清它到底是哪些字节
void hexdump(const char *tag, const std::string &s) {
    std::printf("%-22s (%2d 字节) ", tag, (int)s.size());
    for (unsigned char c : s) std::printf("%02X ", c);
    std::printf("\n");
}

int main() {
    std::printf("path::value_type 是 wchar_t ：%d（%d 字节）\n",
                (int)std::is_same_v<fs::path::value_type, wchar_t>, (int)sizeof(fs::path::value_type));
    std::printf("path 内部按 %d 字节的字符存路径\n\n", (int)sizeof(fs::path::value_type));

    // 源文件是 UTF-8，u8"..." 的原样字节就是 UTF-8
    const char *utf8name = u8"中文目录/测试文件.txt";
    fs::path p = fs::u8path(utf8name);
    hexdump("u8 字面量的字节", utf8name);
    hexdump("u8path 后 u8string()", p.u8string());

    // 最常用却最容易出错的一步：窄字符串
    try {
        std::string s = p.string();
        hexdump("当前 locale 下 string()", s);
    } catch (const std::exception &e) {
        std::printf("string() 抛异常：%s\n", e.what());
    }

    // locale 换成系统默认（本机是 936 代码页）之后再看
    try {
        std::locale::global(std::locale(""));
        std::string s2 = p.string();
        hexdump("切到系统 locale 后 string()", s2);
    } catch (const std::exception &e) {
        std::printf("切换 locale 失败：%s\n", e.what());
    }

    // 系统错误消息是当前代码页的窄字符串，不一定是 UTF-8
    std::error_code zero_ec;
    hexdump("error_code().message()", zero_ec.message());

    // 真正落盘：Windows 的文件 API 收宽字符，中文名照样能建
    fs::path base = fs::u8path(u8"中文目录");
    std::error_code ec;
    fs::create_directories(base, ec);
    std::printf("\ncreate_directories(中文目录)：ec = %d\n", ec.value());

    fs::path file = base / fs::u8path(u8"测试文件.txt");
    { std::ofstream out(file); out << "hello\n"; }
    std::printf("文件存在              ：%d\n", (int)fs::exists(file));
    std::printf("文件大小              ：%lld 字节\n", (long long)fs::file_size(file));
    std::printf("用 u8string 显示路径  ：%s\n", file.u8string().c_str());
    std::printf("用 string 显示路径    ：%s\n", file.string().c_str());

    std::printf("\n遍历这个目录：\n");
    for (const auto &e : fs::directory_iterator(base))
        std::printf("  %s\n", e.path().u8string().c_str());

    fs::remove_all(base, ec);
    std::printf("清理后还存在吗：%d\n", (int)fs::exists(base));
    return 0;
}
```

`实测数据`
`Text`

```text
path::value_type 是 wchar_t ：1（2 字节）
path 内部按 2 字节的字符存路径

u8 字面量的字节  (29 字节) E4 B8 AD E6 96 87 E7 9B AE E5 BD 95 2F E6 B5 8B E8 AF 95 E6 96 87 E4 BB B6 2E 74 78 74
u8path 后 u8string()  (29 字节) E4 B8 AD E6 96 87 E7 9B AE E5 BD 95 2F E6 B5 8B E8 AF 95 E6 96 87 E4 BB B6 2E 74 78 74
当前 locale 下 string() (29 字节) E4 B8 AD E6 96 87 E7 9B AE E5 BD 95 2F E6 B5 8B E8 AF 95 E6 96 87 E4 BB B6 2E 74 78 74
切换 locale 失败：locale::facet::_S_create_c_locale name not valid
error_code().message() (16 字节) B2 D9 D7 F7 B3 C9 B9 A6 CD EA B3 C9 A1 A3 0D 0A

create_directories(中文目录)：ec = 0
文件存在              ：1
文件大小              ：7 字节
用 u8string 显示路径  ：中文目录\测试文件.txt
用 string 显示路径    ：中文目录\测试文件.txt

遍历这个目录：
  中文目录\测试文件.txt
清理后还存在吗：0
```

**四条结论。**

**第一，`path` 在 Windows 上按 UTF-16 存，源文件里的 UTF-8 字面量要先换过去。**
`fs::u8path(u8"...")` 做的就是这件事：**把 UTF-8 的字节解成宽字符**。
反过来 `u8string()` 把宽字符编回 UTF-8，**两者是一对**。

**第二，中文路径在本机完全能用**：目录建出来了、文件写进去了
（7 字节）、`exists` 与 `file_size` 都正确。**因为 Windows 的文件 API
本来就收宽字符**，只要不经过窄字符串这一环，中文名不会出问题。

**第三，`string()` 的编码是实现的自由。**
本机 libstdc++ 给出的字节与 `u8string()` **完全相同**（都是 UTF-8）；
换成别家的标准库，`string()` 走的可能是当前 ANSI 代码页（本机是 936），
同一段代码就会得到另一串字节。**因此不要依赖 `string()` 的编码**，
**要跨实现可移植就只有一条路：进用 `u8path`、出用 `u8string`。**

**第四，`error_code::message()` 给的是当前代码页的窄字符串。**
那 16 个字节 `B2 D9 D7 F7 ...` 是 GBK 编码的「操作成功完成。」加一个回车换行，
**在 UTF-8 的终端里显示为乱码**。要在程序里比较或显示错误消息，
**用 `ec.value()`（数字）而不是 `ec.message()`（本地化文本）。**

> [!CAUTION]
> **Windows 上不要写 `fs::path p = some_utf8_std_string;`。**
> 隐式转换走的是「按当前编码解释字节」这条路，中文会变成乱码或直接抛异常。
> **正确写法是 `fs::u8path(utf8_string)`**，出来是 `u8string()`。
> 本机之所以两条路都对，是因为 libstdc++ 恰好把 `string()` 也做成了 UTF-8，
> **换一个标准库就不成立**。

## 2.2 两个名字要分清

| 名字 | 作用 | 什么时候用 |
|---|---|---|
| `fs::u8path(s)` | UTF-8 的 `std::string` → `path` | **收到的路径来自文本文件、命令行、网络时** |
| `p.u8string()` | `path` → UTF-8 的 `std::string` | **要输出、要比较、要存进文件时** |
| `p.string()` | `path` → 窄字符串，**编码由实现定** | 只在确认全是 ASCII 时用 |
| `p.native()` | `path` → 平台原生字符串（Windows 上是 `std::wstring`） | 要直接调平台 API 时 |

C++20 起 `u8string()` 返回的是 `std::u8string`（元素类型 `char8_t`），
与 C++17 的 `std::string` 不是同一个类型，**升级标准时这一处要改**。

---

# 第 3 节 遍历目录

## 3.1 两种迭代器

`directory_iterator` 遍历一层，`recursive_directory_iterator` 连带子目录，
**两者的用法与范围 `for` 完全兼容**。

`C++`

```cpp
/* iter_dir.cpp    编译：g++ -std=c++17 iter_dir.cpp -o iter_dir */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main() {
    fs::path root = "iter_lab";
    std::error_code ec;
    fs::remove_all(root, ec);

    // 造一棵小树：3 个文件在根下，2 个在子目录里
    fs::create_directories(root / "sub/deep");
    for (const char *n : {"charlie.txt", "alpha.txt", "bravo.log"})
        std::ofstream(root / n) << "0123456789\n";
    std::ofstream(root / "sub/middle.txt") << "12345\n";
    std::ofstream(root / "sub/deep/leaf.bin") << "12345678901234567890\n";

    std::printf("非递归遍历 iter_lab（创建顺序是 charlie, alpha, bravo, sub）：\n");
    std::vector<std::string> order;
    for (const auto &e : fs::directory_iterator(root)) {
        std::string name = e.path().filename().string();
        order.push_back(name);
        std::printf("  %-14s 是目录：%d  是普通文件：%d  ", name.c_str(),
                    (int)e.is_directory(), (int)e.is_regular_file());
        if (e.is_regular_file()) std::printf("大小 %lld", (long long)e.file_size());
        std::printf("\n");
    }
    std::vector<std::string> sorted = order;
    std::sort(sorted.begin(), sorted.end());
    std::printf("\n遍历顺序与排序后是否一致：%d\n", (int)(order == sorted));
    std::printf("遍历顺序：");
    for (const auto &s : order) std::printf(" %s", s.c_str());
    std::printf("\n排序之后：");
    for (const auto &s : sorted) std::printf(" %s", s.c_str());
    std::printf("\n");

    // 递归遍历（depth() 在迭代器上，不在条目上）
    std::printf("\n递归遍历：\n");
    int files = 0, dirs = 0;
    for (auto it = fs::recursive_directory_iterator(root); it != fs::recursive_directory_iterator(); ++it) {
        std::printf("  深度 %d  %-30s %s\n", it.depth(),
                    fs::relative(it->path(), root).string().c_str(),
                    it->is_directory() ? "<目录>" : "<文件>");
        if (it->is_directory()) dirs++; else files++;
    }
    std::printf("合计 %d 个文件、%d 个目录\n", files, dirs);

    // 时间戳：C++17 里 file_time_type 与 system_clock 不是同一个时钟
    auto ft = fs::last_write_time(root / "alpha.txt");
    std::printf("\nlast_write_time 的原始计数：%lld，单位 1/%lld 秒\n",
                (long long)ft.time_since_epoch().count(), (long long)decltype(ft)::duration::period::den);

    // C++17 没有 clock_cast，只能靠两个时钟当前值的差来换算
    auto sys_now = std::chrono::system_clock::now();
    auto file_now = fs::file_time_type::clock::now();
    auto sys_tp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ft - file_now + sys_now);
    std::time_t tt = std::chrono::system_clock::to_time_t(sys_tp);
    char buf[64];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", std::localtime(&tt));
    std::printf("换算成 system_clock 之后：%s\n", buf);

    fs::remove_all(root, ec);
    std::printf("\n清理完成，iter_lab 还在吗：%d\n", (int)fs::exists(root));
    return 0;
}
```

`实测数据`
`Text`

```text
非递归遍历 iter_lab（创建顺序是 charlie, alpha, bravo, sub）：
  alpha.txt      是目录：0  是普通文件：1  大小 12
  bravo.log      是目录：0  是普通文件：1  大小 12
  charlie.txt    是目录：0  是普通文件：1  大小 12
  sub            是目录：1  是普通文件：0

遍历顺序与排序后是否一致：1
遍历顺序： alpha.txt bravo.log charlie.txt sub
排序之后： alpha.txt bravo.log charlie.txt sub

递归遍历：
  深度 0  alpha.txt                      <文件>
  深度 0  bravo.log                      <文件>
  深度 0  charlie.txt                    <文件>
  深度 0  sub                            <目录>
  深度 1  sub\deep                       <目录>
  深度 2  sub\deep\leaf.bin              <文件>
  深度 1  sub\middle.txt                 <文件>
合计 5 个文件、2 个目录

last_write_time 的原始计数：-4646838426000000000，单位 1/1000000000 秒
换算成 system_clock 之后：2026-10-01 11:32:54

清理完成，iter_lab 还在吗：0
```

**遍历顺序这一次恰好与字典序一致，但这不是保证。**
标准只说「顺序不指定」，本机这一次是 NTFS 的返回顺序，
而创建顺序（`charlie`、`alpha`、`bravo`、`sub`）与它并不相同。
**要确定的顺序就必须自行排序**（例子里的 `std::sort` 属于 `09-高阶数据结构` 板块）。

**`depth()` 在迭代器上，不在条目上。** 写成 `e.depth()` 通不过编译：

`实测数据`
`Text`

```text
error: 'const class std::filesystem::__cxx11::directory_entry' has no member named 'depth'
   48 |         std::printf("  深度 %d  %-30s %s\n", e.depth(),
      |                                                ^~~~~
```

**`last_write_time` 返回的时钟不是 `system_clock`。**
它的原始计数是一个很大的负数——**因为它的起点不是 1970 年**
（本机实现是 1601 年，与 Windows 的 FILETIME 一致）。
要变成人能读的时间，**须先换算到 `system_clock`**。
C++17 没有 `clock_cast`，只能像例子那样：
**取两个时钟当前值的差，把文件时间平移到 `system_clock` 上**。
C++20 起可以直接写 `std::chrono::clock_cast<std::chrono::system_clock>(ft)`。

## 3.2 每查一次属性都是一次系统调用

`directory_entry` 会把一些属性缓存起来，但**只在被查询时才真的去查**。
下面这组对照给出这个代价：同一个目录 2000 个空文件，
一种只数条目，另一种每条都问一次类型与大小。

`C++`

```cpp
/* iter_cost.cpp    编译：g++ -std=c++17 iter_cost.cpp -o iter_cost */
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main() {
    fs::path root = "cost_lab";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    for (int i = 0; i < 2000; i++)
        std::ofstream(root / ("f" + std::to_string(i) + ".tmp")).close();

    // A：只数条目
    {
        auto t0 = std::chrono::steady_clock::now();
        int n = 0;
        for (const auto &e : fs::directory_iterator(root)) { (void)e; n++; }
        auto t1 = std::chrono::steady_clock::now();
        std::printf("A 非递归、只计数              ：%4d 条，%.3f ms\n", n,
                    std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    // B：每条都问一次类型与大小
    {
        auto t0 = std::chrono::steady_clock::now();
        int n = 0;
        unsigned long long bytes = 0;
        for (const auto &e : fs::directory_iterator(root)) {
            n++;
            if (e.is_regular_file()) bytes += e.file_size();
        }
        auto t1 = std::chrono::steady_clock::now();
        std::printf("B 非递归、每条查类型与大小    ：%4d 条，%.3f ms（共 %llu 字节）\n", n,
                    std::chrono::duration<double, std::milli>(t1 - t0).count(), bytes);
    }
    // C：递归、只计数
    {
        auto t0 = std::chrono::steady_clock::now();
        int n = 0;
        for (const auto &e : fs::recursive_directory_iterator(root)) { (void)e; n++; }
        auto t1 = std::chrono::steady_clock::now();
        std::printf("C 递归、只计数                ：%4d 条，%.3f ms\n", n,
                    std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    // D：递归、每条查类型
    {
        auto t0 = std::chrono::steady_clock::now();
        int n = 0, dirs = 0;
        for (const auto &e : fs::recursive_directory_iterator(root)) {
            n++;
            if (e.is_directory()) dirs++;
        }
        auto t1 = std::chrono::steady_clock::now();
        std::printf("D 递归、每条查类型            ：%4d 条，%.3f ms（目录 %d）\n", n,
                    std::chrono::duration<double, std::milli>(t1 - t0).count(), dirs);
    }
    fs::remove_all(root, ec);
    return 0;
}
```

`实测数据`
`Text`

```text
A 非递归、只计数              ：2000 条，2.261 ms
B 非递归、每条查类型与大小    ：2000 条，115.828 ms（共 0 字节）
C 递归、只计数                ：2000 条，39.927 ms
D 递归、每条查类型            ：2000 条，92.637 ms（目录 0）
```

**同一批 2000 个条目，只数条目 2.26 毫秒，多问一次类型与大小变成 115.8 毫秒——慢了 51 倍。**
递归那一对同样如此（39.9 对 92.6 毫秒），而且递归本身就比非递归慢，
因为它对每个子目录都要单独打开一次。

**结论是「按需查询」**：只要文件名时不要连带问属性；
确实要大小或类型时，接受这个代价；
**要处理大目录时，先把名字收集起来，再决定对哪些条目去查属性**。

> [!IMPORTANT]
> **`directory_entry` 的属性查询是「懒」的，代价是一次系统调用。**
> 遍历一个几万条目的目录时，**每多问一个属性就多几万次系统调用**，
> 这通常是整个扫描过程里最慢的一环，比读文件内容还慢。

## 3.3 遍历时的其它选项

| 选项 / 成员 | 作用 |
|---|---|
| `directory_options::skip_permission_denied` | 遇到没权限的目录跳过，不抛异常 |
| `directory_options::follow_directory_symlink` | 递归时跟进符号链接（可能成环） |
| `it.disable_recursion_pending()` | 不再进入当前目录 |
| `it.pop()` | 退回上一层（递归迭代器专有） |
| `it.depth()` | 当前深度，根下是 0 |
| `recursive_directory_iterator(root, ec)` | 不抛异常的构造方式，`ec` 报告打不开目录 |

**`follow_directory_symlink` 这一点值得注意**：跟进去之后如果遇到指回上层的链接，
遍历不会自己停下来。**要限制深度或自行判断。**

---

# 第 4 节 创建、删除、改名、复制

## 4.1 全套操作

`C++`

```cpp
/* fs_ops.cpp    编译：g++ -std=c++17 fs_ops.cpp -o fs_ops */
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

void line(const char *what, bool r) { std::printf("%-42s -> %d\n", what, (int)r); }

int main() {
    std::error_code ec;
    fs::path root = "ops_lab";
    fs::remove_all(root, ec);

    // 建目录：一次一层与一次多层
    line("create_directory(\"ops_lab\")", fs::create_directory(root, ec));
    line("create_directory(\"ops_lab\") 再来一次", fs::create_directory(root, ec));
    std::printf("  第二次之后 ec = %d\n", ec.value());
    line("create_directory(\"ops_lab/a/b/c\")", fs::create_directory(root / "a/b/c", ec));
    std::printf("  失败时 ec = %d（%s）\n", ec.value(), ec.message().c_str());
    line("create_directories(\"ops_lab/a/b/c\")", fs::create_directories(root / "a/b/c", ec));
    line("create_directories(\"ops_lab/a/b/c\") 再来一次", fs::create_directories(root / "a/b/c", ec));

    // 造文件并复制、改名
    { std::ofstream(root / "src.txt") << "hello filesystem\n"; }
    line("copy_file(src.txt, dst.txt)", fs::copy_file(root / "src.txt", root / "dst.txt", ec));
    std::printf("  大小：%lld 字节\n", (long long)fs::file_size(root / "dst.txt"));
    line("copy_file 再来一次（不覆盖）", fs::copy_file(root / "src.txt", root / "dst.txt", ec));
    std::printf("  失败时 ec = %d（%s）\n", ec.value(), ec.message().c_str());
    line("copy_file 带 overwrite_existing", fs::copy_file(root / "src.txt", root / "dst.txt",
                                                        fs::copy_options::overwrite_existing, ec));
    fs::rename(root / "dst.txt", root / "renamed.txt", ec);          // rename 返回 void
    std::printf("%-42s -> ec = %d\n", "rename(dst.txt, renamed.txt)", ec.value());
    fs::rename(root / "renamed.txt", root / "src.txt", ec);          // 目标已存在
    std::printf("%-42s -> ec = %d\n", "rename 到一个已存在的文件", ec.value());
    std::printf("  之后 src.txt 还在：%d，renamed.txt 还在：%d\n",
                (int)fs::exists(root / "src.txt"), (int)fs::exists(root / "renamed.txt"));

    // 查询
    std::printf("\n查询：\n");
    std::printf("  exists(root)                ：%d\n", (int)fs::exists(root));
    std::printf("  is_directory(root)          ：%d\n", (int)fs::is_directory(root));
    std::printf("  is_regular_file(root)       ：%d\n", (int)fs::is_regular_file(root));
    std::printf("  is_empty(root/a)            ：%d\n", (int)fs::is_empty(root / "a"));
    std::printf("  absolute(root)              ：%s\n", fs::absolute(root).string().c_str());
    std::printf("  current_path()              ：%s\n", fs::current_path().string().c_str());

    // 删：remove 删空目录或文件，remove_all 连内容一起删并返回个数
    std::printf("\n删除：\n");
    bool r1 = fs::remove(root / "no_such.txt", ec);       // 每步先调用，再读 ec
    std::printf("  remove(不存在的文件)        ：%d，ec = %d\n", (int)r1, ec.value());
    bool r2 = fs::remove(root / "a", ec);
    std::printf("  remove(root/a)（非空目录）  ：%d，ec = %d\n", (int)r2, ec.value());
    std::printf("    这条 ec 的消息（当前代码页的字节）：");
    for (unsigned char c : ec.message()) std::printf("%02X ", c);
    std::printf("\n");
    unsigned long long n = fs::remove_all(root, ec);
    std::printf("  remove_all(root)            ：%llu（删掉的条目数）\n", n);
    n = fs::remove_all(root, ec);
    std::printf("  remove_all(不存在的路径)    ：%llu，ec = %d\n", n, ec.value());

    // 磁盘空间
    fs::space_info si = fs::space(".");
    std::printf("\nspace(\".\")：容量 %llu GB，空闲 %llu GB\n",
                (unsigned long long)(si.capacity / 1000000000ULL),
                (unsigned long long)(si.free / 1000000000ULL));
    return 0;
}
```

`实测数据`
`Text`

```text
create_directory("ops_lab")                -> 1
create_directory("ops_lab") 再来一次   -> 0
  第二次之后 ec = 0
create_directory("ops_lab/a/b/c")          -> 0
  失败时 ec = 2（No such file or directory）
create_directories("ops_lab/a/b/c")        -> 1
create_directories("ops_lab/a/b/c") 再来一次 -> 0
copy_file(src.txt, dst.txt)                -> 1
  大小：18 字节
copy_file 再来一次（不覆盖）      -> 0
  失败时 ec = 17（File exists）
copy_file 带 overwrite_existing           -> 1
rename(dst.txt, renamed.txt)               -> ec = 0
rename 到一个已存在的文件         -> ec = 0
  之后 src.txt 还在：1，renamed.txt 还在：0

查询：
  exists(root)                ：1
  is_directory(root)          ：1
  is_regular_file(root)       ：0
  is_empty(root/a)            ：0
  absolute(root)              ：K:\...\ops_lab
  current_path()              ：K:\...\b0567_lab

删除：
  remove(不存在的文件)        ：0，ec = 0
  remove(root/a)（非空目录）  ：0，ec = 5
    这条 ec 的消息（当前代码页的字节）：BE DC BE F8 B7 C3 CE CA A1 A3 0D 0A
  remove_all(root)            ：5（删掉的条目数）
  remove_all(不存在的路径)    ：0，ec = 0

space(".")：容量 4000 GB，空闲 386 GB
```

**这张表里有六处要点。**

**第一，`create_directory` 与 `create_directories` 的区别是「中间层」。**
`create_directory("ops_lab/a/b/c")` 在 `a` 不存在时失败（`ec = 2`），
`create_directories` 则会把中间层一起建出来。

**第二，目录已存在不是错误。** 两个函数在「目标已存在」时都返回 `false`，
但 **`ec` 保持为 0**——**「返回假」与「出错」是两件事**，
只检查 `ec` 会把这种情况漏掉，只检查返回值又会把它当失败。

**第三，`copy_file` 默认不覆盖。** 目标存在时返回 `false`、`ec = 17`（`EEXIST`），
要覆盖必须显式给 `copy_options::overwrite_existing`。

**第四，`rename` 返回 `void`，只能靠 `ec` 判断。** 目标已存在时本机 `ec = 0`
且目标被**替换**掉了（`src.txt` 还在、`renamed.txt` 没了）——
Windows 上的 `rename` 走的是「替换」语义。**这一条与 POSIX 一致，
但不要依赖它**：要确保替换，用 `fs::rename` 之后自己校验。

**第五，`remove` 的三个结果要分清。**
删不存在的路径返回 `false` 且 `ec = 0`（不算错误）；
删非空目录返回 `false` 且 **`ec = 5`**（Windows 的「拒绝访问」，
那 12 个字节是 GBK 编码的「拒绝访问。」加回车换行）；
删空目录或文件返回 `true`。**`remove` 不会递归**，要连内容一起删就用 `remove_all`。

**第六，`remove_all` 返回删掉的条目数，且对不存在的路径返回 0 而不报错。**
例子里的 `5` 是 `src.txt`、`a`、`a/b`、`a/b/c` 与 `ops_lab` 自己。

> [!WARNING]
> **`remove_all` 的返回值可以用来确认删干净了。** 它返回 0 有两种可能：
> 路径本来就不存在，或者什么都没有删掉。**要区分这两者，先 `exists` 一下**，
> 否则一个写错的路径会让「清理成功」的日志掩盖真实问题。

## 4.2 一个求值顺序的陷阱

上面几节里每一行都是**先调用、把结果存进变量，再读 `ec`**。
写成一行会出问题——**函数实参的求值顺序是不确定的**
（《04-语法/04-表达式与运算符.md》第 4.1 小节）：

`C++`

```cpp
// （下面是节选）
// 错的：ec.value() 可能在 remove 之前就被求值
std::printf("返回 %d，ec = %d\n", (int)fs::remove(p, ec), ec.value());
```

**两种写法放在一起对比，差别一眼可见**：

`C++`

```cpp
/* order_trap.cpp    编译：g++ -std=c++17 order_trap.cpp -o order_trap */
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
    std::error_code ec;
    fs::remove_all("ord_lab", ec);
    fs::create_directories("ord_lab/sub", ec);
    std::ofstream("ord_lab/sub/f.txt") << "x";      // 让 sub 非空，remove 会失败

    // 错的：两个实参的求值顺序不确定，ec.value() 可能先被算出来
    std::printf("错的：返回 %d，ec = %d\n",
                (int)fs::remove("ord_lab/sub", ec), ec.value());

    // 对的：拆成两个语句，先调用，再读 ec
    ec.clear();
    bool ok = fs::remove("ord_lab/sub", ec);
    std::printf("对的：返回 %d，ec = %d\n", (int)ok, ec.value());

    fs::remove_all("ord_lab", ec);
    return 0;
}
```

`实测数据`
`Text`

```text
错的：返回 0，ec = 0
对的：返回 0，ec = 5
```

**错的那一行打印的 `ec` 是上一次留下的 `0`**，而实际上 `remove` 已经把它
设成了 `5`——**错误信息对不上真实的失败原因**。本机 gcc 按从右到左的次序
求值函数实参，因此 `ec.value()` 先被算出来；**换一个编译器可能又变成对的**，
这类问题最难查。

**正确写法只有一条**：把操作与取值分成两个语句。

## 4.3 其余常用件备查

除了上面那几件，`<filesystem>` 还有一批用得少、但迟早会用到的东西。
它们的行为大多由 `copy_options`、`perm_options` 这类枚举决定，
**枚举给错不会报错，只是行为与预期不同**，因此值得先了解：

| 名字 | 做什么 | 注意 |
|---|---|---|
| `fs::copy(from, to, opt)` | 复制文件，**也能复制整个目录树** | 默认不覆盖；递归要显式给 `copy_options::recursive` |
| `fs::resize_file(p, n)` | 把文件截断或扩展到 `n` 字节 | 目标必须是普通文件，**对目录报错** |
| `fs::permissions(p, prms, opt)` | 改权限位 | **Windows 上只有只读位落到磁盘**，其余位被忽略 |
| `fs::equivalent(a, b)` | 两个路径是否指同一个文件 | 要访问磁盘；**抛异常的版本在路径不存在时会失败** |
| `fs::relative(p, base)` | 算出 `p` 相对 `base` 的路径 | **要访问磁盘**（先规范化再算） |
| `p.lexically_relative(base)` | 同上，纯文本版 | 不做符号链接解析，结果可能与 `relative` 不同 |
| `fs::read_symlink(p)` | 读符号链接指向哪里 | 不是链接时报错 |
| `fs::hard_link_count(p)` | 硬链接数 | POSIX 上用来判断「这个文件还有没有别的名字」 |
| `fs::temp_directory_path()` | 系统临时目录 | 目录存在不代表可写，要自己试 |

`实测数据`
`Text`

```text
resize_file(100) -> 大小 100，ec=0         resize_file(目录) -> ec=13
copy recursive   -> ec=0，子文件在吗 1
relative         -> [sub\b.txt] ec=0       lexically_relative -> [sub\b.txt]
equivalent(同一个文件) = 1                 equivalent(不存在的路径) -> 返回 0, ec=0
只留 owner_read -> ec=0，还能追加写吗：0     恢复读写位之后：1
read_symlink(普通文件) -> ec=40            hard_link_count = 1
```

**权限那一行需要单独说明**：它说明 Windows 上确实拦住了写操作，
**而 `ec` 仍然是 0**——权限设置本身是成功的，被拒绝的是后面那次写入。
**「设置成功」与「之后能写」是两件事**，这与第 4.1 小节里
「返回假不等于出错」是同一类区分。

---

# 第 5 节 两条错误处理路线

## 5.1 抛异常的版本

不带 `error_code` 参数的重载在失败时抛 `std::filesystem::filesystem_error`，
它是 `std::system_error` 的派生类，**带三个额外信息**：
`path1()`、`path2()`（涉及的两个路径）与 `code()`（错误码）。

`C++`

```cpp
/* fs_err_exc.cpp    编译：g++ -std=c++17 fs_err_exc.cpp -o fs_err_exc */
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// 抛异常的版本：每个操作都单独 try
void attempt(const char *what, void (*fn)()) {
    std::printf("%-40s ", what);
    try {
        fn();
        std::printf("没有异常\n");
    } catch (const fs::filesystem_error &e) {
        std::printf("抛 filesystem_error\n");
        std::printf("     what()       = %s\n", e.what());
        std::printf("     code().value = %d\n", e.code().value());
        std::printf("     path1()      = [%s]\n", e.path1().string().c_str());
        std::printf("     path2()      = [%s]\n", e.path2().string().c_str());
        std::printf("     是 system_error 吗：%d\n", (int)(dynamic_cast<const std::system_error *>(&e) != nullptr));
    } catch (const std::exception &e) {
        std::printf("抛其它异常：%s\n", e.what());
    }
}

int main() {
    std::error_code ec;
    fs::remove_all("err_lab", ec);
    fs::create_directories("err_lab");

    attempt("遍历不存在的目录", [] {
        for (auto &e : fs::directory_iterator("err_lab/no_such_dir")) (void)e;
    });
    attempt("file_size 一个目录", [] { (void)fs::file_size("err_lab"); });
    attempt("copy_file 目标已存在", [] {
        std::ofstream("err_lab/a.txt") << "a";
        std::ofstream("err_lab/b.txt") << "b";
        fs::copy_file("err_lab/a.txt", "err_lab/b.txt");
    });
    attempt("create_directory 名字含非法字符", [] { fs::create_directory("err_lab/a<b>c"); });
    attempt("create_directories 中间层是文件", [] { fs::create_directories("err_lab/a.txt/x/y"); });
    attempt("canonical 不存在的路径", [] { (void)fs::canonical("err_lab/not_there"); });
    attempt("remove 不存在的文件（不抛）", [] { (void)fs::remove("err_lab/not_there"); });

    fs::remove_all("err_lab", ec);
    return 0;
}
```

`实测数据`
`Text`

```text
遍历不存在的目录                 抛 filesystem_error
     what()       = filesystem error: directory iterator cannot open directory: No such file or directory [err_lab/no_such_dir]
     code().value = 2
     path1()      = [err_lab/no_such_dir]
     path2()      = []
     是 system_error 吗：1
file_size 一个目录                   抛 filesystem_error
     what()       = filesystem error: cannot get file size: Is a directory [err_lab]
     code().value = 21
     path1()      = [err_lab]
copy_file 目标已存在                抛 filesystem_error
     what()       = filesystem error: cannot copy file: File exists [err_lab/a.txt] [err_lab/b.txt]
     code().value = 17
     path1()      = [err_lab/a.txt]
     path2()      = [err_lab/b.txt]
create_directory 名字含非法字符   抛 filesystem_error
     what()       = filesystem error: cannot create directory: Invalid argument [err_lab/a<b>c]
     code().value = 22
create_directories 中间层是文件    抛 filesystem_error
     what()       = filesystem error: cannot create directories: Not a directory [err_lab/a.txt/x/y]
     code().value = 20
canonical 不存在的路径             抛 filesystem_error
     what()       = filesystem error: cannot make canonical path: No such file or directory [err_lab/not_there]
     code().value = 2
remove 不存在的文件（不抛）    没有异常
```

**`what()` 的格式是固定的三段**：`filesystem error: ` 加「做了什么」加
`[路径]`，`copy_file` 那种涉及两个路径的会给出两对方括号。
**这段文字是英文的**，与本机 `ec.message()` 给出 GBK 文本形成对照（第 2.1 小节）。

**最后一行说明「不抛」也是有的**：`remove` 删一个不存在的文件
按标准就不算错误，因此不抛异常。**「不抛」与「成功」不是一回事**，
它的返回值是 `false`。

## 5.2 收 `error_code` 的版本

同一个操作，带 `error_code&` 参数的重载**不抛异常**，
而是把错误码写进这个参数、并返回一个「成功与否」的值。

`C++`

```cpp
/* fs_err_ec.cpp    编译：g++ -std=c++17 fs_err_ec.cpp -o fs_err_ec */
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
    std::error_code ec;
    fs::remove_all("ec_lab", ec);
    fs::create_directories("ec_lab");

    // 同一个操作，走 error_code 这条路的写法：不抛，靠返回值与 ec 判断
    std::printf("遍历不存在的目录：\n");
    fs::directory_iterator it("ec_lab/no_such_dir", ec);
    std::printf("  构造之后 ec = %d（%s），拿到了迭代器吗：%d\n",
                ec.value(), ec.message().c_str(), (int)(it != fs::directory_iterator()));

    ec.clear();
    std::printf("\nfile_size 一个目录：\n");
    auto sz = fs::file_size("ec_lab", ec);
    std::printf("  返回值 = %llu，ec = %d（%s）\n", (unsigned long long)sz, ec.value(), ec.message().c_str());

    ec.clear();
    std::printf("\ncopy_file 目标已存在：\n");
    { std::ofstream("ec_lab/a.txt") << "a"; std::ofstream("ec_lab/b.txt") << "b"; }
    bool ok = fs::copy_file("ec_lab/a.txt", "ec_lab/b.txt", ec);
    std::printf("  返回 %d，ec = %d（%s）\n", (int)ok, ec.value(), ec.message().c_str());

    ec.clear();
    std::printf("\ncreate_directory 名字含非法字符：\n");
    ok = fs::create_directory("ec_lab/a<b>c", ec);
    std::printf("  返回 %d，ec = %d（%s）\n", (int)ok, ec.value(), ec.message().c_str());

    ec.clear();
    std::printf("\ncanonical 不存在的路径：\n");
    fs::path p = fs::canonical("ec_lab/not_there", ec);
    std::printf("  返回路径 = [%s]，ec = %d（%s）\n", p.string().c_str(), ec.value(), ec.message().c_str());

    // 不抛的那几个：返回值本身就能说明问题
    ec.clear();
    std::printf("\n不抛异常的几个：\n");
    std::printf("  remove(不存在)            = %d，ec = %d\n", (int)fs::remove("ec_lab/no", ec), ec.value());
    std::printf("  create_directory(已存在)  = %d，ec = %d\n", (int)fs::create_directory("ec_lab", ec), ec.value());
    std::printf("  remove_all(不存在)        = %llu，ec = %d\n",
                (unsigned long long)fs::remove_all("ec_lab/no", ec), ec.value());

    fs::remove_all("ec_lab", ec);
    return 0;
}
```

`实测数据`
`Text`

```text
遍历不存在的目录：
  构造之后 ec = 2（No such file or directory），拿到了迭代器吗：0

file_size 一个目录：
  返回值 = 18446744073709551615，ec = 21（Is a directory）

copy_file 目标已存在：
  返回 0，ec = 17（File exists）

create_directory 名字含非法字符：
  返回 0，ec = 22（Invalid argument）

canonical 不存在的路径：
  返回路径 = []，ec = 2（No such file or directory）

不抛异常的几个：
  remove(不存在)            = 0，ec = 0
  create_directory(已存在)  = 0，ec = 0
  remove_all(不存在)        = 0，ec = 0
```

**两处与抛异常版本一致的错误码**（`2`、`17`、`21`、`22` 与上一小节一一对应），
说明两条路线报的是同一件事，只是传递方式不同。

**一处要注意的是 `file_size` 的返回值**：失败时它返回
`static_cast<uintmax_t>(-1)`，也就是 `18446744073709551615`。
**这个值不是「一个很大的文件」，而是「出错」**，因此**必须同时检查 `ec`**。

## 5.3 什么时候必须用 `error_code`

| 情形 | 该用哪条路线 | 理由 |
|---|---|---|
| 上层已经用异常处理错误 | 抛异常版 | 与既有风格一致，错误信息更全（自带路径） |
| 「文件不存在」是正常流程 | **`error_code` 版** | 用异常做流程控制代价高、可读性差 |
| 批量操作里允许个别失败 | **`error_code` 版** | 一个失败不该中断整批 |
| 要在 `noexcept` 函数里做文件操作 | **`error_code` 版** | 抛出去会直接 `std::terminate`（《04-语法/13-异常.md》第 4 节） |
| 遍历目录时跳过没权限的子目录 | **`error_code` 版**或 `skip_permission_denied` | 权限不足是常态 |
| 探测「这个路径能不能写」 | **`error_code` 版** | 探测失败本来就是预期结果之一 |

**一条更实用的判断**：**「这个失败是异常情况，还是预期结果？」**
预期的用 `error_code`，意外的用异常。

> [!CAUTION]
> **带 `error_code` 的重载只把「文件系统错误」转成错误码，别的异常照样抛。**
> 内存分配失败会抛 `std::bad_alloc`，路径转换失败可能抛别的异常。
> **因此外层该有的 `try` 不能因为「用了 error_code 版」就省掉。**

`文档`

> "In the cases where the error_code& ec argument is present, ...
> if an error occurs, ec is set to the error code ... and the
> function returns a value indicating failure."
>
> —— N4659 §30.10.7/4（`<filesystem>` 的错误报告约定）

> [!NOTE]
> **第 5 节小结**：`<filesystem>` 的每个操作都有两条路线——
> 抛 `filesystem_error` 的与收 `error_code` 的，**报的是同一批错误码**。
> **「文件不存在」这类预期结果用 `error_code`**，
> 意外情况用异常；**两者都要看返回值**，因为有些情况的返回值是 `false`
> 而 `ec` 仍是 0。

---

# 第 6 节 与 `stdio` 的对照

同一件事，C 与 C++ 各写一遍。对照的重点不只是代码长短，
还有**出错时能拿到什么信息**。

`C++`

```cpp
/* stdio_vs_fs.cpp    编译：g++ -std=c++17 stdio_vs_fs.cpp -o stdio_vs_fs */
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main() {
    std::error_code ec;
    fs::remove_all("cmp_lab", ec);
    fs::create_directories("cmp_lab");

    // 第一件事：建一个文件并写点东西
    std::printf("—— 建文件并写入 ——\n");
    {
        std::FILE *f = std::fopen("cmp_lab/c.txt", "wb");     // C 的路子
        if (f) { std::fputs("stdio\n", f); std::fclose(f); }
        std::printf("C  ：fopen + fputs + fclose，判空靠指针\n");
    }
    {
        std::ofstream out(fs::path("cmp_lab/cpp.txt"));        // C++ 的路子
        out << "filesystem\n";
        std::printf("C++：ofstream 构造即打开，析构即关闭（RAII）\n");
    }

    // 第二件事：问「文件多大」
    std::printf("\n—— 问文件大小 ——\n");
    {
        std::FILE *f = std::fopen("cmp_lab/c.txt", "rb");
        std::fseek(f, 0, SEEK_END);
        long n = std::ftell(f);
        std::fclose(f);
        std::printf("C  ：fseek + ftell = %ld 字节\n", n);
    }
    std::printf("C++：file_size      = %lld 字节\n", (long long)fs::file_size("cmp_lab/c.txt"));

    // 第三件事：改名
    std::printf("\n—— 改名 ——\n");
    std::printf("C  ：rename 返回 %d（0 表示成功）\n", std::rename("cmp_lab/c.txt", "cmp_lab/c2.txt"));
    fs::rename("cmp_lab/cpp.txt", "cmp_lab/cpp2.txt", ec);
    std::printf("C++：fs::rename 之后 ec = %d\n", ec.value());

    // 第四件事：列目录（C 标准库没有，只能靠平台 API）
    std::printf("\n—— 列目录 ——\n");
    std::printf("C  ：标准库没有这一项，要靠 dirent.h / FindFirstFile\n");
    for (const auto &e : fs::directory_iterator("cmp_lab"))
        std::printf("C++：%s\n", e.path().filename().string().c_str());

    // 第五件事：删文件
    std::printf("\n—— 删除 ——\n");
    std::printf("C  ：remove 返回 %d（0 表示成功）\n", std::remove("cmp_lab/c2.txt"));
    std::printf("C++：fs::remove 返回 %d（删掉的条目数）\n", (int)fs::remove("cmp_lab/cpp2.txt", ec));

    fs::remove_all("cmp_lab", ec);
    return 0;
}
```

`实测数据`
`Text`

```text
—— 建文件并写入 ——
C  ：fopen + fputs + fclose，判空靠指针
C++：ofstream 构造即打开，析构即关闭（RAII）

—— 问文件大小 ——
C  ：fseek + ftell = 6 字节
C++：file_size      = 6 字节

—— 改名 ——
C  ：rename 返回 0（0 表示成功）
C++：fs::rename 之后 ec = 0

—— 列目录 ——
C  ：标准库没有这一项，要靠 dirent.h / FindFirstFile
C++：c2.txt
C++：cpp2.txt

—— 删除 ——
C  ：remove 返回 0（0 表示成功）
C++：fs::remove 返回 1（删掉的条目数）
```

**逐件事的对照表**：

| 这件事 | C（A-01） | C++（本章节） | 差别在哪 |
|---|---|---|---|
| 打开文件 | `fopen` 返回指针，要判空 | `ofstream` 构造即打开 | **C++ 靠析构自动关闭**（《05-类与面向对象/06-RAII 与资源管理.md》第 4.1 小节） |
| 关闭文件 | `fclose`，每条路径都要写 | 析构函数里 | C 里漏一条路径就泄漏句柄 |
| 问大小 | `fseek` + `ftell`，**改了读写位置** | `file_size` | C 的做法有副作用 |
| 改名 | `rename`，失败给 `errno` | `fs::rename` + `ec` 或异常 | C++ 能给出路径 |
| 删除 | `remove`，返回 0 表示成功 | `fs::remove`，返回是否删掉 | **返回值语义相反**，不要混淆 |
| 列目录 | **标准库没有** | `directory_iterator` | 这是 `<filesystem>` 最大的补充 |
| 问属性 | 只能 `stat`（平台 API） | `is_regular_file` 等 | 跨平台 |
| 错误信息 | `errno` + `strerror` | `filesystem_error::what()` 带路径 | C++ 的信息更全 |

**一处最容易混的地方是返回值**：C 的 `remove` 成功返回 `0`，
C++ 的 `fs::remove` 成功返回 `true` 或「删掉的条目数」。
**两张相反的表放在一起对照，这一处差别不容易混淆。**

> [!TIP]
> **读写文件内容仍用 `stdio` 或 `iostream`，`<filesystem>` 管的是「文件系统」。**
> `<filesystem>` 里**没有**读写文件内容的函数——
> 那是《07-标准库/A-01-输入输出：stdio.md》与
> 《07-标准库/B-01-输入输出：iostream.md》的地盘。
> 两者的分工是清楚的：**一个管「文件在哪里、叫什么、多大」，一个管「里面写了什么」。**

---

# 第 7 节 速查表

| 常用件 | 一句话用途 | 典型坑 |
|---|---|---|
| `fs::path` | 装一个路径 | Windows 上内部是宽字符，编码见第 2 节 |
| `p / "name"` | 拼路径 | **`+=` 不补分隔符**，会拼成一个新名字 |
| `filename` / `stem` / `extension` | 取名字的三段 | 纯文本操作，不访问磁盘；`.gitignore` 的扩展名是空 |
| `parent_path` | 取上级目录 | 末尾带分隔符时会被吃掉 |
| `remove_filename` / `replace_extension` | 去掉文件名、换扩展名 | 都返回修改后的 `path`，原对象也会被改 |
| `lexically_normal` | 化简 `..` 与 `.` | 只做文本化简，不解析符号链接 |
| `p == q` | 比较路径 | **逐字符**，Windows 上大小写也敏感 |
| `fs::equivalent` | 按文件系统语义比较 | 要访问磁盘，文件不存在时抛异常 |
| `fs::u8path` / `u8string` | UTF-8 与 `path` 互转 | **Windows 上进出都走这一对** |
| `directory_iterator` | 遍历一层 | **顺序不保证** |
| `recursive_directory_iterator` | 递归遍历 | `depth()` 在**迭代器**上，不在条目上 |
| `directory_entry::is_*` | 判断条目类型 | **每次查询都是一次系统调用**，实测慢 51 倍 |
| `file_size` | 文件大小 | 对目录报错；失败返回 `-1`，要同时看 `ec` |
| `last_write_time` | 修改时间 | 时钟不是 `system_clock`，C++17 要手工换算 |
| `create_directory` | 建一层目录 | 已存在时返回 `false` 但 `ec` 是 0 |
| `create_directories` | 连中间层一起建 | 同上 |
| `copy_file` | 复制文件 | **默认不覆盖**，要 `copy_options::overwrite_existing` |
| `rename` | 改名/移动 | **返回 `void`**，只能看 `ec` |
| `remove` | 删文件或空目录 | 非空目录失败；路径不存在返回 `false` 但不算错 |
| `remove_all` | 连内容一起删 | 返回删掉的条目数；不存在的路径返回 0 |
| `exists` / `is_regular_file` / `is_directory` | 查询类型 | 每次都要访问磁盘 |
| `absolute` / `canonical` | 转绝对路径 | `canonical` 要求路径存在 |
| `current_path` | 当前工作目录 | 进程级状态，与「可执行文件所在目录」不是一回事 |
| `space` | 磁盘容量与剩余 | 参数是任意路径 |
| `error_code` 重载 | 不抛异常的那一套 | **返回值与 `ec` 都要看**；别的异常照样抛 |

**配套示例见 [`B-examples/07-standard-library/07-cpp-filesystem-scan/`](../B-examples/07-standard-library/07-cpp-filesystem-scan/)，配套练习见 [`C-templates/07-standard-library/07-cpp-filesystem/`](../C-templates/07-standard-library/07-cpp-filesystem/)。**
示例把本章节的东西串成一个目录扫描工具：递归遍历、按扩展名统计、
输出前几名、两条错误处理路线各写一遍，并对结果做自测。

---

# 附录 A 复现本章节实测

## A.1 环境

`实测数据`

| 项 | 值 |
|---|---|
| 编译器 | g++ 15.2.0（MinGW-w64，x86_64-win32-seh） |
| 标准 | `-std=c++17` |
| 系统 | Windows 11 build 22631 |
| 文件系统 | NTFS |
| 工作目录 | **完整路径中含中文字符**（本仓库所在的目录就是这样一个路径） |
| 控制台代码页 | 65001（UTF-8），系统 ANSI 代码页 936（GBK） |

**三点环境信息会影响结论**：**遍历顺序**由文件系统返回，本机是 NTFS；
**`ec.message()` 的字节**取决于 ANSI 代码页，本机是 936，换一台机器就是别的编码；
**而所有程序都是在含中文的路径下编译并运行的**——第 2.1 小节的
「中文路径能用」因此是一条真实场景下的结论，不是在纯 ASCII 目录里试出来的。
换到 POSIX 上，`path::value_type` 会变成 `char`，这几行输出也会跟着变。

## A.2 各程序的编译与运行

`Bash`

```bash
g++ -std=c++17 <文件名>.cpp -o <可执行名> && ./<可执行名>
```

**几点说明**：

| 程序 | 特殊之处 |
|---|---|
| `path_enc.cpp` | 会创建并删除 `中文目录`，输出与工作目录的编码设置相关 |
| `iter_dir.cpp` | 会创建并删除 `iter_lab`，含 2000 个空文件的计时 |
| `iter_cost.cpp` | 会创建并删除 `cost_lab`，耗时以本机为准 |
| `fs_ops.cpp` | 会创建并删除 `ops_lab`，`absolute` 与 `current_path` 的输出随目录变化 |
| `fs_err_exc.cpp` | 抛出的错误消息里含本机运行库的英文描述 |
| `fs_err_ec.cpp` | `ec.message()` 的字节随 ANSI 代码页变化 |
| `stdio_vs_fs.cpp` | 会创建并删除 `cmp_lab` |

**这些程序都会在自己的工作目录下建临时目录并清理**，
因此放在任意空目录里跑都可以，不需要额外准备数据。

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《07-标准库/A-01-输入输出：stdio.md》 | **对照**：`fopen` 一族与文件读写 |
| 《07-标准库/A-05-工具与其它：stdlib 与杂项.md》第 7 节 | 对照：`errno` 与 `strerror` 的错误报告方式 |
| 《07-标准库/B-02-std-string 与 string_view.md》 | **前置**：`path` 与字符串的互转 |
| 《07-标准库/B-00-导读：C++ 标准库与 C 的关系.md》 | **前置**：`<cxxx>` 与 `<xxx.h>` 的区别 |
| 《05-类与面向对象/06-RAII 与资源管理.md》第 4.1 小节 | **前置**：从 `fopen`/`fclose` 到 RAII 包装 |
| 《04-语法/13-异常.md》第 5 节 | **前置**：异常与错误码的分工 |
| 《04-语法/04-表达式与运算符.md》第 4.1 小节 | **前置**：函数实参的求值顺序不确定 |
| 《04-语法/02-数据类型与类型系统.md》第 2.2 小节 | **前置**：窄字符与宽字符 |
| 《08-一些散落的算法/05-排序（一）：比较排序的三种策略.md》第 5 节、《09-高阶数据结构/A-06-迭代器与范围：容器与算法之间的接口.md》第 1 节 | **后续**：遍历结果的排序与统计 |
| 《06-更底层/11-系统调用：程序与内核的边界.md》章节 | 后续：`stat`、`FindFirstFile` 这些平台 API |
