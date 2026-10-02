# 把标准库用对

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**前面十六篇把标准库里常用的那批东西过了一遍，这一篇回答剩下的三个问题：名字住在哪、实现差在哪、什么时候不该用它。**

标准库是标准的一部分，但**标准只管接口，不管实现**。同一个 `std::sort`，libstdc++ 与 MSVC STL 编出来的代码不同、跑出来的时间不同；同一个 `std::string`，两边的对象大小也可能不同。**写可移植代码的人必须知道哪些东西是标准保证的，哪些只是「本机恰好如此」。** 这一篇把两套实现摆在一起对照。

还有一类问题出在边界上：库的实现在一个模块里，调用方在另一个模块里，而**两套标准库的二进制接口互不兼容**；又或者目标平台关掉了异常与 RTTI，一批标准库设施从此不能碰。这些都不是「用法」问题，而是「该不该用」的问题——**它们通常在项目后期才暴露，代价也最高。** 最后是两页可以贴在手边的表：**一页把 A 段与 B 段全部头文件与代表件列成索引**，一页是十二条自查问题。至此，本板块就该合上了。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整源码与复现命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<VS>`）的含义见《README.md》。

# 本章节定位

| 问题 | 在哪一节 |
|---|---|
| `<cxxx>` 与 `<xxx.h>` 有什么区别、名字住在哪个命名空间 | **第 1 节** |
| 哪几个头文件是「兼容头」 | **第 1 节** |
| 同一段代码在两套标准库上的大小与耗时差多少 | **第 2 节** |
| `std::regex` 能不能用在正式代码里 | **第 2 节** |
| 哪些场合宁可不用标准库 | **第 3 节** |
| 本板块十六篇分别讲了哪些头文件 | **第 4 节** |
| 交代码之前该问自己哪十二个问题 | **第 5 节** |

| 前置知识 | 在哪 |
|---|---|
| 命名空间与 `using` | 《05-类与面向对象/01-命名空间与 using.md》第 2 节 |
| 编译与链接、头文件与库的区别 | 《01-编译器/01-编译与链接.md》第 3 节 |
| ABI 与符号修饰 | 《03-构建工具链/04-符号与调试信息.md》第 2 节 |
| 异常与 RTTI 的语言规则 | 《04-语法/13-异常.md》第 1 节 |
| 本板块 A 段与 B 段的读法 | 《07-标准库/A-00-导读：C 标准库.md》章节、《07-标准库/B-00-导读：C++ 标准库与 C 的关系.md》章节 |

| 相邻章节 | 关系 |
|---|---|
| 《07-标准库/B-00-导读：C++ 标准库与 C 的关系.md》第 2 节 | **前置**：`<cstdio>` 与 `<stdio.h>` 的第一轮对照，本节把它说清 |
| 《07-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 2 节 | **前置**：`tuple` 的实现差异，第 2 节给出两套实现的 `sizeof` 对照 |
| 《07-标准库/B-10-内存与并发的基础设施.md》 | **前置**：本板块最后一篇正课，本章不再重复它的内容 |
| 【待补：10-并发与并行/】 | 相关：`std::thread` 与数据结构部分不属本板块 |

---

# 第 1 节 头文件命名规则

## 1.1 `<cxxx>` 与 `<xxx.h>`：名字住在哪

**C++ 标准库里有两种形式的 C 头文件**：`<cstdio>` 这样的 `c` 前缀形式，与 `<stdio.h>` 这样的兼容形式。**两者的内容相同，名字的归属不同。**

`文档`

> "The header <cstdlib> assuredly provides its declarations and definitions within the namespace std. It may
> also provide these names within the global namespace. The header <stdlib.h> assuredly provides the same
> declarations and definitions within the global namespace, much as in the C Standard. It may also provide
> these names within the namespace std."
>
> —— N4659 §D.5/4

**这段用 `cstdlib` 与 `stdlib.h` 举例，把四种可能说清了**：`<cstdlib>` **保证**名字在 `std` 里，
可能也在全局；`<stdlib.h>` **保证**名字在全局，可能也在 `std`。因此**能依赖的写法只有一种**——
包含 `<cxxx>`、写 `std::` 前缀；反过来（包含 `<xxx.h>` 却写 `std::`）两套实现都不保证。

`C++`

```cpp
/* names_h_std.cpp    编译：g++ -std=c++17 names_h_std.cpp -o names_h_std （失败） */
#include <stdio.h>
int main() { std::printf("用 <stdio.h>、写 std::printf\n"); return 0; }
```

`实测数据`
`Text`

```text
error: 'printf' is not a member of 'std'; did you mean 'printf'?
```

**换成 `<cstdio>` 即可通过编译**：

`C++`

```cpp
/* names_cxx_std.cpp    编译：g++ -std=c++17 names_cxx_std.cpp -o names_cxx_std */
#include <cstdio>
int main() { std::printf("用 <cstdio>、写 std::printf\n"); return 0; }
```

四种组合各编一次，两套标准库的结果如下。

`实测数据`

| 包含 | 写法 | libstdc++ 15.2.0 | MSVC STL（cl 19.44） |
|---|---|---|---|
| `<stdio.h>` | `printf(...)` | 通过 | 通过 |
| `<stdio.h>` | `std::printf(...)` | **通不过编译** | **通不过编译** |
| `<cstdio>` | `printf(...)` | 通过 | 通过 |
| `<cstdio>` | `std::printf(...)` | 通过 | 通过 |

**两套实现在这一格上完全一致**：`<xxx.h>` 都不往 `std` 里放名字，`<cxxx>` 两边都放。
**这不是巧合**——标准对 `<cxxx>` 有硬性要求，对 `<xxx.h>` 只留下「是否」的空间，两套实现都选了最省的做法。
反向的一格（`<cstdio>` 之后写不加前缀的 `printf`）两边都能过，但**标准同样不保证**：
`<cxxx>` 允许把这些名字也放进全局命名空间，却没说必须。

> [!IMPORTANT]
> **一条规则就够：一律包含 `<cxxx>`，一律写 `std::` 前缀。**
> 这样写在 libstdc++、MSVC STL、libc++ 上都成立，也不会被别人的宏撞上。

## 1.2 哪些是「兼容头」

**`<xxx.h>` 那一批在 C++ 里位置特殊**：它们不在正文里，而是收在标准的附录 D（兼容性特性），
因此**标准把它们标为弃用**，理由同样是「名字进了全局命名空间」。

`文档`

> "For compatibility with the C standard library, the C++ standard library provides the C headers shown in
> Table 141."
>
> "Every other C header, each of which has a name of the form name.h, behaves as if each name placed in the
> standard library namespace by the corresponding cname header is placed within the global namespace scope,
> except for the functions described in 29.9.5, the declaration of std::byte (21.2.1), and the functions and
> function templates described in 21.2.5. It is unspecified whether these names are first declared or defined
> within namespace scope (6.3.6) of the namespace std and are then injected into the global namespace scope
> by explicit using-declarations (10.3.3)."
>
> —— N4659 §D.5/1、§D.5/3

**这段把两件事一起说了**：这批头文件收在附录 D 里，**而收进附录 D 就意味着标准把它们标为弃用**；
「`<xxx.h>` 是否也把名字放进 `std`」是**未指定**的，实现怎么选都合规。

**它们一共有 26 个**（N4659 表 141），每一个都有一个 `c` 前缀的 C++ 对应物。

`实测数据`

| C 形式 | C++ 形式 | 主要代表件 |
|---|---|---|
| `<assert.h>` | `<cassert>` | `assert` |
| `<complex.h>` | `<ccomplex>` | `complex`、`I`（C++17 起弃用） |
| `<ctype.h>` | `<cctype>` | `isalpha`、`isdigit`、`tolower` |
| `<errno.h>` | `<cerrno>` | `errno`、`EINVAL` |
| `<fenv.h>` | `<cfenv>` | `fenv_t`、`fegetround` |
| `<float.h>` | `<cfloat>` | `FLT_MAX`、`DBL_EPSILON` |
| `<inttypes.h>` | `<cinttypes>` | `PRId64` 一族、`imaxdiv` |
| `<iso646.h>` | `<ciso646>` | `and`、`or`、`not` 这些替代记号 |
| `<limits.h>` | `<climits>` | `INT_MAX`、`CHAR_BIT` |
| `<locale.h>` | `<clocale>` | `setlocale`、`localeconv` |
| `<math.h>` | `<cmath>` | `sqrt`、`pow`、`fabs` |
| `<setjmp.h>` | `<csetjmp>` | `setjmp`、`longjmp` |
| `<signal.h>` | `<csignal>` | `signal`、`raise` |
| `<stdalign.h>` | `<cstdalign>` | `alignas`、`alignof` 宏（C++17 起弃用） |
| `<stdarg.h>` | `<cstdarg>` | `va_list`、`va_start` |
| `<stdbool.h>` | `<cstdbool>` | `bool`、`true`、`false` 宏（C++17 起弃用） |
| `<stddef.h>` | `<cstddef>` | `size_t`、`nullptr_t`、`offsetof` |
| `<stdint.h>` | `<cstdint>` | `int64_t`、`uintptr_t`、`INT64_MAX` |
| `<stdio.h>` | `<cstdio>` | `printf`、`fopen`、`FILE` |
| `<stdlib.h>` | `<cstdlib>` | `malloc`、`qsort`、`atoi` |
| `<string.h>` | `<cstring>` | `strlen`、`memcpy`、`strcmp` |
| `<tgmath.h>` | `<ctgmath>` | 类型泛型数学宏（C++17 起弃用） |
| `<time.h>` | `<ctime>` | `time`、`clock`、`tm` |
| `<uchar.h>` | `<cuchar>` | `char16_t`、`char32_t` 与 `mbrtoc16` |
| `<wchar.h>` | `<cwchar>` | `wchar_t` 一族 |
| `<wctype.h>` | `<cwctype>` | `iswalpha` 一族 |

**表里 26 行，一行不缺。** 其中 5 个（`<ccomplex>`、`<ciso646>`、`<cstdalign>`、`<cstdbool>`、`<ctgmath>`）
在 C++17 已经被进一步弃用，**它们留在标准里只是为了让老代码能通过编译**。

**关键在于表里没有谁**：C11 的 `<stdatomic.h>`、`<threads.h>` 与 C23 的 `<stdbit.h>` 都不在里面——
**C++ 用 `<atomic>`、`<thread>` 这套自己的设施代替了前两个**（对照见
《07-标准库/B-10-内存与并发的基础设施.md》第 5 节），因此也就不需要 `c` 前缀的兼容形式。
**表 141 收的是「C 有、C++ 也照收」的那批头文件，不是 C 语言全部头文件。**

> [!TIP]
> **兼容头不是不能用，而是不必用。** 接手一份老代码时，看到 `<string.h>` 知道它就是 `<cstring>` 即可；
> 写新代码时包含 `<cstring>`，把 `strlen` 写成 `std::strlen`，
> 将来把这段代码挪进命名空间或改成模板都不会出问题。

## 1.3 一条规则，两个例外

**规则是「包含 `<cxxx>`、写 `std::`」，例外只有两个。**

**例外一：`<stddef.h>` 与 `<cstddef>` 的内容差别最大。** `std::size_t`、`std::nullptr_t` 在 `<cstddef>` 里，
而 `offsetof`、`NULL` 这类宏只看全局那一份——**宏没有命名空间**，`std::` 前缀对它没有意义。
`<cstddef>` 会把它们一并带进来，因此包含 `<cstddef>` 之后 `NULL`、`offsetof` 照样能用。

**例外二：C 兼容头里的宏比函数多。** `<limits.h>` 的 `INT_MAX`、`<float.h>` 的 `FLT_MAX`、
`<errno.h>` 的 `errno`、`<assert.h>` 的 `assert` 全是宏，**它们的名字永远在全局**，
写成 `std::INT_MAX` 反而通不过编译。判断办法很直接：**取决于它有没有括号**——
`sqrt(x)` 是函数，写 `std::sqrt`；`INT_MAX` 是宏，直接写。

`C++`

```cpp
/* cxxx_macro.cpp    编译：g++ -std=c++17 cxxx_macro.cpp -o cxxx_macro */
#include <cassert>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>

