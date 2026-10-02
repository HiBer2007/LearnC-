# RAII 与资源管理

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**用完必须还回去的东西，叫作资源。**

堆上的内存、文件句柄、互斥量、socket、数据库连接，形状都是同一套：
**获取 → 使用 → 释放**。前两段不容易写错，出问题的几乎总是第三段——
**释放必须发生在所有路径上，而且只发生一次。**

《05-类与面向对象/04-构造与析构.md》第 7 节给过一个二十行的 `Buffer`：
构造里申请，析构里释放，调用方看不到 `malloc`，也看不到 `free`。
那个雏形展开成一套方法，它有一个固定的名字：**RAII**。

**它不是新语法。** 用到的还是构造函数、析构函数、`= delete` 那几样东西，
换掉的只是「谁负责释放」这个问题的答案：
**从「写代码的人记住释放」换成「析构函数负责释放」。**

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整源码与复现命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 问题 | 在哪一节 |
|---|---|
| 什么算资源，手工配对会在哪几个地方失效 | **一** |
| RAII 的机制、编译器保证了什么、代价是多少 | **二** |
| 抛出异常之后，资源、对象、赋值运算各自处在什么状态 | **三** |
| 自己写一个 RAII 包装（文件句柄） | **四** |
| `unique_ptr` 与 `shared_ptr` 是什么、为什么，开销多少 | **五** |
| 写代码时逐条对照的检查清单 | **六** |

---

# 第 1 节 什么算资源

## 1.1 判断标准：有没有「还回去」这个动作

**判断一个东西是不是资源，看它有没有一个必须做的反向动作。**

| 资源 | 获取 | 释放 |
|---|---|---|
| 堆内存 | `new` / `malloc` | `delete` / `free` |
| 文件 | `std::fopen` | `std::fclose` |
| 互斥量 | 加锁 | 解锁 |
| socket | `socket` / `accept` | 关闭 |
| 动态库 | 加载 | 卸载 |
| 数据库连接 | 从连接池取出 | 归还连接池 |

**这些释放动作有一个共同点：它们不通过返回值报告自己没做。**
`free` 的返回类型是 `void`；`fclose` 会返回错误码，但那个错误码说明的是「关的时候出了问题」，
不是「没有关」。**没有一条错误返回能报告「忘记释放」这件事**，
所以这类错误既不会被编译器发现，也不会在运行期报错——
它只是让程序慢慢占住越来越多的东西。

释放要满足的条件有两条，缺一不可：**所有路径上都发生**，**恰好一次**。
少一次是泄漏，多一次是重复释放（第 3.4 小节的那个例子）。

## 1.2 手工配对：写出释放语句，不等于它一定会执行

C 里没有析构函数，释放只能写成一条语句，放在函数的某个位置。

`C`

```c
char *a = open_thing("A");
char *b = open_thing("B");
if (n < 0) {
    printf("  参数不合法，提前返回\n");
    return;                     /* 下面两行被跳过去了 */
}
close_thing(b, "B");
close_thing(a, "A");
```

**释放语句写了两条，一条不少。** 问题出在 `return` 上：它一走，
后面那两条 `close_thing` 就永远不会执行。

把同一段逻辑按三种结局各跑一遍，获取与释放各记一次数：

`实测数据`

| 结局 | 获取 | 释放 |
|---|---|---|
| 一直跑到函数末尾 | A、B | B、A |
| 中途判定失败，提前返回 | A、B | **一次也没有** |
| 第二个资源没拿到 | A | **一次也没有** |
| 合计 | **5 次** | **2 次** |

**净泄漏 3 次。** 完整程序与复现命令见附录 A.1。

## 1.3 失效点不在记忆，在路径数量

**「记得释放」这句话是误导。** 上面那段代码的作者确实记得——
释放语句就写在那儿。漏掉的原因是**控制流没有经过它**。

| 出口 | 需要维护的配对 |
|---|---|
| 正常走到函数末尾 | 一份 |
| 每个提前 `return` | 各一份 |
| 每个跳出循环或跳转 | 各一份 |
| 每次资源获取失败 | 各一份 |

**每多一条出口，就多一处要人维护的配对**，而编译器不检查有没有漏。

C 里常见的补救是把释放集中到函数末尾一处，用 `goto` 跳过去
（《04-语法/06-控制流语句.md》第 4.2 小节）。单层函数里有效，
但配对仍然要人维护：多一个资源，就多一个标签和一段判断。

把释放语句换个位置放，情况就不同了。

`C++`

```cpp
void path_early_return(bool bad) {
    Thing a("A");
    Thing b("B");
    if (bad) {
        std::printf("  参数不合法，提前返回\n");
        return;                                /* 两条析构照样执行 */
    }
}
```

**这一段里没有任何一条释放语句。** 同样的三条路径再跑一遍：

`实测数据`

| 结局 | 获取 | 释放 |
|---|---|---|
| 一直跑到函数末尾 | A、B | B、A |
| 中途判定失败，提前返回 | A、B | **B、A** |
| 第二个资源没拿到 | A | **A** |
| 合计 | **5 次** | **5 次** |

**净泄漏 0 次**，释放顺序与获取顺序相反。完整程序见附录 A.2。

> [!IMPORTANT]
> **释放语句放进析构函数之后，它就不再依赖控制流走哪条路。**
> 提前返回、`break`、抛出异常，都只是「离开作用域」，
> 而离开作用域必然析构。

---

# 第 2 节 RAII：把释放写进析构函数

## 2.1 机制

**构造里获取，析构里释放。**

名字取自英文缩写：Resource Acquisition Is Initialization，资源获取即初始化。
**这个名字并不直观**：它强调的是「获取发生在初始化（也就是构造）里」，
释放那一半靠的是析构函数。

一个 RAII 包装就是一个类，它把一份资源的三种状态对应到三个位置：

| 资源的状态 | 对应到 |
|---|---|
| 还没拿到 | 对象构造之前 |
| 已经拿到 | 对象的生存期内 |
| 已经还回去 | 对象析构之后 |

配套的三条规定：

| 规定 | 理由 |
|---|---|
| 在构造函数里获取，获取失败就抛异常 | 构造函数没有返回值可以报告失败（《05-类与面向对象/04-构造与析构.md》第 1.5 小节） |
| 在析构函数里释放 | 析构是唯一保证会被执行的位置 |
| 析构函数里不做会失败的事 | 析构中抛出异常会直接终止程序（第 3.3 小节） |

## 2.2 编译器保证的三件事

| 保证 | 依据 |
|---|---|
| 离开作用域一定调用析构函数 | 《04-语法/11-作用域、生存期与链接.md》第 3 节 |
| 每个对象的析构恰好调用一次 | 对象只有一个，析构也只有一个入口 |
| 析构顺序与构造顺序相反 | 《05-类与面向对象/04-构造与析构.md》第 3 节 |

`文档`

> "On exit from a scope (however accomplished), objects with automatic storage duration (6.7.3)
> that have been constructed in that scope are destroyed in the reverse order of their
> construction."
>
> —— N4659 §9.6/2

第一条包含异常路径：栈展开会逐层析构已经构造好的对象
（《04-语法/13-异常.md》第 2 节）。**这一条是 RAII 能覆盖异常路径的全部原因**，
不需要为异常额外写任何代码。

