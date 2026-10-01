# 工具与其它：`<stdlib.h>` 与杂项

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**A 段讲到这里，剩下的是几个「零散但常用」的头文件。**

`<stdlib.h>` 里除了上一章用掉的数学与随机数，还有两样大件：
**内存分配**（`malloc` 一族）与**程序控制**（`exit`、`atexit`、`abort`、`system`）。
`<stdlib.h>` 之外还有一串小头文件：`<assert.h>`、`<ctype.h>`、`<errno.h>`、
`<setjmp.h>`、`<signal.h>`、`<stddef.h>`、`<stdbool.h>`、`<stdalign.h>`、`<stdarg.h>`。
它们各自只管一件事，但**没有一件是可以绕开的**。

**本章节的东西有一个共同点：出错时要么完全静默，要么直接把进程结束掉。**
`realloc` 用错写法只丢内存、不报错；`qsort` 的比较函数写错只给出错误的顺序；
`ctype` 的参数类型错了可能什么都不发生；而 `abort`、`assert` 失败、
`longjmp` 跳错地方，都是一次性地结束进程。
**中间那一档——编译能过、运行不崩、结果不对——占了多数**，
因此本章的「写错了会怎样」都来自实际运行，而不是推演。

**两个平台在这里的差别也不小**：`assert` 失败的报错格式与退出码、`_Exit` 在刷新
缓冲区上的表现、`signal` 的处理函数会不会被复位、`ctype` 传负值的后果——
以下每一条都在 Windows 与 Linux 两侧实测过。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 复现环境与命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 内容 | 在哪一节 |
|---|---|
| **`malloc`、`calloc`、`realloc`、`free`**，`realloc` 的正确写法 | **一** |
| **`qsort` 与 `bsearch`**，比较函数的返回值约定 | **二** |
| **`exit`、`atexit`、`abort`、`_Exit`** 与缓冲刷新 | **三** |
| **`getenv` 与 `system`** | **四** |
| **`<assert.h>`**：`NDEBUG` 前后的差别 | **五** |
| **`<ctype.h>`**：参数为什么必须是 `unsigned char` 或 `EOF` | **六** |
| **`<errno.h>`**：取值、`strerror`、线程安全 | **七** |
| **`<setjmp.h>`**：`setjmp`/`longjmp`，以及 C++ 里为什么不该用 | **八** |
| **`<signal.h>`**：处理函数会被复位这件事 | **九** |
| **`<stddef.h>`、`<stdbool.h>`、`<stdalign.h>`** | **十** |
| **`<stdarg.h>`**：可变参数 | **十一** |
| 速查表与配套件 | 十二 |

| 需要先知道 | 在哪 |
|---|---|
| 指针、数组退化、回调函数指针 | 《04-语法/08-数组、指针与引用.md》第 4.5 小节 |
| 变量的生存期 | 《04-语法/11-作用域、生存期与链接.md》第 3.1 小节 |
| 显式类型转换 | 《04-语法/02-数据类型与类型系统.md》第 5.5 小节 |
| 条件编译与 `NDEBUG` | 《04-语法/14-预处理器.md》第 4 节 |
| `RAII`：把释放写进析构函数 | 《05-类与面向对象/06-RAII 与资源管理.md》第 2 节 |
| 可变参数函数 | 《04-语法/07-函数.md》第 6 节 |

**相邻的章节**：上一章《06-标准库/A-03-数值、数学与随机.md》用掉了 `<stdlib.h>` 的随机数，
本章节接着用它剩下的部分；输入输出与 `errno` 的配合见
《06-标准库/A-01-输入输出：stdio.md》，动态内存的 C++ 对应物见
《06-标准库/B-10-内存与并发的基础设施.md》第 1 节与第 2 节。

---

# 第 1 节 `<stdlib.h>` 的内存分配

## 1.1 四个函数

**C 的动态内存只有四个函数，没有别的。**

| 函数 | 干什么 |
|---|---|
| `malloc(n)` | 要 `n` 个字节，内容**不清零** |
| `calloc(k, n)` | 要 `k` 个元素、每个 `n` 字节，**内容清零** |
| `realloc(p, n)` | 把 `p` 指向的块改成 `n` 字节，**可能搬家** |
| `free(p)` | 还回去 |

**它们只管「要一块内存」和「还一块内存」**——把 `void *` 转成具体类型、
记住块有多大、什么时候还，全是写代码的人的责任，这正是 C++ 用 RAII
要解决的问题（《05-类与面向对象/06-RAII 与资源管理.md》第 1 节）。

## 1.2 四个函数的行为实测

`实测数据`
`C`

```c
/* alloc_basic.c    编译：gcc -std=c23 alloc_basic.c -o alloc_basic */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(void) {
    /* malloc：按字节给一块未初始化的内存 */
    int *p = malloc(10 * sizeof(int));
    if (!p) { perror("malloc"); return 1; }
    printf("malloc 返回的地址 %% 16 = %llu（对齐到 16 字节）\n",
           (unsigned long long)((uintptr_t)p % 16));
    printf("malloc 出来的内存没被清零：p[0]=%d p[9]=%d\n", p[0], p[9]);
    /* calloc：给一块并清零，还会检查乘法溢出 */
    int *c = calloc(10, sizeof(int));
    printf("calloc 出来的内存是零：c[0]=%d c[9]=%d\n", c[0], c[9]);
    /* free(NULL) 是合法的，什么也不做 */
    free(NULL);
    printf("free(NULL) 执行完毕，没有报错\n");
    /* malloc(0)：返回值要么是 NULL，要么是不能解引用的唯一指针 */
    void *z = malloc(0);
    printf("malloc(0) 返回 %s\n", z ? "非 NULL（不能解引用）" : "NULL");
    free(z);
    /* realloc：扩容保留原内容 */
    for (int i = 0; i < 10; i++) p[i] = i * i;
    int *q = realloc(p, 20 * sizeof(int));
    if (q) p = q;
    printf("realloc 扩容后前 10 个还在：%d %d ... %d\n", p[0], p[1], p[9]);
    free(p);
    free(c);
    return 0;
}
```

`实测数据`
`Text`

```text
malloc 返回的地址 % 16 = 0（对齐到 16 字节）
malloc 出来的内存没被清零：p[0]=1059287200 p[9]=846357826
calloc 出来的内存是零：c[0]=0 c[9]=0
free(NULL) 执行完毕，没有报错
malloc(0) 返回 非 NULL（不能解引用）
realloc 扩容后前 10 个还在：0 1 ... 81
```

**`malloc` 的内存不清零，`calloc` 清。** 上面 `p[0]`、`p[9]` 那些数是这块内存上一次被使用时留下的
残留数据，**每次运行的数都不一样**，不能依赖。

**`free(NULL)` 是合法的**，标准写得很清楚：

`文档`

> "If ptr is a null pointer, no action occurs."
>
> —— N3220 §7.24.3.3/2

因此 `free(p); p = NULL;` 之后再 `free(p)` 不会出问题；**但它不能让
「双重释放一个非空指针」变合法**——那仍然是未定义行为。

**`malloc(0)` 返回一个非空指针**，指向的块不能解引用。标准允许返回 `NULL` 或一个
唯一指针，两种都对，**因此不能拿它的返回值当判断依据**。

**`malloc` 的对齐在本机是 16 字节**，刚好够放 `long double` 与 `max_align_t`，
因此拿 `malloc` 出来的内存放任何基本类型都不会错位（第 10 节有对照）。

## 1.3 `realloc` 用错写法会静默丢内存

**`realloc` 是四个函数里最容易写错的一个。** 它要处理三件事：
扩、缩、以及**搬家**（原来的位置放不下时，会在别处找一块并拷贝过去）。

**最常见的错误写法是「把返回值直接写回原指针」**：

`实测数据`
`C`

```c
/* realloc_bug.c    编译：gcc -std=c23 realloc_bug.c -o realloc_bug */
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    char *p = malloc(1024);
    if (!p) return 1;
    for (int i = 0; i < 1023; i++) p[i] = 'a';
    p[1023] = '\0';
    /* 错的写法：直接把返回值写回 p，失败时原地址丢失 */
    p = realloc(p, (size_t)-1 / 2);
    printf("realloc 失败后 p = %s\n", p ? "非 NULL" : "NULL");
    if (!p) {
        printf("原来那 1024 字节已经无法释放（地址丢了），这就是泄漏\n");
        free(p);        /* free(NULL) 合法，但已经晚了 */
    }
    return 0;
}
```

