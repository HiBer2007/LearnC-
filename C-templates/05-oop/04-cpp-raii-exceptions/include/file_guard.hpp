/**
 * file_guard.hpp —— 文件句柄的 RAII 包装（空模板 09）
 *
 * 这个类管着一份资源：一个 std::FILE *。三条规矩：
 *   1. 构造时拿到资源，拿不到就抛异常（对象根本没被造出来，不会留下半成品）
 *   2. 析构时释放资源，恰好一次，且不抛异常
 *   3. 拷贝必须禁掉（两个对象关同一个句柄会出错），只允许移动（所有权转移）
 *
 * 接口已经定好，main_cli.cpp 与 main_gui.cpp 都按这份接口写好了驱动。
 * 你要做的是在 src/file_guard.cpp 里把标了 TODO 的成员函数实现出来。
 *
 * 构建与运行：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *     build\mingw\bin\app_gui.exe
 */
#ifndef FILE_GUARD_HPP
#define FILE_GUARD_HPP

#include <cstddef>
#include <cstdio>

class FileGuard {
public:
    enum class Mode { Read, Write };

    /* TODO（阶段 1-1）：打开文件，失败抛 std::runtime_error；成功时打印 "  打开 <path>" */
    FileGuard(const char *path, Mode mode);

    /* TODO（阶段 1-2）：释放资源（调用 close），并让 live_count_ 自减。声明为 noexcept */
    ~FileGuard() noexcept;

    /* TODO（阶段 2-1）
     * 要求：在下面这两行的位置，显式禁掉拷贝构造与拷贝赋值（各一行 = delete）。
     * 提示：因为本类声明了移动操作，编译器其实已经把拷贝隐式删掉了；
     *       显式写出来是把「这个类管着资源」这件事写进代码，报错措辞也会从
     *       "call to implicitly-deleted copy constructor" 变成 "use of deleted function"。
     * 验收：取消 main_cli.cpp 里那行 static_assert 的注释后能编过；
     *       取消「FileGuard copy = original;」那行的注释后必须编译失败。
     */

    /* TODO（阶段 3-1）：接管 other 的句柄与路径，把 other 置成「空」；
     *                  同时给声明与定义都补上 noexcept。
     * 验收：阶段 3 里移动之后源对象 valid() 为 false，
     *       日志里每个文件仍然只出现一次「关闭」。 */
    FileGuard(FileGuard &&other);

    /* TODO（阶段 3-2）：处理自赋值、先释放自己手里的资源、再接管 other 的，返回 *this。 */
    FileGuard &operator=(FileGuard &&other);

    /* TODO（阶段 1-3）：句柄是否有效。 */
    bool valid() const noexcept;

    /* 已给出：路径，构造时记下来的 */
    const char *path() const noexcept { return path_; }

    /* TODO（阶段 1-4）：关闭句柄。必须**幂等**：关过之后再调用不出错、不重复计数。 */
    void close() noexcept;

    /* TODO（阶段 1-5）：读 n 字节，返回实际读到的字节数；失败抛 std::runtime_error。 */
    std::size_t read(void *buf, std::size_t n);

    /* TODO（阶段 1-6）：写 n 字节，返回实际写入的字节数；失败抛 std::runtime_error。 */
    std::size_t write(const void *buf, std::size_t n);

    /* 已给出：两个计数器，供验收使用 */
    static int live_count() { return live_count_; }
    static int close_count() { return close_count_; }

private:
    std::FILE  *fp_;
    const char *path_;

    static int live_count_;
    static int close_count_;
};

/* ------------------------------------------------------------------
 * 用 RAII 把 src 的内容复制到 dst
 *
 * TODO（阶段 4-1）
 * 要求：
 *   1. 先做会失败的事：打开源文件（失败就抛，此时还没有任何副作用）；
 *   2. 再打开目标文件；
 *   3. 逐块复制（每块 8 字节），写完关闭；
 *   4. 中途失败时抛异常，并且**不留下半成品**：把已经创建的目标文件删掉（std::remove）；
 *   5. 两个句柄都用 FileGuard 管，函数里一处手写的关闭都不要有。
 * 参数 fail_after：练习用的注入点。≥ 0 时，写入这么多字节之后故意抛 std::runtime_error，
 *   用来验证「失败之后资源被收干净、半成品被删掉」。正常使用时传默认值 -1。
 * 验收：见《配置步骤.md》阶段 4。
 * ------------------------------------------------------------------ */
void copy_file(const char *src, const char *dst, long fail_after = -1);

#endif /* FILE_GUARD_HPP */
