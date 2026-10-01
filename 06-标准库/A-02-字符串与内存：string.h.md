# 字符串与内存：`<string.h>`

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

C 里的一段文本不是一个类型，而是一条约定：一块 `char` 数组，末尾放一个值为 0 的字节，
长度靠数出来。这条约定在《04-语法/10-字符串.md》第 1 节已经立起来，
本章讲的是维持它的那一组函数——`<string.h>` 里的 `strlen`、`strcpy`、`strcmp`、`strstr` 一族，
以及按字节搬运的 `memcpy`、`memmove`、`memset`、`memcmp`。

**这组函数有一个共同点：它们都假设调用者已经把尺寸算对了。**
`strcpy` 不知道目标有多大，`memcpy` 不知道源有多长，`strlen` 不知道那块内存里究竟有没有结尾的 0。
标准把这份责任完整地留给了写代码的人，代价是：写错的时候，
编译往往照样过，程序往往照样跑，只是结果不对——或者过一会儿才崩。
本章因此把「正确的写法」和「写错之后到底发生什么」放在同等的位置上讲。

`<string.h>` 只处理字节。一段中文在 UTF-8 里是多个字节，
`strlen` 数出来的是字节数而不是字数，按字节切开还会把一个字切成两半，
这一层现实在《06-标准库/B-02-std-string 与 string_view.md》第 8 节展开。
数字与字符串之间的转换（`atoi`、`strtol`）虽然声明在 `<stdlib.h>`，
但它服务于同一件事——把文本变成可用的值，因此也放在本章。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 每组数据都注明工具链与编译命令；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

---

# 本章节定位

| 问题 | 在哪一节 |
|---|---|
| `strlen` 数的是什么、`strcpy`/`strcat`/`strcmp`/`strncmp` 怎么用 | **一** |
| `memcpy`/`memmove`/`memset`/`memcmp`、`strncpy` 的坑、`snprintf` 的正确用法 | **二** |
| `strchr`/`strrchr`/`strstr`/`strspn`/`strcspn`/`strtok` | **三** |
| 重叠与越界的真实后果：本机实测与 AddressSanitizer 报告原文 | **四** |
| 文本转数字：`atoi` 为什么不报错、`strtol` 的 `endptr` 与 `errno` | **五** |
| 与 `std::string` 的对照：同一个任务的两种写法 | **六** |
| 一页速查 | **七** |

**需要先知道**：

| 前置知识 | 在哪 |
|---|---|
| C 风格字符串与结尾的 0 | 《04-语法/10-字符串.md》第 1 节 |
| 数组退化成指针、指针运算 | 《04-语法/08-数组、指针与引用.md》第 2 节 |
| 改字符串字面量会崩溃 | 《04-语法/10-字符串.md》第 2.2 小节 |
| 越界的三种结局与段错误 | 《04-语法/08-数组、指针与引用.md》第 5 节 |
| `fgets` 读进来的就是 `char[]` | 《06-标准库/A-01-输入输出：stdio.md》第 3 节 |

**相邻的章节**：上一章《06-标准库/A-01-输入输出：stdio.md》把字节读进来、写出去；
本章处理读进来的那些字节。数字转换的伙伴在《06-标准库/A-03-数值、数学与随机.md》，
`malloc` 与 `free`（`strdup` 返回的内存要用它们释放）在
《06-标准库/A-05-工具与其它：stdlib 与杂项.md》第 1 节，
`errno` 的完整清单在同章第 7 节。
C++ 侧的对照件是《06-标准库/B-02-std-string 与 string_view.md》，
本段与 B 段就同一件事互相引用。

| 本章各节 | 讲什么 |
|---|---|
| 第 1 节 | 靠结尾 0 工作的四个函数，以及它们的无界性质 |
| 第 2 节 | 按字节数搬运的四个函数，`strncpy` 与 `snprintf` 的取舍 |
| 第 3 节 | 在字符串里找东西、把它切开 |
| 第 4 节 | 重叠与越界：本机实测、静默损坏、ASan 报告 |
| 第 5 节 | 文本转数字的两条路，以及怎么写出能报错的解析 |
| 第 6 节 | `<cstring>`、`std::string`、`string_view` 各自接走了什么 |
| 第 7 节 | 速查表与配套件 |

---

# 第 1 节 字符串函数：长度、拷贝、拼接、比较

## 1.1 `strlen` 数的是结尾 0 之前的字节数

**`strlen` 从首字节开始往后数，直到遇见第一个值为 0 的字节，返回它数到的个数。**
这个定义有两个直接推论：它必须逐字节扫描，因此代价与串长成正比；
它依赖那个 0 真的存在，没有 0 就会一直往后读。

`文档`

> "The strlen function returns the number of characters that precede
> the terminating null character."
>
> —— N3220 §7.26.6.4/3

`sizeof` 问的是完全另一件事：这块存储有多大。两者在数组上看起来接近，在指针上就分道扬镳。

`C`

```c
/* strlen_vs_sizeof.c    编译：gcc -std=c23 -Wall -Wextra strlen_vs_sizeof.c -o strlen_vs_sizeof */
#include <stdio.h>
#include <string.h>

int main(void) {
    char a[] = "hello";        /* 编译器数出来 6 个字节：5 个字符 + 结尾 0 */
    char b[32] = "hello";      /* 32 个字节，hello 之后全是 0 */
    const char *p = a;         /* 指针本身 8 字节 */

    printf("strlen(a) = %zu，sizeof a = %zu\n", strlen(a), sizeof a);
    printf("strlen(b) = %zu，sizeof b = %zu\n", strlen(b), sizeof b);
    printf("strlen(p) = %zu，sizeof p = %zu\n", strlen(p), sizeof p);
    printf("a 的第 6 个字节是 %d（结尾 0）\n", a[5]);
    printf("b 里第 6 个字节是 %d，第 31 个是 %d\n", b[5], b[31]);
    return 0;
}
```

`实测数据`
`Text`

```text
strlen(a) = 5，sizeof a = 6
strlen(b) = 5，sizeof b = 32
strlen(p) = 5，sizeof p = 8
a 的第 6 个字节是 0（结尾 0）
b 里第 6 个字节是 0，第 31 个是 0
```

> [!IMPORTANT]
> **`strlen` 的结果是 `size_t`，`sizeof` 的结果也是 `size_t`，但两者只在数组上才有关系。**
> 对指针用 `sizeof` 得到的是指针自己的宽度（本机 8 字节），
> 这行代码编译不报错、运行也不报错，只是数字永远是 8。

`strlen` 每次都要从头数，这在循环里会变成平方级的代价：
每拼接一次就重新数一遍已经拼好的部分，总代价与长度的平方成正比。
第 1.3 小节给出这个代价的实测数字，以及「自己记住写到哪儿了」的写法。

## 1.2 返回值是 `size_t`：减法会回绕

**长度差这类判断必须写成 `strlen(s) < n`，不能写成 `strlen(s) - n < 0`，**
因为 `size_t` 是无符号类型，减法结果不可能小于 0。

`文档`

> "wraparound: the process by which a value is reduced modulo 2N,
> where N is the width of the resulting type"
>
> —— N3220 §3.28

（草案里指数是上标，纯文本抽取之后显示成 `2N`。）

`C`

```c
/* size_t_trap.c    编译：gcc -std=c23 -Wall -Wextra size_t_trap.c -o size_t_trap */
#include <stdio.h>
#include <string.h>

/* 想判断「s 比 n 个字符短」，写成了减法 */
static int shorter_than(const char *s, size_t n) {
    return strlen(s) - n < 0;      /* size_t 是无符号的，这个表达式永远为假 */
}

int main(void) {
    const char *s = "hi";

    printf("strlen(s)      = %zu\n", strlen(s));
    printf("strlen(s) - 4  = %zu\n", strlen(s) - 4);
    printf("shorter_than(s, 4) = %d\n", shorter_than(s, 4));
    printf("正确写法 strlen(s) < 4 = %d\n", strlen(s) < 4);

    /* 另一个方向：把长度塞进 int */
    int n = (int)strlen(s);
    printf("int n = %d\n", n);
    return 0;
}
```

这个写法编译器看得见，`-Wall -Wextra` 会直接点出来：

`实测数据`
`Text`

```text
size_t_trap.c:7:26: warning: comparison of unsigned expression in '< 0' is always false [-Wtype-limits]
    7 |     return strlen(s) - n < 0;      /* size_t 是无符号的，这个表达式永远为假 */
      |                          ^
```

`实测数据`
`Text`

```text
strlen(s)      = 2
strlen(s) - 4  = 18446744073709551614
shorter_than(s, 4) = 0
正确写法 strlen(s) < 4 = 1
int n = 2
```

`18446744073709551614` 就是 2 减 4 在 64 位无符号数里的结果，也就是 2⁶⁴ − 2。
**同一个坑还会以另一种形式出现：`for (size_t i = strlen(s) - 1; i >= 0; i--)`。**
空串时 `strlen(s) - 1` 是一个极大的正数，循环条件永远成立，读写直接冲出去。
反向遍历要写 `for (size_t i = len; i-- > 0; )` 这种形式。

> [!WARNING]
> **`int` 装 `size_t` 是另一个方向的截断。** 长度超过 `INT_MAX` 的串在 32 位 `int`
> 里会变成负数，随后用作数组下标或传给 `memcpy` 时，负数被转回 `size_t` 又是一个极大的值。
> 编译器对显式强制转换不警告（上面那句 `(int)strlen(s)` 就一声不吭），
> 要自己判断长度是否可能超界。

## 1.3 `strcpy` 与 `strcat`：目标够不够大，函数不知道

**这两个函数的签名里没有「目标有多大」这个参数**，因为签名是几十年前定下的：

`文档`

> "The strcpy function copies the string pointed to by s2 (including the
> terminating null character) into the array pointed to by s1.
> If copying takes place between objects that overlap, the behavior is undefined."
>
> —— N3220 §7.26.2.4/2

`strcat` 的规则一样，只是它从目标的结尾 0 处开始写。使用它们的前提是：
`strlen(src) + 1 <= sizeof(dst)` 已经在此之前算过。

`C`