int main() {
    std::printf("std::sqrt(2.0) = %.6f\n", std::sqrt(2.0));   /* 函数：走 std:: */
    std::printf("INT_MAX = %d\n", INT_MAX);                  /* 宏：只能走全局 */
    std::printf("sizeof(std::size_t) = %zu\n", sizeof(std::size_t));
    void *p = NULL;                                          /* 宏，来自 <cstddef> */
    struct S { int a; double b; };
    std::printf("offsetof(S, b) = %zu，p 是空指针：%d\n",
                offsetof(S, b), (int)(p == nullptr));
    errno = 0;                                               /* 宏，来自 <cerrno> */
    assert(errno == 0);
    std::printf("全部通过\n");
    return 0;
}
```

`实测数据`
`Text`

```text
std::sqrt(2.0) = 1.414214
INT_MAX = 2147483647
sizeof(std::size_t) = 8
offsetof(S, b) = 8，p 是空指针：1
全部通过
```

**最后一行 `全部通过` 值得注意**：`assert(errno == 0)` 这一行只有在 `NDEBUG` 没有定义时才真的检查。
发布版本里它会被整个去掉，**不要把有副作用的表达式写进 `assert`**——
那是《07-标准库/A-05-工具与其它：stdlib 与杂项.md》第 5 节讲过的那条坑。

---

# 第 2 节 实现差异：libstdc++ 与 MSVC STL

## 2.1 标准没有规定的那些事

**标准规定接口与可观察行为，不规定实现。** 于是同一份代码在不同标准库上会给出不同的数字。
最容易碰上的有三类：**对象大小**（`sizeof` 是编译期常量，随实现变）、
**同一个算法的常数**（`std::sort`、`std::regex` 的实现策略不同）、
以及**同一件小事的代价**（格式化、查找、分配）。

**量这类差异的办法只有一条：同一份源码，两套工具链各编一遍。** 命令如下。

`Bash`

```bash
g++ -std=c++17 -O2 sizes.cpp -o sizes_gcc.exe                 # libstdc++，MinGW-w64 g++ 15.2.0

