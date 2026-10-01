# 时间与日期：`<time.h>`

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**「记个时间」「算个日期」「看看这段代码跑了多久」，三件事都归 `<time.h>` 管。**

**它是 C 标准库里最不整齐的一个头文件。** 一半内容标准定得很死——
`time_t` 是从某个起点数下来的秒数、`tm_year` 从 1900 起算；
另一半完全交给实现——**起点是哪个时刻、能表示到哪一年、本机在哪个时区、
夏令时怎么算、`strftime` 认哪些格式符**。同一段代码在两个平台上跑出不同的结果，
在这个头文件上是常态。

**因此本章凡是涉及「实现说了算」的地方，都给出两个平台的实际运行结果。**
Windows 侧用 MinGW-w64 的 GCC 15.2.0，Linux 侧用 WSL Ubuntu 24.04 的 GCC 13.3.0
（glibc 2.39）；两者差得最远的是 `clock()` 的含义与 `strftime` 支持的格式符。
另外，**本章节的每个程序都遇到过一次「静态缓冲区」问题**，
它值得单独占一节。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 复现环境与命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 内容 | 在哪一节 |
|---|---|
| **`time_t`、`clock_t`、`struct tm` 三个类型**，`time`、`difftime`、`clock` | **一** |
| **`gmtime`、`localtime`、`mktime`、`asctime`、`ctime`**，静态缓冲区的坑 | **二** |
| **时区与夏令时的现实**，`TZ` 环境变量在两个平台上的差别 | **三** |
| **`strftime` 的格式符**，哪些本机不支持，缓冲区不够会怎样 | **四** |
| **`timespec_get`（C11）**，以及本机为什么没有 | **五** |
| **为什么测耗时不该用 `clock()`** | **六** |
| 速查表与配套件 | 七 |

| 需要先知道 | 在哪 |
|---|---|
| 整数类型家族与它们的宽度 | 《04-语法/02-数据类型与类型系统.md》第 2.1 小节 |
| 结构体与成员访问 | 《04-语法/09-结构体、联合体与 enum.md》第 1 节 |
| 指针与空指针 | 《04-语法/08-数组、指针与引用.md》第 1.3 小节 |
| 链接阶段与库的顺序 | 《01-编译器/01-编译与链接.md》第 2.5 小节 |

**相邻的章节**：同一板块的《06-标准库/A-03-数值、数学与随机.md》第 4 节
讲的是同一类「实现说了算」的东西（随机数）；
C++ 侧的对照在《06-标准库/B-06-时间：chrono.md》第 2 节与第 3 节：
那里有 `duration`、`time_point` 与三种 clock，本章节第 6 节的结论在那里有对应的做法。

---

# 第 1 节 三种时间，三个类型

## 1.1 `time_t`：从某个起点数下来的秒数

`time_t` 是最基本的那个：**一个算术类型，表示从「某个时刻」到今天过了多少秒**。

**那个「某个时刻」叫纪元（epoch），标准没有规定它是哪一刻。**
实践中几乎所有平台都用 1970-01-01 00:00:00 UTC，
但这是约定，不是标准——C23 只说 `time_t` 是「能够表示日历时间的算术类型」。

**它通常是 64 位有符号整数。** 本机实测 `sizeof(time_t)` 是 8，
Linux 上也是 8；用 `%lld` 打印时先转成 `long long` 是稳妥做法。

**2038 年问题来自「`time_t` 是 32 位有符号」的实现**：
32 位有符号能表示到 2147483647 秒，也就是 2038-01-19 03:14:07 UTC。
本机是 64 位，不受影响，但**把 `time_t` 截成 `int` 就等价于把这个问题搬回来**——
下面这个程序的最后两行就是。

`实测数据`
`C`

```c
/* time_epoch.c    编译：gcc -std=c23 time_epoch.c -o time_epoch */
#include <stdio.h>
#include <time.h>

int main(void) {
    /* 纪元：time_t 值为 0 的那一刻，用 gmtime 看它落在哪一天 */
    time_t zero = 0;
    struct tm g = *gmtime(&zero);            /* 立刻拷出来，静态缓冲区会被覆盖 */
    printf("time_t = 0 对应的 UTC：%04d-%02d-%02d %02d:%02d:%02d\n",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
           g.tm_hour, g.tm_min, g.tm_sec);
    printf("sizeof(time_t) = %zu\n", sizeof(time_t));
    printf("(time_t)-1 < 0 = %d（1 表示有符号）\n", (time_t)-1 < 0);
    /* 32 位有符号 time_t 的上界，就是 2038 问题的那一刻 */
    time_t max32 = (time_t)2147483647;
    struct tm m = *gmtime(&max32);
    printf("2147483647 对应的 UTC：%04d-%02d-%02d %02d:%02d:%02d\n",
           m.tm_year + 1900, m.tm_mon + 1, m.tm_mday,
           m.tm_hour, m.tm_min, m.tm_sec);
    /* 把 time_t 截成 int，等于把 2038 问题搬回来 */
    time_t big = (time_t)3000000000LL;
    printf("3000000000 原样打印 = %lld\n", (long long)big);
    printf("截成 int 之后打印   = %d\n", (int)big);
    return 0;
}
```

`实测数据`
`Text`

```text
time_t = 0 对应的 UTC：1970-01-01 00:00:00
sizeof(time_t) = 8
(time_t)-1 < 0 = 1（1 表示有符号）
2147483647 对应的 UTC：2038-01-19 03:14:07
3000000000 原样打印 = 3000000000
截成 int 之后打印   = -1294967296
```

**本机的纪元的确是 1970-01-01 00:00:00 UTC**（这是约定，不是标准），
32 位有符号的上界换算出来正是 2038-01-19 03:14:07 UTC。
**最后一行 `-1294967296` 就是把 `time_t` 截成 `int` 的结果**：
`3000000000` 放不进 32 位有符号，这个转换是**实现定义**的，
不同编译器给出的数可能不同，但一定不是 `3000000000`。

## 1.2 `clock_t`：处理器时间

`clock_t` 表示的是**处理器时间**，不是日历时间。它与 `time_t` 没有关系，
只是恰好都放在同一个头文件里。

`文档`

> "The clock function determines the processor time used."
>
> —— N3220 §7.29.2.1/2

**「处理器时间」这四个字的实现空间很大**，本机与 Linux 给出的答案完全不同（第 6 节）。

`CLOCKS_PER_SEC` 表示一秒有多少个 `clock_t` 单位。
**本机是 1000，Linux 是 1000000**——同一个宏，差了三个数量级
（这两个数由第 1.4 小节的 `time_basic.c` 打印）。

## 1.3 `struct tm`：拆开的日历

`struct tm` 把时间拆成九个人能读的字段：

| 字段 | 含义 | 取值范围 |
|---|---|---|
| `tm_sec` | 秒 | `[0, 60]`，60 用于闰秒 |
| `tm_min` | 分 | `[0, 59]` |
| `tm_hour` | 时 | `[0, 23]` |
| `tm_mday` | 日 | `[1, 31]` |
| `tm_mon` | 月 | **`[0, 11]`** |
| `tm_year` | 年 | **从 1900 起算的偏移** |
| `tm_wday` | 星期 | `[0, 6]`，周日是 0 |
| `tm_yday` | 一年中的第几天 | `[0, 365]` |
| `tm_isdst` | 是否夏令时 | 正数表示是，0 表示不是，负数表示「不知道」 |

**前六个字段的编号方式是这个结构体最容易出错的地方**：
月份从 0 起算，年份从 1900 起算。标准原文写得很直白：

`文档`

> "int tm_year; // years since 1900"
>
> —— N3220 §7.29.1

**字段的顺序不是标准规定的**，因此不要用 `{2024, 1, 15}` 这样的位置初始化
去写 `struct tm`——本机的顺序是 `sec, min, hour, mday, mon, year, wday, yday, isdst`，
换个实现就可能变。要么逐个字段赋值，要么用完全部九个初值。

`实测数据`
`C`

```c
/* tm_fields.c    编译：gcc -std=c23 tm_fields.c -o tm_fields */
#include <stdio.h>
#include <stddef.h>
#include <time.h>

int main(void) {
    /* 九个字段在内存里的先后顺序不是标准规定的 */
    printf("offsetof：sec=%zu min=%zu hour=%zu mday=%zu mon=%zu "
           "year=%zu wday=%zu yday=%zu isdst=%zu\n",
           offsetof(struct tm, tm_sec), offsetof(struct tm, tm_min),
           offsetof(struct tm, tm_hour), offsetof(struct tm, tm_mday),
           offsetof(struct tm, tm_mon), offsetof(struct tm, tm_year),
           offsetof(struct tm, tm_wday), offsetof(struct tm, tm_yday),
           offsetof(struct tm, tm_isdst));
    printf("sizeof(struct tm) = %zu\n", sizeof(struct tm));
    /* 2024-03-05 06:07:08，看两个「偏移式」字段怎么填 */
    struct tm t = {0};
    t.tm_year = 2024 - 1900;
    t.tm_mon  = 3 - 1;
    t.tm_mday = 5;
    t.tm_hour = 6;
    t.tm_min  = 7;
    t.tm_sec  = 8;
    t.tm_isdst = -1;
    mktime(&t);                     /* 归一化，顺便补上 wday 与 yday */
    printf("2024-03-05 06:07:08 -> tm_year=%d tm_mon=%d tm_mday=%d "
           "tm_wday=%d tm_yday=%d\n",
           t.tm_year, t.tm_mon, t.tm_mday, t.tm_wday, t.tm_yday);
    return 0;
}
```

