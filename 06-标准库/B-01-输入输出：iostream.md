# iostream

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**`printf` 把「怎么解释这段字节」写在格式串里，`iostream` 把它交给类型。**

`printf("%d", x)` 里的 `%d` 是一个约定：调用者保证 `x` 是 `int`。
编译器管不了这件事，类型对不上时它默认不报错，程序照常运行，输出是另一个数。
`std::cout << x` 没有这个约定要遵守——`x` 是什么类型，编译器在重载决议里就选好了对应的重载。

**这个差别带来两样东西**：一是编译期的检查，二是可扩展性。
自定义类型只要写一个 `operator<<`，就能像内建类型一样被输出，不必去改库的源码。
代价是运行期多一层：格式状态存在流对象里，数值转换穿过 locale 面，虚函数调用无法全部内联。

**C 的那一半在 A 段已经讲过**：`printf` 的格式细节、`fopen` 的打开模式、缓冲与 `fflush`，
见《06-标准库/A-01-输入输出：stdio.md》第 1 节与第 2 节。本章节只讲换到 C++ 这边之后
多了什么、少了什么、代价落在哪里。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整源码与复现命令见附录 A；`文档` 表示引自标准草案或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<版本号>`）的含义见《README.md》。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 流与缓冲、`printf` 一族、`fopen` 的模式 | 《06-标准库/A-01-输入输出：stdio.md》第 1 节 |
| `printf` 的格式化细节与返回值的坑 | 《06-标准库/A-01-输入输出：stdio.md》第 2 节 |
| `operator<<` 为什么写成非成员函数 | 《05-类与面向对象/09-运算符重载.md》第 3.4 小节 |
| 函数重载与重载决议 | 《04-语法/07-函数.md》第 5 节 |
| 异常的抛出与捕获 | 《04-语法/13-异常.md》第 1 节 |
| RAII：文件为什么不用手动关 | 《05-类与面向对象/06-RAII 与资源管理.md》第 2 节 |

**相邻的章节**：上承 A 段的《06-标准库/A-01-输入输出：stdio.md》，
下接《06-标准库/B-02-std-string 与 string_view.md》（`getline` 读进来的东西就是 `std::string`）。
`<fstream>` 遍历目录的替代品是《06-标准库/B-07-文件系统：filesystem.md》，
`std::endl` 背后那次「刷」与《06-标准库/A-01-输入输出：stdio.md》第 1 节的缓冲是同一件事。

| 本章各节 | 讲什么 |
|---|---|
| 第 1 节 | `cin`/`cout`/`cerr`/`clog` 四个标准流对象；`cerr` 与 `clog` 的区别 |
| 第 2 节 | `<<` 与 `>>` 的本质；给自己的类型写一对；类型安全的实际含义 |
| 第 3 节 | `<iomanip>`：`setw`、`setfill`、`setprecision`、`hex`、`boolalpha`、`fixed` |
| 第 4 节 | 流的状态位（`failbit`/`eofbit`/`badbit`）、`clear` 与 `ignore`、异常开关 |
| 第 5 节 | `getline` 与整行读取；与 `>>` 混用时的换行残留 |
| 第 6 节 | `<sstream>`：把字符串当流用 |
| 第 7 节 | `<fstream>`：打开模式、文本与二进制、与 `fopen` 的对照 |
| 第 8 节 | 与 `printf` 的取舍：同一任务的两种写法 + 吞吐实测 |

---

# 第 1 节 四个标准流对象

## 1.1 它们的来历

`<iostream>` 里预置了四个对象，它们在 `main` 之前就已经构造好，分别接在 C 的三条标准流上：

`文档`

> "The object `cin` controls input from a stream buffer associated with the
> object `stdin`, declared in `<cstdio>`. … The object `cout` controls output
> to a stream buffer associated with the object `stdout`. … The object `cerr`
> controls output to a stream buffer associated with the object `stderr`. …
> The object `clog` controls output to a stream buffer associated with the
> object `stderr`."
>
> —— N4659 §30.4.3/1、3、4、6

**最后两条值得注意**：`cerr` 与 `clog` 接的是**同一个** `stderr`，差别不在设备上，而在标志位上——标准只对 `cerr` 提了「不缓冲」的要求：

`文档`

> "After the object `cerr` is initialized, `cerr.flags() & unitbuf` is nonzero
> and `cerr.tie()` returns `&cout`."
>
> —— N4659 §30.4.3/5

标准对 `clog` 没有对应的那句话：**一件事只对其中一个对象作了要求，这就构成了两者的全部差别。**

`C++`

```cpp
/* streams_flags.cpp    编译：g++ -std=c++17 streams_flags.cpp -o streams_flags */
#include <iostream>

