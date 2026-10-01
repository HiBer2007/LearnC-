# C++ 标准库与 C 的关系

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**C++ 的标准库不是另起炉灶搭起来的，它把 C 的那一套整个搬了进来。**

搬的过程中只动了两个地方：**名字放进 `std`，再在上面加一层类型安全的上层设施**。
`printf` 还在，`memcpy` 还在，`<math.h>` 里的函数一个不少。多出来的是
`std::string`、`iostream`、`<chrono>`、`<filesystem>` 这些 C 里没有的东西。

**这一层关系决定了读法**：读 B 段的任何一章，都要能回答「C 里对应的东西是什么、为什么换掉它」。
C 的那一半在 A 段已经讲透，这里不再重复 `printf` 怎么用，只讲**换到 C++ 这边之后发生了什么变化**。
读完本章节，应当能自己判断该包含 `<cstdio>` 还是 `<stdio.h>`、某个名字在不在 `std` 里、
某个设施需要哪一版标准，以及遇到不确定的名字去哪里查。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整源码与复现命令见附录 A；`文档` 表示引自标准草案或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<版本号>`）的含义见《README.md》。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| C 标准库是怎么组织的、查什么 | 《07-标准库/A-00-导读：C 标准库.md》 |
| `printf` 一族与流的缓冲 | 《07-标准库/A-01-输入输出：stdio.md》第 1 节 |
| `char[]` 与 `<string.h>` | 《07-标准库/A-02-字符串与内存：string.h.md》 |
| 命名空间与 `using` | 《05-类与面向对象/01-命名空间与 using.md》第 2 节 |
| `extern "C"` 与链接属性 | 《04-语法/11-作用域、生存期与链接.md》第 5 节 |

**相邻的章节**：本章是 B 段的导读，紧接着是《07-标准库/B-01-输入输出：iostream.md》与
《07-标准库/B-02-std-string 与 string_view.md》。容器、迭代器与算法不在本板块，见 `08-高阶数据结构` 板块；
CRT 实现、ABI 与内存布局见 `06-更底层` 板块。

| 本章各节 | 讲什么 |
|---|---|
| 第 1 节 | C++ 从 C 那里继承的 26 个头文件，`<xxx.h>` 与 `<cxxx>` 两个位置 |
| 第 2 节 | `<cstdio>` 与 `<stdio.h>` 的真实区别：标准的保证与两套实现的实际行为 |
| 第 3 节 | `std::printf`、`std::size_t`、`std::memcpy` 为什么存在；混用两套输出的后果 |
| 第 4 节 | 与 STL 的边界：本板块讲什么、不讲什么 |
| 第 5 节 | 版本：基准是 C++17，超出基准的怎么标 |
| 第 6 节 | 怎么查：cppreference、标准草案、编译器自己的头文件 |
| 第 7 节 | 三个设计取舍（只点出问题，答案在 B-01、B-02、B-03） |

---

# 第 1 节 一份内容，两个位置

**继承的规模是可以数出来的**：C++ 标准列出的 C 库设施头文件一共 26 个，
与 C 的 26 个头文件一一对应。

## 1.1 C++ 标准库里的 C 那一半

标准草案列了两张表。**Table 16 是 C++ 自己的头文件**（`<string>`、`<iostream>`、`<vector>`…），
**Table 17 是「为 C 标准库设施准备的 C++ 头文件」**，共 26 个：

`文档`

> "The facilities of the C standard library are provided in the additional
> headers shown in Table 17."
>
> —— N4659 §20.5.1.2/3

| Table 17 的 26 个头文件（N4659 §20.5.1.2/3） |
|---|
| `<cassert>` `<ccomplex>` `<cctype>` `<cerrno>` `<cfenv>` `<cfloat>` `<cinttypes>` `<ciso646>` `<climits>` `<clocale>` |
| `<cmath>` `<csetjmp>` `<csignal>` `<cstdarg>` `<cstdalign>` `<cstdbool>` `<cstddef>` `<cstdint>` `<cstdio>` `<cstdlib>` |
| `<cstring>` `<ctgmath>` `<ctime>` `<cuchar>` `<cwchar>` `<cwctype>` |

**这 26 个头文件一一对应 C 的 26 个头文件**（N4659 附录 D.5 的 Table 141）。
对应关系是名字层面的：去掉 `c`、加回 `.h`，就是 C 的那一个。

**其中四个是空壳**：`<ccomplex>`、`<ctgmath>`、`<cstdalign>`、`<cstdbool>` 只走个过场，
前者转手包含 `<complex>` 与 `<cmath>`，后两个在 C++17 里不定义任何东西。
`<ciso646>` 与 `<iso646.h>` 则是「包含它没有任何效果」：

`文档`

> "In particular, including the standard header `<iso646.h>` or `<ciso646>`
> has no effect."
>
> —— N4659 §20.5.1.2 脚注 171

## 1.2 两个位置：`<xxx.h>` 与 `<cxxx>`

C++ 把同一份声明放在**两个命名空间位置**上，于是有两条包含路径：

`Text`

```text
                    一份声明（原本来自 C 标准库）
                              │
            ┌─────────────────┴─────────────────┐
            │                                   │
        <stdio.h>                            <cstdio>
   保证：全局名字 printf                  保证：std::printf
   可能：std::printf                      可能：全局 printf
            │                                   │
            └─────────────────┬─────────────────┘
                              │
                  两套实现都选了「两边都给」
                  （但标准只保证各自那一半）
