/**
 * file_handle.hpp —— RAII 包装：把「打开的文件」当成一个对象来管
 *
 * 对应教材：《05-类与面向对象/05-RAII 与资源管理.md》第 2、4 节
 *           《05-类与面向对象/03-构造与析构.md》第 3、7 节
 *           《05-类与面向对象/04-拷贝与移动.md》第 4、5 节
 *
 * 两个包装类：
 *   FileHandle  持有 FILE*，析构时 fclose。不能拷贝，只能移动
 *   ScopedPath  记住一个文件名，析构时删掉它
 *
 * 它们的意义在于：不管函数是正常返回、提前 return 还是抛异常，
 * 只要对象离开作用域，资源一定被释放。
 */
#ifndef FILE_HANDLE_HPP
#define FILE_HANDLE_HPP

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

class FileHandle {
public:
    /** 空句柄：没有关联任何文件 */
    FileHandle() noexcept = default;

    /** 打开文件。失败时抛 std::runtime_error，因此拿到手的对象一定是可用的 */
    FileHandle(const std::string &path, const char *mode);

    /** 析构时关闭文件 —— RAII 的全部要点就在这一行 */
    ~FileHandle();

    /* 资源不能有两份，因此拷贝一律禁止 */
    FileHandle(const FileHandle &) = delete;
    FileHandle &operator=(const FileHandle &) = delete;

    /* 移动：把句柄交出去，源对象变成空句柄 */
    FileHandle(FileHandle &&other) noexcept;
    FileHandle &operator=(FileHandle &&other) noexcept;

    /** 提前关闭。析构时再调一次也无害 */
    void close() noexcept;

    bool is_open() const noexcept { return file_ != nullptr; }
    const std::string &path() const noexcept { return path_; }

    /** 写一行（自动补换行）。句柄无效时抛出异常 */
    void write_line(const std::string &text);

    /** 读一行，去掉行尾换行。读到文件末尾返回 false */
    bool read_line(std::string &line);

    /** 当前活着的句柄对象个数 */
    static std::size_t live_count() noexcept { return live_count_; }

    /** 当前真正打开着的文件个数。与 live_count 的区别见自测 */
    static std::size_t open_count() noexcept { return open_count_; }

private:
    std::FILE *file_ = nullptr;         /* 类内初始化器：默认就是「没打开」 */
    std::string path_;

    static std::size_t live_count_;
    static std::size_t open_count_;
};

class ScopedPath {
public:
    explicit ScopedPath(std::string path) : path_(std::move(path)) {}
    ~ScopedPath();

    ScopedPath(const ScopedPath &) = delete;
    ScopedPath &operator=(const ScopedPath &) = delete;
    ScopedPath(ScopedPath &&other) noexcept;
    ScopedPath &operator=(ScopedPath &&other) noexcept;

    /** 放弃删除，把文件留给别人 */
    void release() noexcept { path_.clear(); }

    const std::string &path() const noexcept { return path_; }

    /** 文件当前是否存在 */
    bool exists() const;

private:
    std::string path_;
};

/** 文件是否存在。查文件请用这个函数：
    写成 ScopedPath("x").exists() 会临时造一个对象，它析构时把文件删掉。 */
bool file_exists(const std::string &path);

#endif /* FILE_HANDLE_HPP */
