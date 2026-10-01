# 时间：`<chrono>`

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**计时这件事在 C 里是三个互不相干的函数**：`time` 给秒数，`clock` 给 CPU 时间，
`timespec_get` 给纳秒，单位全靠调用者记。**C++ 换了一个做法——把单位写进类型。**

`std::chrono::milliseconds` 与 `std::chrono::seconds` 是两个不同的类型，
**它们之间不能直接赋值**，要显式转换；`1s + 100ms` 的结果单位由编译器算出来；
**「时刻」与「时长」也是两个类型**，前者叫 `time_point`，后者叫 `duration`。
这一层类型检查把 `time.h` 里最容易写错的那些地方——传错单位、混淆时刻与间隔、
拿一个可能被系统调整的时钟去测耗时——变成了编译错误。

《06-标准库/A-04-时间与日期：time.h.md》已经说明，C 侧那几套时间函数各管一段，
单位与起点全靠调用者记住。**这一层记忆负担在 C++ 里被搬进了类型**：
单位不同的量不能直接赋值，时刻与间隔不能混用，不能单调的时钟不再适合计时——
**这些在 C 里只能靠注释和纪律避免的错误，在这里是编译错误。**
本章节的数字都在本机实际运行验证过：**g++ 15.2.0（MinGW-w64），`-std=c++17`，
Windows 11，系统时区 UTC+8**；涉及平台差异的地方都注明了出处，
没有验证过的推断写成 `待确认`。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 复现环境与命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 整数类型与定点/浮点的区别 | 《04-语法/02-数据类型与类型系统.md》第 2.1 小节 |
| 运算符重载（`duration` 的 `+` 与 `<` 都是重载） | 《05-类与面向对象/09-运算符重载.md》第 1 节 |
| 函数重载与默认实参 | 《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 2 节 |
| 类模板与静态成员（`duration` 靠它们做单位换算） | 《05-类与面向对象/11-模板.md》第 4 节 |
| `time_t` 与 `time`、`difftime` | 《06-标准库/A-04-时间与日期：time.h.md》第 1 节 |
| `struct tm`、`localtime` 与 `gmtime` | 《06-标准库/A-04-时间与日期：time.h.md》第 2 节 |
| `strftime` 的格式符 | 《06-标准库/A-04-时间与日期：time.h.md》第 4 节 |
| 优化等级对耗时的影响 | 《03-构建工具链/05-优化等级.md》第 3.3 小节 |

| 本章节讲什么 | 在哪一节 |
|---|---|
| `duration`：把单位写进类型、转换与截断、时间字面量 | 第 1 节 |
| `time_point` 与三种 `clock` 的区别 | 第 2 节 |
| 怎么测一段代码的耗时，测量结果的分布 | 第 3 节 |
| `sleep_for` / `sleep_until` 与实际睡多久 | 第 4 节 |
| 日历与时区：C++17 的过渡写法与 C++20 的新设施 | 第 5 节 |
| 速查表与配套件 | 第 6 节 |

**相邻的章节**：C 侧的时间函数在《06-标准库/A-04-时间与日期：time.h.md》，
两个章节**讲同一件事的两套工具**，可以逐项对照读；
本章节测出的分布数据与《03-构建工具链/05-优化等级.md》第 3.3 小节的速度对照
是同一类测量，做法也应当一样。

---

# 第 1 节 `duration`：带单位的时长

## 1.1 单位是类型的一部分

`duration` 的定义只有两个模板参数：

`C++`

```cpp
/* （下面是节选）标准里的声明 */
template <class Rep, class Period = std::ratio<1>>
class duration;
```

`Rep` 是拿什么数来存（`int`、`long long`、`double`），
`Period` 是**一个计数代表多少秒**，写成一个编译期的分数
`std::ratio<分子, 分母>`。于是 `seconds` 就是 `duration<long long, ratio<1,1>>`，
`milliseconds` 是 `duration<long long, ratio<1,1000>>`。

`C++`

```cpp
/* duration_conv.cpp    编译：g++ -std=c++17 duration_conv.cpp -o duration_conv */
#include <chrono>
#include <cstdio>
#include <ratio>

using namespace std::chrono;
using namespace std::chrono_literals;          // 1s、100ms 这些字面量靠它

int main() {
    // ratio 是编译期的分数，duration 的单位就是它
    std::printf("milli   = %lld/%lld 秒\n", (long long)std::milli::num, (long long)std::milli::den);
    std::printf("kilo    = %lld/%lld\n", (long long)std::kilo::num, (long long)std::kilo::den);

    // 不同单位的 duration 相加，结果自动取能同时表示的细单位
    auto d1 = 1s + 100ms;
    std::printf("\n1s + 100ms 的单位是 1/%lld 秒，值 = %lld\n",
                (long long)decltype(d1)::period::den, (long long)d1.count());

    auto d2 = 1500ms;
    std::printf("\n1500ms 转成 seconds        ：%lld（截断）\n", (long long)duration_cast<seconds>(d2).count());
    std::printf("-1500ms 转成 seconds       ：%lld（朝零截断，不是向下取整）\n",
                (long long)duration_cast<seconds>(-d2).count());
    std::printf("1500ms 转成 duration<double>：%.3f 秒\n", duration_cast<duration<double>>(d2).count());
    std::printf("1500ms 直接构造 duration<double>：%.3f 秒（不丢小数）\n", duration<double>(d2).count());

    // 整型 duration 反复转换会丢精度
    auto x = 1234ms;
    auto back = duration_cast<milliseconds>(duration_cast<seconds>(x));
    std::printf("\n1234ms 先转秒再转回来 = %lld ms，丢了 %lld ms\n",
                (long long)back.count(), (long long)(x - back).count());

    // 超长单位与取模
    std::printf("\n1h + 30min = %lld 分钟\n", (long long)duration_cast<minutes>(1h + 30min).count());
    std::printf("1000000us  = %lld ms\n", (long long)duration_cast<milliseconds>(microseconds(1000000)).count());
    std::printf("2500ms 里有 %lld 个整秒，余 %lld ms\n",
                (long long)duration_cast<seconds>(2500ms).count(),
                (long long)(2500ms % 1s).count());
    std::printf("100ms < 1s ：%d\n", (int)(100ms < 1s));
    return 0;
}
```

`实测数据`
`Text`

```text
milli   = 1/1000 秒
kilo    = 1000/1

1s + 100ms 的单位是 1/1000 秒，值 = 1100

1500ms 转成 seconds        ：1（截断）
-1500ms 转成 seconds       ：-1（朝零截断，不是向下取整）
1500ms 转成 duration<double>：1.500 秒
1500ms 直接构造 duration<double>：1.500 秒（不丢小数）

1234ms 先转秒再转回来 = 1000 ms，丢了 234 ms

1h + 30min = 90 分钟
1000000us  = 1000 ms
2500ms 里有 2 个整秒，余 500 ms
100ms < 1s ：1
```

**`1s + 100ms` 的结果单位是毫秒，不是秒。** 两种单位相加时，
编译器取**能同时精确表示两者的最粗单位**（这里 `1/1000` 秒），
把两边都换算过去再相加。**这个换算在编译期完成，运行期没有额外开销。**

