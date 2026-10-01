# 导读：类与 OOP 是手段

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**在这一板块，我们离开 C，走进 C++ 层次更高的那部分。**

前面四个板块讲的都是两门语言共用的地基，因此每一处都要分清「C 怎么写、C++ 怎么写」；
**从这里起不再并排**。类、构造与析构、拷贝与移动、运算符重载、继承与多态、模板、lambda——
**这些在 C 里都没有对应物**，讲它们时也只有 C++ 一种写法可讲。

第四板块里反复出现的那些「C 与 C++ 的分歧」，大多是在为这里铺路：
引用是为了传参不拷贝，`const` 成员是为了让接口说清自己改不改对象，
`namespace` 是为了让名字能重名，异常是为了让构造函数能报告失败。
**这一板块正是那些铺垫要去的地方。**

**而在写第一个类之前，有一件事要先摆正：类与 OOP 从来不是目标，它们是手段。**

**从这一章起，示例一律以 C++ 为主**，
**需要说明「C 里没有这个」时，用一句话交代，不再展开 C 的实现**——
硬凑一份等价的 C 版，只会把注意力从 C++ 上引开。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整源码与复现命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 问题 | 在哪一节 |
|---|---|
| 没有类的时候，代码会卡在哪 | 一 |
| **封装到底换来了什么** | **二** |
| 类是什么，对象是什么 | 三 |
| 「面向对象」这四个字该怎么摆 | 四 |
| **抽象是不是免费的、「优雅」该怎么看** | **五** |
| 这一板块与其它板块的分工 | 六 |

---

# 第 1 节 类解决的问题

## 1.1 没有封装的时候怎么写

**栈是一个合适的例子**：它有一组数据（元素与栈顶位置），
有一组操作（入栈、出栈），而且**两者必须保持一致**。

**先看不加封装时的写法**——数据摆在外面，操作写成一组独立的函数：

`C++`

```cpp
/* stack_plain.cpp    编译：g++ -std=c++17 -c stack_plain.cpp -o stack_plain.o */
#include <cstdio>

struct Stack { int data[4]; int top; };     // 内部状态全摆在外面

void stack_init(Stack *s) { s->top = 0; }

int stack_push(Stack *s, int v) {
    if (s->top >= 4) return 0;
    s->data[s->top++] = v;
    return 1;
}

int stack_pop(Stack *s, int *out) {
    if (s->top <= 0) return 0;
    *out = s->data[--s->top];
    return 1;
}
```

**这不是「C 的写法」，C++ 里同样能这么写**——上面这份代码没有用到类的任何特性。
麻烦在别处。

## 1.2 三件麻烦事

**第一件：内部状态谁都能改。**

`top` 与 `data` 必须一致——这正是「栈」这个数据结构的意思。
但它们是 `struct` 的普通成员，**任何一个拿到 `struct Stack` 的地方都能直接改**：

`C++`

```cpp
/* stack_broken.cpp    编译：g++ -std=c++17 stack_broken.cpp -o stack_broken */
#include <cstdio>

struct Stack { int data[4]; int top; };

void stack_init(Stack *s) { s->top = 0; }
int stack_push(Stack *s, int v) {
    if (s->top >= 4) return 0;
    s->data[s->top++] = v;
    return 1;
}
int stack_pop(Stack *s, int *out) {
    if (s->top <= 0) return 0;
    *out = s->data[--s->top];
    return 1;
}

int main() {
    Stack s = {};
    stack_init(&s);
    stack_push(&s, 1);
    stack_push(&s, 2);

    s.top = 3;                      // 直接改内部字段：声称有三个元素

    int v = -1;
    stack_pop(&s, &v);
    std::printf("弹出的值是 %d\n", v);   // 这个位置从来没被写过
    return 0;
}
```

`实测数据`
`Text`

```text
弹出的值是 0
```

**编译器帮不上忙。** `s.top = 3;` 完全合法——
`top` 就是一个 `int`，赋什么值都行。
**能保证「栈顶位置与元素个数一致」的只有人的自觉**，
而这正是所有这类 bug 的来源。

**第二件：换实现要动所有调用方。**

假设要把固定数组换成链表（因为栈需要长得更大）。
`struct Stack` 里的 `data` 与 `top` 要换掉，于是**所有直接碰过这两个字段的代码都要改**。
就算改的是函数里的实现，只要签名变了，调用方也要跟着改。

**第三件：复用只能靠复制。**

第二处要用栈，只能把这份代码复制过去、改个名字。
**改一次要改两处**，两处迟早会不一致。

## 1.3 类做的事：把数据与操作它的函数捆在一起，并规定谁能碰

C++ 给的答案是把这两半合成一个东西，**并在外面加一道门**：

