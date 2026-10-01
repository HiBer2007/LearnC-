/**
 * file_handle.cpp —— RAII 包装的实现
 *
 * 这里的每个函数都要么成功、要么明确报错，不留「半开半闭」的中间状态。
 */
#include "file_handle.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

/* 静态成员属于类，不属于对象，存储在这里给出 */
std::size_t FileHandle::live_count_ = 0;
std::size_t FileHandle::open_count_ = 0;

/* ── FileHandle ───────────────────────────────────────── */

FileHandle::FileHandle(const std::string &path, const char *mode)
    : file_(std::fopen(path.c_str(), mode)), path_(path)
{
    ++live_count_;
    if (file_ == nullptr) {
        /* 构造失败就不存在这个对象，计数要退回去 */
        path_.clear();
        --live_count_;
        throw std::runtime_error("打不开文件：" + path);
    }
    ++open_count_;
}

FileHandle::~FileHandle()
{
    close();                    /* 无论正常离开还是抛异常离开，都会走到这里 */
    --live_count_;
}

FileHandle::FileHandle(FileHandle &&other) noexcept
    : file_(other.file_), path_(std::move(other.path_))
{
    other.file_ = nullptr;      /* 交出去之后源对象不再持有文件 */
    ++live_count_;              /* 打开着的文件数不变，只是换了主人 */
}

FileHandle &FileHandle::operator=(FileHandle &&other) noexcept
{
    if (this != &other) {
        close();                /* 先放掉自己手上的那个 */
        file_ = other.file_;
        path_ = std::move(other.path_);
        other.file_ = nullptr;
    }
    return *this;
}

void FileHandle::close() noexcept
{
    if (file_ != nullptr) {
        std::fclose(file_);
        file_ = nullptr;
        --open_count_;
    }
}

void FileHandle::write_line(const std::string &text)
{
    if (file_ == nullptr) {
        throw std::runtime_error("句柄没有打开，不能写");
    }
    std::fputs(text.c_str(), file_);
    std::fputc('\n', file_);
}

bool FileHandle::read_line(std::string &line)
{
    line.clear();
    if (file_ == nullptr) {
        return false;
    }

    int ch = std::fgetc(file_);
    if (ch == EOF) {
        return false;           /* 一个字符都没有，确实到末尾了 */
    }
    while (ch != EOF && ch != '\n') {
        if (ch != '\r') {       /* 顺手吃掉 Windows 的回车 */
            line += static_cast<char>(ch);
        }
        ch = std::fgetc(file_);
    }
    return true;                /* 最后一行没有换行也算读到了 */
}

/* ── ScopedPath ───────────────────────────────────────── */

ScopedPath::~ScopedPath()
{
    if (!path_.empty()) {
        std::remove(path_.c_str());
    }
}

ScopedPath::ScopedPath(ScopedPath &&other) noexcept
    : path_(std::move(other.path_))
{
    other.path_.clear();        /* 交出删除的责任 */
}

ScopedPath &ScopedPath::operator=(ScopedPath &&other) noexcept
{
    if (this != &other) {
        if (!path_.empty()) {
            std::remove(path_.c_str());     /* 先处理掉自己原来负责的文件 */
        }
        path_ = std::move(other.path_);
        other.path_.clear();
    }
    return *this;
}

bool ScopedPath::exists() const
{
    return file_exists(path_);
}

bool file_exists(const std::string &path)
{
    if (path.empty()) {
        return false;
    }
    std::FILE *probe = std::fopen(path.c_str(), "rb");
    if (probe == nullptr) {
        return false;
    }
    std::fclose(probe);
    return true;
}