**同类型的 `duration` 之间可以直接比较、相加、相减**（`100ms < 1s` 成立），
**不同类型的必须显式转换**——这一条把「秒与毫秒混用」这类错误挡在了编译期。

## 1.2 `duration_cast` 的截断

`duration_cast` 做的事是**换单位**，它按整型除法截断：

| 输入 | 转换 | 结果 |
|---|---|---|
| `1500ms` | `duration_cast<seconds>` | `1`（丢掉 500 ms） |
| `-1500ms` | `duration_cast<seconds>` | `-1`（**朝零截断**，不是 `-2`） |
| `1234ms` | 先转 `seconds` 再转回 `milliseconds` | `1000ms`，**永久丢掉 234 ms** |

**第二条容易被忽略**：取整方向是「朝零」，因此负数与正数的行为对称，
**不会出现「负的不足一秒被算成 -1 秒」这种事**——但也不会向下取整。

**第三条是真正的陷阱**：转成粗单位之后，信息就没了。
**中间结果不要用粗单位存放**——要秒就到最后一步再转。

`C++`

```cpp
// （下面是节选）
auto t = 1234ms;
auto bad  = duration_cast<seconds>(t);              // 1 秒，剩下的 234 毫秒没了
auto good = duration<double>(t);                    // 1.234 秒，小数保留
```

**需要小数就用 `duration<double>`，而不是先转整型秒。**
`duration<double>` 的单位换算不做截断，因为它的 `Rep` 是浮点。

**`duration_cast` 不是唯一的取整方式。** 想要「向下取整」「向上取整」
「四舍五入」，C++17 给了 `floor`、`ceil`、`round` 三个函数，
**它们既作用于 `duration`，也作用于 `time_point`**：

`C++`

```cpp
/* cast_round.cpp    编译：g++ -std=c++17 cast_round.cpp -o cast_round */
#include <chrono>
#include <cstdio>

using namespace std::chrono;
using namespace std::chrono_literals;

// 同一个 duration，四种取整方式给出四个不同的答案
void row(const char *name, seconds plus, seconds minus) {
    std::printf("%-24s %6lld %8lld\n", name, (long long)plus.count(), (long long)minus.count());
}

int main() {
    auto a = 1234ms, b = -1234ms;                 // 都是「不足 1.5 秒」

    std::printf("%-24s %6s %8s\n", "写法", "1234ms", "-1234ms");
    row("duration_cast<seconds>", duration_cast<seconds>(a), duration_cast<seconds>(b));
    row("floor<seconds>", floor<seconds>(a), floor<seconds>(b));
    row("ceil<seconds>", ceil<seconds>(a), ceil<seconds>(b));
    row("round<seconds>", round<seconds>(a), round<seconds>(b));

    std::printf("\n1500ms 正好是一半：cast = %lld，round = %lld\n",
                (long long)duration_cast<seconds>(1500ms).count(),
                (long long)round<seconds>(1500ms).count());

    // 同样三个函数也能对齐时刻：这一个时刻距纪元 1790826189.750 秒
    system_clock::time_point tp = system_clock::time_point{} + 1790826189750ms;
    std::printf("\n同一个 time_point 对齐到整秒：\n");
    std::printf("  原始计数   = %lld\n", (long long)tp.time_since_epoch().count());
    std::printf("  floor      = %lld\n", (long long)floor<seconds>(tp).time_since_epoch().count());
    std::printf("  ceil       = %lld\n", (long long)ceil<seconds>(tp).time_since_epoch().count());
    std::printf("  round      = %lld\n", (long long)round<seconds>(tp).time_since_epoch().count());
    return 0;
}
```

`实测数据`
`Text`

```text
写法                   1234ms  -1234ms
duration_cast<seconds>        1       -1
floor<seconds>                1       -2
ceil<seconds>                 2       -1
round<seconds>                1       -1

1500ms 正好是一半：cast = 1，round = 2

同一个 time_point 对齐到整秒：
  原始计数   = 1790826189750000000
  floor      = 1790826189
  ceil       = 1790826190
  round      = 1790826190
```

**四种取整在正数上只差一个数，在负数上差得更多**：`-1234ms` 向下取整得到
`-2` 秒，向上取整得到 `-1` 秒，**而 `duration_cast` 与 `round` 都给出 `-1`**。
需要「至少等满」的语义时用 `ceil`，需要「不超过」时用 `floor`——
**写 `duration_cast` 时并没有选择，它只是朝零截断。**

`round` 在「正好一半」时向上取整：`1500ms` 得到 `2` 秒而不是 `1` 秒，
**这一点与 `std::round` 的「远离零」一致**。

## 1.3 时间字面量与它们的类型

`1s`、`100ms`、`1h` 这些字面量来自 `std::chrono_literals`
（在 `std::literals` 里也有一份），**必须 `using` 进来才能用**。

`C++`

```cpp
/* lit_err.cpp    编译：g++ -std=c++17 lit_err.cpp -o lit_err （失败） */
#include <chrono>
#include <cstdio>

int main() {
    auto d = 1s;                       // 没有 using namespace std::chrono_literals
    std::printf("%lld\n", (long long)d.count());
    return 0;
}
```

`实测数据`
`Text`

```text
lit_err.cpp:6:14: error: unable to find numeric literal operator 'operator""s'
    6 |     auto d = 1s;                       // 没有 using namespace std::chrono_literals
      |              ^~
lit_err.cpp:6:14: note: use '-fext-numeric-literals' to enable more built-in suffixes
```

**报错说的是「找不到字面量运算符」**：编译器把 `1s` 当成了一个用户自定义字面量，
而当前作用域里没有声明 `operator""s`。补上一句即可：

`C++`

```cpp
// （下面是节选）
using namespace std::chrono_literals;      // 一行，之后 1s / 100ms / 1h 都能写
```

**另一个容易忽略的地方是字面量的类型**：带小数点的秒字面量不是
`duration<double>`，而是 `duration<long double>`。

`C++`

```cpp
/* dur_lit_rep.cpp    编译：g++ -std=c++17 dur_lit_rep.cpp -o dur_lit_rep */
#include <chrono>
#include <cstdio>
#include <type_traits>

using namespace std::chrono;
using namespace std::chrono_literals;

int main() {
    auto a = 1.0s;          // 小数部分是 0
    auto b = 1.5s;          // 小数部分非 0
    auto c = 2s;            // 整数写法

    std::printf("1.0s 的 rep 是 double      ：%d\n", (int)std::is_same_v<decltype(a)::rep, double>);
    std::printf("1.0s 的 rep 大小           ：%d 字节\n", (int)sizeof(a.count()));
    std::printf("1.5s 的 rep 是 long double ：%d\n", (int)std::is_same_v<decltype(b)::rep, long double>);
    std::printf("1.5s 的 rep 大小           ：%d 字节\n", (int)sizeof(b.count()));
    std::printf("2s   的 rep 是 long long   ：%d\n", (int)std::is_same_v<decltype(c)::rep, long long>);
    std::printf("1.5s 的值（用 %%Lf 打印）    ：%Lf 秒\n", b.count());
    std::printf("1.5s 的值（强转 double）     ：%.3f 秒\n", (double)b.count());
    return 0;
}
```

`实测数据`
`Text`

