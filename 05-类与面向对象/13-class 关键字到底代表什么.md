# `class` 关键字到底代表什么

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

`class` 是全书出现次数最多的关键字。它出现在至少五个完全不同的位置上：

`C++`

```cpp
class Point { ... };                    // 一、定义一个类
class Point *p;                         // 二、引入一个类型名字（详细类型说明符）
template <class T> void f(T);           // 三、说「这个参数是类型，不是值」
enum class Color { red, green };        // 四、限定作用域的枚举
template class Box<int>;                // 五、显式实例化
```

一个关键字被用在五处，读者很容易以为它们互不相干，或者误以为
`template <class T>` 里的 `T` 必须是类。本章把这五处收在一起，
给出一个统一的解释。

> 哎依旧学习C++不得不品的一环之五个Class用法

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整源码与复现命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 内容 | 在哪一节 |
|---|---|
| `class-key` 是 `class`、`struct`、`union` 三个记号的统称 | **第 1.1 小节** |
| `class` 与 `struct` 的唯一区别（默认访问权限），以及该用哪个 | 第 1.2、1.3 小节 |
| 定义类型：具名类与无名类 | 第 2 节 |
| 详细类型说明符：前置声明、指针与参数、类内部的注入类名、`friend` | **第 3 节** |
| 模板参数里的 `class` 与 `typename` 等价；模板模板参数 | **第 4 节** |
| `enum class` 与 `enum` 的区别、底层类型 | **第 5 节** |
| 显式实例化里的 `class` 可省 | **第 6 节** |
| 统一解释、C 语言的对照、依赖名前的 `class` | **第 7 节** |
| 速查表与检查清单 | **第 8 节** |

---

# 第 1 节 它是一族记号里的一个

## 1.1 `class-key`：三个记号，一种角色

标准把 `class`、`struct`、`union` 统称为 **`class-key`**（类键）。
它们在语法里占同一个位置——**都是「要在这里定义一个类类型」的信号**。

`C++`

```cpp
class  A { public: int v = 1; };      /* class-key：class */
struct B { public: int v = 1; };      /* class-key：struct */
union  C { int i; float f; };         /* class-key：union */
```

`实测数据`
`Text`

```text
sizeof(A) = 4，sizeof(B) = 4
a.v = 1，b.v = 1
```

三个记号的区别在**默认访问权限**与**内存布局**上，而不在「能不能有成员函数」上：

| 记号 | 默认成员访问权限 | 默认继承方式 | 同一时刻只有一个成员存在 |
|---|---|---|---|
| `class` | `private` | `private` | 否 |
| `struct` | `public` | `public` | 否 |
| `union` | `public` | `public` | **是** |

`union` 也在这族里，这一点常被忽略：它可以有成员函数，可以有构造与析构
（《04-语法/09-结构体、联合体与 enum.md》）。

`文档`

> "class-key:
> class
> struct
> union"
>
> —— N4659 §12/1

标准在讲类的那一章开头，把这三个记号写成一个产生式叫 `class-key`——
**它们占的是语法里的同一格**，所以后面讲「默认访问权限」时只需一句话就能区分开。

## 1.2 `class` 与 `struct` 的唯一区别

第 02 章第 1.2 小节说过这条，这里再实测一次，因为它正是「同一个角色、两个记号」的证据：

`C++`

```cpp
/* class_struct_diff.cpp    编译：g++ -std=c++17 class_struct_diff.cpp -o class_struct_diff （失败） */
class A { int v = 1; };              /* 默认 private */
struct B { int v = 1; };             /* 默认 public */

int main() {
    A a; B b;
    return a.v + b.v;                /* a.v 不可访问 */
}
```

`实测数据`
`Text`

```text
class_struct_diff.cpp:7:14: error: 'int A::v' is private within this context
```

换成 `struct` 就能过。**除此之外，两者的语法能力完全相同**：
`struct` 可以继承、可以有虚函数，`class` 也可以只装数据。
把 `struct` 当成「C 的东西」、把 `class` 当成「C++ 的东西」，是误解——
标准里它们是同一族记号。

`文档`

> "Members of a class defined with the keyword class are private by default. Members of a class
> defined with the keywords struct or union are public by default."
>
> —— N4659 §14/3
>
> "In the absence of an access-specifier for a base class, public is assumed when the derived class is
> defined with the class-key struct and private is assumed when the class is defined with the
> class-key class."
>
> —— N4659 §14.2/2

注意区别是**两条**：成员的默认访问权限（第一段引文）、基类的默认继承方式（第二段引文）。
后者常被忘掉——`class Derived : Base` 是私有继承，写成 `struct` 才是公有继承
（《05-类与面向对象/07-继承.md》第 3 节）。

## 1.3 该用哪个