把同一件事写成两种写法对照：

`C++`

```cpp
void with_raw_pointer(int mode) {
    char *p = new char[64];     /* 裸指针：没有任何东西负责释放它 */
    may_throw(mode);            /* 抛出去之后，下面这行永远不会执行 */
    delete[] p;
}

void with_raii(int mode) {
    Buffer b(64);               /* 释放写在 Buffer 的析构函数里 */
    may_throw(mode);            /* 即使在这里抛出，b 的析构也会执行 */
}
```

**在 Windows 侧这个程序看不出差别**：两条路径都只打印一行「捕获：中途失败」。
差别要交给 AddressSanitizer——本机 MinGW 没有它，泄漏检查在 Linux 侧做
（g++ 13.3.0）。

`实测数据`
`Text`

```text
ERROR: LeakSanitizer: detected memory leaks
Direct leak of 64 byte(s) in 1 object(s) allocated from:
    #0 ... in operator new[](unsigned long)
    #1 ... in with_raw_pointer(int)  raii_throw.cpp:24
SUMMARY: AddressSanitizer: 64 byte(s) leaked in 1 allocation(s)
```

报告里只有裸指针那个函数。**RAII 版本一个字节也没有漏。**
完整程序与两侧的复现命令见附录 A.3。

## 2.3 代价：几十个字节的异常出口

RAII 不是语法糖，编译出来的东西有实际差别。把「手工 `new` / `delete`」
与「`unique_ptr`」写成两个函数，都用 `-O2` 编出来看：

`实测数据`

| 写法 | MinGW g++ 15.2.0 | Linux g++ 13.3.0 |
|---|---|---|
| 手工 `new` / `delete` | 64 字节 | 46 字节 |
| `unique_ptr` | **80 字节** | 68 字节（另有 21 字节的 cold 段） |

`-O2` 下两个函数的字节数，`nm -S` 所得。下面是 `unique_ptr` 那一版的汇编：

`实测数据`
`Assembly`

```asm
# MinGW g++ 15.2.0，-O2。前 16 行是正常路径，最后 5 行只有抛出时才会执行
_Z12raii_versionv:
    pushq   %rsi                     # 手工版本没有这一条
    pushq   %rbx
    subq    $40, %rsp
    movl    $4, %ecx
    call    _Znwy                    # operator new
    movl    $7, (%rax)
    movq    %rax, %rcx
    movq    %rax, %rbx
    call    consume
    movl    $4, %edx
    movq    %rbx, %rcx
    addq    $40, %rsp
    popq    %rbx
    popq    %rsi
    jmp     _ZdlPvy                  # operator delete；手工版本末尾同样是这一条
    movq    %rax, %rsi               # 以下五行是异常出口：先 delete，再把异常递出去
    movq    %rbx, %rcx
    movl    $4, %edx
    call    _ZdlPvy
    movq    %rsi, %rcx
    call    _Unwind_Resume
```

两件事从这里可以看清：

| 问题 | 答案 |
|---|---|
| 正常路径上多做了什么 | 只多保存与恢复一个寄存器（`pushq %rsi` / `popq %rsi`） |
| 多出来的 16 字节是什么 | 函数末尾的异常出口，只在栈展开时执行 |

**正常路径上并没有多出一次释放动作。** 手工版本在函数末尾调用一次 `delete`，
RAII 版本在析构函数里调用一次 `delete`，两者编出来的指令几乎逐条相同。

> [!IMPORTANT]
> **RAII 不做额外的事，它只是把同一件事换了个位置写。**
> 它付出的代价落在**代码体积**上——每个用到 RAII 的函数多出一个异常出口；
> 不落在每次执行的时间上。这与异常本身的代价是同一回事
> （《04-语法/13-异常.md》第 5.1 小节）。

## 2.4 三处容易写错的地方

| 写法 | 后果 | 在哪一节 |
|---|---|---|
| 构造函数里用裸指针拿多个资源 | 后面那个失败时，前面那个泄漏 | 第 3.2 小节 |
| 析构函数里抛出异常 | 程序直接终止 | 第 3.3 小节 |
| 默认生成的拷贝构造被留着 | 同一份资源被释放两次 | 第 4.2 小节 |

---

# 第 3 节 异常安全

## 3.1 三条要求

**异常安全不是「保证不抛异常」**，而是「抛出之后，程序仍然处在可解释的状态」。
具体有三条：

| 要求 | 谁负责 |
|---|---|
| 已经拿到的资源不泄漏 | RAII 成员与 RAII 局部对象（第 2.2 小节） |
| 每份资源不重复释放 | 拷贝禁掉、移动把源对象置空（第 4.2、4.3 小节） |
| 对象仍然处于合法状态 | 修改对象的那些操作，尤其是赋值运算符（第 3.4 小节） |

前两条由上面讲过的机制保证，第三条要靠写赋值运算符的人。

## 3.2 构造函数抛异常：已经拿到的资源谁管

构造函数中途抛出异常时，析构函数**不会**被调用——对象从来没有构造完成，
谈不上「销毁一个完整的对象」。那么已经拿到的资源怎么办？先看谁被析构：

`实测数据`
`Text`

```text
构造成功：
  成员 a 构造
  成员 b 构造
  Owner 的构造函数体开始执行
  Owner 构造完成
  Owner 析构
  成员 b 析构
  成员 a 析构
构造失败：
  成员 a 构造
  成员 b 构造
  Owner 的构造函数体开始执行
  成员 b 析构
  成员 a 析构
  捕获：Owner 构造中途失败
```

两边对照，差别只有一处：**失败那一次没有「Owner 析构」这一行**。
已经构造完成的成员会被逆序析构（`b` 先于 `a`），而这个对象自己的析构函数不会被调用。
这条规则在《04-语法/13-异常.md》第 2.2 小节已经出现过，这里要接的是它的后果。

**把资源放进成员，交给成员自己的析构函数管**，那就不存在问题；
**把资源拿在构造函数体里用裸指针管**，拿到一半失败，前面的就泄漏了：

`C++`

```cpp
class Manual {                              /* 两个资源都用裸指针拿着 */
public:
    Manual(bool fail) : a_(new char[64]) {  /* 第一个在初始化列表里拿到 */
        b_ = new char[64];                  /* 第二个在函数体里拿到 */
        if (fail) throw std::runtime_error("拿到 b 之后失败了");
    }
    ~Manual() { delete[] a_; delete[] b_; } /* 这个析构函数不会被调用 */
private:
    char *a_;
    char *b_;
};

class Owner {                               /* 两个资源都交给智能指针 */
public:
    Owner(bool fail) : a_(new char[64]), b_(new char[64]) {
        if (fail) throw std::runtime_error("拿到 b 之后失败了");
    }
private:
    std::unique_ptr<char[]> a_;             /* 成员被逆序析构，两块内存都还回去 */
    std::unique_ptr<char[]> b_;
};
```

`实测数据`
`Text`

```text
ERROR: LeakSanitizer: detected memory leaks
Direct leak of 64 byte(s) in 1 object(s) allocated from:
    #1 ... in Manual::Manual(bool)  ctor_leak.cpp:8
Direct leak of 64 byte(s) in 1 object(s) allocated from:
    #1 ... in Manual::Manual(bool)  ctor_leak.cpp:10
SUMMARY: AddressSanitizer: 128 byte(s) leaked in 2 allocation(s)
```

