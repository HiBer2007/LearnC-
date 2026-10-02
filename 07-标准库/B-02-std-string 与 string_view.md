# `std::string` 与 `string_view`

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**C 里的一段文本不是一种类型，而是一条约定**：一块 `char` 数组，末尾放一个 `'\0'`，
再配上一组函数——`strlen` 负责数、`strcpy` 负责搬、`strcat` 负责接。
约定本身没有强制力：长度要每次去数，目标够不够大须由调用者保证，
缓冲区什么时候释放也须由调用者记住。

**`std::string` 把这条约定收进了一个类**：长度成了成员（`size()` 是常数时间），
缓冲区随内容增长、随对象析构释放，`+`、`==`、`<` 这些运算符也能直接用在文本上。
A 段《07-标准库/A-02-字符串与内存：string.h.md》第 1 节讲的 `strlen`/`strcpy`/`strcat` 一族，
在 C++ 里绝大多数场合不再需要手写。

**但有一件事它没有改变**：`std::string` 存的仍然是一串**字节**，
`size()` 数的是字节数，不是字符数。一段中文的 `size()` 是它的 UTF-8 字节数，
`substr` 可以干脆利落地把一个字切成两半。
本章节把「类替掉了什么、没替掉什么」分开讲清楚：容量与失效规则属于前者，编码现实属于后者。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整源码与复现命令见附录 A；`文档` 表示引自标准草案或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<版本号>`）的含义见《README.md》。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| `strlen`/`strcpy`/`strcat`/`strcmp` 一族 | 《07-标准库/A-02-字符串与内存：string.h.md》第 1 节 |
| `memcpy`/`memmove` 与重叠区域 | 《07-标准库/A-02-字符串与内存：string.h.md》第 2 节 |
| 拷贝构造与移动构造 | 《05-类与面向对象/05-拷贝与移动.md》第 2 节 |
| RAII：资源在析构时释放 | 《05-类与面向对象/06-RAII 与资源管理.md》第 2 节 |
| `operator+`、`operator[]`、`operator==` 的重载 | 《05-类与面向对象/09-运算符重载.md》第 3 节 |
| 迭代器与失效的一般规则 | 《07-标准库/README.md》第二节（容器归 `09-高阶数据结构`） |
| 异常与 `catch` | 《04-语法/13-异常.md》第 1 节 |

**相邻的章节**：上承《07-标准库/A-02-字符串与内存：string.h.md》，
左接《07-标准库/B-01-输入输出：iostream.md》第 5 节（`getline` 读进来的就是 `std::string`）。
`<charconv>` 与 `<random>`、`<limits>` 一起在《07-标准库/B-05-数值.md》里还有一节，
`std::filesystem::path` 的编码问题在《07-标准库/B-07-文件系统：filesystem.md》第 1 节。

| 本章各节 | 讲什么 |
|---|---|
| 第 1 节 | 从 `char[]` 到 `std::string`：同一个任务的两种写法 |
| 第 2 节 | 构造与容量：`size`/`capacity`/`reserve`/`shrink_to_fit`，SSO 的实测阈值 |
| 第 3 节 | 修改与查找：`append`/`insert`/`erase`/`replace`/`find`/`compare`；拼接的分配代价 |
| 第 4 节 | 迭代器与引用失效：标准的规则与 ASan 抓到的悬垂 |
| 第 5 节 | 与 C 字符串互操作：`c_str`/`data`/`copy`，指针什么时候失效 |
| 第 6 节 | `std::string_view`：为什么需要、零拷贝的代价、什么时候不该用 |
| 第 7 节 | 数字与字符串互转：`to_string`/`stoi`/`stod`，`from_chars`/`to_chars` |
| 第 8 节 | 编码现实：UTF-8 是字节序列、`size()` 是字节数、`wstring` 在 Windows 上的坑 |

---

# 第 1 节 从 `char[]` 到 `std::string`

## 1.1 同一个任务的两种写法

数一段文本里有几个词，再把它们用 `-` 连起来。

`C`

```c
/* str_task.c    编译：gcc -std=c23 str_task.c -o str_task_c
 * 与 cpp 版做同一件事：数一段文本里有几个词，再把它们用 '-' 连起来 */
#include <stdio.h>
#include <string.h>

int main(void) {
    char text[64] = "alpha beta gamma";     // strtok 会改内容，所以要可写的数组
    char joined[64] = "";
    int words = 0;

    for (char *p = strtok(text, " "); p != NULL; p = strtok(NULL, " ")) {
        if (words > 0) {
            strcat(joined, "-");
        }
        strcat(joined, p);
        ++words;
    }

    printf("词数：%d\n", words);
    printf("拼接：%s\n", joined);
    printf("总长：%zu\n", strlen(joined));
    return 0;
}
```

`C++`

```cpp
/* str_task.cpp    编译：g++ -std=c++17 str_task.cpp -o str_task_cpp
 * 与 C 版做同一件事，改用 std::string 与 find/substr */
#include <iostream>
#include <string>

int main() {
    const std::string text = "alpha beta gamma";
    std::string joined;
    int words = 0;

    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t sp = text.find(' ', pos);          // npos 表示没找到
        const std::string w = text.substr(pos, sp == std::string::npos
                                                  ? std::string::npos : sp - pos);
        if (!w.empty()) {
            if (words > 0) joined += '-';
            joined += w;
            ++words;
        }
        if (sp == std::string::npos) break;
        pos = sp + 1;
    }

    std::cout << "词数：" << words << "\n";
    std::cout << "拼接：" << joined << "\n";
    std::cout << "总长：" << joined.size() << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
词数：3
拼接：alpha-beta-gamma
总长：16
```

两版输出逐字节相同（都是 52 字节）。**差别不在结果上，在「谁负责什么」上**：

| 谁来负责 | C（`char[]`） | C++（`std::string`） |
|---|---|---|
| 记住长度 | 没人记，`strlen` 每次从头数一遍 | 对象里存着，`size()` 是常数时间 |
| 保证目标够大 | 调用者，靠人眼与经验 | 对象自己增长 |
| 防止越界 | 没有机制，越界是未定义行为 | `[]` 不做检查，但 `at()` 会抛 `out_of_range` |
| 释放内存 | 栈数组自动，堆数组靠 `free` | 析构函数自动 |
| 复制与拼接 | `strcpy`/`strcat`/`strncpy` | `=`、`+`、`+=`、`append` |
| 比较 | `strcmp` 返回三态整数 | `==`、`<` 直接给出布尔结果 |
| 长度上限 | 数组定义时就定了 | 受内存与 `max_size()` 限制 |

**`strlen` 每次都从头数**，这不是实现偷懒，而是那条约定本身没有别的地方可以存长度。
长度成了类型的一部分之后，`size()` 才能是常数时间：

`文档`

> "`size_type size() const noexcept;` Returns: A count of the number of char-like
> objects currently in the string. Complexity: Constant time."
>
> —— N4659 §24.3.2.4/1

## 1.2 三个仍然要留意的旧习惯

**第一，`std::string` 里可以嵌 `'\0'`。** 它存的是「长度 + 字节」，
不是「到 `'\0'` 为止」，所以 `std::string("a\0b", 3)` 的长度是 3。
一旦交给 C 的函数（`printf("%s")`、`strlen`、`fopen`），就只能看到第一个 `'\0'` 之前的部分。

**第二，`c_str()` 是借出去的指针，不是稳定的句柄**（第 5 节展开）。

**第三，`std::string` 的类型名后面还有两个默认参数**：
`std::string` 是 `std::basic_string<char, std::char_traits<char>, std::allocator<char>>` 的别名。
另外三个别名是 `std::wstring`、`std::u16string`、`std::u32string`，元素类型分别是
`wchar_t`、`char16_t`、`char32_t`。**它们的坑在第 8 节。**

## 1.3 它仍然是一串字节

`"中文"` 在源码里是 UTF-8 字节，`std::string` 忠实地存这 6 个字节，
`size()` 返回 6。**类型变了，编码不会跟着变**。
第 8 节把这件事展开；此处先给出一句结论：
**`std::string` 管的是字节，字符与显示宽度须另行处理。**

---

# 第 2 节 构造与容量

## 2.1 构造对象

| 写法 | 结果 |
|---|---|
| `std::string s;` | 空串 |
| `std::string s = "abc";` | 从 C 字符串构造 |
| `std::string s("abc", 2);` | 取前 2 个字符：`"ab"` |
| `std::string s(5, 'x');` | 5 个 `'x'`：`"xxxxx"` |
| `std::string s(other);` | 拷贝构造 |
| `std::string s(std::move(other));` | 移动构造 |
| `std::string s(other, 2, 3);` | 从 `other` 的下标 2 起取 3 个字符 |
| `std::string s{'a', 'b'};` | 初始化列表 |

**下标越界的两个构造函数会抛 `std::out_of_range`**（`(other, pos, n)` 里 `pos > other.size()` 时），
这与 `[]` 的「不检查」正好相反。

## 2.2 `sizeof` 与 SSO

`std::string` 对象本身有多大，与它存的内容多长是两件事。

`C++`

```cpp
/* str_size.cpp    编译：g++ -std=c++17 str_size.cpp -o str_size */
#include <iostream>
#include <string>
#include <string_view>