cmd /c '"<VS>\VC\Auxiliary\Build\vcvars64.bat" >nul && cl /nologo /utf-8 /std:c++17 /EHsc /O2 sizes.cpp'   # MSVC STL，cl 19.44
```

**`<VS>` 是 Visual Studio 或生成工具的安装目录。** 命令里的 `/utf-8` 让源码与执行字符集都按 UTF-8 解释，
**不写它时中文输出会乱码**（《01-编译器/03-嵌入式与交叉编译.md》附录里有这对开关的完整说明）。
两套 `cl` 版本编出的产物略有不同，**同一张表里的数字必须来自同一套**。

## 2.2 对象大小对照

`C++`

```cpp
/* sizes.cpp    编译：g++ -std=c++17 -O2 sizes.cpp -o sizes ；MSVC 见上一小节 */
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <vector>

int main() {
    std::printf("sizeof(std::string)                = %zu\n", sizeof(std::string));
    std::printf("sizeof(std::wstring)               = %zu\n", sizeof(std::wstring));
    std::printf("sizeof(std::string_view)           = %zu\n", sizeof(std::string_view));
    std::printf("sizeof(std::vector<int>)           = %zu\n", sizeof(std::vector<int>));
    std::printf("sizeof(std::map<int, int>)         = %zu\n", sizeof(std::map<int, int>));
    std::printf("sizeof(std::shared_ptr<int>)       = %zu\n", sizeof(std::shared_ptr<int>));
    std::printf("sizeof(std::unique_ptr<int>)       = %zu\n", sizeof(std::unique_ptr<int>));
    std::printf("sizeof(std::function<void()>)      = %zu\n", sizeof(std::function<void()>));
    std::printf("sizeof(std::function<int(int)>)    = %zu\n", sizeof(std::function<int(int)>));
    std::printf("sizeof(std::optional<int>)         = %zu\n", sizeof(std::optional<int>));
    std::printf("sizeof(std::optional<std::string>) = %zu\n", sizeof(std::optional<std::string>));
    std::printf("sizeof(std::tuple<char, int>)      = %zu\n", sizeof(std::tuple<char, int>));
    std::printf("sizeof(std::error_code)            = %zu\n", sizeof(std::error_code));
    std::printf("sizeof(std::regex)                 = %zu\n", sizeof(std::regex));
    std::string s;                        /* 空串的容量就是 SSO 缓冲区的大小 */
    std::printf("空 std::string 的 capacity          = %zu\n", s.capacity());
    std::printf("空 std::vector 的 capacity          = %zu\n", std::vector<int>().capacity());
    return 0;
}
```

`实测数据`

| 表达式 | libstdc++ 15.2.0 | MSVC STL（cl 19.44） | 差多少 |
|---|---|---|---|
| `sizeof(std::string)` | 32 | 32 | 相同 |
| `sizeof(std::wstring)` | 32 | 32 | 相同 |
| `sizeof(std::string_view)` | 16 | 16 | 相同 |
| `sizeof(std::vector<int>)` | 24 | 24 | 相同 |
| `sizeof(std::map<int, int>)` | **48** | **16** | 三倍 |
| `sizeof(std::shared_ptr<int>)` | 16 | 16 | 相同 |
| `sizeof(std::unique_ptr<int>)` | 8 | 8 | 相同 |
| `sizeof(std::function<void()>)` | **32** | **64** | 两倍 |
| `sizeof(std::function<int(int)>)` | **32** | **64** | 两倍 |
| `sizeof(std::optional<int>)` | 8 | 8 | 相同 |
| `sizeof(std::optional<std::string>)` | 40 | 40 | 相同 |
| `sizeof(std::tuple<char, int>)` | 8 | 8 | 相同 |
| `sizeof(std::error_code)` | 16 | 16 | 相同 |
| `sizeof(std::regex)` | **32** | **40** | 八字节 |
| 空 `std::string` 的 `capacity` | 15 | 15 | 相同（SSO 阈值一致） |

**十三行里有十行相同**，这不是巧合：能在 32 字节里放下 SSO 缓冲区的实现都选了 32，
都能用两个指针加一个长度描述 `vector` 的都选了 24。
**真正差出来的三行各有原因**：`std::map` 在 MSVC STL 里用了「压缩结点 + 哨兵」，
把整棵树压进一个指针加一个计数；`std::function` 在 MSVC STL 里留了 48 字节的原地缓冲区、
libstdc++ 只留 16 字节；`std::regex` 一个存的是实现类的指针加两个整数，另一个多存了一个。

**这三行就是差别所在，而它们的代价是实的**：一个 `std::map<int,int>` 的数组在两边占的内存差三倍，
一个装 `std::function` 的容器在 MSVC 上的每个元素要多占 32 字节。
**`sizeof` 是编译期常量，所以这些数字在优化后仍然存在**——它们不是「实现细节」，是内存账。

`待确认`

**这三处差异来自本机的两套实现，不保证别的版本一样。** 判断办法是本机各编一遍上面这个程序，
**不要照抄表里的数字**；能照抄的只有下面这条结论：**`sizeof` 随实现变，跨平台时不要拿它当契约。**

## 2.3 对已排序输入的排序行为

**`std::sort` 的平均复杂度是 O(N log N)，标准没有规定它检测「已经有序」的输入**，
因此同一个数组，两套实现的耗时可能差出一截。

`C++`

```cpp
/* sorted_sort.cpp    编译：g++ -std=c++17 -O2 sorted_sort.cpp -o sorted_sort ；MSVC 见 2.1 */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

