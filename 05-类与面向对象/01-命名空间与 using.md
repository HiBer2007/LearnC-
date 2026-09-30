# 命名空间与 using

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**名字会撞。** 两个库各自的 `print` 一旦碰面，
轻则调用到错的那个，重则链接直接失败。
**命名空间解决的就是这件事：给名字加一层「谁家的」。**

**`::` 与 `using` 做的是相反的动作**：

| 动作 | 工具 | 说明 |
|---|---|---|
| **指路** | `::` | 说清这个名字属于谁——读作「的」 |
| **搬家** | `using` | 把别处的名字引到本文件里使用 |

`std::printf` 里的 `std::` 从前面几个板块起就一直在用，
**它和 `Stack::push` 里的 `Stack::` 是同一个符号**，
两种用法的完整规则见第 2 节。

**C 里没有这个机制。** C 只能靠给函数名加前缀（`liba_print`、`libb_print`）避开冲突，
名字长，前缀规则也只能靠人工维持。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整源码与复现命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 问题 | 在哪一节 |
|---|---|
| **名字为什么会撞、怎么解决** | **第 1 节** |
| **`::` 读作「的」——四种用法** | **第 2 节** |
| **`using` 的四种形式** | **第 3 节** |
| 匿名命名空间与 `static` 的分工 | 第 4 节 |
| 嵌套、`inline`、版本化与 `using Base::f;` | 第 5 节 |

---

# 第 1 节 名字为什么会撞

## 1.1 两个库各有一个 `print`

假设两个库各自都有一个叫 `print` 的函数：

`C++`

```cpp
/* lib_a.h    编译：g++ -std=c++17 -c lib_a.h -o lib_a_h.o */
#pragma once
void print();
```

`C++`

```cpp
/* lib_b.h    编译：g++ -std=c++17 -c lib_b.h -o lib_b_h.o */
#pragma once
void print();
```

`C++`

```cpp
/* lib_a.cpp    编译：g++ -std=c++17 -c lib_a.cpp -o lib_a.o */
#include "lib_a.h"
#include <cstdio>
void print() { std::printf("A 的 print\n"); }
```

`C++`

```cpp
/* lib_b.cpp    编译：g++ -std=c++17 -c lib_b.cpp -o lib_b.o */
#include "lib_b.h"
#include <cstdio>
void print() { std::printf("B 的 print\n"); }
```

**两边的源码都没有错**，单独编译也都通过；一起链接就会失败：

`C++`

```cpp
/* e1_use.cpp    编译：g++ -std=c++17 -c e1_use.cpp -o e1_use.o */
#include "lib_a.h"
int main() { print(); return 0; }
```

`实测数据`
`Bash`

```bash
g++ -std=c++17 -c lib_a.cpp -o lib_a.o
g++ -std=c++17 -c lib_b.cpp -o lib_b.o
g++ -std=c++17 e1_use.cpp lib_a.o lib_b.o -o e1_use     # 链接失败
```

`实测数据`
`Text`