int main() {
    std::cout << "sizeof(std::string)      = " << sizeof(std::string) << "\n";
    std::cout << "sizeof(std::wstring)     = " << sizeof(std::wstring) << "\n";
    std::cout << "sizeof(std::string_view) = " << sizeof(std::string_view) << "\n";
    std::cout << "sizeof(char *)           = " << sizeof(char *) << "\n\n";

    std::cout << "长度  容量  数据是否在对象内部（SSO）\n";
    for (int n = 0; n <= 20; ++n) {
        std::string s(static_cast<std::size_t>(n), 'x');
        const char *p = s.data();
        const char *a = reinterpret_cast<const char *>(&s);
        const bool inside = (p >= a && p < a + sizeof(s));
        std::cout << "  " << n << "    " << s.capacity() << "   "
                  << (inside ? "是（对象内）" : "否（堆上）") << "\n";
        if (n == 0 || n == 15 || n == 16) {
            std::cout << "       （空串的 data() 也指向对象内部，长度 0）\n";
        }
    }
    return 0;
}
```

`实测数据`
`Text`

```text
sizeof(std::string)      = 32
sizeof(std::wstring)     = 32
sizeof(std::string_view) = 16
sizeof(char *)           = 8

长度  容量  数据是否在对象内部（SSO）
  0    15   是（对象内）
       （空串的 data() 也指向对象内部，长度 0）
  1    15   是（对象内）
  …
  15    15   是（对象内）
       （空串的 data() 也指向对象内部，长度 0）
  16    16   否（堆上）
       （空串的 data() 也指向对象内部，长度 0）
  17    17   否（堆上）
  …
  20    20   否（堆上）
```

**本机 libstdc++ 的两个数字**：`sizeof(std::string)` 是 32 字节，
**长度到 15 字节为止数据都在对象内部，16 字节起改用堆**。
这就是小字符串优化（SSO）：用对象里一块固定大小的空间存放短内容，省掉一次堆分配。

**16 到 20 这几行的容量等于长度**，是因为这些对象是用 `std::string(n, 'x')` 构造的，
构造函数按需分配，不多给。下一小节的增长曲线是另外一件事。

`待确认`

**「15 字节」是 libstdc++ 的选择，不是标准的规定。**
标准只要求 `data()`、`size()`、`capacity()` 满足若干条件，没有规定阈值与对象大小。
换一套标准库（MSVC STL、libc++）这两个数字都可能不同，
**判断办法是在本机按上面的做法实测一遍**。

## 2.3 容量增长曲线

`std::string` 无法预知还要追加多少，所以按「不够就翻倍」的策略扩容。

`C++`

```cpp
/* str_growth.cpp    编译：g++ -std=c++17 str_growth.cpp -o str_growth */
#include <iostream>
#include <string>