| 场合 | 惯例 | 理由 |
|---|---|---|
| 只是一组公开数据，没有不变量 | `struct` | 让读者一眼看出「这里没有隐藏状态」 |
| 有 `private` 成员、要维护不变量 | `class` | 让读者一眼看出「内部有约定」 |
| 需要 `union` 的语义 | `union` | 同上 |

这条惯例不是语言要求，但标准库自己就是这么用的：
`std::pair`、`std::string_view` 这类「公开的数据聚合」用 `struct`，
而 `std::string`、`std::vector` 用 `class`。

# 第 2 节 第一种用法：定义类型

## 2.1 定义一个具名类

`C++`

```cpp
class Point {
public:
    Point(int x, int y) : x_(x), y_(y) {}
    int x() const { return x_; }
private:
    int x_, y_;
};
```

这一句同时做了四件事：声明名字 `Point`、定义它的成员、确定它的布局、
把它变成 `type-id`（可以写 `Point`、`Point *`、`Point &`）。
第 02 章整章都在讲它，这里不再重复。

## 2.2 不带名字的类

`class-key` 后面可以没有名字，直接定义出一个对象：

`C++`

```cpp
/* unnamed_class.cpp    编译：g++ -std=c++17 unnamed_class.cpp -o unnamed_class */
#include <cstdio>

class {                     /* 没有名字的类：直接定义出一个对象 */
    int v_ = 5;
public:
    int get() const { return v_; }
} obj;                      /* obj 是它的唯一一个对象 */

typedef struct { int x, y; } Pair;   /* C 风格的写法：class-key 也可以 */

int main() {
    std::printf("obj.get() = %d，sizeof(Pair) = %zu\n", obj.get(), sizeof(Pair));
    return 0;
}
```

`实测数据`
`Text`

```text
obj.get() = 5，sizeof(Pair) = 8
```

这种类叫**无名类**：类型没有名字，因此**除了定义在这里的那个对象，谁也建不出第二个**。
它还有一个限制：**不能有静态数据成员**（静态成员要在类外定义，而写定义时你需要一个名字）。

C 里常见的 `typedef struct { ... } Pair;` 也属于这一类：类本身无名，
`typedef` 给它起了个别名。在 C++ 里 `typedef struct Pair { ... } Pair;`
与 `struct Pair { ... };` 等价，前者只是从 C 带过来的习惯写法。

`文档`

> "A class-specifier whose class-head omits the class-head-name defines an unnamed class. [Note: An
> unnamed class thus can't be final. —end note]"
>
> —— N4659 §12/1
>
> "Unnamed classes and classes contained directly or indirectly within unnamed classes shall not
> contain static data members."
>
> —— N4659 §12.2.3.2/4
>
> "If the typedef declaration defines an unnamed class (or enum), the first typedef-name declared by
> the declaration to be that class type (or enum type) is used to denote the class type (or enum type)
> for linkage purposes only (6.5)."
>
> —— N4659 §10.1.3/9

最后一条引文解释了一个容易忽略的细节：`typedef struct { ... } Pair;` 里的 `Pair`
**只在链接层面**算这个名字——它让链接器能区分不同的类型，但不是真正的类名
（写 `Pair::` 之类是不行的）。

# 第 3 节 第二种用法：引入一个类型名字

## 3.1 详细类型说明符

在**类型名字前面写 `class`**，这个组合叫**详细类型说明符**
（elaborated-type-specifier）。它的作用是：**明确告诉编译器「后面这个词是类型名」**。

最常见的两个场合是前置声明与不完整类型：

`C++`

```cpp
/* elaborated.cpp    编译：g++ -std=c++17 elaborated.cpp -o elaborated */
#include <cstdio>

class Point;                                    /* 这就是「详细类型说明符」：引入名字 Point */

class Point *global_ptr;                        /* 指针：可以只闻其名 */

void take(class Point p);                       /* 参数里也能写 class */

class Point {
public:
    Point(int x, int y) : x_(x), y_(y) {}
    int sum() const { return x_ + y_; }
private:
    int x_, y_;
};

void take(class Point p) { std::printf("take：%d\n", p.sum()); }

int main() {
    Point a(1, 2);
    global_ptr = &a;
    std::printf("global_ptr->sum() = %d\n", global_ptr->sum());
    take(a);
    std::printf("sizeof(Point) = %zu\n", sizeof(Point));
    return 0;
}
```

`实测数据`
`Text`

```text
global_ptr->sum() = 3
take：3
sizeof(Point) = 8
```

第 02 章第 4.2 小节讲过前置声明的用法与限制（只能定义指针与引用，
不能建对象，因为它是不完整类型）。**这里要看清的是 `class` 这个记号的角色**：
它不是在「定义类」，而是在**说「Point 是一个类型名」**。