static unsigned long long g_state = 12345;
static int next_rand() {              /* 自带线性同余，排除 <random> 的实现差异 */
    g_state = g_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (int)(g_state >> 33);
}

template <class F>
static long long time_ms(F body) {
    auto t0 = std::chrono::steady_clock::now();
    body();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - t0).count();
}

int main() {
    const std::size_t kN = 2000000;
    std::vector<int> base(kN);
    for (std::size_t i = 0; i < kN; ++i) { base[i] = next_rand(); }

    std::vector<int> work = base;
    std::sort(work.begin(), work.end());                 /* 先排好，当作「已排序」输入 */
    long long ms_sorted = time_ms([&] { std::sort(work.begin(), work.end()); });

    work = base;
    std::sort(work.begin(), work.end());
    std::reverse(work.begin(), work.end());              /* 逆序输入 */
    long long ms_reverse = time_ms([&] { std::sort(work.begin(), work.end()); });

    work = base;                                         /* 随机输入 */
    long long ms_random = time_ms([&] { std::sort(work.begin(), work.end()); });

    std::printf("两百万元素 std::sort：已排序 %lld 毫秒，逆序 %lld 毫秒，随机 %lld 毫秒\n",
                ms_sorted, ms_reverse, ms_random);
    return 0;
}
```

`实测数据`
`Text`

```text
libstdc++ 15.2.0，连跑三次：
两百万元素 std::sort：已排序 13 毫秒，逆序 7 毫秒，随机 99 毫秒
两百万元素 std::sort：已排序 15 毫秒，逆序 9 毫秒，随机 98 毫秒
两百万元素 std::sort：已排序 11 毫秒，逆序 8 毫秒，随机 94 毫秒

MSVC STL（cl 19.44），同一份源码：
两百万元素 std::sort：已排序 9 毫秒，逆序 12 毫秒，随机 114 毫秒
```

**两件事同时成立，而它们看起来矛盾。**

**第一，随机输入上 libstdc++ 快两成左右**（94 到 99 毫秒对 114 毫秒）。
两套实现用的都是「内省排序」——快排加一个深度上限，超了就换堆排序——**差别在插入排序的阈值、
三数取中的写法、以及分区时的搬移方式上**。两成的差距属于实现风格，不是谁对谁错。

**第二，「已排序输入」在两边都不慢，反而快了十倍。** 直觉上「已经有序」该是快排的最好情况，
但三数取中在完全有序的数组上每次都取到正中，**划分反而最均衡、且一次交换都不用做**，
于是只剩顺序扫描的内存带宽。**真正慢的是随机输入**，因为每次划分都要swap，缓存命中率低。
**还有一处**：逆序输入在 libstdc++ 上比顺序输入还快，原因是这种输入下三数取中取到的同样是正中，
而交换模式对预取器更友好。

> [!WARNING]
> **上面这些数字换了机器、换了元素类型都会变。** 能带走的结论只有一条：
> **「已排序」不等于「最快」，「逆序」也不等于「最慢」，只有实测能给出答案。**
> 要排序已经有序的数据时，先问「我怎么知道它有序」——若真知道，直接跳过排序比任何调优都强。

## 2.4 `std::regex` 的现实

**`std::regex` 是标准库里最常被误用的一件设施。** 它接口漂亮、写起来像脚本语言，
但**它的实现代价与正则在两套实现里的差距都很大**。

`C++`

```cpp
/* regex_cost.cpp    编译：g++ -std=c++17 -O2 regex_cost.cpp -o regex_cost ；MSVC 见 2.1 */
#include <chrono>
#include <cstdio>
#include <regex>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;

static long long ms_since(Clock::time_point t0) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
}

int main() {
    const int kN = 20000;
    std::vector<std::string> samples;
    char buf[32];
    for (int i = 0; i < kN; ++i) {
        std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", 2000 + i % 25, 1 + i % 12, 1 + i % 28);
        samples.emplace_back(buf);
    }
    /* 1. std::regex：正则对象在循环外构造一次，这是正确的用法 */
    std::regex re(R"(^(\d{4})-(\d{2})-(\d{2})$)");
    auto t0 = Clock::now();
    int hits = 0;
    for (const auto &s : samples) {
        std::smatch m;
        if (std::regex_match(s, m, re)) { ++hits; }
    }
    std::printf("std::regex 匹配 %d 次：%lld 毫秒（命中 %d）\n", kN, ms_since(t0), hits);

    /* 2. 把正则对象构造在循环里 —— 同一件事，写法只差一行 */
    t0 = Clock::now();
    int hits2 = 0;
    for (const auto &s : samples) {
        std::regex local(R"(^(\d{4})-(\d{2})-(\d{2})$)");
        std::smatch m;
        if (std::regex_match(s, m, local)) { ++hits2; }
    }
    std::printf("正则构造在循环里  ：%lld 毫秒（命中 %d）\n", ms_since(t0), hits2);

    /* 3. 手写扫描：同一个格式 yyyy-mm-dd */
    t0 = Clock::now();
    int hits3 = 0;
    for (const auto &s : samples) {
        if (s.size() == 10 && s[4] == '-' && s[7] == '-') {
            bool ok = true;
            for (std::size_t i : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u}) {
                if (s[i] < '0' || s[i] > '9') { ok = false; break; }
            }
            if (ok) { ++hits3; }
        }
    }
    std::printf("手写扫描          ：%lld 毫秒（命中 %d）\n", ms_since(t0), hits3);
    std::printf("sizeof(std::regex) = %zu\n", sizeof(std::regex));
    return 0;
}
```

`实测数据`
`Text`

```text
libstdc++ 15.2.0：
std::regex 匹配 20000 次：3 毫秒（命中 20000）
正则构造在循环里  ：130 毫秒（命中 20000）
手写扫描          ：0 毫秒（命中 20000）
sizeof(std::regex) = 32