```

**「保证」与「可能」是这段话的关键。** 标准没有说 `<cstdio>` 不许给全局名字，
也没有说 `<stdio.h>` 不许给 `std::` 名字；它只说各自**至少**要给哪一份。
剩下的那一份给不给，由实现自己决定。

## 1.3 标准原文

`文档`

> "Except as noted in Clauses 20 through 33 and Annex D, the contents of each
> header `cname` is the same as that of the corresponding header `name.h` as
> specified in the C standard library. In the C++ standard library, however,
> the declarations (except for names which are defined as macros in C) are
> within namespace scope of the namespace `std`. It is unspecified whether
> these names (including any overloads added in Clauses 21 through 33 and
> Annex D) are first declared within the global namespace scope and are then
> injected into namespace `std` by explicit *using-declarations*."
>
> —— N4659 §20.5.1.2/4

**三句话，三个信息**：

| 原文 | 含义 |
|---|---|
| contents … is the same as that of … `name.h` | 内容一样，不是两套实现 |
| declarations … are within namespace scope of `std` | `<cxxx>` **必须**把名字放进 `std` |
| It is unspecified whether … global namespace scope | 是否也给全局名字，**标准不说** |

附录 D.5 把同样的事情从另一头说了一遍，还举了一个例子：

`文档`

> "[Example: The header `<cstdlib>` assuredly provides its declarations and
> definitions within the namespace `std`. It may also provide these names
> within the global namespace. The header `<stdlib.h>` assuredly provides the
> same declarations and definitions within the global namespace, much as in
> the C Standard. It may also provide these names within the namespace `std`.
> —end example]"
>
> —— N4659 §D.5/4

`文档`

> "The C standard library headers (Annex D.5) also define names within the
> global namespace, while the C++ headers for C library facilities (20.5.1.2)
> may also define names within the global namespace."
>
> —— N4659 §20.5.1.2 脚注 166

**`<xxx.h>` 这一族在附录 D 里**，也就是「弃用但保留」的清单。
它们不会消失——几亿行现存代码依赖它们——但新写的代码应当用 `<cxxx>`。

> [!IMPORTANT]
> **`<cstdio>` 与 `<stdio.h>` 的差别不是「两套函数」，而是「保证哪一份名字」。**
> `<cstdio>` 保证 `std::printf`，`<stdio.h>` 保证全局的 `printf`。
> 实测两个主流实现都额外给了另一半，但那是实现的选择，不是标准的承诺。

---

# 第 2 节 `<cstdio>` 与 `<stdio.h>` 的真实区别

**标准把话说得很松，日常真正遇到的是实现的选择。** 本节对照两种头文件的保证、
两套主流实现的实际行为，以及写错时编译器给出的报错原文。

## 2.1 只包含 `<cstdio>`：两个名字都能用

`C++`

```cpp
/* cxx_cstdio.cpp    编译：g++ -std=c++17 cxx_cstdio.cpp -o cxx_cstdio */
#include <cstdio>          // 只包含 C++ 形式的头文件

int main() {
    printf("不带 std:: 的 printf：可以调用\n");             // 全局名字：标准没保证
    std::printf("带 std:: 的 printf：也可以调用\n");        // std 名字：标准保证
    std::printf("两个名字是同一个函数吗：%s\n",
                (&printf == &std::printf) ? "是" : "否");
    return 0;
}
```

`实测数据`
`Text`

```text
不带 std:: 的 printf：可以调用
带 std:: 的 printf：也可以调用
两个名字是同一个函数吗：是
```

**取地址得到同一个值**，说明 `std::printf` 与全局的 `printf` 是同一个函数，
只是两个名字都能查到它。

## 2.2 只包含 `<stdio.h>`：`std::printf` 不存在

上面的程序改成只包含 `<stdio.h>`：

`C++`

```cpp
/* cxx_stdioh.cpp    编译：g++ -std=c++17 cxx_stdioh.cpp -o cxx_stdioh （失败） */
#include <stdio.h>         // 只包含 C 形式的头文件

int main() {
    printf("不带 std:: 的 printf：可以调用\n");
    std::printf("带 std:: 的 printf：也可以调用\n");        // 标准只保证全局那一半
    std::printf("两个名字是同一个函数吗：%s\n",
                (&printf == &std::printf) ? "是" : "否");
    return 0;
}
```

`实测数据`
`Text`

```text
cxx_stdioh.cpp:6:10: error: 'printf' is not a member of 'std'; did you mean 'printf'?
    6 |     std::printf("带 std:: 的 printf：也可以调用\n");
      |          ^~~~~~
In file included from cxx_stdioh.cpp:2:
<MinGW>/x86_64-w64-mingw32/include/stdio.h:350:5: note: 'printf' declared here
  350 | int printf (const char *__format, ...)
      |     ^~~~~~
