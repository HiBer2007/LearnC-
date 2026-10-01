/**
 * resource_registry.cpp —— 资源与回调的实现
 *
 * 这里没有任何界面代码：不含 <windows.h>，也不打印任何东西。
 * 面向人的文字一律写成 u8"" 字面量，于是库里流出来的一直是 UTF-8 字节。
 */
#include "resource_registry.hpp"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace registry {

namespace {

/* 自测的小工具：把每一项的结果记下来，不打印 */
class Checker {
public:
    void check(bool ok, const std::string &what, const std::string &detail = std::string())
    {
        ++result_.total;
        std::ostringstream line;
        if (ok) {
            ++result_.passed;
            line << u8"[通过] " << result_.total << ". " << what;
        } else {
            ++result_.failed;
            line << u8"[失败] " << result_.total << ". " << what;
            if (!detail.empty()) {
                line << u8"（" << detail << u8"）";
            }
        }
        result_.lines.push_back(line.str());
    }

    CheckResult take() { return result_; }

private:
    CheckResult result_;
};

/** 空指针也给一行说明，报告里不会出现半句话 */
std::string describe_or_empty(const std::unique_ptr<Resource> &resource)
{
    return resource ? resource->describe() : std::string(u8"空指针（未知种类）");
}

}   /* namespace */

/* ================= 资源 ================= */

std::size_t Resource::live_count_ = 0;

Resource::Resource(std::string name) : name_(std::move(name))
{
    ++live_count_;
}

Resource::~Resource()
{
    --live_count_;
}

std::string Resource::describe() const
{
    return kind() + u8"：" + name_;
}

Texture::Texture(std::string name, int width, int height)
    : Resource(std::move(name)), width_(width), height_(height)
{
}

std::string Texture::kind() const
{
    return "texture";
}

std::string Texture::describe() const
{
    std::ostringstream os;
    os << u8"texture：" << name() << " " << width_ << "x" << height_;
    return os.str();
}

AudioClip::AudioClip(std::string name, double seconds)
    : Resource(std::move(name)), seconds_(seconds)
{
}

std::string AudioClip::kind() const
{
    return "audio";
}

std::string AudioClip::describe() const
{
    std::ostringstream os;
    os << u8"audio：" << name() << " " << seconds_ << u8" 秒";
    return os.str();
}

std::unique_ptr<Resource> make_resource(const std::string &kind, const std::string &name)
{
    if (kind == "texture") {
        return std::make_unique<Texture>(name, 256, 256);
    }
    if (kind == "audio") {
        return std::make_unique<AudioClip>(name, 3.5);
    }
    return nullptr;         /* 未知种类：空指针，调用者必须判空 */
}

std::vector<std::unique_ptr<Resource>> make_default_batch()
{
    std::vector<std::unique_ptr<Resource>> batch;
    batch.push_back(std::make_unique<Texture>(u8"草地", 256, 256));
    batch.push_back(std::make_unique<Texture>(u8"岩壁", 512, 512));
    batch.push_back(std::make_unique<AudioClip>(u8"脚步", 3.5));
    return batch;
}

/* ================= 节点 ================= */

std::size_t Node::live_count_ = 0;

Node::Node(std::string name) : name_(std::move(name))
{
    ++live_count_;
}

Node::~Node()
{
    --live_count_;
}

std::size_t strong_cycle_live_after_scope()
{
    const std::size_t before = Node::live_count();
    std::weak_ptr<Node> parent_handle;
    {
        const std::shared_ptr<Node> parent = std::make_shared<Node>(u8"父");
        const std::shared_ptr<Node> child = std::make_shared<Node>(u8"子");
        parent->set_child(child);               /* 父持有子：强引用 */
        child->set_parent_strong(parent);       /* 子持有父：也是强引用，成环 */
        parent_handle = parent;
    }
    /* 两份局部的 shared_ptr 没了，可两个对象互相持有，计数各剩 1 */
    const std::size_t leaked = Node::live_count() - before;

    /* 量完把环断开，不把这几个对象留给进程：
       子节点一释放，它持有的那条父节点强引用也跟着没 */
    if (const std::shared_ptr<Node> parent = parent_handle.lock()) {
        parent->clear_child();
    }
    return leaked;
}