`实测数据`
`Text`

```text
（Windows / MinGW-w64 15.2.0）
offsetof：sec=0 min=4 hour=8 mday=12 mon=16 year=20 wday=24 yday=28 isdst=32
sizeof(struct tm) = 36
2024-03-05 06:07:08 -> tm_year=124 tm_mon=2 tm_mday=5 tm_wday=2 tm_yday=64

（Linux / glibc 2.39，gcc 13.3.0，同一份源码）
offsetof：sec=0 min=4 hour=8 mday=12 mon=16 year=20 wday=24 yday=28 isdst=32
sizeof(struct tm) = 56
2024-03-05 06:07:08 -> tm_year=124 tm_mon=2 tm_mday=5 tm_wday=2 tm_yday=64
```

**两边的偏移完全一样，`sizeof` 却是 36 与 56**——顺序相同只是这两个实现凑巧一致，
标准没有规定过。**编码方式也在这里看到**：`2024` 存成 `tm_year=124`，
`3` 月存成 `tm_mon=2`，这两个偏移式字段是本节最容易写错的地方。

## 1.4 `time`、`difftime`、`clock` 的实测

`实测数据`
`C`

```c
/* time_basic.c    编译：gcc -std=c23 time_basic.c -o time_basic */
#include <stdio.h>
#include <time.h>

int main(void) {
    time_t now = time(NULL);
    printf("time(NULL)      = %lld\n", (long long)now);
    printf("time(&now)      = %lld\n", (long long)time(&now));
    printf("sizeof(time_t)  = %zu\n", sizeof(time_t));
    printf("sizeof(clock_t) = %zu\n", sizeof(clock_t));
    printf("CLOCKS_PER_SEC  = %ld\n", (long)CLOCKS_PER_SEC);
    printf("clock() 刚启动  = %ld\n", (long)clock());
    printf("time_t 有符号吗 : %d（1 表示有符号）\n", (time_t)-1 < 0);
    printf("difftime(176400, 0) = %.1f 秒\n", difftime((time_t)176400, (time_t)0));
    return 0;
}
```

`实测数据`
`Text`

```text
（Windows / MinGW-w64 15.2.0）
time(NULL)      = 1790825952
time(&now)      = 1790825952
sizeof(time_t)  = 8
sizeof(clock_t) = 4
CLOCKS_PER_SEC  = 1000
clock() 刚启动  = 0
time_t 有符号吗 : 1（1 表示有符号）
difftime(176400, 0) = 176400.0 秒

（Linux / glibc 2.39，gcc 13.3.0）
sizeof(clock_t) = 8
CLOCKS_PER_SEC  = 1000000
```

**其中三件事需要单独说明。**

**第一，`time(NULL)` 与 `time(&now)` 的返回值一样**：
参数传 `NULL` 就只取返回值，传一个 `time_t *` 就同时把结果写进那块内存。两种用法都对。

**第二，`difftime` 返回的是 `double`**，单位是秒。
**它是两个 `time_t` 相减唯一可移植的写法**——直接写 `t1 - t2` 得到的是 `time_t`，
单位是不是秒、能不能为负，都取决于实现。

**第三，`clock()` 刚启动时是 0。** 它的起点是「程序开始执行」，
不是纪元，因此 `clock()` 的绝对值没有意义，只有两次调用之差有意义。

## 1.5 本机能表示到哪一年

**`time_t` 是 64 位，不代表 `mktime` 与 `localtime` 什么年份都能处理。**
本机的实测范围比想象中窄：

`实测数据`
`C`

```c
/* time_range.c    编译：gcc -std=c23 time_range.c -o time_range */
#include <stdio.h>
#include <time.h>
/* 试一个本地日期的 00:00 能不能被 mktime 接受 */
static int try_date(int y, int mo, int d) {
    struct tm t = {0};
    t.tm_year = y - 1900;
    t.tm_mon  = mo - 1;
    t.tm_mday = d;
    t.tm_isdst = -1;
    return mktime(&t) != (time_t)-1;
}

int main(void) {
    int lo[]  = {1969, 1970, 1971};
    int hi[]  = {3000, 3001, 3002, 3003};
    int d31[] = {30, 31};
    printf("下限：");
    for (int i = 0; i < 3; i++)
        printf(" %d-01-01=%s", lo[i], try_date(lo[i], 1, 1) ? "可" : "否");
    printf("\n上限：");
    for (int i = 0; i < 4; i++)
        printf(" %d-01-01=%s", hi[i], try_date(hi[i], 1, 1) ? "可" : "否");
    printf("\n      ");
    for (int i = 0; i < 2; i++)
        printf(" 3001-12-%d=%s", d31[i], try_date(3001, 12, d31[i]) ? "可" : "否");
    printf("\n\n");
    /* 二分找出 localtime 能接受的最大 time_t */
    time_t low = 0, high = (time_t)41024448000LL;
    while (low + 1 < high) {
        time_t mid = low + (high - low) / 2;
        if (localtime(&mid)) low = mid;
        else                 high = mid;
    }
    printf("localtime 能接受的最大 time_t = %lld\n", (long long)low);
    struct tm m = *gmtime(&low);
    printf("换算成 UTC：%04d-%02d-%02d %02d:%02d:%02d\n",
           m.tm_year + 1900, m.tm_mon + 1, m.tm_mday,
           m.tm_hour, m.tm_min, m.tm_sec);
    printf("再大 1 秒 localtime 返回 %s\n", localtime(&high) ? "非空" : "NULL");
    return 0;
}
```

`实测数据`
`Text`

```text
（Windows / MinGW-w64 15.2.0）
下限： 1969-01-01=否 1970-01-01=否 1971-01-01=可
上限： 3000-01-01=可 3001-01-01=可 3002-01-01=否 3003-01-01=否
       3001-12-30=否 3001-12-31=否

localtime 能接受的最大 time_t = 32535244799
换算成 UTC：3001-01-01 07:59:59
再大 1 秒 localtime 返回 NULL

（Linux / glibc 2.39，gcc 13.3.0，同一份源码）
下限： 1969-01-01=可 1970-01-01=可 1971-01-01=可
上限： 3000-01-01=可 3001-01-01=可 3002-01-01=可 3003-01-01=可
       3001-12-30=可 3001-12-31=可

localtime 能接受的最大 time_t = 41024447999
换算成 UTC：3270-01-04 23:59:59
再大 1 秒 localtime 返回 非空
```

**Linux 侧一个「否」都没有**：二分的上界设在 3270 年，它到那里还没碰到天花板。
**因此上面那个 3001 年的上限是本机 `msvcrt.dll` 的限制，不是 `time_t` 的限制。**

**下限是 1970 年**：本机的 `mktime` 对 1970-01-01 之前的日期一律返回 `-1`。
「1970-01-01 可不可」是按**本地时间**判的——
本机在 UTC+8，本地 1970-01-01 00:00 对应 UTC 的 1969-12-31 16:00，
落在纪元之前，于是被判为不可表示。

**上限约在 3001 年初**，具体是 `time_t = 32535244799`，
再多一秒 `localtime` 就返回 `NULL`。这个数不是 `INT64_MAX`，
`time_t` 名义上有 8 字节，但库内部用的是另一套范围检查。

> [!IMPORTANT]
> **`time_t` 的类型宽度与「时间函数能处理的年份范围」是两件事。**
> 前者由类型决定，后者由库的实现决定。要处理历史日期（1970 年之前）
> 或很远的未来，`<time.h>` 这套接口在本机做不到，需要另找办法。

> [!NOTE]
> **本节回顾**：`time_t` 是从纪元的秒数，`clock_t` 是处理器时间的刻度；
> `struct tm` 里月份从 0 起算、年份从 1900 起算；
> 本机的时间函数只覆盖 1970 年到 3001 年初这一段。

---

# 第 2 节 拆分与还原

## 2.1 `gmtime` 与 `localtime`

**`time_t` 是给人算的，`struct tm` 是给人看的。** 两个方向各有两个函数：

`Text`

```text
     time_t  ──gmtime()──►     struct tm（UTC）
        │    ──localtime()──►  struct tm（本地时间）
        │
        └◄──mktime()────────── struct tm（按本地时间解释）
```