```text
multiple definition of `print()'; lib_a.o:lib_a.cpp:(.text+0x0): first defined here
collect2.exe: error: ld returned 1 exit status
```

**链接器的工作就是把名字对上**：现在有两个 `print` 都声称自己叫这个名字，
链接器没有依据选择其中一个。**报错说的是「定义了多次」，根子是「名字撞了」。**

> [!CAUTION]
> **两个库各自定义同一个全局名字，链接必定失败。**
> 这类 `multiple definition` 的根因不是语法错，而是名字撞在了同一个全局作用域里；
> 每个源文件单独编译都不报错，问题要到链接期才暴露。

**即使侥幸通过链接，问题也没有消失**：`main` 里那句 `print()` 该调用哪一个，
从源码上无法判断。

## 1.2 命名空间：给名字加一层「谁家的」

**做法很直接：把名字放进一个命名空间里。**

`C++`

```cpp
/* ns_lib_a.h    编译：g++ -std=c++17 -c ns_lib_a.h -o ns_lib_a_h.o */
#pragma once
namespace liba { void print(); }
```

`C++`

```cpp
/* ns_lib_b.h    编译：g++ -std=c++17 -c ns_lib_b.h -o ns_lib_b_h.o */
#pragma once
namespace libb { void print(); }
```

两个实现文件在各自的命名空间里定义：

`C++`

```cpp
/* ns_a.cpp    编译：g++ -std=c++17 -c ns_a.cpp -o ns_a.o */
#include "ns_lib_a.h"
#include <cstdio>
namespace liba { void print() { std::printf("liba 的 print\n"); } }
```

`C++`

```cpp
/* ns_b.cpp    编译：g++ -std=c++17 -c ns_b.cpp -o ns_b.o */
#include "ns_lib_b.h"
#include <cstdio>
namespace libb { void print() { std::printf("libb 的 print\n"); } }
```

**调用时把「谁家的」写出来**：

`C++`

```cpp
/* e2_qualified.cpp    编译：g++ -std=c++17 -c e2_qualified.cpp -o e2_qualified.o */
#include "ns_lib_a.h"
#include "ns_lib_b.h"
int main() {
    liba::print();          // 用「谁家的」说清楚
    libb::print();
    return 0;
}
```

`实测数据`
`Bash`

```bash
g++ -std=c++17 -c ns_a.cpp -o ns_a.o
g++ -std=c++17 -c ns_b.cpp -o ns_b.o
g++ -std=c++17 e2_qualified.cpp ns_a.o ns_b.o -o e2_qualified && ./e2_qualified
```

`实测数据`
`Text`

```text
liba 的 print
libb 的 print
```

**同一个 `print` 的两个定义都保留了下来，没有互相覆盖。**
`liba::print` 里的 `::` 就是「的」——**`liba` 的 `print`**。

## 1.3 命名空间改的是符号名

**这不是给编译器看的注释，它真的进了目标文件。** 用 `nm` 看那两个实现文件：

`实测数据`
`Bash`

```bash
nm ns_a.o | grep print
nm ns_b.o | grep print
```

`实测数据`
`Text`

```text
ns_a.o：0000000000000000 T _ZN4liba5printEv
ns_b.o：0000000000000000 T _ZN4libb5printEv
```

**两个符号名不一样**（`4liba` 与 `4libb`），链接器不再遇到冲突。
名字怎么修饰的规则见《01-编译器/00-语言的实现.md》第 5.6 小节。

> [!IMPORTANT]
> **命名空间是「名字的一部分」，不是「存放实体的容器」。**
> 它不占内存、不影响布局，只在**编译期**决定一个名字的全名是什么。
> 因此它没有任何运行期开销——在《05-类与面向对象/00-导读：类与 OOP 是手段.md》
> 第 5 节那张「抽象的价格」账本里，它是少数几项真正免费的开销之一。

---

# 第 2 节 `::`：读作「的」

> [!IMPORTANT]
> **`::` 读作「的」：`A::b` 就是「`A` 的 `b`」。**
> 「作用域解析运算符」这个术语不易记住，但它回答的永远是同一个问题——
> 这个名字属于谁。

## 2.1 四种用法

`C++`

```cpp
/* scope_forms.cpp    编译：g++ -std=c++17 scope_forms.cpp -o scope_forms */
#include <cstdio>

int g = 1;                          // 全局

namespace outer {
    int g = 2;
    namespace inner { int g = 3; }
}

class Point {
public:
    static int count;               // 类的静态成员
    int x = 0;
    int get() const;
};
int Point::count = 5;               // 定义在类外，用 Point:: 指明属于谁
int Point::get() const { return x; }

int main() {
    int g = 4;                      // 局部变量遮住了全局那个
    std::printf("局部 g          = %d\n", g);
    std::printf("全局 ::g        = %d\n", ::g);
    std::printf("outer::g        = %d\n", outer::g);
    std::printf("outer::inner::g = %d\n", outer::inner::g);
    std::printf("Point::count    = %d\n", Point::count);
    Point p;
    std::printf("p.get()         = %d\n", p.get());
    return 0;
}
```

`实测数据`
`Text`

```text
局部 g          = 4
全局 ::g        = 1
outer::g        = 2
outer::inner::g = 3
Point::count    = 5
p.get()         = 0
```

`实测数据`

| 写法 | 读作 | 用在哪 |
|---|---|---|
| `std::printf` | `std` 的 `printf` | 命名空间里的名字 |
| `outer::inner::g` | `outer` 的 `inner` 的 `g` | 嵌套命名空间 |
| `Point::count` | `Point` 的 `count` | 类的静态成员 |
| `Point::get` | `Point` 的 `get` | 成员函数定义在类外时（第 02 章展开） |
| `::g` | **全局的 `g`** | 开头的 `::` 表示「从最外层开始找」 |

> [!WARNING]
> **`::` 不是类专用的符号。**
> 命名空间、嵌套命名空间、全局作用域、类的静态成员，用的都是它；
> 只见过 `Point::get` 这类写法，容易以为它只属于类。

**最后一行最容易被忽略**：局部变量 `g` 把全局的 `g` 遮住了，
**在它前面加一个 `::` 就能取到最外层那一个**。

## 2.2 就近原则：不加限定时的查找顺序

**不加 `::` 时，编译器从里往外查找，找到第一个就停止**：

`实测数据`
`Text`

```text
   局部作用域        ← 最先查找
        │
   外层作用域
        │
   全局作用域        ← 最后查找