```c
/* str_basic.c    编译：gcc -std=c23 str_basic.c -o str_basic */
#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[32];
    strcpy(buf, "Hello");
    strcat(buf, ", ");
    strcat(buf, "world");
    printf("拼接后 [%s]，长度 %zu，缓冲区 sizeof %zu\n", buf, strlen(buf), sizeof buf);

    printf("strcmp(\"abc\", \"abd\") = %d\n", strcmp("abc", "abd"));
    printf("strcmp(\"abc\", \"abc\") = %d\n", strcmp("abc", "abc"));
    printf("strcmp(\"abd\", \"abc\") = %d\n", strcmp("abd", "abc"));
    printf("strncmp(\"abcXXX\", \"abcYYY\", 3) = %d\n", strncmp("abcXXX", "abcYYY", 3));
    return 0;
}
```

`实测数据`
`Text`

```text
拼接后 [Hello, world]，长度 12，缓冲区 sizeof 32
strcmp("abc", "abd") = -1
strcmp("abc", "abc") = 0
strcmp("abd", "abc") = 1
strncmp("abcXXX", "abcYYY", 3) = 0
```

**确认目标够大的三条常用办法**：

| 办法 | 写法 | 适用 |
|---|---|---|
| 目标是数组，且就在眼前 | 写之前看一眼 `sizeof dst` | 局部固定缓冲区 |
| 目标是数组，但要拼多次 | 记住已用长度，每次先检查剩余空间 | 循环里累积拼接 |
| 内容是格式化出来的 | 用 `snprintf` 并检查返回值 | 见第 2.3 小节 |

**长度累积时，最容易写错的是「不确定目标还剩多少」**。
`strcat` 的唯一保护是「调用者自己清楚」。第 4.2 小节会给出不检查时真实发生的事。

`strcat` 还有一笔隐藏成本：**它每次调用都要从开头重新数一遍目标已有的长度**，
所以在一个循环里反复拼接，总代价与最终长度的平方成正比。

同样的 200000 次拼接，三次运行的结果：

`实测数据`
`Text`

```text
第 1 次：逐次 strcat 6075 ms，记住位置 0.10 ms，比值约 60750 倍
第 2 次：逐次 strcat 6163 ms，记住位置 0.10 ms，比值约 61630 倍
第 3 次：逐次 strcat 6145 ms，记住位置 0.05 ms，比值约 122900 倍
```

**这里的 `clock()` 精度有限**（本机约 1 ms），所以第二列的数字只在「比第一列小几个数量级」
这个意义上可靠。结论足够清楚：**循环里反复 `strcat` 的写法，长度上到几十万字节之后
就从「慢一点」变成「卡住」**。改成记住当前位置之后，同一件事落进噪声里。
`clock()` 本身的问题在《06-标准库/A-04-时间与日期：time.h.md》里讲。

## 1.4 `strcmp` 与 `strncmp`：只拿返回值的符号

`strcmp` 逐字节比较两个串，遇到第一个不同的字节就停下，返回值的含义只有符号：

`文档`

> "The strcmp function returns an integer greater than, equal to, or less
> than zero, accordingly as the string pointed to by s1 is greater than,
> equal to, or less than the string pointed to by s2."
>
> —— N3220 §7.26.4.2/3

**标准没有规定具体数值。** 这一点在本机与上一个平台上能同时看到：

`C`

```c
/* strcmp_value.c    编译：gcc -std=c23 -Wall -Wextra strcmp_value.c -o strcmp_value */
#include <stdio.h>
#include <string.h>

static void show(const char *a, const char *b) {
    int r = strcmp(a, b);
    printf("strcmp(\"%s\", \"%s\") = %4d （%s）\n", a, b, r,
           r < 0 ? "小于" : (r > 0 ? "大于" : "相等"));
}

int main(void) {
    show("abc", "abc");
    show("abc", "abd");      /* 只差最后一个字符 */
    show("abc", "abz");      /* 差得更多 */
    show("abc", "ab");       /* 一个是另一个的前缀 */
    show("Z", "a");          /* 大写字母的编码比小写小 */

    if (strcmp("abc", "abd") < 0) printf("用法示范：abc 排在 abd 前面\n");
    return 0;
}
```

`实测数据`
`Text`

```text
Windows 侧 gcc 15.2（MinGW-w64）：
strcmp("abc", "abc") =    0 （相等）
strcmp("abc", "abd") =   -1 （小于）
strcmp("abc", "abz") =   -1 （小于）
strcmp("abc", "ab") =    1 （大于）
strcmp("Z", "a") =   -1 （小于）

Linux 侧 gcc 13.3（glibc）：
strcmp("abc", "abc") =    0 （相等）
strcmp("abc", "abd") =   -1 （小于）
strcmp("abc", "abz") =  -23 （小于）
strcmp("abc", "ab") =   99 （大于）
strcmp("Z", "a") =   -7 （小于）
```

**同一组输入，两边给出的数值不同，符号完全一致。**
Windows 侧恰好只给 ±1，Linux 侧给的是字节差，看起来更「有信息量」——
但依赖这些数值的代码换一个平台就会错。

`strncmp` 多一个长度参数，比较到第一个不同的字节或者比较满 n 个字节为止，
适合判断前缀。**`strncmp("abcXXX", "abcYYY", 3)` 返回 0**，
因为前 3 个字节相同——上面那份输出里的最后一行就是它。

> [!WARNING]
> **用 `strncmp` 判「整个串相等」是常见错误。**
> `strncmp(a, b, sizeof a)` 在当前缀相同时就会返回 0，后面差多少都不看。
> 要判整串相等就用 `strcmp`；要判「安全地比较前 n 个字节」才用 `strncmp`。

## 1.5 有界的版本，与几个容易记错的细节

标准里有「带 n」的版本，但它们的 n 含义各不相同：

| 函数 | 第三个参数的含义 | 结尾 0 | 需要目标已经是字符串吗 |
|---|---|---|---|
| `strncpy(dst, src, n)` | **最多写 n 个字节**，不足则补 0 到 n | **不保证补** | 不需要 |
| `strncat(dst, src, n)` | **最多追加 n 个字符**（不含 0） | **一定补** | **需要** |
| `snprintf(buf, n, fmt, ...)` | 缓冲区总大小，含结尾 0 | 一定补 | 不需要 |

`strncat` 的规则在标准里写得很清楚，包括那个容易忽略的脚注：

`文档`

> "The strncat function appends not more than n characters ... A terminating
> null character is always appended to the result.362) If copying takes place
> between objects that overlap, the behavior is undefined."
>
> 脚注 362："Thus, the maximum number of characters that can end up in the
> array pointed to by s1 is strlen(s1)+n+1."
>
> —— N3220 §7.26.3.2/2

**脚注 362 就是那个坑：`n` 是「最多追加几个字符」，不是「目标还剩几个字节」。**
`strncat(dst, src, sizeof dst)` 这种写法看起来像做了保护，实际会写到
`strlen(dst) + sizeof dst + 1` 个字节。

`C`

```c
/* strncat_demo.c    编译：gcc -std=c23 -Wall -Wextra strncat_demo.c -o strncat_demo */
#include <stdio.h>
#include <string.h>

int main(void) {
    char a[16] = "abc";
    strncat(a, "defghij", 3);            /* 最多再追加 3 个字符，然后一定补 0 */
    printf("strncat(a, \"defghij\", 3) 之后：[%s]，长度 %zu\n", a, strlen(a));

    char b[16] = "abc";
    strncpy(b, "xy", 4);                 /* 从开头覆盖 4 个字节，一个 0 也不补 */
    printf("strncpy(b, \"xy\", 4) 之后前 6 个字节：");
    for (int i = 0; i < 6; i++) putchar(b[i] ? b[i] : '.');
    printf("（b 里剩下的旧内容还在，所以还能当字符串用）\n");

    char c[8] = "abc";
    strncpy(c, "xy", 2);                 /* 只覆盖前 2 个字节，0 也没写 */
    printf("strncpy(c, \"xy\", 2) 之后：[%s]\n", c);

    /* strncat 的第三个参数不看你还有多少空间：写多大由你负责 */
    char small[6] = "abc";
    strncat(small, "defgh", 5);          /* 3 + 5 + 1 = 9 字节，缓冲区只有 6 */
    printf("strncat 写满了缓冲区：[%s]\n", small);
    return 0;
}
```

编译器对后两处都给了警告：

`实测数据`
`Text`

```text
strncat_demo.c:17:5: warning: 'strncpy' output truncated before terminating nul copying 2 bytes from a string of the same length [-Wstringop-truncation]
strncat_demo.c:22:5: warning: 'strncat' specified bound 5 equals source length [-Wstringop-overflow=]
```

`实测数据`
`Text`

```text
strncat(a, "defghij", 3) 之后：[abcdef]，长度 6
strncpy(b, "xy", 4) 之后前 6 个字节：xy....
strncpy(c, "xy", 2) 之后：[xyc]
strncat 写满了缓冲区：[abcdefgh]
```

最后一行是越界写的后果：`small` 只有 6 字节，写进去 9 字节，
多出来的字节落在相邻的栈空间上，而这一行照样打印出了内容。
**它没有崩，是因为越界写恰好写进了自己的栈帧。**
同一份代码在 Linux 侧用 AddressSanitizer 编译，运行时会直接报
`WRITE of size 6` 的 `stack-buffer-overflow`，报告原文见第 4.2 小节。

**C23 新进标准的是 `strdup` 与 `strndup`**，它们把一块字符串复制到 `malloc` 出来的空间里，
返回的指针要交给 `free`：

`文档`

> "The strdup function creates a copy of the string pointed to by s in a space
> allocated as if by a call to malloc."
>
> —— N3220 §7.26.2.6/2

`实测数据`
`Text`

```text
__STDC_VERSION__ = 202311L
strdup 可用：[copy]
strnlen("hello", 32) = 5
```

**标准里有、本机的头文件里没有，这种情况在新标准落地时会同时出现。**
`strndup` 就是这样一个名字：

`C`

```c
/* strndup_missing.c    编译：gcc -std=c23 -Wall -Wextra strndup_missing.c -o strndup_missing （失败） */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char *n = strndup("abcdefgh", 4);      /* 标准里有（7.26.2.7），本机头文件里没有 */
    printf("%s\n", n);
    free(n);
    return 0;
}
```