**报告里只有 `Manual`，没有 `Owner`。** 同一个构造函数失败点，
手工管理的漏了两块内存，交给 RAII 的一块也没漏。完整程序见附录 A.5。

> [!CAUTION]
> **构造函数里不要用裸指针管理多个资源。**
> 第二个资源获取失败时，第一个资源没有任何人负责释放——
> 这条路径在代码里看不见，只有把每个资源都写成成员、让成员自己管，
> 才会被栈展开自动覆盖。

## 3.3 析构函数里不要抛异常

析构函数在两种情况下被调用：正常离开作用域，以及栈展开期间。
**第二种情况决定了它不能抛**：同一时刻只能有一个异常在路上。

`C++`

```cpp
class Bad {
public:
    ~Bad() {
        throw std::runtime_error("析构函数里抛出的异常");   /* 不该这么写 */
    }
};
```

编译器直接给出警告，运行结果是终止：

`实测数据`
`Text`

```text
dtor_throw.cpp:8:9: warning: 'throw' will always call 'terminate' [-Wterminate]

terminate called after throwing an instance of 'std::runtime_error'
  what():  析构函数里抛出的异常
```

退出码 3。`catch` 块接不住它——**`terminate` 在异常能被捕获之前就被调用了**。

写下 `noexcept(false)` 可以取消这个限制，于是能看出真正的分界线在哪里：

`实测数据`

| 情形 | 结果 |
|---|---|
| 析构函数里抛出，栈上没有别的异常 | 能被 `catch` 捕获，程序正常结束 |
| 析构函数里抛出，此时正在栈展开 | **`terminate`**，退出码 3 |

**规矩只有一条：析构函数里只做不会失败的事。**
确实需要报告失败的场合（比如关闭文件时写入失败），把错误记录下来，
不要让异常离开析构函数。

## 3.4 先做会失败的，再做不会失败的

赋值运算符要把「自己的旧内容」换成「别人的内容」，中间必然有一步会失败：分配新内存。
这一步放在前面还是后面，结果完全不同。

`C++`

```cpp
    /* 错的顺序：先释放，再去分配 */
    void assign_wrong(const Buffer &other) {
        delete[] data_;                             /* 先把自己清空 */
        data_ = alloc_or_throw(other.size_);        /* 这一步失败，data_ 成了悬垂指针 */
        std::memcpy(data_, other.data_, other.size_);
    }

    /* 对的顺序：先做会失败的事，成功了再释放 */
    void assign_right(const Buffer &other) {
        char *fresh = alloc_or_throw(other.size_);  /* 会抛的一步放前面 */
        std::memcpy(fresh, other.data_, other.size_);
        delete[] data_;                             /* 不会抛的一步放后面 */
        data_ = fresh;
    }
```

`实测数据`
`Text`

```text
对的顺序：分配失败，自己保持原样
  分配失败，a.size() 仍然是 8
错的顺序：分配失败，自己已经残了
  分配失败，a.size() 还是 8，但它管的那块内存已经还回去了
```

**尺寸对不上，是因为 `size_` 还没来得及改**：对象看着完好，
手里的指针已经指向一块被释放的内存。程序继续跑到 `a` 析构时，同一块内存被删第二次：

`实测数据`
`Text`

```text
ERROR: AddressSanitizer: attempting double-free on 0x502000000050 in thread T0:
    #1 ... in Buffer::~Buffer()  assign_order.cpp:16
SUMMARY: AddressSanitizer: double-free ... in operator delete[](void*)
```

Windows 侧没有 AddressSanitizer，这个程序以 `0xC0000374`（堆损坏）终止，
连缓冲区里的输出都没来得及刷出来。完整程序见附录 A.8。

> [!IMPORTANT]
> **顺序原则：会失败的一步放前面，不会失败的一步放后面。**
> 分配可能失败，释放与交换不会失败——所以先分配、再释放、最后交换。
> 这条原则与第 05 章深拷贝那份代码里的顺序是同一条
> （《05-类与面向对象/05-拷贝与移动.md》第 3 节）。

## 3.5 三种保证

库的作者通常按三个档次描述一个函数：「基本保证」「强保证」「不抛保证」。

| 保证 | 抛出之后的状态 | 怎么做到 |
|---|---|---|
| **基本保证** | 不泄漏、对象合法，但内容可能是改了一半的 | 资源都交给 RAII 成员 |
| **强保证** | 要么成功，要么完全不变 | 先做一份副本，成功了再换过去 |
| **不抛保证** | 承诺不抛异常 | 析构、`swap`、移动操作（写 `noexcept`） |

强保证有一种固定写法，叫先拷贝再交换：

`C++`

```cpp
    /* 片段：把「先拷贝再交换」写成赋值运算符 */
    Buffer &operator=(Buffer other) {   /* 按值传参：拷贝在这一步完成，失败就在这里抛出 */
        swap(other);                    /* 交换不会失败 */
        return *this;
    }                                   /* other 在这里析构，把旧内容释放掉 */
```

按值传参收下了副本，函数体里只剩「交换」这一件不会失败的事；
拷贝失败时函数还没进入函数体，对象保持原样，强保证自然成立。

**本节的三条要求落到代码里，就是把一份资源写成一个类。** 下一节用文件句柄走一遍：
构造获取、析构释放、拷贝禁掉、移动把源对象置空——四件事分别对应上表的三条要求。

---

# 第 4 节 自己写一个 RAII 包装

## 4.1 从 `fopen` / `fclose` 到一个类

文件句柄是最适合练手的一份资源：它必须关，关两次会出错，忘了关会占住文件。

`C++`

```cpp
/* 文件句柄的 RAII 包装（类的部分，完整的可运行程序见附录 A.11） */
#include <cstdio>
#include <stdexcept>
#include <utility>

class File {
public:
    explicit File(const char *path) : fp_(std::fopen(path, "w")), path_(path) {
        if (!fp_) throw std::runtime_error("打不开文件");   /* 构造失败就抛出去 */
        std::printf("  打开 %s\n", path_);
    }
    ~File() { close_now(); }                 /* 无论怎么离开作用域都会执行 */

    File(const File &) = delete;             /* 同一个句柄不能关两次 */
    File &operator=(const File &) = delete;

    File(File &&other) noexcept : fp_(other.fp_), path_(other.path_) { other.fp_ = nullptr; }
    File &operator=(File &&other) noexcept {
        if (this != &other) {
            close_now();
            fp_ = other.fp_;
            path_ = other.path_;
            other.fp_ = nullptr;             /* 交出去之后，原来的对象不再持有句柄 */
        }
        return *this;
    }

    void write(const char *s) { if (fp_) std::fputs(s, fp_); }

private:
    void close_now() {
        if (fp_) {
            std::fclose(fp_);
            std::printf("  关闭 %s\n", path_);
            fp_ = nullptr;
        }
    }

    std::FILE  *fp_;
    const char *path_;
};
```

把闭包里的三件事逐条对上看：

| 代码里的位置 | 作用 |
|---|---|
| 构造函数的初始化列表 | 获取资源；拿不到就抛 |
| `~File` | 释放资源 |
| `close_now` 里的 `if (fp_)` | 保证「恰好一次」：句柄交出去之后，原对象手里的 `fp_` 是空的 |