```text
1.0s 的 rep 是 double      ：0
1.0s 的 rep 大小           ：16 字节
1.5s 的 rep 是 long double ：1
1.5s 的 rep 大小           ：16 字节
2s   的 rep 是 long long   ：1
1.5s 的值（用 %Lf 打印）    ：1.500000 秒
1.5s 的值（强转 double）     ：1.500 秒
```

**只要写法里带小数点，得到的就是 `duration<long double>`，哪怕小数部分是 0。**
这有一个直接后果：**用 `printf("%.1f", d.count())` 打印它会得到错误结果**，
因为 `long double` 与 `double` 是两个不同的类型，`%.1f` 按 `double` 取参数。
**要打印就用 `%Lf`，或者先 `static_cast<double>`。**

> [!NOTE]
> **第 1 节小结**：`duration` 把单位写进类型，**不同单位之间的转换必须显式**，
> 换算在编译期完成。`duration_cast` 按整型除法**朝零截断**，
> 中间结果用粗单位会永久丢精度。时间字面量要 `using namespace std::chrono_literals`，
> 而**带小数点的秒字面量是 `long double`，不是 `double`**。

---

# 第 2 节 `time_point` 与 `clock`

## 2.1 时刻是「相对某个起点的一个时长」

`time_point` 的定义同样简单：**一个时钟类型加一个 `duration`**。

`C++`

```cpp
/* （下面是节选）标准里的声明 */
template <class Clock, class Duration = typename Clock::duration>
class time_point;
```

**`time_point` 之间的减法得到 `duration`，`time_point` 加减 `duration` 得到新的
`time_point`**——这与指针和整数的关系是同一个结构（《04-语法/08-数组、指针与引用.md》第 1 节）。

`C++`

```cpp
/* point_math.cpp    编译：g++ -std=c++17 point_math.cpp -o point_math */
#include <chrono>
#include <cstdio>

using namespace std::chrono;
using namespace std::chrono_literals;

int main() {
    auto t0 = steady_clock::now();

    // 干点活，让时间真的过去
    volatile long long s = 0;
    for (int i = 0; i < 2000000; i++) s += i;

    auto t1 = steady_clock::now();

    // 三种表达耗时的方式
    auto d = t1 - t0;                                   // duration，单位跟着时钟走
    std::printf("原始计数   ：%lld，单位 1/%lld 秒\n",
                (long long)d.count(), (long long)decltype(d)::period::den);
    std::printf("转成毫秒   ：%lld ms\n", (long long)duration_cast<milliseconds>(d).count());
    std::printf("转成浮点秒 ：%.6f s\n", duration<double>(d).count());
    std::printf("转成整秒   ：%lld s（小于 1 秒就只剩 0）\n", (long long)duration_cast<seconds>(d).count());

    // time_point 的加减
    auto later = t0 + 500ms;
    std::printf("\nt0 + 500ms 比 t0 晚 %lld ms\n", (long long)duration_cast<milliseconds>(later - t0).count());
    std::printf("t1 是否晚于 t0：%d\n", (int)(t1 > t0));

    // 三次读数的时间戳原值
    auto t2 = steady_clock::now();
    std::printf("\n三次读数的时间戳原值：%lld / %lld / %lld\n",
                (long long)t0.time_since_epoch().count(),
                (long long)t1.time_since_epoch().count(),
                (long long)t2.time_since_epoch().count());
    return 0;
}
```

`实测数据`
`Text`

```text
原始计数   ：759000，单位 1/1000000000 秒
转成毫秒   ：0 ms
转成浮点秒 ：0.000759 s
转成整秒   ：0 s（小于 1 秒就只剩 0）

t0 + 500ms 比 t0 晚 500 ms
t1 是否晚于 t0：1

三次读数的时间戳原值：1790825216430339000 / 1790825216431098000 / 1790825216431125000
```

**这 200 万次加法只花了 759 微秒**，于是 `duration_cast<milliseconds>` 给出 `0`——
**「测出来是 0」不等于「没花时间」，只等于「单位选粗了」**。
`duration<double>` 那一行给出 `0.000759`，才是有意义的数字。

## 2.2 三种 clock 的区别

标准里预定义了三个时钟：

| 时钟 | 起点 | 会被调整吗 | 用途 |
|---|---|---|---|
| `system_clock` | 1970-01-01 00:00:00 UTC | **会**（对时、夏令时、手工改表） | 记「什么时候发生」，能转成 `time_t` |
| `steady_clock` | 不确定，通常是开机时刻 | **不会**，单调递增 | **测耗时** |
| `high_resolution_clock` | 由实现决定 | 由实现决定 | 需要最高分辨率时 |

**关键在 `is_steady` 这个成员**：它为 `true` 时，时钟保证**只向前走**，
后一次读数不会比前一次小。**测耗时只能用这种时钟**——
用 `system_clock` 测耗时时，如果系统在测量期间对了一次时，
时间差可能变成负数或者突然跳大。

`C++`

```cpp
/* clocks.cpp    编译：g++ -std=c++17 clocks.cpp -o clocks */
#include <chrono>
#include <cstdio>
#include <ctime>
#include <type_traits>

using namespace std::chrono;

template <class C>
void info(const char *name) {
    std::printf("%-24s is_steady=%d  period=%lld/%lld 秒  (每秒 %lld 个 tick)\n",
                name, (int)C::is_steady,
                (long long)C::period::num, (long long)C::period::den,
                (long long)(C::period::den / C::period::num));
}

int main() {
    info<system_clock>("system_clock");
    info<steady_clock>("steady_clock");
    info<high_resolution_clock>("high_resolution_clock");

    std::printf("\nhigh_resolution_clock 与 system_clock 是同一个类型：%d\n",
                (int)std::is_same_v<high_resolution_clock, system_clock>);
    std::printf("high_resolution_clock 与 steady_clock 是同一个类型：%d\n",
                (int)std::is_same_v<high_resolution_clock, steady_clock>);

    std::time_t t = system_clock::to_time_t(system_clock::now());
    std::printf("\nsystem_clock 现在距 1970-01-01 有 %lld 秒\n", (long long)t);
    std::printf("steady_clock 的原始计数 = %lld（起点不确定）\n",
                (long long)steady_clock::now().time_since_epoch().count());
    return 0;
}
```

`实测数据`
`Text`

```text
system_clock             is_steady=0  period=1/1000000000 秒  (每秒 1000000000 个 tick)
steady_clock             is_steady=1  period=1/1000000000 秒  (每秒 1000000000 个 tick)
high_resolution_clock    is_steady=0  period=1/1000000000 秒  (每秒 1000000000 个 tick)

high_resolution_clock 与 system_clock 是同一个类型：1
high_resolution_clock 与 steady_clock 是同一个类型：0

system_clock 现在距 1970-01-01 有 1790825215 秒
steady_clock 的原始计数 = 1790825215051517000（起点不确定）
```

**三条结论，其中第三条最出人意料。**

**第一，`period` 只是「类型的精度」，不是「真实分辨率」。**
三个时钟在本机都报告 `1/1000000000` 秒（纳秒），
但这不代表它们真能分辨到纳秒——下一小节实测。

**第二，`system_clock` 的 `is_steady` 是 0，`steady_clock` 是 1**，
符合预期，**测耗时用后者**。