`实测数据`
`Text`

```text
realloc 失败后 p = NULL
原来那 1024 字节已经无法释放（地址丢了），这就是泄漏
```

**为什么这是个错误？** 标准对失败的情形写得明白：

`文档`

> "If memory for the new object is not allocated, the old object is not
> deallocated and its value is unchanged."
>
> —— N3220 §7.24.3.7/3

**旧的那块内存仍然有效，但程序已经失去了指向它的地址**——`realloc` 返回的
`NULL` 把原来的指针覆盖掉了。**程序不会崩、不报错，只是每走一遍就少一块内存。**

**正确写法只有一条：先把返回值存下来，判断之后再决定要不要覆盖。**

`实测数据`
`C`

```c
/* realloc_ok.c    编译：gcc -std=c23 realloc_ok.c -o realloc_ok */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* 把「扩容」封成函数，调用处拿到的永远是有效指针或 NULL */
static char *grow(char *old, size_t want) {
    char *fresh = realloc(old, want);
    if (!fresh) return old;          /* 失败：原块不动，仍然有效 */
    return fresh;
}

int main(void) {
    char *buf = malloc(16);
    if (!buf) return 1;
    strcpy(buf, "hello");
    char *p = grow(buf, 1024);
    printf("扩容成功：%s，缓冲区大小 %zu\n", p, (size_t)1024);
    char *q = grow(p, (size_t)-1 / 2);
    printf("扩容失败时返回的还是老指针：%s（地址没变）\n", q);
    free(q);
    return 0;
}
```

`实测数据`
`Text`

```text
扩容成功：hello，缓冲区大小 1024
扩容失败时返回的还是老指针：hello（地址没变）
```

> [!CAUTION]
> **`realloc(p, 0)`、`realloc(NULL, n)` 与「对同一个指针连续 realloc」都有额外的规定。**
> 标准说 `realloc(NULL, n)` 等价于 `malloc(n)`（有用），
> 而 `realloc(p, 0)` 的行为是**未定义**——不要用它来代替 `free`。
> 另外，`realloc` 之后**原来那个指针可能已经失效**（搬家了），
> 从这一刻起只能用新指针。

> [!TIP]
> **GCC 对「realloc 之后再用旧指针」会告警**（`-Wuse-after-free`）。它在失败分支
> 里会误报——按标准失败时旧指针仍然有效——**但这个写法值得重新审视**：
> 把扩容逻辑封进一个函数（像上面那样）既能消掉告警，也让调用处更难写错。

## 1.4 与 RAII 的对照

**C 里没有析构函数，因此「谁负责释放」只能靠约定与纪律。**
C++ 的写法是把这块内存装进一个对象，由析构函数负责还回去：

`C++`

```cpp
/* （下面是节选）C++ 的对应写法 */
#include <memory>
#include <cstring>
{
    auto buf = std::make_unique<char[]>(1024);   // 不用管什么时候还
    std::strcpy(buf.get(), "hello");
    // 扩容改用 std::vector 或 std::string，它们自己会处理失败
}   // 离开作用域，自动释放
```

**智能指针与 RAII 的完整讲法见《05-类与面向对象/06-RAII 与资源管理.md》
第 4 节与第 5 节**：**`malloc`/`free` 的配对在 C++ 里应当由对象来保证。**

> [!NOTE]
> **本节回顾**：`malloc` 不清零、`calloc` 清零、`free(NULL)` 合法、`malloc(0)` 的返回值不能依赖；
> `realloc` 失败时旧块仍然有效，**必须先把返回值存下来再决定是否覆盖**。

---

# 第 2 节 `qsort` 与 `bsearch`

## 2.1 两个函数都靠回调

**`qsort` 与 `bsearch` 是 C 标准库里唯一一对「通用算法」**——
它们不关心元素是什么类型，只关心「一块连续内存、每个元素多大、怎么比大小」。
C++ 里对应的是 `std::sort` 与 `std::lower_bound`（【待补：08-高阶数据结构/】）。

`C`

```c
/* （下面是节选）两个函数共用同一个比较函数类型 */
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));
```

同一个 `compar` 既能排序也能查找，**回调本身的写法在**
《04-语法/08-数组、指针与引用.md》第 4.5 小节讲过，这里的重点是**返回值约定**：

`文档`

> "The comparison function shall return an integer less than, equal to, or
> greater than zero if the first argument is considered to be respectively
> less than, equal to, or greater than the second."
>
> —— N3220 §7.24.5.2/2

**「小于、等于、大于」是三种符号，不是三个具体数值。**
返回 `-1`、`0`、`1` 可以，返回 `a - b` 也可以——**但 `a - b` 会溢出**（第 2.3 小节）。

**参数是 `const void *`，必须显式转回真实类型**：写成 `const int *` 接收是惯用
做法，**不要写成 `int *`**——库里传进来的是 `const` 指针，丢掉 `const` 会带来
告警，也不符合实际语义。

## 2.2 排序与查找实测

`实测数据`
`C`

```c
/* qsort_demo.c    编译：gcc -std=c23 qsort_demo.c -o qsort_demo */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { char name[12]; int score; } Student;
/* 比较函数的返回值约定：小于返回负、相等返回 0、大于返回正 */
static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);          /* 不写成 x - y，避免溢出 */
}
static int cmp_student(const void *a, const void *b) {
    const Student *x = a, *y = b;
    if (x->score != y->score) return (x->score > y->score) - (x->score < y->score);
    return strcmp(x->name, y->name);   /* 分数相同按名字 */
}
static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

int main(void) {
    int nums[] = {42, 7, 19, 3, 88, 7, 1};
    size_t n = sizeof nums / sizeof nums[0];
    qsort(nums, n, sizeof nums[0], cmp_int);
    printf("整数排序：");
    for (size_t i = 0; i < n; i++) printf(" %d", nums[i]);
    printf("\n");
    /* bsearch：要求数组已经排好序，找不到返回 NULL */
    int key = 19;
    int *hit = bsearch(&key, nums, n, sizeof nums[0], cmp_int);
    printf("bsearch 找 19：%s\n", hit ? "找到" : "没找到");
    key = 20;
    hit = bsearch(&key, nums, n, sizeof nums[0], cmp_int);
    printf("bsearch 找 20：%s\n", hit ? "找到" : "没找到");
    Student st[] = {{"Chen", 90}, {"Li", 85}, {"Wang", 90}, {"Zhao", 72}};
    size_t m = sizeof st / sizeof st[0];
    qsort(st, m, sizeof st[0], cmp_student);
    printf("结构体排序：");
    for (size_t i = 0; i < m; i++) printf(" %s=%d", st[i].name, st[i].score);
    printf("\n");
    const char *words[] = {"pear", "apple", "fig", "banana"};
    size_t k = sizeof words / sizeof words[0];
    qsort(words, k, sizeof words[0], cmp_str);
    printf("字符串排序：");
    for (size_t i = 0; i < k; i++) printf(" %s", words[i]);
    printf("\n");
    Student sk = {"Li", 85};
    Student *f = bsearch(&sk, st, m, sizeof st[0], cmp_student);
    printf("bsearch 找 Li/85：%s\n", f ? f->name : "没找到");
    return 0;
}
```

`实测数据`
`Text`

```text
整数排序： 1 3 7 7 19 42 88
bsearch 找 19：找到
bsearch 找 20：没找到
结构体排序： Zhao=72 Li=85 Chen=90 Wang=90
字符串排序： apple banana fig pear
bsearch 找 Li/85：Li
```

**字符串数组的比较函数收到的是「指向 `char *` 的指针」**，
因此要写成 `*(const char *const *)a`——**两层指针**，这是最容易写错的一处。

**`cmp_student` 先比分数、相同再比名字**：比较函数要对所有字段都能分出胜负，
否则「分数相同的两个元素谁在前」就没有保证——`qsort` 不是稳定排序。

**`bsearch` 要求数组已经排好序，且必须用同一个比较函数**：换一个「等价但不同」
的比较函数，结果没有定义，二分查找依赖的正是那个顺序。

## 2.3 写错了会怎样：比较函数返回 `a - b`

**`return *(const int *)a - *(const int *)b;` 是这个函数的经典写法，
也是经典的溢出点。** 两个 `int` 相减的结果可能超出 `int` 的范围。