**`gmtime` 给的是 UTC，`localtime` 给的是本地时间**，两者之间的差就是时区偏移。
下面这个程序把同一时刻的两种拆法都打出来：

`实测数据`
`C`

```c
/* tm_split.c    编译：gcc -std=c23 tm_split.c -o tm_split */
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void) {
    time_t t = 1700000000;      /* 2023-11-14 22:13:20 UTC */
    /* localtime 与 gmtime 共用一块静态缓冲区，必须立刻把内容拷出来 */
    struct tm g = *gmtime(&t);
    struct tm l = *localtime(&t);
    printf("gmtime   : %04d-%02d-%02d %02d:%02d:%02d wday=%d yday=%d isdst=%d\n",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
           g.tm_hour, g.tm_min, g.tm_sec, g.tm_wday, g.tm_yday, g.tm_isdst);
    printf("tm_year 原值 = %d（从 1900 起算）\n", g.tm_year);
    printf("tm_mon  原值 = %d（从 0 起算）\n", g.tm_mon);
    printf("localtime: %04d-%02d-%02d %02d:%02d:%02d isdst=%d\n",
           l.tm_year + 1900, l.tm_mon + 1, l.tm_mday,
           l.tm_hour, l.tm_min, l.tm_sec, l.tm_isdst);
    /* mktime 把字段当「本地时间」还原，因此它不是 gmtime 的逆运算 */
    struct tm m = g;
    m.tm_isdst = -1;
    time_t back = mktime(&m);
    printf("mktime(gmtime 的结果) = %lld，与原值差 %.0f 秒\n",
           (long long)back, difftime(back, t));
    printf("换成小时：本地时间比 UTC 早 %+.0f 小时\n", difftime(t, back) / 3600.0);
    /* asctime 与 ctime 也是静态缓冲区，同样要先拷出来 */
    char s1[64], s2[64];
    strcpy(s1, asctime(&g));
    strcpy(s2, ctime(&t));
    printf("asctime 长度 = %zu，末字符编码 = %d（换行是 10）\n",
           strlen(s1), (int)s1[strlen(s1) - 1]);
    printf("asctime = [%s]", s1);
    printf("ctime   = [%s]", s2);
    printf("上面两个方括号里各多出一个空行，因为字符串自带 \\n\n");
    /* mktime 会归一化越界的字段 */
    struct tm n = {0};
    n.tm_year = 2024 - 1900; n.tm_mon = 0; n.tm_mday = 32;
    n.tm_hour = 25; n.tm_min = 70; n.tm_isdst = -1;
    time_t norm = mktime(&n);
    printf("归一化后：%04d-%02d-%02d %02d:%02d:%02d（time_t=%lld）\n",
           n.tm_year + 1900, n.tm_mon + 1, n.tm_mday,
           n.tm_hour, n.tm_min, n.tm_sec, (long long)norm);
    return 0;
}
```

`实测数据`
`Text`

```text
gmtime   : 2023-11-14 22:13:20 wday=2 yday=317 isdst=0
tm_year 原值 = 123（从 1900 起算）
tm_mon  原值 = 10（从 0 起算）
localtime: 2023-11-15 06:13:20 isdst=0
mktime(gmtime 的结果) = 1699971200，与原值差 -28800 秒
换成小时：本地时间比 UTC 早 +8 小时
asctime 长度 = 25，末字符编码 = 10（换行是 10）
asctime = [Tue Nov 14 22:13:20 2023
]ctime   = [Wed Nov 15 06:13:20 2023
]上面两个方括号里各多出一个空行，因为字符串自带 \n
归一化后：2024-02-02 02:10:00（time_t=1706811000）
```

**这份输出里有四条结论。**

**`mktime` 与 `gmtime` 不是逆运算。** `gmtime` 拆出来的字段是 UTC，
把同样的字段喂给 `mktime`，它会当成**本地时间**来解释，
于是结果差了 28800 秒——正好是本机的时区偏移（UTC+8）。
**要往返一致，必须配对用 `localtime` 与 `mktime`。**

**`mktime` 会把越界的字段归一化，并把结果写回结构体。**
上面给的 `tm_mday = 32`、`tm_hour = 25`、`tm_min = 70` 都没有被拒绝：
1 月 32 日先变成 2 月 1 日，再补 25 小时、70 分钟，最后是 `2024-02-02 02:10:00`，
`tm_wday`、`tm_yday` 这类派生字段也一并填好。
**这个行为是标准规定的**，可以用来做「日期加天数」这类计算。

**`asctime` 与 `ctime` 返回的字符串自带换行。**
长度 25 里最后那个字节是 `\n`（编码 10）。
**直接写 `printf("%s\n", asctime(&tm))` 会多出一个空行。**

**`tm_wday` 是 2，表示星期二**——星期天是 0，所以 2 是周二。
上面 UTC 那一行是 2023-11-14，确实是星期二。

## 2.2 静态缓冲区：三个函数共用一块内存

**`gmtime`、`localtime`、`asctime`、`ctime` 都不返回调用者给出的内存，
而是返回一个指向「库自己的一块静态缓冲区」的指针。**

**后果是：后一次调用会把前一次的结果覆盖掉。**

`实测数据`
`C`

```c
/* tm_trap.c    编译：gcc -std=c23 tm_trap.c -o tm_trap */
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void) {
    /* 陷阱一：两次调用共用一块静态缓冲区 */
    time_t t1 = 0;                 /* 1970-01-01 UTC */
    time_t t2 = 1700000000;        /* 2023-11-14 UTC */
    struct tm *p1 = localtime(&t1);
    struct tm *p2 = localtime(&t2);
    printf("p1 与 p2 是同一个指针吗：%s\n", p1 == p2 ? "是" : "不是");
    printf("先取 1970 再取 2023，回头读 p1 得到的年份 = %d\n", p1->tm_year + 1900);
    /* 正确做法：立刻拷一份 */
    struct tm a = *localtime(&t1);
    struct tm b = *localtime(&t2);
    printf("各自拷一份之后：%d 与 %d\n", a.tm_year + 1900, b.tm_year + 1900);
    /* 陷阱二：tm_year 是从 1900 起算的偏移 */
    struct tm y = {0};
    y.tm_year = 2024;              /* 想写 2024 年，实际含义是 3924 年 */
    y.tm_mon = 0;
    y.tm_mday = 1;
    y.tm_isdst = -1;
    time_t r = mktime(&y);
    printf("直接把 2024 填进 tm_year：mktime 返回 %lld\n", (long long)r);
    printf("正确写法 2024 - 1900 = %d，mktime 返回 %lld\n",
           2024 - 1900, (long long)mktime(&(struct tm){0, 0, 12, 1, 0, 2024 - 1900, 0, 0, -1}));
    /* 陷阱三：asctime 的字符串自带换行 */
    char *s = asctime(&b);
    printf("asctime 的最后一个字符编码 = %d\n", (int)s[strlen(s) - 1]);
    printf("[%s]", s);
    /* 陷阱四：把 time_t 塞进 int */
    time_t big = 2147483647LL + 1;     /* 2038-01-19 03:14:08 UTC 之后一秒 */
    int cut = (int)big;
    time_t back = cut;
    printf("time_t %lld 截成 int 得 %d，再放回 time_t 得到 %lld\n",
           (long long)big, cut, (long long)back);
    struct tm c1 = *localtime(&big);
    printf("原值的年份 = %d\n", c1.tm_year + 1900);
    struct tm *c2 = localtime(&back);
    printf("截断后 localtime 返回 %s\n", c2 ? "一个可用的指针" : "NULL（这个时刻超出了本机表示范围）");
    return 0;
}
```

`实测数据`
`Text`

```text
p1 与 p2 是同一个指针吗：是
先取 1970 再取 2023，回头读 p1 得到的年份 = 2023
各自拷一份之后：1970 与 2023
直接把 2024 填进 tm_year：mktime 返回 -1
正确写法 2024 - 1900 = 124，mktime 返回 1704081600
asctime 的最后一个字符编码 = 10
[Wed Nov 15 06:13:20 2023
]time_t 2147483648 截成 int 得 -2147483648，再放回 time_t 得到 -2147483648
原值的年份 = 2038
截断后 localtime 返回 NULL（这个时刻超出了本机表示范围）
```

**第一行就给出了答案：两个指针相同。**
所以「先取 1970、再取 2023、回头读第一个指针」得到的年份是 **2023**——
读到的已经是第二次调用的结果了。**这是一个不会报警的静默错误。**

**正确写法只有一条：拿到指针立刻把结构体拷出来。**

`C`

```c
/* （下面是节选）两种写法对照 */
struct tm *p = localtime(&t);          /* 错的：p 随时可能被下一次调用改掉 */
printf("%d\n", p->tm_year + 1900);     /* 中间只要再调用一次时间函数，这里就读错了 */
struct tm tm = *localtime(&t);         /* 对的：值拷贝，之后与库无关 */
printf("%d\n", tm.tm_year + 1900);
```

