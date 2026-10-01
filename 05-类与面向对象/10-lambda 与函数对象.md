# lambda 与函数对象

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**「把一段逻辑交给别的代码去调用」是 C++ 里反复出现的需求**：
排序时要告诉算法怎么比大小，遍历时要告诉它哪些元素算数，
事件发生时要有代码被调用。这类需求在写法上有同一个难点：
**那段逻辑写在哪里，它需要的那几个变量怎么带过去。**

在 lambda 之前，只有三条路：写成具名函数、写成一个仿函数类、或者用函数指针。
三条路各有各的麻烦。lambda 把这件事压缩成一个表达式，就近写、就近用，
需要的外部变量一起带进闭包。

它同时是第 09 章那个 `operator()` 的兑现：
**一个 lambda 就是一个对象，它的类型是编译器生成的、带 `operator()` 的类**
（《05-类与面向对象/09-运算符重载.md》第 3.6 小节）。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整源码与复现命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 内容 | 在哪一节 |
|---|---|
| 三种旧写法的代价，lambda 解决的是什么 | **第 1 节** |
| lambda 是编译器生成的类对象：类型唯一、大小、与函数指针的关系 | **第 2 节** |
| 捕获列表：值捕获、引用捕获、默认捕获、初始化捕获、`this` 与 `*this` | **第 3 节** |
| `mutable`、返回类型推导、泛型 lambda | **第 4 节** |
| 把可调用物传出去：函数指针、模板参数、`std::function` 与它们的代价 | **第 5 节** |
| 实际怎么用：把算法与逻辑分开 | **第 6 节** |
| 写代码时逐条对照的检查清单 | **第 7 节** |

---

# 第 1 节 为什么需要 lambda

## 1.1 三种旧写法各自的代价

先看一件最简单的事：从一个数组里数出「大于某个值」的元素个数。
「怎么算大于」是调用方的事，其余部分与调用方无关。

`C++`

```cpp
/* old_ways.cpp    编译：g++ -std=c++17 old_ways.cpp -o old_ways */
#include <cstdio>

int data[6] = {5, 2, 9, 1, 7, 3};

/* 写法一：具名函数——逻辑离调用点很远 */
bool bigger_than_4(int x) { return x > 4; }

/* 写法二：仿函数类——能带状态，但要为它写一个类 */
class BiggerThan {
public:
    explicit BiggerThan(int t) : t_(t) {}
    bool operator()(int x) const { return x > t_; }
private:
    int t_;
};

/* 只有「怎么判断」这一件事不同，其余都一样 */
int count_if_named(int threshold) {
    int n = 0;
    for (int x : data) if (x > threshold) ++n;      /* 阈值写死在代码里 */
    return n;
}

int count_if_functor(const BiggerThan &pred) {
    int n = 0;
    for (int x : data) if (pred(x)) ++n;
    return n;
}

int main() {
    std::printf("具名函数写死阈值：%d\n", count_if_named(4));

    int threshold = 4;                              /* 阈值是运行期才知道的值 */
    std::printf("仿函数带状态：%d\n", count_if_functor(BiggerThan(threshold)));
    return 0;
}
```

`实测数据`
`Text`

```text
具名函数写死阈值：3
仿函数带状态：3
```

| 写法 | 能带状态吗 | 代价 |
|---|---|---|
| **具名函数** | 只能靠参数或全局变量 | 逻辑必须写在别处，读代码时要跳过去看；阈值只能是编译期常量或额外参数 |
| **仿函数类** | 能，状态就是成员 | 为了一个表达式写一个类：起名字、写 `operator()`、想清楚 `const`，十几行 |
| **函数指针** | 不能（除非再传一个 `void *`） | 类型写起来啰嗦，且编译器看不见函数体，很难内联 |

这三种写法的共同问题不是「写起来长」，而是**逻辑与调用点被迫分开**：
读到这里的人要翻到别处才知道「大于」是怎么定义的。

## 1.2 lambda 的答案

同一件事，lambda 只需要就近编写逻辑：

`C++`

```cpp
int threshold = 4;
int n = count_if(data, 6, [threshold](int x) { return x > threshold; });
```

三个问题一次解决：

| 需求 | lambda 的做法 |
|---|---|
| 逻辑写在调用点旁边 | lambda 本身就是表达式，写在使用它的位置 |
| 逻辑要用到外面的变量 | **捕获列表**（`[threshold]`）把它带进来，不必加函数参数，也不必用全局变量 |
| 类型要给编译器看得到 | 闭包类型就在当前函数里，函数体完全可见，可以内联（第 5.3 小节） |

**它换来的是一种分工**：算法只管「怎么遍历、什么时候调用」，
调用方只管「这一步怎么算」。两边通过一个可调用对象对接，
而这个对象可以现场造，不必提前起名字。

## 1.3 一眼看懂语法

一个 lambda 表达式由四个部分组成，位置是固定的：

`C++`

```cpp
auto f = [threshold](int x) -> bool { return x > threshold; };
//        ~~~~~~~~~  ~~~~~~   ~~~~~~   ~~~~~~~~~~~~~~~~~~~~~~
//            |         |        |             |
//            |         |        |             └─ 函数体
//            |         |        └─ 返回类型，可省（第 4.3 小节）
//            |         └─ 参数表，和普通函数一样
//            └─ 捕获列表：这个 lambda 要用到外面的哪些变量，怎么用
```

四部分里只有**捕获列表**是新的，另外三部分与写普通函数没有区别。
参数表里写 `()` 表示不收参数；返回类型不写时由 `return` 语句推导。

---

# 第 2 节 lambda 就是一个对象

## 2.1 它是编译器生成的闭包类型

lambda 不是函数，而是一个对象；用 `sizeof` 与 `typeid` 可以直接看到这一点：

`C++`