`实测数据`
`C`

```c
/* qsort_overflow.c    编译：gcc -std=c23 qsort_overflow.c -o qsort_overflow */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* 错法的比较函数：x - y 在极值上溢出 */
static int cmp_bad(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}
/* 对法：只比较大小，不做减法 */
static int cmp_good(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}
static void show(const char *tag, int *v, size_t n) {
    printf("%s：", tag);
    for (size_t i = 0; i < n; i++) printf(" %d", v[i]);
    printf("\n");
}
static int sorted(const int *v, size_t n) {
    for (size_t i = 1; i < n; i++) if (v[i - 1] > v[i]) return 0;
    return 1;
}

int main(void) {
    int a[] = {INT_MAX, 0, INT_MIN, 5, -3, INT_MAX - 1, INT_MIN + 1, 100};
    int b[sizeof a / sizeof a[0]];
    size_t n = sizeof a / sizeof a[0];
    memcpy(b, a, sizeof a);
    qsort(a, n, sizeof a[0], cmp_bad);
    show("用 x - y 排序  ", a, n);
    qsort(b, n, sizeof b[0], cmp_good);
    show("用只比大小排序", b, n);
    printf("第一份结果%s，第二份结果%s\n",
           sorted(a, n) ? "有序" : "失序", sorted(b, n) ? "有序" : "失序");
    return 0;
}
```

`实测数据`
`Text`

```text
用 x - y 排序  ： 0 5 2147483646 2147483647 -2147483648 -2147483647 -3 100
用只比大小排序： -2147483648 -2147483647 -3 0 5 100 2147483646 2147483647
第一份结果失序，第二份结果有序
```

**第一份结果里负数全部跑到了后面**，数组根本没有排好。
原因可以算出来：`INT_MIN - INT_MAX` 的结果超出了 `int` 的范围，
在补码机器上回绕成一个**正数**，于是「最小值」被判定为「大于最大值」。
**排序算法一旦拿到自相矛盾的比较结果，输出就没有任何保证。**

> [!CAUTION]
> **比较函数返回 `a - b` 在两种情况下会出错**：一是**值域跨度超过 `int`**（上面这种），
> 二是**元素是无符号类型或比 `int` 宽**（相减的结果是无符号的，转 `int` 会截断）。
> **一律写成 `(x > y) - (x < y)`**，它对所有整型都成立，同样能被编译器优化掉。

> [!NOTE]
> **本节回顾**：`qsort` 与 `bsearch` 共用同一个比较函数；
> 返回值只需符号正确；**不要写 `a - b`**；
> `qsort` 不是稳定排序；`bsearch` 必须在已排序的数组上用同一个比较函数。

---

# 第 3 节 程序控制：`exit`、`atexit`、`abort`

## 3.1 三种结束方式

| 方式 | 做了什么 |
|---|---|
| 从 `main` 返回 | 等价于 `exit(返回值)` |
| `exit(n)` | **先跑 `atexit` 注册的函数**，再刷新并关闭所有流，最后把 `n` 交给系统 |
| `_Exit(n)` | **什么都不做**，直接结束 |
| `abort()` | 触发 `SIGABRT`，异常结束，**不跑 `atexit`** |

**`atexit` 注册的处理函数按「注册顺序的逆序」执行**，这一条标准写得很明确：

`文档`

> "First, all functions registered by the atexit function are called, in the
> reverse order of their registration"
>
> —— N3220 §7.24.4.4/3

**为什么是逆序？** 它模拟的是一叠清理工作：后来注册的往往依赖先注册的资源，
先清理后来者才不会碰到已经被销毁的东西。

## 3.2 `atexit` 的执行顺序实测

`实测数据`
`C`

```c
/* atexit_order.c    编译：gcc -std=c23 atexit_order.c -o atexit_order */
#include <stdio.h>
#include <stdlib.h>
static void f1(void) { printf("第 1 个注册的处理函数\n"); }
static void f2(void) { printf("第 2 个注册的处理函数\n"); }
static void f3(void) { printf("第 3 个注册的处理函数\n"); }

int main(void) {
    atexit(f1);
    atexit(f2);
    atexit(f3);
    printf("main 里最后一行输出\n");
    printf("接下来退出，看处理函数按什么顺序跑\n");
    return 0;          /* 与 exit(0) 等价：先跑 atexit，再刷新缓冲 */
}
```

`实测数据`
`Text`

```text
main 里最后一行输出
接下来退出，看处理函数按什么顺序跑
第 3 个注册的处理函数
第 2 个注册的处理函数
第 1 个注册的处理函数
```

**先注册的后执行**，3、2、1。Linux 上的结果相同。

## 3.3 `abort` 会跳过 `atexit`

`实测数据`
`C`

```c
/* abort_demo.c    编译：gcc -std=c23 abort_demo.c -o abort_demo */
#include <stdio.h>
#include <stdlib.h>
static void bye(void) { printf("atexit 的处理函数被执行了\n"); }

int main(void) {
    atexit(bye);
    printf("第一行：已经 fflush，abort 之后还能看到\n");
    fflush(stdout);
    printf("第二行：没 fflush，abort 之后看不到，atexit 也不会跑\n");
    abort();
    printf("这一行不会执行\n");
    return 0;
}
```

`实测数据`
`Text`

```text
第一行：已经 fflush，abort 之后还能看到
第二行：没 fflush，abort 之后看不到，atexit 也不会跑
运行退出码 3
```

**第二行仍然打印出来了。** 这不是因为 `abort` 刷新了缓冲区，
而是因为下面 3.4 小节要讲的那件事：**本机的 C 运行库在进程卸载时会把缓冲区刷出去**。
**真正的差别在「`atexit` 的处理函数没有执行」**——输出里没有它。

## 3.4 退出路径与缓冲区刷新的两平台对照

**「`_Exit` 不刷新缓冲区」这条结论，在 Windows 上测不出来。**

`实测数据`
`C`

```c
/* exit_flush.c    编译：gcc -std=c23 exit_flush.c -o exit_flush */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    char mode = (argc > 1) ? argv[1][0] : 'r';
    /* 写 20 个字节，不带换行，显式刷出去 */
    for (int i = 0; i < 20; i++) putchar('A' + (i % 26));
    fflush(stdout);
    /* 再写 10 个字节，不刷新 */
    for (int i = 0; i < 10; i++) putchar('0' + (i % 10));
    if (mode == 'e') { printf("\n[走 exit]\n"); exit(0); }
    if (mode == '_') { _exit(0); }      /* 本机的 MinGW 不提供 C99 的 _Exit */
    if (mode == 'a') { abort(); }
    printf("\n[从 main 返回]\n");
    return 0;
}
```

**四种退出路径，两次运行的字节数**：

`实测数据`

| 退出方式 | Windows（MinGW 15.2.0） | Linux（glibc 2.39） | 退出码（Linux） |
|---|---|---|---|
| 从 `main` 返回 | 43 字节 | 42 字节 | 0 |
| `exit(0)` | 40 字节 | 39 字节 | 0 |
| `_exit(0)` | 30 字节 | **20 字节** | 0 |
| `abort()` | 30 字节 | **20 字节** | 134 |

**Linux 那一列才是教科书上的样子**：`_exit` 与 `abort` 只留下 20 字节，也就是显式
`fflush` 过的那部分，后面 10 个字节没有换行也没有刷新，**随进程一起丢了**。
**Windows 那一列全是 30 字节**：`msvcrt.dll` 会在进程卸载（DLL 分离）时把自己的流
刷一遍，**于是「不刷新缓冲区」这件事在本机的进程结束路径上看不到**。

> [!IMPORTANT]
> **`exit` 与 `_Exit` 的区别不只是「刷不刷缓冲」，还有「跑不跑 `atexit`」。**
> 后者两个平台都能测出来：`_exit` 与 `abort` 之后 `atexit` 的处理函数一次都没执行。
> **因此要「立刻结束、不做任何清理」时，真正可靠的是 `atexit` 这一半。**

**还有一处平台差异**：C99 的 `_Exit` 在本机**链接不上**：

`实测数据`
`C`

```c
/* exit_c99.c    编译：gcc -std=c23 exit_c99.c -o exit_c99 （失败） */
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("这一行不刷新，_Exit 直接结束进程\n");
    _Exit(0);
    return 0;
}
```

