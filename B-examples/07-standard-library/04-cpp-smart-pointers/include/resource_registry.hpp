/**
 * resource_registry.hpp —— 资源的所有权与回调注册，不依赖任何界面
 *
 * 库里面向人的文字一律是 UTF-8（u8"" 字面量），显示成哪种编码由界面层决定，
 * 命令行版见 src/main_cli.cpp 的 to_console_encoding。
 */
#ifndef RESOURCE_REGISTRY_HPP
#define RESOURCE_REGISTRY_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace registry {

/* ===============================================================
 *  资源：用存活对象数观察所有权有没有交出去、有没有被释放
 * =============================================================== */

class Resource {
public:
    explicit Resource(std::string name);
    virtual ~Resource();

    /* 资源的身份由地址决定，复制一个资源没有意义，直接禁掉 */
    Resource(const Resource &) = delete;
    Resource &operator=(const Resource &) = delete;

    const std::string &name() const { return name_; }

    /** 派生类各自的种类名：texture、audio 等 */
    virtual std::string kind() const = 0;

    /** 一行说明，界面直接摆出来 */
    virtual std::string describe() const;

    /** 当前存活的资源对象数。智能指针有没有析构对象，看这个数 */
    static std::size_t live_count() { return live_count_; }

private:
    std::string name_;
    static std::size_t live_count_;
};

class Texture : public Resource {
public:
    Texture(std::string name, int width, int height);

    std::string kind() const override;
    std::string describe() const override;

    int width() const { return width_; }
    int height() const { return height_; }

private:
    int width_;
    int height_;
};

class AudioClip : public Resource {
public:
    AudioClip(std::string name, double seconds);

    std::string kind() const override;
    std::string describe() const override;

    double seconds() const { return seconds_; }

private:
    double seconds_;
};

/** 工厂：按种类造一个派生对象，以基类指针交出所有权。
    返回 unique_ptr 而不是裸指针：所有权写在类型里，调用者不必查文档猜
    该不该 delete、该 delete 还是 delete[]，中途抛异常也不会漏掉释放。 */
std::unique_ptr<Resource> make_resource(const std::string &kind, const std::string &name);

/** 造一批资源。vector<unique_ptr<Resource>> 析构时逐个释放，不用手写循环 */
std::vector<std::unique_ptr<Resource>> make_default_batch();

/* ===============================================================
 *  节点：父子互指的两种连法，用来对照成环与断环
 * =============================================================== */

class Node {
public:
    explicit Node(std::string name);
    ~Node();

    const std::string &name() const { return name_; }
    static std::size_t live_count() { return live_count_; }

    /** 父节点持有子节点的强引用 */
    void set_child(std::shared_ptr<Node> child) { child_ = std::move(child); }

    /** 强引用版本的父指针：与 set_child 同时使用就成环 */
    void set_parent_strong(std::shared_ptr<Node> parent) { strong_parent_ = std::move(parent); }

    /** 弱引用版本的父指针：只观察，不增加引用计数 */
    void set_parent_weak(std::weak_ptr<Node> parent) { weak_parent_ = std::move(parent); }

    std::shared_ptr<Node> child() const { return child_; }

    /** 弱引用要先 lock() 提升成 shared_ptr 才能用；父节点没了就得到空指针 */
    std::shared_ptr<Node> parent() const { return weak_parent_.lock(); }
    bool parent_expired() const { return weak_parent_.expired(); }

    /** 断开环用：清掉一条强引用，另一头就能释放 */
    void clear_child() { child_.reset(); }
    void clear_parent_strong() { strong_parent_.reset(); }

private:
    std::string name_;
    std::shared_ptr<Node> child_;
    std::shared_ptr<Node> strong_parent_;
    std::weak_ptr<Node> weak_parent_;
    static std::size_t live_count_;
};

/** 强引用互指并离开作用域，返回之后还活着的节点数。
    函数内部量完会断开环，不把这几个对象留给进程 */
std::size_t strong_cycle_live_after_scope();

/** weak_ptr 版本走一遍的结果 */
struct WeakCycleOutcome {
    std::size_t live_after_scope = 0;   /**< 离开作用域后的存活数，应为 0 */
    bool weak_expired = false;          /**< 离开作用域后弱引用是否过期，应为 true */
    bool parent_reachable = false;      /**< 作用域内从子节点经弱引用够不够得到父节点 */
};

/** 同一结构改用 weak_ptr 连法走一遍，返回上面三项事实 */
WeakCycleOutcome weak_cycle_run();

/** 强引用互指后靠 weak_ptr 找到节点、断开环，返回断开之后的存活数 */
std::size_t strong_cycle_live_after_break();

/* ===============================================================
 *  回调注册表：std::function 装 lambda、成员函数绑定与普通函数
 * =============================================================== */

class CallbackRegistry {
public:
    /** 收一个事件名，返回一段说明文本 */
    using Callback = std::function<std::string(const std::string &)>;

    /** 注册。名字已存在时返回 false，不覆盖原来的回调 */
    bool add(const std::string &name, Callback callback);

    /** 注销，成功返回 true；名字不在表里返回 false */
    bool remove(const std::string &name);

    bool has(const std::string &name) const;

    /** 全部回调名字，按字典序 */
    std::vector<std::string> names() const;

    /** 按名字触发。名字不在表里时返回 false，out 不动 */
    bool trigger(const std::string &name, const std::string &event, std::string &out);

    /** 该回调被触发过多少次；名字不在表里返回 0 */
    std::size_t calls(const std::string &name) const;

    std::size_t size() const { return entries_.size(); }
    void clear() { entries_.clear(); }

private:
    struct Entry {
        Callback callback;
        std::size_t calls = 0;
    };
    std::map<std::string, Entry> entries_;      /**< 按名字有序，便于打印 */
};

/** 用来演示成员函数绑定：回调落在对象的方法上 */
class AuditLog {
public:
    std::string record(const std::string &event);
    std::size_t lines() const { return lines_; }

private:
    std::size_t lines_ = 0;
};

/* ===============================================================
 *  演示与自测
 * =============================================================== */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;     /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;        /**< "18 项中 18 项通过，全部通过" */
};

/** 项目输出：把工厂、共享、断环、回调各走一遍，返回多行 UTF-8 文本 */
std::string build_report();

/** 逐项核对所有权转移、引用计数、循环引用与回调注册 */
CheckResult run_self_tests();

}   /* namespace registry */

#endif /* RESOURCE_REGISTRY_HPP */
