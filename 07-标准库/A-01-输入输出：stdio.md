# 输入输出：`<stdio.h>`

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**一个程序如果既不读也不写，它在机器上就没有痕迹。**

C 把「读进来、写出去」这件事交给 `<stdio.h>`，用的是同一套东西——**流**。
屏幕、键盘、文件在这套接口里长得一样，`printf` 不需要知道自己写的是哪一个。
再用 `printf` 与 `scanf` 两家把「二进制与文本怎么换算」包装起来，
日常要用的输入输出就够了。

一上来就学 `printf("%d\n", x)` 有一个副作用：**它看起来像语法**。
于是 `%d` 必须配 `int`、`scanf` 要写 `&`、文件用完要 `fclose`
被当成规定背下来，背后的模型却没有建立。等到出现
「为什么重定向到文件之后输出顺序变了」「为什么 `scanf` 卡住了」
这类问题，就没有地方可以推。

**本章节先立模型，再讲函数。** 先看流与缓冲，把「输出为什么不是立刻出现」
说清楚；再看格式化的两个家族；然后是文件的打开、读写与定位；
最后讲出错之后怎么知道。文本模式与二进制模式在 Windows 上的差别单独实测一节，
因为那是跨平台代码出错最多的地方。

宽字符与 Unicode（`<wchar.h>`）本章节不覆盖，`FILE` 的内部结构留给 `06-更底层`。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整源码与复现命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

---

# 本章节定位

| 问题 | 在哪一节 |
|---|---|
| 流是什么、缓冲有哪三种、输出为什么不是立刻出现 | **一** |
| `printf` 的宽度、精度、长度修饰符、返回值 | **二** |
| `scanf` 的返回值怎么读、为什么会卡住 | **三** |
| `fopen` 的模式、`fread`/`fwrite`、`fseek`/`ftell`、文本与二进制模式 | **四** |
| 出错之后怎么知道：`errno`、`perror`、`ferror`、`feof` | **五** |
| 一页速查 | **六** |

**需要先知道**：

| 前置知识 | 在哪 |
|---|---|
| C 风格字符串与结尾的 0 | 《04-语法/10-字符串.md》第 1 节 |
| 数组退化成指针 | 《04-语法/08-数组、指针与引用.md》第 2 节 |
| 可变参数函数与 `<stdarg.h>` | 《04-语法/07-函数.md》第 6 节 |
| 越界与 AddressSanitizer | 《04-语法/08-数组、指针与引用.md》第 5 节 |
| 上一章：C 标准库怎么组织、怎么查 | 《07-标准库/A-00-导读：C 标准库.md》第 3 节 |

**相邻的章节**：字符串与内存搬运见《07-标准库/A-02-字符串与内存：string.h.md》；
`<errno.h>` 的完整清单见《07-标准库/A-05-工具与其它：stdlib 与杂项.md》；
C++ 侧的对应写法见《07-标准库/B-01-输入输出：iostream.md》第 1 节。

---

# 第 1 节 流与缓冲

## 1.1 流是什么

**流是程序和外部世界之间的一条字节通道。**
它把「往屏幕写」和「往文件写」这两件本来不同的事统一成同一套接口，
于是 `printf` 不必知道自己写的是控制台还是文件——它只管往 `stdout` 里放字节。

`文档`

> "At program startup, three text streams are predefined and are already
> opened — standard input (for reading conventional input), standard output
> (for writing conventional output), and standard error (for writing
> diagnostic output)."
>
> —— N3220 §7.23.3/7

**三个标准流在 `main` 开始之前就已经打开好了。**

`C`

```c
/* three_streams.c    编译：gcc -std=c23 three_streams.c -o three_streams */
#include <stdio.h>

int main(void) {
    printf("stdin=%d stdout=%d stderr=%d\n",
           fileno(stdin), fileno(stdout), fileno(stderr));
    printf("EOF=%d BUFSIZ=%d FOPEN_MAX=%d TMP_MAX=%d FILENAME_MAX=%d\n",
           EOF, BUFSIZ, FOPEN_MAX, TMP_MAX, FILENAME_MAX);
    printf("stdout 与 stderr 是不同的 FILE 对象：%d\n", (void *)stdout != (void *)stderr);

    FILE *f = fopen("three_streams.txt", "w");   /* 文件是第四类目标，接口一样 */
    if (f) { fputs("这一行去了文件\n", f); fclose(f); }
    fputs("这一行去了标准输出\n", stdout);
    return 0;
}
```

`实测数据`
`Text`

```text
stdin=0 stdout=1 stderr=2
EOF=-1 BUFSIZ=512 FOPEN_MAX=20 TMP_MAX=32767 FILENAME_MAX=260
stdout 与 stderr 是不同的 FILE 对象：1
这一行去了标准输出
```

**表里那五个宏的值都是实现定义的**，换个平台就会变。
`BUFSIZ` 是「默认缓冲区大小的建议值」，本机 512 字节；
`FOPEN_MAX` 是「同时能打开多少个文件」，本机 20。
写代码时不该假设这两个数字——要知道本机的值就打印一次，
或者把同时打开的文件数控制在十几个以内。

`fileno` 不是标准 C 的函数（它来自 POSIX，本机头文件里也声明了它），
上面用它只是为了显示三个流是三条不同的通道。

## 1.2 三种缓冲模式

**流不是每写一个字节就送一次。** 中间隔着一块缓冲区：
写进去的数据先落在内存里，攒够了再一次性交给操作系统。
理由是性能——一次系统调用的代价远大于一次内存拷贝。

`文档`

> "The argument mode determines how stream will be buffered, as follows:
> _IOFBF causes input/output to be fully buffered;
> _IOLBF causes input/output to be line buffered;
> _IONBF causes input/output to be unbuffered."
>
> —— N3220 §7.23.5.6/2

`实测数据`
`C`

```c
/* three_modes.c    编译：gcc -std=c23 three_modes.c -o three_modes */
#include <stdio.h>

int main(void) {
    /* setvbuf 必须在任何输出之前调用，否则行为没有保证 */
    int rc = setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
    printf("_IOFBF=%d（全缓冲） _IOLBF=%d（行缓冲） _IONBF=%d（无缓冲）\n",
           _IOFBF, _IOLBF, _IONBF);
    printf("BUFSIZ=%d（默认缓冲区大小的建议值）\n", BUFSIZ);
    printf("setvbuf(_IOFBF) 返回 %d（0 表示成功）\n", rc);
    return 0;
}
```

`实测数据`
`Text`

```text
_IOFBF=0（全缓冲） _IOLBF=64（行缓冲） _IONBF=4（无缓冲）
BUFSIZ=512（默认缓冲区大小的建议值）
setvbuf(_IOFBF) 返回 0（0 表示成功）
```

**三个宏的值是实现定义的**，本机是 0、64、4。
**默认用哪一种，标准也定了规则**：

`文档`

> "As initially opened, the standard error stream is not fully buffered; the
> standard input and standard output streams are fully buffered if and only
> if the stream can be determined not to refer to an interactive device."
>
> —— N3220 §7.23.3/7

翻译成操作层面的结论有两条：**`stderr` 一定不是全缓冲**；
**`stdout` 与 `stdin` 的模式取决于它接在什么上面**——
接终端是行缓冲，接文件或管道是全缓冲。
**于是同一份程序，重定向之后行为会变。**

## 1.3 重定向之后，顺序变了

**先看顺序。** 程序往 `stdout` 打印两行、往 `stderr` 打一行，中间不调用 `fflush`。

`C`

```c
/* buf_order.c    编译：gcc -std=c23 buf_order.c -o buf_order */
#include <stdio.h>

int main(void) {
    printf("1 这是 stdout 的第一行\n");       /* 带换行，但没有 fflush */
    fprintf(stderr, "2 这是 stderr 的一行\n"); /* stderr 默认不缓冲 */
    printf("3 这是 stdout 的第二行\n");
    return 0;                                 /* 正常退出会冲刷 stdout */
}
```

**把两个流重定向到同一个文件**，写入顺序和代码顺序就不一样了：

`实测数据`
`Bash`

```bash
buf_order.exe > order.txt 2>&1
cat order.txt
```

`实测数据`
`Text`

```text
1 这是 stdout 的第一行
3 这是 stdout 的第二行
2 这是 stderr 的一行
```

**`2` 那一行跑到了最后。** `stdout` 接到文件之后变成全缓冲，
前两行一直压在内存里，直到进程退出才冲刷；`stderr` 当时就写进了文件。
**屏幕上看到的顺序是对的，文件里的顺序是错的**——这就是重定向带来的差别。

**再看「进程还没退出时，文件里有什么」。**

`C`

```c
/* buf_probe.c    编译：gcc -std=c23 buf_probe.c -o buf_probe */
#include <stdio.h>
#include <time.h>

/* 空转等待，避免依赖非标准头文件 */
static void spin(double seconds) {
    clock_t t0 = clock();
    while ((double)(clock() - t0) / CLOCKS_PER_SEC < seconds) { }
}

int main(void) {
    printf("12345");        /* 5 字节，没有换行、没有 fflush */
    spin(3.0);              /* 这 3 秒里父进程可以去看文件大小 */
    printf("67890\n");      /* 再写 6 字节 */
    return 0;
}
```

`实测数据`
`PowerShell`

```powershell
$p = Start-Process -FilePath buf_probe.exe -RedirectStandardOutput probe_out.txt -PassThru
Start-Sleep -Milliseconds 1200
"运行中：文件 $((Get-Item probe_out.txt).Length) 字节"
$p.WaitForExit()
"退出后：文件 $((Get-Item probe_out.txt).Length) 字节"
```