```cpp
/* lambda_is_object.cpp    编译：g++ -std=c++17 lambda_is_object.cpp -o lambda_is_object */
#include <cstdio>
#include <typeinfo>

int main() {
    auto square = [](int x) { return x * x; };      /* 这不是函数，是一个对象 */

    std::printf("square(5) = %d\n", square(5));
    std::printf("sizeof(square) = %zu（空捕获：和空类一样，1 字节）\n", sizeof(square));
    std::printf("typeid 给出的名字：%s\n", typeid(square).name());

    auto square2 = [](int x) { return x * x; };     /* 长得一模一样 */
    std::printf("两个相同的 lambda 是同一个类型吗：%s\n",
                typeid(square) == typeid(square2) ? "是" : "否");

    int limit = 4;
    auto bigger = [limit](int x) { return x > limit; };
    std::printf("sizeof(bigger) = %zu（捕获了一个 int）\n", sizeof(bigger));

    double a = 1.0;
    int    b = 2;
    auto   two = [a, b](int x) { return x + a + b; };
    std::printf("sizeof(two) = %zu（捕获了 double + int）\n", sizeof(two));
    return 0;
}
```

`实测数据`
`Text`

```text
square(5) = 25
sizeof(square) = 1（空捕获：和空类一样，1 字节）
typeid 给出的名字：Z4mainEUliE_
两个相同的 lambda 是同一个类型吗：否
sizeof(bigger) = 4（捕获了一个 int）
sizeof(two) = 16（捕获了 double + int）
```

四件事从这组数字里读出来：

| 现象 | 说明 |
|---|---|
| `sizeof(square) = 1` | 没捕获任何变量的 lambda 与空类一样大：**它没有状态**，`operator()` 里用不到对象本身 |
| 捕获一个 `int` 之后 `sizeof = 4` | 捕获列表里的变量**变成了对象的成员**：值捕获是副本，就存在对象里 |
| 捕获 `double + int` 之后 `16` | 成员按普通类的对齐规则排布（8 + 4，再加 4 字节填充） |
| `typeid` 给的名字是 `Z4mainEUliE_` | 这是编译器给这个**无名闭包类型**取的名字：它没有名字可写，只能由编译器造 |

**lambda 表达式的每一处都对应一个类**，大致相当于编译器生成了这样一份代码：

`C++`

```cpp
class /* 无名 */ {
public:
    explicit /* 无名 */(int limit) : limit_(limit) {}     /* 捕获的变量成为成员 */
    bool operator()(int x) const { return x > limit_; }   /* lambda 的函数体 */
private:
    int limit_;
};
```

差别只有一处：这个类**没有名字**，因此它的类型在源码里写不出来，
要么让编译器推导（`auto`），要么交给模板参数代指。

`文档`

> "A lambda-expression is a prvalue whose result object is called the closure object. ... [Note:
> A closure object behaves like a function object (23.14). —end note]"
>
> —— N4659 §8.1.5/2
>
> "The type of a lambda-expression (which is also the type of the closure object) is a unique,
> unnamed non-union class type, called the closure type, whose properties are described below."
>
> —— N4659 §8.1.5.1/1

## 2.2 类型唯一：因此只能 `auto`、模板参数或者包装器

上面那行「两个相同的 lambda 是同一个类型吗：否」值得单独说：
**每一个 lambda 表达式的类型都是独一无二的**，哪怕两个 lambda 一个字都不差。

`C++`

```cpp
/* two_lambdas.cpp    编译：g++ -std=c++17 two_lambdas.cpp -o two_lambdas （失败） */
#include <vector>

int main() {
    auto a = [](int x) { return x + 1; };
    auto b = [](int x) { return x + 1; };       /* 与 a 长得一样，但是另一个类型 */
    std::vector<decltype(a)> v;
    v.push_back(a);
    v.push_back(b);                             /* 类型不同：塞不进去 */
    return (int)v.size();
}
```

`实测数据`
`Text`

```text
two_lambdas.cpp:9:16: error: no matching function for call to 'std::vector<main()::<lambda(int)> >::push_back(main()::<lambda(int)>&)'
two_lambdas.cpp:9:16: note: there are 2 candidates
```

名字里那个 `<lambda(int)>` 就是编译器打印闭包类型的方式。
想同时存好几个 lambda，只有三条路：

| 做法 | 说明 |
|---|---|
| 分别用 `auto` 保存 | 每个变量一个类型，最简单，但只适合数量固定、位置固定的场合 |
| 函数模板参数 | 把类型交给模板推导，函数体里正常调用（第 5.2 小节） |
| `std::function` | 类型擦除：把不同的闭包类型统一成一种类型，可以放进同一个容器（第 5.4 小节） |

表里第三条路的 `std::function` 是一个**包装器**：包装器把别的东西装在自己里面，
对外只暴露一套统一的接口。类型各不相同的闭包装进去之后，
从外面看只有 `std::function<int(int)>` 这一种类型，这就是表里说的类型擦除。
文件句柄的 RAII 包装是同一个思路，只不过它装进去的是一份资源，
由构造与析构管理这份资源的生命周期
（《05-类与面向对象/06-RAII 与资源管理.md》第 4 节）。

## 2.3 无捕获的 lambda 能当函数指针用

**没有捕获任何变量的 lambda 可以隐式转换成函数指针**——它不需要状态，
与一个普通函数没有区别：

`C++`

```cpp
/* lambda_to_fptr.cpp    编译：g++ -std=c++17 lambda_to_fptr.cpp -o lambda_to_fptr */
#include <cstdio>

int apply(int (*f)(int), int x) { return f(x); }    /* 只认函数指针 */

int main() {
    int (*p)(int) = [](int x) { return x + 1; };    /* 无捕获：隐式转成函数指针 */
    std::printf("p(1) = %d\n", p(1));
    std::printf("apply(..., 5) = %d\n", apply([](int x) { return x * 10; }, 5));
    return 0;
}
```

`实测数据`
`Text`

```text
p(1) = 2
apply(..., 5) = 50
```

一旦发生捕获，转换就不再成立，因为函数指针**没有地方存放那些副本**：

`C++`

```cpp
/* lambda_fptr_fail.cpp    编译：g++ -std=c++17 lambda_fptr_fail.cpp -o lambda_fptr_fail （失败） */
int main() {
    int limit = 4;
    int (*p)(int) = [limit](int x) { return x > limit; };   /* 捕获了：转不过去 */
    return p(5);
}
```

`实测数据`
`Text`

```text
lambda_fptr_fail.cpp:4:21: error: cannot convert 'main()::<lambda(int)>' to 'int (*)(int)' in initialization
```