MSVC STL（cl 19.44）：
std::regex 匹配 20000 次：36 毫秒（命中 20000）
正则构造在循环里  ：73 毫秒（命中 20000）
手写扫描          ：0 毫秒（命中 20000）
sizeof(std::regex) = 40
```

**把次数放大到二十万，比值更稳定**：

`实测数据`
`Text`

```text
libstdc++ 15.2.0，连跑三次：
200000 次 regex_match：37044 微秒（命中 200000），每次 0.185 微秒，每秒约 5398985 次
200000 次 regex_match：35698 微秒（命中 200000），每次 0.178 微秒，每秒约 5602555 次
200000 次 regex_match：41428 微秒（命中 200000），每次 0.207 微秒，每秒约 4827653 次

MSVC STL（cl 19.44），连跑三次：
200000 次 regex_match：379513 微秒（命中 200000），每次 1.898 微秒，每秒约 526991 次
200000 次 regex_match：372711 微秒（命中 200000），每次 1.864 微秒，每秒约 536609 次
200000 次 regex_match：372692 微秒（命中 200000），每次 1.863 微秒，每秒约 536636 次
```

**三行结论，每一行都有数字支撑。**

**第一，同一件事在两套实现上差十倍**：每次 0.185 微秒对 1.87 微秒，每秒 540 万次对 53 万次。
**同一个正则、同一份源码、同一个优化等级**，差的只是标准库。
这解释了为什么「在我机器上够快」不能作为选型依据——**换一套工具链就慢一个数量级**。

**第二，`std::regex` 对象必须构造在循环外。** libstdc++ 上是 130 毫秒对 3 毫秒（四十倍），
MSVC 上是 73 对 36（两倍）——**两边的惩罚不同，但构造一次都是必须的**。
正则的编译要建状态机，那是一次实打实的分配与解析；把它放进循环，
等于每处理一行文本重编一次正则。

**第三，手写扫描在这两个格式上是 0 毫秒。** `yyyy-mm-dd` 的校验只有十次字符比较，
二十万次加起来仍在计时器分辨率以下。**正则解决的是「格式会变」的问题，
格式固定时它是纯粹的额外开销。**

> [!TIP]
> **三条能落地的判断**：正则在循环外构造；**格式固定的解析写成手写扫描**；
> 真要通用的正则能力，先量一量本机 `std::regex` 的吞吐，再决定要不要引入第三方库。
> 标准库没有义务把正则做到最快——**它只保证语义正确**。

## 2.5 换实现时会碰到的三处

| 会变的 | 本机实测的差别 | 写代码时怎么办 |
|---|---|---|
| `sizeof` 与内存占用 | `std::map` 48 对 16 字节、`std::function` 32 对 64 字节 | 不把 `sizeof` 当契约；接口边界上不假设对象大小 |
| 同一算法的耗时 | `std::sort` 差两成、`std::regex` 差十倍 | 性能结论必须标清工具链；热点自己写而不是猜库 |
| 容器的迭代器失效规则之外的细节 | 增长因子、SSO 阈值、`unordered_*` 的桶数 | 只依赖标准写明的部分，见《07-标准库/B-02-std-string 与 string_view.md》第 2 节 |

**这三处的共同点是：标准只管「能不能用」，不管「多快多大」。**
把「多快」写进设计文档时，**必须带上工具链版本**——否则半年后没人能解释那组数字。

---

# 第 3 节 什么时候不该用标准库

## 3.1 性能敏感的格式化

**「用标准库」是默认选择，不是教条。** 一处例外是热循环里的格式化：
`<sstream>` 每次都要构造一个 `basic_stringbuf`、管理一块动态缓冲区，
**这份代价在每行几十字节的报表里看不出来，在百万行的循环里就是几分钟。**

`C++`

```cpp
/* format_cost.cpp    编译：g++ -std=c++17 -O2 format_cost.cpp -o format_cost ；MSVC 见 2.1 */
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

using Clock = std::chrono::steady_clock;

static long long ms_since(Clock::time_point t0) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
}

int main() {
    const int kN = 200000;
    long long sink = 0;

    char buf[64];                                    /* 1. snprintf */
    auto t0 = Clock::now();
    for (int i = 0; i < kN; ++i) {
        std::snprintf(buf, sizeof buf, "第 %d 项：%.2f", i, i * 0.5);
        sink += buf[0] != 0;
    }
    long long ms_snprintf = ms_since(t0);

    t0 = Clock::now();                               /* 2. ostringstream */
    for (int i = 0; i < kN; ++i) {
        std::ostringstream os;
        os << "第 " << i << " 项：" << std::fixed << std::setprecision(2) << i * 0.5;
        sink += os.str()[0] != 0;
    }
    long long ms_oss = ms_since(t0);

    t0 = Clock::now();                               /* 3. std::to_string 拼接 */
    for (int i = 0; i < kN; ++i) {
        std::string s = "第 " + std::to_string(i) + " 项：" + std::to_string(i * 0.5);
        sink += s[0] != 0;
    }
    long long ms_tostring = ms_since(t0);

    std::printf("格式化 %d 行：\n", kN);
    std::printf("  snprintf         %lld 毫秒\n", ms_snprintf);
    std::printf("  ostringstream    %lld 毫秒\n", ms_oss);
    std::printf("  to_string 拼接   %lld 毫秒\n", ms_tostring);
    std::printf("（校验和 %lld）\n", sink);
    return 0;
}
```

`实测数据`
`Text`

```text
libstdc++ 15.2.0：
格式化 200000 行：
  snprintf         41 毫秒
  ostringstream    103 毫秒
  to_string 拼接   55 毫秒
（校验和 600000）

MSVC STL（cl 19.44）：
格式化 200000 行：
  snprintf         35 毫秒
  ostringstream    132 毫秒
  to_string 拼接   66 毫秒