**第三，本机的 `high_resolution_clock` 就是 `system_clock`**，
因此它 **`is_steady` 为 0**，**不能用来测耗时**。
这一点在不同实现上不一样：`<VS>` 的 STL 把 `high_resolution_clock`
定义成 `steady_clock` 的别名（`待确认`：未在本机验证）。
**因此这个类型的名字有误导性——它的名字说的是「分辨率高」，
没有承诺「单调」。要单调就写 `steady_clock`。**

## 2.3 真实分辨率

`period` 是类型层面的声明，**真实分辨率要靠读两次来测**：
连续读取 20 万次，统计有多少次读到的值与上一次完全相同。

`C++`

```cpp
/* clock_res.cpp    编译：g++ -std=c++17 clock_res.cpp -o clock_res */
#include <chrono>
#include <climits>
#include <cstdio>

using namespace std::chrono;

// 连着读 20 万次，看有多少次读到的值和上一次完全相同（说明分辨率不够）
template <class C>
void probe(const char *name) {
    const int N = 200000;
    auto prev = C::now();
    int same = 0;
    long long mn = LLONG_MAX, mx = 0;
    for (int i = 0; i < N; i++) {
        auto cur = C::now();
        auto d = duration_cast<nanoseconds>(cur - prev).count();
        if (d == 0) same++;
        else { if (d < mn) mn = d; if (d > mx) mx = d; }
        prev = cur;
    }
    std::printf("%-24s 连续读 %d 次：%d 次与上一次相同（%.1f%%），最小非零间隔 %lld ns，最大 %lld ns\n",
                name, N, same, 100.0 * same / N, mn, mx);
}

int main() {
    probe<system_clock>("system_clock");
    probe<steady_clock>("steady_clock");
    probe<high_resolution_clock>("high_resolution_clock");
    return 0;
}
```

`实测数据`
`Text`

```text
system_clock             连续读 200000 次：193961 次与上一次相同（97.0%），最小非零间隔 1000 ns，最大 12000 ns
steady_clock             连续读 200000 次：194014 次与上一次相同（97.0%），最小非零间隔 1000 ns，最大 6000 ns
high_resolution_clock    连续读 200000 次：193992 次与上一次相同（97.0%），最小非零间隔 1000 ns，最大 4000 ns
```

**97% 的连续读数完全相同，最小非零间隔是 1000 纳秒。**
两条信息合起来说明：**这三个时钟在本机的真实分辨率是 1 微秒**，
比它们声明的 1 纳秒粗三个数量级。**读到相同的值不是时钟坏了，
是两次读取之间的间隔小于一个计时周期。**

> [!IMPORTANT]
> **测量一段代码之前，先确认它比时钟分辨率长得多。**
> 本机分辨率 1 微秒，那么**耗时几十微秒级的代码测出来误差就在百分之几**；
> 更短的代码必须重复很多次再除以次数。
> 这个道理与《03-构建工具链/05-优化等级.md》第 3.1 小节「环境与探针」
> 讲的是同一件事：**先知道量具的刻度，再谈测出来的数。**

## 2.4 不同时钟的 `time_point` 不能比较

`time_point` 的第一个模板参数就是时钟类型，**比较运算符要求两边时钟相同**。

`C++`

```cpp
/* clock_cmp_err.cpp    编译：g++ -std=c++17 clock_cmp_err.cpp -o clock_cmp_err （失败） */
#include <chrono>
#include <cstdio>

int main() {
    auto a = std::chrono::system_clock::now();
    auto b = std::chrono::steady_clock::now();
    std::printf("%d\n", a < b);        // 两个不同时钟的 time_point 不能比较
    return 0;
}
```

`实测数据`
`Text`

```text
error: no match for 'operator<' (operand types are
  'std::chrono::time_point<std::chrono::_V2::system_clock, ...>' and
  'std::chrono::time_point<std::chrono::_V2::steady_clock, ...>')
note: candidate 2: ... operator<(const time_point<_Clock, _Dur1>&, const time_point<_Clock, _Dur2>&)
note:   deduced conflicting types for parameter '_Clock'
        ('std::chrono::_V2::system_clock' and 'std::chrono::_V2::steady_clock')
```

**这条错误是好事**：两个时钟的起点不同（一个从 1970 年起算，
一个是开机时刻），**直接比较本来就没有意义**。
编译器把这类比较挡在了编译期，而不是给出一个看似合理的数字。

**要跨时钟换算只能手工完成**，比如把 `system_clock` 的时刻与
`steady_clock` 的读数对应起来，做法是**同时读两个时钟、记下差值**：

`C++`

```cpp
// （下面是节选）
auto sys = std::chrono::system_clock::now();
auto st  = std::chrono::steady_clock::now();
auto offset = sys.time_since_epoch() - st.time_since_epoch();   // 需要显式换算，见下
```

**上面这几行只是一个示意**：两个时钟的 `duration` 类型相同才能直接相减，
类型不同时要用 `duration_cast` 先统一。**C++20 提供了 `clock_cast` 做这件事**，
C++17 里只能这样手工换算。

> [!NOTE]
> **第 2 节小结**：`time_point` 是「某个时钟上的一个时刻」，
> 两个时刻相减得到 `duration`。**测耗时只能用 `steady_clock`**；
> 本机的 `high_resolution_clock` 是 `system_clock` 的别名，**同样不单调**。
> 三个时钟声明的精度都是 1 纳秒，**真实分辨率是 1 微秒**。
> 不同时钟的时刻不能比较，这是编译期就挡住的错误。

---

# 第 3 节 测一段代码要多久

## 3.1 正确写法与多次测量

**单次测量没有意义**：一次读数受调度、缓存、频率调节的影响，
必须**重复多次再看分布**。下面这个程序把测量做完整：
固定轮数、每轮记一次、最后给出最小/中位/最大。

`C++`

```cpp
/* bench.cpp    编译：g++ -std=c++17 -O0 bench.cpp -o bench */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

// 被测工作：算一段级数和，结果要打印出来，免得被优化掉
static double work(int n) {
    double s = 0.0;
    for (int i = 1; i <= n; i++) s += 1.0 / (static_cast<double>(i) * i);
    return s;
}

int main(int argc, char **) {
    using namespace std::chrono;
    const int n = 2000000 + (argc - 1);        // 运行期才知道的值
    const int ROUNDS = 20;

    std::vector<double> ms;
    for (int r = 0; r < ROUNDS; r++) {
        auto t0 = steady_clock::now();
        double v = work(n);
        auto t1 = steady_clock::now();
        double e = duration<double, std::milli>(t1 - t0).count();
        ms.push_back(e);
        std::printf("第 %2d 次 %8.3f ms   结果 %.9f\n", r + 1, e, v);
    }

    std::sort(ms.begin(), ms.end());
    double sum = 0;
    for (double x : ms) sum += x;
    std::printf("\n最小 %.3f  中位 %.3f  最大 %.3f  平均 %.3f  最大/最小 = %.2f\n",
                ms.front(), ms[ms.size() / 2], ms.back(), sum / ms.size(), ms.back() / ms.front());
    return 0;
}
```

`实测数据`
`Text`