**`asctime` 与 `ctime` 也是同一块缓冲区。**
因此上面那段程序里用了 `strcpy` 把结果先搬进自己的数组——
不搬的话，`s1` 与 `s2` 会指向同一块内存，打印出来是两行一样的字符串。

> [!CAUTION]
> **`localtime` 与 `gmtime` 不是线程安全的**，原因就是这块共享缓冲区：
> 两个线程同时调用，互相覆盖对方的结果。
> C23 为此加了两个带 `_r` 后缀的版本：
>
> `C`
>
> ```c
> /* （下面是节选）C23 的线程安全版本：缓冲区由调用方提供 */
> struct tm buf;
> localtime_r(&t, &buf);     /* 结果写进 buf，不共用静态内存 */
> gmtime_r(&t, &buf);
> ```
>
> **本机的 MinGW 头文件里已经有这两个函数**（C23 与 POSIX 都有它们），
> 但 `localtime` 本身的静态缓冲区行为没有变。

## 2.3 `mktime` 失败与 `localtime` 返回 `NULL`

**`mktime` 失败时返回 `(time_t)-1`，`localtime` 与 `gmtime` 失败时返回 `NULL`。**

`文档`

> "The localtime functions return a pointer to the broken-down time, or a null
> pointer if the specified time cannot be converted to local time."
>
> —— N3220 §7.29.3.4/3

**不检查这个返回值会崩，而且崩得很彻底。** 先看一个把 `tm_year` 填错的版本：

`实测数据`
`C`

```c
/* tm_null.c    编译：gcc -std=c23 tm_null.c -o tm_null */
#include <stdio.h>
#include <time.h>

int main(void) {
    struct tm bad = {0};
    bad.tm_year = 2024;          /* 想写 2024 年，实际含义是 3924 年 */
    bad.tm_mon = 0;
    bad.tm_mday = 1;
    bad.tm_isdst = -1;
    time_t t = mktime(&bad);     /* 3924 年超出范围，返回 -1 */
    struct tm *p = localtime(&t);        /* 没有检查 NULL */
    printf("年份 = %d\n", p->tm_year + 1900);
    return 0;
}
```

`实测数据`
`Text`

```text
运行退出码 3221225477（0xC0000005，访问冲突：对空指针解引用）
没有任何输出
```

**「没有任何输出」这一点值得注意**：程序的第一条 `printf` 在崩溃之后，
缓冲区里的内容随进程一起消失。**崩溃点之前的信息也可能看不到**，
这正是很多「一跑就没了」的程序难以排查的原因。

**`mktime` 失败时会把 `errno` 设成 `EINVAL`（本机是 22）**，
这一点在第 2.2 小节的输出里能看到（`mktime 返回 -1`）。
正确写法是把两处都检查上：

`C`

```c
/* （下面是节选）两处都要检查 */
errno = 0;
time_t t = mktime(&tm);
if (t == (time_t)-1) {
    /* 要么时间超出范围，要么字段本身不合法 */
    return -1;
}
struct tm *p = localtime(&t);
if (!p) {
    /* 这一次是 localtime 拒绝了这个 time_t */
    return -1;
}
```

> [!WARNING]
> **`(time_t)-1` 是一个合法的时间值**（1969-12-31 23:59:59 UTC），
> 因此不能只凭返回值判断失败，还要看 `errno`。
> 本机的 `mktime` 对 1969 年的日期也返回 `-1`，
> 两种情况用同一个返回值表示，属于这套接口设计上的历史包袱。

> [!NOTE]
> **本节回顾**：`gmtime`/`localtime` 拆时间，`mktime` 按**本地时间**还原并归一化越界字段；
> 四个函数都返回静态缓冲区的指针，拿到就要立刻拷贝；
> 失败返回 `-1` 或 `NULL`，不检查就会崩。

---

# 第 3 节 时区与夏令时的现实

## 3.1 本机时区

**时区不在 `time_t` 里，也不在 `struct tm` 的固定字段里。**
`time_t` 是一个绝对时刻，`localtime` 在拆它的时候才把本机时区考虑进去。

**本机的时区是 UTC+8。** 这一点在第 2 节的输出里已经看到：
`mktime(gmtime 的结果)` 与原值差 `-28800` 秒，也就是 8 小时。
**时区名、偏移与夏令时标志由第 3.2 小节的 `tz_bytes.c` 打印出来。**

## 3.2 `TZ` 环境变量：两个平台认的格式不一样

**`TZ` 环境变量可以覆盖本机时区，但 Windows 与 Linux 认的不是同一套写法。**

`实测数据`
`C`

```c
/* tz_bytes.c    编译：gcc -std=c23 tz_bytes.c -o tz_bytes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static void dump(const char *tag, const char *s) {
    printf("%-4s = [%s]  长度 %zu  字节：", tag, s, strlen(s));
    for (size_t i = 0; i < strlen(s); i++)
        printf(" %02X", (unsigned char)s[i]);
    putchar('\n');
}

int main(void) {
    const char *tz = getenv("TZ");
    printf("TZ = %s\n", tz ? tz : "（未设置）");
    tzset();
    time_t t = 1700000000;
    struct tm loc = *localtime(&t);          /* 先拷出来，后面还要用 */
    struct tm utc = *gmtime(&t);             /* gmtime 会覆盖同一块静态缓冲区 */
    printf("当地时间：%04d-%02d-%02d %02d:%02d:%02d  isdst=%d\n",
           loc.tm_year + 1900, loc.tm_mon + 1, loc.tm_mday,
           loc.tm_hour, loc.tm_min, loc.tm_sec, loc.tm_isdst);
    printf("同一时刻的 UTC：%04d-%02d-%02d %02d:%02d:%02d\n",
           utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
           utc.tm_hour, utc.tm_min, utc.tm_sec);
    char buf[128];
    strftime(buf, sizeof buf, "%Z", &loc);
    dump("%Z", buf);
    strftime(buf, sizeof buf, "%z", &loc);
    dump("%z", buf);
    /* 冬夏各取一个时刻，看本地小时与 isdst */
    struct tm w = {0};
    w.tm_year = 124; w.tm_mon = 0; w.tm_mday = 15; w.tm_hour = 12; w.tm_isdst = -1;
    time_t tw = mktime(&w);
    struct tm sw = {0};
    sw.tm_year = 124; sw.tm_mon = 6; sw.tm_mday = 15; sw.tm_hour = 12; sw.tm_isdst = -1;
    time_t ts = mktime(&sw);
    struct tm lw = *localtime(&tw), uw = *gmtime(&tw);
    struct tm ls = *localtime(&ts), us = *gmtime(&ts);
    printf("冬：本地 %02d:00  isdst=%d   （UTC %02d:00，%lld）\n",
           lw.tm_hour, lw.tm_isdst, uw.tm_hour, (long long)tw);
    printf("夏：本地 %02d:00  isdst=%d   （UTC %02d:00，%lld）\n",
           ls.tm_hour, ls.tm_isdst, us.tm_hour, (long long)ts);
    return 0;
}
```

`实测数据`
`Text`

```text
===== 不设 TZ =====
TZ = （未设置）
当地时间：2023-11-15 06:13:20  isdst=0
同一时刻的 UTC：2023-11-14 22:13:20
%Z   = [中国标准时间]  长度 12  字节： D6 D0 B9 FA B1 EA D7 BC CA B1 BC E4
%z   = [中国标准时间]  长度 12  字节： D6 D0 B9 FA B1 EA D7 BC CA B1 BC E4
冬：本地 12:00  isdst=0   （UTC 04:00，1705291200）
夏：本地 12:00  isdst=0   （UTC 04:00，1721016000）

===== TZ=UTC =====
当地时间：2023-11-14 22:13:20  isdst=0
%Z   = [UTC]  长度 3  字节： 55 54 43
%z   = [UTC]  长度 3  字节： 55 54 43
冬：本地 12:00  isdst=0   （UTC 12:00，1705320000）
夏：本地 12:00  isdst=0   （UTC 12:00，1721044800）

===== TZ=EST5EDT =====
当地时间：2023-11-14 17:13:20  isdst=0
%Z   = [EST]  长度 3  字节： 45 53 54
%z   = [EST]  长度 3  字节： 45 53 54
冬：本地 12:00  isdst=0   （UTC 17:00，1705338000）
夏：本地 12:00  isdst=1   （UTC 16:00，1721059200）

===== TZ=JST-9 =====
当地时间：2023-11-15 07:13:20  isdst=0
%Z   = [JST]  长度 3  字节： 4A 53 54
%z   = [JST]  长度 3  字节： 4A 53 54
冬：本地 12:00  isdst=0   （UTC 03:00，1705287600）
夏：本地 12:00  isdst=0   （UTC 03:00，1721012400）

===== TZ=Asia/Tokyo =====
当地时间：2023-11-14 22:13:20  isdst=0
%Z   = [Asi]  长度 3  字节： 41 73 69
%z   = [Asi]  长度 3  字节： 41 73 69
冬：本地 12:00  isdst=0   （UTC 12:00，1705320000）
夏：本地 12:00  isdst=1   （UTC 11:00，1721041200）
```