`实测数据`
`Text`

```text
ld.exe: undefined reference to `__imp__Exit'
ld.exe: undefined reference to `__imp_quick_exit'
collect2.exe: error: ld returned 1 exit status
```

（链接器那两行带了一长串工具链安装路径，这里略去前缀，只保留报错本身。）

**原因与上一章的 `timespec_get` 相同**：这两个函数也是 UCRT 才有的，
本机链接的是老式的 `msvcrt.dll`。**退回用 `_exit`（下划线一个）即可**，
它在两个平台上都有。

> [!NOTE]
> **本节回顾**：`atexit` 逆序执行，`abort` 与 `_Exit` 都会跳过它；
> 「`_Exit` 不刷新缓冲」这条在 Linux 上成立、在本机看不到；
> 本机的 `_Exit` 与 `quick_exit` 链接不上，用 `_exit` 代替。

---

# 第 4 节 环境与命令：`getenv` 与 `system`

## 4.1 `getenv`

`getenv` 按名字取一个环境变量，**没有这个变量时返回 `NULL`**：

`实测数据`
`C`

```c
/* env_system.c    编译：gcc -std=c23 env_system.c -o env_system */
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *path = getenv("PATH");
    printf("PATH 前 40 字符：%.40s...\n", path ? path : "（没有）");
    printf("getenv(\"NO_SUCH_VAR_XYZ\") = %s\n",
           getenv("NO_SUCH_VAR_XYZ") ? "非空" : "NULL");
    /* system(NULL)：返回非 0 表示有命令处理器可用 */
    printf("system(NULL) = %d（非 0 表示有命令处理器）\n", system(NULL));
    int rc = system("exit 3");
    printf("system(\"exit 3\") 返回 %d\n", rc);
    return 0;
}
```

`实测数据`
`Text`

```text
PATH 前 40 字符：C:\Program Files\PowerShell\7;c:\Users\H...
getenv("NO_SUCH_VAR_XYZ") = NULL
system(NULL) = 1（非 0 表示有命令处理器）
system("exit 3") 返回 3
```

**`getenv` 返回的指针不由调用者拥有**：不要修改它指向的内容，也不要在 `putenv`/`setenv`
之后继续使用它——**下一次修改环境变量可能让这块内存失效**。需要留存时应 `strdup`
一份（`strdup` 是 POSIX 的，不是 C 标准的一部分）。

**`system(NULL)` 是一个特例**：标准规定它不执行命令，只回答「有没有命令处理器」，
**返回值非 0 表示有**。

**`system` 的返回值不是子进程的退出码本身**，标准说它是「实现定义的」：本机与
Linux 上 `system("exit 3")` 都返回 3，但底层换算不同（POSIX 上要自己用
`WEXITSTATUS` 拆）。**要精确知道子进程怎么结束的，不要依赖它。**

## 4.2 `system` 的安全问题

> [!CAUTION]
> **`system` 把字符串交给命令解释器执行，这是注入漏洞最常见的来源之一。**
> 命令串里**拼接了外部输入**（文件名、用户名、配置值）时，输入里的
> `;`、`&`、`|`、`` ` ``、`$()` 会被当成命令分隔符执行；**`PATH` 被改动过**时，
> `system("ls")` 可能执行到另一个程序；在**服务进程、提权进程**里调用它，
> 等于把执行任意命令的能力交出去。
>
> **要执行外部程序，用不带解释器的接口**：POSIX 上是 `fork` + `exec`
> （参数以数组传递，不经过 shell），Windows 上是 `CreateProcess`。
> **`system` 只适合写一次性的小工具，不适合进产品代码。**

`system` 还有一条更基本的限制：**它必须等子进程结束才返回**，中间没有回调、
没有超时、拿不到输出。**要做交互式的子进程控制，标准库给不了。**

> [!NOTE]
> **本节回顾**：`getenv` 的返回值不能改也不能长期持有；
> `system(NULL)` 用来探测命令处理器；`system` 的返回值是实现定义的；
> **拼接外部输入进 `system` 就是注入漏洞**。

---

# 第 5 节 `<assert.h>`

## 5.1 断言是给「不该发生的事」用的

`assert(表达式)` 做一件事：**表达式为假就打印一条消息并让程序异常结束。**

**它不是错误处理**：错误处理要恢复、要给用户提示、要写日志；
断言用的是「这里出了逻辑错误，继续跑下去没有意义」这个前提。
**因此断言检查的是「程序员写错了」，而不是「用户输入错了」。**

## 5.2 `NDEBUG` 前后的差别

**`<assert.h>` 被包含时，如果 `NDEBUG` 已经定义，`assert` 就变成一个空语句。**

`文档`

> "If NDEBUG is defined as a macro name at the point in the source file where
> <assert.h> is included, the assert macro is defined simply as
> #define assert(...) ((void)0)"
>
> —— N3220 §7.2.1/1

**关键在「at the point ... where <assert.h> is included」这几个字**：
`NDEBUG` 必须在包含头文件**之前**定义，因此它通常来自编译命令行（`-DNDEBUG`）
或文件最开头的 `#define`。

**「一个空语句」意味着参数被完全丢掉——包括里面的副作用。**

`实测数据`
`C`

```c
/* assert_demo.c    编译：gcc -std=c23 assert_demo.c -o assert_demo */
#include <assert.h>
#include <stdio.h>

int main(void) {
    int i = 0;
    /* 断言里带副作用：NDEBUG 一开，这个 ++ 就不存在了 */
    assert(++i < 5);
    printf("断言之后 i = %d\n", i);
    int n = 3;
    assert(n > 0 && "n 必须是正数");
    printf("n = %d\n", n);
    assert(n == 3);
    printf("全部断言通过\n");
#ifdef NDEBUG
    printf("本次编译定义了 NDEBUG：assert 已被完全去掉\n");
#else
    printf("本次编译没定义 NDEBUG：assert 有效\n");
#endif
    return 0;
}
```

`实测数据`
`Text`

```text
（不带 -DNDEBUG）
断言之后 i = 1
n = 3
全部断言通过
本次编译没定义 NDEBUG：assert 有效

（带 -DNDEBUG）
断言之后 i = 0
n = 3
全部断言通过
本次编译定义了 NDEBUG：assert 已被完全去掉
```

**`i` 从 1 变成了 0**：同一个源文件、同一处断言，**开了 `NDEBUG` 之后 `++i`
根本没有执行**。

> [!CAUTION]
> **永远不要在 `assert` 里写有副作用的表达式。**
> `assert(++i < n)`、`assert(fclose(f) == 0)`、`assert(p = next())` 都是错的。
> 这类代码在调试版里正常、在发布版里行为改变，
> 而**发布版正是最不容易发现问题的那一版**。
>
> 同理，`assert` 里也不要调用会修改状态的函数——
> 断言应当只读。

**`assert(n > 0 && "n 必须是正数")` 这个写法值得学**：字符串字面量的值是一个
非空指针，恒为真，因此不影响判断，只在断言失败时把这句话一起打进消息里。

## 5.3 断言失败的报错与退出码

`实测数据`
`C`

```c
/* assert_fail.c    编译：gcc -std=c23 assert_fail.c -o assert_fail */
#include <assert.h>
#include <stdio.h>

int main(void) {
    printf("断言之前这一行会输出\n");
    fflush(stdout);
    int n = 3;
    assert(n > 10);
    printf("断言失败后这一行不会执行\n");
    return 0;
}
```

`实测数据`
`Text`