## 4.2 拷贝必须禁掉

**默认生成的拷贝构造会把 `fp_` 照抄一份**，于是两个 `File` 对象拿着同一个文件句柄，
各自在析构里 `fclose` 一次。这与《05-类与面向对象/05-拷贝与移动.md》第 2 节
那个浅拷贝导致的双重释放是同一件事，只是资源从内存换成了文件。

两行 `= delete` 就是全部处理：

`C++`

```cpp
    File(const File &) = delete;
    File &operator=(const File &) = delete;
```

**判据很简单**：这个类管着资源吗。管着，就不要让编译器生成拷贝。
第 05 章第 5 节讲过编译器生成那一对的规律，`= delete` 是关掉它的写法。

## 4.3 允许移动：把句柄交出去

拷贝被禁掉之后，`File` 依然可以放进容器、可以从函数里返回——
靠的是移动构造与移动赋值（《05-类与面向对象/05-拷贝与移动.md》第 4 节）：
**所有权转移，而不是复制一份**。

移动之后，原来的对象不再持有句柄。**这一点必须与析构函数配合**：
`close_now` 里那个 `if (fp_)` 判断的就是这件事。少写这一句，
两个对象都会去关同一个句柄。

> [!TIP]
> **移动构造与移动赋值都写 `noexcept`**：它们只搬指针、不分配内存，
> 本来就没有失败的理由，而标准库容器只在移动操作承诺不抛时才会用它
> （《04-语法/13-异常.md》第 4 节）。

## 4.4 三条路径一起跑

同一段逻辑放在三种结局下，看文件有没有被关掉：

`实测数据`
`Text`

```text
提前返回：
  打开 early.txt
  提前返回
  关闭 early.txt
异常路径：
  打开 throw.txt
  关闭 throw.txt
  捕获：写到一半失败了
把句柄交给另一个对象：
  打开 move.txt
  关闭 move.txt
```

**三种情况下文件都被关掉了，代码里一处 `fclose` 也没有。**
异常那一组里，`关闭 throw.txt` 出现在 `捕获` 之前——
栈展开先清理局部对象，然后才轮到 `catch`（《04-语法/13-异常.md》第 2.1 小节）。
完整程序见附录 A.11。

---

# 第 5 节 智能指针：现成的 RAII 包装

## 5.1 裸指针缺的三样东西

| 缺什么 | 后果 |
|---|---|
| 所有权不写在类型里 | 谁负责 `delete` 只能靠约定，调用方与实现方各猜一半 |
| 拷贝语义不受限制 | 指针可以随便复制，资源却只有一个主人（《05-类与面向对象/05-拷贝与移动.md》第 2 节） |
| 释放语句在正常路径上 | 抛出去之后，那一行不执行（第 2.2 小节） |

**智能指针是别人写好的 RAII 包装**：所有权写在类型里，释放在析构函数里。
本节只讲它们是什么、为什么存在；怎么用、怎么选，归《07-标准库》章节。

## 5.2 `unique_ptr`：独占所有权

一份资源只有一个主人。拷贝被删掉，移动保留——**交接所有权，而不是复制所有权**。

`实测数据`

| 项目 | 结果 |
|---|---|
| `sizeof(std::unique_ptr<T>)` | **8 字节**，与裸指针相同 |
| 异常路径上的释放 | 析构函数照常执行，一个字节也没漏 |

**大小的意义**：默认删除器不占空间，`unique_ptr` 没有比裸指针多出任何存储。
把 `new` 与 `delete` 换成 `unique_ptr`，**不会让对象变大**。

异常路径上的顺序值得留意：

`实测数据`
`Text`

```text
异常路径上，unique_ptr 照样释放：
  Buffer 构造（8 字节）
  Buffer 析构
  捕获：中途失败
```

`Buffer 析构` 出现在 `捕获` **之前**：栈展开先把局部对象清理干净，
控制流随后才进入 `catch`。完整程序见附录 A.10。

`unique_ptr` 还允许换掉释放动作——把「怎么释放」也交给类型来记。
文件句柄这类不是 `delete` 出来的资源，因此也能直接交给它管；
写法见《07-标准库》章节。

## 5.3 `shared_ptr`：共享所有权

生命周期不由某一个使用者决定时，用 `shared_ptr`：
**最后一个持有者消失时，对象才析构**。它靠引用计数实现。

`实测数据`

| 项目 | 结果 |
|---|---|
| `sizeof(std::shared_ptr<T>)` | **16 字节**（对象指针 + 控制块指针） |
| 引用计数的变化 | 从 `1` 到 `2`，再到最后一个引用消失时析构 |

`实测数据`
`Text`

```text
shared_ptr 的引用计数：
  Buffer 构造（8 字节）
  只有 a 指着它：1
  b 也指着它：2
  Buffer 析构
  最后一个引用消失，对象被销毁
```

**16 字节比裸指针大**：多出来的是控制块指针，计数与删除器都放在控制块里。
这份开销换来的是「不需要知道谁会是最后一个使用者」。

## 5.4 引用计数管不了「互相指着」

两个对象互相持有对方的 `shared_ptr`，计数就永远降不到零：

`实测数据`
`Text`

```text
互相指着（循环引用）：
  x 的计数 = 2，y 的计数 = 2
  两个 Node 都没有被析构
```

`实测数据`
`Text`

```text
Indirect leak of 32 byte(s) in 1 object(s) allocated from:
Indirect leak of 32 byte(s) in 1 object(s) allocated from:
SUMMARY: AddressSanitizer: 64 byte(s) leaked in 2 allocation(s)
```

两个对象都不析构，也就是两份内存都收不回来。

> [!CAUTION]
> **用 `shared_ptr` 表达「两个对象互相关联」时，边必须有方向。**
> 表示「我拥有它」的边用 `shared_ptr`，表示「我只是知道它、不决定它的生死」的边
> 用 `std::weak_ptr`——`weak_ptr` 不增加引用计数，因此不会把对象钉住。
> 全部用 `shared_ptr` 就是循环引用，也就是泄漏。

## 5.5 什么时候用哪一个

| 场景 | 选 |
|---|---|
| 一份资源一个主人 | `unique_ptr` |
| 释放动作不是 `delete`（文件、句柄） | `unique_ptr` 加自定义删除器 |
| 生命周期由多个使用者共同决定 | `shared_ptr` |
| 只是观察，不影响生命期 | `weak_ptr`、裸指针或引用 |
| 只在函数调用期间使用，不涉及所有权 | `T &`（《04-语法/03-常量与 const.md》第 5 节） |

**能用引用就不要用指针，能用 `unique_ptr` 就不要用 `shared_ptr`**：
所有权越简单，需要人判断的地方越少。

## 5.6 一个拥有者，多个观察者

**所有权只能有一份，引用它的地方却往往不止一处。**
`unique_ptr` 不允许拷贝，因此「按另一种顺序再存一份」只能存**观察用的裸指针**。

以下引自 `nbtcpp` 的 `NbtCompound`（`src/tags/nbt_compound.cpp` 第 54 至 83 行，
**下面是节选**，原文副本在 `A-教学素材/05-类与面向对象/指针与所有权/`）：

`C++`