```

**这正是上面 `g` 打印出 4 和 1 的原因**：
不加限定取到的是**最近的**那个（局部 4）；
`::g` 是**明确要求从最外层开始**，因此取到 1。

**同一个名字在不同层次存在是完全合法的**——它们是不同的实体，
只是在不同的作用域里恰好用了同一个拼写。
作用域本身的规则见《04-语法/11-作用域、生存期与链接.md》第 2 节。

## 2.3 类里的 `::`

**类名后面加 `::`，表示「这个类的」**——类定义里只写声明，定义放到外面时就要这样写。
类的写法在第 02 章展开，这里先把符号的读法立住：

`C++`

```cpp
int Point::get() const { return x; }    // get 是 Point 的
int Point::count = 5;                   // count 是 Point 的静态成员
```

**这两个写法的共同点是：它们都出现在类定义的外面。**
类定义里只写声明，定义放在外面，就得用 `::` 说清它属于哪个类。

> [!TIP]
> **读代码时把 `::` 念成「的」，长名字就不难了**：
> `std::vector<int>::iterator` 读作「`std` 的 `vector<int>` 的 `iterator`」。
> **看见一串 `::`，就是从外往里一层层点进去。**

---

# 第 3 节 `using`：把名字引进来

**`::` 要求每次写出全名；`using` 先引进来，此后不必再写全名。**

**它有四种形式，其中只有两种与命名空间有关**，用途与后果各不相同，
这也是它容易用错的原因：

`实测数据`

| 形式 | 引进来什么 | 常见场合 |
|---|---|---|
| `using std::printf;` | **一个**名字 | 源文件里用某几个名字 |
| `using namespace std;` | **整个命名空间的所有名字** | 小程序、示例；**头文件里不要写** |
| `using Alias = T;` | 给类型起个别名 | 简写长类型名 |
| `using Base::f;` | 把基类被隐藏的名字**引回派生类** | 继承（第 06 章） |

> [!WARNING]
> **`using std::printf;` 与 `using namespace std;` 只差一个 `namespace`，
> 效果却完全不同。**
> 前者引进一个名字，后者把整个空间的名字都变成候选；
> 把两者当成同一件事，是 `using` 用错的主要来源。

## 3.1 `using` 声明：引一个名字

`using liba::print;` 之后，本文件里写 `print` 就是 `liba::print`。

**它只引进指定的那一个名字**，其余仍然要写限定名：

`C++`

```cpp
/* using_decl.cpp    编译：g++ -std=c++17 using_decl.cpp -o using_decl */
#include <cstdio>
#include <vector>

using std::printf;                  // 只引 printf 这一个