`C++`

```cpp
// stack.h    接口：只有它会被调用方看到
#pragma once

class Stack {
public:                             // 门外的部分：谁都能用
    Stack();
    ~Stack();
    void push(int v);
    bool pop(int &out);
    int  size() const;
private:                            // 门里的部分：只有 Stack 自己能用
    struct Impl;                    // 实现细节连名字都不暴露
    Impl *impl_;
};
```

**调用方看到的就是这五件事**，看不到 `Impl` 里到底放了什么。
于是第 1.2 节的三件麻烦事各有一处着落：

| 麻烦 | 类带来的变化 |
|---|---|
| 内部状态谁都能改 | 放进 `private`，**从外面写它就是编译错误** |
| 换实现要动调用方 | 调用方只认那五个名字；内部换成数组或链表，它都不知道 |
| 复用只能靠复制 | 一份接口可以给多处用，实现只有一份 |

`实测数据`
`C++`

```cpp
// stack_cpp_bad.cpp    编译：g++ -std=c++17 stack_cpp_bad.cpp -o stack_cpp_bad （失败）
class Stack {
    int data_[4] = {0};
    int top_ = 0;                   // 默认私有
};

int main() {
    Stack s;
    s.top_ = 3;                     // 试图从外面改
    return 0;
}
```

`实测数据`
`Text`

```text
error: 'int Stack::top_' is private within this context
```

**同一个赋值，在 C 里合法、在 C++ 里编译不过**——
区别只在于那个成员在不在 `private` 里。

> [!IMPORTANT]
> **封装不是一个新概念，而是一种新的组织方式**：
> 把「数据」与「操作这些数据的函数」放进同一个名字底下，再规定外面能看见哪些。
> **类与 OOP 的价值都从这一条长出来**——后面所有的语法，
> 构造析构也好、继承虚函数也好，都是在回答「这样捆起来之后，随之而来的问题怎么处理」。

---

# 第 2 节 封装换来的四件事

## 2.1 一套标准接口

外部只需要知道 `push`、`pop`、`size` 这三个动作**叫什么、收什么、返回什么**，
不需要知道栈是数组还是链表、容量多大、元素放在哪。

**这就是「按一套标准接口使用组件」的字面意思**：
接口是一份约定，约定之外的细节都属于实现。

## 2.2 解耦：调用方不需要知道实现

`stack.h` 里那一行 `struct Impl;` 只声明了「有这么个东西」，
**没有给出它的任何细节**。因此：

- 调用方的源码里**不出现**数组、链表、节点这些词；
- 实现文件里怎么改，都不会让调用方重新编译出错误；
- 两个人可以同时干活——一个写调用方，一个写实现，中间只靠这份头文件对接。

## 2.3 实现可以替换，而调用方一个字都不用改

这是封装最实在的一条好处，可以直接验：

`C++`

```cpp
// use_stack.cpp    调用方：下面两版实现共用这一份源码
#include <cstdio>
#include "stack.h"

int main() {
    Stack s;
    s.push(1);
    s.push(2);
    s.push(3);
    int v = 0;
    while (s.pop(v)) std::printf("%d ", v);
    std::printf("| size = %d\n", s.size());
    return 0;
}
```

`实测数据`
`Bash`

```bash
g++ -std=c++17 use_stack.cpp stack_array.cpp -o use_array    # 数组实现
g++ -std=c++17 use_stack.cpp stack_list.cpp  -o use_list     # 链表实现
./use_array
./use_list
```

`实测数据`
`Text`

```text
3 2 1 | size = 0
3 2 1 | size = 0
```

**两次链接用的是同一份 `use_stack.cpp`（源码哈希相同），输出也完全相同。**
两个实现文件里，一个是定长数组、一个是带 `new`/`delete` 的链表，
**调用方对此一无所知，也不需要知道**。

> [!IMPORTANT]
> **「实现可以换」这件事，是靠接口与实现分离换来的。**
> 只要那五个名字与签名不变，实现换成什么样子，调用方都不受影响。
> **反过来，一旦接口暴露了实现（比如让调用方拿到数组指针），这条好处立刻消失。**

## 2.4 复用才有基础

**复用的前提是「有一份稳定的东西可以被反复使用」。**
封装恰好提供了这份东西：**接口稳定，实现可以变**。

于是复用的形态从「把代码复制过去」变成了「把接口拿过去用」：

| 复用方式 | 代价 |
|---|---|
| 复制代码 | 每一份都要单独维护，改一处要改多处 |
| 复制接口、共用实现 | 实现只有一份，修一次全体受益 |
| 复制接口、各写实现 | 约定一致，实现可以按场景优化 |