`实测数据`
`Text`

```text
运行中：文件 0 字节
退出后：文件 12 字节
```

**运行中写进去的 5 个字节在文件里一个都找不到**，它们只在进程的内存里。
最终是 12 字节而不是 11，是因为文本模式把结尾的 `\n` 写成了 `\r\n`
（见第 4.5 小节）。

**控制台上是另一种情况。** 一个探针程序在真正的控制台窗口里运行，
打印一行之后立刻读光标位置：**光标从第 0 行跳到第 1 行，说明换行已经把内容送出去了**；
同一个程序先 `setvbuf(stdout, NULL, _IOFBF, 4096)` 再跑，
光标停在原地。**这就是「行缓冲」与「全缓冲」的可观察差别。**

**「是不是终端」有现成的判断办法**：`_isatty(_fileno(stdout))` 在重定向到文件或管道时返回 0，
在控制台窗口里返回 **64**。**并非 1**——标准与文档只保证「非零」，
具体值是实现内部的标志位。判断只能写 `if (_isatty(fd))`，
不能写 `if (_isatty(fd) == 1)`。

## 1.4 `stderr` 在两个平台上不一样

第 1.3 小节的顺序实验里，`stderr` 那一行跑到了最后，
说明**在本机上，`stderr` 接到文件之后也被全缓冲了**。
下面这个程序把两个流分别重定向，并在运行到一半时给出两个文件各自的大小。

`C`

```c
/* errbuf.c    编译：gcc -std=c23 errbuf.c -o errbuf
 * 两个流分别重定向，在空闲时看两个文件各有多大。
 */
#include <stdio.h>
#include <time.h>

static void spin(double seconds) {
    clock_t t0 = clock();
    while ((double)(clock() - t0) / CLOCKS_PER_SEC < seconds) { }
}

int main(void) {
    fprintf(stderr, "E1\n");
    spin(2.0);
    printf("O1\n");
    spin(2.0);
    fprintf(stderr, "E2\n");
    return 0;
}
```

`实测数据`
`Text`

```text
Windows（MinGW gcc 15.2.0）
0.7 秒：stderr 文件 = 0 字节，stdout 文件 = 0 字节
2.7 秒：stderr 文件 = 0 字节，stdout 文件 = 0 字节
退出后：stderr 文件 = 8 字节，stdout 文件 = 4 字节

Linux（Ubuntu gcc 13.3.0）
0.7 秒：stderr 文件 = 3 字节，stdout 文件 = 0 字节
退出后：stderr 文件 = 6 字节，stdout 文件 = 3 字节
```

**两侧对照**：Linux 侧 `stderr` 的第一行在 0.7 秒时就落盘了，
`stdout` 一直是 0，符合「`stderr` 不缓冲、`stdout` 全缓冲」；
Windows 侧两个文件在运行中都是 0，退出时才一起出现。

**标准说的是「`stderr` 不是全缓冲」，Windows 这个实现的做法是
「接到文件时照样攒着」。** 这不矛盾——标准在另一处把余地写明了：

`文档`

> "When a stream is unbuffered, characters are intended to appear from the
> source or at the destination as soon as possible. ... Support for these
> characteristics is implementation-defined, and may be affected via the
> setbuf and setvbuf functions."
>
> —— N3220 §7.23.3/3

> [!WARNING]
> **不要把「报错信息一定能立刻看到」当成跨平台成立的事实。**
> 需要保证顺序或及时性时，显式 `fflush(stderr)` 或 `fflush(NULL)`。

## 1.5 `fflush` 与 `setvbuf`

**`fflush(f)` 把流 `f` 的输出缓冲区立刻写出去；`fflush(NULL)` 冲刷所有输出流**，
后者是标准明文支持的用法。`setvbuf` 用来改模式，但有一条调用时机的硬规定：

`文档`

> "The setvbuf function may be used only after the stream pointed to by
> stream has been associated with an open file and before any other
> operation (other than an unsuccessful call to setvbuf) is performed on
> the stream."
>
> —— N3220 §7.23.5.6/2

**也就是「在第一次读写这个流之前」。**

`C`

```c
/* vbuf_mode.c    编译：gcc -std=c23 vbuf_mode.c -o vbuf_mode
 * 用法：vbuf_mode.exe n | l | f     n=无缓冲 l=行缓冲 f=全缓冲
 */
#include <stdio.h>
#include <time.h>

int main(int argc, char **argv) {
    static char mybuf[4096];
    char mode = (argc > 1) ? argv[1][0] : 'l';

    /* setvbuf 只能在第一次读写这个流之前调用 */
    int rc;
    switch (mode) {
    case 'n': rc = setvbuf(stdout, NULL,  _IONBF, 0);            break;
    case 'f': rc = setvbuf(stdout, mybuf, _IOFBF, sizeof mybuf); break;
    default:  rc = setvbuf(stdout, NULL,  _IOLBF, 0);            break;
    }

    printf("模式 %c，setvbuf 返回 %d\n", mode, rc);
    printf("最后一行，没有换行");

    clock_t t0 = clock();                       /* 空转两秒，供外部观察文件大小 */
    while ((double)(clock() - t0) / CLOCKS_PER_SEC < 2.0) { }
    return 0;
}
```

`实测数据`

| 模式 | `setvbuf` 返回 | 运行中文件大小 | 退出后文件大小 | 说明 |
|---|---|---|---|---|
| `n`（`_IONBF`） | 0 | 36 字节 | 56 字节 | 无缓冲：连没有换行的最后一行也立刻写出去了 |
| `l`（`_IOLBF`） | **-1** | 0 字节 | 57 字节 | **本机不支持行缓冲**，模式没改成功，仍是全缓冲 |
| `f`（`_IOFBF`） | 0 | 0 字节 | 56 字节 | 全缓冲：退出时才落盘 |

**`_IOLBF` 返回 -1 是本机运行库的限制**，它不违背标准——
标准要求的正是「请求无法满足时返回非零」。
**控制台上那套行缓冲行为是运行库自己按「接的是不是终端」决定的，
不能用手工调用复现。**

> [!TIP]
> **需要「立刻可见」的输出（日志、进度）时用 `fflush`，不要用 `setvbuf`。**
> `fflush` 在各平台行为一致，`_IOLBF` 在 Windows 上直接失败。

## 1.6 不冲刷就丢数据

**缓冲区在内存里，进程一异常结束，里面的内容就丢失了。** 标准写得很明确：

`文档`

> "If the main function returns to its original caller, or if the exit
> function is called, all open files are closed (hence all output streams
> are flushed) before program termination. Other paths to program
> termination, such as calling the abort function, are not required to
> close all files properly."
>
> —— N3220 §7.23.3/5

**两条正常路径（`main` 返回、`exit`）保证冲刷，其他路径不保证。**

`C`

```c
/* exit_flush.c    编译：gcc -std=c23 exit_flush.c -o exit_flush
 * 用法：exit_flush.exe return | exit | abort
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *how = (argc > 1) ? argv[1] : "return";
    char path[64];
    snprintf(path, sizeof path, "flush_%s.txt", how);

    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen"); return 1; }
    fputs("这一行只在流被冲刷之后才在磁盘上\n", f);

    if (strcmp(how, "exit") == 0)  exit(0);     /* 保证冲刷 */
    if (strcmp(how, "abort") == 0) abort();     /* 标准不要求冲刷 */
    return 0;                                   /* main 返回同样保证冲刷 */
}
```

`实测数据`
`Text`

```text
return：文件 50 字节
exit：  文件 50 字节
abort： 文件 50 字节
```

**三种方式在本机都保住了数据**，包括标准并不要求冲刷的 `abort`。
这不能当成「以后可以不管 `fclose`」的理由——标准没有保证这一条，
换个运行库就可能丢掉。真正无法挽回的是进程被强杀，同一个探针程序在这一情形下的结果如下：

`实测数据`
`PowerShell`

```powershell
$p = Start-Process -FilePath buf_probe.exe -RedirectStandardOutput killed.txt -PassThru
Start-Sleep -Milliseconds 1000
Stop-Process -Id $p.Id -Force
Start-Sleep -Milliseconds 300
"杀掉之后：文件 $((Get-Item killed.txt).Length) 字节"
```

`实测数据`
`Text`

```text
杀掉之后：文件 0 字节
```

**那 5 个字符再也不会出现。** 崩溃、任务管理器结束进程、断电都属于这一类。

> [!CAUTION]
> **重要的数据不要只依赖「程序退出时会冲刷」。**
> 写完之后显式 `fflush` 或 `fclose`，是把「数据已经在磁盘上」
> 从「依赖退出路径」变成「依赖显式写出的那一行」。

> [!NOTE]
> **这一节的三句结论**：流有缓冲，缓冲让输出延迟；
> `stdout` 接终端时行缓冲、接文件时全缓冲，因此重定向会改变顺序；
> 「`stderr` 不是全缓冲」不保证立刻可见，要顺序就显式 `fflush`。

---

# 第 2 节 `printf` 家族的格式化

## 2.1 一次转换的写法

格式串里一段以 `%` 开头的描述叫**一次转换**，完整形态是：

`Text`

```text
%[标志][宽度][.精度][长度修饰符]转换说明符
   ↑     ↑     ↑        ↑           ↑
  可选   可选  可选     可选      必须有，例如 d、s、f
```

`实测数据`
`C`