`实测数据`
`Text`

```text
strndup_missing.c:7:15: error: implicit declaration of function 'strndup' [-Wimplicit-function-declaration]
    7 |     char *n = strndup("abcdefgh", 4);      /* 标准里有（7.26.2.7），本机头文件里没有 */
      |               ^~~~~~~
strndup_missing.c:7:15: warning: incompatible implicit declaration of built-in function 'strndup' [-Wbuiltin-declaration-mismatch]
```

> [!CAUTION]
> **`implicit declaration` 在 C23 里是错误，在更早的 C 里只是警告。**
> 在旧标准下这段代码会编译通过，然后按「返回 `int`」的函数去调用一个返回指针的函数——
> 64 位平台上指针被截成 32 位，`free` 一个坏指针时崩溃。遇到这条诊断时，
> 正确的反应是查这个函数在当前工具链上到底有没有，而不是加个强制转换把它压下去。

`strnlen` 不是 C 标准里的名字（标准里只有 Annex K 的 `strnlen_s`），
本机头文件声明了它，因此能编过。代码要移植时，这两个名字都要先确认。

> [!NOTE]
> **第 1 节小结**：`strlen` 依赖结尾 0，返回值是 `size_t`（减法会回绕）；
> `strcpy`/`strcat` 不检查目标大小，责任在调用者；
> `strcmp` 只保证返回值的符号，本机与 glibc 给出的数值不同；
> `strncpy` 的 n 是「写几个字节」且不补 0，`strncat` 的 n 是「追加几个字符」且一定补 0。

---

# 第 2 节 按字节搬运：`memcpy` 一族

## 2.1 `memcpy`：字节数与 `sizeof` 的三种写法

`memcpy` 不认识字符串，它只认「从哪、到哪、几个字节」。
正因为不认识，它既能拷字符串，也能拷结构体数组、拷贝一块协议帧。

`memcpy` 与 `memmove` 的分工写在标准里，是这一节的重点：

`文档`

> "The memcpy function copies n characters from the object pointed to by s2
> into the object pointed to by s1. If copying takes place between objects
> that overlap, the behavior is undefined."
>
> —— N3220 §7.26.2.1/2

`文档`

> "The memmove function copies n characters from the object pointed to by s2
> into the object pointed to by s1. Copying takes place as if the n characters
> from the object pointed to by s2 are first copied into a temporary array of
> n characters that does not overlap the objects pointed to by s1 and s2,
> and then the n characters from the temporary array are copied into the object
> pointed to by s1."
>
> —— N3220 §7.26.2.3/2

**一句话记法：`memmove` 允许两块内存重叠，`memcpy` 不允许。**
标准给 `memmove` 的描述是「就像先拷进一个不重叠的临时数组，再拷回去」，
所以它在实现上必须判断方向或者退化成一次临时拷贝，代价略高。

`C`

```c
/* memcpy_bytes.c    编译：gcc -std=c23 -Wall -Wextra memcpy_bytes.c -o memcpy_bytes */
#include <stdio.h>
#include <string.h>

struct point { int x, y; };

int main(void) {
    /* 一、结构体数组整体拷贝：sizeof 后面不写类型，跟着变量走 */
    struct point src[3] = {{1, 2}, {3, 4}, {5, 6}};
    struct point dst[3];
    memcpy(dst, src, sizeof src);
    printf("拷贝后：(%d,%d) (%d,%d) (%d,%d)\n",
           dst[0].x, dst[0].y, dst[1].x, dst[1].y, dst[2].x, dst[2].y);

    /* 二、只拷前两个元素 */
    struct point two[3] = {{0, 0}, {0, 0}, {0, 0}};
    memcpy(two, src, 2 * sizeof src[0]);
    printf("只拷两个：(%d,%d) (%d,%d) 第三个还是 (%d,%d)\n",
           two[0].x, two[0].y, two[1].x, two[1].y, two[2].x, two[2].y);

    /* 三、sizeof 数组与 sizeof 指针的差别 */
    int arr[10];
    int *p = arr;
    printf("sizeof arr = %zu，sizeof p = %zu\n", sizeof arr, sizeof p);

    /* 四、memchr：带 0 的字节块里找字节，strchr 做不到 */
    unsigned char frame[] = {0x01, 0x00, 0x02, 0x03, 0x00, 0x04};
    unsigned char *hit = memchr(frame, 0x03, sizeof frame);
    printf("memchr 找到 0x03 的位置：%d\n", hit ? (int)(hit - frame) : -1);
    return 0;
}
```

`实测数据`
`Text`

```text
拷贝后：(1,2) (3,4) (5,6)
只拷两个：(1,2) (3,4) 第三个还是 (0,0)
sizeof arr = 40，sizeof p = 8
memchr 找到 0x03 的位置：3
```

**`sizeof arr` 是 40，`sizeof p` 是 8**，这是同一个坑在 `memcpy` 上的样子：
`memcpy(dst, src, sizeof src)` 里的 `src` 一旦是函数参数（已经退化成指针），
这一句就只拷 8 个字节，而且编译不报错。

| 写法 | 结果 | 评价 |
|---|---|---|
| `memcpy(dst, src, sizeof src)` | 数组：整块；指针：只拷指针宽度 | **只在 src 确实是数组时正确** |
| `memcpy(dst, src, n * sizeof src[0])` | 按元素个数算 | 推荐，意图明确 |
| `memcpy(dst, src, len + 1)` | 多拷一个字节给结尾 0 | 拷字符串时常用 |

## 2.2 `memmove`：重叠区域必须用它

**重叠分两种方向**：目标在源后面、目标在源前面。
把一块内存往后挪（目标在后）时，「从前往后逐字节搬」的写法会先覆盖掉还没读的源字节。

`C`

```c
/* why_forward.c    编译：gcc -std=c23 why_forward.c -o why_forward
 * 手写一个「从前往后、一个字节一个字节搬」的拷贝函数：
 * 这正是很多 memcpy 实现的基本策略，也是重叠时会出错的原因。
 */
#include <stdio.h>
#include <string.h>

static void copy_forward(char *dst, const char *src, size_t n) {
    while (n--) *dst++ = *src++;        /* 从低地址往高地址搬 */
}

int main(void) {
    char a[16] = "abcdef";
    char b[16] = "abcdef";

    copy_forward(a + 2, a, 6);          /* 目标在源后面，重叠 4 字节 */
    memmove(b + 2, b, 6);               /* 标准保证正确的做法 */

    printf("原始内容 [abcdef]\n");
    printf("从前往后搬：");
    for (int i = 0; i < 8; i++) putchar(a[i] ? a[i] : '.');
    printf("\n");

    /* 反过来：目标在源前面，从前往后搬就是对的 */
    char c[16] = "abcdef";
    copy_forward(c, c + 2, 4);
    printf("目标在前时：");
    for (int i = 0; i < 8; i++) putchar(c[i] ? c[i] : '.');
    printf("\n");

    printf("memmove 的结果：");
    for (int i = 0; i < 8; i++) putchar(b[i] ? b[i] : '.');
    printf("\n");
    return 0;
}
```

`实测数据`
`Text`

```text
原始内容 [abcdef]
从前往后搬：abababab
目标在前时：cdefef..
memmove 的结果：ababcdef
```

`abababab` 就是「自己盖掉自己」的结果：写 `a[2]` 时把 `a[2]` 原来的 `c` 变成 `a`，
下一次读 `a[4]` 读到的已经不是 `e`。**同一个平台上，`memmove` 给出的是正确的 `ababcdef`。**

| 情形 | `memcpy` | `memmove` |
|---|---|---|
| 两块内存完全不重叠 | 正确，快 | 正确，可能略慢 |
| 目标在源前面（`dst < src`） | 未定义行为，实现上通常碰巧正确 | 正确 |
| 目标在源后面（`dst > src`） | 未定义行为，**可能得到一份错数据** | 正确 |
| 不确定会不会重叠 | **不要用** | 用它 |

> [!IMPORTANT]
> **判断标准只有一条：这两块内存会不会碰头。**
> 会碰头、或者无法确定时，应当用 `memmove`。它多出来的那点代价，
> 远小于一次静默的数据损坏——第 4.1 小节给出了 280 组用例的结果，
> 说明为什么「本机跑起来是对的」完全不能当作判据。

## 2.3 `strncpy` 的经典坑与 `snprintf` 的正确用法

`strncpy` 的名字容易被读成「安全版的 `strcpy`」，标准里的定义却是另一回事：

`文档`

> "If the array pointed to by s2 is a string that is shorter than n characters,
> null characters are appended to the copy in the array pointed to by s1, until
> n characters in all have been written."
>
> 脚注 361："Thus, if there is no null character in the first n characters of
> the array pointed to by s2, the result will not be null-terminated."
>
> —— N3220 §7.26.2.5/3

**两句话合起来的意思是：`strncpy` 写满 n 个字节就停，一个 0 都不会多写。**
它的设计目标是填满一个定长字段（例如目录项里的文件名），
不是为了安全地复制字符串。

`C`

```c
/* strncpy_trap.c    编译：gcc -std=c23 strncpy_trap.c -o strncpy_trap */
#include <stdio.h>
#include <string.h>

int main(void) {
    char dst[16];
    memset(dst, '#', sizeof dst);         /* 全部填成 #：没有被写到的位置一眼可见 */

    strncpy(dst, "hello", 3);             /* 只拷 3 个字符，之后一个字节也不动 */
    printf("strncpy(dst, \"hello\", 3) 之后前 8 字节：");
    for (int i = 0; i < 8; i++) {
        putchar(dst[i] >= 32 && dst[i] < 127 ? dst[i] : '?');
    }
    putchar('\n');
    printf("第 3 格是 0 吗：%s\n", dst[3] == '\0' ? "是" : "不是");

    /* 想要「最多拷 n 个字符、并保证有结尾 0」的正确写法 */
    char ok[16];
    memset(ok, '#', sizeof ok);
    strncpy(ok, "hello", sizeof ok - 1);
    ok[sizeof ok - 1] = '\0';             /* 关键的一行 */
    printf("手工补 0 之后：[%s]，长度 %zu\n", ok, strlen(ok));
    return 0;
}
```