| 写法 | 这句话在做什么 |
|---|---|
| `class Point;` | 引入名字 `Point`，声明它是个类类型（前置声明） |
| `class Point *p;` | 引用这个名字，并用它定义指针 |
| `void f(class Point);` | 同上，用在参数类型上 |
| `Point p;` | 不需要 `class`：名字已经可见了 |

`文档`

> "If the identifier resolves to a class-name or enum-name, the elaborated-type-specifier introduces it
> into the declaration the same way a simple-type-specifier introduces its type-name. If the identifier
> resolves to a typedef-name or the simple-template-id resolves to an alias template specialization,
> the elaborated-type-specifier is ill-formed."
>
> —— N4659 §10.1.7.3/2

「引用一个已有的名字」与「引入一个新名字」这两种情况，标准分开写在两处。
后者的原文是：

`文档`

> "If the elaborated-type-specifier is introduced by the class-key and this lookup does not find a
> previously declared type-name, or if the elaborated-type-specifier appears in a declaration with the
> form: class-key attribute-specifier-seqopt identifier; the elaborated-type-specifier is a declaration
> that introduces the class-name as described in 6.3.2."
>
> —— N4659 §6.4.4/2

**因此 `class Point;` 与 `class Point *p;` 都会把 `Point` 这个名字带进来**——
第 3.4 小节讲的就是这条规则的后果。

## 3.2 类内部：注入类名

类名在类自己的作用域里也是可见的，这叫**注入类名**：

`C++`

```cpp
/* injected_name.cpp    编译：g++ -std=c++17 injected_name.cpp -o injected_name */
#include <cstdio>

class Point {
public:
    Point(int v) : v_(v) {}
    class Point *self() { return this; }    /* 类内部：class Point 与 Point 是同一个类型 */
    Point *self2() { return this; }
    int v_;
};

int main() {
    Point p(3);
    std::printf("%d %d\n", p.self()->v_, p.self2()->v_);
    return 0;
}
```

`实测数据`
`Text`

```text
3 3
```

`文档`

> "A class-name is inserted into the scope in which it is declared immediately after the class-name is
> seen. The class-name is also inserted into the scope of the class itself; this is known as the
> injected-class-name. For purposes of access checking, the injected-class-name is treated as if it
> were a public member name."
>
> —— N4659 §12/2

最后一句值得留意：**注入类名在访问检查上等同于公开成员**，
所以类内部无论怎么写 `Point`，都不会因为它在 `private` 段而访问不到。

两种写法的类型完全相同。**类内部写 `class Point` 是多余的**，
它的用处只有一个：在名字可能被遮住的地方（例如基类里有同名成员，见
《05-类与面向对象/07-继承.md》第 4 节）明确说出「我说的是类本身」。

## 3.3 `friend` 声明：三种写法都合法

第 03 章第 6 节讲过友元。这里补一条与关键字有关的细节——
`friend` 后面可以写详细类型说明符，也可以写一个普通的类型名字：

`C++`

```cpp
/* friend_key.cpp    编译：g++ -std=c++17 friend_key.cpp -o friend_key */
#include <cstdio>

class Screen;

class Window {
    friend class Screen;                /* 详细类型说明符：带 class */
    friend Window *clone(const Window &);   /* 函数声明：不带 class */
    int w_ = 7;
public:
    int value() const { return w_; }
};

class Screen {
public:
    int read(const Window &x) { return x.w_; }
};

Window *clone(const Window &x) { Window *p = new Window(x); return p; }

int main() {
    Window w; Screen s;
    std::printf("Screen 读到 %d\n", s.read(w));
    Window *p = clone(w);
    std::printf("clone 读到 %d\n", p->value());
    delete p;
    return 0;
}
```

`实测数据`
`Text`

```text
Screen 读到 7
clone 读到 7
```

`friend class Screen;` 与 `friend Screen;` 等价（前提是 `Screen` 这个名字已经可见）。
**新代码写不带 `class` 的形式**：少一个记号，意思不变，
而且当 `Screen` 是模板或别名时，不带 `class` 的写法不会产生歧义。

`文档`

> "A friend declaration that does not declare a function shall have one of the following forms:
> friend elaborated-type-specifier;
> friend simple-type-specifier;
> friend typename-specifier;"
>
> —— N4659 §14.3/3

三种形式对应三种写法：带 `class` 的详细类型说明符、普通的类型名、
以及 `typename` 说明符。**在类模板里这条区别会变成硬性规定**：
模板参数 `T` 是类型参数时，`friend class T;` 不合法，而 `friend T;` 合法。

`文档`

> "This implies that, within a class template with a template type-parameter T, the declaration
> friend class T; is ill-formed. However, the similar declaration friend T; is allowed (14.3)."
>
> —— N4659 §10.1.7.3/2

## 3.4 它会「声明」一个名字

详细类型说明符有一个容易忽略的副作用：**如果这个名字此前从没出现过，
它就在这里被声明了**。因此下面两行是合法的，且引入了同一个名字：