## 2.5 封装的代价

**封装不是免费的**，这一点同样要说清：

| 代价 | 说明 |
|---|---|
| 多了一层间接 | 调用方要经过接口才能碰到数据；`private` 让某些写法变成编译错误 |
| 样板代码 | 声明、定义、构造、析构，都要写 |
| 编译依赖 | 头文件里的改动会让所有包含它的源文件重编 |
| 运行期开销 | 只有虚函数才有（一次查表跳转），普通成员函数没有，见第 08 章 |

**判断标准仍然只有一个：这样做省下的麻烦，是否多于它引入的麻烦。**
一个只有两个 `int` 的坐标点，给它设计五层接口，省下的远少于引入的。

**这条账不止对封装成立**：虚函数、模板、异常、动态分配，
每一样都有自己的价格，第 5 节把它们放在一张表里算。

---

# 第 3 节 类是一种类型，对象是数据加操作

## 3.1 类是一种类型

**「类是一种类型」不是修辞，是一个字面事实**：
凡是类型能出现的地方，类都能出现。

`C++`

```cpp
// is_a_type.cpp    编译：g++ -std=c++17 is_a_type.cpp -o is_a_type
#include <cstdio>
#include <typeinfo>

class Point {
public:
    Point(int x, int y) : x_(x), y_(y) {}
    int   sum() const { return x_ + y_; }                        // 不修改对象
    Point shifted(int d) const { return Point(x_ + d, y_ + d); } // 返回同类对象
private:
    int x_, y_;
};

int take(Point p) { return p.sum(); }        // 当参数类型用

int main() {
    Point p(1, 2);                           // 当变量类型用
    const Point q(3, 4);                     // 能有 const 对象
    std::printf("typeid(p).name() = %s\n", typeid(p).name());
    std::printf("sizeof(Point)    = %zu\n", sizeof(Point));
    std::printf("p.sum()          = %d\n", p.sum());
    std::printf("q.sum()          = %d\n", q.sum());
    std::printf("take(p)          = %d\n", take(p));
    std::printf("p.shifted(10).sum() = %d\n", p.shifted(10).sum());
    return 0;
}
```

`实测数据`
`Text`

```text
typeid(p).name() = 5Point
sizeof(Point)    = 8
p.sum()          = 3
q.sum()          = 7
take(p)          = 3
p.shifted(10).sum() = 23
```

`sizeof(Point)` 是 8——两个 `int`，**与「有两个 `int` 的结构体」量不出区别**；
`typeid` 能报出它的名字；它还能有 `const` 对象、能当参数、能当返回值。

**把类当类型看，后面几件事就顺了**：

| 现象 | 因为类是一种类型，所以…… |
|---|---|
| 类能有 `const` 对象 | 与 `const int` 同源：不能通过它修改对象 |
| 类能做模板参数 | 模板本来就是「对类型做参数化」 |
| 类能重载运算符 | 运算符本来就是为类型准备的操作 |
| 类的名字能参与重载决议 | 与基本类型、指针一样是类型系统里的一员 |

## 3.2 对象是「数据 + 可对它执行的操作」的复合

**先看四个数字**：

`实测数据`
`C++`

```cpp
// sizeof_class.cpp    编译：g++ -std=c++17 sizeof_class.cpp -o sizeof_class
#include <cstdio>

struct Empty { };                                  // 空类
struct WithFuncs {                                 // 只有成员函数
    int add(int a, int b) { return a + b; }
    int sub(int a, int b);
    static int mul(int a, int b) { return a * b; }
};
int WithFuncs::sub(int a, int b) { return a - b; }

struct WithData  { int x; };                       // 一个数据成员
struct ThreeInts { int x, y, z; };                 // 三个数据成员

int main() {
    std::printf("sizeof(Empty)     = %zu\n", sizeof(Empty));
    std::printf("sizeof(WithFuncs) = %zu\n", sizeof(WithFuncs));
    std::printf("sizeof(WithData)  = %zu\n", sizeof(WithData));
    std::printf("sizeof(ThreeInts) = %zu\n", sizeof(ThreeInts));
    return 0;
}
```

`实测数据`
`Text`

```text
sizeof(Empty)     = 1
sizeof(WithFuncs) = 1
sizeof(WithData)  = 4
sizeof(ThreeInts) = 12
```

**两个结论直接摆在数字里**：

- **成员函数不占对象的空间**。`WithFuncs` 里有两个普通成员函数和一个静态成员函数，
  `sizeof` 仍然是 1——**函数属于类，不属于某一个对象**；
- **占空间的只有数据成员**。`Empty` 的 1 字节不是数据，是为了让每个对象有唯一的地址。