```

**报错信息本身说明了问题**：`printf` 在全局命名空间里好好待着，
`std` 里没有它的名字。编译器还补了一句「你是想写 `printf` 吗」。

换一套工具链会得到同样的结论。用 MSVC 19.44 编译同一个文件：

`实测数据`
`Text`

```text
cxx_stdioh.cpp(6): error C2039: "printf": 不是 "std" 的成员
predefined C++ types (compiler internal)(357): note: 参见"std"的声明
```

**两套互不相干的实现给出同一个结论**：`<xxx.h>` 形式的头文件不负责 `std::` 那一半。
这两次编译里，MSVC 用的是 19.44（本机装了多套 MSVC，下文每处数据都注明用的是哪一套）。

## 2.3 实现里到底发生了什么

考察 libstdc++ 的 `<cstdio>` 到底是什么，比任何转述都直接：

`C++`

```cpp
// （下面是节选）libstdc++ 的 <cstdio> 去掉注释后的骨架
#include <stdio.h>                     // 先把 C 的那一份整个包含进来

// 把 <stdio.h> 里以宏形式给出的几个名字取消掉，换成真正的函数
#undef clearerr
#undef fclose
#undef getc
// …（后面还有一长串 #undef）

namespace std {
  using ::FILE;                        // 类型
  using ::fpos_t;
  using ::printf;                      // 函数
  using ::fopen;
  using ::fwrite;
  // …（Table 17 对应的名字逐个 using 进来）
}
```

**`<cstdio>` 不是另一个头文件，它是 `<stdio.h>` 加一层 `using` 声明。**
`using ::printf;` 写在 `namespace std` 里，所以 `std::printf` 与 `printf` 指的是同一个函数。
`<cstring>`、`<cstdlib>` 的结构一模一样。

这也解释了一条容易困惑的现象：**`<cstdio>` 里居然能拿到 `std::size_t`**。

`C++`

```cpp
/* cxx_mix.cpp    编译：g++ -std=c++17 cxx_mix.cpp -o cxx_mix */
#include <cstdio>          // C++ 形式
#include <stdio.h>         // C 形式，与上一行指向同一个物理头文件
#include <cstddef>         // std::size_t 在这里
#include <cstring>         // std::memcpy / memcpy 在这里

int main() {
    std::printf("两个头文件一起包含：编译通过\n");

    std::size_t n = 0;                       // 标准保证的名字
    size_t m = 0;                            // 全局名字，实现一并给的
    std::printf("std::size_t 与 size_t 同宽：%s（%zu 字节）\n",
                (sizeof(n) == sizeof(m)) ? "是" : "否", sizeof(n));

    char dst[8] = {};
    memcpy(dst, "abc", 4);                   // 不带 std:: 的 memcpy
    std::memcpy(dst, "xyz", 4);              // 带 std:: 的 memcpy
    std::printf("混用 memcpy 的结果：%s\n", dst);
    return 0;
}
```

`实测数据`
`Text`

```text
两个头文件一起包含：编译通过
std::size_t 与 size_t 同宽：是（8 字节）
混用 memcpy 的结果：xyz
```

**同时包含两者不会冲突**：它们是同一个物理头文件，`#include` 的重复包含由守卫宏挡掉。

> [!TIP]
> **`std::size_t` 的标准出处是 `<cstddef>`，`std::memcpy` 的是 `<cstring>`。**
> 只包含 `<cstdio>` 时它们能用，是 libstdc++ 一并包含的结果；
> 换一个实现（或者同一个实现的下一版）就可能不成立。
> **依赖标准保证的那一份，代码才可移植。**

## 2.4 写错了会怎样：把 C++ 的头文件名带进 C

C 里没有 `<cstdio>` 这个名字。用 `gcc` 编译同一个包含：

`C`

```c
/* c_cstdio.c    编译：gcc -std=c23 c_cstdio.c -o c_cstdio （失败） */
#include <cstdio>
int main(void) { printf("hi\n"); return 0; }
```

`实测数据`
`Text`

```text
c_cstdio.c:1:10: fatal error: cstdio: No such file or directory
    1 | #include <cstdio>
      |          ^~~~~~~~
compilation terminated.
```

**C 与 C++ 的头文件表在名字上不重叠**：C 只有 `<stdio.h>`，C++ 两个都有。
反过来把 `<stdio.h>` 写进 C 程序没有问题，因为 C 只认这一个。

> [!WARNING]
> **从 C 转过来的代码最常见的两处改错**：
> 一是保留了 `<stdio.h>` 又在代码里写 `std::printf`（编译不过，见第 2.2 小节）；
> 二是把 `<stdio.h>` 改成 `<cstdio>` 却漏掉了类型——`<cstdio>` 里没有 `size_t` 的保证。

## 2.5 结论：写哪一个

| 代码的类型 | 该用哪个 | 理由 |
|---|---|---|
| 纯 C++ 代码（`.cpp`、`.cc`、`.cxx`） | **`<cxxx>`** | 标准保证 `std::` 名字；不污染全局命名空间 |
| 会被 C 与 C++ 同时包含的头文件 | `<xxx.h>` | C 编译器只认这个；用 `extern "C"` 包住声明即可（见《04-语法/11-作用域、生存期与链接.md》第 5 节） |
| 现存的老代码 | 保持原样 | 换头文件名会把改动扩散到无关的地方 |

**「用 `<cxxx>` 更干净」的意思是**：`std::` 前缀把「这是标准库的东西」写在了每一次使用处，
读代码的人不必回头翻 `#include` 列表。

---

# 第 3 节 `std::` 里的那些 C 名字

**同一批函数为什么要在 `std` 里再放一份**，答案是泛型代码的统一写法与全局命名空间的拥挤。
这一节同时给出 `std::size_t`、`std::memcpy` 这类名字的来处，
以及混用 `printf` 与 `cout` 时输出的顺序问题。

