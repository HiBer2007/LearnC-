# 示例 `07-standard-library/04-cpp-smart-pointers` · 智能指针与回调注册表（纯命令行）

一个围绕「谁拥有这个对象」展开的命令行程序。它用 `std::unique_ptr` 做资源工厂：
按种类造出派生对象，以基类指针交出所有权；用 `std::shared_ptr` 演示共享所有权与 `use_count()`
的涨落；用 `std::weak_ptr` 观察对象、并在父子互指的场景里断开循环引用；
最后用 `std::function` 搭一个回调注册表，支持按名字注册、注销、触发与查询。

三条对照是本项目的主线。第一条是所有权：工厂返回 `unique_ptr` 而不是裸指针，
类型上就写明「这份资源归调用者」，自测用存活对象数证明对象确实被释放了。
第二条是循环引用：父子各持对方的 `shared_ptr` 时，离开作用域后对象数不为 0；
把其中一条改成 `weak_ptr`，对象数立刻归零。
第三条是回调：注册、触发、注销各一步，注销之后再触发不计数。

```text
04-cpp-smart-pointers/
  include/resource_registry.hpp   接口：Resource 与派生类、Node、CallbackRegistry、AuditLog
  src/resource_registry.cpp       实现：工厂、共享、断环、回调，含项目输出与 18 项自测
  src/main_cli.cpp                命令行版：读参数、打印、按控制台代码页转换
  CMakeLists.txt                  目标：core（静态库）与 app_cli
  CMakePresets.json               mingw-gdb 与 msvc 两套预设
  .vscode/                        两个调试配置与四个构建任务
```

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《07-标准库/B-03-智能指针的用法.md》第 1.1 小节 | 创建、判空、取用、转移 | `make_resource` 的返回值与 `std::move` 转移（自测第 6 项） |
| 《07-标准库/B-03-智能指针的用法.md》第 1.3 小节 | 工厂函数：把所有权给出去 | `make_resource`、`make_default_batch` 与报告的第一个小节 |
| 《07-标准库/B-03-智能指针的用法.md》第 2.2 小节 | `use_count` 的增减 | 报告的「共享所有权」一段与自测第 7、8 项 |
| 《07-标准库/B-03-智能指针的用法.md》第 3.1 小节 | 不释放的环 | 父子互指的强引用版本，自测第 13 项 |
| 《07-标准库/B-03-智能指针的用法.md》第 3.2 小节 | `weak_ptr` 的 `lock` 与 `expired` | 观察一段与自测第 11、12、14、15 项 |
| 《07-标准库/B-03-智能指针的用法.md》第 4 节 | 怎么选 | 工厂用 `unique_ptr`、共享才用 `shared_ptr`、观察用 `weak_ptr` |
| 《07-标准库/B-04-可调用物的包装.md》第 1 节 | `std::function`：把可调用物装进一个类型 | `CallbackRegistry::Callback` 与三个回调的注册 |
| 《07-标准库/B-04-可调用物的包装.md》第 1.2 小节 | 空状态与 `bad_function_call` | `add` 里先判回调是否为空，空回调直接拒绝 |
| 《07-标准库/B-04-可调用物的包装.md》第 2.2 小节 | 绑定成员函数 | `audit` 回调用 `std::bind` 绑到 `AuditLog::record` |

## 这个项目要解决什么问题

裸指针看不出所有权：`Resource *make()` 这样的接口，调用者要翻文档才知道该不该 `delete`，
中途抛异常时更容易漏掉释放。项目把工厂的返回类型改成 `std::unique_ptr<Resource>`，
所有权写在类型里，`make_resource("model", ...)` 认不出种类时返回空指针，
调用者必须判空——这是「可能失败」在类型上的表达。

共享所有权要付代价，代价是可见的：`use_count()` 把「现在有几份在指着它」直接摆出来。
项目让计数从 1 涨到 3、再降回 1，最后一份释放时对象才析构，
用存活对象数证明析构确实发生了。循环引用是这条路的边界情况：
两个对象互相持有强引用时，计数永远回不到 0，两个对象一起泄漏。
项目用同一个 `Node` 类给出两种连法，一条成环、一条用 `weak_ptr` 断环，
两条路径都打印存活节点数，差别一目了然。