**所以「对象」在内存里就是一块数据**（这里 4 字节、12 字节），而
**「可对它执行的操作」是编译期挂在类型上的**，一个字节都不占。

**这一点值得记住，因为它解释了后面很多设计的取舍**：
把函数放进类里之所以不增加成本，正是因为它不在对象里；
而虚函数之所以有成本，正是因为它**必须在对象里存一个指针**（见第 08 章）。

---

# 第 4 节 「面向对象」这四个字

## 4.1 它是手段，不是信仰

**「面向对象」听起来像一种更高级的编程方式，它不是。**

**它是一个组织手段，只在特定形状的问题上划算**：
当程序里有一批数据必须一起维护、并且存在「不能被随便破坏的状态」时，
把数据与操作捆起来、再规定谁能碰，就能把这批状态的责任收在一处。
**问题不满足这个形状，这套手段就没用。**

因此下面这些说法都不成立：

| 说法 | 实际情况 |
|---|---|
| 「一切都应该是对象」 | C++ 里基本类型、函数、模板都不是对象 |
| 「面向对象比面向过程高级」 | 两者解决的问题不同，同一份程序里常常混用 |
| 「不用类就不算好代码」 | 一个坐标点用 `struct` 就够，硬套接口只会让人多绕路 |
| 「继承越多越面向对象」 | 继承是最容易被滥用的手段，见第 07 章 |

**把它当成一个道具**：需要时取用，不需要时放下；
不合手时可以改（C++ 允许你重载运算符、改写拷贝行为、自己管内存），
**它没有神圣之处**。

## 4.2 什么时候值得用一个类

`实测数据`

| 情形 | 例子 |
|---|---|
| 有一组数据必须一起维护，且有「不变量」 | 栈的 `top` 与 `data` 必须一致 |
| 有多套实现需要能互换 | 同一个栈，数组版与链表版 |
| 有资源要成对地获取与释放 | 文件、锁、内存（第 06 章 RAII） |
| 有一批操作天然属于同一个名字 | `Stack::push`、`Point::shift` |

**反过来，这些情形不必用类**：

| 情形 | 用什么 |
|---|---|
| 纯粹的数据打包，没有需要守住的约束 | `struct`（见《04-语法/09-结构体、联合体与 enum.md》第 1 节） |
| 只有一两个动作，没有内部状态 | 普通函数 |
| 同一份逻辑要对多种类型用 | 模板，不是类（第 11、12 章） |
| 只是要把一件事推迟到以后做 | lambda，不是类（第 10 章） |

## 4.3 手段不止一种

**C++ 里解决问题的手段有好几种，它们在同一份代码里共存**：

| 手段 | 适合什么 | 在哪讲 |
|---|---|---|
| 过程式（函数 + 数据） | 步骤清晰的算法 | 第四板块 |
| 泛型（模板） | 同一份逻辑对多种类型用 | 第 11、12 章 |
| 面向对象（类 + 继承 + 虚函数） | 运行期需要替换实现 | 第 07、08 章 |
| 函数式（lambda、算法） | 把「做什么」当参数传进来 | 第 10 章 |

**不是非此即彼。** 一个真实的 C++ 程序里，这四种写法常常同时出现在一个文件里，
**选择的标准是哪一个更直接，而不是哪一个更「现代」。**

## 4.4 一个先记住的倾向

**组合优于继承。**

继承是最容易被滥用的手段：它把两个类的实现绑在一起，
基类一改，派生类可能就坏。**能用「有一个」解决的事，不要用「是一个」。**
这一条在第 07 章展开。

---

# 第 5 节 抽象不是免费的

**这一板块的每一样东西都有价格。** 这句话要在动手之前就知道，而不是踩过之后。

## 5.1 每一层抽象都在暗中标了价

**价格主要付在时间上，而不是空间上。**

空间的开销在 `sizeof` 里一眼就能看见；时间的开销藏在两处——
**机器码里多出来的指令**，以及**运行时替你做的事**（启动、建栈帧、查表、展开）。
后者不在你的源码里，但它的时间算在你的程序上。

**先看空间上能直接测出来的那部分。**

`实测数据`

| 抽象 | 换来了什么 | 价格 |
|---|---|---|
| 封装（`private` 加接口） | 不变量管得住、实现可以换 | 一层间接、样板代码、头文件带来的编译依赖 |
| **虚函数**（第 08 章） | 运行期替换实现 | **每个对象多一个指针**，每次调用多一次查表，通常还无法内联 |
| 模板（第 11、12 章） | 一份代码对多种类型 | 代码膨胀、编译时间、报错难读 |
| lambda 与 `std::function`（第 10 章） | 就地写函数、能当值传递 | 一个类对象；`std::function` 还多一次间接调用 |
| 异常（《04-语法/13-异常.md》） | 错误不能被忽略 | 栈展开路径的代价 |
| `new`（第 04 章） | 生存期不受作用域限制 | 分配器开销、泄漏风险、缓存局部性变差 |