**这张表里有四个结论，都不符合直觉。**

**第一，`TZ` 在本机是生效的**，而且换一个进程就换一次时区。
`TZ=UTC` 让本地时间等于 UTC，`TZ=EST5EDT` 让本地时间变成 UTC-5（冬季）。

**第二，Windows 只认 POSIX 形式的时区串，不认 IANA 名字。**
`EST5EDT`（标准时名字 + 偏移 + 夏令时名字）被正确解析；
`JST-9` 也对；**而 `Asia/Tokyo` 被当成了一个名字叫 `Asi` 的时区**，
偏移按 0 算，夏季还给出了 `isdst=1`，把时间往前挪了一小时。
**这是一个静默错误：程序不报错，时区就是不对。**

**第三，`%Z` 与 `%z` 在本机是同一个东西。**
两个格式符都返回时区**名字**，`%z` 并没有返回 `+0800` 这样的偏移。
这一条与 Linux 完全不同（下一小节）。

**第四，不设 `TZ` 时 `%Z` 返回 12 个字节的 GBK 编码文本。**
`D6 D0 B9 FA …` 是「中国标准时间」这六个汉字在代码页 936 下的字节，
**不是 UTF-8**。源码是 UTF-8、运行期拿到 GBK，两者混在同一个 `printf` 里就会乱码。
**要做国际化，不要用 `%Z` 的输出当地址或键值。**

## 3.3 Linux 侧：IANA 名字与夏令时

`实测数据`
`Text`

```text
（Linux / glibc，TZ 由环境变量给，同一份程序）
TZ=UTC                1 月：本地 01-15 12:00 UTC +0000（UTC 12:00，isdst=0）   7 月：本地 07-15 12:00 UTC +0000（UTC 12:00，isdst=0）
TZ=Asia/Shanghai      1 月：本地 01-15 12:00 CST +0800（UTC 04:00，isdst=0）   7 月：本地 07-15 12:00 CST +0800（UTC 04:00，isdst=0）
TZ=Asia/Tokyo         1 月：本地 01-15 12:00 JST +0900（UTC 03:00，isdst=0）   7 月：本地 07-15 12:00 JST +0900（UTC 03:00，isdst=0）
TZ=America/New_York   1 月：本地 01-15 12:00 EST -0500（UTC 17:00，isdst=0）   7 月：本地 07-15 12:00 EDT -0400（UTC 16:00，isdst=1）
TZ=Europe/London      1 月：本地 01-15 12:00 GMT +0000（UTC 12:00，isdst=0）   7 月：本地 07-15 12:00 BST +0100（UTC 11:00，isdst=1）
TZ=Australia/Sydney   1 月：本地 01-15 12:00 AEDT +1100（UTC 01:00，isdst=1）   7 月：本地 07-15 12:00 AEST +1000（UTC 02:00，isdst=0）
TZ=（未设置）          1 月：本地 01-15 12:00 CST +0800（UTC 04:00，isdst=0）   7 月：本地 07-15 12:00 CST +0800（UTC 04:00，isdst=0）
```

**三个差别一眼可见。**

**Linux 认 IANA 名字**（`Asia/Tokyo` 拿到了 `JST +0900`），
**`%z` 给的是 `+0800` 这样的数字偏移**，
**夏令时跨半年自动切换**：`America/New_York` 一月是 `EST -0500`、七月是 `EDT -0400`。

**`Australia/Sydney` 这一行值得注意**：它一月是 `+1100`（夏令时），
七月是 `+1000`（标准时）。**南半球的夏令时方向与北半球相反**，
「夏天」这个词在代码里没有任何意义，**只能靠 `tm_isdst` 与库的时区数据判断**。

## 3.4 关于时区的两条实用结论

> [!IMPORTANT]
> **`<time.h>` 的时区能力完全依赖运行环境。**
> 同一份代码，在 Windows 上只能用 POSIX 形式的 `TZ` 串或系统时区设置，
> 在 Linux 上还能用 IANA 名字与完整的时区数据库。
> **指望 `TZ=Asia/Shanghai` 在两个平台上都生效，是不成立的。**

**要在程序里稳妥处理时区，可行做法有三条**：

| 做法 | 说明 |
|---|---|
| 只用 UTC 存与算 | `time()` + `gmtime()`，把时区问题推迟到显示层 |
| 偏移自己存 | 把「本地时间 + 偏移分钟数」一起存下来，不依赖运行环境 |
| 时区计算交给专门的库 | C 标准库没有这个能力，`<chrono>` 的时区部分（C++20）有 |

> [!NOTE]
> **本节回顾**：`time_t` 是绝对时刻，时区只在 `localtime` 拆解时才参与；
> `TZ` 环境变量在两个平台上认的格式不同，Windows 不认 IANA 名字；
> `%z` 在 Windows 上返回的是时区名而不是偏移；夏令时由库的时区数据决定，
> 南北半球方向相反。

---

# 第 4 节 `strftime`：把时间格式化成文本

## 4.1 常用格式符逐个实测

**`strftime` 是把 `struct tm` 变成字符串的唯一标准手段。**

`C`

```c
/* （下面是节选）签名与返回值 */
size_t strftime(char *restrict s, size_t maxsize,
                const char *restrict format, const struct tm *restrict timeptr);
```

**返回写入的字符数（不含结尾的空字符）；放不下时返回 0。**

`实测数据`
`C`

```c
/* strftime_table.c    编译：gcc -std=c23 strftime_table.c -o strftime_table */
#include <stdio.h>
#include <string.h>
#include <time.h>
struct row { const char *fmt; const char *what; };

int main(void) {
    time_t t = 1700000000;              /* UTC 2023-11-14 22:13:20 */
    struct tm tm = *localtime(&t);      /* 本机是 UTC+8，本地 11-15 06:13:20 */
    char buf[128];
    struct row rows[] = {
        {"%Y", "四位年"},        {"%y", "两位年"},       {"%C", "世纪"},
        {"%m", "月 01-12"},      {"%d", "日 01-31"},     {"%e", "日，空格补位"},
        {"%j", "一年中的第几天"}, {"%H", "小时 00-23"},   {"%I", "小时 01-12"},
        {"%M", "分"},            {"%S", "秒"},           {"%p", "AM/PM"},
        {"%A", "星期全名"},      {"%a", "星期缩写"},      {"%B", "月全名"},
        {"%b", "月缩写"},        {"%h", "同 %b"},
        {"%U", "周数，周日为首日"}, {"%W", "周数，周一为首日"}, {"%V", "ISO 周数"},
        {"%w", "星期 0-6"},      {"%u", "星期 1-7"},     {"%G", "ISO 年"},
        {"%Z", "时区名"},        {"%z", "时区偏移"},
        {"%c", "本地日期时间"},   {"%x", "本地日期"},     {"%X", "本地时间"},
        {"%D", "等于 %m/%d/%y"}, {"%F", "等于 %Y-%m-%d"}, {"%T", "等于 %H:%M:%S"},
        {"%R", "等于 %H:%M"},    {"%r", "12 小时制"},     {"%s", "自 epoch 起的秒"},
        {"%%", "百分号本身"},    {"%n", "换行"},         {"%t", "制表符"},
    };
    int n = (int)(sizeof rows / sizeof rows[0]);
    printf("%-6s %-22s %s\n", "格式符", "含义", "本机输出");
    for (int i = 0; i < n; i++) {
        size_t r = strftime(buf, sizeof buf, rows[i].fmt, &tm);
        if (r == 0) printf("%-6s %-22s <返回 0，未写入>\n", rows[i].fmt, rows[i].what);
        else printf("%-6s %-22s [%s]（%zu 字节）\n", rows[i].fmt, rows[i].what, buf, r);
    }
    /* 缓冲区不够时的返回值 */
    char small[5];
    size_t r = strftime(small, sizeof small, "%Y-%m-%d", &tm);
    printf("\n缓冲区只有 5 字节时 strftime(\"%%Y-%%m-%%d\") 返回 %zu，内容 = [%s]\n", r, small);
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
    printf("常用组合：%s\n", buf);
    return 0;
}
```

`实测数据`
`Text`