```c
/* printf_fmt.c    编译：gcc -std=c23 printf_fmt.c -o printf_fmt */
#include <stdio.h>

int main(void) {
    int n;

    /* 宽度与对齐：字段宽度是「至少占几列」，不会截断内容 */
    n = printf("[%d] [%5d] [%-5d] [%05d]\n", 42, 42, 42, 42);
    printf("  ↑ 这一行一共输出了 %d 个字符（含换行）\n", n);

    /* 精度：对整数是最少位数，对浮点是小数位数，对字符串是最大字符数 */
    printf("[%.5d] [%8.3f] [%.3s]\n", 42, 3.14159265, "abcdef");
    /* 符号、进制前缀、大小写 */
    printf("[%+d] [% d] [%#x] [%#o] [%X]\n", 7, 7, 255, 255, 255);
    /* 浮点的三种写法 */
    printf("[%f] [%e] [%g]\n", 1234.5678, 1234.5678, 1234.5678);
    /* 长度修饰符：类型对不上就是未定义行为 */
    printf("[%zu] [%lld] [%hhd] [%p]\n",
           sizeof(long long), -1234567890123LL, (signed char)-1, (void *)0);
    /* 星号：宽度与精度从参数里取 */
    printf("[%*d] [%.*f]\n", 6, 42, 2, 3.14159);

    int used = 0;                       /* %n 记下「已经写了多少」 */
    n = printf("abc%ndef\n", &used);
    printf("  ↑ 返回 %d，%%n 记下的是 %d\n", n, used);
    return 0;
}
```

`实测数据`
`Text`

```text
[42] [   42] [42   ] [00042]
  ↑ 这一行一共输出了 29 个字符（含换行）
[00042] [   3.142] [abc]
[+7] [ 7] [0xff] [0377] [FF]
[1234.567800] [1.234568e+03] [1234.57]
[8] [-1234567890123] [-1] [0000000000000000]
[    42] [3.14]
abcdef
  ↑ 返回 7，%n 记下的是 3
```

`实测数据`

| 写法 | 输出 | 说明 |
|---|---|---|
| `%5d` | `   42` | 不足时左边补空格，超出时不截断 |
| `%-5d` | `42   ` | 负号表示左对齐 |
| `%05d` | `00042` | `0` 标志补零，只在右对齐时有效 |
| `%.5d` | `00042` | 对整数，精度是「最少位数」 |
| `%8.3f` | `   3.142` | 总宽 8、小数 3 位 |
| `%.3s` | `abc` | 对字符串，精度是「最多几个字符」 |
| `%#x` | `0xff` | `#` 给八进制加 `0`、给十六进制加 `0x` |
| `%*d` / `%.*f` | `    42` / `3.14` | 宽度与精度从参数里读，类型必须是 `int` |

**`%p` 打印出来是 16 位十六进制补零**，这是实现定义的形态：
Linux 侧同一段代码打的是带 `0x` 前缀的形式。**不要把 `%p` 的结果拿去解析或比较。**

## 2.2 长度修饰符：类型必须对齐

**同一个 `%d` 背后可能是好几种不同的类型**：`short`、`int`、`long`、`long long`
都是整数，宽度不同，`printf` 靠长度修饰符决定从参数里取几个字节。

`实测数据`

| 修饰符 | 配什么类型 | 常见错误 |
|---|---|---|
| （无） | `int` | 拿它打 `long`，在 Linux 上直接错位 |
| `hh` / `h` | `signed char` / `short` | 打 `int` 会读到多余字节 |
| `l` / `ll` | `long` / `long long` | 打 `int` 会把相邻的字节一起读走 |
| `z` | `size_t` | 打 `int` 时在 64 位 Linux 上错位 |
| `L` | `long double` | 只对浮点有意义 |

`实测数据`
`C`

```c
/* len_mod.c    编译：gcc -std=c23 -Wall len_mod.c -o len_mod */
#include <stdio.h>

int main(void) {
    long long big = 1234567890123LL;
    size_t sz = sizeof big;

    printf("值 = %lld（长度修饰符与类型对齐）\n", big);
    printf("值 = %d（用 %%d 去读 long long，输出是垃圾）\n", big);
    printf("sizeof 的结果配 %%zu：%zu\n", sz);
    printf("把 size_t 按 %%d 打印：%d\n", (int)sz);   /* 先转成 int 才不会错位 */
    return 0;
}
```

`实测数据`
`Text`

```text
值 = 1234567890123（长度修饰符与类型对齐）
值 = 1912276171（用 %d 去读 long long，输出是垃圾）
sizeof 的结果配 %zu：8
把 size_t 按 %d 打印：8
```

**判断办法只有一条：以这个表达式的类型为准。**
`sizeof` 的结果是 `size_t`，所以永远配 `%zu`；`strlen` 的返回值同理。
`printf` 拿到的只是一串字节和一个格式串，**它不检查类型**。

## 2.3 类型写错会怎样

`实测数据`
`C`

```c
/* printf_wrong.c    编译：gcc -std=c23 -Wall -Wextra printf_wrong.c -o printf_wrong */
#include <stdio.h>

int main(void) {
    /* 格式串说「我要一个 int」，实际给的是 double：类型不匹配 */
    printf("把 double 按 %d 打印：[%d]\n", 3.14);
    /* 格式串说「我要一个 double」，实际给的是 int */
    printf("把 int 按 %f 打印：[%f]\n", 42);
    /* 长度修饰符写错：long long 用了 %d */
    printf("把 long long 按 %d 打印：[%d]\n", 1234567890123LL);
    return 0;
}
```

**编译器给出三条警告**（`-Wall -Wextra`）：

`实测数据`
`Text`

```text
printf_wrong.c:6:27: warning: format '%d' expects argument of type 'int',
but argument 2 has type 'double' [-Wformat=]
printf_wrong.c:8:24: warning: format '%f' expects argument of type 'double',
but argument 2 has type 'int' [-Wformat=]
printf_wrong.c:10:30: warning: format '%d' expects argument of type 'int',
but argument 2 has type 'long long int' [-Wformat=]
```

**程序照跑，输出是这样**：

`实测数据`
`Text`

```text
把 double 按 1374389535 打印：[1038570368]
把 int 按 0.000000 打印：[0.000000]
把 long long 按 1912276171 打印：[1]
```

**连那句中文提示都被替换成了数字**——`%d` 从参数区里读走了别的字节，
而且这几个数字每次运行还会变。

> [!CAUTION]
> **格式串与实参类型不匹配是未定义行为**：可能打印垃圾、可能崩、也可能碰巧打对。
> **`-Wall -Wextra` 是这个坑唯一廉价的防线**，三条警告一条不少。

## 2.4 返回值：写了多少个字符

`文档`

> "The printf function returns the number of characters transmitted, or a
> negative value if an output or encoding error occurred."
>
> —— N3220 §7.23.6.1/3

`C`

```c
/* printf_ret.c    编译：gcc -std=c23 printf_ret.c -o printf_ret
 * 返回值是「交给这个流的字符数」，含格式串里的普通字符与换行。
 */
#include <stdio.h>

int main(void) {
    int n1 = printf("abc\n");                 /* 4 个字符 */
    int n2 = printf("%d", 12345);             /* 5 个字符，没有换行 */
    int n3 = printf("[%8.3f]\n", 3.14159);    /* 10 个字符 */
    printf("\n三次调用的返回值：%d、%d、%d\n", n1, n2, n3);

    int used = -1;                            /* %n 不输出字符，只记下已经写了多少 */
    int n4 = printf("abcdef%n\n", &used);
    printf("这次返回 %d，%%n 记下的是 %d\n", n4, used);

    FILE *f = fopen("printf_ret_out.txt", "w");
    if (f) {
        printf("往文件里 fprintf 返回 %d\n", fprintf(f, "%s-%d\n", "abc", 42));
        fclose(f);
    }
    return 0;
}
```

`实测数据`
`Text`

```text
三次调用的返回值：4、5、11
这次返回 7，%n 记下的是 6
往文件里 fprintf 返回 7
```

**返回值是「实际交给这个流的字符数」，不是「转换了几次」。**
`%n` 记下的是「在它之前已经输出了多少字符」（这里是 6，不含 `%n` 之后的换行），
调试格式化逻辑时有用，但**不要把它用在外部可控的格式串里**。

**出错时返回值不可靠**，这一点在第 5.3 小节用一组实测说明：
有缓冲的 `fprintf` 往只读流里写会返回字符数（看似成功），
错误只体现在 `ferror` 与 `errno` 上；关掉缓冲的 `fputc` 才返回 `-1`。

## 2.5 与 C++ 的对照

**同一件事在 C++ 里由 `std::cout` 与 `operator<<` 完成**，
格式化交给 `<iomanip>`（完整对照见《07-标准库/B-01-输入输出：iostream.md》第 3 节）。
两者的吞吐对比如下：同样输出 200 万行 CSV，各跑三轮。

`实测数据`
`C`

```c
/* io_speed_c.c    编译：gcc -std=c23 -O2 io_speed_c.c -o io_speed_c */
#include <stdio.h>

int main(void) {
    for (int i = 0; i < 2000000; i++) {
        printf("%d,%d,%.3f\n", i, i * 2, i * 0.5);
    }
    return 0;
}
```

`实测数据`
`C++`

```cpp
/* io_speed_cpp.cpp    编译：g++ -std=c++17 -O2 io_speed_cpp.cpp -o io_speed_cpp */
#include <iostream>
#include <iomanip>

int main() {
    std::cout << std::fixed << std::setprecision(3);        /* 与 printf 的 %.3f 对齐 */
    for (int i = 0; i < 2000000; i++) {
        std::cout << i << ',' << i * 2 << ',' << i * 0.5 << '\n';
    }
    return 0;
}
```