int main() {
    printf("引进来就能直接用\n");     // 不用写 std::
    std::vector<int> v{1, 2, 3};    // vector 没引，仍然要写 std::
    printf("v.size() = %zu\n", v.size());
    return 0;
}
```

`实测数据`
`Text`

```text
引进来就能直接用
v.size() = 3
```

> [!IMPORTANT]
> **`using` 引进来的是「名字」，不是「代码」。**
> 目标文件里调用的仍然是 `std::printf`，
> 它只在本文件里省去几个字符的书写。

## 3.2 `using namespace`：引一屋子名字

`using namespace liba;` 之后，`liba` 里**所有**名字都可以不加限定地写。

**方便，代价是重名冲突的机会变大。** 两个命名空间都引进来之后，直接写 `print`：

`C++`

```cpp
/* e2_ambiguous.cpp    编译：g++ -std=c++17 e2_ambiguous.cpp ns_a.o ns_b.o -o e2_ambiguous （失败） */
#include "ns_lib_a.h"
#include "ns_lib_b.h"
using namespace liba;       // 两家的名字都放进来
using namespace libb;
int main() {
    print();                // 两个候选，无法确定调用哪一个
    return 0;
}
```

`实测数据`
`Text`

```text
e2_ambiguous.cpp:7:10: error: call of overloaded 'print()' is ambiguous
```

**报错出现在调用处**：`print` 这个名字对应两个候选，编译器不会替调用者选择。

> [!CAUTION]
> **同时打开两个命名空间，重名的调用一律报 `is ambiguous`。**
> 编译器不会替调用者挑选候选，只能写限定名把两个名字区分开；
> 报错出在调用处，根因在前面的 `using namespace`。

**换成两个 `using` 声明，结果一样**：

`C++`

```cpp
/* e2_usingdecl.cpp    编译：g++ -std=c++17 e2_usingdecl.cpp ns_a.o ns_b.o -o e2_usingdecl （失败） */
#include "ns_lib_a.h"
#include "ns_lib_b.h"
using liba::print;          // 只把 liba 的 print 引进来
using libb::print;          // 再引 libb 的
int main() {
    print();
    return 0;
}
```

`实测数据`
`Text`

```text
e2_usingdecl.cpp:7:10: error: call of overloaded 'print()' is ambiguous
```

**两种写法都会「引入冲突、在使用处报错」。** 它们的差别不在报错时机，而在**引入的范围**：

`实测数据`

| | `using std::printf;` | `using namespace std;` |
|---|---|---|
| 引进来 | 一个名字 | 那个空间里**所有**名字 |
| 会不会影响别人 | 不会 | 会——本文件里所有不加限定的名字都多了一批候选 |
| 遇到同名 | 说明这两个名字确实冲突 | 可能只是恰好同名；空间里其余大量名字一并成为候选，从调用处无法察觉 |

> [!TIP]
> **优先用 `using` 声明。**
> 声明的名字数目明确，冲突时可以直接看出是哪两个；
> 指令引进的是一整片候选，牵连到的名字往往与本意无关。

## 3.3 头文件里不要写 `using namespace`

**头文件会被别人包含**，其中写一句 `using namespace std;`，
**所有包含它的人都被迫接受这一屋子名字**——包括他们自己写的名字。

`C++`

```cpp
/* bad_ns.h    编译：g++ -std=c++17 -c bad_ns.h -o bad_ns_h.o */
#pragma once
#include <algorithm>
using namespace std;
```

调用方自己写了一个 `min`：

`C++`

```cpp
/* e6_min.cpp    编译：g++ -std=c++17 e6_min.cpp -o e6_min （失败） */
#include "bad_ns.h"        // 里面有 using namespace std;

template <class T>          // 调用方自己写的 min
T min(T a, T b) { return a < b ? a : b; }

int main() {
    return min(1, 2);       // 与 std::min 撞了
}
```

`实测数据`
`Text`

```text
e6_min.cpp:8:15: error: call of overloaded 'min(int, int)' is ambiguous
```

**报错在调用方的文件里，根因却在头文件里。** 同一份 `min`，不包含那个头文件就不报错：

`C++`

```cpp
/* e6_min_ok.cpp    编译：g++ -std=c++17 e6_min_ok.cpp -o e6_min_ok */
template <class T>
T min(T a, T b) { return a < b ? a : b; }

int main() {
    return min(1, 2);
}
```

`实测数据`
`Text`

```text
（编译通过）
```

> [!CAUTION]
> **头文件里不要写 `using namespace`，一个也不要。**
> 头文件是给别人用的；在里面打开一个命名空间，
> 等于**替所有使用者做了一次他们没同意的选择**。
> **源码文件（`.cpp`）里可以用**——影响范围只限于本文件。
> 头文件里该放什么，见《04-语法/14-预处理器.md》第 3.3 小节。

## 3.4 `using Alias = T;`：给类型起别名

> [!WARNING]
> **`using Alias = T;` 与命名空间无关。**
> 它是给类型起别名的写法（C++11 起），同样以 `using` 开头，
> 因此常被误当成命名空间的用法。

`C++`

```cpp
/* alias.cpp    编译：g++ -std=c++17 alias.cpp -o alias */
#include <cstdio>
#include <typeinfo>

using MyInt  = int;             // 别名（C++11 起）
typedef int  OldInt;            // 老写法

using Callback = void (*)(int); // 函数指针的别名，老写法很难读

void handler(int v) { std::printf("handler(%d)\n", v); }