`文档`

> "The closure type for a non-generic lambda-expression with no lambda-capture has a conversion
> function to pointer to function with C++ language linkage (10.5) having the same parameter and
> return types as the closure type's function call operator. ... The value returned by this
> conversion function is the address of a function F that, when invoked, has the same effect as
> invoking the closure type's function call operator."
>
> —— N4659 §8.1.5.1/6

> [!IMPORTANT]
> **lambda 的类型是编译器生成的无名类，值是它的对象；捕获的变量成为这个对象的成员。**
> 后面各节都是这一条的推论：
> 大小由捕获决定、类型唯一、无捕获时能退化成函数指针、
> 值捕获的副本默认只读（第 4.1 小节）、引用捕获要自己管生命周期（第 3.2 小节）。

---

# 第 3 节 捕获列表

## 3.1 值捕获与引用捕获

捕获列表里写一个变量名是**值捕获**：lambda 对象里存一份副本。
写 `&变量名` 是**引用捕获**：lambda 用的是外面那个变量本身。

`C++`

```cpp
/* capture_value_ref.cpp    编译：g++ -std=c++17 capture_value_ref.cpp -o capture_value_ref */
#include <cstdio>

int main() {
    int x = 1;

    auto by_value = [x] { return x; };              /* 拷贝一份 */
    auto by_ref   = [&x] { return x; };             /* 引用同一个 */

    x = 100;                                        /* 外面改了 */
    std::printf("值捕获看到 %d，引用捕获看到 %d\n", by_value(), by_ref());

    auto change_copy = [x]() mutable { x = 7; return x; };
    std::printf("mutable lambda 改的是副本：%d，外面的 x 还是 %d\n", change_copy(), x);

    auto change_real = [&x] { x = 7; };
    change_real();
    std::printf("引用捕获改的是本体：x = %d\n", x);
    return 0;
}
```

`实测数据`
`Text`

```text
值捕获看到 1，引用捕获看到 100
mutable lambda 改的是副本：7，外面的 x 还是 100
引用捕获改的是本体：x = 7
```

| | 值捕获 `[x]` | 引用捕获 `[&x]` |
|---|---|---|
| lambda 里看到的是 | 捕获那一刻的**副本** | 外面那个变量**本身** |
| 外面改了之后 | 看不到新值（第一行：`1`） | 看得到（第一行：`100`） |
| lambda 里改了之后 | 只改副本，外面不变 | 外面跟着变（第三行） |
| 代价 | 一次拷贝，对象变大 | 不拷贝，但要保证被引用的对象仍然存活 |

**选择的标准只有一条**：这段逻辑需要看到「当时的值」，还是要看到「现在的值」。
需要在 lambda 内部改并**跨调用保留**，用值捕获 + `mutable`（第 4.2 小节）；
需要在 lambda 内部改并且**外面也要看到**，用引用捕获，同时自己保证对象的生存期。

## 3.2 引用捕获要自己管生命周期

值捕获的副本跟着 lambda 对象走，闭包的生存期有多长，副本的生存期就有多长；
引用捕获只是个引用，**被引用的对象什么时候销毁，与闭包无关**。

`C++`

```cpp
/* dangling.cpp    编译：g++ -std=c++17 -fsanitize=address -g dangling.cpp -o dangling */
#include <cstdio>
#include <functional>

std::function<int()> make_value() {
    int local = 42;
    return [local] { return local; };       /* 值捕获：副本跟着闭包走 */
}

std::function<int()> make_ref() {
    int local = 42;
    return [&local] { return local; };      /* 引用捕获：local 随函数返回而销毁 */
}

int main() {
    auto f = make_value();
    std::printf("值捕获：%d\n", f());

    auto g = make_ref();
    std::printf("引用捕获：%d\n", g());     /* 读的是已经销毁的内存 */
    return 0;
}
```

两个函数做的事看起来一样，后果完全不同。
**第二个函数返回之后，`local` 已经随着栈帧消失，而 `g` 里存着它的地址。**

在这台机器上直接运行时，打印出的结果与正确实现没有区别：

`实测数据`
`Text`

```text
值捕获：42
引用捕获：42
```

**这是巧合**：那块栈内存刚好还没被别的数据覆盖。
换一个编译选项、换一台机器、或者在两次调用之间多做一些事，结果就会变。
在 Linux 上用 AddressSanitizer 编译同一份源码，问题立刻现形：

`实测数据`
`Text`

```text
==443==ERROR: AddressSanitizer: stack-use-after-return on address 0x7fe32d500030 ...
READ of size 4 at 0x7fe32d500030 thread T0
    #0 ... in operator() dangling.cpp:12
    #1 ... in __invoke_impl<int, make_ref()::<lambda()>&> ...
    #5 ... in main dangling.cpp:20
Address 0x7fe32d500030 is located in stack of thread T0 at offset 48 in frame
    #0 ... in make_ref() dangling.cpp:10
  This frame has 2 object(s):
    [48, 52) 'local' (line 11) <== Memory access at offset 48 is inside this variable
SUMMARY: AddressSanitizer: stack-use-after-return ... in operator()
```

报告说得很直白：读的是 `make_ref()` 那个已经返回的栈帧里的 `local`。

标准对这一点的措辞是「很可能导致未定义行为」：

`文档`

> "[Note: If a non-reference entity is implicitly or explicitly captured by reference, invoking the
> function call operator of the corresponding lambda-expression after the lifetime of the entity
> has ended is likely to result in undefined behavior. —end note]"
>
> —— N4659 §8.1.5.2/16

> [!CAUTION]
> **不要引用捕获生存期即将结束的对象**：
> 函数的局部变量、循环里的临时对象、`for` 循环的循环变量，都属于这一类。
> 需要把 lambda 存起来（放进容器、传给回调、从函数里返回）时，
> 一律改用值捕获，或者用初始化捕获把需要的对象搬进闭包（第 3.4 小节）。

## 3.3 默认捕获 `[=]` 与 `[&]`

`[=]` 表示「用到的都按值捕获」，`[&]` 表示「用到的都按引用捕获」。
写法短，但两处代价：