`C++`

```cpp
class Point;            // 在全局作用域引入 Point
void f(class Point);    // 参数类型上再用一次
```

反过来，把一个类型不兼容的名字写进详细类型说明符里，编译器会报错：

`C++`

```cpp
/* elaborated_conflict.cpp    编译：g++ -std=c++17 elaborated_conflict.cpp -o elaborated_conflict （失败） */
typedef int Point;              /* Point 是一个整数类型 */

class Point *p;                 /* 这里又说它是类类型 */

int main() {
    return 0;
}
```

`实测数据`
`Text`

```text
elaborated_conflict.cpp:4:7: error: using typedef-name 'Point' after 'class'
elaborated_conflict.cpp:2:13: note: 'Point' has a previous declaration here
```

**因此：详细类型说明符只能用在「确实是类类型」的名字上**，
用它来「猜」一个名字是不是类型，编译器不会配合。

# 第 4 节 第三种用法：说明「这是一个类型参数」

## 4.1 `template <class T>` 与 `template <typename T>` 完全等价

这是最容易被误读的一处：模板参数列表里的 `class` **与「类」没有关系**，
它只是「类型参数」的两种写法之一。《05-类与面向对象/11-模板.md》第 2.4 小节
已经给过结论与标准原文，这里补两件那里没说的事：**两种写法在模板模板参数里同样可以互换**，
以及**这个 `T` 后来可以不是类**。

`C++`

```cpp
/* tparam_equiv.cpp    编译：g++ -std=c++17 tparam_equiv.cpp -o tparam_equiv */
#include <cstdio>

template <class T>          /* 用 class */
T twice_a(T v) { return v + v; }

template <typename T>       /* 用 typename：两种写法没有任何语义差别 */
T twice_b(T v) { return v + v; }

template <template <class> class C>      /* 模板模板参数：内层也是 class */
struct Uses {
    static const char *name() { return "内层 class"; }
};

template <template <typename> class C>   /* 内层写 typename 也一样 */
struct Uses2 {
    static const char *name() { return "内层 typename"; }
};

template <class T> struct Box { T v; };

int main() {
    std::printf("%d %.1f\n", twice_a(3), twice_b(1.5));
    std::printf("%s / %s\n", Uses<Box>::name(), Uses2<Box>::name());
    return 0;
}
```

`实测数据`
`Text`

```text
6 3.0
内层 class / 内层 typename
```

**`class` 在这里不代表「类」**：

`C++`

```cpp
/* class_not_class.cpp    编译：g++ -std=c++17 class_not_class.cpp -o class_not_class */
#include <cstdio>

template <class T>              /* T 后来是 int，不是类 */
T add(T a, T b) { return a + b; }

int main() {
    std::printf("%d\n", add(2, 3));         /* T = int：模板参数写 class，实参不必是类 */
    std::printf("%.1f\n", add(0.5, 1.5));
    return 0;
}
```

`实测数据`
`Text`

```text
5
2.0
```

`T` 被推导成 `int` 与 `double`，两个都不是类，代码照样编译。
第 11 章第 2 节讲参数推导时用的 `template <class T>` 就是这个意思。

## 4.2 模板模板参数

`template <template <class> class C>` 这种写法里有两个 `class`：
**内层的 `class` 是「这个参数是一个类型」**，
**外层的 `class` 是「这个参数是一个模板」**。按本书的基准（C++17），
外层同样可以写 `typename`：

`C++`

```cpp
/* tparam_typename_tt.cpp    编译：g++ -std=c++17 tparam_typename_tt.cpp -o tparam_typename_tt */
#include <cstdio>

template <class T> struct Box { T v; };

template <template <typename> typename C>   /* 外层写 typename：基准下合法，实测通过 */
struct Uses {
    static const char *name() { return "外层 typename"; }
};

int main() {
    std::printf("%s\n", Uses<Box>::name());
    return 0;
}
```

`实测数据`
`Text`

```text
外层 typename
```

`文档`

> "type-parameter:
> type-parameter-key ...opt identifieropt
> type-parameter-key identifieropt = type-id
> template < template-parameter-list > type-parameter-key ...opt identifieropt
> template < template-parameter-list > type-parameter-key identifieropt = id-expression
> type-parameter-key:
> class
> typename"
>
> —— N4659 §17.1/1

语法里管这一格叫 `type-parameter-key`，它只有 `class` 与 `typename` 两个取值——
**模板模板参数的内外两层用的是同一个格子**，因此两处都能互换。
（具体是哪一版标准把外层从只允许 `class` 放宽到两者皆可，`待确认`；
能确定的是本书基准 C++17 下两种写法都通过编译。）

## 4.3 该写哪个

