/* choices.cpp —— 练习模板 06 的题面与选型表（C++）
 *
 * 版权所有 (C) 2026 HiBer2007，保留所有权利。
 *
 * 本程序是《C 与 C++》教材的一部分，采用与教材文档相同的授权：
 * CC BY-NC-ND 4.0 加附加条款。全文见仓库根目录的 LICENSE 与 许可附加条款.md。
 *
 * 允许在保留本声明的前提下查看、编译、运行本程序用于学习；
 * 不允许二次分发，不允许商用，不允许演绎（修改后再分发），不允许移除署名。
 *
 * 本程序不提供任何担保。
 *
 * ------------------------------------------------------------------
 * 六段需求（kNeeds）已经给出，不要改。
 * 你要填的是 kChoices：每条需求四项，按阶段分四次填完。
 *
 * 填完之后应当看到：
 *   - 阶段 1：structure filled = 2 of 6、cost filled = 2 of 6；
 *   - 阶段 2：两个数都到 4；
 *   - 阶段 3：两个数都到 6，六行都不再有 (not filled)；
 *   - 阶段 4：reason filled = 6、source filled = 6、sources in the right form = 6、
 *             rows complete = 6 of 6。
 *
 * 答案写中文或英文都可以：这一份是给你自己看的表，说清理由比用词漂亮重要。
 */
#include "choices.hpp"

#include <cstddef>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

namespace dsc {

const int kNeedCount = 6;

const Need kNeeds[6] = {
    {1,
     "一帧里几万个物体，渲染时按编号取第 i 个；每帧末尾追加几个新物体，几乎不在中间插入。",
     "按下标随机访问，追加在末尾"},
    {2,
     "一份任务队列，任务的位置在别处已经拿到；频繁在这些位置插入与删除，很少按下标取。",
     "位置已知的中间插入与删除"},
    {3,
     "一张用户表，按用户名查资料；从不需要按名字排序，也不需要范围查询；用户名不断增删。",
     "按值查找，不需要有序"},
    {4,
     "一份成绩单，既要按学号查分，又要按分数区间取出一段、按顺序遍历前一百名。",
     "按值查找，而且要按顺序取、要范围查询"},
    {5,
     "网络连通性：不断把两台机器并到同一网段，随时问两个 IP 通不通；只加不删。",
     "只问连通性，边只加不删"},
    {6,
     "一张稀疏的路线图（十万个点、几十万条边），算法反复问「从这一点出发能到哪些点」。",
     "反复取一个点的所有邻居"},
};

/* ==================================================================
 * 你要填的表
 * ================================================================== */

/* 四栏的含义：
 *     structure  该用哪个结构（写结构名，例如「连续存储」或容器名）
 *     cost       代价落在哪：最常做的那件事有多贵，别的操作又贵在哪
 *     reason     凭什么判断：这一段的「最常做的那件事」是什么，它推出什么形状
 *     source     依据哪一节，写成《09-高阶数据结构/xxx.md》第 N 节的样子
 *
 * 每栏都写成一句话。nullptr 表示还没填（程序会打印 (not filled)）。
 *
 * ------------------------------------------------------------------
 * TODO（阶段 1-1）：填**需求 1 与需求 2** 的 structure 与 cost 两栏。
 *   reason 与 source 留到阶段 4。
 *
 * TODO（阶段 2-1）：填**需求 3 与需求 4** 的 structure 与 cost 两栏。
 *
 * TODO（阶段 3-1）：填**需求 5 与需求 6** 的 structure 与 cost 两栏。
 *
 * TODO（阶段 4-1）：把六行的 reason 与 source 补齐。
 *   source 必须是《…》第 N 节的样子（程序会检查这一条的格式，
 *   格式不对时 sources in the right form 会小于 6）。
 *
 * 提醒：这一栏栏写的不是「哪个结构更高级」，而是
 * 「这一段里最常做的那件事，决定了哪种形状最划算」。
 * 结构只有配上需求才有优劣：同一个结构换一种问法，结论就可能反过来。
 * ------------------------------------------------------------------ */
Choice kChoices[6] = {
    /* 需求 1 */ {nullptr, nullptr, nullptr, nullptr},
    /* 需求 2 */ {nullptr, nullptr, nullptr, nullptr},
    /* 需求 3 */ {nullptr, nullptr, nullptr, nullptr},
    /* 需求 4 */ {nullptr, nullptr, nullptr, nullptr},
    /* 需求 5 */ {nullptr, nullptr, nullptr, nullptr},
    /* 需求 6 */ {nullptr, nullptr, nullptr, nullptr},
};

/* ==================================================================
 * 已给出：打印与机械自查，不需要改
 * ================================================================== */

namespace {

const char *or_placeholder(const char *text)
{
    return (text != nullptr && text[0] != '\0') ? text : "(not filled)";
}

bool is_filled(const char *text)
{
    return text != nullptr && text[0] != '\0';
}

/* 依据字段的样子：《…》第 N 节 */
bool source_looks_right(const char *text)
{
    if (!is_filled(text)) {
        return false;
    }
    const std::string source(text);
    if (source.rfind("《", 0) != 0) {
        return false;
    }
    if (source.find("》第") == std::string::npos) {
        return false;
    }
    return source.size() >= 3 && source.compare(source.size() - 3, 3, "节") == 0;
}

void field(const char *label, const char *text)
{
    std::cout << "  " << std::left << std::setw(12) << label << ": " << or_placeholder(text) << "\n";
}

} /* namespace */

void print_row(int index)
{
    if (index < 0 || index >= kNeedCount) {
        return;
    }
    const Need &need = kNeeds[index];
    const Choice &choice = kChoices[index];

    std::cout << "need " << need.id << "  " << need.scene << "\n";
    field("most often", need.frequent);
    field("structure", choice.structure);
    field("cost", choice.cost);
    field("reason", choice.reason);
    field("source", choice.source);
    std::cout << "\n";
}

Report check_table(void)
{
    Report report{0, 0, 0, 0, 0, 0};
    for (int i = 0; i < kNeedCount; ++i) {
        const Choice &choice = kChoices[i];
        const bool has_structure = is_filled(choice.structure);
        const bool has_cost = is_filled(choice.cost);
        const bool has_reason = is_filled(choice.reason);
        const bool has_source = is_filled(choice.source);

        if (has_structure) {
            ++report.structure_filled;
        }
        if (has_cost) {
            ++report.cost_filled;
        }
        if (has_reason) {
            ++report.reason_filled;
        }
        if (has_source) {
            ++report.source_filled;
        }
        if (source_looks_right(choice.source)) {
            ++report.sources_ok;
        }
        if (has_structure && has_cost && has_reason && has_source) {
            ++report.rows_complete;
        }
    }
    return report;
}

} /* namespace dsc */