| 写法 | 好处 | 代价 |
|---|---|---|
| `[=]` | 不受生命周期影响，也不会误改外面 | 每个用到的大对象都会被拷贝一份（编译器只拷贝 lambda 里实际用到的那些） |
| `[&]` | 不拷贝，改的是本体 | 悬垂风险全部由使用者承担，且从 lambda 的写法上看不出来 |

**在成员函数里，`[=]` 还有一个容易忽略的行为**：它捕获的是 `this`，
也就是**指针**，不是对象本身：

`C++`

```cpp
/* capture_default.cpp    编译：g++ -std=c++17 capture_default.cpp -o capture_default */
#include <cstdio>

class Widget {
public:
    explicit Widget(int v) : v_(v) {}

    auto by_eq()   { return [=] { return v_; }; }    /* 默认值捕获 */
    auto by_star() { return [*this] { return v_; }; }/* 明确捕获整个对象的副本 */

    void set(int v) { v_ = v; }
private:
    int v_;
};

int main() {
    Widget w(1);
    auto a = w.by_eq();
    auto b = w.by_star();

    w.set(99);
    std::printf("[=] 捕到的看到 %d，[*this] 捕到的看到 %d\n", a(), b());
    return 0;
}
```

`实测数据`
`Text`

```text
[=] 捕到的看到 99，[*this] 捕到的看到 1
```

`[=]` 看到的是修改后的 `99`，说明它拿到的确实是 `this` 而不是对象副本；
`[*this]` 拿到的是一份拷贝，所以还是 `1`。
**想真的拷一份对象，要写 `[*this]`。**

> [!TIP]
> **默认捕获只在 lambda 很短、一眼能看完时用。**
> 稍长一点就把用到的那几个变量逐个写出来：
> 读代码的人不必回头找「到底捕了哪些」，编译器也不会多拷一个没注意到的变量。
> 需要改外面就用 `[&x]` 点名，比 `[&]` 安全得多。

## 3.4 初始化捕获：先算一个值，或者搬进来

捕获列表里还可以写「声明 + 初始化」，形式是 `[名字 = 表达式]`：

`C++`

```cpp
/* capture_init.cpp    编译：g++ -std=c++17 capture_init.cpp -o capture_init */
#include <cstdio>
#include <string>
#include <utility>

int main() {
    int a = 3;

    auto calc = [n = a * 2, s = "前缀"] { std::printf("  %s：%d\n", s, n); };   /* 先算值再捕获 */
    calc();

    std::string text = "一段很长的文本";
    auto moved = [s = std::move(text)] { return s.size(); };   /* 把变量搬进闭包，不拷贝 */
    std::printf("搬进来之后长度 %zu（外面那个字符串已经交出去了）\n", moved());
    return 0;
}
```

`实测数据`
`Text`

```text
  前缀：6
搬进来之后长度 21（外面那个字符串已经交出去了）
```

（`21` 是字节数：7 个汉字在 UTF-8 下一个占 3 字节。）

`文档`

> "An init-capture behaves as if it declares and explicitly captures a variable of the form
> "auto init-capture ;" whose declarative region is the lambda-expression's compound-statement
> ... [Note: This enables an init-capture like "x = std::move(x)"; the second "x" must bind to a
> declaration in the surrounding context. —end note]"
>
> —— N4659 §8.1.5.2/6

它解决两类问题：

| 场景 | 写法 | 说明 |
|---|---|---|
| lambda 里要用的值是**算出来的**，外面没有这个名字 | `[n = a * 2]` | 相当于在闭包里声明一个成员并初始化 |
| 对象**只应该搬进去**，不该被拷贝 | `[s = std::move(text)]` | 移动之后外面那个对象进入「有效但内容未指定」的状态（《05-类与面向对象/05-拷贝与移动.md》第 4 节） |

## 3.5 捕获 `this` 与 `*this`

成员函数里的 lambda 想用成员变量，要在捕获列表里写 `this` 或者 `*this`：

`C++`

```cpp
/* capture_this.cpp    编译：g++ -std=c++17 capture_this.cpp -o capture_this */
#include <cstdio>

class Counter {
public:
    explicit Counter(int v) : v_(v) {}

    auto by_pointer() {                 /* 捕获 this：看到的是当前对象 */
        return [this] { return v_; };
    }
    auto by_copy() {                    /* 捕获 *this：拷贝一份对象（C++17） */
        return [*this] { return v_; };
    }
    void set(int v) { v_ = v; }
private:
    int v_;
};

int main() {
    Counter c(10);
    auto p = c.by_pointer();
    auto v = c.by_copy();

    c.set(99);                          /* 对象改了 */
    std::printf("捕获 this 看到 %d，捕获 *this 看到 %d\n", p(), v());
    return 0;
}
```

`实测数据`
`Text`

```text
捕获 this 看到 99，捕获 *this 看到 10
```

| 写法 | 闭包里存的是 | 对象销毁之后再调用 | 什么时候用 |
|---|---|---|---|
| `[this]` | 指向当前对象的指针 | 悬垂，未定义行为 | lambda 的生存期不能超过对象 |
| `[*this]` | 当前对象的一份拷贝 | 安全（但对象必须是可拷贝的） | lambda 要被存起来，或者生存期可能长于对象 |

**`[this]` 不是按值捕获**，所以闭包里存的不是对象，而是「怎么找到这个对象」；
真正把对象拷一份进来的是 `[*this]`。实测里 `[this]` 看到的是修改后的 `99`，正是这个意思。

`文档`

> "For each entity captured by copy, an unnamed non-static data member is declared in the closure
> type."
>
> —— N4659 §8.1.5.2/10
>
> "An entity is captured by reference if it is implicitly or explicitly captured but not captured
> by copy."
>
> —— N4659 §8.1.5.2/12

---

# 第 4 节 mutable 与返回类型

## 4.1 值捕获的副本默认只读

值捕获的副本是闭包对象的成员，而 **lambda 的 `operator()` 默认是 `const` 成员函数**，
所以这个副本在函数体里是只读的：

`C++`

```cpp
/* mutable_counter.cpp    编译：g++ -std=c++17 mutable_counter.cpp -o mutable_counter （失败） */
int main() {
    int n = 0;
    auto bad = [n] { n = n + 1; return n; };        /* 值捕获的副本默认不可改 */
    return bad();
}
```