回调注册表解决的是「运行期才知道要调用谁」：把 lambda、成员函数绑定、
普通函数统一装进 `std::function<std::string(const std::string &)>`，
再按名字存进表里。注册与注销是一对，注销之后按名字触发要返回「找不到」，
而不是调用一个已经不存在的对象。

## 做完能掌握什么

- 会用 `make_unique` 造派生对象，并以基类指针交出所有权
- 会判断一个接口该返回 `unique_ptr`、`shared_ptr` 还是 `weak_ptr`
- 会用对象计数给自己写的资源类做体检：释放没释放，看数字
- 会认出循环引用，并用 `weak_ptr` 断开它，同时说明为什么另一端必须留强引用
- 会把可调用物统一装进 `std::function`，并用 `std::bind` 绑成员函数
- 会把「界面」与「逻辑」分开：`registry` 命名空间不认识窗口，也不认识 `std::cout`

## 文件

```text
include/resource_registry.hpp   接口，210 行：Resource、Texture、AudioClip、Node、
                                CallbackRegistry、AuditLog、CheckResult
src/resource_registry.cpp       实现，556 行（其中非空非注释 454 行），含项目输出与 18 项自测
src/main_cli.cpp                命令行版，119 行：解析参数、打印、UTF-8 到控制台代码页的转换
CMakeLists.txt                  目标 core（静态库）与 app_cli；字符集选项全板块统一
CMakePresets.json               mingw-gdb（Ninja + g++）与 msvc（Visual Studio 17 2022）两套预设
.vscode/launch.json             两个调试配置：GDB · 命令行版、MSVC · 命令行版
.vscode/tasks.json              四个构建任务与两个组合任务
```

`core` 是纯逻辑的静态库，删掉 `src/main_cli.cpp` 它照样能编译、能自测通过。

## 阶段推进

| 阶段 | 目标 | 做什么 | 怎么验收 |
|---|---|---|---|
| **阶段 1** | 跑通命令行版 | 配置、编译，运行 `build\mingw\bin\app_cli.exe` | 看到四个小节的项目输出与「18 项中 18 项通过，全部通过」 |
| **阶段 2** | 看懂计数 | 在 `build_report` 的共享所有权一段加一行 `use_count()`，重新编译 | 新打印的数字与「拷贝出第 N 份」对得上 |
| **阶段 3** | 自己加一种资源 | 加 `class Model : public Resource`，在 `make_resource` 里认 `"model"`，并补一条自测 | 新自测项通过；报告里 `make_resource("model")` 不再是空指针 |
| **阶段 4** | 加一个回调 | 往注册表里加一个捕获局部变量的 lambda，触发两次后注销 | 注销后 `has()` 为假，再触发返回「找不到这个名字」 |

阶段 3 是重点：新增派生类之后，工厂、`vector<unique_ptr<Resource>>` 与多态调用三处都要跟着对，
自测第 1 至 5 项会立刻告诉你有没有漏。

## 构建与运行

`PowerShell`

```powershell
# 在 07-standard-library/04-cpp-smart-pointers 目录下：配置 + 编译
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 运行：演示 + 自测
build\mingw\bin\app_cli.exe

# 只跑自测
build\mingw\bin\app_cli.exe --selftest
```

不想用预设时，等价的手写命令是：

`PowerShell`

```powershell
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build/mingw
```

用 VS Code 打开本文件夹后按 `F5`，有两个配置可选：`GDB · 命令行版`、`MSVC · 命令行版`。
调试时值得留意的断点位置：`strong_cycle_live_after_scope` 里那两条 `set_*`、
`CallbackRegistry::remove` 与 `trigger`。调试路线的选择与产物位置见 [`../../README.md`](../../README.md)。

## 运行后应当看到什么

`build\mingw\bin\app_cli.exe`（不带参数）：

`实测数据`
`Text`