`实测数据`
`Text`

```text
strncpy_trap.c:9:5: warning: 'strncpy' output truncated copying 3 bytes from a string of length 5 [-Wstringop-truncation]
```

`实测数据`
`Text`

```text
strncpy(dst, "hello", 3) 之后前 8 字节：hel#####
第 3 格是 0 吗：不是
手工补 0 之后：[hello]，长度 5
```

**`hel#####` 就是全部问题**：三个字节被写进去，第 4 格还是原来的 `#`。
如果这个缓冲区后面按字符串使用（`printf("%s", dst)`、`strlen(dst)`、`strcat`），
就会从第 4 格继续往后读，直到撞见一个 0——那可能已经出了这个数组。
第 4.3 小节给出了这种写法在 AddressSanitizer 下的报告原文。

**要「最多拷 n 个字节且保证有结尾 0」，正确的工具是 `snprintf`：**

`C`

```c
/* snprintf_trunc.c    编译：gcc -std=c23 snprintf_trunc.c -o snprintf_trunc */
#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[8];
    memset(buf, '#', sizeof buf);

    /* 要写的是 "chapter-7"，共 9 个字符，缓冲区只有 8 字节 */
    int need = snprintf(buf, sizeof buf, "%s-%d", "chapter", 7);
    printf("缓冲区 %zu 字节，内容 [%s]，strlen %zu\n", sizeof buf, buf, strlen(buf));
    printf("snprintf 返回值 %d\n", need);
    printf("返回值 >= 缓冲区大小，说明被截断：%s\n", need >= (int)sizeof buf ? "是" : "否");
    printf("结尾一定有 0：%s\n", buf[sizeof buf - 1] == '\0' ? "是" : "否");

    /* 再要一次真实长度，照样能拿到 */
    char big[32];
    int need2 = snprintf(big, sizeof big, "%s-%d", "chapter", 7);
    printf("换 32 字节缓冲区：返回值 %d，内容 [%s]\n", need2, big);
    return 0;
}
```

`实测数据`
`Text`

```text
snprintf_trunc.c:10:46: warning: '%d' directive output truncated writing 1 byte into a region of size 0 [-Wformat-truncation=]
snprintf_trunc.c:10:16: note: 'snprintf' output 10 bytes into a destination of size 8
```

`实测数据`
`Text`

```text
缓冲区 8 字节，内容 [chapter]，strlen 7
snprintf 返回值 9
返回值 >= 缓冲区大小，说明被截断：是
结尾一定有 0：是
换 32 字节缓冲区：返回值 9，内容 [chapter-7]
```

**返回的 9 是「如果缓冲区足够大，本应写出多少个字符」，它不包含结尾 0。**
这个返回值有两个用途：判断有没有被截断（`need >= sizeof buf`），
以及算出真正需要多大的缓冲区（`malloc(need + 1)`）。
`snprintf` 一定会补结尾 0（除非 `n` 为 0），这一点在标准里写明了：

`文档`

> "Otherwise, output characters beyond the n-1st are discarded rather than
> being written to the array, and a null character is written at the end of
> the characters actually written into the array."
>
> —— N3220 §7.23.6.5/2

> [!WARNING]
> **`strncpy` 之后手工补 0 的写法有一个前提：缓冲区的真实大小是已知的。**
> 写成 `strncpy(dst, src, strlen(src)); dst[strlen(src)] = '\0';` 是错的：
> 长度取的是源串长度，目标如果比源短，这一行补 0 的动作本身就落在数组之外。

## 2.4 `memset` 与 `memcmp`

`memset` 把每个字节都设成给定的值，因此它只能用来把内存设成某个**字节**：

`文档`

> "The memset function copies the value of c (converted to an unsigned char)
> into each of the first n characters of the object pointed to by s."
>
> —— N3220 §7.26.6.1/2

| 想做的事 | 正确写法 | 错误写法与后果 |
|---|---|---|
| 清零一块内存 | `memset(p, 0, n)` | — |
| 把 `int` 数组设为 1 | 循环赋值 | `memset(arr, 1, sizeof arr)` → 每个字节都是 0x01，`int` 值是 16843009 |
| 把指针设为 `NULL` | 循环赋值 | `memset(p, 0, n)` 只在空指针全 0 的平台上成立 |

`memcmp` 与 `strcmp` 的差别只有一条：`memcmp` 不认结尾 0，比满 n 个字节。

`文档`

> "The memcmp function compares the first n characters of the object pointed
> to by s1 to the first n characters of the object pointed to by s2."
>
> —— N3220 §7.26.4.1/2

`C`

```c
/* memcmp_demo.c    编译：gcc -std=c23 memcmp_demo.c -o memcmp_demo */
#include <stdio.h>
#include <string.h>

int main(void) {
    /* 两组字节序列，前 3 字节相同，第 3 字节都是 0 */
    char x[6] = {'a', 'b', '\0', 'd', 'e', '\0'};
    char y[6] = {'a', 'b', '\0', 'X', 'Y', '\0'};

    printf("strcmp(x, y)  = %d（遇到 0 就停，后面不同看不见）\n", strcmp(x, y));
    printf("memcmp(x, y, 6) = %d（比满 6 字节）\n", memcmp(x, y, 6));

    unsigned char a[4] = {0x00, 0x01, 0x02, 0x03};
    unsigned char b[4] = {0x00, 0x01, 0x02, 0xFF};
    /* 注意：memcmp 只保证符号，不保证差值 */
    printf("memcmp 的返回值符号：%s\n", memcmp(a, b, 4) < 0 ? "负数" : "非负");
    return 0;
}
```

`实测数据`
`Text`

```text
Windows 侧 gcc 15.2：
strcmp(x, y)  = 0（遇到 0 就停，后面不同看不见）
memcmp(x, y, 6) = 1（比满 6 字节）

Linux 侧 gcc 13.3（glibc）：
strcmp(x, y)  = 0（遇到 0 就停，后面不同看不见）
memcmp(x, y, 6) = 12（比满 6 字节）
```

两边 `strcmp` 都给 0，因为第 3 个字节就是 0；`memcmp` 不一样大，
但都非零，**所以判等只能写 `memcmp(a, b, n) == 0`，不能写 `== 1`**。

> [!NOTE]
> **第 2 节小结**：`memcpy` 要求两块内存不重叠，重叠必须换 `memmove`；
> `sizeof` 用在退化成指针的参数上只会得到指针宽度；
> `strncpy` 不保证补 0，要定长且安全就用 `snprintf` 并检查返回值；
> `memset` 按字节设置，`memcmp` 只保证返回值的符号。

---

# 第 3 节 查找与切分

## 3.1 `strchr`、`strrchr`、`strstr`

三个查找函数，返回值都是指针或者 `NULL`：

| 函数 | 找什么 | 从哪端找 | 找不到时 |
|---|---|---|---|
| `strchr(s, c)` | 字符 `c`（`c` 为 0 时找的就是结尾 0） | 从头 | `NULL` |
| `strrchr(s, c)` | 字符 `c` | 从尾 | `NULL` |
| `strstr(s, sub)` | 子串 `sub` | 从头 | `NULL` |

**返回值是「找到的位置」，不是「有没有找到」。** 想拿下标就减首地址，
想拿「有没有」就跟 `NULL` 比。这两个动作分开写，代码才读得懂。

`C`

```c
/* str_search.c    编译：gcc -std=c23 str_search.c -o str_search */
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "src/lib/util.c";

    const char *slash = strrchr(path, '/');           /* 从后往前找 */
    printf("最后一个 / 之后：[%s]\n", slash ? slash + 1 : "(没有)");

    const char *dot = strrchr(path, '.');
    printf("扩展名位置：%d，内容 [%s]\n", (int)(dot - path), dot ? dot : "(没有)");

    const char *hit = strstr(path, "lib");
    printf("strstr(path, \"lib\") 找得到吗：%s，位置 %d\n",
           hit ? "找得到" : "找不到", hit ? (int)(hit - path) : -1);

    /* strchr 从左往右找第一个，找不到返回 NULL */
    const char *c = strchr(path, 'u');
    printf("第一个 u 在位置 %d\n", c ? (int)(c - path) : -1);
    printf("path 里没有 'z'，strchr 返回 %s\n", strchr(path, 'z') ? "非空" : "NULL");

    /* 子串为空串时，strstr 返回的是首地址 */
    printf("strstr(path, \"\") 的位置：%d\n", (int)(strstr(path, "") - path));
    return 0;
}
```

`实测数据`
`Text`

```text
最后一个 / 之后：[util.c]
扩展名位置：12，内容 [.c]
strstr(path, "lib") 找得到吗：找得到，位置 4
第一个 u 在位置 8
path 里没有 'z'，strchr 返回 NULL
strstr(path, "") 的位置：0
```

> [!WARNING]
> **`strchr(s, 0)` 返回的是指向结尾 0 的指针，不是 `NULL`。**
> 空子串同理：`strstr(path, "")` 返回首地址，位置是 0。
> 这两条都是标准规定的，用在「找到了吗」的判断里就会误判，
> 因此判断子串是否为空要单独写。

**在二进制数据里找字节要用 `memchr`，不能用 `strchr`**：
`strchr` 遇到值为 0 的字节会当成串尾停下，`memchr` 则只看长度（例子见第 2.1 小节）。

## 3.2 `strspn`、`strcspn`、`strpbrk`

这三个函数的名字不好记，作用却很实用——**它们把「跳过一部分、取出一部分」写成了一次调用**：

| 函数 | 返回 | 记忆方式 |
|---|---|---|
| `strspn(s, set)` | 开头连续**属于** `set` 的字符个数 | span of set |
| `strcspn(s, set)` | 开头连续**不属于** `set` 的字符个数 | span of complement |
| `strpbrk(s, set)` | 指向第一个属于 `set` 的字符的指针 | pointer to break |

有了它们，解析一行 `键 = 值` 就是「跳过空白、取到分隔符为止」两步。

`C`