`实测数据`
`Text`

```text
mutable_counter.cpp:4:24: error: assignment of read-only variable 'n'
```

**这条规则不是针对 lambda 单独定的**，它是两件已有规则的叠加：
`operator()` 是 `const` 成员函数（没写 `mutable` 时），
而 `const` 成员函数里的成员是只读的（《05-类与面向对象/02-类是一种类型.md》第 2.3 小节）。

`文档`

> "The function call operator or operator template is declared const (12.2.2) if and only if the
> lambda-expression's parameter-declaration-clause is not followed by mutable. It is neither
> virtual nor declared volatile."
>
> —— N4659 §8.1.5.1/4

## 4.2 `mutable`：让副本可改，状态留在对象里

在参数表后面写 `mutable`，`operator()` 就不再是 `const` 成员函数，副本可以改：

`C++`

```cpp
/* mutable_ok.cpp    编译：g++ -std=c++17 mutable_ok.cpp -o mutable_ok */
#include <cstdio>

int main() {
    auto counter = [n = 0]() mutable { return ++n; };   /* 副本可改，且跨调用保留 */
    std::printf("第一次 %d\n", counter());
    std::printf("第二次 %d\n", counter());
    std::printf("第三次 %d\n", counter());
    return 0;
}
```

`实测数据`
`Text`

```text
第一次 1
第二次 2
第三次 3
```

**计数能跨调用累加，是因为状态存在闭包对象里**——这正是仿函数相对普通函数的优势
（《05-类与面向对象/09-运算符重载.md》第 3.6 小节）。
`mutable` 只是把「这个成员可以改」写出来，让编译器允许这件事。

> [!WARNING]
> **`mutable` 改的是副本，外面的变量不会跟着变**（第 3.1 小节实测过）。
> 想让外面也变，要么用引用捕获，要么让 lambda 返回新值、由调用方赋值。

## 4.3 返回类型由 `return` 语句推导

lambda 的返回类型不写时，按 `auto` 的规则从 `return` 语句推导：

`C++`

```cpp
/* return_type_ok.cpp    编译：g++ -std=c++17 return_type_ok.cpp -o return_type_ok */
#include <cstdio>
#include <typeinfo>

int main() {
    auto one = [](int x) { return x * 2; };             /* 一条 return：推导出 int */
    auto two = [](int x) { return x * 2.0; };           /* 推导出 double */
    auto t   = [](int x) -> double { return x / 2; };   /* 显式写返回类型：结果是 double */

    std::printf("one(3) 的类型 %s，值 %d\n", typeid(one(3)).name(), one(3));
    std::printf("two(3) 的类型 %s，值 %.1f\n", typeid(two(3)).name(), two(3));
    std::printf("t(3)   的类型 %s，值 %.1f\n", typeid(t(3)).name(), t(3));
    return 0;
}
```

`实测数据`
`Text`

```text
one(3) 的类型 i，值 6
two(3) 的类型 d，值 6.0
t(3)   的类型 d，值 1.0
```

**多条 `return` 的类型必须一致**，否则推导失败：

`C++`

```cpp
/* return_type_fail.cpp    编译：g++ -std=c++17 return_type_fail.cpp -o return_type_fail （失败） */
int main() {
    auto f = [](int x) {                /* 两条 return，类型不一致 */
        if (x > 0) return 1;
        return 2.5;
    };
    return (int)f(1);
}
```

`实测数据`
`Text`

```text
return_type_fail.cpp:5:16: error: inconsistent types 'int' and 'double' deduced for lambda return type
```

`文档`

> "If a function with a declared return type that contains a placeholder type has multiple
> non-discarded return statements, the return type is deduced for each such return statement.
> If the type deduced is not the same in each deduction, the program is ill-formed."
>
> —— N4659 §10.1.7.4/8

改法有两条：把两个 `return` 的表达式统一成同一种类型，
或者在参数表后面显式写 `-> double`（上面 `t` 就是这么做的）。

## 4.4 参数写 `auto`：泛型 lambda

参数表里写 `auto`，lambda 就变成「对多种类型都能用」：

`C++`

```cpp
/* generic_lambda.cpp    编译：g++ -std=c++17 generic_lambda.cpp -o generic_lambda */
#include <cstdio>
#include <string>

int main() {
    auto twice = [](auto x) { return x + x; };      /* 参数写 auto：泛型 lambda */
    std::printf("%d %d\n", twice(3), twice(4));
    std::printf("%.1f，字符串长度 %zu\n", twice(0.5), twice(std::string("ab")).size());
    return 0;
}
```

`实测数据`
`Text`

```text
6 8
1.0，字符串长度 4
```

写法上它很像一个函数模板，事实也是：**编译器为每一种实参类型生成一份 `operator()`**。
它的完整规则属于模板那一部分（《05-类与面向对象/11-模板.md》第 2 节），
这里只需要知道：参数写 `auto` 的 lambda 叫**泛型 lambda**，
无捕获时它也能转成函数指针，但转成的是一个**函数模板**对应的那一份实例。

---

# 第 5 节 把可调用物传出去

## 5.1 三种方式

一段逻辑要交给别的代码去调用，接收方的参数类型有三种常见写法：

| 接收方写法 | 能接收什么 | 类型信息 | 调用开销 |
|---|---|---|---|
| `int (*f)(int)` | 普通函数、无捕获的 lambda | 只有签名，函数体看不见 | 一次间接调用，不能内联 |
| `template <class F> ... F f` | 任何可调用物：函数、lambda、仿函数 | **完整保留**，函数体可见 | 与直接调用相同，可内联 |
| `std::function<int(int)>` | 任何可调用物 | 擦除掉了，只留下签名 | 一次间接调用，通常拿不回来 |

第三行那个 `std::function` 就是第 2.2 小节说的**包装器**：它把类型各异的闭包装进自己里面，
对外只留一种类型。

三者的差别可以一句话概括：**模板参数把类型传下去，`std::function` 把类型藏起来，
函数指针只认最朴素的一种。**

## 5.2 模板参数：类型不擦除