| 写法 | 场合 |
|---|---|
| `typename` | **默认选它**：它的名字就说明「这里要一个类型」，不会让人以为必须是类 |
| `class` | 只在必须与老代码保持一致时用；模板模板参数的外层在更早的标准里只能写它 |

**两者没有性能与语义差别**，唯一的差别是读代码的人怎么理解。
第 11 章与第 12 章为了与常见资料一致用了 `class`，
你自己写代码时用 `typename` 更不容易产生误解。

---

# 第 5 节 第四种用法：`enum class`

## 5.1 它加的是「作用域」，不是「类」

`enum class`（以及等价的 `enum struct`）叫**限定作用域的枚举**。
`class` 在这里表示两件事：**枚举名关在自己的作用域里**、
**不会隐式转成整数**。

`C++`

```cpp
/* enum_class.cpp    编译：g++ -std=c++17 enum_class.cpp -o enum_class */
#include <cstdio>

enum class Color { red, green, blue };       /* 限定作用域：名字在 Color 里面 */
enum Plain { p_red, p_green };               /* 不限定作用域：名字漏到外层 */

enum class Tiny : unsigned char { a, b };    /* 可以指定底层类型 */

int main() {
    Color c = Color::red;                    /* 必须写 Color:: */
    std::printf("Color::red = %d，sizeof(Color) = %zu\n", (int)c, sizeof(Color));
    std::printf("Tiny：sizeof = %zu\n", sizeof(Tiny));
    int n = p_red;                           /* 不限定作用域的可以直接当整数用 */
    std::printf("int n = %d\n", n);
    return 0;
}
```

`实测数据`
`Text`

```text
Color::red = 0，sizeof(Color) = 4
Tiny：sizeof = 1
int n = 0
```

`Tiny` 指定了底层类型 `unsigned char`，因此每个枚举值只占 1 字节。
**底层类型不是 `enum class` 独有的**：`enum E : unsigned char { ... };` 也合法（C++11 起），
区别在于**不给底层类型时**——限定作用域的枚举默认是 `int`，
不限定作用域的枚举由实现按取值范围挑一个能装下的整数类型。

`文档`

> "The enum-keys enum class and enum struct are semantically equivalent; an enumeration type declared
> with one of these is a scoped enumeration, and its enumerators are scoped enumerators."
>
> —— N4659 §10.2/2
>
> "Each enumeration defines a type that is different from all other types. Each enumeration also has an
> underlying type. The underlying type can be explicitly specified using an enum-base. For a scoped
> enumeration type, the underlying type is int if it is not explicitly specified. In both of these
> cases, the underlying type is said to be fixed."
>
> —— N4659 §10.2/5

## 5.2 两条区别，一条实测

不隐式转换这一条，编译器给的错误很直接：

`C++`

```cpp
/* enum_class_fail.cpp    编译：g++ -std=c++17 enum_class_fail.cpp -o enum_class_fail （失败） */
enum class Color { red, green };

int main() {
    Color c = Color::red;
    int n = c;                  /* 限定作用域的枚举不隐式转成整数 */
    return n;
}
```

`实测数据`
`Text`

```text
enum_class_fail.cpp:6:13: error: cannot convert 'Color' to 'int' in initialization
```

`文档`

> "Note that this implicit enum to int conversion is not provided for a scoped enumeration:
> enum class Col { red, yellow, green };
> int x = Col::red; // error: no Col to int conversion"
>
> —— N4659 §10.2/10

| 区别 | `enum` | `enum class` |
|---|---|---|
| 枚举名的可见范围 | 漏到外层作用域（`p_red` 直接可用） | 关在枚举里（必须写 `Color::red`） |
| 能否隐式转成整数 | **能**（整型提升） | **不能**，要显式 `(int)c` 或 `static_cast<int>(c)` |
| 不给底层类型时是什么 | 由实现按取值范围挑 | **`int`**（第 5.1 小节的引文） |
| 两个枚举能否有同名成员 | 不能（会撞名） | **能**（各自关在自己的作用域里） |

**最后一行是它在工程上的主要价值**：两个模块各自定义 `enum class Status { ok, failed };`
不会互相冲突，而写成 `enum` 就必须给成员加前缀（`STATUS_OK`、`MODULE2_STATUS_OK`）。
C++ 里看到成串的大写前缀，通常就是从这种冲突里长出来的。

# 第 6 节 第五种用法：显式实例化里的 `class`

第 11 章第 5.2 小节用过 `template class Stack<int>;` 这一行来把模板的实例化
钉在一个 `.cpp` 文件里。这里要说明的是：**这里的 `class` 不是可省的装饰，
它就是被实例化的那个类型的写法**——写 `class` 或 `struct` 都行，但不能不写。

`C++`