int main() {
    MyInt a = 1;
    OldInt b = 2;
    Callback cb = handler;
    std::printf("a = %d, b = %d\n", a, b);
    cb(7);
    std::printf("两种写法是同一个类型：%s\n",
                (typeid(MyInt).name() == typeid(OldInt).name()) ? "是" : "否");
    return 0;
}
```

`实测数据`
`Text`

```text
a = 1, b = 2
handler(7)
两种写法是同一个类型：是
```

**`using` 与 `typedef` 完全等价**（`typeid` 相同），
但 `using` 的可读性在复杂类型上明显更好：

`实测数据`

| 类型 | `typedef` 写法 | `using` 写法 |
|---|---|---|
| 函数指针 | `typedef void (*Callback)(int);` | `using Callback = void (*)(int);` |
| 函数指针数组 | `typedef void (*Handlers[4])(int);` | `using Handlers = void (*[4])(int);` |

**读 `typedef` 时要先找到名字的位置**，读 `using` 时**名字永远在等号左边**。

---

# 第 4 节 匿名命名空间与 `static`

## 4.1 匿名命名空间 = 内部链接

**不写名字的命名空间**，里面的名字只有本文件可用：

`C++`

```cpp
/* anon_vs_static.cpp    编译：g++ -std=c++17 -c anon_vs_static.cpp -o anon_vs_static.o */
namespace { int hidden_anon = 1; }      // 匿名命名空间
static int hidden_static = 2;           // static
int visible = 3;                        // 外部链接
```

`实测数据`
`Bash`

```bash
g++ -std=c++17 -c anon_vs_static.cpp -o anon_vs_static.o
nm anon_vs_static.o | grep -E "hidden_anon|hidden_static|visible"
```

`实测数据`
`Text`

```text
0000000000000004 d _ZL13hidden_static
0000000000000000 d _ZN12_GLOBAL__N_1L11hidden_anonE
0000000000000008 D visible
```

**三个符号的差别一眼可见**：

| 符号 | 大小写 | 含义 |
|---|---|---|
| `_ZN12_GLOBAL__N_1L11hidden_anonE` | 小写 `d` | 匿名命名空间里 → **内部链接** |
| `_ZL13hidden_static` | 小写 `d` | `static` → **内部链接** |
| `visible` | 大写 `D` | 普通全局 → **外部链接** |

**两种写法在链接属性上等价**，这也是《04-语法/11-作用域、生存期与链接.md》第 4 节
说「`static` 全局变量改的是链接」那句话的另一种写法。

> [!CAUTION]
> **匿名命名空间只应写在源文件里。**
> 头文件被多个源文件包含时，里面的实体在每个源文件里各有一份，彼此互不相干；
> 又因为是内部链接，链接器不会报 `multiple definition`，
> 本意要共享的状态会变成各持一份。

## 4.2 什么时候用哪个

**C++ 里更推荐匿名命名空间**，原因是它能容纳的实体更多：

`C++`

```cpp
/* anon_type.cpp    编译：g++ -std=c++17 -c anon_type.cpp -o anon_type.o */
namespace {
    struct Helper { int x = 1; };   // 匿名命名空间里的类型：只有本文件能用
    Helper h;
}
int use() { return h.x; }
```

**`static` 做不到这件事**——它只能修饰对象与函数：

`实测数据`
`C++`

```cpp
// static_type.cpp    编译：g++ -std=c++17 -c static_type.cpp -o static_type.o （失败）
static struct Helper { int x = 1; };    // 想给类型加 static：不行
int use() { return 0; }
```

`实测数据`
`Text`

```text
static_type.cpp:2:1: error: a storage class can only be specified for objects and functions
```

**报错说得很清楚：存储类只能用于对象和函数**，不能用于类型。

`实测数据`

| 需要隐藏的实体 | `static` | 匿名命名空间 |
|---|---|---|
| 变量、函数 | 可以 | 可以 |
| 类型（`struct`、`class`、`using` 别名） | **不行** | 可以 |
| 模板 | **不行** | 可以 |

> [!TIP]
> **优先用匿名命名空间，`static` 留给简单场合。**
> 匿名命名空间里可以放类型与模板，`static` 只能修饰对象与函数；
> 只需隐藏一个变量或一个函数时，`static` 的写法更短。

---

# 第 5 节 嵌套、`inline` 与版本

## 5.1 嵌套命名空间

**命名空间可以嵌套，用 `::` 逐层限定**——第 2 节已经见过。C++17 起还可以一次写完：

`C++`

```cpp
/* nested17.cpp    编译：g++ -std=c++17 nested17.cpp -o nested17 */
#include <cstdio>

namespace outer::inner {        // C++17 起可以这样一次写完
    int value = 42;
}