（校验和 600000）
```

**两套实现给出的结论一致**：`ostringstream` 比 `snprintf` 慢两倍半到三倍半。
二十万行差 60 到 100 毫秒，**一千万行就是 3 到 5 秒**——而这三个版本干的活完全相同。
`std::to_string` 落在中间：它省掉了流对象，但每次拼接都新建一个 `std::string`。

> [!TIP]
> **三条可操作的规则**：热循环里用 `snprintf` 写进栈上的缓冲区；
> **输出到文件或终端时用 `<<` 而不是先拼成字符串再写**（那会白白多一次分配）；
> 需要类型安全的格式化、且编译器支持 C++20 时，用 `std::format` 替代 `ostringstream`。
> **`printf` 的家族不是「C 的旧东西」，在格式化这件事上它至今是最快的一档**——
> 与 A-01、B-01 的对照见《07-标准库/B-01-输入输出：iostream.md》第 8 节。

## 3.2 需要稳定 ABI 的边界

**库的实现在一个模块里、调用方在另一个模块里时，「用标准库」会撞上 ABI。**
最典型的是导出函数签名里出现 `std::string`：**同一个名字，两套实现修饰出来的符号完全不同**。

`C++`

```cpp
/* abi_sym.cpp    编译：g++ -std=c++17 -c abi_sym.cpp -o abi_sym.o ；MSVC 见下 */
#include <string>
#include <vector>

std::string make_name(int n) { return std::string(static_cast<std::size_t>(n), 'x'); }
std::vector<int> make_vec(int n) { return std::vector<int>(static_cast<std::size_t>(n), 0); }
```

`Bash`

```bash
g++ -std=c++17 -c abi_sym.cpp -o abi_sym.o
nm abi_sym.o | grep make_               # libstdc++：编成目标文件，再看符号表

cmd /c '"<VS>\VC\Auxiliary\Build\vcvars64.bat" >nul && cl /nologo /utf-8 /std:c++17 /EHsc /c abi_sym.cpp'      # MSVC：同一个源文件，同一个命令形状
cmd /c '"<VS>\VC\Auxiliary\Build\vcvars64.bat" >nul && dumpbin /symbols abi_sym.obj'
```

`实测数据`
`Text`

```text
libstdc++ 15.2.0（nm abi_sym.o）：
000000000000006f T _Z8make_veci
0000000000000000 T _Z9make_nameB5cxx11i