```text
g++ -std=c++17 -O0 bench.cpp -o bench      （20 次测量，摘录首尾）
第  1 次    3.042 ms   结果 1.644933567
第  2 次    3.050 ms   结果 1.644933567
...
第 19 次    3.028 ms   结果 1.644933567
第 20 次    3.027 ms   结果 1.644933567

最小 3.025  中位 3.029  最大 3.065  平均 3.036  最大/最小 = 1.01
```

`实测数据`
`Text`

```text
g++ -std=c++17 -O2 bench_o2.cpp -o bench_o2
最小 0.794  中位 0.794  最大 0.818  平均 0.796  最大/最小 = 1.03
```

**两组数字给出三件事。**

**第一，分布很窄。** `-O0` 那组的最大最小只差 1%，`-O2` 那组差 3%。
窄分布说明测量是可信的；**如果最大比最小大好几倍，
先去看是不是有别的进程在抢 CPU**，而不是急着下结论。

**第二，优化等级的差距是 3.8 倍**（3.029 ms 对 0.794 ms）。
这与《03-构建工具链/05-优化等级.md》第 3.3 小节的结论一致：
**同一份源码换个优化等级，速度可以差好几倍，因此基准数据必须标注优化等级**，
否则两组数字没有可比性。

**第三，被测函数的结果必须被使用。** 程序打印了 `1.644933567`——
如果这个值没有被打印、也没有被返回给外部，
`-O2` 下编译器有权把整个循环删掉（死代码消除，
《03-构建工具链/05-优化等级.md》第 5.2 小节），于是测出来是个假的 0。

## 3.2 五个常见的错误

| 错误 | 后果 |
|---|---|
| 用 `system_clock` 测耗时 | 测量期间系统对时就会得到负数或跳变 |
| 用 `duration_cast<milliseconds>` 记很短的耗时 | 小于 1 毫秒一律得到 0（第 2.1 小节的 759 微秒） |
| 把测量代码写进被测区间 | 计时的开销算进了被测量的代码里 |
| 只测一次 | 拿到的是「这一次」的数，不是这段代码的数 |
| 被测结果没人用 | `-O2` 下整段可能被删掉，测出假的 0 |

**第三条的量化**：本机一次 `steady_clock::now()` 的开销就在
第 2.3 小节那张表里——**最小非零间隔 1000 纳秒**，
也就是说**每次读时钟至少引入约 1 微秒的粒度**。
被测量的代码如果只有几微秒，测量误差就是同一量级。

**最后一条的后果可以完整测出来。** 同一个函数、同一份源码，
只改「结果有没有被使用」这一处，编译两遍对比：

`C++`

```cpp
/* dead_code.cpp    编译：g++ -std=c++17 -O2 dead_code.cpp -o dead_code */
#include <chrono>
#include <cstdio>

// 被测工作：算一段级数和。它没有副作用，因此结果没人要时可以被整段删掉
static double work(int n) {
    double s = 0.0;
    for (int i = 1; i <= n; i++) s += 1.0 / (static_cast<double>(i) * i);
    return s;
}

int main(int argc, char **) {
    using namespace std::chrono;
    int n = 2000000 + (argc - 1);          // 运行期才知道的值
    const int ROUNDS = 5;

    // 错的：返回值没人用，-O2 下编译器有权把整段循环删掉
    {
        auto t0 = steady_clock::now();
        for (int r = 0; r < ROUNDS; r++) work(n);
        auto t1 = steady_clock::now();
        std::printf("结果没人用：%.3f ms/次\n", duration<double, std::milli>(t1 - t0).count() / ROUNDS);
    }
    // 对的：把结果累加起来打印，编译器必须真的算
    {
        double sink = 0.0;
        auto t0 = steady_clock::now();
        for (int r = 0; r < ROUNDS; r++) sink += work(n);
        auto t1 = steady_clock::now();
        std::printf("结果被用  ：%.3f ms/次（累加值 %.9f）\n",
                    duration<double, std::milli>(t1 - t0).count() / ROUNDS, sink);
    }
    return 0;
}
```

`实测数据`
`Text`

```text
g++ -std=c++17 -O2 dead_code.cpp -o dead_code
结果没人用：0.000 ms/次
结果被用  ：0.798 ms/次（累加值 8.224667834）

g++ -std=c++17 -O0 dead_code.cpp -o dead_code_o0
结果没人用：3.146 ms/次
结果被用  ：3.358 ms/次（累加值 8.224667834）
```

**`-O2` 那一组的「0.000 ms」是被删掉了，不是「很快」。**
同一份代码在 `-O0` 下老老实实算了 3.146 毫秒，**换个优化等级就变成 0**；
**而 `-O0` 那两行只差 0.2 毫秒**，说明这段代码本身的开销就在 3 毫秒上下。
要证明一个基准没有落入这个陷阱，办法只有一个：**确认被测结果真的被打印或返回出去了。**

**正确做法是「重复 N 次再平均」**：

`C++`

```cpp
// （下面是节选）
const int N = 1000;
auto t0 = steady_clock::now();
for (int i = 0; i < N; i++) {
    volatile long long dummy = work(100);   // 被测的工作
    (void)dummy;
}
auto t1 = steady_clock::now();
double per = duration<double, std::micro>(t1 - t0).count() / N;   // 每次多少微秒
```

**这里有一个新的陷阱**：循环本身的开销也算进了 `per` 里。
**减去它的办法是再写一个空循环测一次，把那个数扣掉**。

---

# 第 4 节 等待与定时

## 4.1 `sleep_for` 实际睡多久

`sleep_for` 接受一个 `duration`，`sleep_until` 接受一个 `time_point`。
**两者都不保证精确**，原因不在标准库，而在操作系统：
**Windows 的默认定时器精度是 15.6 毫秒**，
比它短的等待会被拉长到下一个计时周期。

`C++`

```cpp
/* sleep_meas.cpp    编译：g++ -std=c++17 sleep_meas.cpp -o sleep_meas */
#include <chrono>
#include <cstdio>
#include <thread>

using namespace std::chrono;
using namespace std::chrono_literals;

void trial(const char *name, milliseconds want) {
    auto t0 = steady_clock::now();
    std::this_thread::sleep_for(want);
    auto t1 = steady_clock::now();
    double got = duration<double, std::milli>(t1 - t0).count();
    std::printf("%-16s 要求 %6lld ms，实际 %8.3f ms，多出 %7.3f ms\n",
                name, (long long)want.count(), got, got - want.count());
}

int main() {
    std::printf("sleep_for 的实际时长：\n");
    for (int i = 1; i <= 3; i++) trial("sleep_for(100ms)", 100ms);
    for (int i = 1; i <= 3; i++) trial("sleep_for(1ms)", 1ms);
    for (int i = 1; i <= 2; i++) trial("sleep_for(0ms)", 0ms);

    // sleep_until 用绝对时刻，适合「每隔一段时间做一次」的循环
    auto start = steady_clock::now();
    auto next = start;
    std::printf("\nsleep_until 连续 3 次、每次间隔 20 ms：\n");
    for (int i = 0; i < 3; i++) {
        next += 20ms;
        std::this_thread::sleep_until(next);
        std::printf("  第 %d 次醒来，距开始 %8.3f ms（应该接近 %d ms）\n", i + 1,
                    duration<double, std::milli>(steady_clock::now() - start).count(), (i + 1) * 20);
    }

    // 已经过期的时刻：立即返回
    auto t0 = steady_clock::now();
    std::this_thread::sleep_until(t0 - 1s);
    std::printf("\nsleep_until 传入过去的时刻，耗时 %8.3f ms（立即返回）\n",
                duration<double, std::milli>(steady_clock::now() - t0).count());
    return 0;
}
```