int main() {
    std::cout << std::boolalpha;
    std::cout << "cerr 的 unitbuf 位：" << bool(std::cerr.flags() & std::ios::unitbuf) << "\n";
    std::cout << "clog 的 unitbuf 位：" << bool(std::clog.flags() & std::ios::unitbuf) << "\n";
    std::cout << "cout 的 unitbuf 位：" << bool(std::cout.flags() & std::ios::unitbuf) << "\n";
    std::cout << "cin::tie() == &cout：" << (std::cin.tie() == &std::cout) << "\n";
    std::cout << "cerr::tie() == &cout：" << (std::cerr.tie() == &std::cout) << "\n";
    std::cout << "clog::tie() == &cout：" << (std::clog.tie() == &std::cout) << "\n";
    std::cout << "cin 与 cerr 共用一个缓冲：" << (std::cin.rdbuf() == std::cerr.rdbuf()) << "\n";
    std::cout << "cerr 与 clog 共用一个缓冲：" << (std::cerr.rdbuf() == std::clog.rdbuf()) << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
cerr 的 unitbuf 位：true
clog 的 unitbuf 位：false
cout 的 unitbuf 位：false
cin::tie() == &cout：true
cerr::tie() == &cout：true
clog::tie() == &cout：false
cin 与 cerr 共用一个缓冲：false
cerr 与 clog 共用一个缓冲：true
```

**`cerr` 与 `clog` 共用一个缓冲，但一个设了 `unitbuf`，另一个没设**：区别只在这一个标志位上，与设备无关。

## 1.2 `unitbuf` 与刷新的时机

`unitbuf` 的含义是「每次插入之后刷一次缓冲」。这一差别用一个会数数的缓冲即可看出：

`C++`

```cpp
/* unitbuf_count.cpp    编译：g++ -std=c++17 unitbuf_count.cpp -o unitbuf_count */
#include <iostream>
#include <streambuf>

// 一个自己数数的缓冲：谁刷了它，它就记一笔
struct CountingBuf : std::streambuf {
    int syncs = 0, chars = 0;              // sync() 被调用几次、收到几个字符
    int overflow(int c) override { if (c != EOF) ++chars; return c; }
    int sync() override { ++syncs; return 0; }
};

int main() {
    CountingBuf cerr_buf, clog_buf;
    std::streambuf *old_cerr = std::cerr.rdbuf(&cerr_buf);
    std::streambuf *old_clog = std::clog.rdbuf(&clog_buf);

    std::cerr << "a" << "b" << "c";        // 三次插入
    std::clog << "a" << "b" << "c";        // 三次插入

    std::cout << "cerr：三次插入后 sync 次数 = " << cerr_buf.syncs
              << "，收到字符 = " << cerr_buf.chars << "\n";
    std::cout << "clog：三次插入后 sync 次数 = " << clog_buf.syncs
              << "，收到字符 = " << clog_buf.chars << "\n";

    std::cerr.rdbuf(old_cerr);             // 还原，避免退出时写到已销毁的缓冲
    std::clog.rdbuf(old_clog);
    return 0;
}
```

`实测数据`
`Text`

```text
cerr：三次插入后 sync 次数 = 3，收到字符 = 3
clog：三次插入后 sync 次数 = 0，收到字符 = 3
```

**同样的三次插入，`cerr` 刷了三次，`clog` 一次也没刷**，但两者收到的字符数相同——`clog` 的内容要等到缓冲满、
有人显式刷、或者程序正常退出时才出去。

`unitbuf` 的差别在程序正常结束时看不出来——退出时所有标准流会各刷一次。

`cin` 与 `cout` 是绑定的（`tie`）：**每次从 `cin` 读之前，先刷一次 `cout`**，
这样提示语会先出现、再等输入。`cerr` 也与 `cout` 绑定，`clog` 没有。

`cin` 与 `cerr` 绑在 `cout` 上、`clog` 没绑——三个取值就在 1.1 的输出里。

| 动作 | 什么时候发生 |
|---|---|
| `std::flush` | 立刻刷这一个流 |
| `std::endl` | 插入换行，再刷一次（等价于 `'\n' << std::flush`） |
| `std::cerr` 的每次插入 | 因为 `unitbuf`，自动刷 |
| `cin` 上的读操作 | 先刷与它绑定的 `cout` |
| 程序正常结束 | 所有标准流各刷一次 |

> [!CAUTION]
> **程序异常结束时，缓冲不会被自动刷出。**
> 关键日志要么走 `std::cerr`（每次插入都刷），要么在写完立刻 `std::flush`。
> 另外说明：本机 Windows 上把 `std::clog` 的内容重定向到文件、再调用 `abort()`，
> 那一行仍然留在了文件里——C 运行库把重定向后的 `stderr` 打开成了无缓冲，
> `clog` 那层缓冲没有机会显形；A 段《06-标准库/A-01-输入输出：stdio.md》第 1.6 小节记录了同一现象。

`std::endl` 里那次「刷」是有代价的，第 8 节给出这个代价的具体数字。

---

# 第 2 节 `<<` 与 `>>`：运算符就是函数调用

## 2.1 链式调用与给自己的类型写一对运算符

`std::cout << a << b` 能成立，是因为前一次调用返回了流本身。
这不是 `<<` 的特殊规则，而是《05-类与面向对象/09-运算符重载.md》第 3.4 小节那条「返回引用才能连续写」在标准库里的应用：

`C++`

```cpp
// （下面是节选）<ostream> 里运算符的声明形式
template <class charT, class traits>
basic_ostream<charT, traits>& operator<<(basic_ostream<charT, traits>& os, const char *s);
template <class charT, class traits>
basic_ostream<charT, traits>& operator<<(basic_ostream<charT, traits>& os, int val);
```

**返回的是 `basic_ostream &`**，所以 `a << b << c` 按左结合先算 `a << b`，得到流再拿它去接 `<< c`。

`>>` 同理，返回 `basic_istream &`，所以 `in >> a >> b` 能连着写；返回值还能直接当条件用：

`文档`

> "`explicit operator bool() const;` Returns: `!fail()`"
>
> —— N4659 §30.5.5.4/1

输出与输入是一对：写出什么格式，就该按什么格式读回来。

`C++`

```cpp
/* point_io.cpp    编译：g++ -std=c++17 point_io.cpp -o point_io */
#include <iostream>
#include <sstream>

struct Point { int x = 0, y = 0; };

// 输出：左操作数是流，所以只能写成非成员函数（见《05-类与面向对象/09-运算符重载.md》第 3.4 小节）
std::ostream &operator<<(std::ostream &os, const Point &p) {
    return os << '(' << p.x << ", " << p.y << ')';
}

// 输入：按同样的格式读回来，格式不对时把流置为 failbit
std::istream &operator>>(std::istream &is, Point &p) {
    char open = 0, comma = 0, close = 0;
    int x = 0, y = 0;
    if (is >> open >> x >> comma >> y >> close && open == '(' && comma == ',' && close == ')') {
        p.x = x;
        p.y = y;
    } else {
        is.setstate(std::ios::failbit);
    }
    return is;
}

int main() {
    const Point a{3, 4};
    std::cout << "直接输出：" << a << "\n";

    std::ostringstream os;                 // 绕一圈字符串流，验证写出去与读回来是对称的
    os << a;
    std::cout << "写进字符串流：" << os.str() << "\n";

    std::istringstream is(os.str());
    Point b;
    is >> b;
    std::cout << "读回来：" << b << "，成功=" << !is.fail() << "\n";

    std::istringstream bad("(3;4)");
    Point c;
    bad >> c;
    std::cout << "格式不对时：fail=" << bad.fail() << "，c 保持原值 " << c << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
直接输出：(3, 4)
写进字符串流：(3, 4)
读回来：(3, 4)，成功=1
格式不对时：fail=1，c 保持原值 (0, 0)
```

**读失败时 `c` 保持原值**，这是自己写 `operator>>` 时要留意的地方：
标准库的算术提取会往目标里写值，自己写的版本则可以做到「失败就不动目标」。
两种做法都有人用，关键是**在文档或注释里说清哪一种**。

## 2.2 类型安全实际意味着什么

「类型安全」这个词在这里有一个很具体的含义：**不会出现格式串与实参对不上的情况。**

`C`

```c
/* printf_type_bug.c    编译：gcc -std=c23 printf_type_bug.c -o printf_type_bug_c
 * 格式串与实参类型不匹配：编译器只给警告，运行结果由调用约定决定 */
#include <stdio.h>

int main(void) {
    const double price = 3.5;
    printf("用 %%d 打印 double：%d\n", price);   // 错的
    printf("用 %%f 打印 double：%f\n", price);   // 对的
    return 0;
}
```

`实测数据`
`Text`

```text
用 %d 打印 double：0
用 %f 打印 double：3.500000
```

**程序照常编译、照常运行、照常退出，输出是一个看起来很像数据的 `0`**——不崩、不报错、结果是错的。加上警告开关才会提示：

`实测数据`
`Text`

```text
gcc -std=c23 -Wall printf_type_bug.c
printf_type_bug.c:7:34: warning: format '%d' expects argument of type 'int',
but argument 2 has type 'double' [-Wformat=]
    7 |     printf("用 %%d 打印 double：%d\n", price);   // 错的
      |                                 ~^     ~~~~~
      |                                  int   double
```

**标准原文的说法是关键**：C 与 C++ 的标准都要求格式与实参匹配，不匹配属于未定义行为；这一次跑出来的 `0`
只是本机这一次的结果，不是「会打印 0」的保证。

`C++`

```cpp
/* printf_type_bug.cpp    编译：g++ -std=c++17 printf_type_bug.cpp -o printf_type_bug_cpp
 * 同一个任务改用 iostream：类型由重载决议决定，不存在「格式串写错」这件事 */
#include <iostream>

int main() {
    const double price = 3.5;
    std::cout << "用 << 打印 double：" << price << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
用 << 打印 double：3.5
```

`-Wall -Wextra` 下零警告。

**这就是本章节要用到的对照点**：`printf` 的错误留到运行期，`iostream` 的错误留在编译期。
A 段《06-标准库/A-01-输入输出：stdio.md》第 2 节说明格式串是一份「手写的类型说明书」，
《06-标准库/A-01-输入输出：stdio.md》第 2.5 小节（与 C++ 的对照）从 C 那一侧做了同一处对照。

**代价也要说清**：`printf` 的格式串可以当数据传递（先拼一个格式串再调用，或者转发 `va_list`），`<<` 做不到——
`std::cout << x` 的类型在编译期就定死了；确实需要「格式在运行期才知道」时，`printf` 这一族（或 C++20 的 `std::format`）更合适。

---

# 第 3 节 `<iomanip>`：格式化

## 3.1 常用件与两个细读点

流对象自己带一批「格式状态」：字段宽度、填充字符、精度、进制、对齐、布尔怎么打印。
`<iomanip>` 提供的是**设定这些状态的辅助函数**，
写成 `os << setw(8)` 这种样子，好处是可以夹在插入序列中间。

`C++`

```cpp
/* manip_demo.cpp    编译：g++ -std=c++17 manip_demo.cpp -o manip_demo */
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    // 用 | 标出字段边界，空格与填充才看得见
    std::cout << "setw(8) 只影响下一个输出：" << '|' << std::setw(8) << 42 << "|\n";
    std::cout << "紧接着再输出一个：" << '|' << 42 << "|\n";

    std::cout << "setfill('*') + setw(8)：" << '|' << std::setfill('*')
              << std::setw(8) << 42 << "|\n";
    std::cout << "setfill 会一直留着：" << '|' << std::setw(6) << 7 << "|\n";

    std::cout << std::setfill(' ');
    std::cout << std::setprecision(6) << "默认精度 6：" << 3.14159265358979 << "\n";
    std::cout << std::setprecision(3) << "setprecision(3)：" << 3.14159265358979 << "\n";
    std::cout << std::fixed << std::setprecision(2) << "fixed + precision(2)：" << 3.14159265358979 << "\n";
    std::cout << std::scientific << std::setprecision(3) << "scientific + precision(3)：" << 12345.6789 << "\n";

    // 同一个数、同一个精度 3，三种格式给出三个结果
    std::cout << std::defaultfloat << std::setprecision(3)
              << "同一个数 defaultfloat(3)：" << 3.14159265358979 << "\n";
    std::cout << std::fixed << "同一个数 fixed(3)：" << 3.14159265358979 << "\n";
    std::cout << std::scientific << "同一个数 scientific(3)：" << 3.14159265358979 << "\n";
    std::cout << std::defaultfloat << std::setprecision(6);

    std::cout << std::hex << "hex 255 = " << 255 << "\n";
    std::cout << std::oct << "oct 255 = " << 255 << "\n";
    std::cout << std::dec << std::showbase << std::hex << "showbase + hex 255 = " << 255 << "\n";
    std::cout << std::noshowbase << std::dec;

    std::cout << std::boolalpha << "boolalpha true = " << true << "\n";
    std::cout << std::noboolalpha << "noboolalpha 1 = " << (1 == 1) << "\n";

    std::cout << "对齐：|" << std::left << std::setw(10) << "左" << "|\n";
    std::cout << "对齐：|" << std::right << std::setw(10) << "右" << "|\n";
    std::cout << "对齐：|" << std::internal << std::showpos << std::setw(10) << -42 << "|\n";
    std::cout << std::noshowpos << std::right << std::dec;

    const std::string s = "带\"引号\"和\\反斜杠";   // 写出与读回成对，含引号的串能原样还原
    std::cout << "quoted 原样：" << s << "\n";
    std::cout << "quoted 转义：" << std::quoted(s) << "\n";
    std::ostringstream os;
    os << std::quoted(s);
    std::istringstream is(os.str());
    std::string back;
    is >> std::quoted(back);
    std::cout << "读回来与原串相同：" << (back == s) << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
setw(8) 只影响下一个输出：|      42|
紧接着再输出一个：|42|
setfill('*') + setw(8)：|******42|
setfill 会一直留着：|*****7|
默认精度 6：3.14159
setprecision(3)：3.14
fixed + precision(2)：3.14
scientific + precision(3)：1.235e+04
同一个数 defaultfloat(3)：3.14
同一个数 fixed(3)：3.142
同一个数 scientific(3)：3.142e+00
hex 255 = ff
oct 255 = 377
showbase + hex 255 = 0xff
boolalpha true = true
noboolalpha 1 = 1
对齐：|左       |
对齐：|       右|
对齐：|-       42|
quoted 原样：带"引号"和\反斜杠
quoted 转义："带\"引号\"和\\反斜杠"
读回来与原串相同：1
```

**格式状态的规律大半在这段输出里**：`setw` 只管一次、`setfill` 会留下、`fixed` 与 `scientific`
会改变精度的含义（一个是小数位数，一个是有效位数）、`showbase` 只对非十进制起作用。

标准写得很直白：`setw(n)` 做的事情就是把流的宽度设成 `n`，
而这个宽度会在下一次输出之后被清掉。

`文档`

> "`unspecified setw(int n);` … Returns: An object of unspecified type such that
> the expression `out << setw(n)` behaves as if it called `f(out, n)`, where the
> function `f` is defined as: `void f(ios_base& str, int n) { str.width(n); }`"
>
> —— N4659 §30.7.6/7

**宽度是一个「一次性」的状态**，因为它是为「对齐一列数据」设计的。
标准在格式化输出的要求里写明了宽度用完就被清零：

`文档`

> "… then enough fill characters are added to the sequence at the position
> indicated for padding to bring the length of the sequence to `str.width()`.
> `str.width(0)` is called."
>
> —— N4659 §25.4.2.2.2

**因此对齐一列数据时，每次输出前都要重新写一遍 `setw`**；同样的规则也适用于输入：`in >> setw(5) >> word` 限制这次最多读 5 个字符。

`setprecision(n)` 做的事就是 `str.precision(n)`，但它的**含义取决于浮点格式**。
同一个数 `3.14159265358979`、同一个精度 3，三种格式给出三个不同的结果：
`实测数据`
| 当前格式 | `setprecision(3)` 的含义 | 跑出来的结果 |
|---|---|---|
| 默认（`defaultfloat`） | 有效数字位数 | `3.14` |
| `fixed` | 小数点后的位数 | `3.142` |
| `scientific` | 小数点后的位数 | `3.142e+00` |

**应当先确定要哪一种，再设精度**。默认精度是 6，这一点写在 `basic_ios::init()` 的后置条件表里：

`文档`

> "Table 113 — `basic_ios::init()` effects … `width()` 0 … `precision()` 6 …
> `fill()` `widen(' ')` … `flags()` `skipws | dec`"
>
> —— N4659 §30.5.5.2 表 113

**这也解释了 `setw` 与 `setprecision` 的一个区别**：宽度被清零是每次输出后的事，
精度则是「设了就一直有效」，不会自己恢复。

> [!WARNING]
> **`fixed` 与 `scientific` 是会留下来的状态。**
> 一旦在某个流上设了，后面所有浮点输出都按它来。
> 想恢复默认要显式写 `std::defaultfloat`（C++11 起）或清掉 `floatfield` 位。
> 实测里 `scientific` 那一行之后若不处理，后面的输出全会变成科学计数法。

## 3.2 写错了会怎样：忘了 `<iomanip>`

`setw` 这类名字**不在 `<iostream>` 里**。

`C++`

```cpp
/* no_iomanip.cpp    编译：g++ -std=c++17 no_iomanip.cpp -o no_iomanip （失败） */
#include <iostream>        // 只包含 <iostream>，没有 <iomanip>

int main() {
    std::cout << std::setw(8) << 42 << "\n";   // 期望：报错，setw 不在 <iostream> 里
    return 0;
}
```

`实测数据`
`Text`

```text
no_iomanip.cpp:5:23: error: 'setw' is not a member of 'std'
    5 |     std::cout << std::setw(8) << 42 << "\n";
      |                       ^~~~
no_iomanip.cpp:3:1: note: 'std::setw' is defined in header '<iomanip>';
this is probably fixable by adding '#include <iomanip>'
    2 | #include <iostream>
  +++ |+#include <iomanip>
```

**GCC 15 直接把该加哪一行都写出来了**，这类提示比查文档快。

**为什么 `hex`、`boolalpha` 不报错**：它们是 `ios_base` 的成员函数（或自由函数），
声明在 `<ios>` 里，而 `<iostream>` 包含了 `<ios>`。
`setw`、`setfill`、`setprecision`、`quoted` 才需要 `<iomanip>`。

## 3.3 `<iomanip>` 里的其余件

`文档`

> "`T7 get_money(moneyT& mon, bool intl = false);` …
> `T9 get_time(struct tm* tmb, const charT* fmt);` …
> `T10 put_time(const struct tm* tmb, const charT* fmt);`"
>
> —— N4659 §30.7.3 [iomanip.syn]

| 件 | 用途 | 说明 |
|---|---|---|
| `setbase` | 进制 | 只接受 8、10、16 |
| `showbase` / `showpos` / `boolalpha` | 前缀、正号、布尔文本 | `boolalpha` 默认把 `bool` 打印成 `1`/`0` |
| `setiosflags` / `resetiosflags` | 按位设置与清除格式标志 | 标志名是 `std::ios::*` |
| `put_money` / `get_money` | 金额的本地化输入输出 | 依赖 locale，实际项目里用得少 |
| `put_time` / `get_time` | 按 `strftime` 风格格式化时间 | 与《06-标准库/B-06-时间：chrono.md》比较着看 |
| `quoted` | 带引号与转义的字符串 | C++14 起 |

---

# 第 4 节 流的状态

## 4.1 四个状态位

**每个流对象内部都有一组状态位**，出错时置位，检查时读它。
`<iostream>` 里的一切「*失败了怎么办*」都建立在这组位上。

`文档`

> "`iostate rdstate() const;` Returns: The error state of the stream buffer.
> `void clear(iostate state = goodbit);` … `void setstate(iostate state);`
> Effects: Calls `clear(rdstate() | state)`. `bool good() const;` Returns:
> `rdstate() == 0`. `bool eof() const;` Returns: `true` if `eofbit` is set in
> `rdstate()`. `bool fail() const;` Returns: `true` if `failbit` or `badbit`
> is set in `rdstate()`. `bool bad() const;` Returns: `true` if `badbit` is
> set in `rdstate()`."
>
> —— N4659 §30.5.5.4/3、4、6、7、8、9、10

**七条信息可以归成一张表**：

| 名字 | 什么时候置位 | 能不能恢复 |
|---|---|---|
| `goodbit` | 值为 0，一切正常 | — |
| `eofbit` | 读到输入末尾 | 可以，但通常表示「读完了」 |
| `failbit` | 格式不匹配（该读数字却读到字母） | 可以：`clear()` 加跳过坏字符 |
| `badbit` | 流缓冲出了 I/O 层的问题（读不出、写不进） | 一般不可恢复；`fail()` 把 `badbit` 也算进去 |

**`fail()` 把 `badbit` 也算进去**，标准里对这一点有一句注：
「Checking `badbit` also for `fail()` is historical practice.」

**`operator bool()` 等价于 `!fail()`**，所以 `while (std::cin >> x)` 这类写法是在问「上一轮有没有出错」。

`C++`

```cpp
/* state_bits.cpp    编译：g++ -std=c++17 state_bits.cpp -o state_bits */
#include <iostream>
#include <sstream>
#include <string>

// 把 rdstate() 的几个位翻译成人看得懂的样子
static void show(const char *what, const std::ios &st) {
    std::cout << what << "：good=" << st.good() << " eof=" << st.eof()
              << " fail=" << st.fail() << " bad=" << st.bad() << " rdstate=" << st.rdstate() << "\n";
}

int main() {
    std::istringstream in("abc 12 34");
    int n = -1;

    in >> n;                               // 前三个字符不是数字
    std::cout << "读 int 失败后 n = " << n << "（标准要求写 0）\n";
    show("失败后", in);

    std::string word;
    in >> word;                            // 失败状态下再读，什么也读不到
    std::cout << "失败后再读 word = [" << word << "]\n";

    in.clear();                            // 只清状态位，不跳过「有毒」的字符
    in >> word;
    std::cout << "clear() 之后读到 word = [" << word << "]\n";
    show("读走 abc 之后", in);
    return 0;
}
```

`实测数据`
`Text`

```text
读 int 失败后 n = 0（标准要求写 0）
失败后：good=0 eof=0 fail=1 bad=0 rdstate=4
失败后再读 word = []
clear() 之后读到 word = [abc]
读走 abc 之后：good=1 eof=0 fail=0 bad=0 rdstate=0
```

**几处数字值得注意**：`failbit` 的位值是 4，`eofbit` 是 2，`badbit` 是 1，
`rdstate()` 直接打印出来就是这几个数的或（上面第二行打印出来的就是 4）；失败的那次提取**把 0 写进了 `n`**，原来的 `-1` 没了。

`文档`

> "The resultant numeric value is stored in `val`. If the conversion function
> does not convert the entire field, or if the field represents a value outside
> the range of representable values, `ios_base::failbit` is assigned to `err`."
>
> —— N4659 §25.4.2.1.2/3

## 4.2 坏流：原地转圈与恢复的两步

`failbit` 置位之后，流会一直「坏」着，直到有人显式清掉它。**坏流上的每一次提取都立刻失败，
并且不消耗任何字符**——如果循环条件写得不对，就是死循环。

`C++`

```cpp
/* state_loop.cpp    编译：g++ -std=c++17 state_loop.cpp -o state_loop */
#include <iostream>
#include <sstream>

int main() {
    std::istringstream in("10 20 x 30 40");    // 输入里混了一个不是数字的词

    for (int round = 1; round <= 5; ++round) {
        int v = -1;
        in >> v;
        std::cout << "第 " << round << " 轮：v = " << v << "，good=" << in.good() << "\n";
        if (in.fail()) {
            std::cout << "  流已经坏了，不 clear 也不 ignore 的话，下一轮读到的还是同一个 x\n";
        }
    }
    return 0;
}
```

`实测数据`
`Text`

```text
第 1 轮：v = 10，good=1
第 2 轮：v = 20，good=1
第 3 轮：v = 0，good=0
  流已经坏了，不 clear 也不 ignore 的话，下一轮读到的还是同一个 x
第 4 轮：v = -1，good=0
  流已经坏了，不 clear 也不 ignore 的话，下一轮读到的还是同一个 x
第 5 轮：v = -1，good=0
  流已经坏了，不 clear 也不 ignore 的话，下一轮读到的还是同一个 x
```

**第 3 轮把 `v` 写成了 0；从第 4 轮起连写都不写了**，
`v` 保持上一轮的值。真实的程序里，这一串输出会一直打印下去。

恢复要两步，缺一不可：**先把状态位清掉，再把「有毒」的字符从流里拿走。**

`C++`

```cpp
/* state_recover.cpp    编译：g++ -std=c++17 state_recover.cpp -o state_recover */
#include <iostream>
#include <limits>
#include <sstream>

int main() {
    std::istringstream in("10 20 x 30 40");
    int sum = 0, v = 0;

    while (in >> v) sum += v;              // 读到坏字符就跳出
    std::cout << "跳过坏字符之前读到：" << sum << "\n";

    in.clear();                            // ① 清状态位
    in.ignore(std::numeric_limits<std::streamsize>::max(), ' ');   // ② 丢掉坏字符
    while (in >> v) sum += v;
    std::cout << "跳过坏字符之后读到：" << sum << "\n";
    std::cout << "流的状态：good=" << in.good() << " eof=" << in.eof() << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
跳过坏字符之前读到：30
跳过坏字符之后读到：100
流的状态：good=0 eof=1
```

**`ignore` 的两个参数**：最大丢弃字符数（`numeric_limits<streamsize>::max()` 表示不设上限）
与终止字符（会被一起丢掉）。上例中 `ignore(..., ' ')` 丢掉的是 `x` 和它后面的空格。

> [!WARNING]
> **只 `clear()` 不 `ignore()` 是最常见的半吊子恢复**：状态位清了，下一次提取又会在同一个字符上失败。
> 反过来只 `ignore()` 不 `clear()` 也没用——坏流上的提取会被直接拒绝，`ignore` 也读不到东西。

## 4.3 让流抛异常

默认情况下这些状态位只是被「置位」，不抛异常。
想让它们抛，用 `exceptions()`：

`C++`

```cpp
/* state_except.cpp    编译：g++ -std=c++17 state_except.cpp -o state_except */
#include <iostream>
#include <sstream>

int main() {
    std::istringstream in("abc");
    in.exceptions(std::ios::failbit);      // 让 failbit 直接抛异常

    try {
        int v = 0;
        in >> v;                           // 这里会抛
    } catch (const std::ios_base::failure &e) {
        std::cout << "捕获到 ios_base::failure\n";
        std::cout << "what() = " << e.what() << "\n";
        std::cout << "code().message() = " << e.code().message() << "\n";
    }

    std::cout << "抛过之后流的 rdstate = " << in.rdstate() << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
捕获到 ios_base::failure
what() = basic_ios::clear: iostream error
code().message() = iostream error
抛过之后流的 rdstate = 4
```

**几点实用信息**：异常的 `what()` 是 `basic_ios::clear: iostream error`，不带「第几行读什么失败了」
这类位置信息，想知道哪里出错须自行记录上下文；异常类型 `std::ios_base::failure` 从 C++11 起带一个 `code()`。

## 4.4 `badbit` 什么时候出现

`failbit` 是「这次转换没成功」，`badbit` 是「这条流本身坏了」。
用一个写不进去的缓冲就能造出来：

`C++`

```cpp
/* state_bad.cpp    编译：g++ -std=c++17 state_bad.cpp -o state_bad */
#include <iostream>
#include <streambuf>

// 一个「写不进去」的缓冲：每次要写字符都返回 eof
struct BrokenBuf : std::streambuf {
    int overflow(int) override { return traits_type::eof(); }
};

int main() {
    BrokenBuf broken;
    std::ostream out(&broken);

    out << "x";                            // 写不进去
    std::cout << std::boolalpha << "写一次之后：good=" << out.good() << " fail=" << out.fail()
              << " bad=" << out.bad() << " rdstate=" << out.rdstate() << "\n";

    out << "y";                            // 流已坏，这次什么也不做
    std::cout << "再写一次之后：rdstate=" << out.rdstate() << "\n";

    out.clear();                           // 只清状态位，缓冲还是坏的
    out << "z";
    std::cout << "clear() 之后再写：rdstate=" << out.rdstate() << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
写一次之后：good=false fail=true bad=true rdstate=1
再写一次之后：rdstate=5
clear() 之后再写：rdstate=1
```

**这段输出的过程是**：第一次写把 `badbit` 置上（`rdstate=1`）；
第二次写的时候，`sentry` 发现流不是 `good`，又置上 `failbit`（`5 = 1 | 4`）；
`clear()` 之后重写，缓冲再次失败，`badbit` 又回来了。

**`badbit` 一般意味着「这条流不能用了」**：写文件时磁盘满、读文件时设备出错都会走到这里；与 `failbit` 不同，
清掉状态位并不能修好底下的问题。

---

# 第 5 节 `getline` 与整行读取

| 写法 | 读什么 | 换行怎么处理 |
|---|---|---|
| `in >> s` | 跳过前导空白，读到下一个空白为止 | 换行留在流里 |
| `std::getline(in, s)` | 从当前位置一直读到换行 | 换行被取走并丢掉 |
| `std::getline(in, s, ';')` | 读到指定分隔符 | 分隔符被取走并丢掉 |

**`>>` 会跳过空白，`getline` 不会**——这是两者最容易出问题的地方。

## 5.1 写错了会怎样：换行残留与 `getline` 的失败条件

先读一个数、再读一整行，是最常见的组合。输入文件 `in.txt` 的内容是两行：

`Text`

```text
3
hello world
```

`C++`

```cpp
/* getline_bad.cpp    编译：g++ -std=c++17 getline_bad.cpp -o getline_bad
 * 输入文件 in.txt 两行："3" 与 "hello world"，运行：getline_bad.exe < in.txt */
#include <iostream>
#include <string>

int main() {
    int n = 0;
    std::cin >> n;                         // 只取走数字，行尾的换行留在流里
    std::string line;
    std::getline(std::cin, line);          // 读到的就是这个换行
    std::cout << "n = " << n << "\n";
    std::cout << "line = [" << line << "]（长度 " << line.size() << "）\n";
    return 0;
}
```

`实测数据`
`Text`

```text
n = 3
line = []（长度 0）
```

**`getline` 读到了一个空行**，因为 `>>` 把 `3` 之后的换行留在了流里，
`getline` 立刻遇到换行，于是返回一个空串。第二行 `hello world` 根本没被碰。
同一个现象在 C 那边叫「换行符会留到下一次读」，
见《06-标准库/A-01-输入输出：stdio.md》第 3.4 小节。

修法是**在读之前把本行剩下的内容丢掉**：

`C++`

```cpp
/* getline_fix.cpp    编译：g++ -std=c++17 getline_fix.cpp -o getline_fix
 * 输入文件 in.txt 两行："3" 与 "hello world"，运行：getline_fix.exe < in.txt */
#include <iostream>
#include <limits>
#include <string>

int main() {
    int n = 0;
    std::cin >> n;
    // 丢掉本行剩下的全部字符（含换行），上限用 streamsize 的最大值表示「不设上限」
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::string line;
    std::getline(std::cin, line);
    std::cout << "n = " << n << "\n";
    std::cout << "line = [" << line << "]（长度 " << line.size() << "）\n";
    return 0;
}
```

`实测数据`
`Text`

```text
n = 3
line = [hello world]（长度 11）
```

> [!TIP]
> **两个惯用写法**：混用时固定写一行 `std::cin.ignore(...)`；或者**统一用 `getline` 读整行，再用 `std::istringstream` 解析这一行**——后者把「行」与「字段」两层分开，出问题时容易定位（见第 6 节）。

`getline` 返回流本身，所以可以直接当条件用。它的失败条件写在标准里：

`文档`

> "Effects: Behaves as an unformatted input function … calls `str.erase()` and
> then extracts characters from `is` and appends them to `str` … until any of
> the following occurs: —(6.1) end-of-file occurs on the input sequence (in
> which case, the `getline` function calls `is.setstate(ios_base::eofbit)`).
> —(6.2) `traits::eq(c, delim)` for the next available input character `c` (in
> which case, `c` is extracted but not appended) … If the function extracts no
> characters, it calls `is.setstate(ios_base::failbit)`."
>
> —— N4659 §24.3.3.9/6、8

**两句关键的**：读到文件末尾时置 `eofbit`，若这一行还有内容，**内容仍然算数**；
**一个字符都没读到就失败**（例如流已经是空的），这时置 `failbit`。

这解释了 `while (std::getline(in, line))` 为什么是正确的循环：
最后一行读完之后，下一次调用读到 0 个字符，`failbit` 置位，循环退出。

**还有一个容易忽略的细节**：文件最后一行若没有换行符，`getline` 也会把它读出来
（读到 EOF，置 `eofbit` 但不置 `failbit`），这一行不会被漏掉。

## 5.2 逐行读一个文件

`C++`

```cpp
/* fstream_text.cpp    编译：g++ -std=c++17 fstream_text.cpp -o fstream_text */
#include <fstream>
#include <iostream>
#include <string>

int main() {
    const char *name = "text_out.txt";

    {   // 写：默认模式就是 out|trunc，离开作用域即关闭
        std::ofstream out(name);
        std::cout << "打开成功吗：" << out.is_open() << "\n";
        out << "第一行\n" << "第二行\n";
    }

    {   // 读：默认模式是 in
        std::ifstream in(name);
        std::string line;
        int n = 0;
        while (std::getline(in, line)) {
            ++n;
            std::cout << "第 " << n << " 行：" << line << "（" << line.size() << " 字节）\n";
        }
    }

    {   // 打开一个不存在的文件：is_open() 为假，后面所有操作都无效
        std::ifstream missing("no_such_file_here.txt");
        std::cout << "打开不存在的文件：is_open=" << missing.is_open()
                  << " fail=" << missing.fail() << "\n";
    }
    return 0;
}
```

`实测数据`
`Text`

```text
打开成功吗：1
第 1 行：第一行（9 字节）
第 2 行：第二行（9 字节）
打开不存在的文件：is_open=0 fail=1
```

**两处细节**：`line.size()` 是 9——「第一行」三个汉字各占 3 字节（UTF-8），不是 3；
文本模式下每个换行在磁盘上是 2 字节（CRLF）。编码与字节数的问题在
《06-标准库/B-02-std-string 与 string_view.md》第 8 节展开。

---

# 第 6 节 `<sstream>`：把字符串当成流

## 6.1 解析一行

`sstream` 的三个类型都有「内存里的缓冲」，接口与文件流完全一样：
`istringstream` 读、`ostringstream` 写、`stringstream` 读写。
**它们最大的用处是把「读一行」与「解析这一行」分成两步。**

`C++`

```cpp
/* sstream_demo.cpp    编译：g++ -std=c++17 sstream_demo.cpp -o sstream_demo */
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    // 典型用法一：把一行文本按分隔符拆开
    std::istringstream fields("x=12;y=34;name=abc");
    std::string item;
    while (std::getline(fields, item, ';')) {          // 以 ';' 为界逐段取
        const std::size_t eq = item.find('=');
        if (eq == std::string::npos) continue;
        std::cout << "键[" << item.substr(0, eq) << "] 值[" << item.substr(eq + 1) << "]\n";
    }

    // 典型用法二：拼字符串，不必先算长度
    std::ostringstream report;
    report << "共 " << 3 << " 项，合计 " << std::fixed << std::setprecision(2) << 46.0 << "\n";
    std::cout << "拼接结果：" << report.str();

    // 复用陷阱：读到结尾的流要先 clear() 才能再用
    std::istringstream reuse("7 8");
    int a = 0, b = 0, c = 0;
    reuse >> a >> b >> c;                              // 已经到结尾，这一下设置 eofbit
    std::cout << "读完两个数后 c = " << c << "，fail=" << reuse.fail() << "\n";
    reuse.clear();                                     // 清掉 eofbit
    reuse.str("9");                                    // 再换一份内容
    reuse >> c;
    std::cout << "clear() + str() 之后 c = " << c << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
键[x] 值[12]
键[y] 值[34]
键[name] 值[abc]
拼接结果：共 3 项，合计 46.00
读完两个数后 c = 0，fail=1
clear() + str() 之后 c = 9
```

**第一段的写法值得注意**：外层 `getline(fields, item, ';')` 按分隔符切段，每段内部再用 `find('=')` 定位——比用一串 `>>` 硬拼格式稳健得多。

**复用同一个 `istringstream` 时，顺序是 `clear()` 再 `str(...)`**，反过来仍然是坏流：
上面那份输出里，读到结尾之后第三次提取就失败了（`fail=1`），`clear()` 加 `str("9")` 之后才接着读出了 9。

## 6.2 拼一段文本

**与 `snprintf` 的对照**：A 段《06-标准库/A-01-输入输出：stdio.md》第 2 节讲过 `snprintf` 的返回值与截断问题——要写对
「先算长度、再决定缓冲区」这套流程，须处理返回值、处理截断、处理二次调用时内容变化；`ostringstream` 把这套流程收进了流内部（见上面程序的「典型用法二」）。

---

# 第 7 节 `<fstream>`：文件

## 7.1 三个类型与打开模式

| 类型或模式位 | 含义 |
|---|---|
| `std::ifstream` | 只读，默认模式是 `in` |
| `std::ofstream` | 只写，默认模式是 `out \| trunc`（文件存在就清空） |
| `std::fstream` | 读写同一个文件，默认 `in \| out` |
| `std::ios::in` / `out` | 可读 / 可写 |
| `std::ios::trunc` | 打开时清空文件 |
| `std::ios::ate` | 打开后立刻定位到末尾（**不清空**，之后还能 `seekp` 回去改写） |
| `std::ios::binary` | 二进制模式，不做换行翻译 |

`C++`

```cpp
/* fstream_binary.cpp    编译：g++ -std=c++17 fstream_binary.cpp -o fstream_binary */
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

// 把文件按字节打印出来：换行显示成 \r \n，不可打印的显示成 <十六进制>
static void dump(const char *name) {
    std::ifstream in(name, std::ios::binary | std::ios::ate);
    const std::streamoff n = in.tellg();
    in.seekg(0);
    std::string data(static_cast<std::size_t>(n), '\0');
    in.read(&data[0], n);
    std::cout << name << "：" << n << " 字节  内容 = ";
    for (unsigned char c : data) {
        if (c == '\r' || c == '\n')  std::cout << (c == '\r' ? "\\r" : "\\n");
        else if (c >= 32 && c < 127) std::cout << c;
        else std::cout << "<" << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<int>(c) << std::dec << ">";
    }
    std::cout << "\n";
}

int main() {
    {   // 文本模式：Windows 上 \n 会翻译成 \r\n；二进制模式写什么就是什么
        std::ofstream t("t_mode.txt");
        t << "A\n";
        std::ofstream b("b_mode.txt", std::ios::binary);
        b << "A\n";
    }
    dump("t_mode.txt");
    dump("b_mode.txt");

    {   // 二进制写一个整数：落盘顺序就是内存里的字节序
        const std::int32_t v = 0x01020304;
        std::ofstream f("num.bin", std::ios::binary);
        f.write(reinterpret_cast<const char *>(&v), sizeof v);
    }
    dump("num.bin");

    {   // 文本模式按行读二进制文件：遇到 0x1A 会让读操作停下来
        std::ofstream f("sub.bin", std::ios::binary);
        const char bytes[] = {'A', '\x1a', 'B', '\n'};
        f.write(bytes, 4);
    }
    {
        std::ifstream f("sub.bin");              // 忘了 std::ios::binary
        std::string line;
        std::getline(f, line);
        std::cout << "文本模式读到的长度：" << line.size() << "（文件里是 4 字节）\n";
    }
    {
        std::ifstream f("sub.bin", std::ios::binary);
        std::string line;
        std::getline(f, line);
        std::cout << "二进制模式读到的长度：" << line.size() << "\n";
    }
    return 0;
}
```

`实测数据`
`Text`

```text
t_mode.txt：3 字节  内容 = A\r\n
b_mode.txt：2 字节  内容 = A\n
num.bin：4 字节  内容 = <04><03><02><01>
文本模式读到的长度：1（文件里是 4 字节）
二进制模式读到的长度：3
```

**两条 Windows 特有的结论**：文本模式会翻译换行，写一个 `\n` 得到两个字节 `\r\n`；
文本模式下 `0x1A` 会被当作文件结束（它源自 DOS 的 Ctrl+Z 约定），读到的长度只有 1，而二进制模式下是 3。

换到 Linux 这两条都不成立，**所以「二进制数据一律加 `std::ios::binary`」是跨平台代码的固定动作**。

## 7.2 同一个任务：C 的 `fopen` 与 C++ 的 `fstream`

两个版本做同一件事：写三行、读回来打印行号。

`C`

```c
/* file_task.c    编译：gcc -std=c23 file_task.c -o file_task_c */
#include <stdio.h>

int main(void) {
    const char *name = "c_version.txt";

    FILE *out = fopen(name, "w");                 // 写模式：文件不存在就创建
    if (out == NULL) { perror("fopen"); return 1; }   // C 里必须逐次检查
    fprintf(out, "第一行\n第二行\n第三行\n");
    fclose(out);

    FILE *in = fopen(name, "r");
    if (in == NULL) { perror("fopen"); return 1; }
    char line[256];
    int n = 0;
    while (fgets(line, sizeof line, in) != NULL) {    // 读一行；line 里已经带着换行
        ++n;
        printf("第 %d 行：%s", n, line);
    }
    fclose(in);
    return 0;
}
```

`C++`

```cpp
/* file_task.cpp    编译：g++ -std=c++17 file_task.cpp -o file_task_cpp */
#include <fstream>
#include <iostream>
#include <string>

int main() {
    const char *name = "cpp_version.txt";

    {
        std::ofstream out(name);                  // 打开失败不会抛，靠状态位
        if (!out) { std::cerr << "打开失败：" << name << "\n"; return 1; }
        out << "第一行\n" << "第二行\n" << "第三行\n";
    }                                             // 离开作用域自动关闭

    {
        std::ifstream in(name);
        if (!in) { std::cerr << "打开失败：" << name << "\n"; return 1; }
        std::string line;
        int n = 0;
        while (std::getline(in, line)) {          // 换行不进入 line
            ++n;
            std::cout << "第 " << n << " 行：" << line << "\n";
        }
    }
    return 0;
}
```

`实测数据`
`Text`

```text
第 1 行：第一行
第 2 行：第二行
第 3 行：第三行
```

两版输出逐字节相同（都是 69 字节），写出的两个文件也逐字节相同（33 字节）。**差别不在结果上，在几处细节里**：

| 细节 | C（`fopen`） | C++（`fstream`） |
|---|---|---|
| 关闭 | 必须记得 `fclose` | 析构函数自动关（RAII） |
| 失败处理 | 每次调用都要判 `NULL` | 打开后判 `is_open()`／`!in`，之后靠状态位 |
| 一行读进来 | `fgets` 带换行，缓冲区长度要自己定 | `getline` 不带走换行，长度自动增长 |
| 一行读进来的类型 | `char[256]` 固定长度，长行会被截成两段 | `std::string`，多长都行 |
| 错误详情 | `errno` + `perror` | 状态位 + `std::error_code`（`fstream` 不带 `errno` 的细节） |

**第 4 行值得注意**：`fgets` 遇到超过缓冲区的行会分两次返回，读出来的两段各自像一行；`getline` 没有这个问题。
这是从 C 转 C++ 时最值钱的一处替换。

## 7.3 两条错误处理路线

`fstream` 默认不抛异常，与 C 一样靠「检查返回值／状态位」。
但它也支持异常开关：

`C++`

```cpp
/* fstream_error.cpp    编译：g++ -std=c++17 fstream_error.cpp -o fstream_error */
#include <fstream>
#include <iostream>
#include <string>

int main() {
    {   // 路线一：状态位。打开失败不抛异常，靠 is_open() / fail() 判断
        std::ifstream in("no_such_file.txt");
        std::cout << "路线一：is_open = " << in.is_open() << "，fail = " << in.fail() << "\n";
        std::string line;
        std::getline(in, line);            // 对坏流读，什么也不发生
        std::cout << "  对坏流调用 getline 之后：读到 " << line.size()
                  << " 字节，fail = " << in.fail() << "\n";
    }

    {   // 路线二：让流在失败时抛异常
        std::ifstream in;
        in.exceptions(std::ios::failbit);  // 打开失败就会抛
        try {
            in.open("no_such_file.txt");
        } catch (const std::ios_base::failure &e) {
            std::cout << "路线二：捕获到 " << e.what() << "\n";
        }
    }
    return 0;
}
```

`实测数据`
`Text`

```text
路线一：is_open = 0，fail = 1
  对坏流调用 getline 之后：读到 0 字节，fail = 1
路线二：捕获到 basic_ios::clear: iostream error
```

**路线一的风险在于「忘了检查」**：不检查的话，后面每个操作都是静默的空转，最后拿到一份空结果，
还找不到是哪一步出的问题。**路线二的诱惑在于「一抛了之」**，但 `what()` 里没有文件名。

> [!TIP]
> **实际项目里的折中**：打开文件时**显式检查一次**并打印文件名、路径与原因，打开之后的读写出错才交给状态位或异常——
> 打开是一次性的动作，检查成本低、信息量大；读写是高频动作，逐次检查不现实。

---

# 第 8 节 与 `printf` 的取舍

## 8.1 同一任务的两种写法

同一张表，用两种方式打印。两份程序在同一个目录下各自编译运行。

`C`

```c
/* same_task.c    编译：gcc -std=c23 same_task.c -o same_task_c */
#include <stdio.h>

int main(void) {
    const char *name[3] = {"bearing", "gear", "motor"};
    const int    qty[3]  = {12, 3, 1450};
    const double price[3] = {8.5, 120.0, 3.25};

    printf("%-8s %6s %10s %12s\n", "name", "qty", "price", "amount");
    double total = 0.0;
    for (int i = 0; i < 3; ++i) {
        const double amount = qty[i] * price[i];
        total += amount;
        printf("%-8s %6d %10.2f %12.2f\n", name[i], qty[i], price[i], amount);
    }
    printf("%-8s %6s %10s %12.2f\n", "total", "", "", total);
    return 0;
}
```

`C++`

```cpp
/* same_task.cpp    编译：g++ -std=c++17 same_task.cpp -o same_task_cpp */
#include <iomanip>
#include <iostream>
#include <string>

int main() {
    const std::string name[3] = {"bearing", "gear", "motor"};
    const int    qty[3]   = {12, 3, 1450};
    const double price[3] = {8.5, 120.0, 3.25};

    // 字段之间补一个空格，与 C 版格式串里的空格对齐
    std::cout << std::left << std::setw(8) << "name" << ' '
              << std::right << std::setw(6) << "qty" << ' ' << std::setw(10) << "price" << ' '
              << std::setw(12) << "amount" << "\n";
    double total = 0.0;
    std::cout << std::fixed << std::setprecision(2);
    for (int i = 0; i < 3; ++i) {
        const double amount = qty[i] * price[i];
        total += amount;
        std::cout << std::left << std::setw(8) << name[i] << ' '
                  << std::right << std::setw(6) << qty[i] << ' ' << std::setw(10) << price[i] << ' '
                  << std::setw(12) << amount << "\n";
    }
    std::cout << std::left << std::setw(8) << "total" << ' '
              << std::right << std::setw(6) << "" << ' ' << std::setw(10) << "" << ' '
              << std::setw(12) << total << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
name        qty      price       amount
bearing      12       8.50       102.00
gear          3     120.00       360.00
motor      1450       3.25      4712.50
total                           5174.50
```

两版输出**逐字节相同**（都是 205 字节）：**两种写法的表达能力是等价的**，差别在写法、可扩展性与运行期开销上。

| 对照项 | `printf` | `iostream` |
|---|---|---|
| 格式与实参的匹配 | 运行期，由调用者保证 | 编译期，由重载决议决定 |
| 自定义类型 | 做不到（除非转成字符串再打印） | 写一个 `operator<<` 即可 |
| 格式在运行期才知道 | 可以：格式串是普通字符串 | 不可以：类型在编译期定死 |
| 与 C 代码混用 | 天然是 C 的接口 | 默认与 C 同步，关掉同步后顺序不再保证（见《06-标准库/B-00-导读：C++ 标准库与 C 的关系.md》第 3.4 小节） |

## 8.2 吞吐实测

**bench_io.cpp**（见附录 A）写 20 万行整数，每种写法连续跑三次。
度量用 `std::chrono::steady_clock`（见《06-标准库/B-06-时间：chrono.md》第 3 节）。

`C++`

```cpp
/* bench_io.cpp    编译：g++ -std=c++17 -O2 bench_io.cpp -o bench_io
 * 运行：bench_io.exe files                  写文件：printf 与 ofstream 对照
 *       bench_io.exe cout_sync  > data.txt  写标准输出（默认同步）
 *       bench_io.exe cout_nosync > data.txt 写标准输出（关掉同步） */
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

static const int N = 200000;                      // 每个模式写 20 万行
using Clock = std::chrono::steady_clock;

template <class F>
static void report(const char *name, int rep, F f) {
    const auto t0 = Clock::now();
    f();
    const double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    std::fprintf(stderr, "%-14s 第 %d 次：%8.1f ms\n", name, rep, ms);
}

int main(int argc, char **argv) {
    const std::string mode = (argc > 1) ? argv[1] : "files";

    for (int rep = 1; rep <= 3; ++rep) {
        if (mode == "files") {
            report("printf", rep, [] { FILE *f = std::fopen("bench_c.txt", "w");
                for (int i = 0; i < N; ++i) std::fprintf(f, "%d\n", i);
                std::fclose(f); });
            report("ofstream", rep, [] { std::ofstream f("bench_cpp.txt");
                for (int i = 0; i < N; ++i) f << i << '\n'; });
            report("ofstream+endl", rep, [] { std::ofstream f("bench_endl.txt");
                for (int i = 0; i < N; ++i) f << i << std::endl; });   // 每行刷一次
        } else if (mode == "cout_sync") {
            report("cout(同步)", rep, [] { for (int i = 0; i < N; ++i) std::cout << i << '\n'; });
        } else if (mode == "cout_nosync") {
            std::ios::sync_with_stdio(false);                          // 必须在任何 I/O 之前
            report("cout(不同步)", rep, [] { for (int i = 0; i < N; ++i) std::cout << i << '\n'; });
        }
    }
    return 0;
}
```

**环境**：Windows 侧 `g++` 15.2.0（MinGW-w64），Linux 侧 `g++` 13.3.0（WSL Ubuntu 24.04），两侧都加 `-O2`，
都写到本地磁盘上的普通文件，`cout` 的两个模式把标准输出重定向到文件。**同一组数字来自同一套工具链**。

`实测数据`

| 写法（20 万行） | Windows 三次（ms） | Linux 三次（ms） |
|---|---|---|
| `fprintf` 到 `FILE *` | 23.9 / 21.4 / 21.3 | 6.1 / 7.9 / 6.4 |
| `ofstream << i << '\n'` | 7.9 / 6.6 / 7.4 | 4.6 / 7.7 / 4.8 |
| `ofstream << i << std::endl` | 334.7 / 325.2 / 310.0 | 126.9 / 110.4 / 124.4 |
| `cout`（默认同步） | 14.0 / 11.3 / 11.0 | 6.5 / 4.9 / 5.0 |
| `cout`（关掉同步） | 6.4 / 5.9 / 6.1 | 6.7 / 4.7 / 4.5 |

三个文件的内容与字节数完全一致：每个 1,488,890 字节。

**第一，`std::endl` 的代价是数量级的。** Windows 上 310 到 335 毫秒，换成 `'\n'` 是 6.6 到 7.9 毫秒，**差 40 倍以上**。
原因不复杂：`std::endl` 每写一行就刷一次缓冲，20 万次刷新就是 20 万次「把缓冲交给操作系统」的动作；
`'\n'` 只在缓冲满时才交一次。

**第二，`printf` 在这一组里没有更快。** 两种平台上 `ofstream` 都比 `fprintf` 快一到三倍，这与「`printf` 比 `iostream` 快」
的流传说法相反：`fprintf` 每次调用都要走一遍格式解析、加锁、写入 `FILE` 的缓冲，`ofstream` 的批量化更好。

> [!WARNING]
> **不要把这组数字当成「`iostream` 一定比 `printf` 快」的结论。** 它成立的条件是：写文件、每个数字一次插入、
> 格式简单、`-O2`。换成写格式化浮点数、或者每次都 `std::endl`，排序就会变。

**第三，`sync_with_stdio(false)` 的效果在两个平台上不一样。** Windows 上从 11.0 到 14.0 毫秒降到
5.9 到 6.4 毫秒，接近一倍；Linux 上两种写法都在 4.5 到 6.7 毫秒之间，差别落在测量的波动里。

`待确认`

**为什么两个平台的收益差这么多，本轮没有逐条查证。** 可能与运行库对标准流的实现方式有关
（一侧要经过 `FILE` 的中转，另一侧不必），需要看实现源码或做更细的剖析才能下定论。

## 8.3 怎么选

| 情形 | 选哪个 | 理由 |
|---|---|---|
| 新写的 C++ 代码，格式简单 | `iostream` | 类型由编译器检查；自定义类型能直接输出 |
| 需要输出自定义类型 | `iostream` | 写一个 `operator<<` 就够；`printf` 做不到 |
| 格式要在运行期才决定（日志框架、模板化输出） | `printf` 一族 | 格式串是数据；C++20 起可用 `std::format` |
| 往文件或屏幕大量输出 | 都行，**不要用 `std::endl`** | 决定成本的是刷新的次数，不是 API 的名字 |
| 要在同一程序里同时用两者 | 保留默认的同步 | 关掉同步会让输出顺序不再确定 |

> [!IMPORTANT]
> **这一节的结论是一句话**：`iostream` 与 `printf` 的取舍不在于「谁快」，而在于「格式在编译期还是运行期确定」
> 与「要不要输出自定义类型」。性能差异的主要来源是**缓冲与刷新的用法**，而不是这两个 API 本身。

---

# 速查表

| 件 | 一句话用途 | 典型坑 |
|---|---|---|
| `std::cin` / `std::cout` | 从标准输入读、往标准输出写 | `cin` 读之前会先刷 `cout`（`tie`） |
| `std::cerr` | 立刻可见的错误输出 | 每次插入都刷，量大时慢 |
| `std::clog` | 带缓冲的日志输出 | 崩溃时可能丢内容 |
| `<<` / `>>` | 类型安全的插入与提取 | 返回值要能当条件用：`while (cin >> x)` |
| `std::setw(n)` | 设定字段宽度 | **只对下一次输出有效** |
| `std::setfill(c)` | 设定填充字符 | 会一直留着，用完须改回来 |
| `std::setprecision(n)` | 精度 | 含义随 `fixed`/`scientific` 变化 |
| `std::fixed` | 定点输出 | 是持久状态，用完要 `defaultfloat` |
| `std::boolalpha` | `bool` 打印成 `true`/`false` | 默认打印成 `1`/`0` |
| `std::endl` | 换行并刷新 | 循环里用它会慢几十倍 |
| `std::flush` | 只刷新 | 该刷的时候要显式刷 |
| `failbit` / `eofbit` / `badbit` | 出错状态 | 失败的那次提取会往目标写 0 |
| `std::ios::clear()` | 清状态位 | **不清状态就再也读不动** |
| `std::ios::ignore(n, c)` | 跳过若干字符 | 只 `clear` 不 `ignore` 会反复失败 |
| `exceptions(mask)` | 让状态位抛异常 | 异常里没有文件名与位置 |
| `std::getline(in, s)` | 读一整行 | 与 `>>` 混用时要先 `ignore` 掉换行 |
| `std::istringstream` | 解析一段文本 | 复用前要 `clear()` 再 `str()` |
| `std::ostringstream` | 拼一段文本 | `str()` 会复制一份内容 |
| `std::ifstream` / `std::ofstream` | 文件读写 | 默认 `ofstream` 会清空文件 |
| `std::ios::binary` | 二进制模式 | Windows 上不加会翻译换行、被 `0x1A` 截断 |
| `std::ios::app` | 追加 | 与 `ate` 不是一回事 |

---

# 术语表

| 词 | 含义 |
|---|---|
| **流（stream）** | 一段有方向的字符序列，读流或写流；`iostream` 的全部设施都围绕它 |
| **标准流对象** | `cin`、`cout`、`cerr`、`clog` 四个预置对象 |
| **缓冲（buffer）** | 流内部暂存数据的内存；攒够或显式刷新时才交给设备 |
| **刷新（flush）** | 把缓冲里的内容立刻交出去 |
| **`unitbuf`** | 一个格式标志：每次插入后自动刷新 |
| **`tie`** | 两个流的绑定关系：读之前先刷被绑定的输出流 |
| **状态位** | `eofbit`、`failbit`、`badbit` 三个位，出错时置位 |
| **`sstream`** | 以内存缓冲为目标的流，用来解析或拼接字符串 |
| **文件流** | `ifstream`/`ofstream`/`fstream`，以文件为目标的流 |
| **RAII** | 资源在析构时释放；文件流靠它自动关闭 |

---

# 附录 A 复现本章节实测

**环境**：Windows 11，`g++` 15.2.0（MinGW-w64）；吞吐对照另在 WSL Ubuntu 24.04 上用 `g++` 13.3.0 跑一次。
C++ 一律 `-std=c++17`，C 一律 `-std=c23`。

`Bash`

```bash
# 第 1 节：四个标准流对象与缓冲
g++ -std=c++17 streams_flags.cpp -o streams_flags.exe && ./streams_flags.exe
g++ -std=c++17 unitbuf_count.cpp -o unitbuf_count.exe && ./unitbuf_count.exe

# 第 2 节：运算符与类型安全
g++ -std=c++17 point_io.cpp -o point_io.exe && ./point_io.exe
gcc -std=c23    printf_type_bug.c -o ptb_c.exe && ./ptb_c.exe
gcc -std=c23 -Wall printf_type_bug.c -o ptb_w.exe          # 看警告原文
g++ -std=c++17 -Wall -Wextra printf_type_bug.cpp -o ptb_cpp.exe && ./ptb_cpp.exe

# 第 3 节：格式化（no_iomanip.cpp 期望失败）
g++ -std=c++17 manip_demo.cpp -o manip_demo.exe && ./manip_demo.exe
g++ -std=c++17 no_iomanip.cpp -o no_iomanip.exe

# 第 4 节：流的状态
for f in state_bits state_loop state_recover state_except state_bad; d do g++ -std=c++17 $f.cpp -o $f.exe && ./$f.exe; done

# 第 5 节：整行读取（in.txt 两行："3" 与 "hello world"）
g++ -std=c++17 getline_bad.cpp -o getline_bad.exe && cmd /c "getline_bad.exe < in.txt"
g++ -std=c++17 getline_fix.cpp -o getline_fix.exe && cmd /c "getline_fix.exe < in.txt"
g++ -std=c++17 fstream_text.cpp -o fstream_text.exe && ./fstream_text.exe

# 第 6、7 节：字符串流与文件流
for f in sstream_demo sstream_build fstream_binary fstream_error; d do g++ -std=c++17 $f.cpp -o $f.exe && ./$f.exe; done
gcc -std=c23    file_task.c   -o file_task_c.exe   && ./file_task_c.exe   > ft_c.txt
g++ -std=c++17 file_task.cpp -o file_task_cpp.exe && ./file_task_cpp.exe > ft_cpp.txt

# 第 8 节：同一任务与吞吐对照
gcc -std=c23    same_task.c   -o same_task_c.exe   && ./same_task_c.exe   > st_c.txt
g++ -std=c++17 same_task.cpp -o same_task_cpp.exe && ./same_task_cpp.exe > st_cpp.txt
g++ -std=c++17 -O2 bench_io.cpp -o bench_io.exe
./bench_io.exe files                             # 时间打到 stderr
cmd /c "bench_io.exe cout_sync   > data_sync.txt   2> t_sync.txt"
cmd /c "bench_io.exe cout_nosync > data_nosync.txt 2> t_nosync.txt"

# Linux 侧（同一份源码，写到本地磁盘）
wsl -d Ubuntu -e bash -lc "cd /tmp && g++ -std=c++17 -O2 bench_io.cpp -o bench_io_lnx \
  && ./bench_io_lnx files 2> f.txt > /dev/null && cat f.txt"
```
---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《06-标准库/A-01-输入输出：stdio.md》第 1 节 | **对照**：流、缓冲、`fflush` 的那一半 |
| 《06-标准库/A-01-输入输出：stdio.md》第 2 节 | **对照**：`printf` 的格式化细节与 `snprintf` 的坑 |
| 《06-标准库/B-00-导读：C++ 标准库与 C 的关系.md》第 3.4 小节 | 前置：混用 `printf` 与 `cout` 时的输出顺序 |
| 《06-标准库/B-02-std-string 与 string_view.md》 | **后续**：`getline` 读进来的 `std::string` |
| 《06-标准库/B-07-文件系统：filesystem.md》 | **后续**：目录遍历与路径处理 |
| 《05-类与面向对象/09-运算符重载.md》第 3.4 小节 | **前置**：`operator<<` 为什么写成非成员函数 |
| 《05-类与面向对象/06-RAII 与资源管理.md》第 2 节 | **前置**：文件为什么不用手动关 |
| 《04-语法/13-异常.md》第 1 节 | 前置：异常的抛出与捕获 |

**配套示例见 [`B-examples/06-standard-library/02-cpp-io-report/`](../B-examples/06-standard-library/02-cpp-io-report/)，配套练习见 [`C-templates/06-standard-library/02-cpp-io-format/`](../C-templates/06-standard-library/02-cpp-io-format/)。**