接收方写成模板参数时，闭包的具体类型被完整保留下来：

`C++`

```cpp
/* pass_callable.cpp    编译：g++ -std=c++17 pass_callable.cpp -o pass_callable */
#include <cstdio>
#include <functional>

template <class F>
int apply_t(F f, int x) { return f(x); }                    /* 模板参数：类型不擦除 */

int apply_std(const std::function<int(int)> &f, int x) { return f(x); }   /* 类型擦除 */

int main() {
    int limit = 4;
    auto pred  = [limit](int x) { return x > limit ? x : 0; };
    auto empty = [](int x) { return x; };

    std::printf("apply_t   = %d\n", apply_t(pred, 9));
    std::printf("apply_std = %d\n", apply_std(pred, 9));
    std::printf("sizeof(捕获一个 int 的闭包) = %zu\n", sizeof(pred));
    std::printf("sizeof(空捕获的闭包)        = %zu\n", sizeof(empty));
    std::printf("sizeof(std::function)       = %zu\n", sizeof(std::function<int(int)>));
    std::printf("sizeof(函数指针)            = %zu\n", sizeof(int (*)(int)));
    return 0;
}
```

`实测数据`
`Text`

```text
apply_t   = 9
apply_std = 9
sizeof(捕获一个 int 的闭包) = 4
sizeof(空捕获的闭包)        = 1
sizeof(std::function)       = 32
sizeof(函数指针)            = 8
```

数字背后是两件事：

| 数字 | 说明 |
|---|---|
| 闭包只有 1~4 字节，`std::function` 是 **32 字节** | `std::function` 要同时装下「被擦除的对象」和「怎么调用它」两件事，还要留出小对象的缓冲区 |
| 同一个 lambda 传给两种接口都对 | 传进去的方式不同，**用起来的写法一样**（都是 `f(x)`），差别在编译器还能看到多少 |

## 5.3 代价：一次间接调用

同一件事用两种方式写，`-O0` 下的调用点不一样。先看模板参数这一版：

`实测数据`
`Assembly`

```asm
via_template:
        movl    $4, -4(%rbp)                    # 捕获的 limit = 4
        movl    -4(%rbp), %eax
        movl    $9, %edx                        # 实参 9
        movl    %eax, %ecx                      # this：闭包对象
        call    _Z7apply_tIZ12via_templatevEUliE_EiT_i    # 直接调用，地址编译期就写好
```

再看 `std::function` 这一版：

`实测数据`
`Assembly`

```asm
via_std:
        movl    $4, -4(%rbp)
        movl    -4(%rbp), %eax
        movl    %eax, -8(%rbp)                  # 先在栈上造一个闭包
        leaq    -8(%rbp), %rdx
        leaq    -48(%rbp), %rax
        movq    %rax, %rcx
        call    _ZNSt8functionIFiiEEC1IZ7via_stdvEUliE_vEEOT_   # 把闭包包进 std::function
        leaq    -48(%rbp), %rax
        movl    $9, %edx
        movq    %rax, %rcx
        call    _Z9apply_stdRKSt8functionIFiiEEi                # 调用它，内部还要再转发一层
```

（两段都是节选，省去了栈帧建立与销毁的指令；完整清单见附录 A 的 `pass_asm.cpp`。）

`apply_std` 内部那层转发叫 `_M_invoke`：它拿着擦除后的类型信息，
再回头去调真正的闭包。这一层是 `std::function` 的通用代价。

**但开优化之后这个差别可能消失。** 同一份源码在 `-O2` 下，两个函数都被折叠成了同一个结果：

`实测数据`
`Assembly`

```asm
via_template:
        movl    $9, %eax
        ret

via_std:
        movl    $9, %eax
        ret
```

编译器把整个计算求成了常量，两条路都只剩一条 `movl`。
真实代码里能不能优化掉，要看调用点是否可见、内联是否发生——
所以「哪个更快」不能靠猜，要看实测。

1 亿次调用的实测给出一个量级：

`C++`

```cpp
/* cost_call.cpp    编译：g++ -std=c++17 -O2 cost_call.cpp -o cost_call */
#include <chrono>
#include <cstdio>
#include <functional>

template <class F>
long long sum_with(F f, int n) {
    long long s = 0;
    for (int i = 0; i < n; ++i) s += f(i);
    return s;
}

long long sum_std(const std::function<int(int)> &f, int n) {
    long long s = 0;
    for (int i = 0; i < n; ++i) s += f(i);
    return s;
}

int main() {
    const int n = 100000000;
    auto f = [](int x) { return x & 7; };

    auto t0 = std::chrono::steady_clock::now();
    long long a = sum_with(f, n);
    auto t1 = std::chrono::steady_clock::now();
    long long b = sum_std(f, n);
    auto t2 = std::chrono::steady_clock::now();

    auto ms = [](auto x, auto y) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(y - x).count();
    };
    std::printf("模板参数：%lld，用时 %lld 毫秒\n", a, (long long)ms(t0, t1));
    std::printf("std::function：%lld，用时 %lld 毫秒\n", b, (long long)ms(t1, t2));
    return 0;
}
```

`实测数据`
`Text`

```text
模板参数：350000000，用时 26 毫秒
std::function：350000000，用时 95 毫秒
```

结果相同，时间差了近四倍：模板参数那一版被内联并向量化，
`std::function` 那一版每次调用都要经过一层类型擦除的转发。
（时间是机器相关的，这里看的是量级。）

## 5.4 什么时候用哪个

实际写代码时，按场合的选择如下：

| 场合 | 选择 | 理由 |
|---|---|---|
| 接收方是模板（自己写的算法、`std::sort` 这类） | **直接传 lambda** | 零代价，还能内联 |
| 需要**存**起来：放进容器、写成类的成员、跨接口传递 | `std::function` | 类型必须统一，只能擦除 |
| 对方只接受函数指针（C 接口、回调注册） | 无捕获的 lambda，或者普通函数 | 只有这两种能转换过去 |
| 只在一处用到，立刻调用 | 直接写 lambda 表达式，连变量都不用存 | 类型唯一，存下来反而麻烦 |

**`std::function` 对存进去的可调用物有一条要求**：必须**可拷贝**。