**虚函数那一行的价格可以直接测出来**：

`实测数据`
`C++`

```cpp
/* cost_of_virtual.cpp    编译：g++ -std=c++17 -O2 cost_of_virtual.cpp -o cost_of_virtual */
#include <cstdio>

struct Empty { };                                    // 空类
struct OneInt { int x; };                            // 一个 int
struct WithFunc { int x; void f() {} };              // 普通成员函数：不进对象
struct WithVirtual { int x; virtual void f() {} };   // 虚函数：对象里多一个指针

int main() {
    std::printf("sizeof(Empty)       = %zu\n", sizeof(Empty));
    std::printf("sizeof(OneInt)      = %zu\n", sizeof(OneInt));
    std::printf("sizeof(WithFunc)    = %zu\n", sizeof(WithFunc));
    std::printf("sizeof(WithVirtual) = %zu\n", sizeof(WithVirtual));
    return 0;
}
```

`实测数据`
`Text`

```text
sizeof(Empty)       = 1
sizeof(OneInt)      = 4
sizeof(WithFunc)    = 4
sizeof(WithVirtual) = 16
```

**同样只有一个 `int`**：写成普通成员函数是 **4 字节**，加一个虚函数就成了 **16 字节**。
多出来的 12 字节不是数据，是**每个对象**都要背的一本账
（一个指向虚函数表的指针，加上对齐填充）。
**一万个这种对象，就是十万字节的额外内存。**

**同一批实验里也有「免费」的项**——账要一项一项算，不能笼统地说「用类很贵」：

`实测数据`

| 写法 | 有没有代价 | 依据 |
|---|---|---|
| 普通成员函数 | **没有** | 与等价自由函数的汇编逐条相同（第 02 章第 2.1 小节） |
| 类内定义还是类外定义 | **没有** | 两者的指令序列相同（《05-类与面向对象/02-类是一种类型.md》第 1.4 小节） |
| 用类代替结构体 | **没有** | `sizeof` 只算数据成员（第 3.2 小节） |
| 虚函数 | **有** | 上面那组 `sizeof` 数字 |
| 间址调用（函数指针、虚函数） | **有** | `call *%r8` 与 `call f` 的差别（《04-语法/08-数组、指针与引用.md》第 4.4 小节） |
| 手写移位代替乘除 | **没有收益** | 《04-语法/15-奇技淫巧与优化.md》第 2 节 |

**再看时间——大头在这里。**

**一、一次普通函数调用，光进出栈就要好几条指令**：

`实测数据`
`Assembly`

```asm
add(int, int)          ; -O0
  push   %rbp
  mov    %rsp,%rbp
  mov    %ecx,0x10(%rbp)
  mov    %edx,0x18(%rbp)
  mov    0x10(%rbp),%edx
  mov    0x18(%rbp),%eax
  add    %edx,%eax      ; 只有这一条是干活
  pop    %rbp
  ret
```

**八条指令里只有一条在做加法**，其余都是建栈帧、把参数搬到栈上再搬回来。
（`-O2` 会把这些全部消掉，见《04-语法/15-奇技淫巧与优化.md》；
但每个**没被内联**的调用都要付这笔钱。）

**二、虚函数调用是「取指针、查表、间接跳」**：

`实测数据`
`Assembly`

```asm
call_virtual(Base*)    ; -O0
  ...
  mov    (%rax),%rax    ; 取虚表指针
  mov    (%rax),%rdx    ; 查表，取出函数地址
  call   *%rdx          ; 间接调用
  ...

call_direct(Plain*)    ; -O0
  ...
  call   3d <call_direct(Plain*)+0x18>    ; 直接调用
  ...
```

**三点差别**：多两次读内存、多一次间接跳转，而且**编译器没法内联它**——
内联要求目标在编译期就确定，而 `*%rdx` 里的地址要到运行时才知道。

**编译器有时能把它认回来**：`-O2` 下会先比较虚表指针，确认是已知类型就走直接路径
（实测汇编里出现了 `cmp %rdx,%rax` 与 `jne`）。
**但那是编译器的功劳，不能当成「虚函数不花钱」的证据。**

**三、异常要牵进一整套运行时**：

`实测数据`

| 同一个简单函数 | `.text` 大小 | 额外牵进来的东西 |
|---|---|---|
| 不带 `throw` | 16 字节 | — |
| 带 `throw` | **64 字节** | 展开表（Windows 上是 `.pdata`／`.xdata`）、对 `int` 类型信息的引用、以及 `__cxa_throw` 与 `__cxa_allocate_exception` 两个运行时函数 |