```text
（Windows / MinGW-w64 15.2.0）
断言之前这一行会输出
Assertion failed: n > 10, file …\assert_fail.c, line 9
运行退出码 3

（Linux / glibc 2.39）
断言之前这一行会输出
assert_fail: assert_fail.c:9: main: Assertion `n > 10' failed.
运行退出码 134
```

**两边的消息格式不同，退出码也不同**（3 对 134）。**因此不要写依赖断言消息
文本或退出码的测试脚本**：要么自己写检查并返回约定的退出码，要么两种都接受。

## 5.4 什么时候该用断言

| 场合 | 用不用 |
|---|---|
| 检查函数入口参数的「不可能值」 | 用 |
| 检查内部不变量（循环里的状态） | 用 |
| 检查用户输入是否合法 | **不用**，那是错误处理 |
| 检查文件是否存在、内存是否分配成功 | **不用**，那些是运行期可预期的情况 |
| 有副作用的表达式 | **绝对不用** |

> [!NOTE]
> **本节回顾**：`assert` 检查的是程序员的逻辑错误；
> `NDEBUG` 一开它整个消失，**包括参数里的副作用**；
> 两个平台的失败消息与退出码都不同。

---

# 第 6 节 `<ctype.h>`

## 6.1 分类与转换

`<ctype.h>` 提供两组函数：**分类**（`isalpha`、`isdigit`、`isspace`、`ispunct`……）
与**转换**（`toupper`、`tolower`），名字都以 `is` 或 `to` 开头，参数与返回值都是 `int`。

**参数是 `int`，不是 `char`。** 这一点是这一节的全部重点。

**这些函数还受 locale 影响**：在 "C" locale 下 `isalpha` 只认 A-Z 与 a-z，
换到别的 locale，中文与德语的变音字母都可能被判为字母。
**要写不受 locale 影响的判断，应当自行比较字符范围。**

## 6.2 参数必须是 `unsigned char` 的取值范围或 `EOF`

**标准对参数的要求非常明确，而且给了「否则就是未定义行为」的判决**：

`文档`

> "In all cases the argument is an int, the value of which shall be representable
> as an unsigned char or shall equal the value of the macro EOF. If the argument
> has any other value, the behavior is undefined."
>
> —— N3220 §7.4.1/1

**为什么会有这么一条奇怪的规定？** 这些函数是用查表实现的，表的下标就是参数值：
`unsigned char` 的取值范围是 `[0, 255]`，`EOF` 是 `-1`，刚好覆盖表的有效下标。

**而 `char` 在多数平台上是有符号的。** 于是一个字节 `0xE9`（`é` 的 UTF-8 首字节之外、
GBK 里的一个汉字字节都可能是它）**存进 `char` 就变成了负数**，
再传进 `isalpha` 就落到了表的有效范围之外。

## 6.3 传负值：本机什么都没发生

`实测数据`
`C`

```c
/* ctype_demo.c    编译：gcc -std=c23 ctype_demo.c -o ctype_demo */
#include <stdio.h>
#include <ctype.h>
#include <limits.h>

int main(void) {
    printf("EOF = %d, UCHAR_MAX = %d\n", EOF, UCHAR_MAX);
    char s[] = "Hello, World 123";
    int letters = 0, digits = 0, spaces = 0;
    for (int i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];     /* 先转成 unsigned char */
        if (isalpha(c)) letters++;
        else if (isdigit(c)) digits++;
        else if (isspace(c)) spaces++;
    }
    printf("字母 %d 个，数字 %d 个，空白 %d 个\n", letters, digits, spaces);
    /* 大小写转换：返回的是 int，值在 unsigned char 范围内 */
    printf("toupper('a') = %c (%d), tolower('Z') = %c (%d)\n",
           toupper('a'), toupper('a'), tolower('Z'), tolower('Z'));
    printf("toupper('1') = %c，原样返回\n", toupper('1'));
    /* 传负值的后果：本机是什么表现 */
    char bad = (char)0xE9;                 /* 一个非 ASCII 字节，作为 char 是负数 */
    printf("(char)0xE9 作为 int 是 %d\n", (int)bad);
    printf("isalpha((char)0xE9)                = %d\n", isalpha(bad));
    printf("isalpha((unsigned char)0xE9)       = %d\n", isalpha((unsigned char)bad));
    printf("isdigit((char)0xE9)                = %d\n", isdigit(bad));
    printf("isprint((char)0xE9)                = %d\n", isprint(bad));
    printf("isspace(-1) 即 EOF 传入           = %d\n", isspace(-1));
    return 0;
}
```

`实测数据`
`Text`

```text
EOF = -1, UCHAR_MAX = 255
字母 10 个，数字 3 个，空白 2 个
toupper('a') = A (65), tolower('Z') = z (122)
toupper('1') = 1，原样返回
(char)0xE9 作为 int 是 -23
isalpha((char)0xE9)                = 0
isalpha((unsigned char)0xE9)       = 0
isdigit((char)0xE9)                = 0
isprint((char)0xE9)                = 0
isspace(-1) 即 EOF 传入           = 0
```

**本机什么都没发生**：传 `-23` 进去，几个函数都规规矩矩返回 0。
Linux 侧的 glibc 也一样（实测返回 0，进程正常退出）。

**这正是未定义行为最麻烦的形态。** 标准说「行为未定义」，
本机的两套实现恰好都做了范围检查、都返回 0，
**于是错误写法跑得通、测试也过、代码评审也看不出问题**。
换一个实现——尤其是嵌入式平台上的精简 C 库，
或者开了边界检查的调试版运行库——就可能读到表外的内存，
或者直接在一次断言上停下来。

> [!IMPORTANT]
> **规矩很简单，养成习惯即可**：
> 传进去之前先转成 `unsigned char`。
>
> `C`
>
> ```c
> /* （下面是节选）两种写法 */
> char c = s[i];
> if (isalpha(c)) { }                  /* 错的：char 为负时是未定义行为 */
> if (isalpha((unsigned char)c)) { }   /* 对的：先转成无符号 */
> ```
>
> **判断 `EOF` 时例外**：`isspace(-1)` 单独调用合法，因为它等于 `EOF`；
> **但同一个表达式不能既可能是字符又可能是 `EOF`**，从 `getchar()` 拿到的 `int`
> 才是正确用法（见《06-标准库/A-01-输入输出：stdio.md》第 3 节）。

> [!NOTE]
> **本节回顾**：`<ctype.h>` 的函数参数是 `int`，
> 只能是 `unsigned char` 能表示的值或 `EOF`；
> 传负值的后果是未定义行为，**本机实测没有可见症状，因此更容易留下隐患**。

---

# 第 7 节 `<errno.h>`

## 7.1 `errno` 是一个宏，不是变量

**`errno` 看起来像一个全局变量，实际是一个宏**：在本机展开后是一次函数调用
（`_errno()`），在 glibc 上是线程局部存储的访问。**这决定了它是「每线程一份」的**，
一个线程里设置 `errno` 不会影响另一个线程（C11 明确要求，在此之前是实现质量问题）。

**另一条容易忽略的规则：成功的调用不负责把 `errno` 清零**，
**必须先置 0、再调用、再看。**

## 7.2 取值与 `strerror`

`实测数据`
`C`

```c
/* errno_demo.c    编译：gcc -std=c23 errno_demo.c -o errno_demo */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

