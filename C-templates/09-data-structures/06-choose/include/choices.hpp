/* choices.hpp —— 练习模板 06 的选型表接口（C++）
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
 * 这个模板只做一件事：六段需求摆在面前，你自己决定**该用哪个结构、代价在哪**。
 *
 *     阶段 1  填需求 1、2 的「结构与代价」
 *     阶段 2  填需求 3、4 的「结构与代价」
 *     阶段 3  填需求 5、6 的「结构与代价」
 *     阶段 4  补上「凭什么判断」与「依据哪一节」，并把整张表打出来
 *
 * 题面（六段需求）已经给出，不要改；要填的是 src/choices.cpp 里的 kChoices。
 * 验收标准与自查表见同目录《配置步骤.md》。
 */
#ifndef CHOICES_HPP
#define CHOICES_HPP

namespace dsc {

/* 一条需求：一段场景，加上「这一段里最常做的那件事」 */
struct Need {
    int id;
    const char *scene;
    const char *frequent;
};

/* 六条需求：已经给出，改了就与《配置步骤.md》里的自查表对不上 */
extern const Need kNeeds[6];
extern const int kNeedCount;

/* 你要填的一行，四栏 */
struct Choice {
    const char *structure;  /* 该用哪个结构（阶段 1、2、3） */
    const char *cost;       /* 代价落在哪（阶段 1、2、3） */
    const char *reason;     /* 凭什么判断：这一段里最常做的那件事是什么（阶段 4） */
    const char *source;     /* 依据哪一节（阶段 4） */
};

extern Choice kChoices[6];

/* 已给出：把一条需求与它对应的选择打印出来 */
void print_row(int index);

/* 已给出：机械自查。只看「填了没有」与「依据字段的格式对不对」，
 * 不判断你选得对不对——选得对不对由《配置步骤.md》里的自查表管。 */
struct Report {
    int structure_filled;
    int cost_filled;
    int reason_filled;
    int source_filled;
    int sources_ok;         /* 依据字段写成《…》第 N 节的样子 */
    int rows_complete;      /* 四栏都填了的行数 */
};

Report check_table(void);

} /* namespace dsc */

#endif /* CHOICES_HPP */