```cpp
/* explicit_inst.cpp    编译：g++ -std=c++17 explicit_inst.cpp -o explicit_inst */
#include <cstdio>

template <class T>
struct Box {
    T v;
    int size() const { return (int)sizeof(T); }
};

template struct Box<int>;               /* 用 struct：class-key 写哪个都行 */
template class Box<double>;             /* 用 class：意思完全相同 */

int main() {
    Box<int> a{1};
    Box<double> b{1.5};
    std::printf("%d %d\n", a.size(), b.size());
    return 0;
}
```

`实测数据`
`Text`

```text
4 8
```

两行的效果完全相同：显式实例化的语法是 `template` 后面跟一个**声明**，
而 `struct Box<int>` 与 `class Box<double>` 都是合法的类型写法——
**第二个记号属于声明本身，不是显式实例化的关键字**（第 3 节的详细类型说明符）。

**但整个省掉不行。** 类模板的实例化必须写出 `class-key`：

`C++`

```cpp
/* explicit_inst_omit_fail.cpp    编译：g++ -std=c++17 explicit_inst_omit_fail.cpp -o explicit_inst_omit_fail （失败） */
template <class T>
struct Box { T v; };

template Box<int>;              /* 省掉 class-key：不行 */

int main() {
    Box<int> b{1};
    return b.v;
}
```

`实测数据`
`Text`

```text
explicit_inst_omit_fail.cpp:5:18: error: expected unqualified-id before ';' token
```

原因在语法上：`template` 后面必须跟一个**声明**，
而 `Box<int>` 单独写出来不是声明——它没有说明这是个类还是别的什么。
补上 `class` 或 `struct`，它就成了一条合法声明。

`文档`

> "The syntax for explicit instantiation is:
> explicit-instantiation:
> externopt template declaration
> There are two forms of explicit instantiation: an explicit instantiation definition and an explicit
> instantiation declaration. An explicit instantiation declaration begins with the extern keyword."
>
> —— N4659 §17.7.2/2
>
> "If the explicit instantiation is for a class or member class, the elaborated-type-specifier in the
> declaration shall include a simple-template-id."
>
> —— N4659 §17.7.2/3

| 写法 | 含义 |
|---|---|
| `template class Box<double>;` | 显式实例化 `Box<double>`；`class` 是详细类型说明符（第 3 节） |
| `template struct Box<double>;` | 同上，`struct` 也可以 |
| `template Box<double>;` | **不行**：少了 `class-key`，报 `expected unqualified-id` |
| `template void f<int>(int);` | 函数模板的显式实例化：函数没有 `class-key` 这回事 |

# 第 7 节 统一解释

## 7.1 五处用法是同一件事

把上面五处放在一起看，会发现 `class` 每次出现都在**指明一个类型名字**，
只是它在句子里的角色不同：

| 出现的位置 | 它在说什么 | 换成别的记号行不行 |
|---|---|---|
| `class Point { ... };` | 「我要定义一个类类型，名字叫 Point」 | 可以换 `struct`、`union` |
| `class Point *p;` | 「Point 是一个类型名」（详细类型说明符） | 可以换 `struct`、`union`；名字可见时可整个省掉 |
| `template <class T>` | 「T 是一个**类型**参数，不是值参数」 | 可以换 `typename` |
| `enum class Color` | 「这个枚举的名字关在自己作用域里」 | 可以换 `enum struct` |
| `template class Box<int>;` | 「`Box<int>` 是一个类型」（详细类型说明符） | 可以整个省掉 |

**只有第一种是「定义一个新类型」**，其余四种都是「引用 / 说明一个已经存在的类型」。
把它们看成同一个关键字在干五件不同的事，比记五条互不相干的规则更容易。

## 7.2 跨语言的对照：C 里 `class` 不是关键字

C 里没有类，也没有 `class` 这个关键字：

`C`

```c
/* c_class_ident.c    编译：gcc -std=c23 c_class_ident.c -o c_class_ident */
#include <stdio.h>

int main(void) {
    int class = 3;              /* C 里 class 不是关键字，可以当标识符 */
    int struct_like = class + 1;
    printf("class = %d，struct_like = %d\n", class, struct_like);
    return 0;
}
```

`实测数据`
`Text`

```text
class = 3，struct_like = 4
```

同一份代码用 `g++` 编译会报错（`class` 在 C++ 里是关键字）。
这也解释了为什么 C 的头文件里常见 `struct Point` 而不写 `Point`：
C 没有「类名自动成为类型名」这条规则，必须写 `struct` 才能当类型用——
那个 `struct` 在 C++ 里正是本节的**详细类型说明符**。

## 7.3 一个边界：依赖名前的 `class`

《05-类与面向对象/11-模板.md》第 5.5 小节讲过，模板里引用依赖名要在前面写 `typename`。
实际上**写 `class` 也能达到同样的效果**：

`C++`