int main() {
    std::string s;
    std::size_t last = s.capacity();
    std::cout << "初始：size=" << s.size() << " capacity=" << last << "\n";

    for (int i = 0; i < 100; ++i) {
        s.push_back('x');
        if (s.capacity() != last) {
            std::cout << "第 " << i + 1 << " 次 push_back 之后：size=" << s.size()
                      << " capacity=" << last << " -> " << s.capacity() << "\n";
            last = s.capacity();
        }
    }
    std::cout << "最终：size=" << s.size() << " capacity=" << s.capacity() << "\n";

    // 先 reserve 再追加，分配次数减少
    std::string r;
    r.reserve(100);
    std::cout << "reserve(100) 之后：capacity=" << r.capacity() << "\n";

    // shrink_to_fit 只是请求，不保证缩小
    std::string k(100, 'k');
    k.resize(10);
    std::cout << "resize(10) 之后：size=" << k.size() << " capacity=" << k.capacity() << "\n";
    k.shrink_to_fit();
    std::cout << "shrink_to_fit() 之后：size=" << k.size() << " capacity=" << k.capacity() << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
初始：size=0 capacity=15
第 16 次 push_back 之后：size=16 capacity=15 -> 30
第 31 次 push_back 之后：size=31 capacity=30 -> 60
第 61 次 push_back 之后：size=61 capacity=60 -> 120
最终：size=100 capacity=120
reserve(100) 之后：capacity=100
resize(10) 之后：size=10 capacity=100
shrink_to_fit() 之后：size=10 capacity=15
```

**三次扩容，每次翻倍**：15 → 30 → 60 → 120。
**分配次数是对数级的**，这是「不预先 reserve 也能用」的原因；
但每一次扩容都要把内容搬到新缓冲区，**旧缓冲区上的指针与引用全部作废**（第 4 节）。
最后一次 `shrink_to_fit` 之后容量回到 15，说明 10 个字符又回到了对象内部。

`文档`

> "`void reserve(size_type res_arg=0);` … Effects: After `reserve()`,
> `capacity()` is greater or equal to the argument of `reserve()`. [Note:
> Calling `reserve()` with a `res_arg` argument less than `capacity()` is in
> effect a non-binding shrink request. …] `void shrink_to_fit();` Effects:
> `shrink_to_fit` is a non-binding request to reduce `capacity()` to `size()`.
> … It does not increase `capacity()`, but may reduce `capacity()` by causing
> reallocation. … Remarks: Reallocation invalidates all the references,
> pointers, and iterators referring to the elements in the sequence as well as
> the past-the-end iterator. If no reallocation happens, they remain valid."
>
> —— N4659 §24.3.2.4/10、11、13、15

**两个词值得注意**：「non-binding request」意思是**请求，不是命令**。
标准允许实现不理它。实测里 `shrink_to_fit()` 确实缩小了，但代码不能依赖这一点。

> [!TIP]
> **什么时候该 `reserve`**：已经知道大概要装多少内容时（读一个大文件、拼一段报表）。
> **什么时候不该**：只是「觉得可能要大」时——多要的内存会一直占着，
> 而 `shrink_to_fit` 只是请求，未必收得回来。

---

# 第 3 节 修改与查找

## 3.1 追加、插入、删除、替换

`C++`

```cpp
/* str_api.cpp    编译：g++ -std=c++17 str_api.cpp -o str_api */
#include <iostream>
#include <string>

int main() {
    std::string s = "hello";
    s.append(" world");                       // 尾部追加
    s.insert(5, ",");                         // 在下标 5 处插入
    std::cout << "append + insert：" << s << "（" << s.size() << " 字节）\n";

    s.replace(5, 1, ":");                     // 把下标 5 起的 1 个字符换成冒号
    std::cout << "replace：" << s << "\n";

    s.erase(0, 6);                            // 删掉开头 6 个字符
    std::cout << "erase：" << s << "\n";

    std::cout << "substr(0, 5)：" << s.substr(0, 5) << "\n";
    std::cout << "find('o')：" << s.find('o') << "\n";
    std::cout << "rfind('o')：" << s.rfind('o') << "\n";
    std::cout << "find(\"zz\")：" << s.find("zz") << "（npos = "
              << std::string::npos << "）\n";
    std::cout << "find_first_of(\"ol\")：" << s.find_first_of("ol") << "\n";
    std::cout << "find_last_not_of(\"d\")：" << s.find_last_not_of("d") << "\n";

    const std::string a = "abc", b = "abd";
    std::cout << "compare(abc, abd)：" << a.compare(b) << "（负数表示 a 小）\n";
    std::cout << "运算符比较：" << (a < b) << " " << (a == b) << "\n";

    char buf[4] = {};
    const std::size_t n = s.copy(buf, 3, 0);  // 拷贝且不追加空字符
    buf[n] = '\0';                            // 自己补上才能当 C 字符串用
    std::cout << "copy 前 3 个字符：" << buf << "\n";

    s.clear();
    std::cout << "clear() 之后：size=" << s.size() << " empty=" << s.empty()
              << " capacity=" << s.capacity() << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
append + insert：hello, world（12 字节）
replace：hello: world
erase： world
substr(0, 5)： worl
find('o')：2
rfind('o')：2
find("zz")：18446744073709551615（npos = 18446744073709551615）
find_first_of("ol")：2
find_last_not_of("d")：4
compare(abc, abd)：-1（负数表示 a 小）
运算符比较：1 0
copy 前 3 个字符： wo
clear() 之后：size=0 empty=1 capacity=15
```

**`npos` 是 `size_t` 的最大值**（本机 18446744073709551615），
因此所有查找函数返回的都是 `std::string::size_type`。
**用 `int` 去接 `find` 的返回值是一个常见的坑**：找不到时那个巨大的数会被截断。

**`clear()` 不释放容量**：`size` 归零，`capacity` 还是 15。
想真把内存还回去要 `shrink_to_fit()`，而它只是请求。

## 3.2 查找函数一览

| 函数 | 找什么 | 从哪端开始 |
|---|---|---|
| `find(s)` | 子串第一次出现 | 头 |
| `rfind(s)` | 子串最后一次出现 | 尾 |
| `find_first_of(set)` | **集合中任意一个字符**第一次出现 | 头 |
| `find_last_of(set)` | 集合中任意一个字符最后一次出现 | 尾 |
| `find_first_not_of(set)` | 第一个**不在**集合里的字符 | 头 |
| `find_last_not_of(set)` | 最后一个不在集合里的字符 | 尾 |

**`find` 找的是一整段子串，`find_first_of` 找的是「集合里的任意一个字符」**，
这两个函数的名字很像，用途完全不同。去掉字符串两端的空白，
惯用写法就是 `find_first_not_of(" \t\n\r")` 配 `find_last_not_of(" \t\n\r")`。

所有查找函数都可以带第二个参数 `pos`（从哪一个下标开始找），
返回值同样可能是 `npos`。

## 3.3 拼接的代价

`+` 看起来只是「接起来」，实际上每一步都可能新建一个对象。

`C++`

```cpp
/* str_concat.cpp    编译：g++ -std=c++17 str_concat.cpp -o str_concat */
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

static long g_allocs = 0;                       // 全局分配计数

void *operator new(std::size_t n) {             // 替换全局 operator new
    ++g_allocs;
    if (void *p = std::malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

int main() {
    const std::string a = "0123456789abcdef";   // 16 字节，超出 SSO 的门槛
    const std::string b = "ABCDEFGHIJKLMNOP";
    long before = 0;

    before = g_allocs;
    std::string s;
    for (int i = 0; i < 100; ++i) {
        s = s + a + b;                          // 每一步都产生临时对象
    }
    std::printf("s = s + a + b      100 次：分配 %3ld 次，最终长度 %zu\n",
                g_allocs - before, s.size());

    before = g_allocs;
    std::string t;
    for (int i = 0; i < 100; ++i) {
        t += a;                                 // 原地追加
        t += b;
    }
    std::printf("t += a; t += b;     100 次：分配 %3ld 次，最终长度 %zu\n",
                g_allocs - before, t.size());

    before = g_allocs;
    std::string u;
    u.reserve(100 * (a.size() + b.size()));     // 一次把容量要够
    for (int i = 0; i < 100; ++i) {
        u += a;
        u += b;
    }
    std::printf("reserve 后再追加     100 次：分配 %3ld 次，最终长度 %zu\n",
                g_allocs - before, u.size());
    return 0;
}
```

`实测数据`
`Text`

```text
s = s + a + b      100 次：分配 200 次，最终长度 3200
t += a; t += b;     100 次：分配   8 次，最终长度 3200
reserve 后再追加     100 次：分配   1 次，最终长度 3200
```

**三种写法，三个数量级**：

| 写法 | 100 轮的分配次数 | 说明 |
|---|---|---|
| `s = s + a + b` | 200 | 每轮两次临时对象，各分配一次 |
| `t += a; t += b;` | 8 | 原地追加，只在容量不够时扩容 |
| `u.reserve(...)` 之后 `+=` | 1 | 一次把容量要够，此后不再扩容 |

**`operator+` 返回的是新对象**，这是它必须分配的原因；
`+=`／`append` 改的是自己，所以只在扩容时分配。

> [!TIP]
> **循环里拼字符串**：先 `reserve`，再用 `+=`／`append`。
> 需要把多个值拼成一段文本时，`std::ostringstream` 也很好用
> （见《07-标准库/B-01-输入输出：iostream.md》第 6.2 小节），
> 它同样避免了「每加一次就复制一遍」的开销。

---

# 第 4 节 迭代器与引用失效

## 4.1 标准的规则

`文档`

> "References, pointers, and iterators referring to the elements of a
> `basic_string` sequence may be invalidated by the following uses of that
> `basic_string` object: —(4.1) as an argument to any standard library function
> taking a reference to non-const `basic_string` as an argument. —(4.2) Calling
> non-const member functions, except `operator[]`, `at`, `data`, `front`,
> `back`, `begin`, `rbegin`, `end`, and `rend`."
>
> —— N4659 §24.3.2.1/4

**读成一句话**：只要调用非常量成员函数，之前拿到的指针、引用、迭代器都可能作废；
**唯一例外是那几个观察函数**（`operator[]`、`at`、`data`、`front`、`back`、
`begin`/`rbegin`/`end`/`rend`）。

| 操作 | 会不会让 `data()`／`c_str()` 失效 |
|---|---|
| `operator[]`、`at`、`front`、`back` | 不会 |
| `data()`、`c_str()`、`size()`、`capacity()` | 不会 |
| `+=`、`append`、`push_back`、`insert`、`erase`、`replace` | **可能**（容量不够就重新分配） |
| `reserve`、`resize`、`shrink_to_fit` | **可能** |
| `clear` | 清空内容，但不释放容量（**实测**） |
| `swap`、移动赋值 | 指针跟着缓冲区走，含义变了 |
| 作为非 const 引用传给标准库函数（如 `std::getline`） | **可能** |

**「可能」的意思是：容量够时指针不变，容量不够时全部作废。** 写代码时按「一定作废」处理。

## 4.2 悬垂的指针

`C++`

```cpp
/* str_dangling.cpp    编译：g++ -std=c++17 -g -fsanitize=address str_dangling.cpp -o str_dangling */
#include <cstdio>
#include <string>

int main() {
    std::string s(20, 'a');                    // 超过 15 字节，缓冲区在堆上
    const char *p = s.c_str();                 // 记下指向堆缓冲区的指针
    std::printf("追加之前：%s\n", p);

    s += "0123456789abcdefghij";               // 容量不够，重新分配，旧缓冲区被释放
    std::printf("追加之后 s.data()：%s\n", s.data());
    std::printf("用旧指针读：%s\n", p);          // 旧指针已经悬垂
    return 0;
}
```

MinGW 的 `g++` 没有 AddressSanitizer（链接时报 `cannot find -lasan`），
这一段在 WSL Ubuntu 24.04 上用 `g++` 13.3.0 跑：

`实测数据`
`Text`

```text
==395==ERROR: AddressSanitizer: heap-use-after-free on address 0x503000000040
READ of size 2 at 0x503000000040 thread T0
    #0 … in printf_common …
    #1 … in vprintf …
    #2 … in printf …
    #3 0x564b3da90497 in main str_dangling.cpp:12
    …

0x503000000040 is located 0 bytes inside of 21-byte region [0x503000000040,0x503000000055)
freed by thread T0 here:
    #0 … in operator delete(void*) …
    #1 … in std::__cxx11::basic_string<…>::_M_mutate(…) …
    #2 … in std::__cxx11::basic_string<…>::_M_append(…) …
    #3 0x564b3da90456 in main str_dangling.cpp:10
    …

previously allocated by thread T0 here:
    #0 … in operator new(unsigned long) …
    #1 … in std::__cxx11::basic_string<…>::_M_construct(unsigned long, char) …
    #2 0x564b3da9064f in main str_dangling.cpp:6
    …

SUMMARY: AddressSanitizer: heap-use-after-free … in printf_common
==395==ABORTING
```

**报告把三段调用栈都给了**：这一行在哪读（`str_dangling.cpp:12`）、
缓冲区在哪被释放（第 10 行的 `+=` 里，`_M_append` 调 `_M_mutate` 重新分配）、
当初在哪分配（第 6 行的构造函数）。**「谁释放的」这一栏正是判断失效原因的关键**。

## 4.3 SSO 会让问题暂时藏着

把上例的初始内容改成 3 个字节，同样的代码就不再报错：

`C++`

```cpp
/* str_stale_sso.cpp    编译：g++ -std=c++17 -g -fsanitize=address str_stale_sso.cpp -o str_stale_sso */
#include <cstdio>
#include <string>

int main() {
    std::string s = "abc";                     // 只有 3 字节，缓冲区在对象内部
    const char *p = s.c_str();
    std::printf("追加之前：%s\n", p);

    s += "0123456789abcdefghij";               // 超过 15 字节，改用堆缓冲区
    std::printf("追加之后 s.data()：%s\n", s.data());
    std::printf("用旧指针读：[%s]\n", p);        // 过期数据，工具不报错
    return 0;
}
```

`实测数据`
`Text`

```text
追加之前：abc
追加之后 s.data()：abc0123456789abcdefghij
用旧指针读：[]
```

**AddressSanitizer 没有报错。** 因为旧指针指向的是**对象内部的那块空间**，
对象本身还活着，那块内存没有被释放——只是里面已经不再是 `"abc"` 了。
读到的是一个空串，**一个不崩、不报错的错值**。

> [!CAUTION]
> **短字符串上的悬垂最难查。** 长内容下它立刻崩或报错，短内容下它安静地给出错误结果。
> 一段在测试里（短输入）正常的代码，换到生产数据（长输入）就可能读到垃圾。
> **防线不是工具，而是规则**：每次修改之后重新取 `data()`／`c_str()`，
> 不把指针存起来跨过修改操作。

## 4.4 安全的写法

| 想做的事 | 不安全 | 安全 |
|---|---|---|
| 反复读某个位置 | 存一个 `const char *` | 存下标，用 `s[i]` |
| 遍历中修改 | 存迭代器 | 用下标循环，或先记下要改的位置 |
| 传给 C 函数 | 长期保存 `c_str()` | 调用前现取，用完即弃 |
| 反复追加 | 每次重新分配 | 先 `reserve`，追加完再取指针 |

`reserve` 能减少扩容次数，**但不能作为「指针永远有效」的保证**：
一旦追加超过预留量，还是会重新分配。它能做的是「在已知上限时把扩容次数降到 0」。

---

# 第 5 节 与 C 字符串互操作

## 5.1 `c_str()` 与 `data()`

`文档`

> "`const charT* c_str() const noexcept;` `const charT* data() const noexcept;`
> Returns: A pointer `p` such that `p + i == &operator[](i)` for each `i` in
> `[0, size()]`. Complexity: Constant time. Requires: The program shall not
> alter any of the values stored in the character array. `charT* data() noexcept;`
> Returns: A pointer `p` such that `p + i == &operator[](i)` for each `i` in
> `[0, size()]`."
>
> —— N4659 §24.3.2.7.1/1、4

**三处要点**：

1. `i` 取到 `size()`，说明 `p[size()]` 是合法的——它就是结尾的那个空字符，
   所以 `c_str()` 可以直接交给 `printf("%s")`、`fopen`、`strlen`。
2. **C++17 起 `data()` 也有非 const 版本**，可以写进去；
   但写 `p[size()]` 是禁止的（那是结尾空字符的位置）。
3. **`C++17` 之前 `data()` 不保证以空字符结尾**，`c_str()` 才保证。写 C++17 基准的代码时两者等价。

`C++`

```cpp
/* str_cinterop.cpp    编译：g++ -std=c++17 str_cinterop.cpp -o str_cinterop */
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

int main() {
    std::string s = "hello";

    // c_str()：交给 C 的函数
    std::printf("交给 printf：%s\n", s.c_str());
    std::printf("交给 strlen：%zu\n", std::strlen(s.c_str()));

    // data() 的非 const 版本：C++17 起可以改内容，但不能改 p[size()]
    char *p = s.data();
    p[0] = 'H';
    std::cout << "改过之后：" << s << "\n";

    // copy：把一段字符拷到自己的缓冲区，不追加空字符
    char buf[8] = {};
    const std::size_t n = s.copy(buf, 5, 0);
    buf[n] = '\0';                       // 这一步不能省
    std::printf("copy 出来：%s（返回 %zu）\n", buf, n);

    // 中间没有空字符的串也能处理
    const std::string raw("a\0b", 3);
    std::cout << "内嵌空字符的串：size=" << raw.size() << "\n";
    std::printf("交给 printf 只剩：%s\n", raw.c_str());
    return 0;
}
```

`实测数据`
`Text`

```text
交给 printf：hello
交给 strlen：5
改过之后：Hello
copy 出来：Hello（返回 5）
内嵌空字符的串：size=3
交给 printf 只剩：a
```

**最后两行是一对**：`size()` 说 3，`printf` 只看到 `a`。
`std::string` 的长度与「以空字符结尾」是两套信息，交给 C 的接口时只剩后者。

## 5.2 `c_str()` 的指针什么时候失效

一句话：**只要之后调用了可能引起重新分配的成员函数，它就是野指针**（第 4 节）。
把它当参数**当次用完**是安全的：

`C++`

```cpp
// （下面是节选）安全的两种写法
std::printf("%s\n", s.c_str());          // 当次用完，安全
const auto len = std::strlen(s.c_str()); // 只取长度，指针不留下，安全

// 危险的写法
const char *p = s.c_str();               // 存起来
s += "more";                             // 这一行之后 p 可能已经悬垂
std::printf("%s\n", p);                   // 未定义行为
```

> [!TIP]
> **需要一个「稳定的 C 字符串」时，自己复制一份**：
> `std::vector<char> buf(s.begin(), s.end()); buf.push_back('\0');`
> 或者用 `s.copy()` 拷进自己的缓冲区（记得补空字符）。
> 这两条路都把生命期握在自己手里。

## 5.3 什么时候真的需要 C 字符串

| 场合 | 说明 |
|---|---|
| 调 C 库函数 | `fopen`、`stat`、`printf`、socket 接口都要 `const char *` |
| 系统调用 | Windows 的 `CreateFileA`、POSIX 的 `open` |
| 与环境变量、`argv` 交互 | 它们本来就是 `char *` |
| 写成文件或网络包 | 通常直接写字节（`data()` + `size()`），不必经过 `c_str()` |

**最后一个场合值得强调**：写二进制数据时用 `s.data()` 与 `s.size()`，
**不要用 `strlen(s.c_str())`**——内嵌空字符会被截断。

---

# 第 6 节 `std::string_view`

## 6.1 为什么需要它

**函数参数写成 `const std::string &` 有一个隐含要求：调用者手上必须有一个 `std::string`。**
传进来一个 `const char *`，就会先构造一个临时 `std::string`（一次分配 + 一次复制）；
要在长字符串的中间取一段，`substr` 也要复制一份。

`string_view` 是「别人的字符序列的一个视图」：只记指针与长度，不拥有内容。
它的 `substr` 只挪指针，所以是常数时间：

`文档`

> "The class template `basic_string_view` describes an object that can refer to a
> constant contiguous sequence of char-like objects with the first element of
> the sequence at position zero. … The complexity of `basic_string_view` member
> functions is O(1) unless otherwise specified."
>
> —— N4659 §24.4/1、3

分配次数可以直接数出来：

`C++`

```cpp
/* sv_alloc.cpp    编译：g++ -std=c++17 sv_alloc.cpp -o sv_alloc */
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <string_view>

static long g_allocs = 0;                       // 全局分配计数

void *operator new(std::size_t n) {             // 替换全局 operator new
    ++g_allocs;
    if (void *p = std::malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

int main() {
    // 这一行长 40 字节，取前 24 个字符已经超出 SSO 的 15 字节
    const std::string line = "2024-06-01 12:34:56 级别=INFO 消息=启动完成";
    const std::string_view whole(line);
    std::size_t total = 0;

    long before = g_allocs;
    for (int i = 0; i < 1000; ++i) {
        const std::string d = line.substr(0, 24);   // 每次都新建一个 string
        total += d.size();
    }
    std::printf("std::string::substr 1000 次：分配 %ld 次\n", g_allocs - before);

    before = g_allocs;
    for (int i = 0; i < 1000; ++i) {
        const std::string_view v(line.data(), 24);  // 只记指针与长度
        total += v.size();
    }
    std::printf("string_view 直接构造 1000 次：分配 %ld 次\n", g_allocs - before);

    before = g_allocs;
    for (int i = 0; i < 1000; ++i) {
        const std::string_view v = whole.substr(0, 24);   // 视图的 substr
        total += v.size();
    }
    std::printf("string_view::substr 1000 次：分配 %ld 次\n", g_allocs - before);

    before = g_allocs;
    for (int i = 0; i < 1000; ++i) {
        const std::string back(whole.substr(0, 24));    // 转回 string 时又要分配
        total += back.size();
    }
    std::printf("视图转回 std::string 1000 次：分配 %ld 次\n", g_allocs - before);
    std::printf("（累计字节数 %zu，防止循环被优化掉）\n", total);
    return 0;
}
```

`实测数据`
`Text`

```text
std::string::substr 1000 次：分配 1000 次
string_view 直接构造 1000 次：分配 0 次
string_view::substr 1000 次：分配 0 次
视图转回 std::string 1000 次：分配 1000 次
```

**一千次取子串：`std::string` 版一千次分配，视图版零次。**
最后一行同样重要：**视图一旦要变回 `std::string`，复制就回来了**——
`string_view` 只是把复制推迟到真正需要的时候。

## 6.2 它是「指针 + 长度」

`C++`

```cpp
/* sv_basic.cpp    编译：g++ -std=c++17 sv_basic.cpp -o sv_basic */
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>

int main() {
    std::cout << "sizeof(std::string_view) = " << sizeof(std::string_view) << "\n";

    const std::string line = "2024-06-01 12:34:56 级别=INFO";
    const std::string_view v(line);

    const std::string_view date = v.substr(0, 10);     // 只挪指针，不复制
    const std::string_view tm   = v.substr(11, 8);
    std::cout << "日期：" << date << "  时间：" << tm << "\n";
    std::cout << "两个视图相对原串的偏移：" << (date.data() - line.data())
              << " 和 " << (tm.data() - line.data()) << "\n";

    const std::string with_sub = line.substr(0, 10);   // std::string 版：会分配
    std::cout << "std::string 版的结果一样：" << with_sub
              << "，但它是一个新对象（" << with_sub.size() << " 字节）\n";

    // 指向字符串中段的视图没有空字符结尾
    const std::string text = "abcdef";
    const std::string_view mid(text.data() + 2, 2);    // 只看 "cd"
    std::cout << "mid.size() = " << mid.size() << "，输出 mid = " << mid << "\n";
    std::printf("把 mid.data() 当 C 字符串：%s\n", mid.data());   // 越过了视图的末尾
    return 0;
}
```

`实测数据`
`Text`

```text
sizeof(std::string_view) = 16
日期：2024-06-01  时间：12:34:56
两个视图相对原串的偏移：0 和 11
std::string 版的结果一样：2024-06-01，但它是一个新对象（10 字节）
mid.size() = 2，输出 mid = cd
把 mid.data() 当 C 字符串：cdef
```

（第三行与第四行的偏移量就是视图的全部内容：**指针从哪开始、长度多少**。）

**最后两行是一处静默错误**：`mid` 的长度是 2，打印它是 `cd`；
但 `mid.data()` 指向的仍是原串，`printf` 一直读到原串末尾，
多打印了 `ef`。**视图不保证空字符结尾，这是它最常见的坑。**

## 6.3 零拷贝的代价

| 代价 | 说明 |
|---|---|
| **不拥有内容** | 原字符串销毁、被修改、被移动之后，视图悬垂 |
| **不保证空字符结尾** | 不能直接交给 `printf("%s")`、`strlen`、`fopen` |
| **不能改内容** | 元素是 `const char` |
| **转回 `std::string` 要复制** | 该分配还是得分配 |
| **不是所有接口都接受它** | C++17 的 `<fstream>`、`<filesystem>` 大多要 `const char *` 或 `path` |

悬垂的后果可以用 AddressSanitizer 看到：

`C++`

```cpp
/* sv_dangling.cpp    编译：g++ -std=c++17 -g -fsanitize=address sv_dangling.cpp -o sv_dangling */
#include <iostream>
#include <string>
#include <string_view>

std::string_view make_view() {
    std::string tmp = "这行文字属于一个临时对象，函数返回后它就没了";
    return std::string_view(tmp);     // 视图指向 tmp 的内部缓冲区
}                                      // tmp 在这里销毁

int main() {
    const std::string_view v = make_view();
    std::cout << "视图长度：" << v.size() << "\n";
    std::cout << "视图内容：" << v << "\n";   // 读已释放的内存
    return 0;
}
```

`实测数据`
`Text`

```text
==408==ERROR: AddressSanitizer: heap-use-after-free on address 0x507000000090
READ of size 66 at 0x507000000090 thread T0
    #0 … in fwrite …
    #1 … in std::__ostream_insert<char, std::char_traits<char>>(…)
    #2 … in std::operator<< <char, std::char_traits<char>>(…, std::basic_string_view<…>)
    #3 0x5555d395e830 in main sv_dangling.cpp:14
    …

0x507000000090 is located 0 bytes inside of 67-byte region [0x507000000090,0x5070000000d3)
freed by thread T0 here:
    #0 … in operator delete(void*) …
    #1 0x5555d395e631 in make_view() sv_dangling.cpp:9
    #2 0x5555d395e786 in main sv_dangling.cpp:12
    …

previously allocated by thread T0 here:
    #0 … in operator new(unsigned long) …
    #1 … in std::__cxx11::basic_string<…>::_M_construct<char const*>(…) …
    #3 0x5555d395e5f1 in make_view() sv_dangling.cpp:7
    …

SUMMARY: AddressSanitizer: heap-use-after-free … in fwrite
==408==ABORTING
```

**「freed by … in `make_view()`」这一栏直接点出了问题所在**：
内存在 `make_view` 里分配、也在 `make_view` 里释放，返回出去的视图指着它。

## 6.4 什么时候不该用 `string_view`

| 情形 | 换什么 | 原因 |
|---|---|---|
| 函数要返回一段文本 | 返回 `std::string` | 视图可能指向已被销毁的对象 |
| 要把结果存进容器长期保存 | `std::string` | 视图不拥有内容，原串没了它就没了 |
| 内容之后还要修改 | `std::string` | 视图不能改 |
| 要交给 C 接口（`printf`、`fopen`） | `std::string`，或自己补空字符 | 视图不保证空字符结尾 |
| 只是当参数读一下 | **就用 `string_view`** | 这正是它的用途 |
| 解析一段已有缓冲区（配置、协议） | **就用 `string_view`** | 零拷贝、可以随便切 |

**一条实用的规则**：**参数用 `string_view`，返回值用 `std::string`。**
参数的生命期由调用者保证，返回值的生命期必须由被调用者负责。

> [!CAUTION]
> **不要把 `string_view` 用在「构造函数存下来以后再用」的地方。**
> 一个类里存 `std::string_view` 成员，等于要求调用者保证那块内存在对象整个生命期内有效——
> 这条约定难检查、难维护，出问题时又没有任何提示。

---

# 第 7 节 数字与字符串互转

## 7.1 两个方向各有一族函数

| 方向 | C++11 起 | C++17 起 | C 的对应物 |
|---|---|---|---|
| 数字 → 字符串 | `std::to_string` | `std::to_chars` | `sprintf`/`snprintf` |
| 字符串 → 数字 | `std::stoi`/`stol`/`stod` | `std::from_chars` | `atoi`/`strtol`/`strtod` |

**两族函数的错误处理方式完全不同**：`stoi` 抛异常，`from_chars` 返回结果码。

`C++`

```cpp
/* str_num.cpp    编译：g++ -std=c++17 str_num.cpp -o str_num */
#include <charconv>
#include <iomanip>
#include <iostream>
#include <string>
#include <system_error>

int main() {
    // 数字 -> 字符串
    std::cout << "to_string(42)      = " << std::to_string(42) << "\n";
    std::cout << "to_string(3.14159) = " << std::to_string(3.14159) << "\n";
    std::cout << "to_string(true)    = " << std::to_string(true) << "\n";

    char buf[32] = {};
    const auto tc = std::to_chars(buf, buf + sizeof buf, 3.14159);
    std::cout << "to_chars(3.14159)  = " << std::string(buf, tc.ptr)
              << "（结果码 " << (tc.ec == std::errc() ? "成功" : "失败") << "）\n";
    const auto ti = std::to_chars(buf, buf + sizeof buf, 255, 16);
    std::cout << "to_chars(255, 16)  = " << std::string(buf, ti.ptr) << "\n";

    // 字符串 -> 数字：stoi 用异常报告失败
    try {
        std::size_t used = 0;
        const int v = std::stoi("123abc", &used, 10);
        std::cout << "stoi(\"123abc\") = " << v << "，吃掉了 " << used << " 个字符\n";
    } catch (const std::exception &e) {
        std::cout << "stoi 抛异常：" << e.what() << "\n";
    }
    try {
        const int v = std::stoi("abc");
        std::cout << "stoi(\"abc\") = " << v << "\n";
    } catch (const std::exception &e) {
        std::cout << "stoi(\"abc\") 抛异常：" << e.what() << "\n";
    }
    try {
        const int v = std::stoi("99999999999999");
        std::cout << "stoi 溢出得到 " << v << "\n";
    } catch (const std::exception &e) {
        std::cout << "stoi 溢出抛异常：" << e.what() << "\n";
    }

    // 字符串 -> 数字：from_chars 用结果码，不抛
    const std::string src = "123abc";
    int value = 0;
    const auto r = std::from_chars(src.data(), src.data() + src.size(), value);
    std::cout << "from_chars(\"123abc\")：值 = " << value
              << "，停在偏移 " << (r.ptr - src.data())
              << "，结果码 = " << (r.ec == std::errc() ? "成功" : std::make_error_code(r.ec).message())
              << "\n";

    const std::string bad = "abc";
    const auto r2 = std::from_chars(bad.data(), bad.data() + bad.size(), value);
    std::cout << "from_chars(\"abc\")：结果码 == invalid_argument 吗："
              << (r2.ec == std::errc::invalid_argument) << "\n";

    const std::string big = "99999999999999";    // 14 位，超出了 int 的范围
    int small = 0;
    const auto r3 = std::from_chars(big.data(), big.data() + big.size(), small);
    std::cout << "from_chars 溢出：结果码 == result_out_of_range 吗："
              << (r3.ec == std::errc::result_out_of_range) << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
to_string(42)      = 42
to_string(3.14159) = 3.141590
to_string(true)    = 1
to_chars(3.14159)  = 3.14159（结果码 成功）
to_chars(255, 16)  = ff
stoi("123abc") = 123，吃掉了 3 个字符
stoi("abc") 抛异常：stoi
stoi 溢出抛异常：stoi
from_chars("123abc")：值 = 123，停在偏移 3，结果码 = 成功
from_chars("abc")：结果码 == invalid_argument 吗：1
from_chars 溢出：结果码 == result_out_of_range 吗：1
```

## 7.2 `to_string` 与 `stoi` 的几个细节

**`to_string(3.14159)` 得到 `3.141590`（6 位小数）**，
它内部等价于 `sprintf("%f")`，即固定 6 位；
`to_chars(3.14159)` 得到 `3.14159`，**默认给出能精确还原的最短表示**。
两者不是同一个东西，写浮点时需要留意选哪一个。

**`stoi("123abc")` 返回 123**，并把下标 `3` 写进 `used`——
它只要求前缀能转换，不要求整串都是数字。
**要让「整串必须是数字」成立，须自行检查 `used == str.size()`。**

`stoi` 的异常类型与条件写在标准里：

`文档`

> "Throws: `invalid_argument` if `strtol`, `strtoul`, `strtoll`, or `strtoull`
> reports that no conversion could be performed. Throws `out_of_range` if
> `strtol`, `strtoul`, `strtoll` or `strtoull` sets `errno` to `ERANGE`, or if
> the converted value is outside the range of representable values for the
> return type."
>
> —— N4659 §24.3.4/3

**实测里两种失败的 `what()` 都是 `stoi`**——只给出函数名，
不给是哪个字符串、哪一位出的问题。要区分「不是数字」与「超出范围」，
须 `catch` 两个不同的异常类型：

`C++`

```cpp
// （下面是节选）区分两种失败
try {
    const int v = std::stoi(text);
    use(v);
} catch (const std::invalid_argument &) {
    // 一个数字都没有
} catch (const std::out_of_range &) {
    // 数字太大或太小
}
```

## 7.3 `from_chars` 与 `to_chars`

**这两个函数不抛异常、不分配内存、不看 locale**，
结果通过返回的结构体报告：

`C++`

```cpp
// （下面是节选）两个返回类型的形状
struct from_chars_result { const char* ptr; std::error_code ec; };
struct to_chars_result   { char* ptr;       std::error_code ec; };
```

| 情形 | `from_chars` 的结果 |
|---|---|
| 成功 | `ec` 为空（`== std::errc()`），`ptr` 指向第一个没被转换的字符 |
| 一个字符都转换不了 | `ec == std::errc::invalid_argument` |
| 数值超出目标类型的范围 | `ec == std::errc::result_out_of_range` |

**三个实际的好处**：

1. **不抛异常**：可以在 `noexcept` 函数、嵌入式环境、异常被关掉（`-fno-exceptions`）的项目里用；
2. **不用判断 `used == size()` 之外的额外状态**：`ptr` 直接给出停在哪里；
3. **不受 locale 影响**：`stod("1,5")` 会不会按某种语言的逗号小数点解释，
   在 `from_chars` 这里不存在这个问题。

**代价**：`from_chars` 需要自行提供缓冲区与范围（`first`/`last`），
写起来比 `stoi` 啰嗦；浮点版本在早期的标准库里支持得晚，
`<charconv>` 的整数版本是 C++17，**浮点版本到 C++17 后期才补齐**，老编译器上可能缺。

## 7.4 选哪一个

| 情形 | 用哪个 |
|---|---|
| 只是把数字接到日志或界面文本里 | `std::to_string`（简单） |
| 需要固定格式（几位小数、补零） | `std::ostringstream` 加 `<iomanip>`，或 `std::format`（C++20 起） |
| 性能敏感的批量转换 | `std::to_chars`/`std::from_chars` |
| 不允许异常 | `std::from_chars`，或 C 的 `strtol` 配 `errno`（见 A 段数字转换那一节） |
| 解析配置文件、命令行参数 | `std::from_chars`，因为「停在哪里」是重要信息 |

---

# 第 8 节 编码现实

## 8.1 UTF-8 是字节序列

`C++`

```cpp
/* str_encoding.cpp    编译：g++ -std=c++17 str_encoding.cpp -o str_encoding */
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>

static void dump(const std::string &s) {
    for (unsigned char c : s) {
        std::printf("%02X ", c);
    }
    std::printf("\n");
}

int main() {
    const std::string s = "中文abc";
    std::cout << "s.size() = " << s.size() << "（字节数）\n";
    std::cout << "字节序列：";
    dump(s);

    const std::string one = s.substr(0, 1);      // 一个字被切开
    std::cout << "substr(0, 1) 的字节：";
    dump(one);
    std::cout << "substr(0, 3) 的字节：";
    dump(s.substr(0, 3));

    const std::wstring w = L"中文abc";
    std::cout << "wstring.size() = " << w.size()
              << "，sizeof(wchar_t) = " << sizeof(wchar_t) << "\n";

    const char *u8 = u8"中文";
    std::cout << "u8\"中文\" 的字节数 = " << std::string(u8).size() << "\n";

    // 定宽字段里的中文：宽度按字节算，不是按显示宽度
    std::printf("C   ：|%-8s|\n", "轴承");
    std::cout << "C++ ：|" << std::left << std::setw(8) << std::string("轴承") << "|\n";
    return 0;
}
```

`实测数据`
`Text`

```text
s.size() = 9（字节数）
字节序列：E4 B8 AD E6 96 87 61 62 63
substr(0, 1) 的字节：E4
substr(0, 3) 的字节：E4 B8 AD
wstring.size() = 5，sizeof(wchar_t) = 2
u8"中文" 的字节数 = 6
C   ：|轴承  |
C++ ：|轴承  |
```

**逐行说明如下**：

- `"中文abc"` 的 `size()` 是 **9**：两个汉字各 3 字节（UTF-8），加 3 个 ASCII 字节；
- `substr(0, 1)` 取到 **一个字节 `E4`**——这是那个汉字编码的第一个字节，
  **单独拿出来不是合法的 UTF-8**；
- `substr(0, 3)` 才是完整的第一个字；
- `wstring` 的长度是 **5**，因为 Windows 上 `wchar_t` 是 2 字节，用 UTF-16 表示，
  两个汉字各占一个 `wchar_t`（在基本多文种平面内），加 3 个 ASCII；
- `u8"中文"` 是 6 字节，与 `std::string` 里的字节一致；
- **两种写法的定宽字段都是 `|轴承  |`**：宽度按**字节**算（6 字节 + 2 个空格），
  于是两列中文在终端上看起来比 ASCII 窄。

## 8.2 `size()` 是字节数，不是字符数

| 想知道什么 | 用的函数 | 在 UTF-8 下对不对 |
|---|---|---|
| 占多少字节 | `size()` | 对 |
| 有多少个字符 | `size()` | **错**（要自己按编码数） |
| 显示多宽 | `size()` | **错**（还要看字符宽度） |
| 第 5 个字符是什么 | `s[4]` | **错**（那是第 5 个字节） |

**`std::string` 从头到尾不知道编码这件事。** 它就是一个字节数组加上一些好用的成员函数。
C++20 引入了 `char8_t` 与 `std::u8string`，把「这是 UTF-8」写进了类型，
但**那改变的是类型检查，不是「按字符数数」这件事**——
要按字符切分，仍然需要 Unicode 库（ICU、utf8cpp 之类）。

> [!IMPORTANT]
> **C++17 基准下的结论**：把 `std::string` 当成「字节序列」用，
> 所有与显示、截断、对齐相关的事情都按字节算；
> 一旦涉及「第几个字」「多少个字」「截断到 20 个字」，
> 必须交给专门的 Unicode 处理代码。

## 8.3 截断会把字符切坏

上例的 `substr(0, 1)` 就是一次「切坏」。
更常见的场景是「只显示前 20 个字节」：

`C++`

```cpp
// （下面是节选）按字节截断的后果
const std::string s = "中文abc";
const std::string cut = s.substr(0, 4);   // E4 B8 AD E6 —— 第二个字只剩一个字节
std::cout << cut << "\n";                 // 输出里会出现一个替换字符或乱码
```

**要安全地截断，只能按编码规则来**：找到第 n 个字符的边界
（UTF-8 里就是「字节的高两位不是 `10`」的那些位置），再 `substr`。

## 8.4 `wstring` 在 Windows 上的坑

`std::wstring` 的元素类型是 `wchar_t`，而 **`wchar_t` 的宽度是实现定义的**：

| 平台 | `sizeof(wchar_t)` | 编码 |
|---|---|---|
| Windows（MSVC、MinGW） | 2 | UTF-16 |
| Linux（glibc） | 4 | UTF-32 |

`实测数据`
`Text`

```text
sizeof(wchar_t) = 2（Windows 侧实测）
wstring.size() = 5（"中文abc"）
```

**三条后果**：

1. **同一个 `wstring` 在两个平台上的长度可能不同**：Windows 上按 UTF-16 的码元数，
   Linux 上按码点数。辅助平面上的字符（如部分 emoji）在 Windows 上占两个 `wchar_t`。
2. **与系统 API 交界处必须用 `wstring`**：Windows 的 `CreateFileW`、`MessageBoxW` 都是宽字符接口；
   Linux 的文件名接口用 `char *`（通常是 UTF-8 字节）。
3. **在 `wstring` 与 `string` 之间转换需要显式做编码转换**：两者不是同一个编码的两种宽度。

**转换工具的历史包袱**：`std::wstring_convert`（配 `<codecvt>`）是 C++11 引入、
C++17 起弃用的工具。在本机的 libstdc++ 15.2.0 上它还在，但编译会给出弃用警告：

`C++`

```cpp
/* wconv.cpp    编译：g++ -std=c++17 wconv.cpp -o wconv */
#include <codecvt>
#include <iostream>
#include <locale>
#include <string>

int main() {
    // C++17 起 <codecvt> 与 wstring_convert 被列为弃用
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    const std::string utf8 = conv.to_bytes(L"中文");
    std::cout << utf8.size() << "\n";
    return 0;
}
```

`实测数据`
`Text`

```text
wconv.cpp:9:10: warning: 'template<class _Codecvt, class _Elem, class _Wide_alloc,
class _Byte_alloc> class std::__cxx11::wstring_convert' is deprecated
[-Wdeprecated-declarations]
    9 |     std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
      |          ^~~~~~~~~~~~~~~
In file included from …/c++/15.2.0/locale:47:
…/c++/15.2.0/bits/locale_conv.h:262:33: note: declared here
  262 |     class _GLIBCXX17_DEPRECATED wstring_convert
```

**它编译得过去，只是警告**；但既然标准已弃用，新的代码应当改用平台接口
（Windows 上 `MultiByteToWideChar`、`WideCharToMultiByte`）或跨平台的 Unicode 库。

> [!WARNING]
> **本机 MinGW 上还有个更隐蔽的问题**：把 `"中文"` 直接写进 `std::string`
> 依赖的是「源文件是 UTF-8 且编译器不做转换」。MSVC 默认按系统代码页读源文件，
> 需要 `/utf-8` 开关，否则中文会变成乱码——这与 `std::string` 本身无关，
> 是源码编码的问题（见《01-编译器/02-环境配置.md》第 9.6 小节与附录 A.2）。

---

# 速查表

| 件 | 一句话用途 | 典型坑 |
|---|---|---|
| `size()` / `length()` | 字节数 | **不是字符数**，中文一个字 3 字节 |
| `capacity()` | 已分配的容量 | 与 `size()` 不是一回事 |
| `reserve(n)` | 预留容量 | 只保证「不小于 `n`」，不改变 `size()` |
| `shrink_to_fit()` | 请求收缩容量 | 只是请求，不保证 |
| `append` / `+=` | 尾部追加 | 循环里追加前先 `reserve` |
| `operator+` | 拼接成新对象 | **每次都要分配**，见第 3.3 小节 |
| `insert` / `erase` / `replace` | 中段修改 | 会让指针与迭代器失效 |
| `substr(pos, n)` | 取子串 | 返回新对象；`n` 默认到末尾 |
| `find` / `rfind` | 找子串 | 返回 `size_t`，用 `int` 接会截断 |
| `find_first_of` 一族 | 找集合里的任一字符 | 与 `find` 的语义完全不同 |
| `compare` | 三态比较 | 返回负数/0/正数，不是布尔值 |
| `npos` | 「没找到」的标记 | 是 `size_t` 的最大值 |
| `c_str()` / `data()` | 借出 C 字符串指针 | **修改之后可能失效** |
| `copy(buf, n, pos)` | 拷进自己的缓冲区 | **不追加空字符**，须自行补 |
| `std::string_view` | 只读视图，零拷贝 | 不拥有内容、不保证空字符结尾 |
| `to_string` | 数字转字符串 | 浮点是固定 6 位小数 |
| `stoi` / `stod` | 字符串转数字 | 抛异常，`what()` 只有函数名 |
| `from_chars` / `to_chars` | 不抛异常的转换（C++17） | 须自行提供缓冲区与范围 |
| `std::wstring` | 宽字符串 | Windows 2 字节 / Linux 4 字节，长度不一致 |

---

# 术语表

| 词 | 含义 |
|---|---|
| **SSO（小字符串优化）** | 短内容直接放在对象内部，省掉一次堆分配；阈值由实现决定 |
| **容量（capacity）** | 已分配的存储能放多少个字符，通常大于等于 `size()` |
| **重新分配（reallocation）** | 容量不够时申请新缓冲区、搬内容、释放旧的 |
| **失效（invalidation）** | 指针、引用、迭代器不再指向有效内容 |
| **悬垂（dangling）** | 指针指向的对象已经被释放或已被改写 |
| **`npos`** | `size_type` 的最大值，表示「没找到」 |
| **视图（view）** | 只记指针与长度、不拥有内容的对象 |
| **零拷贝** | 不复制内容，只传递指针与长度 |
| **UTF-8** | 一种变长编码，一个字符占 1 到 4 个字节 |
| **码元（code unit）** | 编码的最小单位；UTF-8 是字节，UTF-16 是 16 位 |
| **`wchar_t`** | 宽字符类型，宽度由实现决定（Windows 2 字节、Linux 4 字节） |

---

# 附录 A 复现本章节实测

**环境**：Windows 11，`g++` 15.2.0（MinGW-w64）；AddressSanitizer 的两段在 WSL Ubuntu 24.04 上用
`g++` 13.3.0 跑（MinGW 侧没有 `libasan`，链接会报 `cannot find -lasan`）。
C++ 一律 `-std=c++17`，C 一律 `-std=c23`。

**A.1 与 C 的对照与容量**

`Bash`

```bash
gcc -std=c23    str_task.c   -o str_task_c.exe   && ./str_task_c.exe   > st_c.txt
g++ -std=c++17 str_task.cpp -o str_task_cpp.exe && ./str_task_cpp.exe > st_cpp.txt
g++ -std=c++17 str_size.cpp   -o str_size.exe   && ./str_size.exe
g++ -std=c++17 str_growth.cpp -o str_growth.exe && ./str_growth.exe
```

**A.2 修改、查找与拼接代价**

`Bash`

```bash
g++ -std=c++17 str_api.cpp    -o str_api.exe    && ./str_api.exe
g++ -std=c++17 str_concat.cpp -o str_concat.exe && ./str_concat.exe
g++ -std=c++17 str_cinterop.cpp -o str_cinterop.exe && ./str_cinterop.exe
```

**A.3 失效与悬垂**

`Bash`

```bash
# Windows 侧只看行为，不带 ASan
g++ -std=c++17 str_stale_sso.cpp -o str_stale_sso.exe && ./str_stale_sso.exe

# Linux 侧带 AddressSanitizer
wsl -d Ubuntu -e bash -lc "cd /tmp && \
  g++ -std=c++17 -g -fsanitize=address str_dangling.cpp -o sd && ./sd"
wsl -d Ubuntu -e bash -lc "cd /tmp && \
  g++ -std=c++17 -g -fsanitize=address str_stale_sso.cpp -o ss && ./ss"
wsl -d Ubuntu -e bash -lc "cd /tmp && \
  g++ -std=c++17 -g -fsanitize=address sv_dangling.cpp -o sv && ./sv"
```

**A.4 `string_view` 与数字互转**

`Bash`

```bash
g++ -std=c++17 sv_alloc.cpp -o sv_alloc.exe && ./sv_alloc.exe
g++ -std=c++17 sv_basic.cpp -o sv_basic.exe && ./sv_basic.exe
g++ -std=c++17 str_num.cpp  -o str_num.exe  && ./str_num.exe
```

**A.5 编码**

`Bash`

```bash
g++ -std=c++17 str_encoding.cpp -o str_encoding.exe && ./str_encoding.exe
g++ -std=c++17 wconv.cpp -o wconv.exe            # 期望失败（弃用警告）
```

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《07-标准库/A-02-字符串与内存：string.h.md》第 1 节 | **对照**：`strlen`/`strcpy`/`strcat` 一族 |
| 《07-标准库/A-02-字符串与内存：string.h.md》第 2 节 | **对照**：`memcpy`/`memmove` 与字节操作 |
| 《07-标准库/B-01-输入输出：iostream.md》第 5 节 | 前置：`getline` 读进 `std::string` |
| 《07-标准库/B-00-导读：C++ 标准库与 C 的关系.md》第 3.2 小节 | 前置：`std::size_t` 与 C 名字的关系 |
| 《07-标准库/B-05-数值.md》 | 相关：`<limits>`、浮点比较与容差 |
| 《07-标准库/B-07-文件系统：filesystem.md》第 1 节 | **后续**：`path` 与编码 |
| 《05-类与面向对象/05-拷贝与移动.md》第 2 节 | 前置：拷贝构造与移动构造 |
| 《05-类与面向对象/06-RAII 与资源管理.md》第 2 节 | 前置：析构时释放资源 |
| 《05-类与面向对象/09-运算符重载.md》第 3 节 | 前置：`+`、`[]`、`==` 的重载写法 |
| 《04-语法/13-异常.md》第 1 节 | 前置：异常与 `catch` |
| 《01-编译器/02-环境配置.md》第 9.6 小节 | 相关：源文件编码与中文乱码 |
| `09-高阶数据结构` 板块 | 容器与迭代器的一般失效规则 |

**配套示例见 [`B-examples/07-standard-library/03-cpp-string-text/`](../B-examples/07-standard-library/03-cpp-string-text/)，配套练习见 [`C-templates/07-standard-library/03-cpp-string-text/`](../C-templates/07-standard-library/03-cpp-string-text/)。**