```text
格式符 含义                 本机输出
%Y     四位年              [2023]（4 字节）
%y     两位年              [23]（2 字节）
%C     世纪                 <返回 0，未写入>
%m     月 01-12              [11]（2 字节）
%d     日 01-31              [15]（2 字节）
%e     日，空格补位     <返回 0，未写入>
%j     一年中的第几天  [319]（3 字节）
%H     小时 00-23           [06]（2 字节）
%I     小时 01-12           [06]（2 字节）
%M     分                    [13]（2 字节）
%S     秒                    [20]（2 字节）
%p     AM/PM                  [AM]（2 字节）
%A     星期全名           [Wednesday]（9 字节）
%a     星期缩写           [Wed]（3 字节）
%B     月全名              [November]（8 字节）
%b     月缩写              [Nov]（3 字节）
%h     同 %b                 <返回 0，未写入>
%U     周数，周日为首日 [46]（2 字节）
%W     周数，周一为首日 [46]（2 字节）
%V     ISO 周数             <返回 0，未写入>
%w     星期 0-6             [3]（1 字节）
%u     星期 1-7             <返回 0，未写入>
%G     ISO 年                <返回 0，未写入>
%Z     时区名              [中国标准时间]（12 字节）
%z     时区偏移           [中国标准时间]（12 字节）
%c     本地日期时间     [11/15/23 06:13:20]（17 字节）
%x     本地日期           [11/15/23]（8 字节）
%X     本地时间           [06:13:20]（8 字节）
%D     等于 %m/%d/%y        <返回 0，未写入>
%F     等于 %Y-%m-%d        <返回 0，未写入>
%T     等于 %H:%M:%S        <返回 0，未写入>
%R     等于 %H:%M           <返回 0，未写入>
%r     12 小时制           <返回 0，未写入>
%s     自 epoch 起的秒    <返回 0，未写入>
%%     百分号本身        [%]（1 字节）
%n     换行                 <返回 0，未写入>
%t     制表符              <返回 0，未写入>

缓冲区只有 5 字节时 strftime("%Y-%m-%d") 返回 0，内容 = []
常用组合：2023-11-15 06:13:20
```

**本机的 `strftime` 只认一部分格式符。** 认得的这些可以直接用；
返回 0 的那十四个，**本机一个都不支持**。

**`%Z` 与 `%z` 那两个方括号里的汉字不是源码里的 UTF-8 文本**，
而是运行库按代码页 936 输出的 12 个 GBK 字节，第 3.2 小节有逐字节的对照。
上面的表格按终端实际显示的形态记录。

**同一份程序在 Linux 上全都认**：

`实测数据`
`Text`

```text
（Linux / glibc，TZ=Asia/Shanghai）
%Y   -> 2023      %C   -> 20        %e   -> 15        %h   -> Nov
%V   -> 46        %u   -> 3         %G   -> 2023      %D   -> 11/15/23
%F   -> 2023-11-15  %T -> 06:13:20  %R   -> 06:13     %r   -> 06:13:20 AM
%s   -> 1700000000  %n -> （换行）  %t   -> （制表符）
%Z   -> CST       %z   -> +0800     %c   -> Wed Nov 15 06:13:20 2023
%x   -> 11/15/23  %X   -> 06:13:20  %j   -> 319       %U   -> 46
%W   -> 46        %w   -> 3         %p   -> AM        %I   -> 06
缓冲区 5 字节：返回 0，内容 = [2023]
```

> [!IMPORTANT]
> **`%F`、`%T`、`%D`、`%R`、`%r`、`%s` 这些「方便的」格式符都不是 C89 的一部分**，
> 它们是后来的标准或 POSIX 加进去的。**本机的 C 运行库只实现了 C89 那一批**，
> 因此这些写法在 Windows 上会静默地返回 0。
>
> **可移植的写法是自己拼**：
>
> `C`
>
> ```c
> /* （下面是节选）不用 %F 与 %T，自己拼出来 */
> strftime(buf, sizeof buf, "%Y-%m-%d", &tm);       /* 代替 %F */
> strftime(buf, sizeof buf, "%H:%M:%S", &tm);       /* 代替 %T */
> ```

## 4.2 缓冲区不够时会发生什么

**返回 0，而且缓冲区里的内容是「不确定的」。**

`文档`

> "Otherwise, zero is returned and the members of the array have an
> indeterminate representation."
>
> —— N3220 §7.29.3.5

**两个平台给出的结果不一样，这恰好印证了「不确定」三个字**：

`实测数据`
`C`

```c
/* strftime_small.c    编译：gcc -std=c23 strftime_small.c -o strftime_small */
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void) {
    struct tm t = {0};
    t.tm_year = 2023 - 1900;
    t.tm_mon  = 10 - 1;
    t.tm_mday = 5;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    mktime(&t);
    /* 先在缓冲区里放一个记号，看失败时它会不会被改写 */
    char small[5];
    strcpy(small, "XXXX");
    size_t n = strftime(small, sizeof small, "%Y-%m-%d", &t);
    printf("缓冲区 5 字节：  返回 %zu，内容是 [%s]\n", n, small);
    /* 空格式串的结果也是 0 个字符，与「放不下」共用返回值 0 */
    char empty[16];
    strcpy(empty, "YYYY");
    n = strftime(empty, sizeof empty, "", &t);
    printf("空格式串：       返回 %zu，内容是 [%s]\n", n, empty);
    /* 缓冲区够大时才是正常路径 */
    char ok[32];
    n = strftime(ok, sizeof ok, "%Y-%m-%d", &t);
    printf("缓冲区 32 字节： 返回 %zu，内容是 [%s]\n", n, ok);
    return 0;
}
```

**程序先把缓冲区填成 `XXXX`，再交给 `strftime`**，这样「失败后里面剩什么」才看得出来：

`实测数据`

| 平台 | 缓冲区 5 字节，格式 `%Y-%m-%d` | 空格式串 | 缓冲区 32 字节 |
|---|---|---|---|
| 本机（MinGW） | 返回 0，内容 `[]` | 返回 0，内容 `[]` | 返回 10，内容 `[2023-10-05]` |
| Linux（glibc） | 返回 0，内容 `[2023]` | 返回 0，内容 `[]` | 返回 10，内容 `[2023-10-05]` |

**glibc 把已经写进去的部分留下了**，本机把它清掉了。
**两边都符合标准**，因为标准说的是「不确定」，两种都算。

> [!CAUTION]
> **判断 `strftime` 是否成功的唯一依据是返回值，不是缓冲区内容。**
> 返回 0 时缓冲区里可能是空的、可能是半截、也可能是上一次的残留，
> **绝对不能拿它去打印或当键值用。**
>
> 另外一个容易忽略的地方：**格式串合法但结果为空时也返回 0**。
> 空格式串 `""` 的结果就是 0 个字符，与「放不下」共用同一个返回值。

> [!NOTE]
> **本节回顾**：`strftime` 的格式符清单依实现而定，本机只支持 C89 那一批；
> 缓冲区不够时返回 0 且内容不确定，必须检查返回值。

---

# 第 5 节 `timespec_get` 与 `struct timespec`

## 5.1 C11 给的墙上时钟

**`time()` 的精度是秒，测一小段代码根本不够用。** C11 补了一个更细的接口：

`文档`

> "The timespec_get function sets the interval pointed to by ts to hold the
> current calendar time based on the specified time base. If base is TIME_UTC,
> the tv_sec member is set to the number of seconds since an implementation-defined
> epoch, truncated to a whole value and the tv_nsec member is set to the integral
> number of nanoseconds, rounded to the resolution of the system clock."
>
> —— N3220 §7.29.2.6/2-3

**用法只有一行**：

`C`

```c
/* （下面是节选）取当前 UTC 时刻 */
struct timespec ts;
timespec_get(&ts, TIME_UTC);
/* ts.tv_sec 是秒，ts.tv_nsec 是纳秒（0 到 999999999） */
```

**`struct timespec` 有两个成员：`tv_sec`（秒）与 `tv_nsec`（纳秒）。**
标准只规定了 `tv_sec` 至少 64 位、`tv_nsec` 至少 32 位，没有规定具体类型。

## 5.2 本机的 MinGW 没有它

**这一段是本机的一个硬限制：`timespec_get` 与 `TIME_UTC` 都用不了。**

`实测数据`
`C`

```c
/* timespec_missing.c    编译：gcc -std=c23 timespec_missing.c -o timespec_missing （失败） */
#include <stdio.h>
#include <time.h>

int main(void) {
    struct timespec ts;
    int r = timespec_get(&ts, TIME_UTC);
    printf("返回 %d，秒 = %lld，纳秒 = %ld\n", r, (long long)ts.tv_sec, ts.tv_nsec);
    return 0;
}
```

`实测数据`
`Text`

```text
timespec_missing.c:7:13: error: implicit declaration of function 'timespec_get'
    [-Wimplicit-function-declaration]
    7 |     int r = timespec_get(&ts, TIME_UTC);
      |             ^~~~~~~~~~~~
timespec_missing.c:7:31: error: 'TIME_UTC' undeclared (first use in this function)
    7 |     int r = timespec_get(&ts, TIME_UTC);
      |                               ^~~~~~~~
```

**原因不在编译器版本，而在运行库目标。** 本机的 MinGW-w64 构建链接的是
老式的 `msvcrt.dll`，而 `timespec_get` 是 UCRT（通用 C 运行库）才有的函数。
头文件里的写法是：

`C`