```cpp
void NbtCompound::insert(size_t index, NbtTagPtr tag) {
    if (!tag) throw std::invalid_argument("Cannot insert null tag into NbtCompound");
    if (tag->name().empty())
        throw std::invalid_argument("Tags added to NbtCompound must have a non-empty name");
    if (tag->parent())
        throw std::invalid_argument("Tag already has a parent");

    const auto& name = tag->name();
    if (tags_.find(name) != tags_.end())
        throw std::invalid_argument("Duplicate tag name in NbtCompound: " + name);

    adopt(tag.get());
    tags_[name] = std::move(tag);
    order_.insert(order_.begin() + static_cast<ptrdiff_t>(index), tags_[name].get());
    fire_changed();
}

NbtTagPtr NbtCompound::remove(NbtTag* tag) {
    if (!tag) return nullptr;
    auto it = tags_.find(tag->name());
    if (it == tags_.end() || it->second.get() != tag) return nullptr;

    auto ptr = std::move(it->second);
    tags_.erase(it);
    auto oit = std::find(order_.begin(), order_.end(), ptr.get());
    if (oit != order_.end()) order_.erase(oit);
    ptr->set_parent(nullptr);
    fire_changed();
    return ptr;
}
```

两个成员的分工写在声明里（`include/nbtcpp/tags/nbt_compound.h` 第 118 至 119 行，**原文**）：

`C++`

```cpp
    std::map<std::string, NbtTagPtr> tags_;
    std::vector<NbtTag*> order_;   // insertion-order tracking
```

**按名字查找要用 `map`，保持插入顺序要用 `vector`**，两者不能互相替代；
而所有权只有一份，因此 `order_` 里只能是裸指针。
`NbtTagPtr` 这个别名本身就把「谁负责释放」回答了（`nbt_tag.h` 第 213 行，**下面是节选**）：

`C++`

```cpp
using NbtTagPtr = std::unique_ptr<NbtTag>;
```

> [!CAUTION]
> **删掉对象时必须两处一起改。** `remove` 里先从 `tags_` 擦除（拥有者），
> 紧接着从 `order_` 中查找并擦除对应项（观察者）。
> 漏掉第二步，`order_` 里就留下悬空指针，要等到下一次遍历它时才崩。

`实测数据`
`C++`

```cpp
// owner_observer.cpp
// 编译（WSL Ubuntu 24.04.5）：g++ -std=c++17 -fsanitize=address -g owner_observer.cpp -o owner_observer
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct Tag {
    std::string name;
    int id;
};

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);            /* 后面会崩，输出立刻落盘 */

    std::map<std::string, std::unique_ptr<Tag>> owned;   /* 拥有，按名字索引 */
    std::vector<Tag *> order;                            /* 观察，按登记顺序 */

    owned["a"] = std::make_unique<Tag>(Tag{"a", 1});
    order.push_back(owned["a"].get());

    owned.erase("a");            /* 只改了拥有者，忘了同步 order */
    Tag *stale = order[0];
    std::printf("order 里还有 %zu 个观察指针\n", order.size());
    std::printf("它指向的对象 id = %d\n", stale->id);    /* 悬空访问 */
    return 0;
}
```

`Bash`

```bash
g++ -std=c++17 -fsanitize=address -g owner_observer.cpp -o owner_observer && ./owner_observer
```

`实测数据`
`Text`

```text
order 里还有 1 个观察指针
=================================================================
==494==ERROR: AddressSanitizer: heap-use-after-free on address 0x504000000030
READ of size 4 at 0x504000000030 thread T0
    #0 in main                owner_observer.cpp:25
0x504000000030 is located 32 bytes inside of 40-byte region
freed by thread T0 here:
    #0 in operator delete(void*, unsigned long)
    #1 in std::default_delete<Tag>::operator()(Tag*) const

SUMMARY: AddressSanitizer: heap-use-after-free
```

报告里的路径与地址列从略，其余照抄运行输出。

**`remove` 的签名也值得留意**：参数是用于查找的裸指针，返回值是 `unique_ptr`，
它把所有权**交还**给调用方，由调用方决定什么时候销毁。
换成 `void remove(NbtTag *)` 直接删掉，调用方就无法「先摘下来，过一会儿再装回去」。

---

# 第 6 节 检查清单

| 检查 | 为什么 |
|---|---|
| 这个类管着资源吗 | 管着就必须自己写析构，并禁掉或自己写拷贝（第 4.2 小节） |
| 析构函数里有没有可能抛出的操作 | 抛出即终止（第 3.3 小节） |
| 构造函数里用裸指针拿了几个资源 | 第二个失败时，前面的全漏（第 3.2 小节） |
| 赋值运算符里，会失败的一步在前面吗 | 顺序反了会留下悬垂指针（第 3.4 小节） |
| 移动构造与移动赋值写了 `noexcept` 吗 | 不写，容器就不会用移动（第 4.3 小节） |
| 业务代码里还有裸的 `new`、`delete`、`fopen` 吗 | 它们应当只出现在 RAII 包装的构造函数里 |
| 每个提前 `return` 上，谁负责释放 | 找不到负责人，就说明还缺一个 RAII 包装 |
| 两个对象互相关联时，边都是 `shared_ptr` 吗 | 循环引用就是泄漏（第 5.4 小节） |

---

# 术语表

| 术语 | 英文 | 含义 |
|---|---|---|
| 资源 | Resource | 用完必须还回去的东西；释放要覆盖所有路径，且恰好一次 |
| RAII | Resource Acquisition Is Initialization | 构造里获取、析构里释放；靠对象的生存期管理资源 |
| RAII 包装 | RAII wrapper | 一个类，构造获取一份资源，析构释放它 |
| 所有权 | Ownership | 谁负责释放这份资源；写在类型里比写在注释里可靠 |
| 异常安全 | Exception safety | 抛出之后不泄漏、不重复释放、对象仍然合法 |
| 基本保证 | Basic guarantee | 不泄漏、对象合法，但内容可能是改了一半的 |
| 强保证 | Strong guarantee | 要么成功，要么完全不变 |
| 不抛保证 | Nothrow guarantee | 承诺不抛异常，写法是 `noexcept` |
| 浅拷贝 | Shallow copy | 只抄成员的值，成员指向的东西不复制（《05-类与面向对象/05-拷贝与移动.md》第 2 节） |
| `unique_ptr` | — | 独占所有权的智能指针；不可拷贝、可移动；与裸指针同样大小 |
| `shared_ptr` | — | 共享所有权的智能指针；最后一份引用消失时析构 |
| 引用计数 | Reference count | `shared_ptr` 记录持有者数量的计数，放在控制块里 |
| 控制块 | Control block | `shared_ptr` 里存放引用计数与删除器的部分 |
| `weak_ptr` | — | 不增加引用计数的观察者指针；用来打破循环引用 |
| 循环引用 | Reference cycle | 两个对象互相持有对方的 `shared_ptr`，计数降不到零 |
| 删除器 | Deleter | 智能指针析构时调用的释放动作，可以自定义 |
| 零法则 | Rule of zero | 资源都交给成员与智能指针管，自己不写析构与拷贝 |

---

# 附录 A 复现本章节实测

## A.1 对照：C 的手工配对

`C`