**四、全局对象的构造不由 `main` 负责，由运行时在启动时做**：

`实测数据`
`C++`

```cpp
/* crt_init.cpp    编译：g++ -std=c++17 crt_init.cpp -o crt_init */
#include <cstdio>

struct Tracer {
    Tracer() { std::printf("构造（在 main 之前）\n"); }
};
Tracer g;                       // 全局对象

int main() {
    std::printf("main\n");
    return 0;
}
```

`实测数据`
`Text`

```text
构造（在 main 之前）
main
```

`实测数据`

| 看哪里 | 看到什么 |
|---|---|
| `nm crt_init.o` | `_GLOBAL__sub_I_g`——编译器生成的初始化函数 |
| `objdump -h` | `.ctors` 段，8 字节，是一张函数指针表 |
| 运行 | 那行输出确实在 `main` 之前 |

**程序还没进 `main`，运行时已经在按表逐个调用构造函数了。**
那段启动代码叫 **CRT（C Runtime）**，它不在你的源码里，
但它做的事、花的时间，都算在你的程序上。
**CRT 的细节归 `06-更底层` 板块**（待补）。

> [!IMPORTANT]
> **抽象的价格主要付在时间上**：建栈帧、查表、间接跳转、栈展开、启动时的初始化。
> **空间上的那点开销在 `sizeof` 里看得见，时间上的看不见**——
> 正因为看不见，它才最容易被忽略。

## 5.2 「优雅」是结果，不是目标

**代码不是越短越好，也不是抽象层次越多越好。**

同一个需求，多写一层基类、多包一层接口、多引入一个模式，
读的人就要多跳几次才能看到真正干活的那一行。
**如果这一层没有换来实际的东西**（能换实现、能复用、能挡住错误），
**它就是纯支出**。

> [!IMPORTANT]
> **判断一个抽象值不值，只问一个问题：它换来的东西，值不值它的价格。**
> **这个问题没有统一答案。** 同一个设计，在每秒调用上亿次的热路径上不值，
> 在一天跑一次的脚本里很值。**账只能在具体场景里算。**

## 5.3 编程是取舍的艺术

**没有「最好的写法」，只有「在这里更合适的写法」。**

`实测数据`

| 机制 | 什么时候值 | 什么时候不值 |
|---|---|---|
| 虚函数 | 运行期确实需要换实现（插件、异构容器） | 类型在编译期就定死，且调用频率以亿计 |
| 异常 | 错误必须无法被忽略（构造函数报告失败） | 不能接受栈展开代价的场合 |
| 动态分配 | 对象要活过创建它的作用域 | 能待在栈上就待在栈上（第 04 章） |
| 模板 | 同一份逻辑要对多种类型用 | 只有一两种类型，写两份反而更清楚 |
| 继承 | 「是一个」的关系真的成立 | 只是想复用几行代码（用组合，第 4.4 小节） |

**取舍时问三个问题**：

| 问题 | 说明 |
|---|---|
| **贵在哪** | 是每个对象一份（空间）、每次调用一份（时间），还是每次编译一份（构建时间） |
| **值不值** | 换来的东西，在这个场景里真的用得上吗 |
| **能不能回头** | 改起来要动多少地方 |

**第三条常被忽略，权重却往往最大**：实现收敛在内部，随时能换；
一旦细节泄到调用方，改起来就是重写。
**封装真正的价值就在这里**——它让「回头」这件事一直保留着。

# 第 6 节 这一板块与其它板块的分工

| 内容 | 在哪 |
|---|---|
| 命名空间、`::`、`using` | 《05-类与面向对象/01-命名空间与 using.md》 |
| 类、构造析构、拷贝移动、运算符重载、继承多态、模板、lambda | **本板块** |
| `std::string`、智能指针、`std::function`、输入输出、时间、文件系统 | 《07-标准库/README.md》章节 |
| STL 容器、迭代器、算法 | 【待补：08-高阶数据结构/】 |
| 虚函数表的二进制布局、名字修饰规则、对象内存布局 | 《06-更底层/09-C++ 对象布局与它的硬件代价.md》章节、《06-更底层/05-ABI 与调用约定.md》章节 |
| **CRT（C Runtime）**：程序启动、全局对象初始化、异常的运行时支持 | 《06-更底层/08-CRT 与程序启动.md》章节 |
| `constexpr`、`static_assert`、编译期计算 | 《04-语法/12-编译期能力.md》 |
| `const` 的完整语义 | 《04-语法/03-常量与 const.md》 |
| 异常与栈展开 | 《04-语法/13-异常.md》 |