```c
/* str_span.c    编译：gcc -std=c23 -Wall -Wextra str_span.c -o str_span */
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *line = "  \t key = value ; ";

    /* strspn：从开头数，有多少个字符属于这个集合 */
    size_t blank = strspn(line, " \t");
    printf("开头空白 %zu 个字符\n", blank);

    /* strcspn：从开头数，到第一个属于这个集合的字符为止 */
    const char *rest = line + blank;
    size_t keylen = strcspn(rest, "=; \t");
    printf("键名 [%.*s]，长度 %zu\n", (int)keylen, rest, keylen);

    /* strpbrk：找出这几个字符里最先出现的那个 */
    const char *sep = strpbrk(rest, "=;");
    printf("最先出现的分隔符是 [%c]，在偏移 %d\n", *sep, (int)(sep - rest));

    /* 找下一个键值对：跳过 = 与空白，再跳过 ; 与空白 */
    const char *v = sep + 1;
    v += strspn(v, " \t");
    size_t vlen = strcspn(v, "; \t");
    printf("值 [%.*s]，长度 %zu\n", (int)vlen, v, vlen);

    /* 不存在的字符：strpbrk 返回 NULL */
    printf("strpbrk 找不到时返回 %s\n", strpbrk(rest, "@#$") ? "非空" : "NULL");
    return 0;
}
```

`实测数据`
`Text`

```text
开头空白 4 个字符
键名 [key]，长度 3
最先出现的分隔符是 [=]，在偏移 4
值 [value]，长度 5
strpbrk 找不到时返回 NULL
```

**`%.*s` 这个格式配合长度用**：`printf("%.*s", (int)len, p)` 只打印前 `len` 个字符，
不需要先把那段内存改成以 0 结尾。它是处理「一段没有结尾 0 的字节」时最直接的工具，
长度参数的类型是 `int`，`size_t` 要显式转换（见第 1.2 小节）。

## 3.3 `strtok`：会改写原串，而且不可重入

`strtok` 的用法是「第一次给串，之后给 `NULL`」，看起来很方便，代价有三个：

`文档`

> "If such a character is found, it is overwritten by a null character, which
> terminates the current token."
>
> —— N3220 §7.26.5.9/4

1. **它改写原串**：每个分隔符被就地写成 0。传进去的必须是可写内存，
   传字符串字面量会崩溃（见本小节末尾）。
2. **它把状态藏在内部**：第二次调用靠 `NULL` 找回上次的位置，
   因此两个循环不能嵌套切分同一个缓冲区，多线程也不能同时用。
3. **它会跳过空字段**：`"aa,,bb"` 用 `,` 切出来是两段，中间那个空字段直接没了。

`C`

```c
/* strtok_demo.c    编译：gcc -std=c23 strtok_demo.c -o strtok_demo */
#include <stdio.h>
#include <string.h>

int main(void) {
    char csv[] = "aa,,bb;cc";          /* 原串会被改成什么样，注意看 */
    char work[32];
    strcpy(work, csv);

    printf("切分前：[%s]\n", work);
    int n = 0;
    for (char *tok = strtok(work, ",;"); tok; tok = strtok(NULL, ",;")) {
        printf("  第 %d 段 [%s]\n", ++n, tok);
    }
    printf("切分后原缓冲区变成：");
    for (size_t i = 0; i < sizeof work && work[i]; i++) putchar(work[i] == '\0' ? '|' : work[i]);
    printf("（每个分隔符被改写成了 0，所以打不出来）\n");

    /* 空字段被跳过的实证：aa,,bb 只有两段 */
    char two[] = "aa,,bb";
    n = 0;
    for (char *tok = strtok(two, ","); tok; tok = strtok(NULL, ",")) n++;
    printf("aa,,bb 用 \",\" 切出 %d 段（空字段被跳过）\n", n);

    return 0;
}
```

`实测数据`
`Text`

```text
切分前：[aa,,bb;cc]
  第 1 段 [aa]
  第 2 段 [bb]
  第 3 段 [cc]
切分后原缓冲区变成：aa（每个分隔符被改写成了 0，所以打不出来）
aa,,bb 用 "," 切出 2 段（空字段被跳过）
不改原串的切分：[aa] [bb] [cc]
```

**把字面量交给 `strtok` 是最常见的错法**，它的后果在两个平台上都很直接：

`C`

```c
/* strtok_literal.c    编译：gcc -std=c23 -Wall -Wextra strtok_literal.c -o strtok_literal */
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *line = "aa,bb,cc";
    char *tok = strtok((char *)line, ",");      /* 强制去掉了 const，等于明知故犯 */
    printf("第一段 [%s]\n", tok);
    return 0;
}
```

`实测数据`
`Text`

```text
Windows 侧 gcc 15.2：无输出，退出码 3221225477（0xC0000005，访问冲突）
Linux 侧 gcc 13.3：无输出，退出码 139（SIGSEGV，段错误）
```

> [!CAUTION]
> **字面量放在只读段里，`strtok` 往分隔符位置写 0，这一步就是一次非法写。**
> 那句 `(char *)` 强制转换把编译器的警告压了下去，也把唯一的提示去掉了——
> 这正是《04-语法/10-字符串.md》第 2.2 小节讲过的「改字面量会崩溃」在库函数上的再现。
> **需要在原地切分就把数据复制到 `char[]` 里；不想复制就用下一小节的手写切分。**

`strtok` 还有一个可重入的版本 `strtok_r`（POSIX 的名字，标准里没有），
它把「上次的位置」交给调用者保管，因此可以嵌套使用。
C 里需要可重入时，本机头文件声明了 `strtok_r`；要移植的代码先确认它在不在。

## 3.4 不改原串的切分写法

**`strcspn` 与 `strspn` 的两个返回值，正好组成一个循环**：
`strcspn` 给出「这一段有多长」，`strspn` 给出「分隔符有多长」。

`C`

```c
/* split_line.c    编译：gcc -std=c23 -Wall -Wextra split_line.c -o split_line
 * 不用 strtok 解析一行配置：原串不动，空字段也能看见。
 */
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *line = "host=127.0.0.1;port=8080;;debug=1";
    printf("原始行：[%s]（它是 const 的，全程不会被改写）\n", line);

    const char *p = line;
    int n = 0;
    while (*p) {
        size_t seg = strcspn(p, ";");            /* 这一段有多长 */
        if (seg > 0) {                           /* 空字段直接跳过 */
            const char *eq = memchr(p, '=', seg);    /* 只在段内找 = */
            if (eq) {
                printf("  第 %d 项：键 [%.*s] 值 [%.*s]\n", ++n,
                       (int)(eq - p), p, (int)(seg - (eq - p) - 1), eq + 1);
            } else {
                printf("  第 %d 项：[%.*s] 没有等号，当作开关\n", ++n, (int)seg, p);
            }
        } else {
            printf("  （空字段）\n");
        }
        p += seg;
        p += strspn(p, ";");                     /* 跳过分隔符 */
    }
    printf("共 %d 项\n", n);
    return 0;
}
```

`实测数据`
`Text`

```text
原始行：[host=127.0.0.1;port=8080;;debug=1]（它是 const 的，全程不会被改写）
  第 1 项：键 [host] 值 [127.0.0.1]
  第 2 项：键 [port] 值 [8080]
  第 3 项：键 [debug] 值 [1]
共 3 项
```

这一段用的是 `memchr(p, '=', seg)` 而不是 `strchr`：
**`strchr` 会越过这一段的边界去找等号**，落在下一段里的等号会被当成自己的。
`memchr` 带长度，只在段内找——这也让整段代码不需要修改原串。

> [!NOTE]
> **第 3 节小结**：三个查找函数返回指针，找不到给 `NULL`，空子串与结尾 0 是两处例外；
> `strspn`/`strcspn`/`strpbrk` 把「跳过与取出」写成一趟循环；
> `strtok` 会改写原串、藏内部状态、跳过空字段，传字面量会崩；
> 不想改原串就用 `strcspn` + `strspn` + `memchr` 手写十行。

---

# 第 4 节 重叠与越界：实测与真实后果

## 4.1 `memcpy` 重叠的实测

「重叠时用 `memmove`」这条规则，背下来很容易，理解它为什么值得多写三个字母却不容易。
**标准给的是「未定义行为」，而本机跑出来的结果往往完全正常**，这正是它危险的地方。
下面这个程序把常见的重叠情形扫了一遍：40 种距离乘 7 种长度，共 280 组，
每组都用同一个初始内容分别走 `memcpy` 与 `memmove`，再逐字节比较。

`C`

```c
/* overlap_truth.c    编译：gcc -std=c23 -O2 overlap_truth.c -o overlap_truth
 * 用 volatile 函数指针强制调用运行库里的 memcpy，避免编译器把调用内联掉。
 */
#include <stdio.h>
#include <string.h>

#define BUFSZ 256

static void *(*volatile real_memcpy)(void *, const void *, size_t) = memcpy;
static void *(*volatile real_memmove)(void *, const void *, size_t) = memmove;

static int one_case(int off, int len, int *first_diff) {
    unsigned char a[BUFSZ], b[BUFSZ];
    for (int i = 0; i < BUFSZ; i++) a[i] = b[i] = (unsigned char)(i * 37 + 11);

    real_memcpy(a + 64 + off, a + 64, (size_t)len);      /* 重叠：dst 在后 */
    real_memmove(b + 64 + off, b + 64, (size_t)len);

    int diff = 0;
    *first_diff = -1;
    for (int i = 0; i < BUFSZ; i++) {
        if (a[i] != b[i]) { if (*first_diff < 0) *first_diff = i; diff++; }
    }
    return diff;
}

int main(void) {
    static const int lens[] = {1, 3, 8, 15, 16, 33, 64};
    int total = 0, bad = 0;

    for (int off = 1; off <= 40; off++) {
        for (int k = 0; k < 7; k++) {
            int first = -1;
            if (one_case(off, lens[k], &first)) bad++;
            total++;
        }
    }
    printf("共 %d 组，结果与 memmove 不同的有 %d 组\n", total, bad);
    return 0;
}
```

`实测数据`
`Text`