## 3.1 为什么要有 `std::printf` 这一份

标准完全可以规定「C 的名字就留在全局，C++ 的名字放在 `std`」，那样 `<cstdio>` 就没有必要存在。
它没有那样做，原因有两条。

**第一条：泛型代码需要一个统一的名字。** 写模板时，凡是来自标准库的名字都写成 `std::` 开头，
读者一眼能分清「这是标准库的」还是「这是本项目自己定义的」。
`std::size_t`、`std::memcpy`、`std::printf` 与 `std::vector` 遵守同一条规矩：

`文档`

> "All library entities except `operator new` and `operator delete` are defined
> within the namespace `std` or namespaces nested within namespace `std`."
>
> —— N4659 §20.5.1.1/2

**第二条：全局命名空间是公共资源。** C 的头文件往全局塞了 `printf`、`free`、`time`、`abs` 这些
极常见的短名字。C++ 把它们再放一份到 `std` 里，就是给使用者一个「不去动全局」的选项。

## 3.2 三个典型名字

`C++`

```cpp
/* std_names.cpp    编译：g++ -std=c++17 std_names.cpp -o std_names */
#include <cstddef>       // std::size_t
#include <cstdio>        // std::printf
#include <cstring>       // std::memcpy

int main() {
    std::size_t n = sizeof(double) * 3;            // 来自 <cstddef>
    void *raw = new char[n];                       // 申请一段字节

    std::memcpy(raw, "abcdef", 7);                 // 来自 <cstring>
    std::printf("n = %zu，内容 = %s\n", n,
                static_cast<const char *>(raw));   // 来自 <cstdio>

    delete[] static_cast<char *>(raw);
    return 0;
}
```

`实测数据`
`Text`

```text
n = 24，内容 = abcdef
```

**这三个名字在 C 里分别叫 `size_t`、`memcpy`、`printf`**，功能完全相同，
差别只在写不写 `std::`。C 里没有别的选择，C++ 里两个都能写，因此要挑一个并保持一致。

## 3.3 哪些名字进了 `std`，哪些没进

**宏没有进 `std`。** 宏由预处理器处理，与命名空间无关，所以 `<cstdint>` 里的
`INT32_MAX`、`<cstdio>` 里的 `EOF`、`<cstdlib>` 里的 `EXIT_SUCCESS` 一律是全局的。

`文档`

> "Names which are defined as macros in C shall be defined as macros in the C++
> standard library, even if C grants license for implementation as functions.
> [Note: The names defined as macros in C include the following: `assert`,
> `offsetof`, `setjmp`, `va_arg`, `va_end`, and `va_start`. —end note]"
>
> —— N4659 §20.5.1.2/5

| 名字的种类 | 例子 | 在 `std` 里吗 | 在全局吗 |
|---|---|---|---|
| 函数 | `printf`、`memcpy`、`sqrt` | 有（`<cxxx>` 保证） | 可能有 |
| 类型 | `size_t`、`FILE`、`time_t` | 有（`<cxxx>` 保证） | 可能有 |
| 宏 | `EOF`、`INT32_MAX`、`assert`、`offsetof` | **没有** | 有 |
| 关键字化的东西 | `bool`、`true`、`false`（`<cstdbool>`） | 不存在 | 不存在（C++ 里是关键字） |

**`<stdbool.h>` 那一条需要单独说明**：C 里 `bool` 是宏，C++ 里 `bool` 是关键字，
所以 `<cstdbool>` 与 `<stdbool.h>` 都**不许**定义 `bool`、`true`、`false` 这三个宏：

`文档`

> "The header `<cstdbool>` and the header `<stdbool.h>` shall not define macros
> named `bool`, `true`, or `false`."
>
> —— N4659 §D.4.3/1

## 3.4 写错了会怎样：混用两套输出，顺序错乱

C 的 `printf` 与 C++ 的 `std::cout` 走的是两条路，标准默认把它们**绑在一起**
（`std::ios::sync_with_stdio` 默认为 `true`），所以混着写不会乱序：

`C++`

```cpp
/* mix_sync_on.cpp    编译：g++ -std=c++17 mix_sync_on.cpp -o mix_sync_on
 * 运行：mix_sync_on.exe > out.txt      默认同步（同步是默认值） */
#include <cstdio>
#include <iostream>

int main() {
    std::cout << "cout 第 1 行\n";
    std::printf("printf 第 1 行\n");
    std::cout << "cout 第 2 行\n";
    std::printf("printf 第 2 行\n");
    return 0;
}
```

`实测数据`
`Text`

```text
cout 第 1 行
printf 第 1 行
cout 第 2 行
printf 第 2 行
```

**一旦关掉同步，两条路各走各的缓冲，顺序就不再由程序决定**：

`C++`

```cpp
/* mix_sync_off.cpp    编译：g++ -std=c++17 mix_sync_off.cpp -o mix_sync_off
 * 运行：mix_sync_off.exe > out.txt     关掉 C 与 C++ 的同步 */
#include <cstdio>
#include <iostream>

int main() {
    std::ios::sync_with_stdio(false);      // 关掉同步：两套缓冲各自为政
    std::cout << "cout 第 1 行\n";
    std::printf("printf 第 1 行\n");
    std::cout << "cout 第 2 行\n";
    std::printf("printf 第 2 行\n");
    return 0;
}
```

`实测数据`
`Text`