```cpp
/* dep_class.cpp    编译：g++ -std=c++17 dep_class.cpp -o dep_class */
#include <cstdio>

struct Holder {
    struct Inner { int v = 9; };
};

template <class T>
int read() {
    class T::Inner x;           /* 用详细类型说明符说明这是个类型，就不必写 typename */
    return x.v;
}

int main() {
    std::printf("read<Holder>() = %d\n", read<Holder>());
    return 0;
}
```

`实测数据`
`Text`

```text
read<Holder>() = 9
```

它能编译，是因为详细类型说明符（第 3 节）本身就带「这是个类型名」的信息。
**但不要用它**：

| 理由 | 说明 |
|---|---|
| 只能用在声明里 | `class T::Inner x;` 是声明；而 `T::Inner::value` 这类**表达式**里只能写 `typename` |
| 名字会变得含糊 | 读代码的人会以为 `T::Inner` 与外围的某个类同名 |
| 标准里的写法是 `typename` | 《05-类与面向对象/11-模板.md》第 5.5 小节引的那一条就是它（§17.6/3） |

一句话：**`class` 能用于这个场合是详细类型说明符的副作用，不是它的用途。**

## 7.4 为什么一个关键字被复用了五处

这五处有一个共同点：**它们都需要在一句话里指出「这里有一个类型」**。

`class` 之所以被反复借用，是因为 C++ 的语法里「类型」这个位置特别多：
定义类型的地方、声明变量的地方、模板参数列表、枚举声明、显式实例化。
与其为每个位置各造一个关键字，语言沿用了同一个记号，
再用**它后面跟的东西**来区分意思：

| 后面跟什么 | 意思 |
|---|---|
| 一个名字加 `{ ... }` | 定义这个类 |
| 一个名字（不带 `{}`） | 引用这个类型名 |
| 在 `template <>` 里 | 类型参数 |
| 在 `enum` 后面 | 限定作用域的枚举 |
| 在 `template` 声明里 | 被实例化的那个类型 |

`enum class` 是这条思路最晚近的一次应用：它借用 `class-key` 来表示
「把名字关进去」这层意思，而**被修饰的东西根本不是类**。

# 第 8 节 速查与检查清单

## 8.1 一页速查

`实测数据`

| 你看到的写法 | 意思 | 能不能换别的 |
|---|---|---|
| `class A { ... };` | 定义类 `A` | `struct`（默认权限不同） |
| `class A;` | 前置声明 | `struct A;` |
| `class A *p;`、`void f(class A);` | 详细类型说明符：引用类型名 | 名字可见时可省 |
| `friend class A;` | 友元声明（详细类型说明符） | `friend A;`（三种形式都合法） |
| `template <class T>` | 类型参数 | `typename T` |
| `template <template <class> class C>` | 模板模板参数（内层是类型参数） | 两处的 `class` 都能换成 `typename` |
| `enum class E { ... };` | 限定作用域的枚举 | `enum struct` |
| `template class Box<int>;` | 显式实例化 | `struct` 可以；**整个省掉不行**（第 6 节） |
| `class { ... } obj;` | 无名类 | 无 |

## 8.2 检查清单

| 检查 | 说明 |
|---|---|
| 把 `template <class T>` 读成「T 是一个类型」了吗 | 它**不是**「T 是一个类」（第 4.1 小节） |
| 新写的模板参数用的是 `typename` 吗 | 与 `class` 等价，但更不容易误读（第 4.3 小节） |
| 需要限定作用域的枚举时，写的是 `enum class` 吗 | 否则枚举名会漏到外层、还会隐式转成整数（第 5 节） |
| 枚举需要固定大小时，指定底层类型了吗 | `enum class E : unsigned char`（第 5.1 小节） |
| `friend` 后面那个 `class` 是多余的吗 | 可以省；但类模板里必须写 `friend T;`，`friend class T;` 不合法（第 3.3 小节） |
| 类模板的显式实例化写了 `class` 或 `struct` 吗 | 少了它报 `expected unqualified-id`（第 6 节） |
| 用 `class X *p;` 声明时，`X` 真的定义过吗 | 详细类型说明符只在名字**确实是类类型**时成立（第 3.4 小节） |
| 用 `class T::X x;` 代替 `typename` 了吗 | 换掉：它只在声明里有效，可读性也差（第 7.3 小节） |
| 数据聚合用 `struct`、有不变量用 `class` 了吗 | 惯例，让读者一眼看出有没有隐藏状态（第 1.3 小节） |

---

# 术语表