`实测数据`
`Text`

```text
sleep_for 的实际时长：
sleep_for(100ms) 要求    100 ms，实际  104.572 ms，多出   4.572 ms
sleep_for(100ms) 要求    100 ms，实际  108.425 ms，多出   8.425 ms
sleep_for(100ms) 要求    100 ms，实际  107.580 ms，多出   7.580 ms
sleep_for(1ms)   要求      1 ms，实际   14.993 ms，多出  13.993 ms
sleep_for(1ms)   要求      1 ms，实际   15.133 ms，多出  14.133 ms
sleep_for(1ms)   要求      1 ms，实际   15.197 ms，多出  14.197 ms
sleep_for(0ms)   要求      0 ms，实际    0.001 ms，多出   0.001 ms
sleep_for(0ms)   要求      0 ms，实际    0.000 ms，多出   0.000 ms

sleep_until 连续 3 次、每次间隔 20 ms：
  第 1 次醒来，距开始   32.381 ms（应该接近 20 ms）
  第 2 次醒来，距开始   48.120 ms（应该接近 40 ms）
  第 3 次醒来，距开始   63.531 ms（应该接近 60 ms）

sleep_until 传入过去的时刻，耗时    0.000 ms（立即返回）
```

**三组数字，三种现象。**

**第一，短睡眠会被拉长到计时周期。** `sleep_for(1ms)` 实际睡了约 15 毫秒，
**是要求的 15 倍**；`sleep_for(100ms)` 多出 4 到 8 毫秒，
相对误差小得多。**结论是：毫秒级以下不要指望 `sleep_for`。**

**第二，`sleep_for(0ms)` 几乎立即返回**（0.001 毫秒），
它相当于让出一次 CPU，不进入等待。

**第三，`sleep_until` 的偏差不会累积。** 三次醒来分别在 32.4、48.1、63.5 毫秒，
目标是 20、40、60——**第一次超了 12 毫秒，但后面两次只落后 8 毫秒与 3.5 毫秒**。
原因是 `sleep_until` 每次都传入**绝对时刻**（`next += 20ms`），
上一次睡过头不会推到下一次。

> [!IMPORTANT]
> **要周期性执行就用 `sleep_until` 加绝对时刻，不要用 `sleep_for` 循环。**
> `sleep_for(20ms)` 写十次循环，每次多出十几毫秒，误差会一路累积；
> 而 `next += 20ms` 这种写法把目标时刻写死，**睡过头的部分会自动被下一次扣回来**。

## 4.2 需要更精确的等待时

Windows 上把定时器精度调到 1 毫秒需要调用平台 API
（`timeBeginPeriod` / `timeEndPeriod`，属于 `07-更底层` 板块的内容），
而且它会影响整个系统的功耗表现。**这里不展开，只说明一件事**：
标准库的 `sleep_for` 没有「提高精度」的开关，
**它的精度就是操作系统的定时器精度**。

**这个粒度可以直接测出来**：把要求的时间从 1 毫秒依次加到 34 毫秒，
每种要求测三次取最小值（减少被别的进程抢占的干扰），
**测出实际睡醒的时刻落在哪些档位上**：

`C++`

```cpp
/* timer_quantum.cpp    编译：g++ -std=c++17 timer_quantum.cpp -o timer_quantum */
#include <chrono>
#include <cstdio>
#include <thread>

using namespace std::chrono;

int main() {
    std::printf("%8s %12s %12s\n", "要求(ms)", "实际(ms)", "多出(ms)");
    // 每种要求测 3 次取最小值：最小值最接近「没有被抢占」的那一次
    for (int want = 1; want <= 34; want += 3) {
        double best = 1e9;
        for (int k = 0; k < 3; k++) {
            auto t0 = steady_clock::now();
            std::this_thread::sleep_for(milliseconds(want));
            auto t1 = steady_clock::now();
            double got = duration<double, std::milli>(t1 - t0).count();
            if (got < best) best = got;
        }
        std::printf("%8d %12.3f %12.3f\n", want, best, best - want);
    }
    return 0;
}
```

`实测数据`
`Text`

```text
要求(ms)   实际(ms)   多出(ms)
       1        6.794        5.794
       4       15.220       11.220
       7       15.495        8.495
      10       15.094        5.094
      13       15.538        2.538
      16       31.100       15.100
      19       30.137       11.137
      22       30.268        8.268
      25       29.967        4.967
      28       30.604        2.604
      31       31.971        0.971
      34       46.319       12.319
```

**实际睡醒的时刻落在 15.6 毫秒的整数倍上**：要求 4 到 13 毫秒的都在
15 毫秒上下醒来，要求 16 到 31 毫秒的都在 30 毫秒上下，要求 34 毫秒的跳到 46 毫秒
（`15.6 × 3 = 46.8`）。**「多出多少」因此不是固定值，而是「距下一个档位还差多少」**——
要求 31 毫秒时只多出 0.971 毫秒，要求 16 毫秒时却多出 15.1 毫秒。
**这也解释了 `sleep_for` 的一个反直觉之处：要求得越接近档位，误差越小。**

`待确认`：本机未验证 `timeBeginPeriod(1)` 之后 `sleep_for(1ms)` 的实际改善幅度。

> [!NOTE]
> **第 4 节小结**：`sleep_for` 的实际时长**只会比要求的长**，
> 在 Windows 上短于 15.6 毫秒的等待会被拉长到约 15 毫秒。
> **`sleep_until` 配绝对时刻不会累积误差**，是周期性任务该用的写法；
> 传入已经过去的时刻会立即返回。

---

# 第 5 节 日历与时区

## 5.1 C++17 的过渡写法

**C++17 的 `<chrono>` 里没有「年月日」这个类型。**
`system_clock` 只能给出一个距 1970-01-01 的时长，
要变成「2026 年 10 月 1 日 11 时 43 分」必须借道 `<ctime>`：
先转 `time_t`，再交给 `localtime` / `gmtime`，最后用 `strftime` 排版。
这一整套东西 C 那边已经讲过了（《06-标准库/A-04-时间与日期：time.h.md》第 2 节与第 4 节）。

`C++`