```c
/* leak_paths.c    编译：gcc -std=c23 leak_paths.c -o leak_paths */
#include <stdio.h>
#include <stdlib.h>

static int g_open = 0;      /* 获取了几次 */
static int g_close = 0;     /* 释放了几次 */

static char *open_thing(const char *name) {
    char *p = malloc(32);
    if (p == NULL) return NULL;
    ++g_open;
    printf("  获取 %s\n", name);
    return p;
}

static void close_thing(char *p, const char *name) {
    if (p == NULL) return;
    ++g_close;
    printf("  释放 %s\n", name);
    free(p);
}

/* 场景一：一直跑到结尾，两条都记得释放 */
static void path_normal(void) {
    char *a = open_thing("A");
    char *b = open_thing("B");
    close_thing(b, "B");
    close_thing(a, "A");
}

/* 场景二：中途判定失败，提前返回 */
static void path_early_return(int n) {
    char *a = open_thing("A");
    char *b = open_thing("B");
    if (n < 0) {
        printf("  参数不合法，提前返回\n");
        return;                     /* 忘了释放 a 与 b */
    }
    close_thing(b, "B");
    close_thing(a, "A");
}

/* 场景三：第二个资源没拿到 */
static void path_second_fails(void) {
    char *a = open_thing("A");
    char *b = NULL;
    if (b == NULL) {
        printf("  第二个资源没拿到，提前返回\n");
        return;                     /* 忘了释放 a */
    }
    close_thing(b, "B");
    close_thing(a, "A");
}

int main(void) {
    printf("场景一：正常返回\n");
    path_normal();
    printf("场景二：中途提前返回\n");
    path_early_return(-1);
    printf("场景三：第二个资源获取失败\n");
    path_second_fails();
    printf("获取 %d 次，释放 %d 次，净泄漏 %d 次\n", g_open, g_close, g_open - g_close);
    return 0;
}
```

`Bash`

```bash
gcc -std=c23 leak_paths.c -o leak_paths
./leak_paths
```

`实测数据`
`Text`

```text
场景一：正常返回
  获取 A
  获取 B
  释放 B
  释放 A
场景二：中途提前返回
  获取 A
  获取 B
  参数不合法，提前返回
场景三：第二个资源获取失败
  获取 A
  第二个资源没拿到，提前返回
获取 5 次，释放 2 次，净泄漏 3 次
```

## A.2 同样的三条路径改成 RAII

`C++`

```cpp
/* raii_paths.cpp    编译：g++ -std=c++17 raii_paths.cpp -o raii_paths */
#include <cstdio>

static int g_open = 0;
static int g_close = 0;

class Thing {
public:
    explicit Thing(const char *name) : name_(name) {
        ++g_open;
        std::printf("  获取 %s\n", name_);
    }
    ~Thing() {
        ++g_close;
        std::printf("  释放 %s\n", name_);
    }
    Thing(const Thing &) = delete;              /* 一份资源不能有两个主人 */
    Thing &operator=(const Thing &) = delete;

private:
    const char *name_;
};

void path_normal() {
    Thing a("A");
    Thing b("B");
}                                              /* 离开作用域：先析构 b，再析构 a */

void path_early_return(bool bad) {
    Thing a("A");
    Thing b("B");
    if (bad) {
        std::printf("  参数不合法，提前返回\n");
        return;                                /* 两条析构照样执行 */
    }
}

void path_second_fails(bool got_second) {
    Thing a("A");
    if (!got_second) {
        std::printf("  第二个资源没拿到，提前返回\n");
        return;
    }
    Thing b("B");
}

int main() {
    std::printf("场景一：正常返回\n");
    path_normal();
    std::printf("场景二：中途提前返回\n");
    path_early_return(true);
    std::printf("场景三：第二个资源获取失败\n");
    path_second_fails(false);
    std::printf("获取 %d 次，释放 %d 次，净泄漏 %d 次\n", g_open, g_close, g_open - g_close);
    return 0;
}
```

`实测数据`
`Text`

```text
场景一：正常返回
  获取 A
  获取 B
  释放 B
  释放 A
场景二：中途提前返回
  获取 A
  获取 B
  参数不合法，提前返回
  释放 B
  释放 A
场景三：第二个资源获取失败
  获取 A
  第二个资源没拿到，提前返回
  释放 A
获取 5 次，释放 5 次，净泄漏 0 次
```

## A.3 异常路径上的两种写法

`C++`

```cpp
/* raii_throw.cpp    编译：g++ -std=c++17 -fsanitize=address -g raii_throw.cpp -o raii_throw */
#include <cstdio>
#include <cstring>
#include <stdexcept>

class Buffer {
public:
    explicit Buffer(int n) : data_(new char[n]), size_(n) { std::memset(data_, 0, n); }
    ~Buffer() { delete[] data_; }
    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;
    char *data() { return data_; }

private:
    char *data_;
    int   size_;
};

void may_throw(int mode) {
    if (mode == 1) throw std::runtime_error("中途失败");
}

void with_raw_pointer(int mode) {
    char *p = new char[64];                 /* 裸指针：没有任何东西负责释放它 */
    std::strcpy(p, "raw");
    may_throw(mode);                        /* 抛出去之后，下面这行永远不会执行 */
    delete[] p;
    std::printf("  raw 版本正常结束\n");
}

void with_raii(int mode) {
    Buffer b(64);                           /* RAII：释放写在析构函数里 */
    std::strcpy(b.data(), "raii");
    may_throw(mode);                        /* 即使在这里抛出，b 的析构也会执行 */
    std::printf("  raii 版本正常结束\n");
}

int main() {
    std::printf("裸指针 + 异常：\n");
    try {
        with_raw_pointer(1);
    } catch (const std::exception &e) {
        std::printf("  捕获：%s\n", e.what());
    }
    std::printf("RAII + 异常：\n");
    try {
        with_raii(1);
    } catch (const std::exception &e) {
        std::printf("  捕获：%s\n", e.what());
    }
    return 0;
}
```

`Bash`

```bash
g++ -std=c++17 raii_throw.cpp -o raii_throw          # Windows 侧：两条路径的打印一模一样
wsl -d Ubuntu -e bash -c "g++ -std=c++17 -fsanitize=address -g raii_throw.cpp -o raii_throw_lin && ./raii_throw_lin"
                                                     # Linux 侧：AddressSanitizer 报出裸指针那一处的泄漏
```

## A.4 构造函数抛异常时谁被析构

`C++`

```cpp
/* ctor_throw.cpp    编译：g++ -std=c++17 ctor_throw.cpp -o ctor_throw */
#include <cstdio>
#include <stdexcept>

class Member {
public:
    explicit Member(const char *name) : name_(name) { std::printf("  成员 %s 构造\n", name_); }
    ~Member() { std::printf("  成员 %s 析构\n", name_); }

private:
    const char *name_;
};

class Owner {
public:
    Owner(bool fail) : a_("a"), b_("b") {
        std::printf("  Owner 的构造函数体开始执行\n");
        if (fail) throw std::runtime_error("Owner 构造中途失败");
        std::printf("  Owner 构造完成\n");
    }
    ~Owner() { std::printf("  Owner 析构\n"); }

private:
    Member a_;
    Member b_;
};

int main() {
    std::printf("构造成功：\n");
    { Owner o(false); }

    std::printf("构造失败：\n");
    try {
        Owner o(true);
    } catch (const std::exception &e) {
        std::printf("  捕获：%s\n", e.what());
    }
    return 0;
}
```