int main(void) {
    printf("errno 起始值 = %d\n", errno);
    printf("常见取值：EDOM=%d ERANGE=%d EINVAL=%d ENOMEM=%d ENOENT=%d\n",
           EDOM, ERANGE, EINVAL, ENOMEM, ENOENT);
    errno = 0;
    strtol("abc", NULL, 10);
    printf("strtol(\"abc\") 之后 errno = %d (%s)\n", errno, strerror(errno));
    errno = 0;
    long v = strtol("99999999999999999999", NULL, 10);
    printf("strtol 溢出：返回 %ld，errno = %d (%s)\n", v, errno, strerror(errno));
    errno = 0;
    double d = sqrt(-1.0);
    printf("sqrt(-1.0)：返回 %g，errno = %d (%s)\n", d, errno, strerror(errno));
    /* errno 是「每线程一份」的：它是一个宏，展开后是一次函数调用 */
#ifdef errno
    printf("errno 是宏，地址 = %p（每线程不同）\n", (void *)&errno);
#else
    printf("errno 不是宏\n");
#endif
    /* 成功的调用不会清零 errno，必须自己先置 0 */
    errno = 0;
    strtol("123", NULL, 10);
    printf("成功调用后 errno 仍是 %d\n", errno);
    errno = ERANGE;
    strtol("456", NULL, 10);
    printf("上一次留下 ERANGE，成功调用后仍是 %d（所以要先置 0）\n", errno);
    return 0;
}
```

`实测数据`
`Text`

```text
errno 起始值 = 0
常见取值：EDOM=33 ERANGE=34 EINVAL=22 ENOMEM=12 ENOENT=2
strtol("abc") 之后 errno = 0 (No error)
strtol 溢出：返回 2147483647，errno = 34 (Result too large)
sqrt(-1.0)：返回 nan，errno = 33 (Domain error)
errno 是宏，地址 = 00000154bad10870（每线程不同）
成功调用后 errno 仍是 0
上一次留下 ERANGE，成功调用后仍是 34（所以要先置 0）
```

**`strtol("abc")` 不设置 `errno`**：它返回 0，「转换失败」要靠第二个参数（`endptr`）
判断，**不能靠 `errno`**（见《06-标准库/A-02-字符串与内存：string.h.md》第 5 节）。

**溢出时 `errno` 被设成 `ERANGE`，返回值是 `LONG_MAX`**，本机的 `long` 是 4 字节，
因此这个值是 `2147483647`。

**`strerror(errno)` 给的是英文文本**（"Result too large"）**且随当前 locale 变**：
打印给用户看可以，**不能当稳定的键值或写进协议**。

**最后两行是这一节的实用结论**：成功的调用不动 `errno`，
上一次留下的 `ERANGE` 会一直挂在那里。

> [!CAUTION]
> **`errno` 的取值是实现定义的**，标准只保证 `EDOM`、`ERANGE`、`EILSEQ`
> 这几个名字存在，具体数字由实现定。
> 上面那组数字（33、34、22）在本机成立，**换一个平台可能完全不同**。
> **代码里永远写宏名，不要写数字。**

> [!NOTE]
> **本节回顾**：`errno` 是每线程一份的宏；调用前先清 0；
> 成功的调用不清 `errno`；取值是实现定义的；`strerror` 的文本是本地化的。

---

# 第 8 节 `<setjmp.h>`

## 8.1 一次「跳回过去」

**`setjmp` 与 `longjmp` 是 C 里唯一的非局部跳转。**
`setjmp` 把当前的执行环境（寄存器、栈指针）存进一个 `jmp_buf`，
`longjmp` 用它把控制权送回去——**中间那几层函数调用被整个丢掉，不会返回。**

`C`

```c
/* （下面是节选）基本形状 */
#include <setjmp.h>
jmp_buf env;
int r = setjmp(env);      /* 第一次到这里返回 0 */
if (r == 0) {
    /* 正常路径 */
    may_longjmp();        /* 里面某处调用 longjmp(env, 42) */
} else {
    /* 从 longjmp 回来，r 是 longjmp 的第二个参数（42） */
}
```

**三条硬性限制**：

| 限制 | 说明 |
|---|---|
| `setjmp` 只能出现在有限的几种上下文里 | 赋值、比较、作为独立语句等；**不能**写成 `x = setjmp(env) + 1` |
| `longjmp` 必须在 `setjmp` 所在函数还活着的时候调用 | 那个函数已经返回了，行为就是未定义 |
| 跨越的栈帧上的局部变量，值不确定 | 下面就有实测 |

**`setjmp` 是宏，而且编译器要对它做特殊处理**（保存寄存器的动作必须在调用点
完成），因此**写成 `std::setjmp` 在 C++ 里也编不过**（第 8.3 小节有实测）。

## 8.2 局部变量的值不确定

`文档`

> "... the representation of objects of automatic storage duration that are
> local to the function containing the invocation of the corresponding setjmp
> macro that do not have volatile-qualified type and have been changed between
> the setjmp invocation and longjmp call is indeterminate."
>
> —— N3220 §7.13.2.1/3

**翻译成一句话**：**在 `setjmp` 与 `longjmp` 之间被改过、
又不是 `volatile` 的局部变量，值是不确定的。**

`实测数据`
`C`

```c
/* setjmp_c.c    编译：gcc -std=c23 -O2 setjmp_c.c -o setjmp_c */
#include <stdio.h>
#include <setjmp.h>
static jmp_buf env;
static void inner(void) {
    printf("进入 inner()\n");
    longjmp(env, 42);                 /* 直接跳回 setjmp 处，middle 还没返回就被丢掉 */
}
static void middle(void) {
    printf("进入 middle()\n");
    inner();
    printf("这一行不会执行\n");
}

int main(void) {
    int x = 1;                        /* 没加 volatile */
    volatile int y = 1;               /* 加了 volatile */
    int r = setjmp(env);
    if (r == 0) {
        printf("setjmp 第一次返回 0，开始往下走\n");
        x = 2;
        y = 2;
        middle();
        printf("这一行不会执行\n");
    } else {
        printf("longjmp 带回来的值 = %d\n", r);
        printf("x = %d（未加 volatile，值不确定）   y = %d（加了 volatile）\n", x, y);
    }
    return 0;
}
```

`实测数据`
`Text`

```text
（-O0，不加优化）
setjmp 第一次返回 0，开始往下走
进入 middle()
进入 inner()
longjmp 带回来的值 = 42
x = 2（未加 volatile，值不确定）   y = 2（加了 volatile）

（-O2）
setjmp 第一次返回 0，开始往下走
进入 middle()
进入 inner()
longjmp 带回来的值 = 42
x = 1（未加 volatile，值不确定）   y = 2（加了 volatile）
```

**同一个程序、同一份数据，`x` 在 `-O0` 下是 2，在 `-O2` 下是 1。**
原因不复杂：`-O2` 把 `x` 放进了寄存器，而 `longjmp` 恢复的是 `setjmp` 那一刻的寄存器状态，
`x = 2` 那次赋值随着寄存器一起被丢了。`volatile` 强制它待在内存里，因此 `y` 一直是 2。

**「中间那两层被丢掉了」这一点值得注意**：输出里没有 `middle()` 剩余的部分，
也没有 `inner()` 的返回——**它们不是返回，是被丢弃了**。

## 8.3 为什么 C++ 里不该用它

**C++ 有异常，而且异常会沿着栈展开、逐个调用析构函数。**
`longjmp` 不会——**它直接把栈指针挪回去，析构函数一次都不会执行。**

**一个 C++ 程序就可以把这件事测出来**：

`实测数据`
`C++`

```cpp
/* setjmp_cpp.cpp    编译：g++ -std=c++17 setjmp_cpp.cpp -o setjmp_cpp */
#include <csetjmp>
#include <cstdio>
struct Tracker {
    const char *name;
    explicit Tracker(const char *n) : name(n) { std::printf("构造 %s\n", name); }
    ~Tracker() { std::printf("析构 %s\n", name); }
};
static std::jmp_buf env;
static void deep() {
    Tracker t("deep 里的对象");
    std::printf("deep 里准备跳走\n");
    std::longjmp(env, 1);
}