**本板块只讲「有虚表、虚表里存什么」这一层**，
虚表在二进制里怎么排、名字怎么修饰，归 `06-更底层`。

---

# 相邻节点

| 关系 | 文档 |
|---|---|
| **前置**：结构体与 `struct` | 《04-语法/09-结构体、联合体与 enum.md》第 1 节 |
| **前置**：引用 | 《04-语法/08-数组、指针与引用.md》第 3 节 |
| **前置**：函数与重载 | 《04-语法/07-函数.md》第 5 节 |
| **前置**：`const` 的语义 | 《04-语法/03-常量与 const.md》第 2 节 |
| **前置**：生存期与 `static` | 《04-语法/11-作用域、生存期与链接.md》第 3 节 |
| **前置**：异常与栈展开 | 《04-语法/13-异常.md》第 2 节 |
| **前置**：读法与本教材的立场 | 《04-语法/00-导读：从记住规则到理解意图.md》 |
| **后续**：`std::string`、智能指针 | 《07-标准库/B-02-std-string 与 string_view.md》章节、《07-标准库/B-03-智能指针的用法.md》章节 |
| **后续**：容器、迭代器、算法 | 【待补：08-高阶数据结构/】 |
| **后续**：ABI、名字修饰、编译器扩展 | 《06-更底层/05-ABI 与调用约定.md》章节、《06-更底层/12-编译器扩展与未定义行为.md》章节 |

---

# 附录 A 复现本章节实测

## A.1 没有封装时：数据与操作分成两半

`C++`

```cpp
/* stack_plain.cpp    编译：g++ -std=c++17 -c stack_plain.cpp -o stack_plain.o */
#include <cstdio>

struct Stack { int data[4]; int top; };

void stack_init(Stack *s) { s->top = 0; }

int stack_push(Stack *s, int v) {
    if (s->top >= 4) return 0;
    s->data[s->top++] = v;
    return 1;
}

int stack_pop(Stack *s, int *out) {
    if (s->top <= 0) return 0;
    *out = s->data[--s->top];
    return 1;
}
```

`C++`

```cpp
/* stack_broken.cpp */
#include <cstdio>

struct Stack { int data[4]; int top; };

void stack_init(Stack *s) { s->top = 0; }
int stack_push(Stack *s, int v) {
    if (s->top >= 4) return 0;
    s->data[s->top++] = v;
    return 1;
}
int stack_pop(Stack *s, int *out) {
    if (s->top <= 0) return 0;
    *out = s->data[--s->top];
    return 1;
}

int main() {
    Stack s = {};
    stack_init(&s);
    stack_push(&s, 1);
    stack_push(&s, 2);

    s.top = 3;                      // 直接改内部字段：声称有三个元素

    int v = -1;
    stack_pop(&s, &v);
    std::printf("弹出的值是 %d\n", v);
    return 0;
}
```

`Bash`

```bash
g++ -std=c++17 -Wall stack_broken.cpp -o stack_broken && ./stack_broken
```

## A.2 C++ 版：从外面改私有成员

`C++`

```cpp
/* stack_cpp_bad.cpp    编译：g++ -std=c++17 stack_cpp_bad.cpp -o stack_cpp_bad （失败） */
class Stack {
    int data_[4] = {0};
    int top_ = 0;                   // 默认私有
};

int main() {
    Stack s;
    s.top_ = 3;
    return 0;
}
```

## A.3 同一份调用方，两版实现

`C++`

```cpp
/* stack.h    编译：g++ -std=c++17 -c stack.h -o stack_h.o */
#pragma once

class Stack {
public:
    Stack();
    ~Stack();
    void push(int v);
    bool pop(int &out);
    int  size() const;
private:
    struct Impl;
    Impl *impl_;
};
```

`C++`

```cpp
/* stack_array.cpp    编译：g++ -std=c++17 -c stack_array.cpp -o stack_array.o */
#include "stack.h"

struct Stack::Impl {
    int data[8];
    int top = 0;
};

Stack::Stack() : impl_(new Impl) {}
Stack::~Stack() { delete impl_; }

void Stack::push(int v) { if (impl_->top < 8) impl_->data[impl_->top++] = v; }
bool Stack::pop(int &out) {
    if (impl_->top <= 0) return false;
    out = impl_->data[--impl_->top];
    return true;
}
int Stack::size() const { return impl_->top; }
```

`C++`