```text
Windows 侧 gcc 15.2（MinGW-w64），-O0：共 280 组，结果与 memmove 不同的有 0 组
Windows 侧 gcc 15.2（MinGW-w64），-O2：共 280 组，结果与 memmove 不同的有 0 组
Linux 侧 gcc 13.3（glibc），-O2：共 280 组，结果与 memmove 不同的有 0 组
反向的 280 组（目标在源前面），-O2：共 280 组，结果与 memmove 不同的有 0 组
```

**1120 组用例，一组差异都没有。** 两个平台上运行库里的 `memcpy` 恰好都能处理这些重叠，
于是这段错代码「看起来完全正常」——**这就是未定义行为的常见形态**：
它不保证失败，只保证没有保证。

**但换个写法就会真的坏掉。** 把源与目标都写成编译期已知的短串，
编译器会在 `-O2` 下把 `memcpy` 展开成几条机器指令，展开的顺序不再有任何照顾重叠的义务：

`C`

```c
/* 节选：上面那个程序里真正干活的三行（编译命令同上） */
    char buf[16] = "abcdef";
    memcpy(buf + 2, buf, 6);          /* 目标与源重叠：标准说这是未定义行为 */
    printf("[%s]\n", buf);
```

`实测数据`
`Text`

```text
Windows 侧 gcc 15.2：restrict_warn.c:7:5: warning: 'memcpy' accessing 6 bytes at offsets 2 and 0 overlaps 4 bytes at offset 2 [-Wrestrict]
Linux 侧 gcc 13.3：restrict_warn.c:7:5: warning: 'memcpy' accessing 6 bytes at offsets 2 and 0 overlaps 4 bytes at offset 2 [-Wrestrict]
```

`实测数据`
`Text`

```text
期望的内容      [ababcdef]
Windows 侧实际  [ababcdcd]
Linux 侧实际    [ababcdcd]
```

`[ababcdcd]` 是这样来的：`-O2` 把它展开成「每次搬 4 个字节」，
第一次搬完 `buf[2..5]` 已经被 `abcd` 覆盖，第二次再读 `buf[4..5]` 时读到的已经是
被自己改过的 `c`、`d`。**两个平台给出了完全相同的错数据，因为这一步是编译器在前端做的。**

`-Wrestrict` 这条警告只在编译器能看见重叠时才会出现。上面那份 280 组的扫描里，
距离和长度都是运行期变量，编译器看不见重叠，于是既没有警告，运行结果也正常。

最后一行是 Linux 侧的实测，报告原文：

`实测数据`
`Text`

```text
==464==ERROR: AddressSanitizer: memcpy-param-overlap: memory ranges [0x7fda4c400022,0x7fda4c400028) and [0x7fda4c400020, 0x7fda4c400026) overlap
    #0 … in memcpy …
    #1 0x55ce3cacf334 in main asan_memcpy_overlap.c:14
    …
SUMMARY: AddressSanitizer: memcpy-param-overlap … in memcpy
    …
```

> [!CAUTION]
> **`memcpy-param-overlap` 是 AddressSanitizer 专门为这个错误准备的一类报告。**
> 看到它就说明两块内存重叠了，改法是换成 `memmove`，而不是去调长度或者加偏移。
> **注意它只在能检查到的情况下报**：本机 MinGW 的 gcc 15.2 没有 AddressSanitizer
> （链接时报 `cannot find -lasan`），即便有这个工具，上面那 280 组「碰巧正确」的用例
> 也说明了一件事——**没报错不等于写对了**。

## 4.2 越界写：静默损坏，或者当场终止

**越界写最坏的结果不是崩溃，而是写坏了旁边的数据然后继续跑。**
下面这个程序里两个成员挨在一起，`name` 溢出之后正好落进 `tag`：

`C`

```c
/* silent_overflow.c    编译：gcc -std=c23 silent_overflow.c -o silent_overflow
 * 没有 ASan 的时候，越界写是静默的：它写坏的是相邻的那个成员。
 */
#include <stdio.h>
#include <string.h>

struct record {
    char name[8];        /* 溢出就从这里往后写 */
    char tag[8];
};

int main(void) {
    struct record r;
    memset(&r, 0, sizeof r);

    strcpy(r.tag, "TAG-OK");
    printf("写之前：name [%s]，tag [%s]\n", r.name, r.tag);

    strcpy(r.name, "0123456789ABC");     /* 15 字节写进 8 字节的 name */
    printf("写之后：name [%s]，tag [%s]\n", r.name, r.tag);
    printf("sizeof(struct record) = %zu，两个成员相隔 %zu 字节\n",
           sizeof r, (size_t)(r.tag - r.name));
    return 0;
}
```

`实测数据`
`Text`

```text
Windows 侧 gcc 15.2，-O0 与 -O2 都是：
写之前：name []，tag [TAG-OK]
写之后：name [0123456789ABC]，tag [89ABC]
sizeof(struct record) = 16，两个成员相隔 8 字节
```

**`tag` 从 `TAG-OK` 变成了 `89ABC`，程序正常退出，退出码 0，编译时连警告都没有。**
这段代码在 Windows 侧两种优化级别下的输出完全一样。

同一份代码在 Linux 侧的遭遇不同：

`实测数据`
`Text`

```text
Linux 侧 gcc 13.3，-O2：
silent_overflow.c: warning: '__builtin___memcpy_chk' writing 14 bytes into a region of size 8 overflows the destination [-Wstringop-overflow=]
*** buffer overflow detected ***: terminated
Aborted
```

glibc 的 `_FORTIFY_SOURCE` 在运行时核对了目标对象的大小，直接终止了进程。
把这一层加固关掉（`-D_FORTIFY_SOURCE=0`），Linux 侧也给出与 Windows 一样的静默结果：

`实测数据`
`Text`

```text
Linux 侧 gcc 13.3，-O2 -D_FORTIFY_SOURCE=0：
写之后：name [0123456789ABC]，tag [89ABC]
```

**同一段错代码，三种表现，取决于平台与编译选项。**
这解释了为什么「在我机器上是好的」这句话没有意义：
**没有反馈不等于没有错误，只是这一次没有任何提示。**

`strncat` 有一处同样容易踩的地方：它的 `n` 只管追加几个字符，不管目标还剩多少空间。
第 1.5 小节那个程序里，6 字节的缓冲区被 `strncat(small, "defgh", 5)` 写进了 9 字节，
Windows 侧打印出了 `[abcdefgh]`——多出来的字节落在相邻的栈空间上。
同一份代码在 Linux 侧带 AddressSanitizer 编译，运行时报：

`实测数据`
`Text`

```text
strncat_demo.c:22:5: warning: 'strncat' specified bound 5 equals source length [-Wstringop-overflow=]
==558==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7f81ffc00026 at pc 0x7f8201e55303
WRITE of size 6 at 0x7f81ffc00026 thread T0
    #0 … in memcpy …
    #1 0x55badedf3675 in main strncat_demo.c:22
    …
  This frame has 4 object(s):
    …
```

## 4.3 用 AddressSanitizer 抓住它

**这一小节的三份报告都来自同一条命令**：在 Linux 侧用
`gcc -std=c2x -fsanitize=address -g 源文件.c -o 可执行文件` 编译。
Windows 侧的 MinGW 没有 AddressSanitizer，这是它目前最实际的短板。

**第一种：`strcpy` 写越界。**

`C`

```c
/* asan_strcpy.c    编译：gcc -std=c23 -fsanitize=address -g asan_strcpy.c -o asan_strcpy */
#include <stdio.h>
#include <string.h>

int main(void) {
    char small[8];
    strcpy(small, "0123456789");       /* 11 字节写进 8 字节 */
    printf("%s\n", small);
    return 0;
}
```

`实测数据`
`Text`

```text
==421==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7f771f600028 at pc 0x7f7721a88303
WRITE of size 11 at 0x7f771f600028 thread T0
    #0 … in memcpy …
    #1 0x5622d92822ca in main asan_strcpy.c:7
    …
    …
```

**三行关键信息**：`WRITE of size 11` 说明写出去多远；
`'small' (line 6)` 说明越界的是哪个对象、在哪一行声明；
`main asan_strcpy.c:7` 说明是哪一行干的。

**第二种：没有结尾 0 的字符串被当成字符串用。**

`C`

```c
/* asan_strncpy_read.c
 * 编译：gcc -std=c23 -fsanitize=address -g asan_strncpy_read.c -o asan_strncpy_read
 * 经典坑：用 strlen 当长度，拷贝完一个结尾 0 都没有，接着按字符串用。
 */
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *src = "hello";
    char dst[5];                        /* 5 字节，正好被填满 */

    strncpy(dst, src, strlen(src));     /* 一个 0 也没写 */
    printf("长度 %zu\n", strlen(dst));  /* 从 dst 之后继续找 0：越界读 */
    return 0;
}
```

`实测数据`
`Text`

```text
==411==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7f777ad00025 at pc 0x7f777d11c96f
READ of size 6 at 0x7f777ad00025 thread T0
    #0 … in strlen …
    #1 0x56191d40d311 in main asan_strncpy_read.c:13
    …
    …
```

**`strncpy(dst, src, strlen(src))` 是这一族里最常见的错法**：
它一个字节都不多写，也一个 0 都不补。要定长复制就用
`snprintf(dst, sizeof dst, "%s", src)`，或者复制之后自己补 0（补 0 的下标必须由
缓冲区大小算出来，不能由源串长度算出来）。

**第三种：`strlen` 撞上一块没有结尾 0 的内存。** 把上例的 `dst[5]` 换成
`char buf[5] = {'a', 'b', 'c', 'd', 'e'};`，只调用一次 `strlen(buf)`，
ASan 给出的是同一类报告：`READ of size 6`，`[32, 37) 'buf' (line 6)`，
`SUMMARY: AddressSanitizer: stack-buffer-overflow … in strlen`。
**`<string.h>` 里凡是按字符串工作的函数，都在同一处依赖那个 0。**

> [!WARNING]
> **AddressSanitizer 只护对象边界，不护结构体内部的成员。**
> 第 4.2 小节那个 `struct record` 的溢出，在本机 Linux 侧带 ASan 运行时
> **没有报错，依然打印出 `tag 现在是 [89ABC]`**——
> 因为写坏的位置仍在同一个结构体对象内部。
> 这类问题要靠代码评审、单元测试和断言来发现，
> 不能指望工具抓住一切。

