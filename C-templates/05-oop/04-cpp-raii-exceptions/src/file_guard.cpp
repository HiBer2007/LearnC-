/**
 * file_guard.cpp —— FileGuard 与 copy_file 的实现（空模板 09）
 *
 * 骨架里的每个函数都是**能编译、能运行**的占位版本：它们不打开文件、不关闭文件，
 * 也不搬移句柄，因此运行结果与《配置步骤.md》里的期望值处处不同。
 * 把每个 TODO 换成真正的实现，逐阶段对照验收标准。
 */
#include "file_guard.hpp"

#include <cstring>
#include <stdexcept>

/* ==================================================================
 * 计数器（已给出）
 * ================================================================== */

int FileGuard::live_count_ = 0;
int FileGuard::close_count_ = 0;

/* ==================================================================
 * 阶段 1 · 构造、析构、读写、关闭
 * ================================================================== */

/* TODO（阶段 1-1）
 * 要求：
 *   1. 按 mode 选择 "rb" 或 "wb"，用 std::fopen 打开 path；
 *   2. 打不开就抛 std::runtime_error，消息里带上路径；
 *   3. 打开成功时打印 "  打开 <path>"，并让 live_count_ 自增。
 * 提示：消息可以用 std::string("打不开文件：") + path 拼出来（需要 <string>）。
 * 验收：阶段 1 的三条路径都打印出「打开」与「关闭」两行；
 *       对一个不存在的文件用 Mode::Read 构造时抛出异常，且 live 不增加。
 */
FileGuard::FileGuard(const char *path, Mode mode) : fp_(nullptr), path_(path)
{
    (void)mode;
    ++live_count_;
}

/* TODO（阶段 1-2）
 * 要求：调用 close()，并让 live_count_ 自减。
 * 验收：程序结束时 live 回到 0；每个文件只出现一次「关闭」。
 */
FileGuard::~FileGuard() noexcept
{
    /* TODO */
}

/* TODO（阶段 1-3）
 * 要求：句柄有效时返回 true。
 * 验收：打开成功的对象 valid() 为 true；被移动走的对象为 false。
 */
bool FileGuard::valid() const noexcept
{
    return false;
}

/* TODO（阶段 1-4）
 * 要求：
 *   1. 句柄为空时直接返回（这一步保证幂等：关两次不会出错）；
 *   2. 关闭句柄，把 fp_ 置空；
 *   3. 打印 "  关闭 <path>"，并让 close_count_ 自增。
 * 验收：移动之后、以及阶段 4 里手动关闭之后再让析构函数执行，都不会重复计数。
 */
void FileGuard::close() noexcept
{
    /* TODO */
}

/* TODO（阶段 1-5）
 * 要求：用 std::fread 读最多 n 字节，返回实际读到的字节数；
 *       出错（std::ferror 非零）时抛 std::runtime_error。
 * 验收：阶段 4 里读回刚写出去的内容，字节数与内容都对得上。
 */
std::size_t FileGuard::read(void *buf, std::size_t n)
{
    (void)buf;
    (void)n;
    return 0;
}

/* TODO（阶段 1-6）
 * 要求：用 std::fwrite 写 n 字节，返回实际写入的字节数；
 *       写入字节数少于 n 时抛 std::runtime_error。
 * 验收：阶段 4 里复制出来的文件内容与源文件一致。
 */
std::size_t FileGuard::write(const void *buf, std::size_t n)
{
    (void)buf;
    (void)n;
    return 0;
}

/* ==================================================================
 * 阶段 3 · 移动
 * ================================================================== */

/* TODO（阶段 3-1）
 * 要求：接管 other 的句柄与路径，把 other 置成「空」（句柄为空）；
 *       同时给头文件里的声明与这里的定义都补上 noexcept。
 * 验收：阶段 3 里移动之后源对象 valid() 为 false，
 *       目标对象 valid() 为 true，日志里每个文件仍然只关闭一次。
 */
FileGuard::FileGuard(FileGuard &&other) : fp_(nullptr), path_("")
{
    (void)other;
    ++live_count_;
}

/* TODO（阶段 3-2）
 * 要求：先判断自赋值，再关闭并释放自己手里的资源，然后接管 other 的，
 *       把 other 置空，返回 *this。
 * 提示：先 close() 自己（而不是先接管），否则原来的句柄会漏掉。
 */
FileGuard &FileGuard::operator=(FileGuard &&other)
{
    (void)other;
    return *this;
}

/* ==================================================================
 * 阶段 4 · 异常安全
 * ================================================================== */

/* TODO（阶段 4-1）
 * 要求：
 *   1. 先用 FileGuard 打开源文件（失败会抛，此时还没有任何副作用）；
 *   2. 再打开目标文件；
 *   3. 循环：读一块、写一块，直到读完；
 *   4. fail_after >= 0 且已写字节数超过它时，抛 std::runtime_error("写入失败（练习用）")；
 *   5. 一旦中途失败，必须把已经创建的目标文件删掉——注意 Windows 上不能删除
 *      还开着的文件，所以要**先关闭再 std::remove**，然后把异常继续抛出去。
 * 提示：关闭动作可以写在 catch 块里；析构函数随后会再调用一次 close()，
 *       这正是 close() 必须幂等的原因。
 * 验收：见《配置步骤.md》阶段 4 的四行输出。
 */
void copy_file(const char *src, const char *dst, long fail_after)
{
    (void)src;
    (void)dst;
    (void)fail_after;
    /* 占位版本什么也不做：调用方会发现目标文件不存在或内容为空 */
}