std::size_t strong_cycle_live_after_break()
{
    const std::size_t before = Node::live_count();
    std::weak_ptr<Node> parent_handle;
    {
        const std::shared_ptr<Node> parent = std::make_shared<Node>(u8"父");
        const std::shared_ptr<Node> child = std::make_shared<Node>(u8"子");
        parent->set_child(child);
        child->set_parent_strong(parent);
        parent_handle = parent;
    }
    /* 此刻存活数还是 2：先把父节点提升出来，再断开父指向子的那条强引用 */
    if (const std::shared_ptr<Node> parent = parent_handle.lock()) {
        parent->clear_child();
    }
    return Node::live_count() - before;
}

WeakCycleOutcome weak_cycle_run()
{
    WeakCycleOutcome outcome;
    const std::size_t before = Node::live_count();
    std::weak_ptr<Node> parent_handle;
    {
        const std::shared_ptr<Node> parent = std::make_shared<Node>(u8"父");
        const std::shared_ptr<Node> child = std::make_shared<Node>(u8"子");
        parent->set_child(child);
        child->set_parent_weak(parent);

        /* 活着的时候，从子节点经弱引用够得到父节点 */
        outcome.parent_reachable = child->parent() != nullptr && !child->parent_expired();
        parent_handle = parent;
    }
    outcome.live_after_scope = Node::live_count() - before;
    outcome.weak_expired = parent_handle.expired();
    return outcome;
}

/* ================= 回调注册表 ================= */

bool CallbackRegistry::add(const std::string &name, Callback callback)
{
    if (name.empty() || !callback) {
        return false;
    }
    /* emplace 在名字已存在时不覆盖，second 为 false */
    return entries_.emplace(name, Entry{ std::move(callback), 0 }).second;
}

bool CallbackRegistry::remove(const std::string &name)
{
    return entries_.erase(name) > 0;
}

bool CallbackRegistry::has(const std::string &name) const
{
    return entries_.find(name) != entries_.end();
}

std::vector<std::string> CallbackRegistry::names() const
{
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (const auto &item : entries_) {
        out.push_back(item.first);
    }
    return out;
}

bool CallbackRegistry::trigger(const std::string &name, const std::string &event,
                               std::string &out)
{
    const auto it = entries_.find(name);
    if (it == entries_.end()) {
        return false;       /* 名字不在表里：out 不动，计数也不动 */
    }
    out = it->second.callback(event);
    ++it->second.calls;
    return true;
}

std::size_t CallbackRegistry::calls(const std::string &name) const
{
    const auto it = entries_.find(name);
    return it == entries_.end() ? 0 : it->second.calls;
}

std::string AuditLog::record(const std::string &event)
{
    ++lines_;
    std::ostringstream os;
    os << u8"audit 第 " << lines_ << u8" 条：" << event;
    return os.str();
}

/* ================= 演示与自测 ================= */

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << total << u8" 项中 " << passed << u8" 项通过";
    if (failed == 0) {
        os << u8"，全部通过";
    } else {
        os << u8"，" << failed << u8" 项失败";
    }
    return os.str();
}