`实测数据`
`C++`

```cpp
/* io_speed_cpp2.cpp    编译：g++ -std=c++17 -O2 io_speed_cpp2.cpp -o io_speed_cpp2 */
#include <iostream>
#include <iomanip>

int main() {
    std::ios::sync_with_stdio(false);       /* 不再和 C 的流同步 */
    std::cout << std::fixed << std::setprecision(3);
    for (int i = 0; i < 2000000; i++) {
        std::cout << i << ',' << i * 2 << ',' << i * 0.5 << '\n';
    }
    return 0;
}
```

**三份输出逐字节相同**，都是 54,111,115 字节。

`实测数据`

| 轮次 | `printf` | `std::cout`（默认同步） | `std::cout`（关同步） |
|---|---|---|---|
| 第 1 轮 | 2,147 ms | 1,144 ms | 1,055 ms |
| 第 2 轮 | 1,103 ms | 1,126 ms | 1,051 ms |
| 第 3 轮 | 1,088 ms | 1,097 ms | 1,051 ms |

**第一轮那个 2,147 ms 是冷启动的假象。** 第 2、3 轮中，三者都在 1,050 到 1,130 ms 之间，
**`printf` 与 `std::cout` 的吞吐差不多**，关掉同步稳定快 5% 左右。

**「`iostream` 慢」在这个场景里测不出来。** 真正影响速度的是缓冲策略与写入粒度，
不是选哪一家。两家的差别在别处：类型安全、可扩展、代码量——留到 B 段展开。

---

# 第 3 节 `scanf` 家族的坑

## 3.1 返回值有三种含义

`文档`

> "The scanf function returns the value of the macro EOF if an input failure
> occurs before the first conversion (if any) has completed. Otherwise, the
> scanf function returns the number of input items assigned, which can be
> fewer than provided for, or even zero, in the event of an early matching
> failure."
>
> —— N3220 §7.23.6.4/3

`实测数据`

| 返回值 | 含义 | 该怎么办 |
|---|---|---|
| `EOF`（本机 -1） | 还没读到任何东西，输入就结束了 | 结束循环 |
| `0` | 一个都没匹配上 | 通常说明输入格式不对 |
| `> 0` | 成功赋值了几项，**可能少于所要求的项数** | 与期望的项数比较 |

**观察这一类行为用 `sscanf` 最方便**：输入是内存里的字符串，不必准备输入文件。

`实测数据`
`C`

```c
/* sscanf_demo.c    编译：gcc -std=c23 sscanf_demo.c -o sscanf_demo */
#include <stdio.h>

int main(void) {
    int rc, year, month, day;
    double price;
    char name[16];

    rc = sscanf("2026-09-30", "%d-%d-%d", &year, &month, &day);
    printf("整串都匹配：返回 %d，得到 %d/%d/%d\n", rc, year, month, day);

    rc = sscanf("2026-09-xx", "%d-%d-%d", &year, &month, &day);
    printf("第三个字段坏了：返回 %d，day 仍是旧值 %d\n", rc, day);

    rc = sscanf("abc", "%d", &year);
    printf("开头就不匹配：返回 %d，year 未被写入（仍是 %d）\n", rc, year);

    rc = sscanf("", "%d", &year);
    printf("空字符串：返回 %d（EOF 就是 %d）\n", rc, EOF);

    rc = sscanf("苹果 3.50", "%15s %lf", name, &price);
    printf("混着读：返回 %d，name = %s，price = %.2f\n", rc, name, price);

    rc = sscanf("42abc", "%d", &year);
    printf("后面有多余字符：返回 %d，year = %d\n", rc, year);
    return 0;
}
```

`实测数据`
`Text`

```text
整串都匹配：返回 3，得到 2026/9/30
第三个字段坏了：返回 2，day 仍是旧值 30
开头就不匹配：返回 0，year 未被写入（仍是 2026）
空字符串：返回 -1（EOF 就是 -1）
混着读：返回 2，name = 苹果，price = 3.50
后面有多余字符：返回 1，year = 42
```

**两个容易忽略的点**：**没被赋值的变量保持原样**（第二行的 `day` 还是上一轮的 30），
因此「读失败了」只能看返回值；**返回值只数「赋成了几项」**，
最后一行 `"42abc"` 返回 1，多出来的 `abc` 留在流里不作处理。

## 3.2 匹配失败时，那个字符还在流里

**`scanf` 遇到无法转换的输入时不会把它读走。**
代码里写 `while (scanf("%d", &x) == 1)`，一旦输入里出现一个非数字，
**循环就永远转下去**。这一步拆开之后，流里留下的内容如下。

`实测数据`
`C`

```c
/* scanf_trap.c    编译：gcc -std=c23 scanf_trap.c -o scanf_trap
 * 用法：scanf_trap.exe < scanf_input.txt
 */
#include <stdio.h>

int main(void) {
    int n = 0;
    char word[8];
    char line[16];
    int ch;

    int rc = scanf("%d", &n);            /* 第一关：%d 碰到非数字 */
    if (rc == EOF) { puts("（标准输入已经结束）"); return 0; }
    printf("scanf(\"%%d\") 返回 %d，n 还是原来的 %d\n", rc, n);

    ch = getchar();                      /* 那个吃不动的东西还在流里 */
    printf("紧随其后的字符码值是 %d，也就是 '%c'\n", ch, ch);

    while ((ch = getchar()) != '\n' && ch != EOF) { }   /* 补救：把这一行丢干净 */
    puts("把这一行丢干净之后，再读一次");

    rc = scanf("%d", &n);                /* 第二关：本行的残余仍留在流里 */
    printf("scanf(\"%%d\") 返回 %d，n = %d\n", rc, n);
    if (fgets(line, sizeof line, stdin))
        printf("紧接着 fgets 读到 [%s]", line);

    rc = scanf("%7s", word);             /* 第三关：%s 必须带宽度 */
    printf("scanf(\"%%7s\") 返回 %d，word = [%s]\n", rc, word);
    return 0;
}
```

`实测数据`
`Text`

```text
输入文件三行：abc 42 / 7 8 9 / supercalifragilistic

scanf("%d") 返回 0，n 还是原来的 0
紧随其后的字符码值是 97，也就是 'a'
把这一行丢干净之后，再读一次
scanf("%d") 返回 1，n = 7
紧接着 fgets 读到 [ 8 9
]scanf("%7s") 返回 1，word = [superca]
```

**三行结论分别对应三行输出**：

- `"abc"` 让 `%d` 返回 0，**一个字符都没被消耗**，`getchar` 立刻读到 `'a'`。
  这就是死循环的来源：同一个字符会被无数次数到；
- 读到 `7` 之后，**同一行剩下的 `" 8 9\n"` 还在流里**，
  下一个 `fgets` 把它们全读走了——它读到的不是「下一行」，而是「上一行的残余」；
- `%7s` 只吃 7 个字符，剩下的留在流里，这正是必须给 `%s` 写宽度的原因。

## 3.3 `%s` 必须写宽度

**`scanf` 与 `printf` 最大的不同：它要往调用者给出的内存里写东西，
而那块内存有多大它并不知道。**

`实测数据`
`C`

```c
/* scanf_width.c    编译：gcc -std=c23 scanf_width.c -o scanf_width */
#include <stdio.h>
#include <string.h>

int main(void) {
    char small[8], big[64];

    /* 带宽度：最多读 7 个字符，第 8 格留给结尾的 0 */
    int rc = sscanf("supercalifragilistic", "%7s", small);
    printf("%%7s 读进 8 字节的缓冲区：返回 %d，内容 [%s]，长度 %zu\n",
           rc, small, strlen(small));

    /* 不带宽度：读多少由输入决定，缓冲区多大它并不知道 */
    rc = sscanf("supercalifragilistic", "%s", big);
    printf("%%s  读进 64 字节的缓冲区：返回 %d，内容 [%s]，长度 %zu\n",
           rc, big, strlen(big));
    return 0;
}
```

`实测数据`
`Text`

```text
%7s 读进 8 字节的缓冲区：返回 1，内容 [superca]，长度 7
%s  读进 64 字节的缓冲区：返回 1，内容 [supercalifragilistic]，长度 20
```

**`%s` 读进来的长度是输入决定的，不是缓冲区决定的。**
把上面那个 `big` 换成 `small`，就是一次 20 字节写进 8 字节的越界：

`C`

```c
/* 节选：上面那个程序换掉目标缓冲区之后的三行（编译命令加上 -fsanitize=address -g） */
    char small[8];
    sscanf("supercalifragilistic", "%s", small);   /* 没有宽度 */
    printf("[%s]\n", small);
```

AddressSanitizer 给出的报告原文如下（复现环境见《07-标准库/A-02-字符串与内存：string.h.md》第 4.3 小节）：

`实测数据`
`Text`

```text
==486==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7f7682100028 at pc 0x7f768445dad0
WRITE of size 21 at 0x7f7682100028 thread T0
    #0 … in scanf_common …
    #3 0x563b55b822d4 in main asan_pct_s.c:6
  This frame has 1 object(s):
    [32, 40) 'small' (line 5) <== Memory access at offset 40 overflows this variable
SUMMARY: AddressSanitizer: stack-buffer-overflow … in scanf_common
```

同一条规则适用于所有会写内存的转换：`%s`、`%c` 的数组形式、`%[` 集合形式，
都必须写宽度，**而且宽度要比缓冲区小 1**（留结尾的 0）。

## 3.4 换行符会留到下一次读

