/**
 * main_cli.cpp —— 空模板 09 的命令行验收程序（C++）
 *
 * 这个文件**不需要改**：它按 4 个阶段使用 FileGuard 与 copy_file，
 * 把结果打印成《配置步骤.md》里的期望输出。你的实现写在 src/file_guard.cpp 里。
 * 界面版（src/main_gui_win32.cpp 与 src/main_gui_qt.cpp）用同一个核心模块，把日志与计数器显示在窗口上。
 *
 * 构建与运行：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 阶段的划分：
 *     阶段 1  构造、析构、读写、关闭（三条路径：正常、提前返回、抛异常）
 *     阶段 2  拷贝必须禁掉
 *     阶段 3  移动转移所有权
 *     阶段 4  异常安全：先做会失败的，再做不会失败的；失败不留半成品
 *
 * 程序会在当前目录下创建若干 raii_*.tmp 文件，运行结束前自己删掉。
 */
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "file_guard.hpp"

/* 完成阶段 2-1（在头文件里写上两行 = delete）之后，把下面两行取消注释。
 * 注意：这两条在骨架状态下也能编过——因为本类声明了移动操作，
 *       编译器已经把拷贝隐式删掉了。它们的作用是把「不可拷贝」钉死在代码里。 */
/* static_assert(!std::is_copy_constructible<FileGuard>::value, "FileGuard 不该可拷贝"); */
/* static_assert(!std::is_copy_assignable<FileGuard>::value, "FileGuard 不该可拷贝赋值"); */

/* 完成阶段 3-1（给移动操作补上 noexcept）之后，把下面这行取消注释。 */
/* static_assert(std::is_nothrow_move_constructible<FileGuard>::value, "移动构造必须标 noexcept"); */

/* ==================================================================
 * 阶段 1 · 三条路径
 * ================================================================== */

static void path_normal(const char *path)
{
    FileGuard guard(path, FileGuard::Mode::Write);
    guard.write("normal\n", 7);
    /* 离开作用域时析构函数自动关闭 */
}

static void path_early_return(const char *path)
{
    FileGuard guard(path, FileGuard::Mode::Write);
    guard.write("early\n", 6);
    std::printf("  提前返回\n");
    return;   /* 提前返回同样会走析构 */
}

static void path_throw(const char *path)
{
    FileGuard guard(path, FileGuard::Mode::Write);
    guard.write("half\n", 5);
    throw std::runtime_error("写到一半失败了");
}

static void stage1()
{
    std::printf("=== 阶段 1：三条路径 ===\n");

    std::printf("[正常路径]\n");
    path_normal("raii_a.tmp");

    std::printf("[提前返回]\n");
    path_early_return("raii_b.tmp");

    std::printf("[异常路径]\n");
    try {
        path_throw("raii_c.tmp");
    } catch (const std::exception &e) {
        std::printf("  捕获：%s\n", e.what());
    }

    std::printf("  live = %d, close = %d\n", FileGuard::live_count(), FileGuard::close_count());

    std::remove("raii_a.tmp");
    std::remove("raii_b.tmp");
    std::remove("raii_c.tmp");
}

/* ==================================================================
 * 阶段 2 · 拷贝必须禁掉
 * ================================================================== */
static void stage2()
{
    std::printf("\n=== 阶段 2：拷贝必须禁掉 ===\n");

    /* 完成阶段 2-1 之后，把下面这一行的注释去掉：它**必须编译失败**，
     * 报错关键词是 "use of deleted function"。
     * FileGuard copy = original;
     */

    std::printf("  拷贝构造与拷贝赋值都不可用（静态检查见本文件顶部的 static_assert）\n");
}

/* ==================================================================
 * 阶段 3 · 移动转移所有权
 * ================================================================== */
static void stage3()
{
    std::printf("\n=== 阶段 3：移动转移所有权 ===\n");

    {
        FileGuard a("raii_d.tmp", FileGuard::Mode::Write);
        a.write("moved\n", 6);

        FileGuard b(std::move(a));   /* 移动构造：句柄交给 b，a 变成空 */

        std::printf("  移动之后：a.valid() = %s, b.valid() = %s\n",
                    a.valid() ? "true" : "false", b.valid() ? "true" : "false");
        std::printf("  live = %d\n", FileGuard::live_count());
    }
    std::printf("  作用域结束：live = %d, close = %d\n",
                FileGuard::live_count(), FileGuard::close_count());

    std::vector<FileGuard> guards;
    guards.reserve(1);   /* 故意留小，让容器扩容时搬运元素 */
    guards.emplace_back("raii_e.tmp", FileGuard::Mode::Write);
    guards.emplace_back("raii_f.tmp", FileGuard::Mode::Write);
    std::printf("  容器里 %zu 个句柄，live = %d\n", guards.size(), FileGuard::live_count());

    guards.clear();
    std::printf("  清空容器后：live = %d, close = %d\n",
                FileGuard::live_count(), FileGuard::close_count());

    std::remove("raii_d.tmp");
    std::remove("raii_e.tmp");
    std::remove("raii_f.tmp");
}

/* ==================================================================
 * 阶段 4 · 异常安全
 * ================================================================== */

/* 已给出：写一个测试用的源文件 */
static void write_demo_source(const char *path)
{
    FileGuard out(path, FileGuard::Mode::Write);
    const char *text = "0123456789abcdefghijklmn";   /* 24 字节 */
    out.write(text, std::strlen(text));
}

/* 已给出：判断文件能不能打开（用的是 FileGuard，因此也会出现在日志里） */
static bool file_exists(const char *path)
{
    try {
        FileGuard probe(path, FileGuard::Mode::Read);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

/* 已给出：把文件内容读出来打印 */
static void print_file(const char *path)
{
    FileGuard in(path, FileGuard::Mode::Read);
    char buf[64] = "";
    const std::size_t got = in.read(buf, sizeof(buf) - 1);
    buf[got] = '\0';
    std::printf("%s", buf);
}

static void stage4()
{
    std::printf("\n=== 阶段 4：异常安全 ===\n");

    const char *src = "raii_src.tmp";
    const char *dst = "raii_dst.tmp";
    write_demo_source(src);

    std::printf("[正常复制]\n");
    try {
        copy_file(src, dst);
        std::printf("  复制成功，校验 dst 的内容：");
        print_file(dst);
        std::printf("\n");
    } catch (const std::exception &e) {
        std::printf("  复制失败：%s\n", e.what());
    }

    std::printf("[源文件不存在]\n");
    try {
        copy_file("raii_no_such.tmp", dst);
        std::printf("  不该走到这里\n");
    } catch (const std::exception &e) {
        std::printf("  抛出异常：%s\n", e.what());
    }

    std::printf("[中途注入失败：写入 8 字节之后出错]\n");
    try {
        copy_file(src, dst, 8);
        std::printf("  不该走到这里\n");
    } catch (const std::exception &e) {
        std::printf("  抛出异常：%s\n", e.what());
    }
    std::printf("  失败之后 dst 还存在吗：%s\n", file_exists(dst) ? "是" : "否");
    std::printf("  live = %d, close = %d\n", FileGuard::live_count(), FileGuard::close_count());

    std::remove(src);
    std::remove(dst);
}

int main()
{
    stage1();
    stage2();
    stage3();
    stage4();

    std::printf("\n=== 收尾 ===\n");
    std::printf("  live = %d（所有句柄都应当被释放）\n", FileGuard::live_count());
    return 0;
}