int main() {
    std::printf("outer::inner::value = %d\n", outer::inner::value);
    return 0;
}
```

`实测数据`
`Text`

```text
outer::inner::value = 42
```

**这个写法是 C++17 才有的**：用 C++14 编译并打开严格检查，会直接报出来：

`实测数据`
`Text`

```text
error: nested namespace definitions only available with '-std=c++17' or '-std=gnu++17'
```

**旧写法**（任何版本都可用）：

`C++`

```cpp
namespace outer {
    namespace inner {
        int value = 42;
    }
}
```

## 5.2 `inline namespace`：版本化

**`inline namespace` 里的名字，可以不加限定直接用**——
因此它常被用来做「默认版本」：

`C++`

```cpp
/* inline_ns.cpp    编译：g++ -std=c++17 inline_ns.cpp -o inline_ns */
#include <cstdio>

namespace lib {
    inline namespace v2 {       // 默认用这一版
        int api() { return 2; }
    }
    namespace v1 {
        int api() { return 1; }
    }
}

int main() {
    std::printf("lib::api()      = %d\n", lib::api());        // 走 v2
    std::printf("lib::v1::api()  = %d\n", lib::v1::api());    // 老版本还能点名调用
    return 0;
}
```

`实测数据`
`Text`

```text
lib::api()      = 2
lib::v1::api()  = 1
```

**`lib::api()` 不必写 `v2` 就能调用到新版本**，需要使用旧版本时仍可写 `lib::v1::api()`。

> [!IMPORTANT]
> **`inline namespace` 的用途是版本化：新版设为默认，老代码一个字都不用改。**
> 需要固定某一版的地方，仍然可以写出 `lib::v1::` 这样的全名（C++11 起）。

## 5.3 `using Base::f;` 与 `using enum`

**剩下两种形式分别在别处讲解，位置如下：**

`实测数据`

| 形式 | 从哪一版起 | 在哪讲 |
|---|---|---|
| `using Base::f;` | C++11 起（解除基类名字被隐藏） | 第 06 章 继承 |
| `using enum Color;` | **C++20 起**，超出本书基准 | 《04-语法/09-结构体、联合体与 enum.md》第 3 节 |

**`using Base::f;` 解决的问题是「名字隐藏」**：
派生类里定义了同名函数，会把基类那一整组都挡住；
写一句 `using Base::f;` 就能把基类那一组重新引进来。
**它和 `using std::printf;` 是同一个动作——把名字引进来**，只是引的对象换成了基类。

**名字到这里就组织好了**：同一个名字可以分属不同的命名空间，也可以分属不同的类，
而 `::` 是通用的那一个符号。第 02 章接着往下走——把数据与操作数据的函数
放进同一个「类」里，`::` 在那里第二次出现，表示「这个类的」。

---

# 术语表

| 术语 | 英文 | 含义 |
|---|---|---|
| 命名空间 | Namespace | 给名字加的一层「谁家的」；编译期概念，没有运行期开销 |
| 作用域解析运算符 | Scope resolution operator | `::`；`A::b` 读作「`A` 的 `b`」 |
| 限定名 | Qualified name | 带 `::` 的全名，如 `std::printf` |
| `using` 声明 | using-declaration | `using std::printf;`——引一个名字 |
| `using` 指令 | using-directive | `using namespace std;`——引一屋子名字；**头文件里不要写** |
| 别名声明 | Alias declaration | `using Alias = T;`；与 `typedef` 等价，可读性更好 |
| 匿名命名空间 | Anonymous namespace | 没有名字的命名空间；里面的名字都是内部链接 |
| 内部链接 | Internal linkage | 名字只在本文件可见（与 `static` 同效） |
| `inline namespace` | — | 里面的名字可以不加限定使用；用来做默认版本（C++11 起） |
| 名字隐藏 | Name hiding | 内层同名声明挡住外层那一组名字 |

---

# 附录 A 复现本章节实测

## A.1 名字相撞：两个库各有一个 `print`

`C++`

```cpp
/* lib_a.h    编译：g++ -std=c++17 -c lib_a.h -o lib_a_h.o */
#pragma once
void print();
```

`C++`

```cpp
/* lib_b.h    编译：g++ -std=c++17 -c lib_b.h -o lib_b_h.o */
#pragma once
void print();
```

`C++`

```cpp
/* lib_a.cpp    编译：g++ -std=c++17 -c lib_a.cpp -o lib_a.o */
#include "lib_a.h"
#include <cstdio>
void print() { std::printf("A 的 print\n"); }
```

`C++`

```cpp
/* lib_b.cpp    编译：g++ -std=c++17 -c lib_b.cpp -o lib_b.o */
#include "lib_b.h"
#include <cstdio>
void print() { std::printf("B 的 print\n"); }
```

`C++`

```cpp
/* e1_use.cpp    编译：g++ -std=c++17 -c e1_use.cpp -o e1_use.o */
#include "lib_a.h"
int main() { print(); return 0; }
```

`Bash`

```bash
g++ -std=c++17 -c lib_a.cpp -o lib_a.o
g++ -std=c++17 -c lib_b.cpp -o lib_b.o
g++ -std=c++17 e1_use.cpp lib_a.o lib_b.o -o e1_use     # 链接失败：multiple definition
```

## A.2 放入命名空间之后

`C++`

```cpp
/* ns_lib_a.h    编译：g++ -std=c++17 -c ns_lib_a.h -o ns_lib_a_h.o */
#pragma once
namespace liba { void print(); }
```

`C++`

```cpp
/* ns_lib_b.h    编译：g++ -std=c++17 -c ns_lib_b.h -o ns_lib_b_h.o */
#pragma once
namespace libb { void print(); }
```

`C++`

```cpp
/* ns_a.cpp    编译：g++ -std=c++17 -c ns_a.cpp -o ns_a.o */
#include "ns_lib_a.h"
#include <cstdio>
namespace liba { void print() { std::printf("liba 的 print\n"); } }
```

`C++`

```cpp
/* ns_b.cpp    编译：g++ -std=c++17 -c ns_b.cpp -o ns_b.o */
#include "ns_lib_b.h"
#include <cstdio>
namespace libb { void print() { std::printf("libb 的 print\n"); } }
```

`C++`

```cpp
/* e2_qualified.cpp    编译：g++ -std=c++17 -c e2_qualified.cpp -o e2_qualified.o */
#include "ns_lib_a.h"
#include "ns_lib_b.h"
int main() {
    liba::print();
    libb::print();
    return 0;
}
```

`Bash`

```bash
g++ -std=c++17 -c ns_a.cpp -o ns_a.o
g++ -std=c++17 -c ns_b.cpp -o ns_b.o
g++ -std=c++17 e2_qualified.cpp ns_a.o ns_b.o -o e2_qualified && ./e2_qualified
nm ns_a.o | grep print
nm ns_b.o | grep print
```

## A.3 两种 `using` 引出的二义

`C++`

```cpp
/* e2_ambiguous.cpp    编译：g++ -std=c++17 e2_ambiguous.cpp ns_a.o ns_b.o -o e2_ambiguous （失败） */
#include "ns_lib_a.h"
#include "ns_lib_b.h"
using namespace liba;
using namespace libb;
int main() {
    print();
    return 0;
}
```

`C++`

```cpp
/* e2_usingdecl.cpp    编译：g++ -std=c++17 e2_usingdecl.cpp ns_a.o ns_b.o -o e2_usingdecl （失败） */
#include "ns_lib_a.h"
#include "ns_lib_b.h"
using liba::print;
using libb::print;
int main() {
    print();
    return 0;
}
```

## A.4 `::` 的四种用法

`C++`

```cpp
/* scope_forms.cpp */
#include <cstdio>

