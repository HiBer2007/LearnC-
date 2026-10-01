/**
 * exporter.hpp —— 抽象基类与工厂
 *
 * 对应教材：《05-类与面向对象/06-继承.md》第 1、3、7 节
 *           《05-类与面向对象/07-多态：重载、虚函数与它们的分工.md》
 *               第 3、4、5 节
 *
 * 设计要点：
 *   · Exporter 是抽象基类：name / hint / render 三个纯虚函数，
 *     想新增一种格式，只要再写一个派生类，不用改调用方的任何代码。
 *   · 析构函数写成虚的：调用方持有的是基类指针，
 *     删除时必须先跑派生类的析构函数。
 *   · render_checked 是非虚函数，内部调用虚函数 render，
 *     这样「先检查表格」这件公共的事只写一遍，
 *     派生类只关心自己那种格式怎么写。
 *   · 工厂 make_exporter 是唯一的创建入口，
 *     调用方不必知道到底有哪几个派生类。
 */
#ifndef EXPORTER_HPP
#define EXPORTER_HPP

#include "table.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class Exporter {
public:
    Exporter();
    virtual ~Exporter();                    /* 虚析构：经基类指针删除才安全 */

    Exporter(const Exporter &) = delete;            /* 导出器无状态，不必拷贝 */
    Exporter &operator=(const Exporter &) = delete;

    /** 格式名，例如 "csv"。工厂认它，界面也显示它 */
    virtual std::string name() const = 0;

    /** 一句话说明这种格式的特点，界面上显示 */
    virtual std::string hint() const = 0;

    /** 把表格渲染成文本。纯虚：基类不知道该怎么写 */
    virtual std::string render(const Table &table) const = 0;

    /** 非虚：先检查表格，再交给上面的虚函数。公共步骤只写一遍 */
    std::string render_checked(const Table &table) const;

    /** 当前活着的导出器对象个数，用来验证虚析构确实释放了派生部分 */
    static std::size_t live_count() { return live_count_; }

private:
    static std::size_t live_count_;
};

/** 工厂：按名字造导出器，不认识的名字返回空指针 */
std::unique_ptr<Exporter> make_exporter(const std::string &format);

/** 支持的格式名，顺序与界面上的排列一致 */
std::vector<std::string> supported_formats();

/** 派生类里那个生存期探针的当前计数。经基类指针删除后应当归零 */
int exporter_tag_live_count();

#endif /* EXPORTER_HPP */