```text
示例 07-standard-library/04-cpp-smart-pointers · 智能指针与回调注册表（命令行版）

== 项目输出 ==
资源工厂（unique_ptr 交出所有权）
  make_resource("texture") → texture：草地 256x256
  make_resource("audio")   → audio：脚步 3.5 秒
  make_resource("model")   → 空指针（未知种类）
  三个 unique_ptr 离开作用域就释放，存活资源数 0
  一批 3 个资源装进 vector：texture：草地 256x256、texture：岩壁 512x512、audio：脚步 3.5 秒
  vector 析构时逐个释放，存活资源数 0

共享所有权（shared_ptr 与 use_count）
  造一份 shared_ptr：use_count = 1
  拷贝出第二份：use_count = 2
  拷贝出第三份：use_count = 3
  释放第三份：use_count = 2
  对象本身：texture：草地 256x256
  两份都释放后对象才析构，存活资源数 0

观察与断环（weak_ptr）
  弱引用不增加计数：use_count = 1，weak.use_count() = 1
  lock() 提升之后 use_count = 2，提升成功
  对象销毁后：弱引用过期 是，lock() 得到空指针
  强引用互指：离开作用域后仍活着的节点数 2
  靠 weak_ptr 找到节点再断开环：存活数 0
  父子改成 weak_ptr 连法：离开作用域后存活数 0，弱引用过期 是

回调注册表（std::function）
  注册 3 个回调：audit、banner、counter
  触发 banner → 欢迎：资源加载完成
  触发 counter → 累计 1 次：加载完成
  触发 audit → audit 第 1 条：加载完成
  再触发 banner → 欢迎：第二次
  各回调的调用次数：audit 1、banner 2、counter 1
  注销 counter：成功
  注销后再触发 counter：找不到这个名字，lambda 里的计数器停在 1

== 自测 ==
  [通过] 1. 工厂用 make_unique 造 Texture，以基类指针返回
  [通过] 2. 认得出的只有 texture 与 audio，其余返回空指针
  [通过] 3. unique_ptr 离开作用域就析构对象，不用手写 delete
  [通过] 4. 经基类指针调用虚函数，得到派生类的说明
  [通过] 5. vector<unique_ptr<Resource>> 析构时逐个释放
  [通过] 6. unique_ptr 移动之后源指针为空，所有权不重复
  [通过] 7. 拷贝 shared_ptr 把 use_count 从 1 涨到 3
  [通过] 8. 逐份释放把 use_count 从 3 降回 1
  [通过] 9. 最后一份释放后对象才析构
  [通过] 10. unique_ptr 转移给 shared_ptr 之后，那一份所有权仍然只有一份
  [通过] 11. weak_ptr 不增加引用计数，lock() 提升之后才临时加一
  [通过] 12. 对象销毁后弱引用过期，lock() 得到空指针
  [通过] 13. 强引用互指后对象数不为 0
  [通过] 14. 靠 weak_ptr 找到节点、断开环之后存活数归零
  [通过] 15. weak_ptr 版本对象数归零
  [通过] 16. 按名字注册并触发，返回文本与调用次数都对
  [通过] 17. 回调注销后再触发不计数
  [通过] 18. 成员函数绑定与 lambda 捕获都能注册并触发

  自测结果：18 项中 18 项通过，全部通过
```

`build\mingw\bin\app_cli.exe --selftest`（只跑自测，没有项目输出）：

`实测数据`
`Text`

