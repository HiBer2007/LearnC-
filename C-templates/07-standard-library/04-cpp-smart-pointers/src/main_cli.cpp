/**
 * main_cli.cpp —— 练习模板 04 的命令行验收程序（C++）
 *
 * 这个文件**不需要改**：它按 4 个阶段调用 resource.cpp 里的函数，
 * 把结果打印成《配置步骤.md》里的验收输出。你的实现写在 src/resource.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 阶段的划分：
 *     阶段 1  unique_ptr：工厂函数、移动、reset、自定义删除器
 *     阶段 2  shared_ptr：引用计数、按值与按引用传参
 *     阶段 3  循环引用（泄漏）与 weak_ptr（断环）
 *     阶段 4  std::function 回调注册表、bind 与 reference_wrapper
 */
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "resource.hpp"

/* ------------------------------------------------------------------
 * 已给出：一个记账用的删除器，用来证明「自定义删除器真的被调用了」
 * ------------------------------------------------------------------ */
namespace {

struct Counted {
    int payload = 7;
};

int g_deleter_calls = 0;

struct CountedDeleter {
    void operator()(Counted *p) const
    {
        ++g_deleter_calls;
        delete p;
    }
};

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: unique_ptr and custom deleter ===\n";

    std::unique_ptr<res::Session> p = res::make_session("alpha");
    std::cout << "make_session    : " << (p ? p->name() : std::string("(null)"))
              << ", alive = " << res::Session::alive()
              << ", created = " << res::Session::created() << "\n";

    std::unique_ptr<res::Session> moved = std::move(p);
    std::cout << "after move      : from = " << (p ? "not null" : "(null)")
              << ", to = " << (moved ? moved->name() : std::string("(null)"))
              << ", alive = " << res::Session::alive() << "\n";

    moved.reset();
    std::cout << "after reset     : alive = " << res::Session::alive() << "\n";

    {
        std::unique_ptr<Counted, CountedDeleter> c(new Counted());
        std::cout << "counted payload : " << c->payload << "\n";
    }
    std::cout << "deleter calls   : " << g_deleter_calls << "\n";

    res::FilePtr f = res::open_file("CMakeLists.txt");
    std::cout << "open file       : " << (f ? "opened" : "(not opened)") << "\n";
    std::cout << "note            : the FILE is closed by the deleter, not by user code\n";

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: shared_ptr and use_count ===\n";
    {
        std::shared_ptr<res::Session> s = res::make_shared_session("beta");
        std::cout << "make_shared     : " << (s ? s->name() : std::string("(null)"))
                  << ", use_count = " << s.use_count() << "\n";
        {
            std::shared_ptr<res::Session> copy = s;
            std::cout << "after copy      : use_count = " << s.use_count() << "\n";
            std::cout << "touch saw count : " << res::touch(s) << "\n";
            std::cout << "after touch     : use_count = " << s.use_count() << "\n";
            res::peek(s);
            std::cout << "after peek      : use_count = " << s.use_count() << "\n";
        }
        std::cout << "after scope     : use_count = " << s.use_count() << "\n";
    }
    std::cout << "alive sessions  : " << res::Session::alive() << "\n";

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: weak_ptr breaks the cycle ===\n";

    /* 先演示正常的一侧：强引用 + 弱引用，没有环 */
    {
        std::shared_ptr<res::Node> a = res::make_node("c");
        std::shared_ptr<res::Node> b = res::make_node("d");
        res::link_weak(a, b);
        std::cout << "weak link       : a.use_count = " << a.use_count()
                  << ", b.use_count = " << b.use_count() << "\n";
        std::shared_ptr<res::Node> back = b ? res::peer_of(b->back) : std::shared_ptr<res::Node>();
        std::cout << "peer_of(b->back): " << (back ? back->name : std::string("(none)")) << "\n";
        std::cout << "alive in scope  : " << res::Node::alive() << "\n";
    }
    std::cout << "alive after scope: " << res::Node::alive() << "   (weak link frees both)\n";

    /* 再演示有问题的一侧：两个强引用互相指着，出了作用域也释放不掉 */
    {
        std::shared_ptr<res::Node> a = res::make_node("a");
        std::shared_ptr<res::Node> b = res::make_node("b");
        res::link_strong(a, b);
        std::cout << "strong cycle    : a.use_count = " << a.use_count()
                  << ", b.use_count = " << b.use_count() << "\n";
        std::cout << "alive in scope  : " << res::Node::alive() << "\n";
    }
    std::cout << "alive after scope: " << res::Node::alive()
              << "   (the strong cycle leaks these two nodes)\n";

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: callback registry ===\n";
    {
        res::Registry reg;
        res::Accumulator acc;
        int seen_total = 0;

        reg.on("tick", [&seen_total](int v) { seen_total += v; });        /* 捕获引用 */
        reg.on("tick", [](int v) { (void)v; });                           /* 不捕获 */
        reg.on("tick", std::bind(&res::Accumulator::add, &acc, std::placeholders::_1));
        reg.on("tock", [](int v) { (void)v; });

        std::cout << "handlers        : " << reg.size() << "\n";
        std::cout << "emit tick(5)    : called = " << reg.emit("tick", 5) << "\n";
        std::cout << "emit tick(7)    : called = " << reg.emit("tick", 7) << "\n";
        std::cout << "emit tock(1)    : called = " << reg.emit("tock", 1) << "\n";
        std::cout << "emit none(1)    : called = " << reg.emit("none", 1) << "\n";
        std::cout << "captured total  : " << seen_total << "\n";
        std::cout << "bound total     : " << acc.total() << "\n";

        /* reference_wrapper：存的是引用，改的还是 acc 自己 */
        std::function<void(int)> by_copy = acc;    /* 按值存：动的是副本 */
        by_copy(7);
        std::cout << "copy  total     : " << acc.total() << "   (copy changed its own state)\n";

        std::function<void(int)> by_ref = std::ref(acc);   /* 存引用 */
        by_ref(100);
        std::cout << "ref   total     : " << acc.total() << "\n";
    }

    std::cout << "\n=== done ===\n";
    return 0;
}