**`scanf("%d", &n)` 读完数字就停，后面的换行留在流里**，
下一个读整行的函数会先读到它，于是得到一个「空行」。
这就是上一小节 `fgets` 读到 `" 8 9"` 的同一个原因。

**避免的办法是统一读法**：要么全用 `scanf`，要么全用 `fgets` 读整行再解析。
后者可控得多，因为拿到的是哪一行是明确的。

`实测数据`
`C`

```c
/* fgets_demo.c    编译：gcc -std=c23 fgets_demo.c -o fgets_demo
 * 用法：fgets_demo.exe < fgets_input.txt
 */
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[16];      /* 故意开得比某些行长 */

    while (fgets(line, sizeof line, stdin)) {
        size_t len = strlen(line);
        int has_nl = (len > 0 && line[len - 1] == '\n');   /* 换行也读进来了 */
        printf("读到 %zu 个字符，结尾有换行：%s  内容 [%s]",
               len, has_nl ? "是" : "否", line);
        if (!has_nl) {                      /* 装不下：剩下的还在流里 */
            int ch, dropped = 0;
            while ((ch = getchar()) != '\n' && ch != EOF) dropped++;
            printf("  ← 被截断，另外丢掉了 %d 个字符\n", dropped);
        } else {
            line[strcspn(line, "\r\n")] = '\0';            /* 去掉行尾换行 */
            printf("  ← 去换行之后 [%s]\n", line);
        }
    }
    return 0;
}
```

`实测数据`
`Text`

```text
输入文件三行：abc / 一二三四五六七八九十 / 最后一行没有换行

读到 4 个字符，结尾有换行：是  内容 [abc
]  ← 去换行之后 [abc]
读到 15 个字符，结尾有换行：否  内容 [一二三四五]  ← 被截断，另外丢掉了 15 个字符
读到 15 个字符，结尾有换行：否  内容 [最后一行没]  ← 被截断，另外丢掉了 9 个字符
```

**`fgets` 的三条行为**：**它把换行也读进来**（只要装得下），
不需要时应删掉，惯用写法是 `line[strcspn(line, "\r\n")] = '\0';`；
**它最多读 `n-1` 个字符**并一定补上结尾的 `0`，因此不会越界；
**装不下时它不报错**，只是这一行被切成两半，剩下的留给下一次调用，
判断依据就是结尾有没有换行。

**「丢掉了 15 个字符」那一行还有一层信息**：一行中文 10 个字，
在 UTF-8 里是 30 个字节，`char[16]` 装不下，于是按字节被切成了两半。
**按字节操作 `char[]` 时中文会被切开**，编码层面的问题 B 段展开
（见《07-标准库/B-02-std-string 与 string_view.md》第 8 节）。

## 3.5 什么时候该用 `scanf`

**判断标准是「输入格式有多规整」。**

`实测数据`

| 情形 | 用什么 | 理由 |
|---|---|---|
| 定格式的输入（竞赛、固定协议） | `scanf` | 短、直观 |
| 配置文件、日志、CSV | **`fgets` + 解析** | 需要看整行、需要报行号、要处理超长行 |
| 输入来自用户 | **`fgets`** | 用户输入无法预期，`scanf` 的失败模式太多 |

**读整行再解析的骨架**：

`实测数据`
`C`

```c
/* read_loop.c    编译：gcc -std=c23 read_loop.c -o read_loop
 * 用法：read_loop.exe < read_input.txt
 * 逐行读整个输入并统计：比 scanf 稳得多的读法。
 */
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[256];
    long lines = 0, chars = 0, longest = 0;
    int truncated = 0;

    while (fgets(line, sizeof line, stdin)) {
        size_t len = strlen(line);
        lines++;
        chars += (long)len;
        if ((long)len > longest) longest = (long)len;
        if (len == sizeof line - 1 && line[len - 1] != '\n') {   /* 这一行没读完 */
            truncated++;
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) { }
        }
    }
    printf("行数 %ld，字符数 %ld，最长一行 %ld\n", lines, chars, longest);
    if (truncated) printf("有 %d 行超过了缓冲区\n", truncated);
    return 0;
}
```

`实测数据`
`Text`

```text
输入文件：与第 3.4 小节同一份（共 59 字节）

行数 3，字符数 59，最长一行 31
```

`fgets` 在文件末尾与出错时都返回 `NULL`，要分清这两者只能靠 `feof` 与 `ferror`
（见第 5.3 小节）。**同一个项目里最好不要两套读法混用**：
混用时「上一个 `scanf` 留下的换行」会变成下一个 `fgets` 读到的空行，
而这种错从输出上看不出来——它只是少了一行数据。

---
# 第 4 节 文件：打开、读写、定位

## 4.1 `fopen` 与 `fclose`：模式与返回值

**`fopen` 把「一个文件」变成「一条流」**，此后 `fread`、`fprintf`、`fseek` 都只跟这条流打交道。
它的第一个参数是路径，第二个参数是模式串，返回值是流指针——**失败时返回 `NULL`**。

| 模式 | 读 | 写 | 文件不存在时 | 打开后内容 |
|---|---|---|---|---|
| `"r"` | 是 | 否 | 失败，返回 `NULL` | 保留 |
| `"w"` | 否 | 是 | 新建 | **清空** |
| `"a"` | 否 | 是（追加） | 新建 | 保留，写在末尾 |
| `"r+"` | 是 | 是 | 失败，返回 `NULL` | 保留 |
| `"w+"` | 是 | 是 | 新建 | **清空** |
| `"a+"` | 是 | 是（追加） | 新建 | 保留，写在末尾 |
| 加 `b`（如 `"rb"`、`"wb"`） | — | — | — | **不做换行转换**，见第 4.5 小节 |

**`"w"` 会清空已有文件，这一步没有第二次机会。** 要「没有就新建、有就保留」，
C 标准里没有这个模式，须先试 `"r"`，失败再试 `"w"`。

`C`

```c
/* file_roundtrip.c    编译：gcc -std=c23 file_roundtrip.c -o file_roundtrip */
#include <stdio.h>

int main(void) {
    const char *path = "roundtrip.txt";
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen 写"); return 1; }

    /* fputs 写字符串，fprintf 写格式化内容：返回值都要看 */
    int rc1 = fputs("第一行\n", f);
    int rc2 = fprintf(f, "%s=%d\n", "count", 3);
    printf("fputs 返回 %d（非负表示成功），fprintf 返回 %d（写出的字符数）\n", rc1, rc2);

    /* 看看写位置：ftell 给的是「距离文件开头多远」 */
    long pos = ftell(f);
    printf("还没关闭时 ftell = %ld\n", pos);
    printf("fclose 返回 %d\n", fclose(f));

    f = fopen(path, "rb");
    if (!f) { perror("fopen 读"); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    printf("关闭之后按二进制打开，文件共 %ld 字节\n", size);
    rewind(f);

    /* fread 一次读一块，返回的是「读到了几个元素」 */
    char block[64];
    size_t got = fread(block, 1, sizeof block - 1, f);
    block[got] = '\0';
    printf("fread 返回 %zu，内容：\n%s", got, block);
    printf("读完之后 feof = %d，ferror = %d\n", feof(f) != 0, ferror(f) != 0);
    fclose(f);
    remove(path);
    return 0;
}
```

`实测数据`
`Text`

```text
fputs 返回 0（非负表示成功），fprintf 返回 8（写出的字符数）
还没关闭时 ftell = 20
fclose 返回 0
关闭之后按二进制打开，文件共 20 字节
fread 返回 20，内容：
第一行
count=3

读完之后 feof = 1，ferror = 0
```

两处细节值得留意：**`fputs` 成功时返回的是非负数**，本机给 0，不能拿它当字符数；
**`ftell` 在文件没关闭时就能用**，它问的是流当前的位置，不必等 `fclose`。

`fclose` 做两件事：把缓冲区里剩下的内容冲刷出去，释放这条流。**返回值也要看**，
因为它正是「冲刷成没成功」的答案：

| 写法 | 后果 |
|---|---|
| `fclose(f)` 不检查 | 磁盘满、网络盘断开时数据悄悄丢一半 |
| 忘记 `fclose` | 正常退出时由运行时补上；`abort` 或崩溃时缓冲区内容全丢 |
| 对同一个流 `fclose` 两次 | 未定义行为，通常直接崩 |

**不检查 `fopen` 的返回值是这一节最常见的错**，它的表现两个平台完全不同：

`C`

```c
/* null_file.c    编译：gcc -std=c23 null_file.c -o null_file
 * 不检查 fopen 的返回值：失败时拿到 NULL，接着用就崩。
 */
#include <stdio.h>

int main(void) {
    FILE *f = fopen("no_such_file.txt", "r");   /* 失败，f 是 NULL */

    char line[64];
    if (fgets(line, sizeof line, f)) {          /* 把 NULL 交给了 fgets */
        printf("读到了 [%s]\n", line);
    }
    fclose(f);
    return 0;
}
```

`实测数据`
`Text`

```text
Windows 侧 gcc 15.2（MinGW-w64／msvcrt）：没有输出，退出码 0
Linux 侧 gcc 13.3（glibc）：无输出，退出码 139（SIGSEGV，段错误）
```

> [!CAUTION]
> **这份差异本身就是结论：同一段错代码，Windows 上安安静静地什么也没做，
> Linux 上直接段错误。** 判断「有没有出错」只能靠自己检查返回值，
> 不能靠程序有没有崩——它可能在本机不崩，在别人的机器上崩。

## 4.2 `fread` 与 `fwrite`：按块读写

两个函数的签名一样，返回值也一样：**「成功读/写了几个元素」，不是「几个字节」**。

`文档`