`文档`

> "Requires: F shall be CopyConstructible."
>
> —— N4659 §23.14.13.2.1/7

因此捕获了 `std::unique_ptr` 这类只能移动的对象的 lambda 无法存入
（《05-类与面向对象/05-拷贝与移动.md》第 4 节）。
`std::function` 的完整用法与替代品（`std::bind`、函数视图）属于【待补：06-标准库/】。

---

# 第 6 节 实战：把算法与逻辑分开

## 6.1 同一套算法，逻辑由调用方给

把前面的写法放进一个完整的例子里：一份物品清单，按不同条件计数，按不同依据排序。

`C++`

```cpp
/* use_case.cpp    编译：g++ -std=c++17 use_case.cpp -o use_case */
#include <cstdio>
#include <cstring>

struct Item {
    const char *name;
    int         weight;
};

/* 只是一个「按什么算」的算法，逻辑由调用方给 */
template <class F>
int count_if(const Item *items, int n, F pred) {
    int c = 0;
    for (int i = 0; i < n; ++i)
        if (pred(items[i])) ++c;
    return c;
}

template <class F>
void sort_items(Item *items, int n, F less) {
    for (int i = 1; i < n; ++i) {                       /* 插入排序，够用就好 */
        Item key = items[i];
        int  j = i - 1;
        while (j >= 0 && less(key, items[j])) { items[j + 1] = items[j]; --j; }
        items[j + 1] = key;
    }
}

int main() {
    Item items[5] = {
        {"扳手", 300}, {"螺丝刀", 80}, {"锤子", 500}, {"卷尺", 150}, {"钳子", 200},
    };

    int limit = 200;                                    /* 运行期才知道的阈值 */
    std::printf("超过 %d 克的有 %d 件\n", limit,
                count_if(items, 5, [limit](const Item &it) { return it.weight > limit; }));

    sort_items(items, 5, [](const Item &a, const Item &b) { return a.weight < b.weight; });
    std::printf("按重量排序：");
    for (const Item &it : items) std::printf(" %s(%d)", it.name, it.weight);
    std::printf("\n");

    const char *prefix = "螺";                           /* 按名字前缀筛选 */
    std::printf("名字以「%s」开头的有 %d 件\n", prefix,
                count_if(items, 5, [prefix](const Item &it) {
                    return std::strncmp(it.name, prefix, std::strlen(prefix)) == 0;
                }));
    return 0;
}
```

`实测数据`
`Text`

```text
超过 200 克的有 2 件
按重量排序： 螺丝刀(80) 卷尺(150) 钳子(200) 扳手(300) 锤子(500)
名字以「螺」开头的有 1 件
```

两个算法各写一次，条件由调用方现场给：`limit` 是运行期的值，
`prefix` 是另一个筛选条件，`less` 决定排序方向。
**如果不用 lambda，这三个条件就得各写一个具名函数（或者一个仿函数类），
函数与它要用的那个变量被迫分开。**

## 6.2 常见错误

前面几节出现过的误用，按症状与改法归总如下：

| 错误写法 | 症状 | 改法 |
|---|---|---|
| `[&local]` 里引用的是局部变量，lambda 被存起来延后调用 | 调用时读的是已销毁的内存（第 3.2 小节） | 改值捕获，或用 `[v = local]` 搬进来 |
| 在成员函数里用 `[=]` 却以为拷了整个对象 | 对象改了之后 lambda 看到的是新值（第 3.3 小节） | 明确写 `[*this]` |
| 想改值捕获的副本但没写 `mutable` | `assignment of read-only variable` | 加 `mutable`，或者改成引用捕获 |
| 想用 lambda 改外面的变量，却用了值捕获 | 编译能过，外面没变 | 用 `[&x]`，或让 lambda 返回值再赋值 |
| 把两个不同的 lambda 放进同一个容器 | `no matching function for call to ... push_back` | 统一成 `std::function`，或改模板参数 |
| 在类定义里存一个 `auto` 成员 | `auto` 不能作数据成员类型 | 用 `std::function`，或者把类型写成模板参数 |

---

# 第 7 节 检查清单

写代码时逐条对照：

| 检查 | 说明 |
|---|---|
| lambda 的生存期会不会超过它引用的那些变量 | 会，就不用引用捕获（第 3.2 小节） |
| 需要的是「当时的值」还是「现在的值」 | 决定值捕获还是引用捕获（第 3.1 小节） |
| 成员函数里的 lambda 要的是 `this` 还是对象副本 | 要副本写 `[*this]`（第 3.3 小节） |
| 改值捕获的副本时写了 `mutable` 吗 | 没写会直接编不过（第 4.1 小节） |
| 多条 `return` 的类型一致吗，或者写了 `-> T` | 不一致时推导失败（第 4.3 小节） |
| 接收方能收模板参数吗 | 能，就不必用 `std::function`，省一次类型擦除（第 5.4 小节） |
| 传出去的 lambda 是不是只用了无捕获的形式 | 只有无捕获才能转成函数指针（第 2.3 小节） |
| 默认捕获 `[=]`、`[&]` 用在短 lambda 上了吗 | 稍长就逐个写清楚（第 3.3 小节） |

---

# 术语表

| 词 | 含义 | 在哪一节 |
|---|---|---|
| **lambda 表达式** | 就近写出一段可调用逻辑的表达式 | 第 1.2 小节 |
| **闭包对象** | lambda 表达式产生出来的那个对象 | 第 2.1 小节 |
| **闭包类型** | 编译器为每个 lambda 生成的、唯一且无名的类类型 | 第 2.1 小节 |
| **捕获列表** | `[]` 里写明这段逻辑要用外面的哪些变量、怎么用 | 第 3 节 |
| **值捕获** | 拷贝一份存进闭包（`[x]`、`[=]`） | 第 3.1 小节 |
| **引用捕获** | 用外面那个变量本身（`[&x]`、`[&]`） | 第 3.1 小节 |
| **初始化捕获** | `[x = 表达式]`，先算一个值或把对象搬进来 | 第 3.4 小节 |
| **`mutable`** | 写在参数表后面，让闭包里的副本可以改 | 第 4.2 小节 |
| **泛型 lambda** | 参数写 `auto` 的 lambda，对多种类型都能用 | 第 4.4 小节 |
| **函数对象 / 仿函数** | 重载了 `operator()` 的类的对象，lambda 就是它 | 第 2.1 小节 |
| **包装器 / wrapper** | 把别的东西装在自己里面、对外只暴露一套统一接口的类（`std::function` 是可调用物的包装器） | 第 2.2 小节 |
| **类型擦除** | 把不同的具体类型统一成一种类型（`std::function` 的做法） | 第 5.1 小节 |