```text
cout 第 1 行
cout 第 2 行
printf 第 1 行
printf 第 2 行
```

**两个程序只差一行 `sync_with_stdio(false)`，输出顺序完全不同。**
这段输出是重定向到文件时看到的（管道同理）；直接打在控制台上时，
行缓冲的时机不同，现象可能不一样。

`文档`

> "If any input or output operation has occurred using the standard streams prior
> to the call, the effect is implementation-defined. Otherwise, called with a
> `false` argument, it allows the standard streams to operate independently of
> the standard C streams."
>
> —— N4659 §30.5.3.4/2

> [!CAUTION]
> **同一个程序里不要同时用 `printf` 与 `cout` 输出同一份结果。**
> 顺序、缓冲、线程安全三件事都需要重新考虑一遍。
> 关掉同步换来的那点速度见第 7.1 小节，量级远小于一次调试的时间。

---

# 第 4 节 与 STL 的边界

**本板块不装容器、迭代器与算法**，这不是遗漏，而是分工。这一节说明那条线画在哪里，
以及 `std::string` 为什么是个例外。

## 4.1 一条按用途划的线

`07-标准库` 讲的是**「怎么读进来、写出去、算一下、记个时间」**。
凡是「把一堆对象按某种结构装起来、再按某种顺序过一遍」的内容，都不在这个板块。

| 内容 | 归哪 | 理由 |
|---|---|---|
| `printf`、`iostream`、`fstream` | **本板块** A-01、B-01 | 输入输出的接口 |
| `std::string`、`string_view` | **本板块** B-02 | 处理文本的工具 |
| `std::vector`、`std::map`、`std::sort`、迭代器 | **`08-高阶数据结构`** | 它们是数据结构与算法 |
| CRT 的实现、ABI、内存布局、系统调用 | **`06-更底层`** | 属于实现细节 |
| 类、模板、lambda、异常这些**语言机制** | `04-语法`、`05-类与面向对象` | 已经讲过，这里只讲怎么用现成件 |

**`std::string` 是个例外，它技术上就是容器**。把它留在本板块的理由是用途：
它是处理文本的入口，与输入输出放在一起讲更顺；而且它有一堆非容器的用法
（`c_str`、数字互转、编码），分散到 `08` 去讲反而割裂。

## 4.2 本段各章的位置

| 章 | 内容 | 与 C 的对应 |
|---|---|---|
| **B-00** | C++ 标准库与 C 的关系 | 本小节 |
| **B-01** | `iostream` 家族、`<iomanip>`、`<sstream>`、`<fstream>` | A-01 的 `printf`、`fopen` |
| **B-02** | `std::string` 与 `string_view`、数字互转、编码 | A-02 的 `char[]` 与 `<string.h>` |
| **B-03** | 智能指针 | A-05 的 `malloc`/`free`、`05-06` 的 RAII |
| **B-04** | `std::function`、`bind`、`invoke` | C 的函数指针 |
| **B-05** | `<limits>`、`<cmath>`、`<random>`、`<numeric>` | A-03 的 `<math.h>`、`rand` |
| **B-06** | `<chrono>` | A-04 的 `time`、`clock` |
| **B-07** | `<filesystem>` | A-01 的 `fopen` 与平台 API |
| **B-08、B-09** | `pair`/`tuple`/`optional`/`variant`/`any`、类型特征与概念 | C 里只能靠手写或宏 |
| **B-10** | 分配器、对齐、`<mutex>`、`<atomic>` 的接口 | 无 |
| **B-11** | 头文件命名规则、实现差异、什么时候不该用标准库 | 收尾 |

**完整的阅读顺序与配套件清单见《07-标准库/README.md》第三节与第六节。**

---

# 第 5 节 版本：本教材的基准是 C++17

**标准库是逐版长大的，同一段代码换个开关就可能编译不过。** 这一节给出各版本的增补清单、
两组实际编译得到的对照，以及标注版本的写法。

## 5.1 为什么必须标版本

**标准库是逐版长大的。** 同一段代码，在 `-std=c++14` 下编译不过，在 `-std=c++17` 下就没问题，
差别只在开关。**本教材的基准是 C++17 与 C23**，凡是超出这个基准的东西，一律写明版本。

| 版本 | 给标准库加的东西（本段会用到的） |
|---|---|
| **C++11** | 智能指针（`<memory>`）、`<chrono>`、`<random>`、`<thread>`、`<regex>`、`std::function`、`std::string` 的移动语义、`to_string` / `stoi` 一族、`std::array`、`<tuple>` |
| **C++14** | `std::make_unique`、`std::quoted`、`<shared_mutex>`、`chrono` 的字面量后缀（`1s`、`100ms`） |
| **C++17** | `<filesystem>`、`<charconv>`（`from_chars`/`to_chars`）、`string_view`、`optional`/`variant`/`any`、`std::byte`、`std::apply`、`as_const`、`clamp`、`std::size`/`data`/`empty` 自由函数、`scoped_lock` |
| **C++20** | `<format>`、`<span>`、`<ranges>`、`<concepts>`、`<numbers>`、`chrono` 的日历与时区、`std::jthread`、`bit_cast`、`string` 与 `string_view` 的 `starts_with`/`ends_with`、`map::contains`、`source_location` |

**写代码时的规矩**：用 C++17 基准里有的东西；需要 C++20 的设施时，
在注释或文档里写一句「需要 C++20」，并给出编译开关。