```c
/* （下面是节选）MinGW 的 <time.h> 里的条件编译 */
#ifdef _UCRT
#define TIME_UTC 1
#endif
/* …后面才是 timespec_get 的声明，同样在 _UCRT 条件下 */
```

**因此在这套工具链上，C11 的这个接口整个不存在。**
把 `_UCRT` 手动定义上也没有用——声明有了，函数还是没有：

`实测数据`
`Text`

```text
$ gcc -std=c23 -D_UCRT timespec_missing.c -o timespec_missing
ld.exe: undefined reference to `_timespec64_get'
collect2.exe: error: ld returned 1 exit status
```

**Linux 侧的 glibc 有它**，同一段代码在那里正常工作：

`实测数据`
`C`

```c
/* Linux 侧的程序，本机 C 编译器没有 <time.h> 的 timespec_get，这一段在 Linux 上编译运行：
   timespec_probe.c    编译（Linux 侧）：gcc -std=gnu2x timespec_probe.c -o timespec_probe */
#include <stdio.h>
#include <time.h>

int main(void) {
    struct timespec ts;
    int r = timespec_get(&ts, TIME_UTC);
    printf("timespec_get 返回 %d（TIME_UTC=%d）\n", r, TIME_UTC);
    printf("tv_sec=%lld tv_nsec=%ld  sizeof=%zu\n",
           (long long)ts.tv_sec, ts.tv_nsec, sizeof(struct timespec));
    /* 连续读，看最小间隔（分辨率） */
    long min = 1000000000L;
    struct timespec a, b;
    timespec_get(&a, TIME_UTC);
    for (int i = 0; i < 200000; i++) {
        timespec_get(&b, TIME_UTC);
        long d = (long)(b.tv_sec - a.tv_sec) * 1000000000L + (b.tv_nsec - a.tv_nsec);
        if (d > 0 && d < min) min = d;
        a = b;
    }
    printf("20 万次连续读里最小的正间隔 = %ld 纳秒\n", min);
    return 0;
}
```

`实测数据`
`Text`

```text
timespec_get 返回 1（TIME_UTC=1）
tv_sec=1790824874 tv_nsec=957111604  sizeof=16
20 万次连续读里最小的正间隔 = 12 纳秒
```

## 5.3 本机可用的替代：POSIX 的 `clock_gettime`

**MinGW-w64 另带了一套 POSIX 时钟**，声明在 `<pthread_time.h>` 里
（严格模式下包含 `<time.h>` 也会把它带进来）：

`C`

```c
/* （下面是节选）POSIX 的写法，选项顺序不能反 */
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);   /* 单调钟：只往前走的墙上时间 */
clock_gettime(CLOCK_REALTIME, &ts);    /* 实时钟：可以被系统对时改掉 */
```

**它需要一个额外的链接选项，而且必须写在源文件后面**：

`实测数据`
`Bash`

```bash
# 库在源码之前：链接器扫过库时还没有未解决的符号，等于白写
$ gcc -std=gnu23 -lwinpthread timespec_probe.c -o probe
ld.exe: undefined reference to `clock_gettime64'
collect2.exe: error: ld returned 1 exit status

# 库在源码之后：能链接
$ gcc -std=gnu23 timespec_probe.c -o probe -lwinpthread
（编译链接都通过）
```

`实测数据`
`C`

```c
/* 本机上没有 timespec_get，这一段用 POSIX 的 clock_gettime 顶上：
   monotonic_probe.c    编译：gcc -std=gnu23 monotonic_probe.c -o monotonic_probe -lwinpthread */
#include <stdio.h>
#include <time.h>
#include <pthread_time.h>

int main(void) {
    struct timespec m0, b;
    if (clock_gettime(CLOCK_MONOTONIC, &m0) != 0) {
        puts("clock_gettime 不可用");
        return 1;
    }
    printf("sizeof(struct timespec) = %zu\n", sizeof(struct timespec));
    printf("sizeof(tv_sec)=%zu sizeof(tv_nsec)=%zu\n",
           sizeof(m0.tv_sec), sizeof(m0.tv_nsec));
    long min = 1000000000L;
    struct timespec a = m0;
    for (int i = 0; i < 200000; i++) {
        clock_gettime(CLOCK_MONOTONIC, &b);
        long d = (long)(b.tv_sec - a.tv_sec) * 1000000000L + (b.tv_nsec - a.tv_nsec);
        if (d > 0 && d < min) min = d;
        a = b;
    }
    printf("CLOCK_MONOTONIC 在 20 万次连续读里最小的正间隔 = %ld 纳秒\n", min);
    printf("CLOCK_MONOTONIC 的 tv_sec（系统启动以来的秒数） = %lld\n", (long long)m0.tv_sec);
    return 0;
}
```

`实测数据`
`Text`

```text
sizeof(struct timespec) = 16
sizeof(tv_sec)=8 sizeof(tv_nsec)=4
CLOCK_MONOTONIC 在 20 万次连续读里最小的正间隔 = 100 纳秒
CLOCK_MONOTONIC 的 tv_sec（系统启动以来的秒数） = 95686
```

**最后一行是当时那台机器的开机时长（约 26 小时半）**，
它每次运行都不一样，其余三行每次都一样。

**把两个平台的分辨率放在一起看**：

`实测数据`

| 时钟 | 平台 | 20 万次连续读的最小正间隔 |
|---|---|---|
| `timespec_get(TIME_UTC)` | Linux / glibc 2.39 | 12 纳秒 |
| `clock_gettime(CLOCK_MONOTONIC)` | Windows / MinGW-w64 15.2.0 | 100 纳秒 |

**12 与 100 的差别来自底层计数器的频率**，不是库的好坏。
**测耗时之前先知道自己手里的钟有多细**——用 100 纳秒的钟去测一段 200 纳秒的代码，
结果没有意义。

> [!TIP]
> **`CLOCK_MONOTONIC` 与 `TIME_UTC` 不是一回事。**
> `TIME_UTC` 取的是系统实时钟，**系统对时会让它跳**（本节末尾有实测）；
> `CLOCK_MONOTONIC` 只往前走，适合测耗时。
> C23 给 `timespec_get` 补了一个 `TIME_MONOTONIC`，但本机还用不上。

> [!NOTE]
> **本节回顾**：`timespec_get` 是 C11 的纳秒级接口，本机的 MinGW（msvcrt 目标）没有它；
> Linux 侧可用，分辨率 12 纳秒；本机可用 POSIX 的 `clock_gettime` 替代，
> 需要 `-lwinpthread` 且必须写在源文件之后。

---

# 第 6 节 为什么测耗时不该用 `clock()`

## 6.1 标准说的与两个实现做的

**标准说 `clock()` 量的是「处理器时间」**（本节开头引过原文），
也就是 CPU 真正花在这段代码上的时间，**睡眠、等待 I/O 的时间不算**。

**本机的 MinGW 没有照这个说法做。** 它链接的 `msvcrt.dll` 里，
`clock()` 返回的是**从进程启动到现在的墙上时间**，睡眠照算。
Linux 的 glibc 则是标准的处理器时间。

**于是同一个 `clock()`，在两个平台上量的是两样东西。**

## 6.2 含睡眠与不含睡眠的差别

**同一段代码里：先睡 500 毫秒（不占 CPU），再跑一段纯计算（占 CPU）。**
另用一个墙上时钟量同一段代码。

`实测数据`
`C`

```c
/* clock_vs_wall.c    编译：gcc -std=c23 clock_vs_wall.c -o clock_vs_wall */
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int ms) { Sleep((DWORD)ms); }
static double wall_now(void) {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}
#else
#include <unistd.h>
static void sleep_ms(int ms) {
    struct timespec req = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&req, NULL);
}
static double wall_now(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);            /* Linux 侧才有 timespec_get */
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
#endif

int main(void) {
    clock_t c0 = clock();
    double w0 = wall_now();
    sleep_ms(500);                          /* 睡 500 毫秒，不占 CPU */
    volatile double x = 0;
    for (int i = 0; i < 60000000; i++) x += i * 0.5;   /* 纯计算，占 CPU */
    clock_t c1 = clock();
    double w1 = wall_now();
    double cpu = (double)(c1 - c0) / CLOCKS_PER_SEC;
    printf("这段代码：睡 500 ms，再跑一段纯计算\n");
    printf("clock()  量到 = %7.3f 秒\n", cpu);
    printf("墙上时钟 量到 = %7.3f 秒\n", w1 - w0);
    printf("两者相差      = %7.3f 秒\n", (w1 - w0) - cpu);
    printf("CLOCKS_PER_SEC = %ld\n", (long)CLOCKS_PER_SEC);
    printf("x = %g\n", x);
    return 0;
}
```

`实测数据`
`Text`

```text
（Windows / MinGW-w64 15.2.0，连跑两次）
这段代码：睡 500 ms，再跑一段纯计算
clock()  量到 =   0.598 秒
墙上时钟 量到 =   0.598 秒
两者相差      =  -0.000 秒
CLOCKS_PER_SEC = 1000
x = 9e+14

第二次：
clock()  量到 =   0.606 秒
墙上时钟 量到 =   0.606 秒
两者相差      =   0.000 秒
```