## A.5 构造函数里手工拿第二个资源失败

`C++`

```cpp
/* ctor_leak.cpp    编译：g++ -std=c++17 -fsanitize=address -g ctor_leak.cpp -o ctor_leak */
#include <cstdio>
#include <memory>
#include <stdexcept>

class Manual {                              /* 两个资源都用裸指针拿着 */
public:
    Manual(bool fail) : a_(new char[64]) {  /* 第一个在初始化列表里拿到 */
        std::printf("  Manual：拿到 a\n");
        b_ = new char[64];                  /* 第二个在函数体里拿到 */
        std::printf("  Manual：拿到 b\n");
        if (fail) throw std::runtime_error("拿到 b 之后失败了");
        std::printf("  Manual：构造完成\n");
    }
    ~Manual() {
        delete[] a_;
        delete[] b_;
        std::printf("  Manual 析构\n");
    }
    Manual(const Manual &) = delete;
    Manual &operator=(const Manual &) = delete;

private:
    char *a_;
    char *b_;
};

class Owner {                               /* 两个资源都交给智能指针 */
public:
    Owner(bool fail) : a_(new char[64]), b_(new char[64]) {
        std::printf("  Owner：两个资源都拿到了\n");
        if (fail) throw std::runtime_error("拿到 b 之后失败了");
        std::printf("  Owner：构造完成\n");
    }

private:
    std::unique_ptr<char[]> a_;
    std::unique_ptr<char[]> b_;
};

int main() {
    std::printf("手工管理，构造函数失败：\n");
    try {
        Manual m(true);
    } catch (const std::exception &e) {
        std::printf("  捕获：%s\n", e.what());
    }

    std::printf("交给 RAII，构造函数失败：\n");
    try {
        Owner o(true);
    } catch (const std::exception &e) {
        std::printf("  捕获：%s\n", e.what());
    }
    return 0;
}
```

`Bash`

```bash
g++ -std=c++17 ctor_leak.cpp -o ctor_leak            # Windows 侧：两个版本都只打印到「捕获」
wsl -d Ubuntu -e bash -c "g++ -std=c++17 -fsanitize=address -g ctor_leak.cpp -o ctor_leak_lin && ./ctor_leak_lin"
                                                     # Linux 侧：只有 Manual 出现在泄漏报告里
```

## A.6 析构函数里抛异常

`C++`

```cpp
/* dtor_throw.cpp    编译：g++ -std=c++17 dtor_throw.cpp -o dtor_throw */
#include <cstdio>
#include <stdexcept>

class Bad {
public:
    ~Bad() {
        throw std::runtime_error("析构函数里抛出的异常");   /* 不该这么写 */
    }
};

int main() {
    try {
        Bad b;
    } catch (const std::exception &e) {
        std::printf("捕获：%s\n", e.what());              /* 这一行永远不会执行 */
    }
    std::printf("程序结束\n");
    return 0;
}
```

## A.7 加 `noexcept(false)` 之后

`C++`

```cpp
/* dtor_throw2.cpp    编译：g++ -std=c++17 dtor_throw2.cpp -o dtor_throw2
   用法：dtor_throw2        析构抛出时没有别的异常
         dtor_throw2 unwind 栈展开期间再抛 */
#include <cstdio>
#include <stdexcept>

class Bad {
public:
    ~Bad() noexcept(false) {                     /* 明确声明「我可能会抛」 */
        throw std::runtime_error("析构函数里抛出的异常");
    }
};

void outer() {
    Bad b;
    throw std::runtime_error("外层先抛了一个");    /* 先有一个异常在路上 */
}

int main(int argc, char **argv) {
    try {
        if (argc > 1) {
            outer();
        } else {
            Bad b;
        }
    } catch (const std::exception &e) {
        std::printf("捕获：%s\n", e.what());
    }
    std::printf("程序结束\n");
    return 0;
}
```

`Bash`

```bash
g++ -std=c++17 dtor_throw2.cpp -o dtor_throw2
./dtor_throw2            # 输出：捕获：析构函数里抛出的异常 / 程序结束
./dtor_throw2 unwind     # 输出：terminate called after throwing an instance of 'std::runtime_error'
```

## A.8 赋值运算符里的顺序

`C++`

```cpp
/* assign_order.cpp    编译：g++ -std=c++17 -fsanitize=address -g assign_order.cpp -o assign_order */
#include <cstdio>
#include <cstring>
#include <new>

static bool g_fail_next_alloc = false;

static char *alloc_or_throw(int n) {
    if (g_fail_next_alloc) throw std::bad_alloc();
    return new char[n];
}

class Buffer {
public:
    explicit Buffer(int n) : data_(new char[n]), size_(n) { std::memset(data_, 0, n); }
    ~Buffer() { delete[] data_; }
    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;

    /* 错的顺序：先释放，再去分配 */
    void assign_wrong(const Buffer &other) {
        delete[] data_;                             /* 先把自己清空 */
        data_ = alloc_or_throw(other.size_);        /* 这一步失败，data_ 就成了悬垂指针 */
        std::memcpy(data_, other.data_, other.size_);
        size_ = other.size_;
    }

    /* 对的顺序：先做会失败的事，成功了再释放 */
    void assign_right(const Buffer &other) {
        char *fresh = alloc_or_throw(other.size_);  /* 会抛的一步放前面 */
        std::memcpy(fresh, other.data_, other.size_);
        delete[] data_;                             /* 不会抛的一步放后面 */
        data_ = fresh;
        size_ = other.size_;
    }

    int size() const { return size_; }

private:
    char *data_;
    int   size_;
};

int main() {
    Buffer b(16);

    std::printf("对的顺序：分配失败，自己保持原样\n");
    {
        Buffer a(8);
        g_fail_next_alloc = true;
        try {
            a.assign_right(b);
        } catch (const std::bad_alloc &) {
            std::printf("  分配失败，a.size() 仍然是 %d\n", a.size());
        }
        g_fail_next_alloc = false;
    }

    std::printf("错的顺序：分配失败，自己已经残了\n");
    {
        Buffer a(8);
        g_fail_next_alloc = true;
        try {
            a.assign_wrong(b);
        } catch (const std::bad_alloc &) {
            std::printf("  分配失败，a.size() 还是 %d，但它管的那块内存已经还回去了\n", a.size());
        }
        g_fail_next_alloc = false;
    }                                               /* a 析构 → 再删一次同一块内存 */
    return 0;
}
```

`Bash`

```bash
g++ -std=c++17 assign_order.cpp -o assign_order.exe
./assign_order.exe                     # Windows 侧：以 0xC0000374（堆损坏）终止
wsl -d Ubuntu -e bash -c "g++ -std=c++17 -fsanitize=address -g assign_order.cpp -o assign_order_lin && ./assign_order_lin"
                                       # Linux 侧：attempting double-free
```

## A.9 RAII 的汇编对照

`C++`