**把这三样工具放进日常**：

| 工具 | 命令 | 抓什么 |
|---|---|---|
| 警告 | `-Wall -Wextra` | 格式串不匹配、`strncpy` 截断、参数重叠（`-Wrestrict`） |
| ASan | `-fsanitize=address -g` | 越界读写、悬垂、`memcpy` 重叠、双重释放 |
| 静态检查 | `gcc -fanalyzer` | 一部分越界与空指针路径 |

> [!NOTE]
> **第 4 节小结**：`memcpy` 重叠是未定义行为，本机 1120 组用例全部「碰巧正确」，
> 但编译期已知长度时 `-O2` 会真的写错（`[ababcdcd]`），ASan 则当场报 `memcpy-param-overlap`；
> 越界写在不同平台上分别是静默损坏、运行时终止、ASan 报告；
> **ASan 不检查结构体成员之间的越界**。

---

# 第 5 节 数字转换：`atoi` 与 `strtol`

## 5.1 `atoi` 出错时什么也不说

`atoi` 的签名里没有错误出口：它返回 `int`，没有 `endptr`，也不设 `errno`。
标准对它的描述是「除了出错时的行为，等价于 `(int)strtol(nptr, nullptr, 10)`」：

`文档`

> "Except for the behavior on error, they are equivalent to
> atoi: (int)strtol(nptr, nullptr, 10)"
>
> —— N3220 §7.24.1.2/2

**「除了出错时的行为」这几个字，就是它全部的坑。**

`实测数据`
`Text`

```text
atoi("0") = 0，errno = 0
atoi("abc") = 0，errno = 0
atoi("") = 0，errno = 0
atoi("99999999999999999999") = 1661992959，errno = 0
atoi("12abc") = 12，errno = 0
INT_MAX = 2147483647，LONG_MAX = 2147483647
```

**四种完全不同的输入，`errno` 全是 0**：合法的 0、根本不是数字的 `"abc"`、
超出一整个数量级的数字、后面还跟着垃圾的 `"12abc"`。
其中 `"99999999999999999999"` 给出的 `1661992959` 是溢出之后的残值，
它会随实现变化，**但这个数字本身看起来完全像个正常结果**。

> [!CAUTION]
> **只要输入的来源不受控制（命令行参数、配置文件、网络报文），就不能用 `atoi`。**
> 它把「转换失败」和「转换结果是 0」合并成同一个返回值，
> 调用方没有任何办法区分这两件事。用户输入 `abc` 得到 0，
> 用户输入 `0` 也得到 0——这在配置校验里就是一个可以被利用的缺口。

## 5.2 `strtol` 的三件套

`strtol` 多出三样东西：**`endptr`、`errno`、以及一个比 `int` 宽的返回类型**。

`文档`

> "If the subject sequence is empty or does not have the expected form, no
> conversion is performed; the value of nptr is stored in the object pointed
> to by endptr, provided that endptr is not a null pointer."
>
> "If the correct value is outside the range of representable values, LONG_MIN,
> LONG_MAX, ... is returned ... and the value of the macro ERANGE is stored in errno."
>
> —— N3220 §7.24.1.7/7、§7.24.1.7/8

`C`

```c
/* atoi_strtol.c    编译：gcc -std=c23 atoi_strtol.c -o atoi_strtol */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *cases[] = {"42", "  -7", "12abc", "abc", "99999999999999999999", "2147483648"};
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const char *s = cases[i];
        char *end = NULL;

        errno = 0;
        long v = strtol(s, &end, 10);
        int e1 = errno;
        printf("输入 [%s]\n", s);
        printf("  strtol  -> %-22ld errno = %d，停在偏移 %d\n", v, e1, (int)(end - s));

        errno = 0;
        int a = atoi(s);
        printf("  atoi    -> %-22d errno = %d（错误信息一个也没有）\n", a, errno);
    }
    return 0;
}
```

`实测数据`
`Text`

```text
输入 [42]
  strtol  -> 42                     errno = 0，停在偏移 2
  atoi    -> 42                     errno = 0（错误信息一个也没有）
输入 [  -7]
  strtol  -> -7                     errno = 0，停在偏移 4
  atoi    -> -7                     errno = 0（错误信息一个也没有）
输入 [12abc]
  strtol  -> 12                     errno = 0，停在偏移 2
  atoi    -> 12                     errno = 0（错误信息一个也没有）
输入 [abc]
  strtol  -> 0                      errno = 0，停在偏移 0
  atoi    -> 0                      errno = 0（错误信息一个也没有）
输入 [99999999999999999999]
  strtol  -> 2147483647             errno = 34，停在偏移 20
  atoi    -> 1661992959             errno = 0（错误信息一个也没有）
输入 [2147483648]
  strtol  -> 2147483647             errno = 34，停在偏移 10
  atoi    -> -2147483648            errno = 0（错误信息一个也没有）
```

**三种失败各有各的判据**，这是本节最要紧的一张表：

| 输入 | `strtol` 的返回 | `endptr` | `errno` | 判据 |
|---|---|---|---|---|
| `"42"` | 42 | 指向结尾 | 0 | 成功 |
| `"12abc"` | 12 | 指向 `'a'` | 0 | **只转了一部分**：`*endptr != '\0'` |
| `"abc"` | 0 | **等于起始地址** | 0 | **一个数字都没吃到**：`endptr == s` |
| 超出范围 | `LONG_MAX` | 指向结尾 | **`ERANGE`（本机 34）** | `errno == ERANGE` |
| `"  -7"` | −7 | 指向结尾 | 0 | 成功，前导空白与正负号都吃掉了 |

> [!IMPORTANT]
> **`errno` 不会自己清零，必须先置 0 再调用。**
> 上面每一轮都写了 `errno = 0;` 这一句；漏掉它，上一轮留下的 `ERANGE`
> 会被当成本次的结果，或者本次真正的 `ERANGE` 被上一轮的 0 掩盖。
> 错误码一律遵守这个用法，`<errno.h>` 的完整说明见
> 《06-标准库/A-05-工具与其它：stdlib 与杂项.md》第 7 节。

**范围判断要连着类型一起做。** 本机 `long` 与 `int` 都是 32 位，
`strtol` 返回 `LONG_MAX` 就等于 `INT_MAX`；但在 Linux 上 `long` 是 64 位，
`strtol("2147483648")` 会正常返回 2147483648 且 `errno` 为 0，
**把它赋给 `int` 就悄悄溢出了**。因此拿到 `long` 之后还要比一次范围：

| 目标类型 | 该用什么 | 还要做的检查 |
|---|---|---|
| `long` | `strtol` | `errno == ERANGE` |
| `long long` | `strtoll` | `errno == ERANGE` |
| `int` | `strtol` + 比较 | `errno == ERANGE` 且 `INT_MIN <= v && v <= INT_MAX` |
| 无符号 | `strtoul` / `strtoull` | 负号的处理与 `ERANGE` |

## 5.3 写成能报错的解析函数

把上面那些判据组装成一个函数，就是工程里真正该用的形态：
**返回值区分「成功」与「哪种失败」，结果通过指针带出来**。

`C`

```c
/* parse_int.c    编译：gcc -std=c23 -Wall -Wextra parse_int.c -o parse_int
 * 一个能报错的整数解析函数：strtol 的 endptr + errno 全用上。
 */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

enum { OK = 0, ERR_EMPTY, ERR_CHAR, ERR_RANGE, ERR_TAIL };

static int parse_int(const char *s, int *out) {
    if (!s || !*s) return ERR_EMPTY;               /* 空串 */

    errno = 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);

    if (end == s) return ERR_CHAR;                 /* 一个数字都没吃到 */
    while (*end == ' ' || *end == '\t') end++;     /* 允许尾随空白 */
    if (*end != '\0') return ERR_TAIL;             /* 后面还有别的东西 */
    if (errno == ERANGE || v < INT_MIN || v > INT_MAX) return ERR_RANGE;

    *out = (int)v;
    return OK;
}

int main(void) {
    const char *cases[] = {"42", "  -7", "12abc", "abc", "", "99999999999999999999",
                           "2147483647", "  100  "};
    const char *names[] = {"OK", "空串", "一个数字都没吃到", "超出范围", "后面还有别的东西"};

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int v = 0;
        int rc = parse_int(cases[i], &v);
        if (rc == OK) printf("[%-22s] -> %d\n", cases[i], v);
        else          printf("[%-22s] -> 失败：%s\n", cases[i], names[rc]);
    }
    return 0;
}
```

`实测数据`
`Text`

```text
[42                    ] -> 42
[  -7                  ] -> -7
[12abc                 ] -> 失败：后面还有别的东西
[abc                   ] -> 失败：一个数字都没吃到
[                      ] -> 失败：空串
[99999999999999999999  ] -> 失败：超出范围
[2147483647            ] -> 2147483647
[  100                 ] -> 100
```

**四步顺序不能换**：先判空串，再判「一个数字都没吃到」，再判尾部残留，
最后判范围。范围判断要放在最后，因为 `endptr` 已经指向结尾时，
才谈得上「这个数整体是不是超范围」。

| 函数 | 类型 | 出错时 | 什么时候用 |
|---|---|---|---|
| `atoi` / `atol` / `atoll` | `int` / `long` / `long long` | 静默 | **不推荐**，除非输入绝对可信 |
| `strtol` 一族 | 整型，支持 2..36 进制 | `endptr` + `errno` | 解析整数 |
| `strtod` / `strtof` | 浮点，认 `inf`、`nan`、十六进制浮点 | `endptr` + `errno` | 解析浮点 |
| `strtoimax` / `strtoumax` | `<inttypes.h>` 的最宽整型 | 同上 | 需要定宽时 |

**C++ 侧对应的是 `<charconv>` 的 `from_chars`**，它不用 `errno`，
而是把「有没有出错」放进返回值里，用法与 `strtol` 是两种风格，
见《06-标准库/B-05-数值.md》。