**两个数一模一样，说明本机的 `clock()` 把 500 毫秒睡眠全算进去了。**

`实测数据`
`Text`

```text
（Linux / glibc 2.39，同一份程序，连跑三次）
CLOCKS_PER_SEC = 1000000
clock()       = 0.093 秒（CPU）      timespec_get = 17.256 秒（墙上）   相差 17.163 秒
clock()       = 0.094 秒（CPU）      timespec_get = -16.111 秒（墙上）  相差 -16.205 秒
clock()       = 0.097 秒（CPU）      timespec_get =  0.597 秒（墙上）   相差  0.500 秒
```

**Linux 的 `clock()` 是 0.09 秒出头，睡眠那 500 毫秒完全没有算进去**，
两者相差正好 0.500 秒。

> [!IMPORTANT]
> **`clock()` 的语义依平台而异，不能拿来写「这段代码跑了多久」。**
> 本机（MinGW + `msvcrt.dll`）量的是墙上时间，Linux（glibc）量的是处理器时间。
> 同一份代码在两边的结果可能差出一个数量级，
> 而**在 Windows 上它看起来「是对的」，因此这个错误很难被发现**。

**上面 Linux 那三次里还有一处值得注意**：第 1 次墙上时钟给出 `17.256` 秒，
第 2 次给出 `-16.111` 秒——**墙上时间倒退了**。
原因在第 5 节提过：`timespec_get(TIME_UTC)` 读的是系统实时钟，
而虚拟机里的系统时间会被对时机制调整。
**这三行数据本身就是「实时钟不能用来测耗时」的证据。**

## 6.3 该用什么

| 目的 | 用什么 | 为什么 |
|---|---|---|
| 量一段时间过了多久（含睡眠、I/O） | **单调钟**：`CLOCK_MONOTONIC` 或 C++ 的 `steady_clock` | 只往前走，不受对时影响 |
| 量 CPU 真正花了多少 | `clock()`，且只在 POSIX 上 | 本机的 `clock()` 不是 CPU 时间 |
| 要一个日历时间戳 | `time()` 或 `timespec_get(TIME_UTC)` | 这是它们的本职 |
| 分辨率要求高于 1 微秒 | 平台的高精度计数器 | 见 `07-更底层` 板块 |

> [!TIP]
> **在 Windows 上想要一个可靠的单调钟**，用
> `QueryPerformanceCounter` / `QueryPerformanceFrequency`（上面那个程序就是这么做的），
> 或者用 C++ 的 `std::chrono::steady_clock`。
> 后者的做法见《06-标准库/B-06-时间：chrono.md》第 2 节。

> [!NOTE]
> **本节回顾**：`clock()` 在标准里是处理器时间，在本机的 MinGW 上却是墙上时间；
> 测耗时要用单调钟；`timespec_get(TIME_UTC)` 是实时钟，会被对时改动。

---

# 第 7 节 速查表

`实测数据`

| 常用件 | 一句话用途 | 典型坑 |
|---|---|---|
| `time_t` | 从纪元的秒数 | 纪元是哪一刻由实现定；本机是 8 字节有符号 |
| `time` | 取当前时刻 | 只到秒 |
| `difftime` | 两个 `time_t` 相减 | 直接写 `t1 - t2` 不可移植 |
| `clock_t` / `CLOCKS_PER_SEC` | 处理器时间 | 本机是 1000，Linux 是 1000000；**本机给的却是墙上时间** |
| `struct tm` | 拆开的日历 | `tm_mon` 从 0 起算，`tm_year` 从 1900 起算 |
| `gmtime` / `localtime` | 拆成 UTC / 本地时间 | **共用一块静态缓冲区**，必须立刻拷贝；失败返回 `NULL` |
| `mktime` | 按本地时间还原 | 与 `gmtime` 不是逆运算；会归一化越界字段；失败返回 `-1` |
| `asctime` / `ctime` | 直接给字符串 | 自带换行；同样是静态缓冲区 |
| `tzset` / `TZ` | 换时区 | Windows 只认 POSIX 形式，不认 `Asia/Tokyo` |
| `strftime` | 格式化 | 本机只支持 C89 那一批；放不下时返回 0 |
| `%Z` / `%z` | 时区名 / 偏移 | 本机两个都给名字；`%Z` 给的是 GBK 字节 |
| `timespec_get` | 纳秒级墙上时间 | **本机的 MinGW 没有它**；`TIME_UTC` 不是单调钟 |
| `clock_gettime` | POSIX 时钟 | 要 `-lwinpthread`，且写在源文件之后 |

**配套示例与练习**：本章节的综合示例见
[`B-examples/06-standard-library/01-c-stdlib-toolbox/`](../B-examples/06-standard-library/01-c-stdlib-toolbox/)，
练习见 [`C-templates/06-standard-library/01-c-stdlib-toolbox/`](../C-templates/06-standard-library/01-c-stdlib-toolbox/)。
示例里用本章节的接口给整条流水线计时，并打印一份带时间戳的报表。

---

# 附录 A 复现本章节实测

## A.1 环境

| 项 | 值 |
|---|---|
| Windows 侧 | MinGW-w64，GCC 15.2.0，目标 `x86_64-win32-seh-rev0`（链接 `msvcrt.dll`），C 标准 `-std=c23` |
| Linux 侧 | WSL Ubuntu 24.04.5，glibc 2.39，GCC 13.3.0，C 标准 `-std=c2x` |
| 本机时区 | UTC+8，不实行夏令时 |

**GCC 13.3 不认识 `-std=c23` 这个名字**，Linux 侧的命令一律写成 `-std=c2x`。
`-std=c23` 是 GCC 14 才加进去的。

## A.2 各程序的编译与运行

`Bash`

```bash
gcc -std=c23 time_epoch.c        -o time_epoch        && ./time_epoch
gcc -std=c23 tm_fields.c         -o tm_fields         && ./tm_fields
gcc -std=c23 time_range.c        -o time_range        && ./time_range
gcc -std=c23 time_basic.c        -o time_basic        && ./time_basic
gcc -std=c23 tm_split.c          -o tm_split          && ./tm_split
gcc -std=c23 tm_trap.c           -o tm_trap           && ./tm_trap
gcc -std=c23 tm_null.c           -o tm_null           ; ./tm_null      # 会崩，退出码 3221225477
gcc -std=c23 tz_bytes.c          -o tz_bytes
gcc -std=c23 strftime_table.c    -o strftime_table    && ./strftime_table
gcc -std=c23 strftime_small.c    -o strftime_small    && ./strftime_small
gcc -std=c23 clock_vs_wall.c     -o clock_vs_wall     && ./clock_vs_wall
```

**时区那一个要在运行前设置环境变量**：

`Bash`

```bash
TZ=UTC ./tz_bytes
TZ=EST5EDT ./tz_bytes
TZ=JST-9 ./tz_bytes
TZ=Asia/Tokyo ./tz_bytes
```

**期望失败的两个**（报错原文见正文）：

`Bash`

```bash
gcc -std=c23 timespec_missing.c -o timespec_missing
gcc -std=c23 -D_UCRT timespec_missing.c -o timespec_missing     # 声明有了，链接还是失败
```

**只在 Linux 侧编译的一个**：

`Bash`

```bash
gcc -std=gnu2x timespec_probe.c -o timespec_probe && ./timespec_probe
```

**本机的 POSIX 时钟要额外加链接选项，且写在源文件之后**：

`Bash`

```bash
gcc -std=gnu23 monotonic_probe.c -o monotonic_probe -lwinpthread
```

**时钟分辨率的实测用连续读两万次、取最小正间隔的办法**，
三个程序（`timespec_probe`、`monotonic_probe`、`clock_vs_wall`）里的循环都写死了次数，
因此结果可复现；间隔本身会随机器负载略有波动。

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《04-语法/02-数据类型与类型系统.md》第 2.1 小节 | **前置**：整数类型与宽度（`time_t` 是 8 字节有符号） |
| 《04-语法/09-结构体、联合体与 enum.md》第 1 节 | **前置**：结构体与成员访问 |
| 《04-语法/08-数组、指针与引用.md》第 1.3 小节 | **前置**：空指针与解引用 |
| 《01-编译器/01-编译与链接.md》第 2.5 小节 | **前置**：库的顺序为什么不能反 |
| 《06-标准库/A-03-数值、数学与随机.md》第 4 节 | **相关**：同样是「实现说了算」的接口 |
| 《06-标准库/A-05-工具与其它：stdlib 与杂项.md》第 7 节 | **相关**：`mktime` 失败时设置的 `errno` |
| 《06-标准库/B-06-时间：chrono.md》第 2 节 | **后续**：`time_point` 与三种 clock；第 3 节 测一段代码要多久、第 4 节 等待与定时 |