```cpp
/* overhead.cpp    编译：g++ -std=c++17 -O2 -S overhead.cpp */
#include <memory>

extern "C" void consume(int *p);        /* 外部函数：指针逃出去，编译器不能把它优化掉 */

void manual_version() {
    int *p = new int(7);
    consume(p);
    delete p;
}

void raii_version() {
    std::unique_ptr<int> p(new int(7));
    consume(p.get());
}
```

`Bash`

```bash
g++ -std=c++17 -O2 -S overhead.cpp -o overhead.s
g++ -std=c++17 -O2 -c overhead.cpp -o overhead.o
nm -S --size-sort overhead.o            # 两个函数的字节数
```

`实测数据`
`Text`

```text
0000000000000000 0000000000000040 T _Z14manual_versionv
0000000000000040 0000000000000050 T _Z12raii_versionv
```

## A.10 智能指针

`C++`

```cpp
/* smart_ptr.cpp    编译：g++ -std=c++17 smart_ptr.cpp -o smart_ptr */
#include <cstdio>
#include <memory>
#include <stdexcept>

class Buffer {
public:
    explicit Buffer(int n) : size_(n) { std::printf("  Buffer 构造（%d 字节）\n", n); }
    ~Buffer() { std::printf("  Buffer 析构\n"); }

private:
    int size_;
};

void may_throw(int mode) {
    if (mode == 1) throw std::runtime_error("中途失败");
}

struct Node {
    std::shared_ptr<Node> next;
    ~Node() { std::printf("  Node 析构\n"); }
};

int main() {
    std::printf("unique_ptr 的大小 = %zu 字节，裸指针 = %zu 字节\n",
                sizeof(std::unique_ptr<Buffer>), sizeof(Buffer *));
    std::printf("shared_ptr 的大小 = %zu 字节\n", sizeof(std::shared_ptr<Buffer>));

    std::printf("异常路径上，unique_ptr 照样释放：\n");
    try {
        std::unique_ptr<Buffer> p(new Buffer(8));
        may_throw(1);
        std::printf("  不会走到这里\n");
    } catch (const std::exception &e) {
        std::printf("  捕获：%s\n", e.what());
    }

    std::printf("shared_ptr 的引用计数：\n");
    {
        std::shared_ptr<Buffer> a = std::make_shared<Buffer>(8);
        std::printf("  只有 a 指着它：%ld\n", a.use_count());
        std::shared_ptr<Buffer> b = a;
        std::printf("  b 也指着它：%ld\n", a.use_count());
    }
    std::printf("  最后一个引用消失，对象被销毁\n");

    std::printf("互相指着（循环引用）：\n");
    {
        std::shared_ptr<Node> x = std::make_shared<Node>();
        std::shared_ptr<Node> y = std::make_shared<Node>();
        x->next = y;
        y->next = x;
        std::printf("  x 的计数 = %ld，y 的计数 = %ld\n", x.use_count(), y.use_count());
    }
    std::printf("  两个 Node 都没有被析构\n");
    return 0;
}
```

`Bash`

```bash
g++ -std=c++17 smart_ptr.cpp -o smart_ptr
wsl -d Ubuntu -e bash -c "g++ -std=c++17 -fsanitize=address -g smart_ptr.cpp -o smart_ptr_lin && ./smart_ptr_lin"
                                       # Linux 侧：SUMMARY: AddressSanitizer: 64 byte(s) leaked in 2 allocation(s)
```

## A.11 文件句柄的 RAII 包装

`C++`

```cpp
/* file_guard.cpp    编译：g++ -std=c++17 file_guard.cpp -o file_guard */
#include <cstdio>
#include <stdexcept>
#include <utility>

class File {
public:
    explicit File(const char *path) : fp_(std::fopen(path, "w")), path_(path) {
        if (!fp_) throw std::runtime_error("打不开文件");
        std::printf("  打开 %s\n", path_);
    }
    ~File() { close_now(); }

    File(const File &) = delete;                 /* 同一个句柄不能关两次 */
    File &operator=(const File &) = delete;

    File(File &&other) noexcept : fp_(other.fp_), path_(other.path_) { other.fp_ = nullptr; }
    File &operator=(File &&other) noexcept {
        if (this != &other) {
            close_now();
            fp_ = other.fp_;
            path_ = other.path_;
            other.fp_ = nullptr;
        }
        return *this;
    }

    void write(const char *s) { if (fp_) std::fputs(s, fp_); }

private:
    void close_now() {
        if (fp_) {
            std::fclose(fp_);
            std::printf("  关闭 %s\n", path_);
            fp_ = nullptr;
        }
    }

    std::FILE  *fp_;
    const char *path_;
};

void early_return(bool bad) {
    File f("early.txt");
    f.write("hello\n");
    if (bad) {
        std::printf("  提前返回\n");
        return;                                  /* 不需要写 fclose */
    }
    std::printf("  正常结束\n");
}

void throw_path() {
    File f("throw.txt");
    f.write("hello\n");
    throw std::runtime_error("写到一半失败了");
}

int main() {
    std::printf("提前返回：\n");
    early_return(true);

    std::printf("异常路径：\n");
    try {
        throw_path();
    } catch (const std::exception &e) {
        std::printf("  捕获：%s\n", e.what());
    }

    std::printf("把句柄交给另一个对象：\n");
    {
        File a("move.txt");
        File b(std::move(a));
        b.write("hi\n");
    }
    std::printf("两种情况下文件都被关掉了\n");
    return 0;
}
```

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《05-类与面向对象/00-导读：类与 OOP 是手段.md》第 5 节 | **前置**：抽象不是免费的 |
| 《05-类与面向对象/02-类是一种类型.md》第 1 节 | **前置**：访问控制（`private` 析构的写法） |
| 《05-类与面向对象/04-构造与析构.md》第 1.5 小节 | **前置**：构造函数没有返回值 |
| 《05-类与面向对象/04-构造与析构.md》第 3 节 | **前置**：析构函数、析构顺序 |
| 《05-类与面向对象/04-构造与析构.md》第 7 节 | **前置**：RAII 的雏形 |
| 《05-类与面向对象/05-拷贝与移动.md》第 2 节 | **前置**：浅拷贝与双重释放 |
| 《05-类与面向对象/05-拷贝与移动.md》第 4 节 | **前置**：移动构造与 `std::move` |
| 《05-类与面向对象/05-拷贝与移动.md》第 5 节 | **前置**：编译器什么时候生成这几对 |
| 《04-语法/11-作用域、生存期与链接.md》第 3 节 | **前置**：生存期与析构时机 |
| 《04-语法/13-异常.md》第 2 节 | **前置**：栈展开 |
| 《04-语法/13-异常.md》第 4 节 | **前置**：`noexcept` |
| 《04-语法/06-控制流语句.md》第 4.2 小节 | 相关：`goto` 与集中释放 |
| 《04-语法/08-数组、指针与引用.md》第 5.4 小节 | 相关：本机 MinGW 没有 AddressSanitizer |
| 《04-语法/13-异常.md》第 5.1 小节 | 相关：不抛异常时的代价 |
| 《05-类与面向对象/07-继承.md》第 3.1 小节 | **后续**：基类与派生类的构造析构顺序 |
| 《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 3.5 小节 | **后续**：虚析构——通过基类指针删除派生类对象 |
| 《07-标准库/B-03-智能指针的用法.md》章节 | **后续**：`unique_ptr`、`shared_ptr` 的用法、删除器与选择 |
