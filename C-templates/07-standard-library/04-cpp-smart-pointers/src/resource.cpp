/**
 * resource.cpp —— 练习模板 04 的核心逻辑（智能指针与回调注册）
 *
 * 4 个阶段的实现都写在这个文件里。骨架给的是占位实现：
 * 能编译、能运行、结果明显不对（返回空指针、计数不动）。
 * 各阶段的任务、验收标准与自查方法见同目录《配置步骤.md》。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 */
#include "resource.hpp"

namespace res {

/* ==================================================================
 * Session：构造与析构（已给出，用来核对资源有没有被释放）
 * ================================================================== */

int  Session::alive_ = 0;
long Session::created_ = 0;

Session::Session(std::string name) : name_(std::move(name))
{
    ++alive_;
    ++created_;
}

Session::~Session()
{
    --alive_;
}

int Session::alive()
{
    return alive_;
}

long Session::created()
{
    return created_;
}

/* ==================================================================
 * 阶段 1：unique_ptr 与自定义删除器
 * ================================================================== */

/* TODO（阶段 1-1）：用 std::make_unique 造一个 Session 返回。
 *
 * 提示：std::make_unique<Session>(name)；它的实参会转给 Session 的构造函数。
 *       不要写 new Session(name)——那也能编译，但练习的重点是工厂函数把所有权的
 *       产生点收在一处。
 *
 * 验收：session 那一行打印出 alpha 且 alive = 1；reset 之后 alive 回到 0。 */
std::unique_ptr<Session> make_session(const std::string &name)
{
    (void)name;
    return std::unique_ptr<Session>();   /* 占位实现：空指针 */
}

/* TODO（阶段 1-2）：用 std::fopen 打开文件，交给带删除器的 unique_ptr。
 *
 * 提示：
 *   1. std::FILE *f = std::fopen(path.c_str(), "rb");
 *   2. 失败时返回 FilePtr(nullptr, std::fclose)；
 *   3. 成功时返回 FilePtr(f, std::fclose)：文件会在 unique_ptr 析构或 reset 时
 *      自动关闭，用户代码里一个 fclose 都不用写——这就是自定义删除器的价值。
 *
 * 验收：CMakeLists.txt 那一行打印 opened = yes；换成不存在的路径打印 no 且不崩溃。 */
FilePtr open_file(const std::string &path)
{
    (void)path;
    return FilePtr(nullptr, std::fclose);   /* 占位实现：没有打开任何文件 */
}

/* ==================================================================
 * 阶段 2：shared_ptr 与引用计数
 * ================================================================== */

/* TODO（阶段 2-1）：用 std::make_shared 造一个 Session。
 *
 * 提示：std::make_shared<Session>(name)。与 new 相比，对象与控制块一次分配，
 *       异常安全也更好；代价是自定义删除器的场合用不了 make_shared。
 *
 * 验收：make_shared 那一行 use_count = 1。 */
std::shared_ptr<Session> make_shared_session(const std::string &name)
{
    (void)name;
    return std::shared_ptr<Session>();   /* 占位实现：空指针 */
}

/* TODO（阶段 2-2）：按值接收，返回进入函数时看到的引用计数。
 *
 * 提示：直接返回 static_cast<int>(s.use_count()) 即可；不要把它转存到别处，
 *       也不要再复制一份。这个数字比调用方看到的多 1，多出来的就是参数自己。
 *
 * 验收：touch 那一行是 3（调用方一份 + 局部 copy 一份 + 参数一份）。 */
int touch(std::shared_ptr<Session> s)
{
    (void)s;
    return -1;   /* 占位值 */
}

/* ==================================================================
 * 阶段 3：循环引用与 weak_ptr
 * ================================================================== */

int Node::alive_ = 0;

Node::Node(std::string n) : name(std::move(n))
{
    ++alive_;
}

Node::~Node()
{
    --alive_;
}

int Node::alive()
{
    return alive_;
}

/* TODO（阶段 3-1）：用 std::make_shared 造一个 Node。
 *
 * 验收：阶段 3 里两次演示的 alive 计数。 */
std::shared_ptr<Node> make_node(const std::string &name)
{
    (void)name;
    return std::shared_ptr<Node>();   /* 占位实现：空指针 */
}

/* TODO（阶段 3-2）：两个节点互相强引用：a->next = b; b->next = a;
 *
 * 这会形成环：a 的计数里有 b 的一份，b 的计数里有 a 的一份，
 * 函数返回后局部变量销毁，两个节点仍然互相持有，谁也释放不掉。
 *
 * 验收：两个 use_count 都变成 2，出了作用域 alive 仍然是 2（泄漏）。 */
void link_strong(const std::shared_ptr<Node> &a, const std::shared_ptr<Node> &b)
{
    (void)a;
    (void)b;
    /* 占位实现：什么都没做，因此不会出现循环引用的现象 */
}

/* TODO（阶段 3-3）：a->next 强引用 b，b->back 弱引用 a。
 *
 * 提示：weak_ptr 由 shared_ptr 赋值而来（b->back = a），它不增加引用计数；
 *       只有一条强引用链，没有环，两个节点都能正常析构。
 *
 * 验收：两个 use_count 都是 1，出了作用域 alive 回到 0。 */
void link_weak(const std::shared_ptr<Node> &a, const std::shared_ptr<Node> &b)
{
    (void)a;
    (void)b;
    /* 占位实现：什么都没做 */
}

/* TODO（阶段 3-4）：从弱引用取回强引用。
 *
 * 提示：
 *   1. 先判断 w.expired()，失效就返回空的 shared_ptr；
 *   2. 否则返回 w.lock()——注意 lock 之后对象就多了一个强引用，生命期被延长；
 *   3. 不用 expired 直接 lock 也可以（lock 失败返回空），两种写法对照着看。
 *
 * 验收：peer_of(b->back) 打印出 a 的名字。 */
std::shared_ptr<Node> peer_of(const std::weak_ptr<Node> &w)
{
    (void)w;
    return std::shared_ptr<Node>();   /* 占位实现：空指针 */
}

/* ==================================================================
 * 阶段 4：std::function 回调注册表
 * ================================================================== */

/* TODO（阶段 4-1）：把一个回调登记到事件名下。
 *
 * 提示：
 *   1. handler 已经是一个 std::function 对象，直接 std::move 进容器即可；
 *   2. 同一个事件可以登记多个，顺序按登记先后（emit 时依次调用）；
 *   3. 这里不查重，也不覆盖。
 *
 * 验收：handlers 打印 4（三次 tick + 一次 tock）。 */
void Registry::on(const std::string &event, Handler handler)
{
    (void)event;
    (void)handler;
    /* 占位实现：什么都没做 */
}

/* TODO（阶段 4-2）：触发事件，返回真正调用了几次。
 *
 * 提示：
 *   1. 遍历 handlers_，名字对得上就调用 handler(value)，计数加一；
 *   2. 没有登记过就返回 0；
 *   3. 注意不要在遍历过程中修改 handlers_（例如在回调里调用 on），
 *      那会让迭代器失效，见《07-标准库/B-02-std-string 与 string_view.md》第 4 节
 *      讲的「失效规则」在容器上的同一类问题。
 *
 * 验收：emit("tick", 5) 返回 3，emit("none", 1) 返回 0。 */
int Registry::emit(const std::string &event, int value)
{
    (void)event;
    (void)value;
    return -1;   /* 占位值 */
}

} /* namespace res */