## 5.2 同一段代码在两个开关下的差别

下面这个程序用了四样 C++17 才有的东西：`std::string_view`、`std::optional`、
结构化绑定、`if constexpr`。

`C++`

```cpp
/* ver17.cpp    编译：g++ -std=c++17 ver17.cpp -o ver17 */
#include <iostream>
#include <optional>
#include <string_view>
#include <tuple>

int main() {
    const std::string_view v = "hello";
    const std::optional<int> o = 42;
    const auto [a, b] = std::pair<int, int>{1, 2};      // 结构化绑定
    if constexpr (sizeof(void *) == 8) {                // if constexpr
        std::cout << "64 位\n";
    }
    std::cout << v << " " << *o << " " << a + b << "\n";
    return 0;
}
```

换 `-std=c++14` 编译，第一处就过不去：

`实测数据`
`Text`

```text
ver17.cpp:8:16: error: 'string_view' in namespace 'std' does not name a type
    8 |     const std::string_view v = "hello";
      |                ^~~~~~~~~~~
ver17.cpp:8:11: note: 'std::string_view' is only available from C++17 onwards
```

**编译器的提示直接给出了版本**（`only available from C++17 onwards`），
这是判断「某个名字属于哪一版」最快的手段。
用 `-std=c++17` 编译同一个文件则通过：

`实测数据`
`Text`

```text
64 位
hello 42 3
```

反过来，用了 C++20 的东西在 C++17 下也会被拦下：

`C++`

```cpp
/* ver20.cpp    编译：g++ -std=c++20 ver20.cpp -o ver20 */
#include <iostream>
#include <span>
#include <string_view>

static int sum(std::span<const int> s) {          // C++20 的 span
    int total = 0;
    for (int x : s) total += x;
    return total;
}

int main() {
    const std::string_view v = "hello";
    const int data[3] = {1, 2, 3};
    std::cout << std::boolalpha << v.starts_with("he") << " "   // C++20
              << sum(data) << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
ver20.cpp:6:21: error: 'span' is not a member of 'std'
    6 | static int sum(std::span<const int> s) {          // C++20 的 span
      |                     ^~~~
ver20.cpp:6:21: note: 'std::span' is only available from C++20 onwards
```

`-std=c++20` 下运行结果：

`实测数据`
`Text`

```text
true 6
```

## 5.3 版本标注的写法

| 场合 | 写法 |
|---|---|
| 正文里提到 C++20 的设施 | 写「C++20 起」或「C++20」，再补一句编译开关 |
| 代码块需要更高标准 | 首行的编译命令写实际需要的开关（如 `g++ -std=c++20`） |
| 表格里的清单 | 单列一栏标版本 |
| 本教材的默认 | 不标版本的就是 C++17 基准内 |

---

# 第 6 节 怎么查

**标准、手册、编译器自己的头文件，三条路径各答各的问题，不能互相替代。**
本节给出三条路径的分工与章节号的读法，并以一次完整查证作为示例。

## 6.1 三条路径

| 路径 | 适合查什么 | 代价 |
|---|---|---|
| **cppreference**（<https://en.cppreference.com/w/cpp>） | 某个名字在哪个头文件、函数签名、复杂度、哪个版本加的 | 网络；偶尔滞后于最新草案 |
| **标准草案 N4659**（<https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4659.pdf>） | 「标准到底怎么规定的」——保证、未指定行为、失效规则 | 一千多页；需要会读章节号 |
| **编译器自己的头文件** | 「这个实现到底做了什么」——最权威的实现证据 | 依赖具体实现，不能当标准用 |

**三条路径各有各的用处，不能互相替代。** 想知道「`<stdio.h>` 到底给不给 `std::printf`」，
标准答「未指定」，必须去看实现；想知道「重新分配之后旧指针还能不能用」，
标准答得斩钉截铁，看实现反而会被带偏。

## 6.2 标准里的章节号怎么读

N4659 的每一节都有一个方括号标签，写引用时用它比页码可靠（页码在正式 ISO 版里不一样）：

| 标签 | 内容 |
|---|---|
| `[headers]`（§20.5.1.2） | 头文件总表与 C 头文件的关系 |
| `[depr.c.headers]`（§D.5） | `<xxx.h>` 形式的 C 头文件 |
| `[string.require]`（§24.3.2.1） | `basic_string` 的通用要求，含失效规则 |
| `[string.accessors]`（§24.3.2.7.1） | `c_str()` 与 `data()` |
| `[string.conversions]`（§24.3.4） | `stoi`、`stod` 一族 |
| `[string.view]`（§24.4） | `string_view` |
| `[narrow.stream.objects]`（§30.4.3） | `cin`、`cout`、`cerr`、`clog` 四个对象 |
| `[iostate.flags]`（§30.5.5.4） | `good`/`eof`/`fail`/`bad` 与 `clear`、`setstate` |
| `[std.manip]`（§30.7.6） | `<iomanip>` 的 `setw`、`setfill`、`setprecision` 等 |

**引用时写「N4659 §24.3.2.1/4」**：`§` 后面是章节号，斜杠后面是**段号**。
上表里的 `[string.require]` 第 4 段就是那条失效规则，所以写成 §24.3.2.1/4。

## 6.3 一次完整的查证：`c_str()` 的指针什么时候失效