int g = 1;

namespace outer {
    int g = 2;
    namespace inner { int g = 3; }
}

class Point {
public:
    static int count;
    int x = 0;
    int get() const;
};
int Point::count = 5;
int Point::get() const { return x; }

int main() {
    int g = 4;
    std::printf("局部 g          = %d\n", g);
    std::printf("全局 ::g        = %d\n", ::g);
    std::printf("outer::g        = %d\n", outer::g);
    std::printf("outer::inner::g = %d\n", outer::inner::g);
    std::printf("Point::count    = %d\n", Point::count);
    Point p;
    std::printf("p.get()         = %d\n", p.get());
    return 0;
}
```

## A.5 `using` 声明与别名

`C++`

```cpp
/* using_decl.cpp */
#include <cstdio>
#include <vector>

using std::printf;

int main() {
    printf("引进来就能直接用\n");
    std::vector<int> v{1, 2, 3};
    printf("v.size() = %zu\n", v.size());
    return 0;
}
```

`C++`

```cpp
/* alias.cpp */
#include <cstdio>
#include <typeinfo>

using MyInt  = int;
typedef int  OldInt;

using Callback = void (*)(int);

void handler(int v) { std::printf("handler(%d)\n", v); }

int main() {
    MyInt a = 1;
    OldInt b = 2;
    Callback cb = handler;
    std::printf("a = %d, b = %d\n", a, b);
    cb(7);
    std::printf("两种写法是同一个类型：%s\n",
                (typeid(MyInt).name() == typeid(OldInt).name()) ? "是" : "否");
    return 0;
}
```

## A.6 头文件里的 `using namespace`

`C++`

```cpp
/* bad_ns.h    编译：g++ -std=c++17 -c bad_ns.h -o bad_ns_h.o */
#pragma once
#include <algorithm>
using namespace std;
```

`C++`

```cpp
/* e6_min.cpp    编译：g++ -std=c++17 e6_min.cpp -o e6_min （失败） */
#include "bad_ns.h"

