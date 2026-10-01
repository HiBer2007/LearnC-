/**
 * resource.hpp —— 练习模板 04 的核心接口（C++）
 *
 * 接口已经定好，src/main_cli.cpp 按 4 个阶段调用它们。
 * 你要做的是在 src/resource.cpp 里把标了 TODO 的函数与成员实现出来。
 *
 * 四个阶段的对应关系：
 *     阶段 1   make_session、open_file、Session      unique_ptr 与自定义删除器
 *     阶段 2   make_shared_session、touch、peek      shared_ptr 与引用计数
 *     阶段 3   make_node、link_strong、link_weak、
 *              peer_of                              循环引用与 weak_ptr
 *     阶段 4   Registry                              std::function 回调注册表
 */
#ifndef RESOURCE_HPP
#define RESOURCE_HPP

#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace res {

/* ==================================================================
 * 阶段 1：unique_ptr 与自定义删除器
 * ================================================================== */

/* 会话对象：构造与析构各改一次存活计数，用来核对资源有没有被释放 */
class Session {
public:
    explicit Session(std::string name);
    ~Session();

    const std::string &name() const { return name_; }

    static int  alive();          /* 当前活着的对象个数 */
    static long created();        /* 累计构造次数 */

private:
    std::string name_;
    static int  alive_;
    static long created_;
};

/* TODO（阶段 1-1）：用 std::make_unique 造一个 Session 返回。
 * 提示：make_unique 的参数会转给构造函数；返回类型是 std::unique_ptr<Session>。 */
std::unique_ptr<Session> make_session(const std::string &name);

/* 用 std::fclose 当删除器的文件指针类型。
 * 自定义删除器写成函数指针类型时，unique_ptr 的第二个模板参数就是它。 */
using FilePtr = std::unique_ptr<std::FILE, int (*)(std::FILE *)>;

/* TODO（阶段 1-2）：打开文件并返回带删除器的 unique_ptr。
 * 提示：
 *   1. std::fopen(path.c_str(), "rb")；
 *   2. 打开失败时返回 FilePtr(nullptr, std::fclose)；注意不能返回默认构造的
 *      FilePtr{}——删除器为空的 unique_ptr 析构时是安全的，但调用 reset 之前
 *      要保证删除器可用，显式传进去更清楚；
 *   3. 成功时返回 FilePtr(f, std::fclose)，文件会在 unique_ptr 析构时自动关闭。
 * 验收：main_cli.cpp 会打印文件是否打开成功，以及删除器是否真的跑了。 */
FilePtr open_file(const std::string &path);

/* ==================================================================
 * 阶段 2：shared_ptr 与引用计数
 * ================================================================== */

/* TODO（阶段 2-1）：用 std::make_shared 造一个 Session。
 * 提示：与 make_unique 对应；make_shared 只分配一次（控制块与对象在同一块内存里）。 */
std::shared_ptr<Session> make_shared_session(const std::string &name);

/* TODO（阶段 2-2）：按值接收一个 shared_ptr（调用方与这里各持有一份），
 * 返回进入函数时看到的引用计数。
 * 提示：函数体内 s.use_count() 就是「调用方那一份 + 参数这一份」的和；
 *       把它与下面的 peek 对照：一个是按值、一个是按 const 引用，计数不同。 */
int touch(std::shared_ptr<Session> s);

/* 已给出：按 const 引用接收，不增加引用计数 */
inline void peek(const std::shared_ptr<Session> &s)
{
    (void)s;
}

/* ==================================================================
 * 阶段 3：循环引用与 weak_ptr
 * ================================================================== */

/* 节点：next 是强引用，back 是弱引用，两种引用各演示一次 */
struct Node {
    std::string          name;
    std::shared_ptr<Node> next;   /* 强引用：会改变对方的引用计数 */
    std::weak_ptr<Node>   back;   /* 弱引用：不影响引用计数，也不会阻止析构 */

    explicit Node(std::string n);
    ~Node();

    static int alive();

private:
    static int alive_;
};

/* TODO（阶段 3-1）：用 std::make_shared 造一个 Node */
std::shared_ptr<Node> make_node(const std::string &name);

/* TODO（阶段 3-2）：让两个节点互相强引用：a->next = b; b->next = a;
 * 这会造成循环引用，两个节点谁也释放不掉。main_cli.cpp 会把这一点打印出来。 */
void link_strong(const std::shared_ptr<Node> &a, const std::shared_ptr<Node> &b);

/* TODO（阶段 3-3）：把 a->next 设为 b 的强引用，b->back 设为 a 的弱引用。
 * 这样只有一条强引用链，没有环，两个节点都能正常析构。 */
void link_weak(const std::shared_ptr<Node> &a, const std::shared_ptr<Node> &b);

/* TODO（阶段 3-4）：从弱引用取回强引用：先 expired() 判断，再 lock()。
 * 已经失效时返回空的 shared_ptr。 */
std::shared_ptr<Node> peer_of(const std::weak_ptr<Node> &w);

/* ==================================================================
 * 阶段 4：std::function 回调注册表
 * ================================================================== */

class Registry {
public:
    using Handler = std::function<void(int)>;

    /* TODO（阶段 4-1）：把一个回调登记到某个事件名下。
     * 提示：同一个事件可以登记多个回调，全部都要保留（顺序按登记先后）。 */
    void on(const std::string &event, Handler handler);

    /* TODO（阶段 4-2）：触发某个事件，把 value 依次交给它的每个回调，
     * 返回真正调用了几次；事件没有登记过回调时返回 0。 */
    int emit(const std::string &event, int value);

    std::size_t size() const { return handlers_.size(); }

private:
    std::vector<std::pair<std::string, Handler>> handlers_;
};

/* 已给出：给阶段 4 用的一个函数对象，配合 std::ref 演示 reference_wrapper。
 * 它有 operator()，因此既可以被 bind 成成员函数调用，也可以直接当可调用物。 */
class Accumulator {
public:
    void add(int v) { total_ += v; }
    void operator()(int v) { total_ += v; }
    int  total() const { return total_; }

private:
    int total_ = 0;
};

} /* namespace res */

#endif /* RESOURCE_HPP */