这是 B-02 会用到的结论，此处按四步完整查证一遍，因为流程本身会反复用到。

**第一步，查标准。** `[string.accessors]` 给出的是指针的构造方式：

`文档`

> "Returns: A pointer `p` such that `p + i == &operator[](i)` for each `i` in
> `[0, size()]`. Complexity: Constant time. Requires: The program shall not
> alter any of the values stored in the character array."
>
> —— N4659 §24.3.2.7.1/1

**`i` 取到 `size()` 这一点值得注意**：`p + size()` 就是那个结尾的空字符，所以 `c_str()` 保证以空字符结尾。

**第二步，查失效规则。** 在 `[string.require]` 里：

`文档`

> "References, pointers, and iterators referring to the elements of a
> `basic_string` sequence may be invalidated by the following uses of that
> `basic_string` object: —(4.1) as an argument to any standard library function
> taking a reference to non-const `basic_string` as an argument. —(4.2) Calling
> non-const member functions, except `operator[]`, `at`, `data`, `front`,
> `back`, `begin`, `rbegin`, `end`, and `rend`."
>
> —— N4659 §24.3.2.1/4

**第三步，读成一句话**：**只要调用非常量成员函数（除了列出的那几个观察函数），
之前拿到的指针就可能失效**。`s += "x"` 是 `operator+=`，非常量成员函数，所以指针失效。

**第四步，跑一遍。** 用 AddressSanitizer 跑一遍，观察它实际报出什么（见《07-标准库/B-02-std-string 与 string_view.md》第 5 节）。

**四步下来，结论是「文档 + 实测」双重支撑的**，而不是记忆里的印象。

## 6.4 一次查头文件边界的例子

`to_chars` 在哪一个头文件里，从名字看不出来。查 cppreference 会给出 `<charconv>`，
但更快的方法是直接编译一次：

`C++`

```cpp
/* probe_tochars.cpp    编译：g++ -std=c++17 probe_tochars.cpp -o probe_tochars （失败） */
#include <utility>       // 猜错了：to_chars 不在 <utility> 里

int main() {
    char buf[8] = {};
    std::to_chars(buf, buf + 8, 42);
    return 0;
}
```

`实测数据`
`Text`

```text
probe_tochars.cpp:5:10: error: 'to_chars' is not a member of 'std'
    5 |     std::to_chars(buf, buf + 8, 42);
      |          ^~~~~~~~
```

**报错说的只是「没有这个名字」，没说是哪个头文件。** 换成 `#include <charconv>` 就通过——
`<charconv>` 正是 `[utility.to.chars]`（§23.2.8）与 `[utility.from.chars]`（§23.2.9）两个小节
所描述的那批函数所在的头文件。

> [!TIP]
> **查头文件归属的固定动作**：先用最可能的名字编译一次，看报错；
> 再查 cppreference 的「Defined in header」一行确认。
> 编译器的报错有时会直接给出建议（本机 GCC 15 在漏掉 `<iomanip>` 时会提示
> "this is probably fixable by adding '#include <iomanip>'"）。

---

# 第 7 节 三个设计问题

B 段后面各章会反复遇到三个取舍。**这里只提出问题**，
答案在对应的章节里，顺着问题读过去比先背结论有用。

## 7.1 `iostream` 比 `printf` 慢吗

**问题**：`std::cout` 要把格式与类型的信息带到运行期（虚函数、locale、
`num_put` 面），而 `printf` 的格式串在编译期就定了。
可是实测出来的数字并不总是偏向 `printf`——它取决于写到哪里、用不用 `std::endl`、
有没有开同步。**那么「谁快」这个问题到底该怎么问，才不至于得出一个错误的结论。**

**答案在**《07-标准库/B-01-输入输出：iostream.md》第 8 节。
那里有同一台机器上的三组数字与它们成立的条件。

## 7.2 `std::string` 为什么要 SSO

**问题**：小字符串优化（SSO）把短内容直接放在对象内部，避免一次堆分配。
代价是 `std::string` 对象变大（本机 32 字节），并且**搬移一个短字符串与搬移一个长字符串
的行为不一样**。**这个代价换来了什么，什么时候该在意它。**

**答案在**《07-标准库/B-02-std-string 与 string_view.md》第 2 节，
那里实测了本机 libstdc++ 的阈值（15 字节）与搬移时的指针变化。

## 7.3 智能指针为什么分两种

**问题**：`unique_ptr` 与 `shared_ptr` 做的事情有重叠——都能自动释放。
可是一个的 `sizeof` 是 8 字节，另一个是 16 字节；一个可以当成零开销的包装，
另一个要维护引用计数。**为什么不合成一个「智能指针」，让编译器自己去判断该用哪种。**

**答案在**《07-标准库/B-03-智能指针的用法.md》。
`05-类与面向对象/06-RAII 与资源管理.md` 讲了「是什么」，那一章讲「怎么用」。

---

# 速查表