```cpp
/* to_calendar.cpp    编译：g++ -std=c++17 to_calendar.cpp -o to_calendar */
#include <chrono>
#include <cstdio>
#include <ctime>

int main() {
    using namespace std::chrono;

    // C++17 里把 system_clock 的时刻变成「年月日时分秒」只有这一条路：
    // 先转 time_t，再交给 <ctime> 的 localtime / gmtime
    std::time_t t = system_clock::to_time_t(system_clock::now());

    // 关键一步：两个函数可能返回同一个静态缓冲区，必须各自拷一份出来
    std::tm local_buf = *std::localtime(&t);
    std::tm utc_buf   = *std::gmtime(&t);
    std::tm *local = &local_buf;
    std::tm *utc = &utc_buf;

    char buf[64];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", local);
    std::printf("本地时间：%s\n", buf);
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", utc);
    std::printf("UTC 时间：%s\n", buf);

    // 时区偏移只能自己算：把两边的 tm 各自转回 time_t 再相减
    std::tm l2 = local_buf, u2 = utc_buf;
    std::time_t tl = std::mktime(&l2);
    std::time_t tu = std::mktime(&u2);
    std::printf("本地时间比 UTC 早 %lld 小时\n", (long long)((tl - tu) / 3600));

    // 反过来：把年月日时分秒变回 time_point
    std::tm mine{};
    mine.tm_year = 2026 - 1900;
    mine.tm_mon  = 10 - 1;
    mine.tm_mday = 1;
    mine.tm_hour = 12;
    std::time_t t2 = std::mktime(&mine);
    std::printf("2026-10-01 12:00（按本地时区解释）= %lld 秒\n", (long long)t2);
    std::printf("对应的 system_clock 时刻距纪元 %lld 秒\n",
                (long long)duration_cast<seconds>(system_clock::from_time_t(t2).time_since_epoch()).count());
    return 0;
}
```

`实测数据`
`Text`

```text
本地时间：2026-10-01 11:43:09
UTC 时间：2026-10-01 03:43:09
本地时间比 UTC 早 8 小时
2026-10-01 12:00（按本地时区解释）= 1790827200 秒
对应的 system_clock 时刻距纪元 1790827200 秒
```

**本机的系统时区是 UTC+8，因此本地时间比 UTC 早 8 小时。**
这个数字会影响本章节所有涉及 `localtime` 的输出，因此要写清楚。

**代码里那两行「拷一份」值得注意**——少了它们，这个程序会打印出错误的答案。
下一小节说明原因。

## 5.2 `localtime` 与 `gmtime` 共用一块缓冲区

C 标准规定 `localtime` 与 `gmtime` 返回的指针**可能指向同一个静态对象**，
因此**后一次调用会覆盖前一次的结果**。本机的实现正是如此。

`C++`

```cpp
/* tm_share.cpp    编译：g++ -std=c++17 tm_share.cpp -o tm_share */
#include <cstdio>
#include <ctime>

int main() {
    // 固定时刻：2026-10-01 03:33:41 UTC（本机时区 UTC+8，本地时间是 11:33:41）
    std::time_t t = 1790825621;

    std::tm *local = std::localtime(&t);
    std::printf("localtime 之后：tm_hour = %2d，结构体地址 %p\n", local->tm_hour, (void *)local);

    std::tm kept = *local;                      // 先拷一份下来

    std::tm *utc = std::gmtime(&t);
    std::printf("gmtime   之后：tm_hour = %2d，结构体地址 %p\n", utc->tm_hour, (void *)utc);
    std::printf("两个指针相等吗：%d\n", (int)(local == utc));
    std::printf("回头看 local 指向的内容：tm_hour = %2d\n", local->tm_hour);
    std::printf("事先拷下来的那一份    ：tm_hour = %2d\n", kept.tm_hour);

    char b1[64], b2[64];
    std::strftime(b1, sizeof b1, "%Y-%m-%d %H:%M:%S", local);
    std::strftime(b2, sizeof b2, "%Y-%m-%d %H:%M:%S", &kept);
    std::printf("local 现在格式化出来  ：%s\n", b1);
    std::printf("拷贝那一份格式化出来  ：%s\n", b2);
    return 0;
}
```

`实测数据`
`Text`

```text
localtime 之后：tm_hour = 11，结构体地址 000001a8d2912420
gmtime   之后：tm_hour =  3，结构体地址 000001a8d2912420
两个指针相等吗：1
回头看 local 指向的内容：tm_hour =  3
事先拷下来的那一份    ：tm_hour = 11
local 现在格式化出来  ：2026-10-01 03:33:41
拷贝那一份格式化出来  ：2026-10-01 11:33:41
```

**两个指针完全相等，都是 `000001a8d2912420`。**
`gmtime` 把 UTC 时间写进了同一块内存，于是 `local` 指向的内容
从 `11` 变成了 `3`——**变量名叫 `local`，读出来却是 UTC**。
**这就是第 5.1 小节那份代码必须「先拷一份」的原因。**

> [!CAUTION]
> **`localtime` 与 `gmtime` 返回的是静态缓冲区，不能长期持有。**
> 典型症状是「两个 `tm` 结构体打印出来一模一样」，
> 或者「调了一次别的函数之后，之前那个时间变了」。
> 稳妥做法是**立刻拷进自己的 `std::tm` 对象**，
> 或者用平台提供的可重入版本（POSIX 的 `localtime_r`、MSVC 的 `localtime_s`）。

## 5.3 C++20 的日历与时区

**C++20 给 `<chrono>` 补上了日历类型**：`year`、`month`、`day`、
`year_month_day`、`weekday`，以及时区支持 `locate_zone`、`zoned_time`。
**这些在 `-std=c++17` 下不存在**，编译器会说找不到名字。

`C++`

```cpp
/* cal17_err.cpp    编译：g++ -std=c++17 cal17_err.cpp -o cal17_err （失败） */
#include <chrono>
#include <cstdio>

int main() {
    std::chrono::year_month_day d = std::chrono::year(2026) / 10 / 1;
    std::printf("%d\n", (int)d.year());
    return 0;
}
```

`实测数据`
`Text`

```text
error: 'year_month_day' is not a member of 'std::chrono'
    6 |     std::chrono::year_month_day d = std::chrono::year(2026) / 10 / 1;
      |                  ^~~~~~~~~~~~~~
error: 'd' was not declared in this scope
```

**换成 `-std=c++20` 之后一切正常**：

`C++`

```cpp
/* cal20.cpp    编译：g++ -std=c++20 cal20.cpp -o cal20 */
#include <chrono>
#include <cstdio>

using namespace std::chrono;

int main() {
    // C++20 的日历类型：年、月、日各有自己的类型
    year_month_day d = 2026y / October / 1d;
    std::printf("year_month_day      ：%04d-%02u-%02u\n",
                (int)d.year(), (unsigned)d.month(), (unsigned)d.day());
    std::printf("是否合法日期        ：%d\n", (int)d.ok());
    std::printf("这一天的星期        ：%u（0 = 星期日）\n", (unsigned)weekday(sys_days(d)).c_encoding());

    year_month_day bad = 2026y / February / 30d;
    std::printf("\n2026-02-30 是否合法 ：%d\n", (int)bad.ok());
    std::printf("把它当 sys_days 用  ：%s\n", sys_days(bad).time_since_epoch().count() ? "有值" : "无值");

    // 月末、闰年判断
    std::printf("\n2024 年是闰年       ：%d\n", (int)year(2024).is_leap());
    std::printf("2026 年是闰年       ：%d\n", (int)year(2026).is_leap());
    std::printf("2026 年 2 月的最后一天：%u\n", (unsigned)year_month_day_last(2026y / February / last).day());

    // 时区：需要系统里有 tzdata
    std::printf("\n时区数据库：\n");
    try {
        const time_zone *tz = locate_zone("Asia/Shanghai");
        std::printf("  Asia/Shanghai 的当前偏移：%lld 秒\n",
                    (long long)tz->get_info(system_clock::now()).offset.count());
    } catch (const std::exception &e) {
        std::printf("  locate_zone 失败：%s\n", e.what());
    }
    return 0;
}
```