> "The fread function returns the number of elements successfully read, which
> may be less than nmemb if a read error or end-of-file is encountered."
>
> —— N3220 §7.23.8.1/3

`C`

```c
/* fread_partial.c    编译：gcc -std=c23 fread_partial.c -o fread_partial
 * fread 的返回值是「读到了几个元素」，元素大小是第二个参数。
 */
#include <stdio.h>

struct rec { int id; char name[12]; };

int main(void) {
    const char *path = "recs.bin";
    FILE *f = fopen(path, "wb");
    struct rec out[3] = {{1, "one"}, {2, "two"}, {3, "three"}};
    size_t wrote = fwrite(out, sizeof out[0], 3, f);
    printf("fwrite 写了 %zu 个元素，每个 %zu 字节\n", wrote, sizeof out[0]);
    fclose(f);

    f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    printf("文件共 %ld 字节\n", ftell(f));
    rewind(f);

    struct rec in[5];
    size_t got = fread(in, sizeof in[0], 5, f);      /* 只准备了 3 个 */
    printf("要 5 个，fread 返回 %zu 个\n", got);
    for (size_t i = 0; i < got; i++) printf("  第 %zu 条：id=%d name=%s\n", i, in[i].id, in[i].name);
    printf("此时 feof = %d（到末尾了），ferror = %d\n", feof(f) != 0, ferror(f) != 0);
    fclose(f);
    remove(path);
    return 0;
}
```

`实测数据`
`Text`

```text
fwrite 写了 3 个元素，每个 16 字节
文件共 48 字节
要 5 个，fread 返回 3 个
  第 0 条：id=1 name=one
  第 1 条：id=2 name=two
  第 2 条：id=3 name=three
此时 feof = 1（到末尾了），ferror = 0
```

**「要 5 个给 3 个」不是错误**，它是「文件到这儿就没了」的正常答案。
只有 `ferror(f)` 为真才说明读的过程中出了错。

**结构体直接写进文件的前提是「写的人和读的人是同一个程序、同一个编译器」**：
结构体里有填充字节（本例 `int` 与 `char[12]` 之间没有填充，换一个成员顺序就会多出 4 字节），
不同编译器、不同对齐设置下的布局可能不同。**要长期保存或跨程序交换，就得逐字段写成文本或者定长字段。**

| 需求 | 用什么 | 理由 |
|---|---|---|
| 结构体数组整块存盘 | `fwrite` + `fread` | 快，但只在同构环境下成立 |
| 给人看的文本 | `fprintf` / `fgets` | 换平台、换语言都能读 |
| 定长记录的二进制文件 | `fwrite` + `fseek` 定位 | 记录号 × 记录长度就是偏移 |
| 逐字节处理 | `fgetc` / `fputc` | 见下一小节 |

## 4.3 `fgetc` 与 `fgets`：逐个与逐行

| 函数 | 一次处理 | 返回值 | 结尾 0 |
|---|---|---|---|
| `fgetc` | 一个字节 | `int`：字符，或者 `EOF` | 不涉及 |
| `fputc` | 一个字节 | `int`：写出的字符，或者 `EOF` | 不涉及 |
| `fgets` | 一行（最多 n−1 个字符） | 缓冲区首地址，或者 `NULL` | **一定补** |
| `fputs` | 一个字符串 | 非负表示成功 | 不涉及 |

> [!WARNING]
> **`fgetc` 的返回值必须用 `int` 接，不能存进 `char`。**
> 因为 `EOF` 是一个负的 `int`（本机 −1），存进 `char` 之后可能被截成某个正常字符
> （`0xFF` 在某些平台上就是 −1，另一些平台上变成 255），
> **判断循环结束的那一句就永远不成立，程序变成死循环。**
> 这一点在《07-标准库/A-05-工具与其它：stdlib 与杂项.md》第 6 节讲 `<ctype.h>` 时也会遇到，同一个道理。

`C`

```c
/* lines_count.c    编译：gcc -std=c23 -Wall -Wextra lines_count.c -o lines_count */
#include <stdio.h>

int main(void) {
    const char *path = "sample.txt";
    FILE *w = fopen(path, "wb");   /* 二进制写，避免 CRLF 转换干扰 */
    fputs("first\nsecond line\n\nlast without newline", w);
    fclose(w);

    /* 一、fgetc：一次一个字符，最直接 */
    FILE *f = fopen(path, "rb");
    long chars = 0, lines = 0;
    int c, prev = '\n';
    while ((c = fgetc(f)) != EOF) {
        chars++;
        if (c == '\n') lines++;
        prev = c;
    }
    if (prev != '\n') lines++;          /* 最后一行没有换行也算一行 */
    printf("fgetc：%ld 字节，%ld 行，读到结尾后 feof = %d\n", chars, lines, feof(f) != 0);
    fclose(f);

    /* 二、fgets：一次一行，装不下就被切开 */
    f = fopen(path, "rb");
    char buf[8];
    int n = 0;
    while (fgets(buf, sizeof buf, f)) {
        n++;
        printf("  第 %d 次读到 [", n);
        for (const char *p = buf; *p; p++) putchar(*p == '\n' ? '$' : *p);
        printf("]\n");
    }
    printf("fgets：一共调用 %d 次（缓冲区只有 %zu 字节）\n", n, sizeof buf);
    fclose(f);
    remove(path);
    return 0;
}
```

`实测数据`
`Text`

```text
fgetc：39 字节，4 行，读到结尾后 feof = 1
  第 1 次读到 [first$]
  第 2 次读到 [second ]
  第 3 次读到 [line$]
  第 4 次读到 [$]
  第 5 次读到 [last wi]
  第 6 次读到 [thout n]
  第 7 次读到 [ewline]
fgets：一共调用 7 次（缓冲区只有 8 字节）
```

**同一个文件，一次一行的读法调用了 7 次，其中两次是「一行被切开」**：
`second line` 与 `last without newline` 都超过了 8 字节的缓冲区。
判断依据还是第 3.4 小节那条：**读回来的内容结尾有没有 `\n`**。
有换行的行是完整读到的，没有的就是被切开的——**不要拿 `feof` 当「这一行读完了吗」的判据**。

## 4.4 `fseek`、`ftell`、`rewind`：定位

| 函数 | 作用 | 返回值 |
|---|---|---|
| `fseek(f, 偏移, 起点)` | 把读写位置挪到指定处 | 0 表示成功，非 0 表示失败 |
| `ftell(f)` | 问当前位置 | 当前偏移，失败返回 −1 |
| `rewind(f)` | 回到开头 | 无返回值，也**不报告失败** |

起点有三个：`SEEK_SET`（文件开头）、`SEEK_CUR`（当前位置）、`SEEK_END`（文件末尾），
偏移可以是负数。**`fseek(f, 0, SEEK_END)` 加 `ftell(f)` 是「问文件多大」的标准写法。**

`C`

```c
/* seek_write.c    编译：gcc -std=c23 seek_write.c -o seek_write
 * 定位到文件末尾之后很远的地方再写：中间那段是什么？
 */
#include <errno.h>
#include <stdio.h>

int main(void) {
    const char *path = "hole.bin";
    FILE *f = fopen(path, "wb");
    fputs("HEAD", f);
    fseek(f, 1000, SEEK_END);          /* 往后跳 1000 字节 */
    fputs("TAIL", f);
    fclose(f);

    f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    printf("文件大小 %ld 字节（写了 4 + 4 个字节，中间隔着 1000 字节）\n", size);

    /* 读一下中间那一格，看看是什么 */
    fseek(f, 500, SEEK_SET);
    int c = fgetc(f);
    printf("偏移 500 处读到 %d（0 表示空字节）\n", c);

    /* 定位失败长什么样：偏移写成负数 */
    int rc = fseek(f, -1, SEEK_SET);
    printf("fseek(f, -1, SEEK_SET) 返回 %d（非 0 表示失败），errno = %d\n", rc, errno);
    printf("失败之后 feof = %d，ferror = %d\n", feof(f) != 0, ferror(f) != 0);
    fclose(f);
    remove(path);
    return 0;
}
```

`实测数据`
`Text`

```text
文件大小 1008 字节（写了 4 + 4 个字节，中间隔着 1000 字节）
偏移 500 处读到 0（0 表示空字节）
fseek(f, -1, SEEK_SET) 返回 -1（非 0 表示失败），errno = 22
失败之后 feof = 0，ferror = 0
```

**跳过去的那 1000 字节在磁盘上读出来全是 0**，这叫文件空洞，
文件系统只在真正写过的地方占用空间。用 `fseek` + `fwrite` 预分配一个定长记录文件时，
中间那些空洞是正常现象。

**`fseek` 失败既不改 `feof` 也不改 `ferror`**，它自己的返回值就是全部信息；
`errno` 会被设成 22（`EINVAL`，参数不合法）。这三者要分清：

| 出了什么事 | 看哪里 |
|---|---|
| 定位失败（偏移非法、流不可定位） | `fseek` 的返回值、`errno` |
| 读到文件末尾 | `feof(f)` |
| 读写出错 | `ferror(f)` |
| 缓冲区冲刷失败 | `fclose` 的返回值 |

**stdin／stdout 接在管道或控制台上时不可定位**，对着它们调用 `fseek` 会失败，
这是 `fseek(stdin, 0, SEEK_END)` 这类「问输入有多长」的写法在实际中行不通的原因。

## 4.5 文本模式与二进制模式

**同一个 `fputs("a\nb\n")`，在 Windows 上写出来的文件比 Linux 上多两个字节。**
差别来自文本模式下的换行转换：写的时候 `\n` 变成 `\r\n`，读的时候再变回来。
二进制模式（模式串里带 `b`）不做任何转换。