| 件 | 一句话用途 | 典型坑 |
|---|---|---|
| `<cxxx>`（如 `<cstdio>`） | C 标准库设施的 C++ 形式，保证 `std::` 名字 | 以为 C 里也能用；C 里只有 `<xxx.h>` |
| `<xxx.h>`（如 `<stdio.h>`） | C 兼容形式，保证全局名字（附录 D 的弃用清单） | 以为它给 `std::printf`；实测两套实现都不给 |
| `std::printf` 等 C 名字 | 与全局同名函数是同一个实体 | 依赖它等于依赖实现的选择 |
| `std::ios::sync_with_stdio(false)` | 关掉 C 与 C++ 输出的同步，换取速度 | 与 `printf` 混用时输出顺序不确定 |
| `-std=c++17` | 本教材的基准开关 | 用了 C++20 的设施却忘了改开关 |
| cppreference | 查名字归属、签名、版本 | 把示例当标准原文引用 |
| N4659 | 查标准怎么规定 | 混用页码与章节号 |
| `<MinGW>/lib/gcc/.../include/c++/` | 查实现到底做了什么 | 把某个实现的行为当成标准保证 |

---

# 术语表

| 词 | 含义 |
|---|---|
| **C++ 标准库** | 标准规定的那批头文件、类型、函数与宏；C++ 标准的一部分，不是第三方库 |
| **C 标准库** | C 标准规定的那批设施；C++ 把它们整份继承了过来 |
| **`<cxxx>`** | C 标准库设施的 C++ 形式头文件，如 `<cstdio>`、`<cstring>` |
| **`<xxx.h>`** | C 兼容形式头文件，位于 N4659 附录 D.5 的弃用清单 |
| **未指定（unspecified）** | 标准允许实现自己决定的行为，写代码时不能依赖 |
| **实现定义（implementation-defined）** | 实现必须选一个行为并写进文档，代码同样不能依赖 |
| **命名空间污染** | 头文件往全局命名空间塞进大量短名字，增加重名风险 |
| **SSO** | 小字符串优化，`std::string` 把短内容放在对象内部 |

---

# 附录 A 复现本章节实测

**环境**：Windows 11，`g++` 15.2.0（MinGW-w64），MSVC `cl` 19.44（VS 生成工具 2022）。
**C++ 用 `g++ -std=c++17`，C 用 `gcc -std=c23`。**

**A.1 `<cstdio>` 与 `<stdio.h>` 的名字归属**

`Bash`

```bash
g++ -std=c++17 cxx_cstdio.cpp -o cxx_cstdio && ./cxx_cstdio.exe
g++ -std=c++17 cxx_stdioh.cpp  -o cxx_stdioh.exe      # 期望失败
g++ -std=c++17 cxx_mix.cpp     -o cxx_mix.exe && ./cxx_mix.exe
cmd /c '"C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cl /nologo /utf-8 /std:c++17 /EHsc cxx_stdioh.cpp'
```

**A.2 C 里没有 `<cstdio>`**

`Bash`

```bash
gcc -std=c23 c_cstdio.c -o c_cstdio.exe               # 期望失败
```

**A.3 混用两套输出的顺序**

`Bash`

```bash
g++ -std=c++17 mix_sync_on.cpp  -o mix_sync_on.exe  && cmd /c "mix_sync_on.exe  > out_on.txt"
g++ -std=c++17 mix_sync_off.cpp -o mix_sync_off.exe && cmd /c "mix_sync_off.exe > out_off.txt"
```

**A.4 版本开关**

`Bash`

```bash
g++ -std=c++14 ver17.cpp -o ver17_14.exe              # 期望失败
g++ -std=c++17 ver17.cpp -o ver17.exe && ./ver17.exe
g++ -std=c++17 ver20.cpp -o ver20_17.exe              # 期望失败
g++ -std=c++20 ver20.cpp -o ver20.exe && ./ver20.exe
```

**A.5 头文件归属**

`Bash`

```bash
g++ -std=c++17 probe_tochars.cpp -o probe_tochars.exe # 期望失败
```

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《07-标准库/README.md》 | **本板块入口**：A、B 两段的分工、阅读顺序、配套件清单 |
| 《07-标准库/A-00-导读：C 标准库.md》 | **前置**：C 标准库是怎么组织的 |
| 《07-标准库/A-01-输入输出：stdio.md》第 1 节 | **对照**：`printf`、流与缓冲那一半 |
| 《07-标准库/A-02-字符串与内存：string.h.md》 | **对照**：`char[]` 与 `<string.h>` |
| 《07-标准库/B-01-输入输出：iostream.md》 | **后续**：`iostream` 家族 |
| 《07-标准库/B-02-std-string 与 string_view.md》 | **后续**：`std::string` 与 `string_view` |
| 《07-标准库/B-03-智能指针的用法.md》 | **后续**：智能指针怎么选 |
| 《04-语法/11-作用域、生存期与链接.md》第 5 节 | 前置：`extern "C"` 与链接属性 |
| 《05-类与面向对象/01-命名空间与 using.md》第 2 节 | 前置：命名空间与 `::` |
| 《05-类与面向对象/06-RAII 与资源管理.md》 | 相关：RAII 与资源释放 |
| `08-高阶数据结构` 板块 | 容器、迭代器、算法 |
| `06-更底层` 板块 | CRT 实现、ABI、内存布局 |

**配套示例见 [`B-examples/07-standard-library/01-c-stdlib-toolbox/`](../B-examples/07-standard-library/01-c-stdlib-toolbox/) 与 [`02-cpp-io-report`](../B-examples/07-standard-library/02-cpp-io-report/)，配套练习见 [`C-templates/07-standard-library/01-c-stdlib-toolbox/`](../C-templates/07-standard-library/01-c-stdlib-toolbox/) 与 [`02-cpp-io-format`](../C-templates/07-standard-library/02-cpp-io-format/)。**