> [!NOTE]
> **第 5 节小结**：`atoi` 无错误出口，四种错法都返回 0 或残值且 `errno` 不变；
> `strtol` 靠 `endptr == s`（没吃到数字）、`*endptr != '\0'`（尾部残留）、
> `errno == ERANGE`（超范围）三条判据区分失败；
> `errno` 必须先清零，返回 `long` 之后还要按目标类型再比一次范围。

---

# 第 6 节 与 C++ 的对照

## 6.1 `<cstring>` 与 `<string.h>`

C++ 把 C 的头文件重新包了一遍：`<string.h>` 有了 C++ 版本 `<cstring>`，
名字进 `std::`。**两边的函数是同一批，`<cstring>` 不是新实现。**

`C++`

```cpp
/* cstring_pair.cpp    编译：g++ -std=c++17 -Wall -Wextra cstring_pair.cpp -o cstring_pair */
#include <cstring>
#include <iostream>
#include <string>

int main() {
    /* 一、<cstring> 里的名字在 std:: 里，同时在全局命名空间里（实现通常如此） */
    const char *c = "hello";
    std::cout << "std::strlen = " << std::strlen(c) << "，::strlen = " << ::strlen(c) << "\n";

    /* 二、同一个任务：拼接与比较 */
    char buf[32];
    std::strcpy(buf, "Hello");
    std::strcat(buf, ", world");
    std::cout << "C 风格：" << buf << "，长度 " << std::strlen(buf) << "\n";

    std::string s = "Hello";
    s += ", world";
    std::cout << "std::string：" << s << "，长度 " << s.size() << "\n";

    std::cout << "strcmp 判等：" << (std::strcmp(buf, "Hello, world") == 0) << "\n";
    std::cout << "operator== 判等：" << (s == "Hello, world") << "\n";

    /* 三、查找：C 版返回指针，C++ 版返回下标 */
    const char *p = std::strstr(buf, "world");
    std::cout << "strstr 之后剩下：" << (p ? p : "(没有)") << "\n";
    std::cout << "find 返回下标：" << s.find("world") << "\n";

    /* 四、互操作：std::string 的 c_str() 给 C 函数用 */
    std::printf("交给 printf：%s\n", s.c_str());
    return 0;
}
```

`实测数据`
`Text`

```text
std::strlen = 5，::strlen = 5
C 风格：Hello, world，长度 12
std::string：Hello, world，长度 12
strcmp 判等：1
operator== 判等：1
strstr 之后剩下：world
find 返回下标：7
交给 printf：Hello, world
```

`<cstring>` 的完整对照与 `string_view` 的边界见
《06-标准库/B-02-std-string 与 string_view.md》第 5、6 节。

## 6.2 同一个任务：切分与求和

把 `"a=1;bb=22;ccc=333"` 拆成键值对并求和，两种写法各写一遍。
**两边的输出必须完全一致**，否则就是有一边写错了。

`C`

```c
/* pair_split.c    编译：gcc -std=c23 -Wall -Wextra pair_split.c -o pair_split
 * 任务：把 "a=1;bb=22;ccc=333" 拆成键值对并求和。
 * C 的写法：char[] + strcspn + strtol。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char line[] = "a=1;bb=22;ccc=333";
    long sum = 0;
    int n = 0;

    for (char *p = line; *p; ) {
        char *eq = strchr(p, '=');
        if (!eq) break;
        *eq = '\0';                       /* 就地切开，键与值各成一段 */
        char *semi = strchr(eq + 1, ';');
        if (semi) *semi = '\0';

        long v = strtol(eq + 1, NULL, 10);
        printf("键 [%s] 值 %ld\n", p, v);
        sum += v;
        n++;

        if (!semi) break;
        p = semi + 1;
    }
    printf("共 %d 项，合计 %ld\n", n, sum);
    return 0;
}
```

`C++`

```cpp
/* pair_split.cpp    编译：g++ -std=c++17 -Wall -Wextra pair_split.cpp -o pair_split
 * 同一个任务：std::string + find + stol。
 */
#include <iostream>
#include <string>

int main() {
    std::string line = "a=1;bb=22;ccc=333";
    long sum = 0;
    int n = 0;
    std::size_t pos = 0;

    while (pos <= line.size()) {
        std::size_t semi = line.find(';', pos);
        std::string item = line.substr(pos, semi - pos);     // npos 会一直取到结尾
        if (!item.empty()) {
            std::size_t eq = item.find('=');
            long v = std::stol(item.substr(eq + 1));
            std::cout << "键 [" << item.substr(0, eq) << "] 值 " << v << "\n";
            sum += v;
            n++;
        }
        if (semi == std::string::npos) break;
        pos = semi + 1;
    }
    std::cout << "共 " << n << " 项，合计 " << sum << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
两边输出相同：
键 [a] 值 1
键 [bb] 值 22
键 [ccc] 值 333
共 3 项，合计 356
```

**差别在出错的时候**：C 版把 `=` 就地改成 0，原串被破坏，
`strtol` 失败会静默给出 0；C++ 版的 `substr` 各复制一份，原串不动，
`std::stol` 遇到非法输入抛 `std::invalid_argument`。
`std::stol` 的这三个细节（异常、`pos` 参数、与 `from_chars` 的取舍）在
《06-标准库/B-02-std-string 与 string_view.md》第 7 节。

**`std::string` 接走了本章大部分工作，但下面几种场合仍然要用 `char[]` 和 `<string.h>`**：
与 C 接口打交道时对方的签名就是 `char *`；需要一块不会被重新分配的连续内存时
（`std::string` 扩容会搬家，`string_view` 与旧指针随之失效，见
《06-标准库/B-02-std-string 与 string_view.md》第 4 节）；栈上的小缓冲区与嵌入式环境；
以及报文与协议帧的解析——`memchr`/`memcmp`/`memcpy` 的语义与协议一致。
`std::string` 与 C 字符串的边界在同章第 5 节：`c_str()` 给的是只读指针，
任何修改都可能让它失效。

---

# 第 7 节 速查表

## 7.1 常用件一览

| 名字 | 一句话用途 | 典型坑 |
|---|---|---|
| `strlen` | 数结尾 0 之前的字节数 | 返回 `size_t`，减法回绕；没有 0 就一路读下去 |
| `strcpy` / `strcat` | 拷贝 / 追加整个字符串 | 不检查目标大小；`strcat` 在循环里有平方代价 |
| `strncpy` | 最多写 n 个字节 | **不保证补结尾 0**；n 是「写几个字节」 |
| `strncat` | 最多追加 n 个字符 | **n 与目标剩余空间无关**；目标必须已是字符串 |
| `strcmp` / `strncmp` | 比较字符串 / 比较前 n 个字节 | 只保证返回值符号；`strncmp` 不能判整串相等 |
| `strchr` / `strrchr` | 找第一个 / 最后一个字符 | 找不到给 `NULL`；`strchr(s, 0)` 指向结尾 0 |
| `strstr` | 找子串 | 空子串返回首地址，不是 `NULL` |
| `strspn` / `strcspn` | 数开头「属于 / 不属于」集合的字符数 | 名字容易记反，`c` 是 complement |
| `strpbrk` | 找集合里最先出现的字符 | 返回指针，找不到给 `NULL` |
| `strtok` | 按分隔符切分 | **改写原串**、藏内部状态、跳过空字段、不可重入 |
| `strtok_r` | 可重入版本（POSIX） | 不是 C 标准的名字，移植前先确认 |
| `memcpy` | 拷贝 n 个字节 | **重叠时未定义**；`sizeof` 用在指针上只拷 8 字节 |
| `memmove` | 拷贝 n 个字节，允许重叠 | 不确定会不会重叠时就用它 |
| `memset` | 按**字节**填充 | 只能填 0 或按字节有意义的值 |
| `memcmp` | 比较 n 个字节 | 不认结尾 0；只能拿返回值与 0 比 |
| `memchr` | 在 n 个字节里找某个字节 | 二进制数据里替代 `strchr` |
| `strdup` / `strndup` | 复制一份到 `malloc` 的空间 | C23 才有；本机头文件里没有 `strndup` |
| `snprintf` | 格式化到定长缓冲区 | 返回值是「本应写多少」，不是「写了多少」 |
| `atoi` 一族 | 文本转整数 | **出错时静默**，不要用于不可信输入 |
| `strtol` 一族 | 文本转整数，可检测错误 | `errno` 要先清零；返回 `long` 还要比范围 |

## 7.2 配套件与相关章节

配套示例见 [`B-examples/06-standard-library/01-c-stdlib-toolbox/`](../B-examples/06-standard-library/01-c-stdlib-toolbox/)，
配套练习见 [`C-templates/06-standard-library/01-c-stdlib-toolbox/`](../C-templates/06-standard-library/01-c-stdlib-toolbox/)。
两者做的是同一件事：读一段文本、切词、计数、排序、输出报表——
正好把本章的 `strlen`/`strtok`/`memcpy`/`strtol` 与《06-标准库/A-05-工具与其它：stdlib 与杂项.md》的
`qsort` 串起来。C++ 侧的对应件在《06-标准库/B-02-std-string 与 string_view.md》第 1 节。

| 相关章节 | 关系 |
|---|---|
| 《04-语法/10-字符串.md》第 1 节 | **前置**：C 风格字符串这条约定 |
| 《04-语法/08-数组、指针与引用.md》第 5 节 | **前置**：越界与段错误的一般情形 |
| 《06-标准库/A-00-导读：C 标准库.md》第 5.2 小节 | **背景**：`strncpy` 为什么是设计遗留 |
| 《06-标准库/A-01-输入输出：stdio.md》第 3 节 | **上游**：`fgets` 读进来的字节 |
| 《06-标准库/A-03-数值、数学与随机.md》 | **下游**：解析出来的数怎么算 |
| 《06-标准库/A-05-工具与其它：stdlib 与杂项.md》第 1 节 | **配套**：`strdup` 的内存谁来释放 |
| 《06-标准库/B-02-std-string 与 string_view.md》第 1 节 | **对照**：同一个任务的两种写法 |
| 《06-标准库/B-02-std-string 与 string_view.md》第 8 节 | **延伸**：字节数与字符数 |
| 《06-标准库/B-05-数值.md》 | **对照**：`from_chars` 的错误处理风格 |