`C`

```c
/* text_vs_binary.c    编译：gcc -std=c23 text_vs_binary.c -o text_vs_binary
 * 同一份内容，分别用文本模式与二进制模式写，再看磁盘上究竟有多少字节。
 */
#include <stdio.h>

static long size_of(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

static void dump(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return;
    int c;
    printf("  ");
    while ((c = fgetc(f)) != EOF) printf("%02X ", (unsigned char)c);
    printf("\n");
    fclose(f);
}

int main(void) {
    const char *content = "a\nb\n";        /* 5 个字符，其中两个是换行 */

    FILE *f = fopen("text_mode.txt", "w");
    fputs(content, f);
    fclose(f);

    f = fopen("binary_mode.txt", "wb");
    fputs(content, f);
    fclose(f);

    printf("写进去的字符串是 \"a\\nb\\n\"，共 5 个字符\n");
    printf("文本模式 \"w\"  磁盘上 %ld 字节\n", size_of("text_mode.txt"));
    printf("二进制模式 \"wb\" 磁盘上 %ld 字节\n", size_of("binary_mode.txt"));
    printf("文本模式实际字节：\n");
    dump("text_mode.txt");
    printf("二进制模式实际字节：\n");
    dump("binary_mode.txt");

    /* 读数也要看模式：文本模式读回来会少掉那些回车 */
    f = fopen("text_mode.txt", "r");
    fseek(f, 0, SEEK_END);
    printf("文本模式 \"r\" 打开，fseek 到末尾后 ftell = %ld\n", ftell(f));
    fclose(f);
    f = fopen("text_mode.txt", "rb");
    fseek(f, 0, SEEK_END);
    printf("二进制模式 \"rb\" 打开，fseek 到末尾后 ftell = %ld\n", ftell(f));
    fclose(f);

    remove("text_mode.txt");
    remove("binary_mode.txt");
    return 0;
}
```

`实测数据`
`Text`

```text
Windows 侧 gcc 15.2（MinGW-w64）：
写进去的字符串是 "a\nb\n"，共 5 个字符
文本模式 "w"  磁盘上 6 字节
二进制模式 "wb" 磁盘上 4 字节
文本模式实际字节：
  61 0D 0A 62 0D 0A
二进制模式实际字节：
  61 0A 62 0A
文本模式 "r" 打开，fseek 到末尾后 ftell = 6
二进制模式 "rb" 打开，fseek 到末尾后 ftell = 6

Linux 侧 gcc 13.3（glibc）：
文本模式 "w"  磁盘上 4 字节
二进制模式 "wb" 磁盘上 4 字节
文本模式实际字节：
  61 0A 62 0A
二进制模式实际字节：
  61 0A 62 0A
文本模式 "r" 打开，fseek 到末尾后 ftell = 4
二进制模式 "rb" 打开，fseek 到末尾后 ftell = 4
```

**这就是第 1.6 小节那个「12 字节而不是 11」的来源**：最后一次 `printf` 的 `\n`
在文本模式下变成了 `\r\n`。**在 Linux 上写同样的代码，字节数会与 Windows 不同。**

> [!IMPORTANT]
> **要写跨平台交换的数据文件，就在模式串里加 `b`。**
> 加 `b` 的那一边不做任何转换，`ftell` 给出的偏移与实际字节一一对应；
> 文本模式下 `ftell` 的返回值是「转换之后的位置」，拿它当字节偏移会算错。
> 只在「这个文件只给本机的记事本看」时，文本模式才是合适的选择。

> [!NOTE]
> **第 4 节小结**：`fopen` 失败返回 `NULL`，`"w"` 会清空；
> `fread`/`fwrite` 返回元素个数，少于请求不是错误；
> `fgetc` 的返回值要用 `int` 接；`fgets` 装不下就把行切开，判断依据是结尾有没有换行；
> `fseek`/`ftell` 负责定位，失败信息只在返回值与 `errno` 里；
> 文本模式在 Windows 上做 `\n` 与 `\r\n` 的转换，跨平台数据文件一律加 `b`。

---

# 第 5 节 出错之后怎么知道

## 5.1 `errno`、`perror`、`strerror`

**标准库里的失败信息有两条通道**：函数的返回值说「失败了」，
`errno` 说「为什么失败」。三个工具配合使用：

| 工具 | 用法 | 输出到哪 |
|---|---|---|
| `errno` | 一个宏，返回左值，可以直接赋值与读取 | 不输出，取值是整数 |
| `perror("前缀")` | 打印 `前缀: 错误描述` | `stderr` |
| `strerror(errno)` | 返回错误描述的字符串 | 由调用者决定怎么用 |

`C`

```c
/* errno_demo.c    编译：gcc -std=c23 errno_demo.c -o errno_demo */
#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    /* 第一种：文件不存在 */
    errno = 0;
    FILE *f = fopen("no_such_file.txt", "r");
    printf("fopen 不存在的文件：返回 %s，errno = %d\n", f ? "非空" : "NULL", errno);
    if (!f) perror("  perror 说的是");
    printf("  strerror 说的是：%s\n", strerror(errno));

    /* 第二种：路径是一个目录 */
    errno = 0;
    f = fopen(".", "r");
    printf("fopen 一个目录：返回 %s，errno = %d（%s）\n",
           f ? "非空" : "NULL", errno, strerror(errno));
    if (f) fclose(f);

    /* 成功时 errno 不会自动清零：上一轮的旧值会留在这里 */
    errno = 0;
    f = fopen("errno_demo_ok.txt", "w");
    printf("fopen 成功之后 errno 还是 %d\n", errno);
    fclose(f);
    remove("errno_demo_ok.txt");
    return 0;
}
```

`实测数据`
`Text`

```text
Windows 侧 gcc 15.2（MinGW-w64）：
fopen 不存在的文件：返回 NULL，errno = 2
  strerror 说的是：No such file or directory
fopen 一个目录：返回 NULL，errno = 13（Permission denied）
fopen 成功之后 errno 还是 0
stderr：  perror 说的是: No such file or directory

Linux 侧 gcc 13.3（glibc）：
fopen 不存在的文件：返回 NULL，errno = 2
  strerror 说的是：No such file or directory
fopen 一个目录：返回 非空，errno = 0（Success）
fopen 成功之后 errno 还是 0
```

**文件不存在这一条两边一致：`errno` 都是 2（`ENOENT`）。**
但「打开一个目录」两边不同：Windows 给 `EACCES`（13），Linux 允许打开目录用于读取。
**错误码的数值一致只是巧合**，标准只规定 `ENOENT`、`EACCES` 这些名字，
数值由实现定；要比较就写 `errno == ENOENT`，不要写 `errno == 2`。

> [!WARNING]
> **`errno` 不会被成功的调用清零。**
> 上面每一处都先写了 `errno = 0;` 再调用——这不是多余的仪式：
> 少了它，上一轮留下的值会被当成本次的结果。
> **判断顺序永远是「先看返回值说失败，再看 `errno` 说原因」**，
> 不能反过来用 `errno` 判断有没有出错。

`perror` 有一个容易忽略的性质：**它把内容写到 `stderr`，不是 `stdout`**。
上面那段输出里，`perror` 的那一行出现在 `stderr` 分组里，
在重定向与管道里这两个流会分开走（第 1.4 小节）。

## 5.2 `feof` 与 `ferror`：把「结束」和「出错」分开

**读函数把两种完全不同的结局合并成了同一个返回值**：`fread` 返回「不足数」、
`fgets` 返回 `NULL`，都可能是「读完了」，也可能是「出错了」。
分开它们的是两个状态标志：

| 标志 | 含义 | 什么时候为真 |
|---|---|---|
| `feof(f)` | 上次读**撞到了文件末尾** | 读操作确实尝试越过末尾之后 |
| `ferror(f)` | 流上发生过**读写错误** | 出错之后一直为真，直到 `clearerr` |
| `clearerr(f)` | 把两个标志都清掉 | — |

`C`

```c
/* feof_ferror.c    编译：gcc -std=c23 feof_ferror.c -o feof_ferror
 * 三种结束方式：正常读完、读出错、还没读就问 feof。
 */
#include <errno.h>
#include <stdio.h>

int main(void) {
    const char *path = "feof_demo.txt";
    FILE *f = fopen(path, "w");
    fputs("0123456789", f);
    fclose(f);

    f = fopen(path, "rb");
    char buf[4];

    printf("刚打开、还没读的时候：feof = %d，ferror = %d\n",
           feof(f) != 0, ferror(f) != 0);

    /* 一直读到 fread 返回 0 为止：这才是判断结束的正确时机 */
    size_t got;
    while ((got = fread(buf, 1, sizeof buf, f)) > 0) {
        printf("读到 %zu 字节，此时 feof = %d\n", got, feof(f) != 0);
    }
    printf("读完最后一轮之后：feof = %d，ferror = %d\n", feof(f) != 0, ferror(f) != 0);
    fclose(f);

    /* 写模式的文件拿去读，就是一次真的错误 */
    f = fopen(path, "w");
    errno = 0;
    got = fread(buf, 1, 1, f);
    printf("用 \"w\" 打开之后 fread：返回 %zu，ferror = %d，errno = %d\n",
           got, ferror(f) != 0, errno);
    fclose(f);
    remove(path);
    return 0;
}
```

`实测数据`
`Text`