```text
示例 07-standard-library/04-cpp-smart-pointers · 智能指针与回调注册表（命令行版）

== 自测 ==
  [通过] 1. 工厂用 make_unique 造 Texture，以基类指针返回
  [通过] 2. 认得出的只有 texture 与 audio，其余返回空指针
  [通过] 3. unique_ptr 离开作用域就析构对象，不用手写 delete
  [通过] 4. 经基类指针调用虚函数，得到派生类的说明
  [通过] 5. vector<unique_ptr<Resource>> 析构时逐个释放
  [通过] 6. unique_ptr 移动之后源指针为空，所有权不重复
  [通过] 7. 拷贝 shared_ptr 把 use_count 从 1 涨到 3
  [通过] 8. 逐份释放把 use_count 从 3 降回 1
  [通过] 9. 最后一份释放后对象才析构
  [通过] 10. unique_ptr 转移给 shared_ptr 之后，那一份所有权仍然只有一份
  [通过] 11. weak_ptr 不增加引用计数，lock() 提升之后才临时加一
  [通过] 12. 对象销毁后弱引用过期，lock() 得到空指针
  [通过] 13. 强引用互指后对象数不为 0
  [通过] 14. 靠 weak_ptr 找到节点、断开环之后存活数归零
  [通过] 15. weak_ptr 版本对象数归零
  [通过] 16. 按名字注册并触发，返回文本与调用次数都对
  [通过] 17. 回调注销后再触发不计数
  [通过] 18. 成员函数绑定与 lambda 捕获都能注册并触发

  自测结果：18 项中 18 项通过，全部通过
```

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `resource_registry.hpp` 的 `Resource` | 拷贝构造与拷贝赋值被 `= delete` 掉，身份由地址决定；析构函数是 `virtual`，经基类指针删除派生对象才安全 |
| `resource_registry.hpp` 的 `live_count()` | 静态计数器，构造加一、析构减一；「有没有释放」由此变成可断言的数字 |
| `resource_registry.cpp` 的 `make_resource` | 内部用 `std::make_unique` 造派生对象，返回类型是 `unique_ptr<Resource>`，未知种类给空指针 |
| `resource_registry.cpp` 的 `strong_cycle_live_after_scope` | 父子各持对方的 `shared_ptr`，离开作用域后计数各剩 1；测完用 `weak_ptr` 找到节点并清掉一条强引用，不把对象留给进程 |
| `resource_registry.cpp` 的 `strong_cycle_live_after_break` | 与上一处相同的场景，区别只在测量点放在断环之后，返回 0 |
| `resource_registry.hpp` 的 `Node` | 强、弱两种父指针并存，只为对照；真实项目里一种关系只选一种连法 |
| `resource_registry.cpp` 的 `CallbackRegistry::add` | `map::emplace` 在名字已存在时不覆盖，重名注册直接返回 false；空回调也拒绝 |
| `resource_registry.cpp` 的 `CallbackRegistry::trigger` | 名字不在表里时 `out` 不动、计数不动，返回 false；这一条由自测第 17 项盯着 |
| `resource_registry.cpp` 的 `run_self_tests` | 18 项自测集中在最后，界面只负责把结果摆出来 |
| `main_cli.cpp` 的 `to_console_encoding` | 全项目唯一的编码转换点：控制台是 65001 就原样输出，是 936 就转 GBK |
| `CMakeLists.txt` 的字符集段 | 核心库内部一律用 `u8""` 字面量，因此 `-fexec-charset=GBK` 只影响非 u8 字面量 |

## 已知问题

- 自测第 13 项会让两个节点短暂地互相持有，测完立刻在函数内部断环；不这样做，
  这两个对象会一直留到进程结束，后面的存活计数全部要按增量算
- 对象计数来自静态变量，只在单线程下准确；多线程同时造对象会让数字对不上
- `CallbackRegistry::trigger` 会改动调用次数，因此不是 `const` 成员函数；
  表本身也不带锁，多线程使用要在外面加互斥量
- 注册表用 `std::map` 按名字有序存放，触发是按名字查找；回调数量很大时，
  名字查询会成为瓶颈，可以换成哈希表
- `std::bind` 只能固定实参，遇到重载的成员函数还要写 `static_cast` 指明是哪一个；
  统一调用写法见《07-标准库/B-04-可调用物的包装.md》第 3 节
- `Node` 里的强、弱两种父指针放在同一个类里，会让「这个节点的父指针到底是哪种」变得含糊，
  示例之外不要这样写
- 报告里 `make_resource` 只认 `texture` 与 `audio` 两个种类名，字符串是硬编码的；
  种类多起来应当换成枚举或注册表