MSVC STL（cl 19.44，dumpbin /symbols abi_sym.obj）：
?make_name@@YA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@H@Z
?make_vec@@YA?AV?$vector@HV?$allocator@H@std@@@std@@H@Z
```

**两行同名函数，两个世界。** libstdc++ 用的是 Itanium C++ ABI（`_Z` 开头），
MSVC 用自己的修饰方案（`?` 开头，模板参数展开成一长串 `?$basic_string@...`）。
**同一个函数在这两套 ABI 下的符号名没有任何公共部分**，
因此一个用 MinGW 编出来的 DLL，不能被 MSVC 的程序链接，反之亦然。

**libstdc++ 那一行里还有一处值得注意的细节**：`make_name` 的符号多了一段 `B5cxx11`，
而 `make_vec` 没有。**这段后缀是 `std::string` 的 ABI 标记**——
它区分「C++11 之后的新 `std::string`」与「旧实现」两种布局，
由 `_GLIBCXX_USE_CXX11_ABI` 这个宏控制。**同一台机器上，这个宏取不同值的两个库也连不起来**，
报错是一大片 `undefined reference`，而且符号名看起来只差那几个字符。

`待确认`

**`_GLIBCXX_USE_CXX11_ABI` 切换后的完整报错原文没有在本机跑过**——
本机只装了默认配置的 libstdc++。上面那段结论的另一半（符号里带 `B5cxx11`）是实测观察到的，
「切换宏会改这段后缀」出自 GCC 官方文档，**遇到这种情况时必须在本机验证之后才能下结论**。

> [!CAUTION]
> **不要在库的导出接口上直接传标准库类型。**
> 一个返回 `std::string` 的函数、一个收 `std::vector<int>` 的参数，
> 都把「两边的标准库必须同源、同版本、同配置」写进了接口契约。
> **跨模块传数据时传 C 风格的东西**：`const char *` 加长度、裸指针加计数、或者自己定义的 POD 结构体。
> 这不是「不用标准库」，而是**把标准库用在不跨越边界的地方**。

## 3.3 无异常、无 RTTI 的场景

**嵌入式与游戏主机上常见两个开关：`-fno-exceptions` 与 `-fno-rtti`。**
语言本身能关，但**标准库的一部分设施依赖它们**，于是「关掉之后哪些还能用」成了必须先回答的问题。

**先看编译期**。关掉异常之后，`try`/`catch` 直接变成语法错误。

`实测数据`
`Text`

```text
$ g++ -std=c++17 -fno-exceptions noexc.cpp -o noexc
noexc.cpp: In function 'int main()':
noexc.cpp:12:39: error: exception handling disabled, use '-fexceptions' to enable
   12 |     } catch (const std::out_of_range &e) {
      |                                       ^
noexc.cpp:13:35: error: 'e' was not declared in this scope
```

关掉 RTTI 之后，`typeid` 与 `dynamic_cast` 各自报一条。

`实测数据`
`Text`

```text
$ g++ -std=c++17 -fno-rtti nortti.cpp -o nortti
nortti.cpp: In function 'int main()':
nortti.cpp:11:46: error: cannot use 'typeid' with '-fno-rtti'
   11 |     std::printf("typeid 名字：%s\n", typeid(*p).name());
      |                                              ^
nortti.cpp:12:47: error: 'dynamic_cast' not permitted with '-fno-rtti'
   12 |     std::printf("dynamic_cast：%p\n", (void *)dynamic_cast<Derived *>(p));
      |                                               ^~~~~~~~~~~~~~~~~~~~~~~~~~
```

**再看运行期，还有一条更容易吃亏的规律**：**通过编译不等于库不抛异常。**

`C++`

```cpp
/* noexc_run.cpp    编译：g++ -std=c++17 -fno-exceptions noexc_run.cpp -o noexc_run */
#include <cstdio>
#include <string>

int main() {
    std::printf("先打印一行，确认程序起来了\n");
    std::fflush(stdout);
    int n = std::stoi("这不是数字");             /* stoi 出错要抛 std::invalid_argument */
    std::printf("没有走到这里：n = %d\n", n);
    return 0;
}
```

`实测数据`
`Text`

```text
先打印一行，确认程序起来了
terminate called after throwing an instance of 'std::invalid_argument'
  what():  stoi
```

**退出码为 3，程序没有走到第二行打印。** 原因在于分工：
`std::stoi` 的实现编译在 libstdc++ 里，而**库里是带着异常编的**；
`-fno-exceptions` 只作用于你这一次编译，**它管不到已经编好的库**。
于是异常真的抛了出来，而调用方没有展开信息、也没有处理器，标准库只能调 `terminate`。

> [!WARNING]
> **关掉异常之后，标准库里所有「出错就抛」的接口都变成了「出错就终止」。**
> `std::stoi`、`std::string::at`、`std::vector::at`、`std::filesystem` 的抛异常重载、
> `std::regex` 的构造，全都属于这一类。**要留在无异常环境里的代码，得换成不抛的那一套**：
> `from_chars` 代替 `stoi`、下标访问代替 `at`、`error_code` 重载代替 `filesystem` 的抛异常版本
> （见《07-标准库/B-02-std-string 与 string_view.md》第 7 节与
> 《07-标准库/B-07-文件系统：filesystem.md》第 5 节）。

## 3.4 一张决策表

| 场合 | 该不该用标准库 | 替代做法 |
|---|---|---|
| 普通业务逻辑、数据搬运 | **用** | —— |
| 热循环里的格式化 | 不用 `<sstream>` | `snprintf`、C++20 的 `std::format` |
| 格式固定的解析 | 不用 `std::regex` | 手写扫描；要通用正则就上第三方库 |
| 跨 DLL / so 的导出接口 | **不传标准库类型** | `const char *` 加长度、POD 结构体 |
| 无异常环境 | 用不抛的那一套 | `from_chars`、`error_code` 重载 |
| 无 RTTI 环境 | 不用 `dynamic_cast` | 虚函数、标签联合、`variant` |
| 固定大小的内存受限环境 | 慎用会分配的设施 | 预分配缓冲区、`array`、自定分配器 |
| 单文件小工具 | **放开用** | 开发速度比纳秒重要 |

**表里最后一行并非玩笑。** 标准库的第一次收益来自「不写、不错」，
**它的代价只在实测确认的热点上才值得讨论**——没测之前不要提前优化。

---

# 第 4 节 一页速查：本板块全部头文件与代表件

## 4.1 A 段：C 标准库

| 章 | 讲什么 | 头文件 | 代表件 |
|---|---|---|---|
| `A-00` | 导读：C 标准库是怎么组织的、怎么查 | `<xxx.h>` 全体、`<stddef.h>` | 库与语言的分界、实现定义与未定义、头文件与 C 运行库 |
| `A-01` | 输入输出 | `<stdio.h>` | `printf` 家族、`scanf` 家族、`fopen`/`fread`/`fwrite`/`fseek`/`ftell`、`stdout` 缓冲、`perror` |
| `A-02` | 字符串与内存 | `<string.h>`、`<stdlib.h>` | `strlen`/`strcpy`/`strcmp`/`strstr`、`memcpy`/`memmove`/`memset`、`strncpy` 的坑、`snprintf` |
| `A-03` | 数值、数学与随机 | `<math.h>`、`<float.h>`、`<limits.h>`、`<stdint.h>`、`<inttypes.h>`、`<stdbit.h>` | `sqrt`/`pow`/`fmod`、`INT_MAX`/`DBL_EPSILON`、`int64_t` 与 `PRIu64`、`rand`/`srand` |
| `A-04` | 时间与日期 | `<time.h>` | `time`/`clock`/`difftime`、`localtime`/`gmtime`/`mktime`、`strftime`、`timespec_get` |
| `A-05` | 工具与其它 | `<stdlib.h>`、`<assert.h>`、`<ctype.h>`、`<errno.h>`、`<setjmp.h>`、`<signal.h>`、`<stdbool.h>`、`<stdarg.h>` | `malloc` 一族、`qsort`/`bsearch`、`exit`/`atexit`、`getenv`/`system`、`assert`、`isalpha`、`errno`、`va_list` |

## 4.2 B 段：C++ 标准库

| 章 | 讲什么 | 头文件 | 代表件 |
|---|---|---|---|
| `B-00` | 导读：C++ 标准库与 C 的关系 | `<cstdio>`、`<stdio.h>` | 一份内容两个位置、`std::` 里的 C 名字、与 STL 的边界 |
| `B-01` | 输入输出 | `<iostream>`、`<iomanip>`、`<sstream>`、`<fstream>` | `cin`/`cout`/`cerr`/`clog`、`<<` 与 `>>`、`setw`/`setprecision`、流状态、`getline`、`stringstream`、`ifstream`/`ofstream` |
| `B-02` | `std::string` 与 `string_view` | `<string>`、`<string_view>` | 构造与容量（SSO）、查找与修改、引用失效、`c_str`/`data`、`stoi`/`to_string`/`from_chars` |
| `B-03` | 智能指针的用法 | `<memory>` | `unique_ptr`/`shared_ptr`/`weak_ptr`、`make_unique`/`make_shared`、删除器、循环引用 |
| `B-04` | 可调用物的包装 | `<functional>` | `function`、`bind`、`mem_fn`、`reference_wrapper`、`invoke` |
| `B-05` | 数值 | `<limits>`、`<cmath>`、`<random>`、`<numeric>`、`<complex>`、`<valarray>` | `numeric_limits`、`std::sqrt` 的重载、引擎与分布、`accumulate`/`iota`/`gcd`/`lcm` |
| `B-06` | 时间 | `<chrono>` | `duration`/`time_point`、`steady_clock` 与 `system_clock`、`sleep_for` |
| `B-07` | 文件系统 | `<filesystem>` | `path`、`directory_iterator`、`exists`/`file_size`、创建删除改名复制、`error_code` 与异常两条路 |
| `B-08` | 工具类（上） | `<utility>`、`<tuple>`、`<optional>`、`<variant>`、`<any>` | `pair`、结构化绑定、`optional`、`variant` 与 `visit`、`any`、`swap`/`exchange` |
| `B-09` | 工具类（下） | `<type_traits>`、`<typeinfo>`、`<concepts>` | `is_*`/`enable_if`/`decay`/`common_type`、`typeid`/`type_info` 的边界、标准概念库 |
| `B-10` | 内存与并发的基础设施 | `<memory>`、`<mutex>`、`<shared_mutex>`、`<condition_variable>`、`<atomic>` | `allocator`/`allocator_traits`、`alignas` 与 `align_val_t`、`mutex` 家族与三种锁包装、`wait` 的谓词版、`atomic` 与 CAS、`volatile` 的边界 |

**两张表与板块入口的索引一一对应**；加上本章节，本板块共 18 章。
**`<thread>` 不在 B-10 里**：线程的创建与 `join`、数据竞争与内存序归【待补：10-并发与并行/】，
B-10 只讲同步设施的接口怎么用。

**章号之外还有一条线索**：A 段的每一章都能在 B 段找到对照——
`A-01` 对 `B-01`、`A-02` 对 `B-02`、`A-03` 对 `B-05`、`A-04` 对 `B-06`、`A-01` 对 `B-07`。
**遇到「这件事 C 怎么做」时，可以按这条线查找。**

---

# 第 5 节 检查清单

**交代码之前应当逐条核对，每一条都对应本板块里一处真实的坑。**

1. **错误路径处理了吗**——`fopen` 返回空、`stoi` 抛异常、`filesystem` 的 `error_code`，三条路各验证一次。
2. **引用会不会失效**——`std::string`/`vector` 扩容之后，之前拿到的指针、引用、迭代器全部作废。
3. **锁的作用域对吗**——锁要包住「读—改—写」整段，不是只包住写；临界区里不调用未知函数。
4. **`wait` 用的是谓词版吗**——`cv.wait(lk)` 与 `cv.wait(lk, pred)` 只差一个参数，后者才正确。
5. **格式串与实参对得上吗**——`printf` 的 `%d` 配 `long long` 是未定义行为，编译器的 `-Wformat` 要打开。
6. **长度参数给对了吗**——`snprintf(buf, sizeof buf, ...)`、`fread(p, size, n, f)`，两处都容易漏。
7. **缓冲区会不会溢出**——`strcpy`/`sprintf`/`gets` 不用；`strncpy` 不补 `\0`，改 `snprintf`。
8. **`sizeof` 当契约了吗**——写进接口、写进配置文件、写进协议的都是错，它随实现变。
9. **跨模块传的是标准库类型吗**——导出接口上出现 `std::string`、`std::vector` 就是 ABI 风险。
10. **异常被关掉了吗**——`-fno-exceptions` 下 `stoi`、`at`、`regex` 都变成「出错就终止」。
11. **流的错误状态查了吗**——`>>` 失败只置 `failbit`，不抛异常；不查就会拿到半截数据。
12. **性能结论标了工具链吗**——同一个 `std::sort` 在两套实现上差两成，`std::regex` 差十倍。

> [!NOTE]
> **十二条里有八条是「错误路径」**，这不是巧合：
> **标准库把正确路径写得很短，把错误路径留给了调用方。**
> 这也是本板块一开始那句话的落点——**常用的那些用得对不对，比会多少个头文件重要。**

---

# 附录 A 复现本章节实测

**环境**：Windows 11；libstdc++ 侧是 MinGW-w64 `g++` 15.2.0，命令为 `g++ -std=c++17 -O2`；
MSVC 侧是 VS 生成工具 2022 的 `cl` 19.44，命令为 `cl /nologo /utf-8 /std:c++17 /EHsc /O2`，
两者都先执行 `<VS>\VC\Auxiliary\Build\vcvars64.bat`。

| 程序 | 说明 | 小节 |
|---|---|---|
| `names_h_std.cpp` | **期望失败**：`<stdio.h>` 之后写 `std::printf` | 1.1 |
| `names_cxx_std.cpp` | 同样的内容换成 `<cstdio>` 即可通过编译 | 1.1 |
| `cxxx_macro.cpp` | 函数走 `std::`、宏走全局 | 1.3 |
| `sizes.cpp` | 两套实现的对象大小对照 | 2.2 |
| `sorted_sort.cpp` | 已排序 / 逆序 / 随机三种输入 | 2.3 |
| `regex_cost.cpp` | `std::regex` 的两次对照 | 2.4 |
| `format_cost.cpp` | 三种格式化写法的成本 | 3.1 |
| `abi_sym.cpp` | **只编译**：两套实现的符号名 | 3.2 |
| `noexc_run.cpp` | **运行时终止**：无异常下 `stoi` 出错 | 3.3 |

**两条复现注意**：计时数字与机器有关，**量级关系比绝对值重要**；
`noexc.cpp` 与 `nortti.cpp` 那两个只在 `-fno-exceptions` / `-fno-rtti` 下才失败，
完整命令见 3.3 小节的两段报错原文上方。

配套示例见 [`B-examples/07-standard-library/09-stdlib-capstone/`](../B-examples/07-standard-library/09-stdlib-capstone/)，配套练习见 [`C-templates/07-standard-library/09-stdlib-capstone/`](../C-templates/07-standard-library/09-stdlib-capstone/)。

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《07-标准库/B-00-导读：C++ 标准库与 C 的关系.md》第 2 节 | **前置**：`<cstdio>` 与 `<stdio.h>` 的第一轮对照 |
| 《01-编译器/01-编译与链接.md》第 3 节 | **前置**：头文件、库与链接的分工 |
| 《03-构建工具链/04-符号与调试信息.md》第 2 节 | **前置**：符号修饰与 ABI |
| 《04-语法/13-异常.md》第 1 节 | **前置**：异常的语言规则 |
| 《07-标准库/A-05-工具与其它：stdlib 与杂项.md》第 1 节 | 对照：C 的 `malloc` 与 C++ 的分配器 |
| 《06-更底层/05-ABI 与调用约定.md》章节、《06-更底层/09-C++ 对象布局与它的硬件代价.md》章节、《06-更底层/11-系统调用：程序与内核的边界.md》章节 | 相关：ABI、内存布局与系统调用的完整讲法 |
| 【待补：10-并发与并行/】 | 相关：`std::thread`、数据竞争与内存序 |