---

# 附录 A 复现本章节实测

主线的程序都是完整可编译的单文件，源码就在正文里；这里列出全部程序的编译命令与出处。

`实测数据`

| 程序 | 编译命令 | 演示什么 | 源码在哪 |
|---|---|---|---|
| `old_ways.cpp` | `g++ -std=c++17 old_ways.cpp -o old_ways` | 三种旧写法 | 第 1.1 小节 |
| `lambda_is_object.cpp` | 同上 | 大小、类型、独一无二 | 第 2.1 小节 |
| `two_lambdas.cpp` | 同上（**期望失败**） | 类型不同不能混存 | 第 2.2 小节 |
| `lambda_to_fptr.cpp` | 同上 | 无捕获转函数指针 | 第 2.3 小节 |
| `lambda_fptr_fail.cpp` | 同上（**期望失败**） | 捕获之后无法转换 | 第 2.3 小节 |
| `capture_value_ref.cpp` | 同上 | 值捕获与引用捕获 | 第 3.1 小节 |
| `dangling.cpp` | `g++ -std=c++17 -fsanitize=address -g dangling.cpp -o dangling` | 悬垂引用（Linux 侧） | 第 3.2 小节 |
| `capture_default.cpp` | `g++ -std=c++17 capture_default.cpp -o capture_default` | `[=]` 在成员函数里捕的是 `this` | 第 3.3 小节 |
| `capture_init.cpp` | 同上 | 初始化捕获 | 第 3.4 小节 |
| `capture_this.cpp` | 同上 | `[this]` 与 `[*this]` | 第 3.5 小节 |
| `mutable_counter.cpp` | 同上（**期望失败**） | 副本默认只读 | 第 4.1 小节 |
| `mutable_ok.cpp` | 同上 | `mutable` 让状态跨调用保留 | 第 4.2 小节 |
| `return_type_ok.cpp` | 同上 | 返回类型推导 | 第 4.3 小节 |
| `return_type_fail.cpp` | 同上（**期望失败**） | 两条 `return` 类型不一致 | 第 4.3 小节 |
| `generic_lambda.cpp` | 同上 | 泛型 lambda | 第 4.4 小节 |
| `pass_callable.cpp` | 同上 | 两种传递方式的大小 | 第 5.2 小节 |
| `pass_asm.cpp` | `g++ -std=c++17 -O0 -S pass_asm.cpp -o pass_asm_O0.s` | 调用点汇编 | 第 5.3 小节 |
| `cost_call.cpp` | `g++ -std=c++17 -O2 cost_call.cpp -o cost_call` | 1 亿次调用的时间 | 第 5.3 小节 |
| `use_case.cpp` | `g++ -std=c++17 use_case.cpp -o use_case` | 算法与逻辑分开 | 第 6.1 小节 |

## A.1 复现本章节实测用的环境

`实测数据`

| 项目 | 取值 |
|---|---|
| Windows 侧编译器 | `g++` 15.2.0（MinGW-w64，x86-64） |
| Linux 侧编译器 | `g++` 13.3.0（Ubuntu 24.04，x86-64） |
| 语言标准 | `-std=c++17` |
| 优化等级 | 未特别注明处不开优化；第 5.3 小节标注 `-O0` / `-O2` |
| 地址宽度 | 64 位，因此一个指针是 8 字节 |

闭包与 `std::function` 的大小都与地址宽度、标准库实现有关，换平台时这些数字会变。

## A.2 悬垂引用那条实测的复现命令

第 3.2 小节的 AddressSanitizer 报告要用 Linux 侧复现（本机 MinGW 没有 ASan，
见《05-类与面向对象/06-RAII 与资源管理.md》第 3.1 小节）：

`Bash`

```bash
g++ -std=c++17 -fsanitize=address -g dangling.cpp -o dangling
ASAN_OPTIONS=detect_stack_use_after_return=1 ./dangling
```

`detect_stack_use_after_return=1` 是为了让「函数返回后的栈内存」被真正标记出来；
不开这个选项时，那块内存可能还没被复用，程序会像在 Windows 上一样打印出看似正确的结果。

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《05-类与面向对象/09-运算符重载.md》第 3.6 小节 | **前置**：`operator()` 与仿函数 |
| 《05-类与面向对象/02-类是一种类型.md》第 2.3 小节 | **前置**：`const` 成员函数（值捕获的副本为什么只读） |
| 《05-类与面向对象/05-拷贝与移动.md》第 4 节 | **前置**：移动构造与 `std::move`（初始化捕获里搬对象） |
| 《05-类与面向对象/05-拷贝与移动.md》第 2 节 | **前置**：拷贝与赋值（值捕获要拷一份） |
| 《05-类与面向对象/06-RAII 与资源管理.md》第 3.1 小节 | 相关：本机 MinGW 没有 AddressSanitizer |
| 《04-语法/01-一些基础概念.md》第 1.5 小节 | **前置**：lambda 是「一个带 `operator()` 的类的对象」 |
| 《04-语法/08-数组、指针与引用.md》第 4 节 | **前置**：函数指针与间接调用 |
| 《04-语法/11-作用域、生存期与链接.md》第 3 节 | **前置**：生存期（引用捕获的风险来自这里） |
| 《04-语法/13-异常.md》第 4 节 | 相关：`noexcept` 与转换出来的函数指针 |
| 《05-类与面向对象/11-模板.md》第 2 节 | **后续**：泛型 lambda 与函数模板 |
| 【待补：06-标准库/】 | **后续**：`std::function`、`std::bind`、函数视图 |
| 《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 5 节 | 相关：重载、覆盖、隐藏的分工（lambda 的 `operator()` 不参与这三者） |