template <class T>
T min(T a, T b) { return a < b ? a : b; }

int main() {
    return min(1, 2);
}
```

`C++`

```cpp
/* e6_min_ok.cpp    编译：g++ -std=c++17 e6_min_ok.cpp -o e6_min_ok */
template <class T>
T min(T a, T b) { return a < b ? a : b; }

int main() {
    return min(1, 2);
}
```

## A.7 匿名命名空间与 `static`

`C++`

```cpp
/* anon_vs_static.cpp    编译：g++ -std=c++17 -c anon_vs_static.cpp -o anon_vs_static.o */
namespace { int hidden_anon = 1; }
static int hidden_static = 2;
int visible = 3;
```

`C++`

```cpp
/* anon_type.cpp    编译：g++ -std=c++17 -c anon_type.cpp -o anon_type.o */
namespace {
    struct Helper { int x = 1; };
    Helper h;
}
int use() { return h.x; }
```

`C++`

```cpp
/* static_type.cpp    编译：g++ -std=c++17 -c static_type.cpp -o static_type.o （失败） */
static struct Helper { int x = 1; };
int use() { return 0; }
```

`Bash`

```bash
g++ -std=c++17 -c anon_vs_static.cpp -o anon_vs_static.o
nm anon_vs_static.o | grep -E "hidden_anon|hidden_static|visible"
g++ -std=c++17 -c anon_type.cpp -o anon_type.o
g++ -std=c++17 -c static_type.cpp -o static_type.o      # 失败
```

## A.8 嵌套命名空间与 `inline namespace`

`C++`

```cpp
/* nested17.cpp */
#include <cstdio>

namespace outer::inner {
    int value = 42;
}

int main() {
    std::printf("outer::inner::value = %d\n", outer::inner::value);
    return 0;
}
```

`Bash`

```bash
g++ -std=c++17 nested17.cpp -o nested17 && ./nested17
g++ -std=c++14 -pedantic-errors nested17.cpp -o nested14     # 失败：C++17 才有
```

`C++`

```cpp
/* inline_ns.cpp */
#include <cstdio>

namespace lib {
    inline namespace v2 {
        int api() { return 2; }
    }
    namespace v1 {
        int api() { return 1; }
    }
}

int main() {
    std::printf("lib::api()      = %d\n", lib::api());
    std::printf("lib::v1::api()  = %d\n", lib::v1::api());
    return 0;
}
```

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《05-类与面向对象/00-导读：类与 OOP 是手段.md》 | **前置**：类与 OOP 是手段；抽象不是免费的 |
| 《04-语法/11-作用域、生存期与链接.md》第 2 节 | **前置**：作用域（就近原则） |
| 《04-语法/11-作用域、生存期与链接.md》第 4 节 | **前置**：链接（内部/外部链接） |
| 《04-语法/14-预处理器.md》第 3.3 小节 | **前置**：头文件里该放什么 |
| 《01-编译器/00-语言的实现.md》第 5.6 小节 | 相关：名字修饰（`_ZN4liba5printEv` 怎么来的） |
| 《04-语法/09-结构体、联合体与 enum.md》第 3 节 | 相关：`enum` 与 `enum class`（`using enum` 是 C++20） |
| 《05-类与面向对象/02-类是一种类型.md》第 1.4 小节 | **后续**：类与结构体、访问控制、成员函数与 `this`、成员函数定义在类外的写法 |
| 《05-类与面向对象/03-构造与析构.md》 | **后续**：构造函数与析构函数 |
| 【待补：05-类与面向对象/06-继承.md】 | **后续**：`using Base::f;` 与名字隐藏 |