```cpp
/* stack_list.cpp    编译：g++ -std=c++17 -c stack_list.cpp -o stack_list.o */
#include "stack.h"

struct Stack::Impl {
    struct Node { int v; Node *next; };
    Node *head = nullptr;
    int n = 0;
};

Stack::Stack() : impl_(new Impl) {}
Stack::~Stack() {
    while (impl_->head) {
        Impl::Node *p = impl_->head;
        impl_->head = p->next;
        delete p;
    }
    delete impl_;
}

void Stack::push(int v) {
    impl_->head = new Impl::Node{v, impl_->head};
    ++impl_->n;
}
bool Stack::pop(int &out) {
    if (!impl_->head) return false;
    Impl::Node *p = impl_->head;
    out = p->v;
    impl_->head = p->next;
    delete p;
    --impl_->n;
    return true;
}
int Stack::size() const { return impl_->n; }
```

`C++`

```cpp
/* use_stack.cpp    编译：g++ -std=c++17 -c use_stack.cpp -o use_stack.o */
#include <cstdio>
#include "stack.h"

int main() {
    Stack s;
    s.push(1);
    s.push(2);
    s.push(3);
    int v = 0;
    while (s.pop(v)) std::printf("%d ", v);
    std::printf("| size = %d\n", s.size());
    return 0;
}
```

`Bash`

```bash
g++ -std=c++17 use_stack.cpp stack_array.cpp -o use_array && ./use_array
g++ -std=c++17 use_stack.cpp stack_list.cpp  -o use_list  && ./use_list
```

`实测数据`
`Text`

```text
3 2 1 | size = 0
3 2 1 | size = 0
```

**两次用的 `use_stack.cpp` 是同一份**（源码 SHA-256 相同），
差别只在链接时给了哪个实现文件。

## A.4 成员函数不占对象空间

`C++`

```cpp
/* sizeof_class.cpp */
#include <cstdio>

struct Empty { };
struct WithFuncs {
    int add(int a, int b) { return a + b; }
    int sub(int a, int b);
    static int mul(int a, int b) { return a * b; }
};
int WithFuncs::sub(int a, int b) { return a - b; }

struct WithData  { int x; };
struct ThreeInts { int x, y, z; };

int main() {
    std::printf("sizeof(Empty)     = %zu\n", sizeof(Empty));
    std::printf("sizeof(WithFuncs) = %zu\n", sizeof(WithFuncs));
    std::printf("sizeof(WithData)  = %zu\n", sizeof(WithData));
    std::printf("sizeof(ThreeInts) = %zu\n", sizeof(ThreeInts));
    return 0;
}
```

## A.5 类是一种类型

`C++`

```cpp
/* is_a_type.cpp */
#include <cstdio>
#include <typeinfo>

class Point {
public:
    Point(int x, int y) : x_(x), y_(y) {}
    int   sum() const { return x_ + y_; }
    Point shifted(int d) const { return Point(x_ + d, y_ + d); }
private:
    int x_, y_;
};

int take(Point p) { return p.sum(); }

int main() {
    Point p(1, 2);
    const Point q(3, 4);
    std::printf("typeid(p).name() = %s\n", typeid(p).name());
    std::printf("sizeof(Point)    = %zu\n", sizeof(Point));
    std::printf("p.sum()          = %d\n", p.sum());
    std::printf("q.sum()          = %d\n", q.sum());
    std::printf("take(p)          = %d\n", take(p));
    std::printf("p.shifted(10).sum() = %d\n", p.shifted(10).sum());
    return 0;
}
```

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《04-语法/00-导读：从记住规则到理解意图.md》 | **前置**：本教材的读法与立场 |
| 《04-语法/09-结构体、联合体与 enum.md》第 1 节 | **前置**：结构体是类的直接前身 |
| 《04-语法/08-数组、指针与引用.md》第 3 节 | **前置**：引用（成员函数与传参都要用） |
| 《04-语法/07-函数.md》第 5 节 | **前置**：函数与重载 |
| 《04-语法/03-常量与 const.md》第 2 节 | **前置**：`const` 的语义（`const` 对象、`const` 成员函数） |
| 《04-语法/11-作用域、生存期与链接.md》第 3 节 | **前置**：生存期（构造与析构要对上它） |
| 《04-语法/13-异常.md》第 2 节 | **前置**：栈展开（析构函数在异常里的角色） |
| 《07-标准库/B-02-std-string 与 string_view.md》章节、《07-标准库/B-03-智能指针的用法.md》章节、《07-标准库/B-04-可调用物的包装.md》章节 | **后续**：`std::string`、智能指针、`std::function` |
| 【待补：08-高阶数据结构/】 | **后续**：容器、迭代器、算法 |
| 《06-更底层/05-ABI 与调用约定.md》章节、《06-更底层/09-C++ 对象布局与它的硬件代价.md》章节 | **后续**：ABI、名字修饰、对象布局 |