std::string build_report()
{
    std::ostringstream os;

    /* ---------- 工厂 ---------- */
    os << u8"资源工厂（unique_ptr 交出所有权）\n";
    {
        const std::unique_ptr<Resource> texture = make_resource("texture", u8"草地");
        const std::unique_ptr<Resource> audio = make_resource("audio", u8"脚步");
        const std::unique_ptr<Resource> unknown = make_resource("model", u8"水面");
        os << u8"  make_resource(\"texture\") → " << describe_or_empty(texture) << "\n";
        os << u8"  make_resource(\"audio\")   → " << describe_or_empty(audio) << "\n";
        os << u8"  make_resource(\"model\")   → " << describe_or_empty(unknown) << "\n";
    }
    os << u8"  三个 unique_ptr 离开作用域就释放，存活资源数 " << Resource::live_count() << "\n";

    {
        const std::vector<std::unique_ptr<Resource>> batch = make_default_batch();
        os << u8"  一批 " << batch.size() << u8" 个资源装进 vector：";
        for (std::size_t i = 0; i < batch.size(); ++i) {
            if (i != 0) {
                os << u8"、";
            }
            os << batch[i]->describe();
        }
        os << "\n";
    }
    os << u8"  vector 析构时逐个释放，存活资源数 " << Resource::live_count() << "\n";

    /* ---------- 共享所有权 ---------- */
    os << u8"\n共享所有权（shared_ptr 与 use_count）\n";
    {
        const std::shared_ptr<Resource> first = std::make_shared<Texture>(u8"草地", 256, 256);
        os << u8"  造一份 shared_ptr：use_count = " << first.use_count() << "\n";
        const std::shared_ptr<Resource> second = first;
        os << u8"  拷贝出第二份：use_count = " << first.use_count() << "\n";
        std::shared_ptr<Resource> third = second;
        os << u8"  拷贝出第三份：use_count = " << third.use_count() << "\n";
        third.reset();
        os << u8"  释放第三份：use_count = " << first.use_count() << "\n";
        os << u8"  对象本身：" << first->describe() << "\n";
    }
    os << u8"  两份都释放后对象才析构，存活资源数 " << Resource::live_count() << "\n";

    /* ---------- 观察与断环 ---------- */
    os << u8"\n观察与断环（weak_ptr）\n";
    std::weak_ptr<Resource> watcher;
    {
        const std::shared_ptr<Resource> owner = std::make_shared<Texture>(u8"岩壁", 512, 512);
        watcher = owner;
        os << u8"  弱引用不增加计数：use_count = " << owner.use_count()
           << u8"，weak.use_count() = " << watcher.use_count() << "\n";
        const std::shared_ptr<Resource> promoted = watcher.lock();
        os << u8"  lock() 提升之后 use_count = " << owner.use_count()
           << u8"，提升" << (promoted ? u8"成功" : u8"失败") << "\n";
    }
    os << u8"  对象销毁后：弱引用过期 " << (watcher.expired() ? u8"是" : u8"否")
       << u8"，lock() 得到" << (watcher.lock() ? u8"非空指针" : u8"空指针") << "\n";
    os << u8"  强引用互指：离开作用域后仍活着的节点数 " << strong_cycle_live_after_scope() << "\n";
    os << u8"  靠 weak_ptr 找到节点再断开环：存活数 " << strong_cycle_live_after_break() << "\n";
    const WeakCycleOutcome weak = weak_cycle_run();
    os << u8"  父子改成 weak_ptr 连法：离开作用域后存活数 " << weak.live_after_scope
       << u8"，弱引用过期 " << (weak.weak_expired ? u8"是" : u8"否") << "\n";

    /* ---------- 回调注册表 ---------- */
    os << u8"\n回调注册表（std::function）\n";
    CallbackRegistry table;
    AuditLog audit;
    std::size_t counter = 0;
    const std::string prefix = u8"欢迎：";
    table.add("audit", std::bind(&AuditLog::record, &audit, std::placeholders::_1));
    table.add("banner", [prefix](const std::string &event) { return prefix + event; });
    table.add("counter", [&counter](const std::string &event) {
        ++counter;
        return u8"累计 " + std::to_string(counter) + u8" 次：" + event;
    });
    const std::vector<std::string> names = table.names();
    os << u8"  注册 " << table.size() << u8" 个回调：";
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != 0) {
            os << u8"、";
        }
        os << names[i];
    }
    os << "\n";

    std::string out;
    table.trigger("banner", u8"资源加载完成", out);
    os << u8"  触发 banner → " << out << "\n";
    table.trigger("counter", u8"加载完成", out);
    os << u8"  触发 counter → " << out << "\n";
    table.trigger("audit", u8"加载完成", out);
    os << u8"  触发 audit → " << out << "\n";
    table.trigger("banner", u8"第二次", out);
    os << u8"  再触发 banner → " << out << "\n";
    os << u8"  各回调的调用次数：audit " << table.calls("audit") << u8"、banner "
       << table.calls("banner") << u8"、counter " << table.calls("counter") << "\n";

    os << u8"  注销 counter：" << (table.remove("counter") ? u8"成功" : u8"失败") << "\n";
    const bool triggered = table.trigger("counter", u8"注销之后", out);
    os << u8"  注销后再触发 counter：" << (triggered ? u8"仍然成功" : u8"找不到这个名字")
       << u8"，lambda 里的计数器停在 " << counter << "\n";
    return os.str();
}