| 词 | 含义 | 在哪一节 |
|---|---|---|
| **`class-key`** | `class`、`struct`、`union` 三个记号的统称 | 第 1.1 小节 |
| **详细类型说明符** | 在类型名前写 `class-key` 的写法，作用是引用 / 引入一个类型名 | 第 3.1 小节 |
| **前置声明** | 只引入名字、不定义类的声明 | 第 3.1 小节 |
| **注入类名** | 类名在类自己的作用域里也可见 | 第 3.2 小节 |
| **无名类** | 没有名字的类，只能定义出它自己的那个对象 | 第 2.2 小节 |
| **类型参数** | 模板参数列表里「代表一个类型」的参数；写法 `class T` 或 `typename T` | 第 4.1 小节 |
| **模板模板参数** | 参数本身是一个模板，例如 `template <template <class> class C>` | 第 4.2 小节 |
| **限定作用域的枚举** | `enum class` / `enum struct`；名字关在枚举里，不隐式转整数 | 第 5 节 |
| **底层类型** | 枚举用来存值的整数类型，`enum class E : unsigned char` 里的那一个 | 第 5.1 小节 |

---

# 附录 A 复现本章节实测

`实测数据`

| 程序 | 编译命令 | 演示什么 | 源码在哪 |
|---|---|---|---|
| `class_struct.cpp` | `g++ -std=c++17 class_struct.cpp -o class_struct` | `class` 与 `struct` 定义的类没有区别 | 第 1.1 小节 |
| `class_struct_diff.cpp` | 同上（**期望失败**） | 唯一区别是默认访问权限 | 第 1.2 小节 |
| `unnamed_class.cpp` | 同上 | 无名类与 C 风格的 `typedef struct` | 第 2.2 小节 |
| `elaborated.cpp` | 同上 | 详细类型说明符：前置声明、指针、参数 | 第 3.1 小节 |
| `injected_name.cpp` | 同上 | 注入类名 | 第 3.2 小节 |
| `friend_key.cpp` | 同上 | `friend class X;` 与 `friend X;` 等价 | 第 3.3 小节 |
| `elaborated_conflict.cpp` | 同上（**期望失败**） | 详细类型说明符只能用在类类型上 | 第 3.4 小节 |
| `tparam_equiv.cpp` | 同上 | `class` 与 `typename` 在模板参数里等价 | 第 4.1 小节 |
| `class_not_class.cpp` | 同上 | `template <class T>` 的 `T` 可以是 `int` | 第 4.1 小节 |
| `tparam_typename_tt.cpp` | 同上 | C++17 起模板模板参数处也能写 `typename` | 第 4.2 小节 |
| `enum_class.cpp` | 同上 | `enum class` 的作用域、底层类型 | 第 5.1 小节 |
| `enum_class_fail.cpp` | 同上（**期望失败**） | 限定作用域的枚举不隐式转整数 | 第 5.2 小节 |
| `explicit_inst.cpp` | 同上 | 显式实例化里的 `class` 可省 | 第 6 节 |
| `c_class_ident.c` | `gcc -std=c23 c_class_ident.c -o c_class_ident` | C 里 `class` 可以当标识符 | 第 7.2 小节 |
| `dep_class.cpp` | `g++ -std=c++17 dep_class.cpp -o dep_class` | 依赖名前的 `class` 也能代替 `typename` | 第 7.3 小节 |

## A.1 复现本章节实测用的环境

`实测数据`

| 项目 | 取值 |
|---|---|
| C++ 编译器 | `g++` 15.2.0（MinGW-w64，x86-64），`-std=c++17` |
| C 编译器 | `gcc` 15.2.0，`-std=c23` |
| 优化等级 | 未开优化 |

`sizeof(enum)` 的取值与底层类型有关；`enum class` 不给底层类型时，
编译器按取值范围挑一个有符号整数类型，本机是 4 字节。

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《05-类与面向对象/02-类是一种类型.md》第 1.2 小节 | **前置**：`class` 与 `struct` 的唯一区别 |
| 《05-类与面向对象/02-类是一种类型.md》第 4.2 小节 | **前置**：前置声明与不完整类型 |
| 《05-类与面向对象/03-成员与细节.md》第 6 节 | **前置**：友元（`friend class X;`） |
| 《05-类与面向对象/11-模板.md》第 2.2 小节 | **前置**：`template <class T>` 与参数推导 |
| 《05-类与面向对象/11-模板.md》第 2.4 小节 | **前置**：`class` 与 `typename` 没有区别（结论与标准原文） |
| 《05-类与面向对象/11-模板.md》第 5.2 小节 | **前置**：显式实例化 |
| 《05-类与面向对象/11-模板.md》第 5.5 小节 | **前置**：依赖名与 `typename` |
| 《05-类与面向对象/07-继承.md》第 4 节 | 相关：名字隐藏（注入类名要消歧的场合） |
| 《05-类与面向对象/12-模板的高阶使用.md》第 4 节 | 相关：SFINAE 与类型特征 |
| 《04-语法/09-结构体、联合体与 enum.md》 | **前置**：`union` 与 `enum` 的基本用法 |
| 《04-语法/04-表达式与运算符.md》第 3.11 小节 | 相关：`typeid`、`alignof` 这类与类型有关的运算符 |