int main() {
    Tracker m("main 里的对象");
    if (setjmp(env) == 0) {          /* setjmp 在 C++ 里仍是宏，不写成 std::setjmp */
        deep();
    } else {
        std::printf("从 longjmp 回到了 main\n");
    }
    std::printf("main 即将结束\n");
    return 0;
}
```

`实测数据`
`Text`

```text
构造 main 里的对象
构造 deep 里的对象
deep 里准备跳走
从 longjmp 回到了 main
main 即将结束
析构 main 里的对象
```

**「构造 deep 里的对象」有，对应的「析构」没有。** 如果那个对象持有文件句柄、
`malloc` 出来的内存或一把锁，**这些资源就永远留在那里了**——这就是 RAII 被绕过
的直接后果（《05-类与面向对象/06-RAII 与资源管理.md》第 2.2 小节讲的
「编译器保证的三件事」，在 `longjmp` 这条路径上一件都不成立）。

**还有一个编译错误**：`setjmp` 在 C++ 里也是宏，
写成 `std::setjmp` 会得到：

`实测数据`
`Text`

```text
csetjmp:47:  In function 'int main()':
error: '_setjmp' is not a member of 'std'; did you mean '_setjmp'?
   21 |     if (std::setjmp(env) == 0) {
      |              ^~~~~~
note: '_setjmp' declared here
```

**`<csetjmp>` 把 `jmp_buf` 与 `longjmp` 放进了 `std`，唯独 `setjmp` 还是那个宏。**

> [!CAUTION]
> **只要跨越的栈帧上有任何带析构函数的对象，`longjmp` 就是未定义行为。**
> 标准把这一条写得很直接：`longjmp` 不能用于「会跳过非平凡析构」的场合。
>
> **C++ 里要用「跳出去」这个能力，就用异常**：异常会展开栈、调用析构、
> 按类型匹配 `catch`，而 `longjmp` 只挪动栈指针。
>
> **`setjmp` 在 C 里仍然有用**：实现「一次解析失败就整段放弃」这类逻辑，
> 在 C 里模拟异常（例如某些虚拟机与解释器）。前提是**跨过去的帧上没有需要
> 清理的资源，或者资源是手动管的**。

> [!NOTE]
> **本节回顾**：`setjmp` 保存环境、`longjmp` 跳回去，中间的栈帧被整段丢掉；
> 跨过的非 `volatile` 局部变量值不确定（`-O0` 与 `-O2` 实测不同）；
> **C++ 里它会跳过析构，因此不该用**。

---

# 第 9 节 `<signal.h>`

## 9.1 `signal` 与 `raise`

`<signal.h>` 提供一个极简的信号机制：

| 名字 | 作用 |
|---|---|
| `signal(sig, handler)` | 给某个信号装处理函数，返回旧的处置方式 |
| `raise(sig)` | 给自己发一个信号 |
| `SIG_DFL` / `SIG_IGN` | 两个特殊值：默认处置 / 忽略 |
| `SIG_ERR` | 安装失败时的返回值 |
| `sig_atomic_t` | 处理函数里唯一能安全读写的整数类型 |

**处理函数能做的事很少**：标准规定它只能修改 `volatile sig_atomic_t` 对象、
调用 `signal`、`abort`、`_Exit` 以及少数几个 async-signal-safe 的函数。
**`printf`、`malloc`、`free` 都不在其中**——它们可能在信号到来时正持有内部锁，
再进去一次就是死锁或堆损坏。

## 9.2 处理函数会被复位

**这是 `signal` 最容易出错的地方：处理函数安装一次只生效一次。**

`实测数据`
`C`

```c
/* signal_demo.c    编译：gcc -std=c23 signal_demo.c -o signal_demo */
#include <signal.h>
#include <stdio.h>
static volatile sig_atomic_t hits = 0;
static void on_int(int sig) {
    hits++;                       /* 处理函数里只碰 sig_atomic_t 这类对象 */
    (void)sig;
}

int main(void) {
    printf("SIGINT=%d SIGSEGV=%d SIGTERM=%d SIGABRT=%d SIGFPE=%d\n",
           SIGINT, SIGSEGV, SIGTERM, SIGABRT, SIGFPE);
    printf("SIG_DFL=%p SIG_IGN=%p SIG_ERR=%p\n",
           (void *)SIG_DFL, (void *)SIG_IGN, (void *)SIG_ERR);
    fflush(stdout);
    signal(SIGINT, on_int);
    printf("已装上 SIGINT 的处理函数，下面 raise 两次\n");
    fflush(stdout);
    raise(SIGINT);
    printf("第一次 raise 之后 hits = %d\n", (int)hits);
    fflush(stdout);
    raise(SIGINT);
    printf("第二次 raise 之后 hits = %d\n", (int)hits);
    fflush(stdout);
    signal(SIGINT, SIG_IGN);
    raise(SIGINT);
    printf("改成忽略后再发一次，hits = %d\n", (int)hits);
    return 0;
}
```

`实测数据`
`Text`

```text
（Windows / MinGW-w64 15.2.0）
SIGINT=2 SIGSEGV=11 SIGTERM=15 SIGABRT=22 SIGFPE=8
SIG_DFL=0000000000000000 SIG_IGN=0000000000000001 SIG_ERR=ffffffffffffffff
已装上 SIGINT 的处理函数，下面 raise 两次
第一次 raise 之后 hits = 1
运行退出码 3

（Linux / glibc 2.39）
第一次 raise 后 hits = 1
第二次 raise 后 hits = 2
第三次 raise 后 hits = 3
退出码 0
```

**Windows 那一侧只跑到第一次 `raise` 就结束了**（退出码 3）：第二次 `raise(SIGINT)`
用的是**默认处置**，直接把进程结束掉；**Linux 那一侧三次都进了同一个函数**。

**原因在于标准给实现留了口子**：`signal` 可以选「一次性」语义（调用处理函数之前
先把处置方式复位成默认），也可以选「持久」语义。**本机选的是一次性**，glibc 选持久。

**要写成两边都对，就在处理函数里重新装一次**：

`C`

```c
/* （下面是节选）一次性语义下的写法 */
static void on_int(int sig) {
    hits++;
    signal(SIGINT, on_int);      /* 重新装上，否则下一次就是默认处置 */
    (void)sig;
}
```

**要精细控制（阻塞、屏蔽、带参数的处理函数），标准库给不了。**
那些能力在 POSIX 的 `sigaction` 里，属于平台接口。

> [!WARNING]
> **`SIG_DFL` 是 0，`SIG_IGN` 是 1，`SIG_ERR` 是 `(void (*)(int))-1`。**
> 上面那三个指针值就是实测出来的样子。
> **判断安装是否失败要写 `== SIG_ERR`，不要写 `== NULL`**——
> `NULL` 是「恢复默认处置」这个合法值。

> [!NOTE]
> **本节回顾**：`signal` 装处理函数、`raise` 给自己发信号；
> 处理函数里只能碰 `volatile sig_atomic_t` 与少数安全函数；
> **本机的 `signal` 是一次性语义，Linux 是持久语义**；
> 精细控制要用 `sigaction`。

---

# 第 10 节 `<stddef.h>`、`<stdbool.h>`、`<stdalign.h>`

**三个头文件都很小，各自解决一件语言层面的事。**

`<stddef.h>` 给的是几个「与实现有关的类型和宏」：

| 名字 | 是什么 |
|---|---|
| `size_t` | `sizeof` 的结果类型，无符号 |
| `ptrdiff_t` | 两个指针相减的结果类型，有符号 |
| `NULL` | 空指针常量 |
| `offsetof(T, m)` | 成员 `m` 在结构体 `T` 里的字节偏移 |
| `max_align_t` | 对齐要求最严的那个标量类型，用来问「最严要多少」 |
| `wchar_t` | 宽字符类型 |

`<stdbool.h>` 与 `<stdalign.h>` 在 C23 之前分别提供 `bool`/`true`/`false` 与
`alignas`/`alignof` 这几组宏；**C23 起它们都成了关键字**，两个头文件还在，
但已经没什么事可做。

`实测数据`
`C`

```c
/* misc_headers.c    编译：gcc -std=c23 misc_headers.c -o misc_headers */
#include <stddef.h>
#include <stdbool.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
struct Packed { char c; double d; int i; };

int main(void) {
    /* stddef.h */
    printf("NULL = %p\n", (void *)NULL);
    printf("offsetof(struct Packed, d) = %zu，i = %zu\n",
           offsetof(struct Packed, d), offsetof(struct Packed, i));
    printf("sizeof(struct Packed) = %zu，max_align_t 的对齐 = %zu\n",
           sizeof(struct Packed), alignof(max_align_t));
    printf("ptrdiff_t 是 %zu 字节\n", sizeof(ptrdiff_t));
    /* stdbool.h */
    bool ok = true;
    printf("true=%d false=%d sizeof(bool)=%zu ok=%d\n", true, false, sizeof(bool), (int)ok);
    printf("bool 在 C23 里是关键字：%s\n",
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
           "是"
#else
           "否"
#endif
    );
    /* stdalign.h */
    printf("alignof(double) = %zu，alignof(struct Packed) = %zu\n",
           alignof(double), alignof(struct Packed));
    alignas(64) char buf[128];
    printf("加了 alignas(64) 的数组地址 %% 64 = %llu\n",
           (unsigned long long)((size_t)(void *)buf % 64));
    /* 与 malloc 的对齐对照 */
    void *p = malloc(64);
    printf("malloc 返回的地址 %% 16 = %llu\n", (unsigned long long)((size_t)p % 16));
    free(p);
    return 0;
}
```

`实测数据`
`Text`

```text
NULL = 0000000000000000
offsetof(struct Packed, d) = 8，i = 16
sizeof(struct Packed) = 24，max_align_t 的对齐 = 16
ptrdiff_t 是 8 字节
true=1 false=0 sizeof(bool)=1 ok=1
bool 在 C23 里是关键字：是
alignof(double) = 8，alignof(struct Packed) = 8
加了 alignas(64) 的数组地址 % 64 = 0
malloc 返回的地址 % 16 = 0
```

**`offsetof` 那一行解释了结构体为什么会「变大」**：
`char c` 占 1 字节，但 `double d` 要求 8 字节对齐，
于是中间空了 7 个字节；`int i` 从 16 开始，结构体总共 24 字节。
**把它算清楚是理解内存布局的第一步**（《04-语法/09-结构体、联合体与 enum.md》）。

**`max_align_t` 的对齐是 16，与 `malloc` 给的对齐一致**——这不是巧合：
`malloc` 必须返回「能放下任何标量类型」的内存。**`sizeof(bool)` 是 1**，
`true` 与 `false` 展开成 1 与 0。

> [!TIP]
> **C23 起不必再包含 `<stdbool.h>` 才能用 `bool`**：`bool`、`true`、`false`、
> `alignas`、`alignof`、`static_assert`、`thread_local` 都成了关键字
> （本机实测 `bool` 确实是关键字）。**反过来，要兼容 C17 及更早的编译器，
> 就得把头文件留着。**

> [!NOTE]
> **本节回顾**：`<stddef.h>` 给 `size_t`、`ptrdiff_t`、`NULL`、`offsetof`、`max_align_t`；
> `bool` 与 `alignas`/`alignof` 在 C23 里已经是关键字，
> 那三个头文件仍然可以包含，但不再是必需。

---

# 第 11 节 `<stdarg.h>`

**可变参数函数的完整讲法在《04-语法/07-函数.md》第 6 节**，
那里讲了 `va_list`、`va_start`、`va_arg`、`va_end` 的用法、
默认实参提升带来的坑，以及 `printf` 为什么必须靠格式串才知道参数类型。

**这里只补三点与标准库有关的**：

**第一，`va_list` 的传递要用 `va_copy`。** 把一个 `va_list` 传给另一个函数
（例如 `vprintf`）之后，原来的那份可能已经不能再用；
要保留就先用 `va_copy` 复制一份，用完各自 `va_end`。

**第二，C23 给 `va_start` 松了绑。** 在此之前，`va_start` 的第二个参数
必须是「最后一个具名参数」；C23 起允许省略它。**本机的 GCC 15.2.0 支持新写法，
但为了源码还能在老编译器上编译，写老写法更稳妥。**

**第三，格式串的解析不要自行实现。** 需要「按格式串取参数」时用 `vprintf`、
`vsnprintf`：**它们与 `printf` 共享同一套解析逻辑**，自己写一遍必然会在宽度、
精度、长度修饰符上出偏差（见《06-标准库/A-01-输入输出：stdio.md》）。

> [!NOTE]
> **本节回顾**：`<stdarg.h>` 的用法与陷阱在《04-语法/07-函数.md》第 6 节；
> 这里的关键是「传 `va_list` 要 `va_copy`」与「不要自行解析格式串」。

---

# 第 12 节 速查表

`实测数据`

| 常用件 | 一句话用途 | 典型坑 |
|---|---|---|
| `malloc` / `calloc` | 要一块内存 | `malloc` 不清零；`malloc(0)` 的返回值不能依赖 |
| `realloc` | 改变块大小 | **先把返回值存好再覆盖原指针**，否则失败即泄漏 |
| `free` | 还回去 | `free(NULL)` 合法；**双重释放是未定义行为** |
| `qsort` | 通用排序 | **比较函数不要写 `a - b`**；不是稳定排序 |
| `bsearch` | 通用二分查找 | 数组必须已排序，且用同一个比较函数 |
| `exit` / `atexit` | 正常退出、注册清理 | `atexit` **逆序**执行 |
| `abort` | 异常结束 | 不跑 `atexit`；退出码 3 或 134，随平台 |
| `_Exit` / `_exit` | 立刻结束 | 本机的 `_Exit` 链接不上，用 `_exit` |
| `getenv` | 读环境变量 | 返回值不能改、不能长期持有 |
| `system` | 执行命令 | **拼接外部输入就是注入漏洞**；返回值是实现定义的 |
| `assert` | 检查逻辑错误 | `NDEBUG` 一开就整个消失，**副作用也没了** |
| `isalpha` 一族 | 字符分类 | **参数必须是 `unsigned char` 的值或 `EOF`** |
| `toupper` / `tolower` | 大小写转换 | 返回 `int`；同样受 locale 与参数范围影响 |
| `errno` | 错误码 | 是宏、每线程一份；**调用前要先置 0** |
| `strerror` | 错误码转文本 | 文本随 locale 变，不能做判断依据 |
| `setjmp` / `longjmp` | 非局部跳转 | **C++ 里会跳过析构**；跨过的局部变量值不确定 |
| `signal` / `raise` | 信号 | 本机的处理函数**只生效一次**；处理函数里几乎什么都不能做 |
| `offsetof` | 成员偏移 | 只在标准布局类型上有意义 |
| `bool` / `alignas` / `alignof` | 布尔与对齐 | C23 起是关键字，不再需要头文件 |

**配套示例与练习**：本章节的综合示例见
[`B-examples/06-standard-library/01-c-stdlib-toolbox/`](../B-examples/06-standard-library/01-c-stdlib-toolbox/)，
练习见 [`C-templates/06-standard-library/01-c-stdlib-toolbox/`](../C-templates/06-standard-library/01-c-stdlib-toolbox/)。
示例里用 `qsort` 排词频、用 `malloc` 管缓冲、用 `atexit` 收尾、用 `assert` 自测。

---

# 附录 A 复现本章节实测

## A.1 环境

| 项 | 值 |
|---|---|
| Windows 侧 | MinGW-w64，GCC 15.2.0，目标 `x86_64-win32-seh-rev0`（链接 `msvcrt.dll`），C 标准 `-std=c23` |
| Linux 侧 | WSL Ubuntu 24.04.5，glibc 2.39，GCC 13.3.0，C 标准 `-std=c2x` |
| C++ 侧 | `g++ -std=c++17`，与 C 编译器同一套工具链 |

## A.2 各程序的编译与运行

**每个程序的首行注释里已经写了文件名与编译命令**，按注释中的命令执行即可。
需要额外参数的只有四个：

`Bash`

```bash
gcc -std=c23 -DNDEBUG assert_demo.c -o assert_demo    # 与不带 -DNDEBUG 的那次对照
gcc -std=c23 -O2 setjmp_c.c -o setjmp_c               # 与 -O0 的那次对照
./exit_flush r | wc -c                                # 统计四种退出路径的输出字节数
./exit_flush e | wc -c                                # 把 r 依次换成 e、_、a
```

**会异常结束的三个程序**（退出码见正文，用 `echo $?` 或 `$LASTEXITCODE` 取），
**以及一个期望失败的**（报错原文见正文）：

`Bash`

```bash
./abort_demo         # 退出码 3
./assert_fail        # 退出码 3（本机）/ 134（Linux）
./signal_demo        # 退出码 3（本机）
gcc -std=c23 exit_c99.c -o exit_c99     # 用了 C99 的 _Exit，本机链接失败
```

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《04-语法/07-函数.md》第 6 节 | **前置**：可变参数函数的完整讲解（第 11 节只补了三条） |
| 《04-语法/08-数组、指针与引用.md》第 4.5 小节 | **前置**：回调与函数指针 |
| 《04-语法/09-结构体、联合体与 enum.md》第 1 节 | **前置**：结构体布局与 `offsetof` |
| 《04-语法/11-作用域、生存期与链接.md》第 3.1 小节 | **前置**：三种生存期——`longjmp` 跨过的那些帧 |
| 《04-语法/02-数据类型与类型系统.md》第 5.5 小节 | **前置**：显式类型转换——`ctype` 的参数为什么要转 |
| 《04-语法/14-预处理器.md》第 4 节 | **前置**：条件编译与 `NDEBUG` |
| 《05-类与面向对象/06-RAII 与资源管理.md》第 2.2 小节 | **对照**：编译器保证的三件事，在 `longjmp` 路径上都不成立 |
| 《06-标准库/A-01-输入输出：stdio.md》第 1 节 | **相关**：缓冲刷新、`perror`；第 2 节 `printf` 家族与 `vprintf` |
| 《06-标准库/A-02-字符串与内存：string.h.md》第 5 节 | **相关**：`strtol` 家族的转换与错误检测 |
| 《06-标准库/A-03-数值、数学与随机.md》第 4 节 | **前置**：`<stdlib.h>` 的随机数部分 |
| 《06-标准库/A-04-时间与日期：time.h.md》第 2 节 | **相关**：另一个「静态缓冲区」陷阱 |
| 《06-标准库/B-10-内存与并发的基础设施.md》第 1 节 | **后续**：`allocator` 与对齐分配；第 6 节 `atomic`、第 7 节 `std::thread` |