`实测数据`
`Text`

```text
year_month_day      ：2026-10-01
是否合法日期        ：1
这一天的星期        ：4（0 = 星期日）

2026-02-30 是否合法 ：0
把它当 sys_days 用  ：有值

2024 年是闰年       ：1
2026 年是闰年       ：0
2026 年 2 月的最后一天：28

时区数据库：
  Asia/Shanghai 的当前偏移：28800 秒
```

**四点值得注意。**

**第一，`ok()` 是「这个日期合法吗」的开关。**
`2026-02-30` 的 `ok()` 是 0，**但它照样能转成 `sys_days` 并给出一个值**
（相当于 3 月 2 日）。**因此合法性要自己用 `ok()` 检查，编译器不会拦。**

**第二，`/` 在这里是重载出来的日期分隔符**，不是除法
（《05-类与面向对象/09-运算符重载.md》第 1 节）。
`2026y / October / 1d` 里的 `y` 与 `d` 也是字面量，来自 `std::chrono_literals`。

**第三，闰年与月末有现成的类型**：`year::is_leap()`、
`year_month_day_last`，不必自己写判断。

**第四，本机的时区数据库可用**：`locate_zone("Asia/Shanghai")`
给出了 28800 秒（8 小时）的偏移，与第 5.1 小节算出来的 8 小时一致。
**时区数据来自操作系统，不是标准库自带的**，因此换一台机器要看它有没有 tzdata。

> [!TIP]
> **项目还在用 C++17 时，把日期计算交给 `<ctime>`，把时间间隔交给 `<chrono>`。**
> 上面那套日历类型要等到能整体升到 C++20 再用；
> **能否使用，编译一次 `-std=c++20` 即可判断**，不必查文档。

> [!NOTE]
> **第 5 节小结**：C++17 里「时刻变日期」只能走 `to_time_t` 加 `<ctime>`，
> **而 `localtime` 与 `gmtime` 共用静态缓冲区，必须立刻拷贝**。
> C++20 补上了日历类型与时区库，`ok()` 是日期合法性的开关，
> 时区数据来自操作系统的 tzdata。

---

# 第 6 节 速查表

| 常用件 | 一句话用途 | 典型坑 |
|---|---|---|
| `duration<Rep, Period>` | 带单位的时长 | 单位是类型的一部分，不同类型不能直接赋值 |
| `duration_cast<To>(d)` | 换单位 | **朝零截断**；中间结果用粗单位会永久丢精度 |
| `duration<double>(d)` | 保留小数的换算 | 需要小数时用它，不要先转整型秒 |
| `1s` / `100ms` / `1h` | 时间字面量 | 要 `using namespace std::chrono_literals` |
| `1.5s` 的类型 | `duration<long double>` | 带小数点就是 `long double`，用 `%.1f` 打印会出错 |
| `time_point<Clock, Dur>` | 某个时钟上的时刻 | 不同时钟的 `time_point` 不能比较 |
| `steady_clock` | **测耗时只用它** | 别的时钟可能被系统调整 |
| `system_clock` | 要转 `time_t`、要日历时间时用 | `is_steady` 为假，不能用来计时 |
| `high_resolution_clock` | 名字有误导性 | 本机它就是 `system_clock`，**不单调** |
| `clock::period` | 类型的精度 | 只是声明，真实分辨率要实测（本机 1 微秒） |
| `clock::is_steady` | 是否单调 | 测耗时的第一道检查 |
| `now()` | 读当前时刻 | 一次调用的开销约 1 微秒（本机） |
| `this_thread::sleep_for` | 等一段时间 | 实际只会更长；1 ms 会睡成约 15 ms |
| `this_thread::sleep_until` | 等到某个时刻 | **周期性任务用它，不会累积误差** |
| `system_clock::to_time_t` | 转成 `time_t` | 精度掉到秒 |
| `localtime` / `gmtime` | 转成 `struct tm` | **共用静态缓冲区，必须立刻拷贝** |
| `mktime` | `struct tm` 转 `time_t` | 按本地时区解释，且会改写传入的结构体 |
| `year_month_day` 等 | C++20 的日历类型 | **C++17 下不存在**；`ok()` 要自己查 |
| `locate_zone` / `tzdb` | C++20 的时区 | 依赖系统的 tzdata |

**配套示例见 [`B-examples/06-standard-library/06-cpp-chrono-benchmark/`](../B-examples/06-standard-library/06-cpp-chrono-benchmark/)，配套练习见 [`C-templates/06-standard-library/06-cpp-chrono-benchmark/`](../C-templates/06-standard-library/06-cpp-chrono-benchmark/)。**
示例把本章节的测量方法做成一个小基准工具：多轮测量、给出分位数、
对比 `-O0` 与 `-O2`，并对 `sleep_for` 的实际时长做一次校准。

---

# 附录 A 复现本章节实测

## A.1 环境

`实测数据`

| 项 | 值 |
|---|---|
| 编译器 | g++ 15.2.0（MinGW-w64，x86_64-win32-seh） |
| 标准 | `-std=c++17`（第 5.3 小节另用 `-std=c++20`） |
| 系统 | Windows 11 build 22631 |
| 系统时区 | UTC+8（影响所有 `localtime` 的输出） |
| 对照环境 | g++ 13.3.0（Ubuntu 24.04，WSL） |

## A.2 各程序的编译与运行

`Bash`

```bash
g++ -std=c++17 <文件名>.cpp -o <可执行名> && ./<可执行名>
```

**几处例外**：

| 程序 | 特殊之处 |
|---|---|
| `bench.cpp` | 编两遍：`-O0` 与 `-O2`，比较两次分布 |
| `lit_err.cpp`、`clock_cmp_err.cpp`、`cal17_err.cpp` | 期望编译失败，报错原文见正文 |
| `cal20.cpp` | 用 `-std=c++20` |
| `sleep_meas.cpp` | 要跑一秒左右，属正常 |
| `to_calendar.cpp` | 输出与系统时区相关 |

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《06-标准库/A-04-时间与日期：time.h.md》 | **对照**：C 侧的 `time`、`clock`、`localtime`、`strftime` |
| 《06-标准库/B-00-导读：C++ 标准库与 C 的关系.md》 | **前置**：`<cxxx>` 与 `<xxx.h>` 的区别 |
| 《05-类与面向对象/09-运算符重载.md》第 1 节 | **前置**：`duration` 的 `+`、`<` 与日期的 `/` 都是重载 |
| 《05-类与面向对象/11-模板.md》第 4 节 | **前置**：类模板与静态成员——`duration` 的单位换算靠它们 |
| 《04-语法/08-数组、指针与引用.md》第 1 节 | 相关：时刻与时长、指针与整数是同一个结构 |
| 《03-构建工具链/05-优化等级.md》第 3.3 小节 | **相关**：性能实测的做法与这里一致 |
| 《03-构建工具链/05-优化等级.md》第 5.2 小节 | 相关：死代码消除会让「没人用的结果」被删掉 |
| 《06-标准库/B-10-内存与并发的基础设施.md》 | **后续**：并发里的等待与超时同样用 `chrono` |
| 【待补：07-更底层/】 | 后续：`timeBeginPeriod` 这类平台 API |