```text
Windows 侧 gcc 15.2（MinGW-w64）：
刚打开、还没读的时候：feof = 0，ferror = 0
读到 4 字节，此时 feof = 0
读到 4 字节，此时 feof = 0
读到 2 字节，此时 feof = 1
读完最后一轮之后：feof = 1，ferror = 0
用 "w" 打开之后 fread：返回 0，ferror = 1，errno = 0

Linux 侧 gcc 13.3（glibc）：
刚打开、还没读的时候：feof = 0，ferror = 0
读到 4 字节，此时 feof = 0
读到 4 字节，此时 feof = 0
读到 2 字节，此时 feof = 1
读完最后一轮之后：feof = 1，ferror = 0
用 "w" 打开之后 fread：返回 0，ferror = 1，errno = 9
```

**三处关键读数**：

第一，**刚打开时 `feof` 是 0**。它表示「上次读撞到了末尾」，不是「文件已经到头了」。
所以 `while (!feof(f)) { fread(...); }` 这种写法一定会多循环一次、
把上一次的旧数据再处理一遍——**正确的顺序是先读，再看返回值为不为 0**，
`feof` 只在读完之后用来区分原因。上面那个循环就是这么写的。

第二，最后一次 `fread` 只读到 2 个字节。**元素个数不足时 `feof` 已经为 1**，
说明「不足」的原因是文件读完了，而不是出错。

第三，往只读流上写的那一次，**`ferror` 为 1，`errno` 两边不同**：
Windows 是 0，Linux 是 9（`EBADF`，坏的文件描述符）。
**要判断「出错了没有」就只看 `ferror`，不要依赖 `errno` 有没有被设置。**

## 5.3 缓冲会把错误推迟

**带缓冲的流在写的时候先把内容放进内存**，等缓冲区满了或者冲刷的时候才真正写到设备上。
于是「这一句写成功了吗」这个问题的答案，可能来得比这一句晚得多。

`C`

```c
/* error_late.c    编译：gcc -std=c23 -Wall -Wextra error_late.c -o error_late
 * 往一个只读打开的流里写：错误什么时候才暴露出来。
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "ro.txt";
    FILE *w = fopen(path, "w");
    fputs("data", w);
    fclose(w);

    /* 一、带缓冲的 fprintf：先记进内存，看起来一切正常 */
    FILE *f = fopen(path, "r");
    errno = 0;
    int rc = fprintf(f, "写入 %d 个字符\n", 12345);
    printf("带缓冲的 fprintf 返回 %d，errno = %d，ferror = %d\n",
           rc, errno, ferror(f) != 0);

    fflush(f);                       /* 冲刷时才真的去写 */
    printf("冲刷之后：ferror = %d，errno = %d（%s）\n",
           ferror(f) != 0, errno, strerror(errno));
    fclose(f);

    /* 二、关掉缓冲再来一次：错误当场返回 */
    f = fopen(path, "r");
    setvbuf(f, NULL, _IONBF, 0);
    errno = 0;
    rc = fputc('X', f);
    printf("无缓冲的 fputc 返回 %d，ferror = %d，errno = %d（%s）\n",
           rc, ferror(f) != 0, errno, strerror(errno));
    fclose(f);

    remove(path);
    return 0;
}
```

`实测数据`
`Text`

```text
Windows 侧 gcc 15.2（MinGW-w64）：
带缓冲的 fprintf 返回 23，errno = 9，ferror = 1
冲刷之后：ferror = 1，errno = 9（Bad file descriptor）
无缓冲的 fputc 返回 -1，ferror = 1，errno = 9（Bad file descriptor）

Linux 侧 gcc 13.3（glibc）：
带缓冲的 fprintf 返回 -1，errno = 9，ferror = 1
冲刷之后：ferror = 1，errno = 9（Bad file descriptor）
无缓冲的 fputc 返回 -1，ferror = 1，errno = 9（Bad file descriptor）
```

**同一段代码，Windows 的 `fprintf` 返回 23**——一个正数，
与它成功时返回的字符数长得一模一样；**Linux 则直接给 −1**。
两边的 `ferror` 都是 1、`errno` 都是 9，**只有 `ferror` 在两个平台上给出同一个答案**。

> [!IMPORTANT]
> **这就是第 2.4 小节说的「出错时返回值不可靠」的完整含义**：
> 带缓冲的写函数里，返回值只说明「内容进了缓冲区」，
> 不代表「内容到了文件里」。真正可靠的判据是 `ferror`，
> 以及最后 `fclose` 的返回值——**那一次冲刷才是最后一次真正写盘的机会**。

**第 3.5 小节末尾留下的那半个问题也在这里收口**：
`fgets` 返回 `NULL` 有两种含义——读到了文件末尾，或者读的过程中出错了；
`fread` 返回不足数同样是这两种可能。**先用返回值判断「有没有读到东西」，
再用 `feof` 与 `ferror` 判断「为什么没读到」**，三步的顺序不能换。

`实测数据`
`Text`

```text
正确的三步顺序：

    size_t got = fread(buf, 1, sizeof buf, f);
    if (got > 0)              { /* 有数据，正常处理 */ }
    else if (ferror(f))       { perror("读文件失败"); }   /* 出错 */
    else                      { /* feof 为真：正常读完 */ }
```

> [!NOTE]
> **第 5 节小结**：`errno` 说原因，用之前先清零，成功不会清它；
> `perror` 写到 `stderr`；错误码要用 `ENOENT` 这样的名字比较，不要比数值；
> `feof` 表示「上次读撞到末尾」，不能拿它当循环条件；
> `ferror` 是「出错了没有」在两个平台上唯一一致的答案；
> 带缓冲的写函数返回值可能看着像成功，`fclose` 的返回值是最后一道关。

---

# 第 6 节 速查表

## 6.1 常用件一览

| 名字 | 一句话用途 | 典型坑 |
|---|---|---|
| `fopen` | 打开文件得到流 | 失败返回 `NULL`；`"w"` 会清空文件 |
| `fclose` | 冲刷并释放流 | 忘记或重复关闭；返回值不看 |
| `fread` / `fwrite` | 按块读写 | 返回值是元素个数，不是字节数 |
| `fgetc` / `fputc` | 读写一个字节 | 返回值要用 `int` 接，否则 `EOF` 判断失效 |
| `fgets` / `fputs` | 读写一行 / 一个字符串 | 装不下就切开，判断依据是结尾有没有换行 |
| `fseek` / `ftell` / `rewind` | 定位与问位置 | 失败信息只在返回值与 `errno`；管道上不可定位 |
| `fflush` | 把缓冲区冲刷出去 | 只对输出流有意义；不能替代 `fclose` |
| `setvbuf` | 设置缓冲模式 | 必须在任何读写之前调用 |
| `feof` | 上次读撞到末尾了 | 不能当循环条件，会多跑一轮 |
| `ferror` | 流上出过错 | 出错后一直为真，除非 `clearerr` |
| `clearerr` | 清掉两个标志 | 清理之后才能继续用这条流 |
| `perror` | 打印「前缀: 错误描述」 | 写到 `stderr`，不是 `stdout` |
| `strerror` | 取错误描述字符串 | 返回的字符串不要改；不保证线程安全 |
| `errno` | 上一次失败的原因码 | 成功不清零，用前先置 0 |
| `remove` / `rename` | 删除 / 改名 | 失败同样只看返回值与 `errno` |
| `tmpfile` / `tmpnam` | 临时文件 | `tmpnam` 有竞态，不要用于安全相关的场合 |

## 6.2 文本模式与二进制模式怎么选

| 场合 | 模式 | 理由 |
|---|---|---|
| 只给本机的人看、只在本机读 | `"r"` / `"w"` | 与记事本等工具的换行约定一致 |
| 跨平台交换的数据文件 | **`"rb"` / `"wb"`** | 字节数可预期，`ftell` 的偏移与实际一致 |
| 二进制结构体、图片、压缩包 | **`"rb"` / `"wb"`** | 任何字节转换都会破坏内容 |
| 需要「没有就新建、有就保留」 | 先 `"r"`，失败再 `"w"` | 标准没有直接对应的模式 |

## 6.3 配套件与相关章节

配套示例见 [`B-examples/07-standard-library/01-c-stdlib-toolbox/`](../B-examples/07-standard-library/01-c-stdlib-toolbox/)，
配套练习见 [`C-templates/07-standard-library/01-c-stdlib-toolbox/`](../C-templates/07-standard-library/01-c-stdlib-toolbox/)。
这一个示例把本章的流与缓冲、格式化输出、文件的打开与逐行读取串在一起，
再配合《07-标准库/A-02-字符串与内存：string.h.md》第 3 节的切分写成一份报表。
C++ 侧的对应件是 [`B-examples/07-standard-library/02-cpp-io-report/`](../B-examples/07-standard-library/02-cpp-io-report/)。

| 相关章节 | 关系 |
|---|---|
| 《07-标准库/A-00-导读：C 标准库.md》第 3 节 | **前置**：怎么查一个库函数 |
| 《04-语法/10-字符串.md》第 1 节 | **前置**：`char[]` 与结尾的 0 |
| 《07-标准库/A-02-字符串与内存：string.h.md》第 3 节 | **配套**：把读进来的一行切开 |
| 《07-标准库/A-02-字符串与内存：string.h.md》第 4.3 小节 | **配套**：`%s` 越界的 ASan 报告 |
| 《07-标准库/A-05-工具与其它：stdlib 与杂项.md》第 7 节 | **配套**：`<errno.h>` 的完整清单 |
| 《07-标准库/B-01-输入输出：iostream.md》 | **对照**：`<<` 与流状态 |
| 《07-标准库/B-07-文件系统：filesystem.md》 | **对照**：路径、目录与文件属性的现代写法 |