CheckResult run_self_tests()
{
    Checker c;

    /* 1. 工厂返回派生对象，指针类型是基类 */
    {
        const std::unique_ptr<Resource> texture = make_resource("texture", u8"草地");
        c.check(texture != nullptr && texture->kind() == "texture"
                    && dynamic_cast<Texture *>(texture.get()) != nullptr,
                u8"工厂用 make_unique 造 Texture，以基类指针返回",
                describe_or_empty(texture));
    }

    /* 2. 认不出的种类给空指针，而不是随便造一个 */
    {
        const std::unique_ptr<Resource> audio = make_resource("audio", u8"脚步");
        const std::unique_ptr<Resource> unknown = make_resource("model", u8"水面");
        c.check(audio != nullptr && audio->kind() == "audio" && unknown == nullptr,
                u8"认得出的只有 texture 与 audio，其余返回空指针",
                describe_or_empty(audio));
    }

    /* 3. unique_ptr 负责析构：活着时对象在，离开作用域就没了 */
    const std::size_t resource_baseline = Resource::live_count();
    bool alive_inside = false;
    {
        const std::unique_ptr<Resource> one = make_resource("texture", u8"临时");
        alive_inside = Resource::live_count() == resource_baseline + 1;
    }
    c.check(alive_inside && Resource::live_count() == resource_baseline,
            u8"unique_ptr 离开作用域就析构对象，不用手写 delete",
            u8"存活数 " + std::to_string(Resource::live_count()));

    /* 4. 经基类指针调用虚函数 */
    {
        const std::unique_ptr<Resource> poly = make_resource("audio", u8"脚步");
        const std::string text = poly->describe();
        c.check(text.compare(0, 5, "audio") == 0 && text.find(u8"脚步") != std::string::npos,
                u8"经基类指针调用虚函数，得到派生类的说明", text);
    }

    /* 5. 一批资源装进 vector，容器析构后一个不剩 */
    const std::size_t before_batch = Resource::live_count();
    std::size_t batch_size = 0;
    {
        const std::vector<std::unique_ptr<Resource>> batch = make_default_batch();
        batch_size = batch.size();
    }
    c.check(batch_size == 3 && Resource::live_count() == before_batch,
            u8"vector<unique_ptr<Resource>> 析构时逐个释放",
            std::to_string(batch_size) + u8" 个资源");

    /* 6. 移动之后所有权只有一份 */
    {
        std::unique_ptr<Resource> owner = make_resource("texture", u8"移动");
        const std::unique_ptr<Resource> taken = std::move(owner);
        c.check(owner == nullptr && taken != nullptr && taken->name() == u8"移动",
                u8"unique_ptr 移动之后源指针为空，所有权不重复");
    }

    /* 7 至 9. 共享所有权与引用计数 */
    const std::size_t before_shared = Resource::live_count();
    long count_one = 0;
    long count_three = 0;
    long count_one_again = 0;
    {
        const std::shared_ptr<Resource> first = std::make_shared<Texture>(u8"共享", 128, 128);
        count_one = first.use_count();
        std::shared_ptr<Resource> second = first;
        std::shared_ptr<Resource> third = second;
        count_three = third.use_count();
        third.reset();
        second.reset();
        count_one_again = first.use_count();
    }
    c.check(count_one == 1 && count_three == 3,
            u8"拷贝 shared_ptr 把 use_count 从 1 涨到 3",
            std::to_string(count_one) + u8" → " + std::to_string(count_three));
    c.check(count_one_again == 1,
            u8"逐份释放把 use_count 从 3 降回 1",
            u8"释放两份后 use_count = " + std::to_string(count_one_again));
    c.check(Resource::live_count() == before_shared,
            u8"最后一份释放后对象才析构",
            u8"存活数 " + std::to_string(Resource::live_count()));

    /* 10. unique_ptr 转移成 shared_ptr */
    {
        std::unique_ptr<Resource> unique_owner = make_resource("audio", u8"转移");
        const std::shared_ptr<Resource> shared_owner = std::move(unique_owner);
        c.check(unique_owner == nullptr && shared_owner != nullptr
                    && shared_owner.use_count() == 1,
                u8"unique_ptr 转移给 shared_ptr 之后，那一份所有权仍然只有一份",
                u8"use_count = " + std::to_string(shared_owner.use_count()));
    }

    /* 11 至 12. weak_ptr 观察 */
    std::weak_ptr<Resource> watcher;
    {
        const std::shared_ptr<Resource> alive = std::make_shared<Texture>(u8"观察", 64, 64);
        watcher = alive;
        const bool not_counted = alive.use_count() == 1 && watcher.use_count() == 1;
        const std::shared_ptr<Resource> promoted = watcher.lock();
        c.check(not_counted && promoted != nullptr && alive.use_count() == 2
                    && !watcher.expired(),
                u8"weak_ptr 不增加引用计数，lock() 提升之后才临时加一",
                u8"use_count " + std::to_string(alive.use_count()));
    }
    c.check(watcher.expired() && watcher.lock() == nullptr,
            u8"对象销毁后弱引用过期，lock() 得到空指针");

    /* 13. 强引用互指：对象数不为 0 */
    const std::size_t leaked = strong_cycle_live_after_scope();
    c.check(leaked == 2,
            u8"强引用互指后对象数不为 0",
            u8"离开作用域后仍活着 " + std::to_string(leaked) + u8" 个节点");

    /* 14. 断开环之后才释放 */
    const std::size_t after_break = strong_cycle_live_after_break();
    c.check(after_break == 0,
            u8"靠 weak_ptr 找到节点、断开环之后存活数归零",
            std::to_string(after_break) + u8" 个节点");

    /* 15. weak_ptr 版本：对象数归零 */
    const WeakCycleOutcome weak = weak_cycle_run();
    c.check(weak.live_after_scope == 0 && weak.weak_expired && weak.parent_reachable,
            u8"weak_ptr 版本对象数归零",
            u8"存活 " + std::to_string(weak.live_after_scope) + u8" 个节点，弱引用过期 "
                + (weak.weak_expired ? u8"是" : u8"否"));

    /* 16. 注册、触发、重名拒绝 */
    CallbackRegistry table;
    std::size_t hits = 0;
    const std::string prefix = u8"回调：";
    const bool added_banner = table.add("banner",
        [prefix](const std::string &event) { return prefix + event; });
    const bool added_twice = table.add("banner",
        [](const std::string &) { return std::string(u8"第二个"); });
    const bool added_counter = table.add("counter",
        [&hits](const std::string &event) { ++hits; return event; });
    std::string out;
    const bool triggered = table.trigger("banner", u8"加载完成", out);
    c.check(added_banner && added_counter && !added_twice && triggered
                && out == u8"回调：加载完成" && table.calls("banner") == 1
                && table.has("counter") && table.size() == 2,
            u8"按名字注册并触发，返回文本与调用次数都对", out);

    /* 17. 注销之后再触发不计数 */
    (void)table.trigger("counter", u8"第一次", out);
    (void)table.trigger("counter", u8"第二次", out);
    const std::size_t hits_before_remove = hits;
    const bool removed = table.remove("counter");
    const bool removed_again = table.remove("counter");
    const std::size_t calls_after_remove = table.calls("counter");
    const bool triggered_after_remove = table.trigger("counter", u8"第三次", out);
    c.check(removed && !removed_again && !triggered_after_remove
                && hits == hits_before_remove && calls_after_remove == 0,
            u8"回调注销后再触发不计数",
            u8"计数器停在 " + std::to_string(hits) + u8"，再触发返回"
                + (triggered_after_remove ? u8"成功" : u8"找不到这个名字"));

    /* 18. 成员函数绑定与 lambda 捕获 */
    CallbackRegistry bound;
    AuditLog log;
    const std::string tag = u8"lambda 捕获：";
    const bool added_member = bound.add("audit",
        std::bind(&AuditLog::record, &log, std::placeholders::_1));
    const bool added_lambda = bound.add("tagged",
        [tag](const std::string &event) { return tag + event; });
    std::string member_out;
    std::string lambda_out;
    const bool member_ok = bound.trigger("audit", u8"加载完成", member_out);
    const bool lambda_ok = bound.trigger("tagged", u8"加载完成", lambda_out);
    c.check(added_member && added_lambda && member_ok && lambda_ok && log.lines() == 1
                && member_out.find(u8"第 1 条") != std::string::npos
                && lambda_out == u8"lambda 捕获：加载完成",
            u8"成员函数绑定与 lambda 捕获都能注册并触发",
            member_out + u8" / " + lambda_out);

    return c.take();
}

}   /* namespace registry */
