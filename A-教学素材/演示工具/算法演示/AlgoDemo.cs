// 算法演示 —— 九个算法场景的交互式可视化（本教材配套程序）
// 版权所有 (C) 2026 HiBer2007，保留所有权利。
//
// 本程序是《C 与 C++》教材的一部分，采用与教材文档相同的授权：
// CC BY-NC-ND 4.0 加附加条款。全文见仓库根目录的 LICENSE 与 许可附加条款.md。
//
// 允许在保留本声明的前提下查看、编译、运行本程序用于学习；
// 不允许二次分发，不允许商用，不允许演绎（修改后再分发），不允许移除署名。
//
// 本程序不提供任何担保。

/* ============================================================================
 *  AlgoDemo.cs —— 算法演示：一个程序 + 一个算法选择器，覆盖九个场景
 *
 *  1  递归调用树          调用怎么一层层长出来、在哪一层触底、栈帧深度随参数怎么变、
 *                         尾递归开／关优化的对照
 *  2  记忆化·分治·回溯     同一棵递归树上：重复子问题被标出来（记忆化前 vs 后访问次数）、
 *                         分治的合并发生在哪一层、回溯的选择与撤销
 *  3  排序对照            四种比较排序并排跑同一份数据：比较次数、交换次数、
 *                         已就位区域的推进；稳定性用带标记的相等元素看得见
 *  4  查找对照            线性／二分／哈希在同一批数据上的步数推进；二分区间怎么收缩
 *  5  动态规划            状态表一格一格填，高亮当前格的转移来源；滚动数组前后对照
 *  6  字符串匹配          朴素匹配的回溯 vs KMP 失配函数的滑动；失配表本身也画出来
 *  7  状态机              当前状态、走过的转移、输入串读到哪一位；表驱动与 switch 对照
 *  8  随机与采样          rand()%n 的取模偏差用直方图看出来；洗牌的均匀性、
 *                         蓄水池抽样的入选概率
 *  9  图上的结构性问题     拓扑排序的层次、割点去掉后图散开、最小生成树选中的边
 *
 *  **搜索与最短路不在本演示里**：那四个算法（BFS／DFS／Dijkstra／A*）已经有
 *  `A-教学素材/演示工具/搜索演示/`，本演示不重做。
 *
 *  编译（两种都实测通过，见 README）：
 *    csc.exe /nologo /target:winexe /out:AlgoDemo.exe /r:System.Windows.Forms.dll ^
 *      /r:System.Drawing.dll ..\_common\VisualCore.cs AlgoDemo.cs
 *    dotnet build -c Release
 *
 *  语法：C# 5。.NET Framework 自带的 csc.exe 只支持到 C# 5，
 *        因此这里不用字符串内插、空条件运算符、表达式体成员这些写法。
 *
 *  ── 三条自检 ──────────────────────────────────────────────────────────
 *    --selftest      不开窗、同进程，九个场景各给一组可复现的结构量
 *    --palettecheck  两套主题逐控件算对比度，低于 4.5:1 非零退出（核心提供）
 *    --layoutcheck   文字放不下为 0，且连线每一段只许横或只许竖（本演示实现 IPathInspector）
 *
 *  ── 与核心的两条约定 ──────────────────────────────────────────────────
 *  一、**九个场景共用同一份元素数组**（384 个）。核心的 Snapshot 与共享内存按
 *      Scene.ElementCount 一次定长（见 VisualFormBase 的构造函数），中途换不来长度。
 *      九个场景不会同时出现，因此结构区是所有场景共用的：自检逐个场景量它用到的
 *      最大下标，断言都不越过 256。元素 383 留作命令邮箱，画布上永远不画它。
 *  二、**界面进程的改动只能通过共享内存的场景区传给运算进程**。
 *      WriteScene 写的是 Marker／Tag／Attr0..5／Seed，因此邮箱里
 *      Marker 是命令序号，Attr0..2 是命令码与两个参数，运算进程按序号去重。
 * ==========================================================================*/

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Text;
using System.Windows.Forms;
using VisualCore;

namespace AlgoDemo
{
    // ───────────────────────────────────────────────────────────── 常量

    internal static class AlgoConst
    {
        /// <summary>元素总数。九个场景共用这一份，长度不能再变（见文件头）。</summary>
        public const int Elements = 384;

        /// <summary>命令邮箱用的元素下标，画布上永远不画它。</summary>
        public const int Mailbox = 383;

        /// <summary>结构区的上界：每个场景用到的最大下标都要落在它以内（自检逐场景断言）。</summary>
        public const int StructLimit = 256;

        /// <summary>输入区起点：场景数据（待排序的数、文本串、边表、输入串）。</summary>
        public const int InputBase = 256;

        public const int DefaultSeed = 20261002;

        public const int CanvasWidth = 940;
        public const int CanvasHeight = 620;

        /// <summary>画布顶部标题行的基线，正文从 TitleBottom 往下画。</summary>
        public const int TitleTop = 6;
        public const int TitleBottom = 30;

        /// <summary>刷子值（键值）的取值范围。</summary>
        public const int MinValue = 1;
        public const int MaxValue = 99;

        /// <summary>线宽（像素）。数量过，不是凭感觉写的，见 README 的「线宽」一节。</summary>
        public const float HaloWidth = 3.0f;
        public const float MainWidth = 1.6f;

        // ── 字号（像素）。**按像素定，不凭感觉**：
        //    13 像素是格上数字与键值那一档，11 像素是次要一行那一档，
        //    11 像素同时也是本演示的最小字号，--layoutcheck 按它断言。

        /// <summary>格子上的数字、键值、顶点名：13 像素粗体。</summary>
        public const float CellFontPx = 13f;

        /// <summary>次要那一行（下标、来源、计数）：11 像素粗体。</summary>
        public const float TinyFontPx = 11f;

        /// <summary>本演示的最小字号。低于它 --layoutcheck 以非零退出码结束。</summary>
        public const float MinFontPx = 11f;

        /// <summary>一行字至少要这么高才排得下（13 像素字加一点余量）。</summary>
        public const int OneLineHeight = 20;

        /// <summary>两行字至少要这么高（13 + 11 像素两档加行距）。</summary>
        public const int TwoLineHeight = 36;
    }

    // ───────────────────────────────────────────────────────────── 状态码

    /// <summary>
    /// 全局语义状态码。**每个码在九个场景里含义一致**，因为图例的调色板
    /// 在建窗口时绑定一次（LegendPanel 的 palette 是只读字段），
    /// 九个场景只能共用一套色表，因此不能一个码两种意思。
    /// </summary>
    internal static class St
    {
        public const byte Empty = ElementStates.Empty;       // 0 空位、未使用
        public const byte Pending = ElementStates.Pending;   // 1 待处理（在候选里）
        public const byte Done = ElementStates.Done;         // 2 已处理、已确定
        public const byte Result = ElementStates.Result;     // 3 最终结果、命中
        public const byte Active = ElementStates.Active;     // 4 当前
        public const byte Marked = ElementStates.Marked;     // 5 刚变更
        public const byte Blocked = ElementStates.Blocked;   // 6 已失效、不可用
        public const byte Anchor = 7;                        // 7 结构锚点（根、起点）
        public const byte Data = 8;                          // 8 数据单元
        public const byte Path = 9;                          // 9 路径、转移来源
        public const byte Ref = 10;                          // 10 参照元素
        public const byte Cut = 11;                          // 11 已跳过、旁路

        /// <summary>命令码：界面进程写进邮箱，运算进程照着做。</summary>
        public const int CmdNone = 0;
        public const int CmdSelect = 1;      // 选中一个元素，状态行报出它的信息
        public const int CmdSetValue = 2;    // 把刷子值写到这个元素上
        public const int CmdToggle = 3;      // 翻转这个元素（图上的边、字符串里的字符）
        public const int CmdClear = 4;       // 清空这一轮的场景数据
        public const int CmdRegenerate = 5;  // 按当前种子重新生成
        public const int CmdUndo = 6;        // 撤销上一次落笔
    }

    /// <summary>九个场景的名字、选项名与元素上标名。下拉里的字都在这里。</summary>
    internal static class AlgoNames
    {
        public static readonly string[] Modes =
        {
            "1 递归调用树", "2 记忆化·分治·回溯", "3 排序对照", "4 查找对照", "5 动态规划",
            "6 字符串匹配", "7 状态机", "8 随机与采样", "9 图上的结构性问题"
        };

        /// <summary>选项一：每个场景三项，含义各不相同。</summary>
        public static readonly string[][] Options =
        {
            new string[] { "线性递归 sum(5)", "斐波那契 fib(6)", "尾递归 fact(5, 1)" },
            new string[] { "记忆化：fib(6)", "分治：归并排序", "回溯：子集与排列" },
            new string[] { "初始次序：随机", "初始次序：近乎有序", "初始次序：逆序" },
            new string[] { "目标：中间那个值", "目标：不存在的值", "目标：第一个值" },
            new string[] { "最长公共子序列 LCS", "编辑距离" },
            new string[] { "文本：含匹配", "文本：大量失配", "文本：周期串" },
            new string[] { "输入：两种注释都有", "输入：字符串里的斜杠", "输入：块注释未闭合" },
            new string[] { "rand()%n 的取模偏差", "洗牌：两种算法", "蓄水池抽样" },
            new string[] { "拓扑排序的层次", "割点：去掉后图散开", "最小生成树" }
        };

        /// <summary>选项二：按（场景，选项一）给，因此换选项一的时候它也跟着换。</summary>
        public static readonly string[][][] Options2 =
        {
            new string[][]
            {
                new string[] { "尾调用优化 关", "尾调用优化 开" },
                new string[] { "尾调用优化 关", "尾调用优化 开" },
                new string[] { "尾调用优化 关", "尾调用优化 开" }
            },
            new string[][]
            {
                new string[] { "记忆化 关", "记忆化 开" },
                new string[] { "8 个元素", "16 个元素" },
                new string[] { "子集（4 个元素）", "全排列（3 个元素）" }
            },
            new string[][]
            {
                new string[] { "互不相同（看代价）", "含相等元素（看稳定性）" },
                new string[] { "互不相同（看代价）", "含相等元素（看稳定性）" },
                new string[] { "互不相同（看代价）", "含相等元素（看稳定性）" }
            },
            new string[][]
            {
                new string[] { "16 个键（互不相同）", "16 个键（含重复）" },
                new string[] { "16 个键（互不相同）", "16 个键（含重复）" },
                new string[] { "16 个键（互不相同）", "16 个键（含重复）" }
            },
            new string[][]
            {
                new string[] { "完整二维表", "滚动数组（只留两行）" },
                new string[] { "完整二维表", "滚动数组（只留两行）" }
            },
            new string[][]
            {
                new string[] { "朴素与 KMP 同时跑", "只看朴素", "只看 KMP" },
                new string[] { "朴素与 KMP 同时跑", "只看朴素", "只看 KMP" },
                new string[] { "朴素与 KMP 同时跑", "只看朴素", "只看 KMP" }
            },
            new string[][]
            {
                new string[] { "switch 与表驱动对照", "只看 switch", "只看表驱动" },
                new string[] { "switch 与表驱动对照", "只看 switch", "只看表驱动" },
                new string[] { "switch 与表驱动对照", "只看 switch", "只看表驱动" }
            },
            new string[][]
            {
                new string[] { "小样本", "大样本" },
                new string[] { "24 轮", "120 轮" },
                new string[] { "40 轮", "200 轮" }
            },
            new string[][]
            {
                new string[] { "边集 A（稀疏）", "边集 B（稠密）" },
                new string[] { "边集 A（稀疏）", "边集 B（稠密）" },
                new string[] { "边集 A（稀疏）", "边集 B（稠密）" }
            }
        };

        public static readonly string[] DisplayNames =
        {
            "主数值", "下标／编号", "次序／计数", "不写"
        };

        public static readonly string[] StatNames =
        {
            "已推进步数", "本场景工作量", "待处理／候选", "主指标", "次指标",
            "实测推进速率", "两种模式对照"
        };

        public static int OptionCount(int mode)
        {
            if (mode < 0 || mode >= Options.Length) return 1;
            return Options[mode].Length;
        }

        public static int Option2Count(int mode, int optionA)
        {
            if (mode < 0 || mode >= Options2.Length) return 1;
            string[][] table = Options2[mode];
            if (optionA < 0 || optionA >= table.Length) return table[0].Length;
            return table[optionA].Length;
        }

        public static string OptionName(int mode, int optionA)
        {
            if (mode < 0 || mode >= Options.Length) return "";
            string[] names = Options[mode];
            if (optionA < 0 || optionA >= names.Length) optionA = 0;
            return names[optionA];
        }

        public static string Option2Name(int mode, int optionA, int optionB)
        {
            if (mode < 0 || mode >= Options2.Length) return "";
            string[][] table = Options2[mode];
            if (optionA < 0 || optionA >= table.Length) optionA = 0;
            string[] names = table[optionA];
            if (optionB < 0 || optionB >= names.Length) optionB = 0;
            return names[optionB];
        }
    }

    // ───────────────────────────────────────────────────────────── 色表

    /// <summary>
    /// 语义色表。场景只按状态码取色，色值由主题给；这里给出图例的条目。
    /// 十二个状态码正好把十二个色槽用完，因此每个码在屏幕上都有自己的一种填充色，
    /// 不必靠形状或纹理区分。
    /// </summary>
    internal static class AlgoPalette
    {
        public static ScenePalette Build()
        {
            ScenePalette p = new ScenePalette();
            p.AddState(St.Empty, "空位（未使用）", ThemeSlot.Empty);
            p.AddState(St.Pending, "待处理（在候选里）", ThemeSlot.Pending);
            p.AddState(St.Done, "已处理／已确定", ThemeSlot.Done);
            p.AddState(St.Result, "最终结果／命中", ThemeSlot.Result);
            p.AddState(St.Active, "当前", ThemeSlot.Active);
            p.AddState(St.Marked, "刚变更", ThemeSlot.Extra2);
            p.AddState(St.Blocked, "已失效／不可用", ThemeSlot.Blocked);
            p.AddState(St.Anchor, "结构锚点（根／起点）", ThemeSlot.Start);
            p.AddState(St.Data, "数据单元", ThemeSlot.Terrain);
            p.AddState(St.Path, "路径／转移来源", ThemeSlot.Goal);
            p.AddState(St.Ref, "参照元素", ThemeSlot.Extra1);
            p.AddState(St.Cut, "已跳过／旁路", ThemeSlot.Extra3);
            return p;
        }

        public static Color Fill(byte state) { return ThemeManager.Palette.FillOf(Slot(state)); }
        public static Color Text(byte state) { return ThemeManager.Palette.TextOf(Slot(state)); }

        /// <summary>图例上的名字。自检打印对比度表时也用它，两边不会各写一套。</summary>
        public static string LegendName(byte state)
        {
            switch (state)
            {
                case St.Empty: return "空位（未使用）";
                case St.Pending: return "待处理";
                case St.Done: return "已处理／已确定";
                case St.Result: return "最终结果／命中";
                case St.Active: return "当前";
                case St.Marked: return "刚变更";
                case St.Blocked: return "已失效／不可用";
                case St.Anchor: return "结构锚点";
                case St.Data: return "数据单元";
                case St.Path: return "路径／转移来源";
                case St.Ref: return "参照元素";
                case St.Cut: return "已跳过／旁路";
                default: return "未知";
            }
        }

        public static ThemeSlot Slot(byte state)
        {
            switch (state)
            {
                case St.Empty: return ThemeSlot.Empty;
                case St.Pending: return ThemeSlot.Pending;
                case St.Done: return ThemeSlot.Done;
                case St.Result: return ThemeSlot.Result;
                case St.Active: return ThemeSlot.Active;
                case St.Marked: return ThemeSlot.Extra2;
                case St.Blocked: return ThemeSlot.Blocked;
                case St.Anchor: return ThemeSlot.Start;
                case St.Data: return ThemeSlot.Terrain;
                case St.Path: return ThemeSlot.Goal;
                case St.Ref: return ThemeSlot.Extra1;
                case St.Cut: return ThemeSlot.Extra3;
                default: return ThemeSlot.Empty;
            }
        }

        /// <summary>
        /// 格子边框的颜色。**不能用核心的 GridLine**：它与画布底色的对比度只有
        /// 1.71（浅色 `#BDBDBD` 配 `#F4F4F4`）／1.56（深色 `#3C3C3C` 配 `#1B1B1B`），
        /// 低于图形元素该有的 3:1——`--palettecheck` 把画布算进去之后就把它抓出来了。
        /// 这里按主题给一个中性灰，两套都在 3:1 以上。
        ///
        /// 边框只按**与画布底色**的对比度判：格子彼此之间还靠填充色区分，
        /// 而填充色的明度跨度很大（浅色下有 `#FFFFFF` 也有 `#6A1B9A`），
        /// 一个颜色不可能同时与十二种填充色都拉开 3:1。
        /// </summary>
        public static Color CellBorder()
        {
            return ThemeManager.Current == ThemeMode.Dark
                ? ThemePalette.ParseHex("#707070")
                : ThemePalette.ParseHex("#8A8A8A");
        }

        /// <summary>普通连线：主色取自「最终结果」槽，与核心的折线主色同色。</summary>
        public static Color LineMain() { return ThemeManager.Palette.FillOf(ThemeSlot.Result); }

        /// <summary>
        /// 连线的描边色。**不能拿画布底色当描边**：深色主题下核心给的描边色
        /// （`#1B1B1B`）正好等于画布底色，实画出来整条线只剩 1 像素
        /// （截图逐像素量过），白线也就没有边。这里深色改用比画布更暗的那一档
        /// 填充色（`#0B0E12`），浅色仍用核心给的中灰。
        /// </summary>
        public static Color LineHalo()
        {
            return ThemeManager.Current == ThemeMode.Dark
                ? ThemeManager.Palette.FillOf(ThemeSlot.Blocked)
                : ThemeManager.Palette.PathHalo;
        }

        /// <summary>
        /// 高亮连线（当前转移、当前选中）：取该主题下与画布底色对比度更高的那一支——
        /// 浅色主题用「当前」槽的字色（近黑），深色主题用备用槽的字色（近白）。
        /// 两个取值都不是写死的色值，且与十二种填充色的对比度都不低于 3:1，
        /// 实测值由 --selftest 打印并断言。
        /// </summary>
        public static Color LineHot()
        {
            return ThemeManager.Current == ThemeMode.Dark
                ? ThemeManager.Palette.TextOf(ThemeSlot.Extra1)
                : ThemeManager.Palette.TextOf(ThemeSlot.Active);
        }
    }

    // ───────────────────────────────────────────────────────────── 连线

    /// <summary>画出去的一段连线。只有横段与竖段两种，斜段由自检抓。</summary>
    internal sealed class AlgoLine
    {
        public int X1, Y1, X2, Y2;
        public bool Hot;        // 当前转移／当前选中

        public bool Horizontal { get { return Y1 == Y2; } }
        public bool Vertical { get { return X1 == X2; } }
    }

    /// <summary>
    /// 连线集合。**只提供正交的加法**：横段、竖段，以及把一个任意方向的连接
    /// 拆成「先横后竖」两段。因此产出斜段的唯一可能是有人直接 Add 一个斜的，
    /// 自检里专门造了一条这样的线当反例，证明检查抓得住。
    /// </summary>
    internal sealed class AlgoPath
    {
        public readonly List<AlgoLine> Lines = new List<AlgoLine>();

        public void Clear() { Lines.Clear(); }

        public void Add(int x1, int y1, int x2, int y2, bool hot)
        {
            if (x1 == x2 && y1 == y2) return;
            AlgoLine line = new AlgoLine();
            line.X1 = x1; line.Y1 = y1; line.X2 = x2; line.Y2 = y2; line.Hot = hot;
            Lines.Add(line);
        }

        public void H(int y, int x1, int x2, bool hot)
        {
            if (x1 == x2) return;
            Add(x1, y, x2, y, hot);
        }

        public void V(int x, int y1, int y2, bool hot)
        {
            if (y1 == y2) return;
            Add(x, y1, x, y2, hot);
        }

        /// <summary>两点之间的正交连接：同排或同列直接连，否则拆成「先横后竖」。</summary>
        public void Link(int x1, int y1, int x2, int y2, bool hot)
        {
            if (y1 == y2) { H(y1, x1, x2, hot); return; }
            if (x1 == x2) { V(x1, y1, y2, hot); return; }
            H(y1, x1, x2, hot);
            V(x2, y1, y2, hot);
        }

        /// <summary>经中间通道绕行：竖 → 横 → 竖，三段都是正交的。</summary>
        public void ViaY(int x1, int y1, int x2, int y2, int midY, bool hot)
        {
            V(x1, y1, midY, hot);
            H(midY, x1, x2, hot);
            V(x2, midY, y2, hot);
        }

        /// <summary>斜段条数。0 表示每一段都是水平或垂直。</summary>
        public int DiagonalCount()
        {
            int bad = 0;
            for (int i = 0; i < Lines.Count; i++)
            {
                AlgoLine line = Lines[i];
                bool h = line.Y1 == line.Y2;
                bool v = line.X1 == line.X2;
                if (!h && !v) bad++;
            }
            return bad;
        }

        /// <summary>某段线是否穿过这个矩形（「线在字的位置让开」要用）。</summary>
        public bool Hits(Rectangle box)
        {
            for (int i = 0; i < Lines.Count; i++)
            {
                AlgoLine line = Lines[i];
                int left = Math.Min(line.X1, line.X2);
                int right = Math.Max(line.X1, line.X2);
                int top = Math.Min(line.Y1, line.Y2);
                int bottom = Math.Max(line.Y1, line.Y2);
                if (right < box.Left || left > box.Right) continue;
                if (bottom < box.Top || top > box.Bottom) continue;
                return true;
            }
            return false;
        }
    }

    /// <summary>元素上的一段文字：写什么、写在哪个矩形里、盖底用哪个颜色。</summary>
    internal sealed class AlgoRun
    {
        public int Owner = -1;
        public string Text;
        public Rectangle Box;
        public Color Pad;
        /// <summary>true 表示这一行用 11 像素那一档（次要一行），false 用 13 像素那一档。</summary>
        public bool Small;
        /// <summary>这一行实际用的字号（像素）。--layoutcheck 按它断言最小字号。</summary>
        public float FontPx;
    }

    /// <summary>画布上的一行说明文字（不在元素里，画在留白处）。</summary>
    internal sealed class AlgoLabel
    {
        public Rectangle Box;
        public string Text;
        public bool Muted;      // 次要说明用附注色，主要标签用正文色
    }

    // ───────────────────────────────────────────────────────────── 一步的轨迹

    /// <summary>一次写入：把某个元素的某个字段改成某个值。</summary>
    internal sealed class AlgoWrite
    {
        public int Index;
        public int Field;    // 0 状态，1 标记，2 标签，3..8 属性 0..5
        public int Value;

        public AlgoWrite(int index, int field, int value)
        {
            Index = index; Field = field; Value = value;
        }
    }

    /// <summary>
    /// 一步：若干次写入 + 这一帧的头部数字 + 一句状态行附注。
    ///
    /// **九个场景都用这一种步**：模型在 Build 时把整条轨迹算出来，
    /// 引擎只按序号播放。因此「回放到第 N 步」天然成立，
    /// 自检里跑到底的那一组数字与界面上看到的必然一致。
    /// </summary>
    internal sealed class AlgoStep
    {
        public readonly List<AlgoWrite> Writes = new List<AlgoWrite>();
        public int StateCode;
        public int Work;
        public int Pending;
        public int Primary = -1;
        public int Secondary = -1;
        public int CustomA;
        public int CustomB;
        public int Current = -1;
        public int Focus = -1;
        public bool Finished;
        public bool HasResult;
        public string Note = "";
    }

    /// <summary>整条轨迹：初始帧的写入 + 一步一步的写入。</summary>
    internal sealed class AlgoTrace
    {
        public readonly List<AlgoWrite> Init = new List<AlgoWrite>();
        public readonly List<AlgoStep> Steps = new List<AlgoStep>();
    }

    /// <summary>
    /// 九个场景的共同底座：把算法写成一串 AlgoStep。
    /// 子类只要实现 Build（生成轨迹）、Title、StatusText 三件事。
    /// </summary>
    internal abstract class AlgoModel
    {
        protected readonly AlgoTrace Trace = new AlgoTrace();

        // 这一帧的头部数字：每步之前改，End 时快照进 AlgoStep
        protected int StateCode;
        protected int Work;
        protected int Pending;
        protected int Primary = -1;
        protected int Secondary = -1;
        protected int CustomA;
        protected int CustomB;
        protected int Current = -1;
        protected int Focus = -1;
        protected bool Finished;
        protected bool HasResult;
        protected string Note = "";

        private AlgoStep open;

        public AlgoTrace Result { get { return Trace; } }
        public int StepCount { get { return Trace.Steps.Count; } }

        /// <summary>按配置生成整条轨迹。optionA／optionB 越界时由这里收拢。</summary>
        public abstract void Build(int optionA, int optionB, int seed);

        public abstract string Title();

        public abstract string StatusText();

        // ── 写轨迹的三个动作：Begin / 若干次写入 / End

        protected void Begin() { open = new AlgoStep(); }

        protected void Set(int element, byte state) { open.Writes.Add(new AlgoWrite(element, 0, state)); }

        protected void Mark(int element, byte marker) { open.Writes.Add(new AlgoWrite(element, 1, marker)); }

        protected void Tag(int element, byte tag) { open.Writes.Add(new AlgoWrite(element, 2, tag)); }

        /// <summary>属性 0 到 5。slot 是属性号，不是元素下标。</summary>
        protected void A(int element, int slot, int value) { open.Writes.Add(new AlgoWrite(element, 3 + slot, value)); }

        protected void Say(string text) { Note = text; }

        protected void End()
        {
            open.StateCode = StateCode;
            open.Work = Work;
            open.Pending = Pending;
            open.Primary = Primary;
            open.Secondary = Secondary;
            open.CustomA = CustomA;
            open.CustomB = CustomB;
            open.Current = Current;
            open.Focus = Focus;
            open.Finished = Finished;
            open.HasResult = HasResult;
            open.Note = Note;
            Trace.Steps.Add(open);
            open = null;
        }

        /// <summary>一次改动里同时写两三个元素时的简写。</summary>
        protected void Set2(int a, byte sa, int b, byte sb)
        {
            Set(a, sa);
            Set(b, sb);
        }

        // ── 初始帧：结构里那些一开始就摆在那里的东西

        protected void InitSet(int element, byte state) { Trace.Init.Add(new AlgoWrite(element, 0, state)); }

        protected void InitMark(int element, byte marker) { Trace.Init.Add(new AlgoWrite(element, 1, marker)); }

        protected void InitTag(int element, byte tag) { Trace.Init.Add(new AlgoWrite(element, 2, tag)); }

        protected void InitA(int element, int slot, int value)
        {
            Trace.Init.Add(new AlgoWrite(element, 3 + slot, value));
        }

        /// <summary>把一个元素整块设好：状态 + 六个属性。场景数据大多这样摆。</summary>
        protected void InitCell(int element, byte state, int a0, int a1, int a2)
        {
            InitSet(element, state);
            InitA(element, 0, a0);
            InitA(element, 1, a1);
            InitA(element, 2, a2);
        }

        protected static int Clamp(int value, int low, int high)
        {
            if (value < low) return low;
            if (value > high) return high;
            return value;
        }

        /// <summary>自检里逐场景打印的一行附加说明，默认没有。</summary>
        public virtual string Detail() { return ""; }

        /// <summary>
        /// 界面进程通过邮箱发过来的命令（翻转一格、清掉翻转）。
        /// 默认什么都不做：只有转移表与图的边是可翻转的。
        /// </summary>
        public virtual void Command(int op, int arg0, int arg1) { }

        /// <summary>把这一轮的翻转都撤掉，回到种子生成的那一份数据。</summary>
        public virtual void ClearEdits() { }
    }

    // ───────────────────────────────────────────────────────────── 场景 1 递归调用树

    /// <summary>
    /// 递归调用树。节点 0..31 是调用树，32..47 是栈帧列。
    ///
    /// 三种形态：线性递归 sum(n)、斐波那契 fib(n)、尾递归 fact(n, acc)。
    /// 选项二开「尾调用优化」时，**只有尾递归那一支的栈深会掉到 1**，
    /// 另外两支不是尾调用，开关不生效——这一点在状态行里写明。
    /// </summary>
    internal sealed class RecursionModel : AlgoModel
    {
        public const int TreeBase = 0;
        public const int TreeMax = 32;
        public const int StackBase = 32;
        public const int StackMax = 16;

        private int optionA, optionB;
        private int calls, bottoms, maxDepth, answer, deepestNode;
        private int planCount;
        private readonly int[] planArg = new int[TreeMax];
        private readonly int[] planParent = new int[TreeMax];
        private readonly int[] planDepth = new int[TreeMax];
        private readonly int[] planLeft = new int[TreeMax];
        private readonly int[] planRight = new int[TreeMax];

        public override void Build(int a, int b, int seed)
        {
            optionA = Clamp(a, 0, 2);
            optionB = Clamp(b, 0, 1);
            Trace.Init.Clear();
            Trace.Steps.Clear();
            calls = 0; bottoms = 0; maxDepth = 0; answer = 0; deepestNode = -1;
            Plan();
            for (int i = 0; i < planCount; i++)
            {
                InitA(TreeBase + i, 1, planParent[i]);
                InitA(TreeBase + i, 2, planDepth[i]);
            }
            for (int i = 0; i < StackMax; i++) InitA(StackBase + i, 0, -1);
            if (optionA == 0) EmitLinear(0, planArg[0], 0, -1, 0);
            else if (optionA == 1) EmitFib(0, planArg[0], 0, -1);
            else EmitTail(0, planArg[0], 1, 0, -1);
        }

        /// <summary>先定下整棵树的形状：参数、父节点、层数、左右孩子。节点号是前序。</summary>
        private void Plan()
        {
            planCount = 0;
            if (optionA == 0) PlanChain(5);
            else if (optionA == 1) PlanFib(6, -1, 0);
            else PlanChain(5);
        }

        private void PlanChain(int n)
        {
            int parent = -1;
            for (int value = n; value >= 0; value--)
            {
                int id = planCount++;
                planArg[id] = value;
                planParent[id] = parent;
                planDepth[id] = n - value;
                planLeft[id] = -1;
                planRight[id] = -1;
                if (parent >= 0) planLeft[parent] = id;
                parent = id;
            }
        }

        private int PlanFib(int n, int parent, int depth)
        {
            int id = planCount++;
            planArg[id] = n;
            planParent[id] = parent;
            planDepth[id] = depth;
            planLeft[id] = -1;
            planRight[id] = -1;
            if (n > 1)
            {
                planLeft[id] = PlanFib(n - 1, id, depth + 1);
                planRight[id] = PlanFib(n - 2, id, depth + 1);
            }
            return id;
        }

        /// <summary>尾递归开优化时所有调用共用一个栈帧，因此帧号恒为 0。</summary>
        private int FrameOf(int depth)
        {
            if (optionA == 2 && optionB == 1) return 0;
            return depth;
        }

        private void Enter(int node, int value, int parent, int depth, string call, int acc)
        {
            calls++;
            int frame = FrameOf(depth);
            // 尾调用优化开的时候所有调用共用一个帧，**栈深因此不涨**：
            // 这里量的就是帧号，不是递归层数。
            if (frame + 1 > maxDepth) { maxDepth = frame + 1; deepestNode = node; }
            Begin();
            Set(TreeBase + node, St.Active);
            A(TreeBase + node, 0, value);
            A(TreeBase + node, 3, -1);
            if (parent >= 0) Set(TreeBase + parent, St.Pending);
            Set(StackBase + frame, St.Active);
            A(StackBase + frame, 0, value);
            A(StackBase + frame, 1, depth);
            A(StackBase + frame, 2, acc);
            Work = calls;
            Pending = frame + 1;
            Primary = calls;
            Secondary = maxDepth;
            Current = TreeBase + node;
            Focus = TreeBase;
            StateCode = 1;
            Say("进入 " + call + "：栈深 " + (frame + 1)
                + (optionA == 2 && optionB == 1 ? "（尾调用优化：帧复用，栈深不涨）" : ""));
            End();
        }

        private void Bottom(int node, int value, int depth, int result, string call)
        {
            bottoms++;
            Begin();
            Set(TreeBase + node, St.Result);
            A(TreeBase + node, 3, result);
            // 出栈的帧标成「已失效」而不是抹掉：整列的高度因此就是这次递归到过的最深处
            Set(StackBase + FrameOf(depth), St.Blocked);
            Work = calls;
            Pending = optionA == 2 && optionB == 1 ? 1 : depth;
            Current = TreeBase + node;
            StateCode = 2;
            Say("触底：" + call + " 命中基线，返回 " + result + "（这一层不再往下调）");
            End();
        }

        private void Return(int node, int depth, int result, string call, string how)
        {
            Begin();
            Set(TreeBase + node, St.Done);
            A(TreeBase + node, 3, result);
            Set(StackBase + FrameOf(depth), St.Blocked);
            Work = calls;
            Pending = optionA == 2 && optionB == 1 ? 1 : depth;
            Primary = calls;
            Secondary = maxDepth;
            Current = TreeBase + node;
            StateCode = 2;
            Say(how + "：" + call + " = " + result);
            End();
        }

        private int EmitLinear(int node, int n, int depth, int parent, int acc)
        {
            Enter(node, n, parent, depth, "sum(" + n + ")", acc);
            int value;
            if (n == 0)
            {
                Bottom(node, n, depth, 0, "sum(0)");
                value = 0;
            }
            else
            {
                int child = planLeft[node];
                int inner = EmitLinear(child, planArg[child], depth + 1, node, n);
                value = n + inner;
                Return(node, depth, value, "sum(" + n + ")",
                       "返回：sum(" + n + ") = " + n + " + sum(" + (n - 1) + ")");
            }
            if (depth == 0)
            {
                answer = value;
                Finished = true;
                HasResult = true;
                CustomA = bottoms;
                CustomB = answer;
                StateCode = 3;
                Begin();
                Set(TreeBase + node, St.Result);
                Work = calls; Pending = 0; Primary = calls; Secondary = maxDepth;
                Current = TreeBase + node;
                Say("整棵树走完：sum(5) = " + answer + "，调用 " + calls + " 次、触底 " + bottoms
                    + " 次、最深栈 " + maxDepth + " 层");
                End();
            }
            return value;
        }

        private int EmitFib(int node, int n, int depth, int parent)
        {
            Enter(node, n, parent, depth, "fib(" + n + ")", 0);
            int value;
            if (n <= 1)
            {
                Bottom(node, n, depth, n, "fib(" + n + ")");
                value = n;
            }
            else
            {
                int left = planLeft[node];
                int right = planRight[node];
                int a = EmitFib(left, planArg[left], depth + 1, node);
                int b = EmitFib(right, planArg[right], depth + 1, node);
                value = a + b;
                Return(node, depth, value, "fib(" + n + ")",
                       "合并：fib(" + n + ") = " + a + " + " + b);
            }
            if (depth == 0)
            {
                answer = value;
                Finished = true;
                HasResult = true;
                CustomA = bottoms;
                CustomB = answer;
                StateCode = 3;
                Begin();
                Set(TreeBase + node, St.Result);
                Work = calls; Pending = 0; Primary = calls; Secondary = maxDepth;
                Current = TreeBase + node;
                Say("整棵树走完：fib(6) = " + answer + "，调用 " + calls + " 次、触底 " + bottoms
                    + " 次、最深栈 " + maxDepth + " 层");
                End();
            }
            return value;
        }

        private int EmitTail(int node, int n, int acc, int depth, int parent)
        {
            Enter(node, n, parent, depth, "fact(" + n + ", " + acc + ")", acc);
            int value;
            if (n <= 1)
            {
                Bottom(node, n, depth, acc, "fact(" + n + ", " + acc + ")");
                value = acc;
            }
            else
            {
                int child = planLeft[node];
                value = EmitTail(child, planArg[child], n * acc, depth + 1, node);
                Return(node, depth, value, "fact(" + n + ", " + acc + ")",
                       "返回：fact(" + n + ", " + acc + ") 的栈帧已经用不着了");
            }
            if (depth == 0)
            {
                answer = value;
                Finished = true;
                HasResult = true;
                CustomA = bottoms;
                CustomB = answer;
                StateCode = 3;
                Begin();
                Set(TreeBase + node, St.Result);
                Work = calls; Pending = 0; Primary = calls; Secondary = maxDepth;
                Current = TreeBase + node;
                Say("整棵树走完：fact(5) = " + answer + "，调用 " + calls + " 次、最深栈 " + maxDepth
                    + " 层" + (optionB == 1 ? "（尾调用优化开：帧复用）" : "（尾调用优化关：每层一个帧）"));
                End();
            }
            return value;
        }

        public override string Title()
        {
            string form = optionA == 0 ? "线性递归 sum(5)"
                        : optionA == 1 ? "斐波那契 fib(6)" : "尾递归 fact(5, 1)";
            return "递归调用树：" + form + "　尾调用优化" + (optionB == 1 ? "开" : "关")
                   + "　调用 " + calls + " 次　最深栈 " + maxDepth + " 层";
        }

        public override string StatusText()
        {
            return "调用 " + calls + " 次　触底 " + bottoms + " 次　当前栈深 " + Pending
                   + "　最深栈 " + maxDepth + " 层　返回值 " + (HasResult ? answer : -1);
        }

        public override string Detail()
        {
            return "calls=" + calls + " bottoms=" + bottoms + " maxdepth=" + maxDepth
                   + " answer=" + answer + " deepestnode=" + deepestNode;
        }
    }

    // ───────────────────────────────────────────────────────────── 场景 2 记忆化·分治·回溯

    /// <summary>
    /// 三个子场景共用一棵树（节点 0..63）与右侧一列辅助格（64..79）：
    ///   记忆化  —— 同一棵 fib 树上重复子问题被标出来，记忆化前／后访问次数对照；
    ///   分治    —— 归并排序的递归树，合并发生在哪一层直接写在状态行上；
    ///   回溯    —— 子集／全排列的决策树，选择与撤销一步一步成对出现。
    /// </summary>
    internal sealed class DivideModel : AlgoModel
    {
        public const int TreeBase = 0;
        public const int TreeMax = 64;
        public const int AuxBase = 64;
        public const int AuxMax = 16;

        private int optionA, optionB;
        private int planCount, nodes, hits, calls, maxDepth, solutions, chooses, undos;
        private int mergeCount, compares, treeHeight, writes, answer;
        private readonly int[] planArg = new int[TreeMax];
        private readonly int[] planParent = new int[TreeMax];
        private readonly int[] planDepth = new int[TreeMax];
        private readonly int[] planLeft = new int[TreeMax];
        private readonly int[] planRight = new int[TreeMax];
        private int[] values = new int[16];
        private int valueCount;

        public override void Build(int a, int b, int seed)
        {
            optionA = Clamp(a, 0, 2);
            optionB = Clamp(b, 0, 1);
            Trace.Init.Clear();
            Trace.Steps.Clear();
            planCount = 0; nodes = 0; hits = 0; calls = 0; maxDepth = 0;
            solutions = 0; chooses = 0; undos = 0; mergeCount = 0; compares = 0;
            treeHeight = 0; writes = 0; answer = 0; valueCount = 0;
            if (optionA == 0) BuildMemo(seed);
            else if (optionA == 1) BuildMerge(seed);
            else BuildBacktrack(seed);
        }

        // ── 记忆化：同一棵 fib 树，重复子问题标出来

        private void BuildMemo(int seed)
        {
            PlanFibTree(6, -1, 0);
            for (int i = 0; i < planCount; i++)
            {
                InitA(TreeBase + i, 1, planParent[i]);
                InitA(TreeBase + i, 2, planDepth[i]);
            }
            for (int i = 0; i < 8; i++)
            {
                InitCell(AuxBase + i, St.Data, i, i, -1);
                cache[i] = 0;
                cached[i] = false;
            }
            for (int i = 8; i < AuxMax; i++) InitA(AuxBase + i, 1, -2);
            MemoFib(0, planArg[0], 0, -1);
            Finished = true;
            HasResult = true;
            CustomA = hits;
            CustomB = maxDepth;
            Begin();
            Set(TreeBase, St.Result);
            Primary = nodes + hits; Secondary = hits; Work = calls; Pending = 0; StateCode = 3;
            Say("走完：" + (optionB == 1 ? "记忆化开" : "记忆化关") + "　调用 " + calls
                + " 次、树上节点 " + (nodes + hits) + " 个、命中缓存 " + hits + " 次、fib(6) = " + answer);
            End();
        }

        private void PlanFibTree(int n, int parent, int depth)
        {
            int id = planCount++;
            planArg[id] = n;
            planParent[id] = parent;
            planDepth[id] = depth;
            planLeft[id] = -1;
            planRight[id] = -1;
            if (depth + 1 > treeHeight) treeHeight = depth + 1;
            if (n > 1)
            {
                planLeft[id] = planCount;
                PlanFibTree(n - 1, id, depth + 1);
                planRight[id] = planCount;
                PlanFibTree(n - 2, id, depth + 1);
            }
        }

        private int MemoFib(int node, int n, int depth, int parent)
        {
            calls++;
            if (depth + 1 > maxDepth) maxDepth = depth + 1;
            bool cached = optionB == 1 && CacheHas(n);
            Begin();
            Set(TreeBase + node, cached ? St.Cut : St.Active);
            A(TreeBase + node, 0, n);
            A(TreeBase + node, 2, depth);
            if (cached)
            {
                hits++;
                A(TreeBase + node, 3, CacheValue(n));
                A(TreeBase + node, 4, 1);
                Set(AuxBase + n, St.Result);
                Mark(TreeBase + node, 1);
                Work = calls; Primary = nodes + hits; Secondary = hits;
                Current = TreeBase + node;
                StateCode = 2;
                Say("命中缓存：fib(" + n + ") 已经算过，直接取 " + CacheValue(n) + "，不再往下展开");
                End();
                return CacheValue(n);
            }
            nodes++;
            Tag(TreeBase + node, 1);
            A(TreeBase + node, 4, 1);
            Set(AuxBase + n, St.Pending);
            A(AuxBase + n, 0, n);
            Work = calls; Primary = nodes + hits; Secondary = hits; Pending = depth + 1;
            Current = TreeBase + node;
            StateCode = 1;
            Say("第一次遇到 fib(" + n + ")：" + (optionB == 1 ? "算完写进缓存" : "记忆化关，每次重算"));
            End();

            int value;
            if (n <= 1)
            {
                value = n;
                Store(n, value);
                Begin();
                Set(TreeBase + node, St.Result);
                A(TreeBase + node, 3, value);
                Set(AuxBase + n, St.Done);
                A(AuxBase + n, 2, value);
                Pending = depth;
                StateCode = 2;
                Say("触底：fib(" + n + ") = " + value
                    + (optionB == 1 ? "，写进缓存" : "（记忆化关，不写缓存）"));
                End();
            }
            else
            {
                int a = MemoFib(planLeft[node], planArg[planLeft[node]], depth + 1, node);
                int b = MemoFib(planRight[node], planArg[planRight[node]], depth + 1, node);
                value = a + b;
                Store(n, value);
                Begin();
                Set(TreeBase + node, St.Done);
                A(TreeBase + node, 3, value);
                Set(AuxBase + n, St.Done);
                A(AuxBase + n, 2, value);
                Pending = depth;
                StateCode = 2;
                Say("合并：fib(" + n + ") = " + a + " + " + b + " = " + value);
                End();
            }
            if (depth == 0)
            {
                answer = value;
                Begin();
                Set(TreeBase, St.Result);
                Work = calls; Primary = nodes + hits; Secondary = hits; Pending = 0;
                StateCode = 3;
                Say("走完：调用 " + calls + " 次、树上节点 " + (nodes + hits) + " 个、命中 " + hits
                    + " 次、fib(6) = " + value);
                End();
            }
            return value;
        }

        private int[] cache = new int[8];
        private bool[] cached = new bool[8];

        private bool CacheHas(int n) { return n >= 0 && n < 8 && cached[n]; }

        private int CacheValue(int n) { return n >= 0 && n < 8 ? cache[n] : 0; }

        /// <summary>算出一个值就写进缓存。记忆化关的时候不写，因此每次都要重算。</summary>
        private void Store(int n, int value)
        {
            if (optionB != 1 || n < 0 || n >= 8) return;
            cache[n] = value;
            cached[n] = true;
        }

        // ── 分治：归并排序的递归树

        private void BuildMerge(int seed)
        {
            int n = optionB == 1 ? 16 : 8;
            valueCount = n;
            DeterministicRng rng = new DeterministicRng((uint)(seed + 77));
            for (int i = 0; i < n; i++) values[i] = 1 + rng.Next(90);
            for (int i = 0; i < n; i++) InitCell(AlgoConst.InputBase + i, St.Data, values[i], i, -1);
            PlanMerge(0, n, -1, 0);
            for (int i = 0; i < planCount; i++)
            {
                InitA(TreeBase + i, 1, planParent[i]);
                InitA(TreeBase + i, 2, planDepth[i]);
                InitTag(TreeBase + i, 1);
            }
            for (int i = 0; i < AuxMax; i++) InitA(AuxBase + i, 1, -2);
            MergeSort(0, 0, n, -1);
            Finished = true;
            HasResult = true;
            CustomA = treeHeight;
            CustomB = writes;
            Begin();
            Set(TreeBase, St.Result);
            Primary = planCount; Secondary = mergeCount; Work = compares; Pending = 0;
            StateCode = 3;
            Say("走完：节点 " + planCount + " 个、合并 " + mergeCount + " 次、比较 " + compares
                + " 次、树高 " + treeHeight + " 层");
            End();
        }

        private void PlanMerge(int lo, int hi, int parent, int depth)
        {
            int id = planCount++;
            planArg[id] = lo;
            planHi[id] = hi;
            planParent[id] = parent;
            planDepth[id] = depth;
            planLeft[id] = -1;
            planRight[id] = -1;
            if (depth + 1 > treeHeight) treeHeight = depth + 1;
            if (hi - lo > 1)
            {
                int mid = (lo + hi) / 2;
                planLeft[id] = planCount;
                PlanMerge(lo, mid, id, depth + 1);
                planRight[id] = planCount;
                PlanMerge(mid, hi, id, depth + 1);
            }
        }

        private int MergeSort(int id, int lo, int hi, int parent)
        {
            nodes++;
            Begin();
            Set(TreeBase + id, St.Active);
            A(TreeBase + id, 0, lo);
            A(TreeBase + id, 3, hi);
            if (parent >= 0) Set(TreeBase + parent, St.Pending);
            Work = compares; Primary = nodes; Pending = planDepth[id] + 1;
            Current = TreeBase + id;
            StateCode = 1;
            Say("分：" + (hi - lo == 1 ? "触底，" : "拆成 ") + "[" + lo + "," + hi + ")"
                + "（第 " + planDepth[id] + " 层）");
            End();

            if (hi - lo == 1)
            {
                Begin();
                Set(TreeBase + id, St.Result);
                A(TreeBase + id, 4, values[lo]);
                Pending = planDepth[id];
                StateCode = 2;
                Say("触底：[" + lo + "," + hi + ") 只剩一个元素，它自己就是有序的");
                End();
                return values[lo];
            }

            int mid = (lo + hi) / 2;
            MergeSort(planLeft[id], lo, mid, id);
            MergeSort(planRight[id], mid, hi, id);

            // 合并：两个有序段并成一个，写在右侧的合并输出行上
            mergeCount++;
            int i2 = lo, j2 = mid, out2 = 0;
            int[] buffer = new int[hi - lo];
            Begin();
            Set(TreeBase + id, St.Marked);
            Mark(TreeBase + id, 1);
            Pending = planDepth[id] + 1;
            StateCode = 1;
            Say("治：合并第 " + planDepth[id] + " 层的 [" + lo + "," + mid + ") 与 ["
                + mid + "," + hi + ")");
            End();
            while (i2 < mid || j2 < hi)
            {
                int take;
                bool leftWins;
                if (i2 >= mid) { take = j2++; leftWins = false; }
                else if (j2 >= hi) { take = i2++; leftWins = true; }
                else
                {
                    compares++;
                    leftWins = values[i2] <= values[j2];
                    take = leftWins ? i2++ : j2++;
                }
                buffer[out2] = values[take];
                Begin();
                Set(TreeBase + id, St.Marked);
                Set(AuxBase + out2, St.Active);
                A(AuxBase + out2, 0, values[take]);
                A(AuxBase + out2, 1, take);
                Set(AlgoConst.InputBase + take, St.Active);
                Work = compares;
                Current = AuxBase + out2;
                Pending = planDepth[id] + 1;
                StateCode = 1;
                Say("合并：从" + (leftWins ? "左" : "右") + "半段取 " + values[take]
                    + " 放到输出第 " + out2 + " 格");
                End();
                out2++;
            }
            for (int k = 0; k < hi - lo; k++)
            {
                values[lo + k] = buffer[k];
                writes++;
                Begin();
                Set(AuxBase + k, St.Done);
                Set(AlgoConst.InputBase + lo + k, St.Marked);
                A(AlgoConst.InputBase + lo + k, 0, buffer[k]);
                A(AuxBase + k, 0, buffer[k]);
                Work = compares;
                Current = AlgoConst.InputBase + lo + k;
                StateCode = 2;
                Say("写回：第 " + (lo + k) + " 格 ← " + buffer[k]);
                End();
            }
            Begin();
            for (int k = 0; k < hi - lo; k++) Set(AuxBase + k, St.Empty);
            Set(TreeBase + id, St.Done);
            A(TreeBase + id, 4, values[lo]);
            Pending = planDepth[id];
            StateCode = 2;
            Say("合并完成：[" + lo + "," + hi + ") 已有序");
            End();
            return values[lo];
        }

        // ── 回溯：子集与全排列

        private void BuildBacktrack(int seed)
        {
            bool subsets = optionB == 0;
            int n = subsets ? 4 : 3;
            PlanBacktrack(n, subsets, -1, 0, 0);
            for (int i = 0; i < planCount; i++)
            {
                InitA(TreeBase + i, 1, planParent[i]);
                InitA(TreeBase + i, 2, planDepth[i]);
            }
            for (int i = 0; i < AuxMax; i++) InitA(AuxBase + i, 1, -2);
            for (int i = 0; i < n; i++) InitCell(AlgoConst.InputBase + i, St.Data, i + 1, i, -1);
            Backtrack(0, 0, -1, n, subsets, 0);
            Finished = true;
            HasResult = true;
            CustomA = solutions;
            CustomB = maxDepth;
            Begin();
            Set(TreeBase, St.Result);
            Primary = nodes; Secondary = chooses; Work = undos; Pending = 0; StateCode = 3;
            Say("走完：" + (subsets ? "子集" : "全排列") + " 共 " + solutions + " 个解、节点 "
                + nodes + " 个、选择 " + chooses + " 次、撤销 " + undos + " 次");
            End();
        }

        private int[] planHi = new int[TreeMax];
        private int[] planChoice = new int[TreeMax];

        private void PlanBacktrack(int n, bool subsets, int parent, int depth, int choice)
        {
            int id = planCount++;
            planArg[id] = n;
            planChoice[id] = choice;
            planParent[id] = parent;
            planDepth[id] = depth;
            planLeft[id] = -1;
            planRight[id] = -1;
            if (depth + 1 > treeHeight) treeHeight = depth + 1;
            int next = depth + 1;
            if (next > n) return;
            if (subsets)
            {
                planLeft[id] = planCount;
                PlanBacktrack(n, true, id, next, next);
                planRight[id] = planCount;
                PlanBacktrack(n, true, id, next, 0);
            }
            else
            {
                // 排列树：这一层可以挑任何一个还没用过的元素，兄弟们串成一条右链
                int previous = -1;
                planLeft[id] = planCount;
                for (int pick = 1; pick <= n; pick++)
                {
                    if (UsedOnPath(id, pick)) continue;
                    int child = planCount;
                    PlanBacktrack(n, false, id, next, pick);
                    if (previous >= 0) planRight[previous] = child;
                    previous = child;
                }
            }
        }

        private bool UsedOnPath(int node, int pick)
        {
            int walk = node;
            int guard = 0;
            while (walk >= 0 && guard++ < 32)
            {
                if (planChoice[walk] == pick) return true;
                walk = planParent[walk];
            }
            return false;
        }

        /// <summary>决策树：每一步要么「选」要么「不选」（子集），要么挑一个没用过的元素（排列）。</summary>
        private void Backtrack(int id, int depth, int parent, int n, bool subsets, int acc)
        {
            nodes++;
            if (depth + 1 > maxDepth) maxDepth = depth + 1;
            Begin();
            Set(TreeBase + id, St.Active);
            A(TreeBase + id, 0, planChoice[id]);
            A(TreeBase + id, 2, depth);
            if (parent >= 0) Set(TreeBase + parent, St.Pending);
            Work = undos; Primary = nodes; Pending = depth + 1;
            Current = TreeBase + id;
            StateCode = 1;
            Say("走到第 " + depth + " 层：当前部分解 " + PartialLabel(id, depth));
            End();

            int next = depth + 1;
            bool leaf = subsets ? next > n : next > n;
            if (leaf)
            {
                solutions++;
                Begin();
                Set(TreeBase + id, St.Result);
                A(TreeBase + id, 3, solutions);
                Pending = depth;
                StateCode = 2;
                Say("得到一个解：" + PartialLabel(id, depth) + "（第 " + solutions + " 个）");
                End();
                return;
            }

            if (subsets)
            {
                Choose(id, planLeft[id], next, next, true, "选 " + next);
                Choose(id, planRight[id], next, 0, true, "不选 " + next);
            }
            else
            {
                int child = planLeft[id];
                for (int pick = 1; pick <= n; pick++)
                {
                    if (UsedOnPath(id, pick)) continue;
                    Choose(id, child, next, pick, false, "选 " + pick);
                    child = NextSibling(child);
                }
            }

            Begin();
            Set(TreeBase + id, St.Done);
            Pending = depth;
            StateCode = 2;
            Say("这一层的分支都试完了，回到第 " + depth + " 层（撤销）");
            End();
        }

        private void Choose(int parentId, int childId, int depth, int pick, bool subsets, string what)
        {
            chooses++;
            Begin();
            Set(TreeBase + childId, St.Active);
            A(TreeBase + childId, 0, pick);
            Set(AuxBase + depth, St.Marked);
            A(AuxBase + depth, 0, pick);
            Pending = depth + 1;
            Work = undos; Primary = nodes;
            Current = TreeBase + childId;
            StateCode = 1;
            Say("选择：" + what + "（第 " + depth + " 层），选择次数 " + chooses);
            End();
            Backtrack(childId, depth, parentId, planArg[parentId], subsets, 0);
            undos++;
            Begin();
            Set(TreeBase + childId, St.Cut);
            Set(AuxBase + depth, St.Empty);
            A(AuxBase + depth, 0, -1);
            Pending = depth;
            Work = undos; Primary = nodes;
            Current = TreeBase + childId;
            StateCode = 1;
            Say("撤销：" + what + " 收回（撤销次数 " + undos + "，与选择次数成对）");
            End();
        }

        /// <summary>规划时按前序编号，兄弟节点就是右链上的下一个。</summary>
        private int NextSibling(int child)
        {
            if (child < 0 || child >= TreeMax) return -1;
            return planRight[child];
        }

        /// <summary>沿父指针往上收集选择，得到这个节点的部分解。</summary>
        private string PartialLabel(int node, int depth)
        {
            if (depth == 0) return "（空）";
            int[] picks = new int[8];
            int count = 0;
            int walk = node;
            int guard = 0;
            while (walk >= 0 && guard++ < 32)
            {
                if (planChoice[walk] > 0) picks[count++] = planChoice[walk];
                walk = planParent[walk];
            }
            string text = "";
            for (int i = count - 1; i >= 0; i--) text += picks[i].ToString(CultureInfo.InvariantCulture);
            return text.Length == 0 ? "（空）" : text;
        }

        public override string Title()
        {
            if (optionA == 0)
                return "记忆化：同一棵 fib(6) 树上重复子问题　记忆化" + (optionB == 1 ? "开" : "关")
                       + "　调用 " + calls + " 次　命中 " + hits + " 次";
            if (optionA == 1)
                return "分治：归并排序的递归树　" + valueCount + " 个元素　节点 " + planCount
                       + " 个　合并 " + mergeCount + " 次　树高 " + treeHeight + " 层";
            return "回溯：" + (optionB == 0 ? "子集（4 个元素）" : "全排列（3 个元素）")
                   + "　节点 " + nodes + " 个　解 " + solutions + " 个　选择 " + chooses
                   + " 次／撤销 " + undos + " 次";
        }

        public override string StatusText()
        {
            if (optionA == 0)
                return "调用 " + calls + " 次　树上节点 " + (nodes + hits) + " 个　命中缓存 " + hits
                       + " 次　缓存里已有 " + CacheSize() + " 个参数　fib(6) = " + (HasResult ? answer : -1);
            if (optionA == 1)
                return "节点 " + nodes + " 个　合并 " + mergeCount + " 次　比较 " + compares
                       + " 次　写回 " + writes + " 次　树高 " + treeHeight + " 层";
            return "节点 " + nodes + " 个　解 " + solutions + " 个　选择 " + chooses + " 次　撤销 "
                   + undos + " 次　最深 " + maxDepth + " 层";
        }

        private int CacheSize()
        {
            int total = 0;
            for (int i = 0; i < 8; i++) if (cached[i]) total++;
            return total;
        }

        public override string Detail()
        {
            if (optionA == 0)
                return "calls=" + calls + " nodes=" + (nodes + hits) + " computed=" + nodes
                       + " hits=" + hits + " result=" + answer;
            if (optionA == 1)
                return "nodes=" + planCount + " merges=" + mergeCount + " compares=" + compares
                       + " height=" + treeHeight + " writes=" + writes + " sorted=" + SortedFlag();
            return "nodes=" + nodes + " solutions=" + solutions + " chooses=" + chooses
                   + " undos=" + undos + " maxdepth=" + maxDepth;
        }

        private int SortedFlag()
        {
            for (int i = 1; i < valueCount; i++) if (values[i - 1] > values[i]) return 0;
            return 1;
        }
    }

    // ───────────────────────────────────────────────────────────── 场景 3 排序对照

    /// <summary>排序算法走出的一步。四种算法各自记一串，播放时按序号对齐。</summary>
    internal sealed class SortOp
    {
        public int Kind;      // 0 比较，1 交换，2 写入，3 就位，4 取起
        public int I, J;
        public int Value, Tag;
        public string Note = "";
    }

    /// <summary>
    /// 四种比较排序并排跑同一份数据：冒泡、插入（稳定）与选择、快速（不稳定）。
    /// 每一行是同一份输入的副本，播放时每一步让四行各走自己的一步，
    /// 因此「谁比较得多、谁先排好、已就位区域怎么推进」在屏幕上是同时发生的。
    /// 稳定性靠**带标记的相等元素**看：值相等时比较原始下标。
    /// </summary>
    internal sealed class SortModel : AlgoModel
    {
        public const int Rows = 4;
        public const int Cols = 12;
        public const int RowBase = 0;

        private int optionA, optionB, seed;
        private readonly int[] values = new int[Cols];
        private readonly int[] tags = new int[Cols];
        private readonly List<SortOp>[] ops = new List<SortOp>[Rows];
        private readonly int[] rowValue = new int[Rows * Cols];
        private readonly int[] rowTag = new int[Rows * Cols];
        private readonly int[] compares = new int[Rows];
        private readonly int[] moves = new int[Rows];
        private readonly int[] settled = new int[Rows];
        private readonly bool[] done = new bool[Rows];
        private readonly int[] activeA = new int[Rows];
        private readonly int[] activeB = new int[Rows];
        private readonly bool[] settledCell = new bool[Rows * Cols];
        private int finishedRows, unstableInversions, stableInversions;

        public static readonly string[] RowNames =
        {
            "冒泡排序（稳定）", "插入排序（稳定）", "选择排序（不稳定）", "快速排序（不稳定）"
        };

        public override void Build(int a, int b, int seedValue)
        {
            optionA = Clamp(a, 0, 2);
            optionB = Clamp(b, 0, 1);
            seed = seedValue;
            Trace.Init.Clear();
            Trace.Steps.Clear();
            GenerateData();
            for (int i = 0; i < Cols; i++) InitCell(AlgoConst.InputBase + i, St.Data, values[i], tags[i], i);
            for (int r = 0; r < Rows; r++)
            {
                ops[r] = new List<SortOp>();
                compares[r] = 0; moves[r] = 0; settled[r] = 0; done[r] = false;
                activeA[r] = -1; activeB[r] = -1;
                Array.Copy(values, 0, rowValue, r * Cols, Cols);
                Array.Copy(tags, 0, rowTag, r * Cols, Cols);
                for (int i = 0; i < Cols; i++)
                {
                    settledCell[r * Cols + i] = false;
                    InitCell(RowBase + r * Cols + i, St.Data, values[i], tags[i], i);
                }
                BuildOps(r);
            }
            Play();
        }

        private void GenerateData()
        {
            DeterministicRng rng = new DeterministicRng((uint)(seed + 31 * (optionA + 1) + 7 * optionB));
            if (optionB == 0)
            {
                for (int i = 0; i < Cols; i++) { values[i] = i + 1; tags[i] = i; }
                for (int i = Cols - 1; i > 0; i--)
                {
                    int j = rng.Next(i + 1);
                    int t = values[i]; values[i] = values[j]; values[j] = t;
                }
            }
            else
            {
                // 含相等元素：值只取四档，相等的一对对稳定性最有说服力
                for (int i = 0; i < Cols; i++) { values[i] = 1 + rng.Next(4); tags[i] = i; }
            }
            if (optionA == 1)
            {
                // 近乎有序：先排好，再换两对相邻的
                for (int i = 1; i < Cols; i++)
                    for (int j = i; j > 0 && values[j - 1] > values[j]; j--)
                        SwapData(j - 1, j);
                for (int k = 0; k < 2; k++)
                {
                    int i = rng.Next(Cols - 1);
                    SwapData(i, i + 1);
                }
            }
            else if (optionA == 2)
            {
                for (int i = 1; i < Cols; i++)
                    for (int j = i; j > 0 && values[j - 1] < values[j]; j--)
                        SwapData(j - 1, j);
            }
        }

        private void SwapData(int i, int j)
        {
            int v = values[i]; values[i] = values[j]; values[j] = v;
            int t = tags[i]; tags[i] = tags[j]; tags[j] = t;
        }

        private void BuildOps(int row)
        {
            if (row == 0) OpsBubble();
            else if (row == 1) OpsInsertion();
            else if (row == 2) OpsSelection();
            else OpsQuick(0, Cols - 1);
        }

        private void Add(int row, int kind, int i, int j, string note)
        {
            AddValue(row, kind, i, j, 0, 0, note);
        }

        private void AddValue(int row, int kind, int i, int j, int value, int tag, string note)
        {
            SortOp op = new SortOp();
            op.Kind = kind; op.I = i; op.J = j; op.Value = value; op.Tag = tag; op.Note = note;
            ops[row].Add(op);
        }

        private void OpsBubble()
        {
            int[] work = new int[Cols];
            int[] wt = new int[Cols];
            Array.Copy(values, work, Cols);
            Array.Copy(tags, wt, Cols);
            for (int pass = 0; pass < Cols - 1; pass++)
            {
                bool swapped = false;
                for (int i = 0; i < Cols - 1 - pass; i++)
                {
                    Add(0, 0, i, i + 1, "比较 " + work[i] + " 与 " + work[i + 1]);
                    if (work[i] > work[i + 1])
                    {
                        int t = work[i]; work[i] = work[i + 1]; work[i + 1] = t;
                        int g = wt[i]; wt[i] = wt[i + 1]; wt[i + 1] = g;
                        Add(0, 1, i, i + 1, "逆序，交换");
                        swapped = true;
                    }
                }
                Add(0, 3, Cols - 1 - pass, -1, "第 " + (pass + 1) + " 趟结束：末尾那个定了");
                if (!swapped)
                {
                    for (int j = 0; j < Cols - 1 - pass; j++)
                        Add(0, 3, j, -1, "这一趟没有交换，整段已经有序");
                    break;
                }
            }
        }

        private void OpsInsertion()
        {
            int[] work = new int[Cols];
            int[] wt = new int[Cols];
            Array.Copy(values, work, Cols);
            Array.Copy(tags, wt, Cols);
            Add(1, 3, 0, -1, "第一个元素自成一个有序段");
            for (int i = 1; i < Cols; i++)
            {
                int key = work[i], keyTag = wt[i];
                Add(1, 4, i, -1, "取起 " + key + "（原始下标 " + keyTag + "）");
                int j = i - 1;
                while (j >= 0)
                {
                    Add(1, 0, j, i, "比较 " + work[j] + " 与手里的 " + key);
                    if (work[j] > key)
                    {
                        work[j + 1] = work[j];
                        wt[j + 1] = wt[j];
                        Add(1, 2, j + 1, j, "比它大，往后挪一格");
                        j--;
                    }
                    else break;
                }
                work[j + 1] = key;
                wt[j + 1] = keyTag;
                AddValue(1, 2, j + 1, -1, key, keyTag, "把 " + key + " 插到第 " + (j + 1) + " 格");
            }
        }

        private void OpsSelection()
        {
            int[] work = new int[Cols];
            int[] wt = new int[Cols];
            Array.Copy(values, work, Cols);
            Array.Copy(tags, wt, Cols);
            for (int i = 0; i < Cols - 1; i++)
            {
                int min = i;
                for (int j = i + 1; j < Cols; j++)
                {
                    Add(2, 0, min, j, "比较 " + work[min] + " 与 " + work[j]);
                    if (work[j] < work[min]) min = j;
                }
                if (min != i)
                {
                    int t = work[i]; work[i] = work[min]; work[min] = t;
                    int g = wt[i]; wt[i] = wt[min]; wt[min] = g;
                    Add(2, 1, i, min, "把最小的换到第 " + i + " 格");
                }
                Add(2, 3, i, -1, "第 " + i + " 格定了");
            }
            Add(2, 3, Cols - 1, -1, "最后一格不用挑");
        }

        private void OpsQuick(int lo, int hi)
        {
            int[] work = new int[Cols];
            int[] wt = new int[Cols];
            Array.Copy(values, work, Cols);
            Array.Copy(tags, wt, Cols);
            QuickPlan(3, lo, hi, work, wt);
        }

        /// <summary>快速排序的分区是就地改的，递归时带着当前的工作副本往下走。</summary>
        private void QuickPlan(int row, int lo, int hi, int[] work, int[] wt)
        {
            if (lo > hi) return;
            if (lo == hi)
            {
                Add(row, 3, lo, -1, "区间只剩一个元素，它就在最终位置上");
                return;
            }
            int pivot = work[hi];
            Add(row, 4, hi, -1, "取最后一个 " + pivot + " 当主元");
            int store = lo;
            for (int j = lo; j < hi; j++)
            {
                Add(row, 0, j, hi, "比较 " + work[j] + " 与主元 " + pivot);
                if (work[j] < pivot)
                {
                    if (store != j)
                    {
                        int t = work[store]; work[store] = work[j]; work[j] = t;
                        int g = wt[store]; wt[store] = wt[j]; wt[j] = g;
                        Add(row, 1, store, j, "小于主元，换到左区");
                    }
                    store++;
                }
            }
            if (store != hi)
            {
                int t = work[store]; work[store] = work[hi]; work[hi] = t;
                int g = wt[store]; wt[store] = wt[hi]; wt[hi] = g;
                Add(row, 1, store, hi, "主元归位到第 " + store + " 格");
            }
            Add(row, 3, store, -1, "主元落在最终位置，左右两段各自再排");
            QuickPlan(row, lo, store - 1, work, wt);
            QuickPlan(row, store + 1, hi, work, wt);
        }

        /// <summary>四行交错播放：第 k 步让每一行各走自己的第 k 步。</summary>
        private void Play()
        {
            int maxSteps = 0;
            for (int r = 0; r < Rows; r++) if (ops[r].Count > maxSteps) maxSteps = ops[r].Count;
            for (int k = 0; k < maxSteps; k++)
            {
                Begin();
                for (int r = 0; r < Rows; r++)
                {
                    if (k >= ops[r].Count)
                    {
                        if (!done[r])
                        {
                            done[r] = true;
                            finishedRows++;
                        }
                        continue;
                    }
                    Apply(r, ops[r][k]);
                }
                Primary = Total(compares);
                Secondary = Total(moves);
                Work = Total(settled);
                Pending = Rows - finishedRows;
                CustomA = stableInversions;
                CustomB = unstableInversions;
                Current = -1;
                StateCode = finishedRows >= Rows ? 2 : 1;
                Say(NoteOf(k));
                End();
            }
            // 收尾：四行都排好之后，把稳定性数出来
            stableInversions = CountInversions(0) + CountInversions(1);
            unstableInversions = CountInversions(2) + CountInversions(3);
            for (int r = 0; r < Rows; r++)
            {
                if (done[r]) continue;
                done[r] = true;
                finishedRows++;
            }
            Finished = true;
            HasResult = true;
            Begin();
            for (int r = 0; r < Rows; r++)
                for (int i = 0; i < Cols; i++) Set(RowBase + r * Cols + i, St.Done);
            Primary = Total(compares);
            Secondary = Total(moves);
            Work = Total(settled);
            Pending = 0;
            CustomA = stableInversions;
            CustomB = unstableInversions;
            StateCode = 3;
            Say("四行都排完：比较 " + Primary + " 次、交换／写入 " + Secondary + " 次；"
                + "相等元素的倒置：稳定两行 " + stableInversions + " 处、不稳定两行 "
                + unstableInversions + " 处");
            End();
        }

        private string NoteOf(int k)
        {
            string text = "第 " + (k + 1) + " 步：";
            for (int r = 0; r < Rows; r++)
            {
                if (k >= ops[r].Count) continue;
                if (text.Length > 0 && r > 0) text += "；";
                text += RowNames[r].Substring(0, 2) + " " + ops[r][k].Note;
            }
            return text;
        }

        private void Apply(int row, SortOp op)
        {
            int baseIndex = RowBase + row * Cols;
            // 上一步点亮的两格收回来。**已经就位的格子不许退回数据色**：
            // 插入排序会拿有序段的元素做比较，退回去就把「已就位」擦掉了。
            if (activeA[row] >= 0 && op.Kind != 3)
                Set(baseIndex + activeA[row],
                    settledCell[baseIndex + activeA[row]] ? St.Done : St.Data);
            if (activeB[row] >= 0 && op.Kind != 3 && activeB[row] != activeA[row])
                Set(baseIndex + activeB[row],
                    settledCell[baseIndex + activeB[row]] ? St.Done : St.Data);
            activeA[row] = -1;
            activeB[row] = -1;
            switch (op.Kind)
            {
                case 0:
                    compares[row]++;
                    activeA[row] = op.I;
                    activeB[row] = op.J;
                    Set(baseIndex + op.I, St.Active);
                    Set(baseIndex + op.J, St.Ref);
                    break;
                case 1:
                    {
                        moves[row]++;
                        int i = op.I, j = op.J;
                        int v = rowValue[baseIndex + i]; rowValue[baseIndex + i] = rowValue[baseIndex + j];
                        rowValue[baseIndex + j] = v;
                        int t = rowTag[baseIndex + i]; rowTag[baseIndex + i] = rowTag[baseIndex + j];
                        rowTag[baseIndex + j] = t;
                        A(baseIndex + i, 0, rowValue[baseIndex + i]);
                        A(baseIndex + i, 1, rowTag[baseIndex + i]);
                        A(baseIndex + j, 0, rowValue[baseIndex + j]);
                        A(baseIndex + j, 1, rowTag[baseIndex + j]);
                        Set(baseIndex + i, St.Marked);
                        Set(baseIndex + j, St.Marked);
                        activeA[row] = i;
                        activeB[row] = j;
                    }
                    break;
                case 2:
                    {
                        moves[row]++;
                        int i = op.I;
                        rowValue[baseIndex + i] = op.J >= 0 ? rowValue[baseIndex + op.J] : op.Value;
                        rowTag[baseIndex + i] = op.J >= 0 ? rowTag[baseIndex + op.J] : op.Tag;
                        A(baseIndex + i, 0, rowValue[baseIndex + i]);
                        A(baseIndex + i, 1, rowTag[baseIndex + i]);
                        Set(baseIndex + i, St.Marked);
                        activeA[row] = i;
                    }
                    break;
                case 3:
                    settled[row]++;
                    settledCell[baseIndex + op.I] = true;
                    Set(baseIndex + op.I, St.Done);
                    break;
                case 4:
                    activeA[row] = op.I;
                    Set(baseIndex + op.I, St.Active);
                    break;
            }
        }

        private static int Total(int[] source)
        {
            int sum = 0;
            for (int i = 0; i < source.Length; i++) sum += source[i];
            return sum;
        }

        /// <summary>值相等的元素之间，原始下标被排反了几对。</summary>
        public int CountInversions(int row)
        {
            int baseIndex = RowBase + row * Cols;
            int bad = 0;
            for (int i = 0; i < Cols; i++)
            {
                for (int j = i + 1; j < Cols; j++)
                {
                    if (rowValue[baseIndex + i] != rowValue[baseIndex + j]) continue;
                    if (rowTag[baseIndex + i] > rowTag[baseIndex + j]) bad++;
                }
            }
            return bad;
        }

        public int RowCompares(int row) { return compares[row]; }

        public int RowMoves(int row) { return moves[row]; }

        public bool RowSorted(int row)
        {
            int baseIndex = RowBase + row * Cols;
            for (int i = 1; i < Cols; i++)
                if (rowValue[baseIndex + i - 1] > rowValue[baseIndex + i]) return false;
            return true;
        }

        public int StableInversions { get { return stableInversions; } }

        public int UnstableInversions { get { return unstableInversions; } }

        public override string Title()
        {
            return "排序对照：" + RowNames[0] + "／" + RowNames[1] + "／" + RowNames[2] + "／"
                   + RowNames[3] + "　" + Cols + " 个元素　"
                   + (optionB == 0 ? "互不相同" : "含相等元素（标着原始下标）");
        }

        public override string StatusText()
        {
            return "比较 " + Primary + " 次　交换／写入 " + Secondary + " 次　已就位 " + Work
                   + "／" + (Rows * Cols) + "　已排完 " + finishedRows + "／" + Rows + " 行　"
                   + "相等元素倒置：稳定行 " + stableInversions + "、不稳定行 " + unstableInversions;
        }

        public override string Detail()
        {
            string text = "";
            for (int r = 0; r < Rows; r++)
            {
                if (r > 0) text += "; ";
                text += RowNames[r].Substring(0, 2) + " cmp=" + compares[r] + " mv=" + moves[r]
                        + " settled=" + settled[r] + " inv=" + CountInversions(r)
                        + " sorted=" + (RowSorted(r) ? 1 : 0);
            }
            return text;
        }
    }

    // ───────────────────────────────────────────────────────────── 场景 4 查找对照

    /// <summary>
    /// 同一批数据上三种查找并排推进：线性从头扫、二分每次砍一半、哈希先定位桶再走链。
    /// 二分那一行的**区间宽度**写在快照的次指标里，屏幕上直接看得到它怎么收缩。
    /// </summary>
    internal sealed class SearchModel : AlgoModel
    {
        public const int DataCount = 16;
        public const int LinBase = 0;
        public const int BinBase = 16;
        public const int BucketBase = 32;
        public const int BucketCount = 8;
        public const int ChainBase = 40;
        public const int ChainMax = 4;

        private int optionA, optionB, seed, target;
        private readonly int[] data = new int[DataCount];
        private readonly List<int>[] chain = new List<int>[BucketCount];
        private readonly List<SearchOp>[] ops = new List<SearchOp>[3];
        private readonly int[] probes = new int[3];
        private readonly int[] align = new int[3];
        private readonly int[] lo = new int[3];
        private readonly int[] hi = new int[3];
        private readonly int[] chainAt = new int[3];
        private readonly bool[] done = new bool[3];
        private int finishedRows, binaryWidth, resultIndex;

        internal sealed class SearchOp
        {
            public int Kind;    // 0 比较，1 收区间，2 算哈希，3 走链，4 命中，5 未找到
            public int I, J;
            public string Note = "";
        }

        public static readonly string[] RowNames = { "线性查找", "二分查找", "哈希查找" };

        public override void Build(int a, int b, int seedValue)
        {
            optionA = Clamp(a, 0, 2);
            optionB = Clamp(b, 0, 1);
            seed = seedValue;
            Trace.Init.Clear();
            Trace.Steps.Clear();
            GenerateData();
            for (int i = 0; i < DataCount; i++) InitCell(AlgoConst.InputBase + i, St.Data, data[i], i, -1);
            for (int i = 0; i < DataCount; i++)
            {
                InitCell(LinBase + i, St.Data, data[i], i, -1);
                InitCell(BinBase + i, St.Data, data[i], i, -1);
            }
            for (int i = 0; i < BucketCount; i++)
            {
                chain[i] = new List<int>();
                InitCell(BucketBase + i, St.Data, i, chain[i].Count, -1);
            }
            for (int i = 0; i < DataCount; i++) chain[data[i] % BucketCount].Add(data[i]);
            for (int i = 0; i < BucketCount; i++)
            {
                InitA(BucketBase + i, 1, chain[i].Count);
                for (int k = 0; k < chain[i].Count && k < ChainMax; k++)
                    InitA(BucketBase + i, 2 + k, chain[i][k]);
            }
            for (int i = 0; i < ChainMax; i++) InitA(ChainBase + i, 0, -1);
            BuildOps();
            Play();
        }

        private void GenerateData()
        {
            DeterministicRng rng = new DeterministicRng((uint)(seed + 313 + optionB * 17));
            for (int i = 0; i < DataCount; i++) data[i] = 3 + i * 2 + (optionB == 1 && i % 5 == 4 ? -2 : 0);
            for (int i = 1; i < DataCount; i++)
                for (int j = i; j > 0 && data[j - 1] > data[j]; j--)
                {
                    int t = data[j - 1]; data[j - 1] = data[j]; data[j] = t;
                }
            if (optionA == 0) target = data[DataCount / 2];
            else if (optionA == 2) target = data[0];
            else target = data[DataCount - 1] + 1;
            resultIndex = -1;
        }

        private void Add(int row, int kind, int i, int j, string note)
        {
            SearchOp op = new SearchOp();
            op.Kind = kind; op.I = i; op.J = j; op.Note = note;
            ops[row].Add(op);
        }

        private void BuildOps()
        {
            for (int r = 0; r < 3; r++)
            {
                ops[r] = new List<SearchOp>();
                probes[r] = 0;
                done[r] = false;
            }
            // 线性
            int found = -1;
            for (int i = 0; i < DataCount; i++)
            {
                Add(0, 0, i, -1, "看第 " + i + " 个：" + data[i]);
                if (data[i] == target) { found = i; break; }
            }
            Add(0, found >= 0 ? 4 : 5, found, -1, found >= 0
                ? "线性找到 " + target + " 在第 " + found + " 个位置"
                : "线性扫到末尾也没找到 " + target);
            // 二分
            int left = 0, right = DataCount - 1;
            int hit = -1;
            while (left <= right)
            {
                int mid = (left + right) / 2;
                Add(1, 0, mid, right - left + 1, "取中点 " + mid + "：" + data[mid]);
                if (data[mid] == target) { hit = mid; break; }
                if (data[mid] < target)
                {
                    Add(1, 1, mid + 1, right, "比目标小，区间收成 [" + (mid + 1) + "," + right + "]");
                    left = mid + 1;
                }
                else
                {
                    Add(1, 1, left, mid - 1, "比目标大，区间收成 [" + left + "," + (mid - 1) + "]");
                    right = mid - 1;
                }
            }
            Add(1, hit >= 0 ? 4 : 5, hit, -1, hit >= 0
                ? "二分找到 " + target + " 在第 " + hit + " 个位置"
                : "区间空了，二分判定 " + target + " 不在表里");
            // 哈希
            int bucket = target % BucketCount;
            if (bucket < 0) bucket += BucketCount;
            Add(2, 2, bucket, -1, "算哈希：h(" + target + ") = " + target + " mod " + BucketCount
                + " = " + bucket);
            int pos = -1;
            for (int k = 0; k < chain[bucket].Count; k++)
            {
                Add(2, 3, bucket, k, "沿桶 " + bucket + " 的链比较第 " + k + " 个键：" + chain[bucket][k]);
                if (chain[bucket][k] == target) { pos = k; break; }
            }
            Add(2, pos >= 0 ? 4 : 5, bucket, pos, pos >= 0
                ? "哈希找到 " + target + "：桶 " + bucket + " 的链上第 " + pos + " 个"
                : "桶 " + bucket + " 的链走完也没找到 " + target);
        }

        private void Play()
        {
            int maxSteps = 0;
            for (int r = 0; r < 3; r++) if (ops[r].Count > maxSteps) maxSteps = ops[r].Count;
            for (int k = 0; k < maxSteps; k++)
            {
                Begin();
                for (int r = 0; r < 3; r++)
                {
                    if (k >= ops[r].Count)
                    {
                        if (!done[r]) { done[r] = true; finishedRows++; }
                        continue;
                    }
                    Apply(r, ops[r][k]);
                }
                Primary = probes[0] + probes[1] + probes[2];
                Secondary = binaryWidth;
                Work = probes[0] + probes[1] + probes[2];
                Pending = 3 - finishedRows;
                CustomA = probes[0];
                CustomB = probes[1];
                StateCode = finishedRows >= 3 ? 2 : 1;
                Say(NoteOf(k));
                End();
            }
            Finished = true;
            HasResult = true;
            Begin();
            Primary = probes[0] + probes[1] + probes[2];
            Secondary = binaryWidth;
            Work = Primary;
            Pending = 0;
            CustomA = probes[0];
            CustomB = probes[1];
            StateCode = 3;
            Say("三种查找都停了：线性 " + probes[0] + " 次比较、二分 " + probes[1]
                + " 次比较、哈希 " + probes[2] + " 次比较（目标 " + target + "）");
            End();
        }

        private string NoteOf(int k)
        {
            string text = "";
            for (int r = 0; r < 3; r++)
            {
                if (k >= ops[r].Count) continue;
                if (text.Length > 0) text += "；";
                text += RowNames[r] + " " + ops[r][k].Note;
            }
            return text;
        }

        private void Apply(int row, SearchOp op)
        {
            int baseIndex = row == 0 ? LinBase : (row == 1 ? BinBase : BucketBase);
            switch (op.Kind)
            {
                case 0:
                    probes[row]++;
                    if (row == 1)
                    {
                        // 二分：区间外的格子标成「已跳过」，区间内的留在待处理
                        for (int i = 0; i < DataCount; i++)
                        {
                            if (i < lo[1] || i > hi[1]) continue;
                            Set(BinBase + i, i == op.I ? St.Active : St.Pending);
                        }
                        lo[1] = Math.Min(lo[1], op.I);
                        hi[1] = Math.Max(hi[1], op.I);
                        binaryWidth = hi[1] - lo[1] + 1;
                    }
                    else
                    {
                        if (row == 0)
                        {
                            // 「扫过但被否掉」记成已处理（蓝），**不要记成「已跳过」**：
                            // 二分那边的「已跳过」是「区间之外」，同一个颜色不能有两种意思。
                            if (op.I > 0) Set(LinBase + op.I - 1, St.Done);
                            Set(LinBase + op.I, St.Active);
                        }
                    }
                    break;
                case 1:
                    if (row == 1)
                    {
                        for (int i = 0; i < DataCount; i++)
                        {
                            if (i < op.I || i > op.J) Set(BinBase + i, St.Cut);
                        }
                        lo[1] = op.I;
                        hi[1] = op.J;
                        binaryWidth = op.J - op.I + 1;
                        if (binaryWidth < 1) binaryWidth = 0;
                    }
                    break;
                case 2:
                    probes[row]++;
                    Set(BucketBase + op.I, St.Active);
                    for (int k = 0; k < ChainMax; k++)
                    {
                        if (k < chain[op.I].Count)
                        {
                            Set(ChainBase + k, St.Pending);
                            A(ChainBase + k, 0, chain[op.I][k]);
                            A(ChainBase + k, 1, k);
                        }
                        else
                        {
                            Set(ChainBase + k, St.Empty);
                            A(ChainBase + k, 0, -1);
                        }
                    }
                    break;
                case 3:
                    probes[row]++;
                    chainAt[row] = op.J;
                    Set(BucketBase + op.I, St.Marked);
                    for (int k = 0; k < ChainMax; k++)
                    {
                        if (k >= chain[op.I].Count) continue;
                        Set(ChainBase + k, k == op.J ? St.Active : St.Pending);
                    }
                    break;
                case 4:
                    if (row == 0) { Set(LinBase + op.I, St.Result); resultIndex = op.I; }
                    else if (row == 1) { Set(BinBase + op.I, St.Result); resultIndex = op.I; }
                    else
                    {
                        Set(BucketBase + op.I, St.Result);
                        for (int k = 0; k < ChainMax; k++)
                            if (k < chain[op.I].Count) Set(ChainBase + k, k == op.J ? St.Result : St.Cut);
                    }
                    break;
                case 5:
                    if (row == 0)
                        for (int i = 0; i < DataCount; i++) Set(LinBase + i, St.Cut);
                    else if (row == 1)
                        for (int i = 0; i < DataCount; i++) Set(BinBase + i, St.Cut);
                    else
                    {
                        Set(BucketBase + op.I, St.Cut);
                        for (int k = 0; k < ChainMax; k++)
                            if (k < chain[op.I].Count) Set(ChainBase + k, St.Cut);
                    }
                    break;
            }
        }

        public int Probes(int row) { return probes[row]; }

        public int Target { get { return target; } }

        public int ResultIndex { get { return resultIndex; } }

        public override string Title()
        {
            string goal = optionA == 0 ? "中间那个值" : (optionA == 1 ? "不存在的值" : "第一个值");
            return "查找对照：同一批 " + DataCount + " 个有序键上找「" + goal + "」（目标 "
                   + target + "）　线性／二分／哈希并排推进";
        }

        public override string StatusText()
        {
            return "比较次数：线性 " + probes[0] + "、二分 " + probes[1] + "、哈希 " + probes[2]
                   + "　二分区间宽 " + binaryWidth + "　哈希桶数 " + BucketCount
                   + "　目标 " + target + (resultIndex >= 0 ? "（在下标 " + resultIndex + "）" : "（不在表里）");
        }

        public override string Detail()
        {
            return "target=" + target + " linear=" + probes[0] + " binary=" + probes[1]
                   + " hash=" + probes[2] + " result=" + resultIndex
                   + " chains=" + ChainSizes();
        }

        private string ChainSizes()
        {
            string text = "";
            for (int i = 0; i < BucketCount; i++)
            {
                if (i > 0) text += ",";
                text += chain[i].Count;
            }
            return text;
        }
    }

    // ───────────────────────────────────────────────────────────── 场景 5 动态规划

    /// <summary>
    /// 状态表一格一格填，当前格的转移来源用折线连出来。
    /// 选项二换成滚动数组：只留上一行与当前行，格数从 64 掉到 16，答案不变。
    /// </summary>
    internal sealed class DpModel : AlgoModel
    {
        public const int Rows = 8;
        public const int Cols = 8;
        public const int TableBase = 0;
        public const int RollBase = 64;
        public const int RollRows = 2;

        private int optionA, optionB, seed;
        private string textX = "", textY = "";
        private readonly int[] table = new int[Rows * Cols];
        private readonly int[] roll = new int[RollRows * Cols];
        private int filled, compares, result;
        private int currentI, currentJ;
        private int prevCurrent = -1;
        private readonly int[] prevSources = new int[3];
        private int prevSourceCount;

        public override void Build(int a, int b, int seedValue)
        {
            optionA = Clamp(a, 0, 1);
            optionB = Clamp(b, 0, 1);
            seed = seedValue;
            Trace.Init.Clear();
            Trace.Steps.Clear();
            filled = 0; compares = 0; result = 0; currentI = 0; currentJ = 0;
            PickStrings();
            InitCell(AlgoConst.InputBase, St.Data, 0, 0, -1);
            for (int j = 1; j < Cols; j++) InitCell(AlgoConst.InputBase + j, St.Data, textX[j - 1], j, -1);
            InitCell(AlgoConst.InputBase + Cols, St.Data, 0, 0, -1);
            for (int i = 1; i < Rows; i++)
                InitCell(AlgoConst.InputBase + Cols + i, St.Data, textY[i - 1], i, -1);
            for (int i = 0; i < Rows; i++)
                for (int j = 0; j < Cols; j++)
                {
                    table[i * Cols + j] = -1;
                    InitCell(TableBase + i * Cols + j, St.Empty, -1, i, j);
                }
            for (int i = 0; i < RollRows * Cols; i++) InitCell(RollBase + i, St.Empty, -1, i / Cols, i % Cols);
            if (optionB == 0) FillFull();
            else FillRolling();
            prevCurrent = -1;
            prevSourceCount = 0;
            Finished = true;
            HasResult = true;
            CustomA = result;
            CustomB = optionB == 0 ? 0 : Rows * Cols - RollRows * Cols;
            Begin();
            Set(optionB == 0 ? TableBase + (Rows - 1) * Cols + (Cols - 1) : RollBase + Cols + (Cols - 1),
                St.Result);
            Primary = filled;
            Secondary = result;
            Work = compares;
            Pending = 0;
            StateCode = 3;
            Say("表填完：" + (optionA == 0 ? "最长公共子序列长度 " : "编辑距离 ") + result
                + "　填了 " + filled + " 格　比较 " + compares + " 次"
                + (optionB == 1 ? "　滚动数组只用了 " + (RollRows * Cols) + " 格" : ""));
            End();
        }

        private void PickStrings()
        {
            // 四个组合都是 7 个字符，表因此固定 8×8；前两组给 LCS，后两组给编辑距离
            string[] xs = { "ABCBDAB", "AGGTABA", "XMJYAUZ", "KITTENS" };
            string[] ys = { "BDCABAB", "GXTXAYB", "MZJAWXU", "SITTING" };
            int pick = (seed % 2 == 0 ? 0 : 1) + (optionA == 1 ? 2 : 0);
            textX = xs[pick];
            textY = ys[pick];
        }

        /// <summary>完整二维表：一格一步，每步把转移来源指出来。</summary>
        private void FillFull()
        {
            // 边界：LCS 的边界是 0；编辑距离的边界是「空串变成这么长要几步」
            for (int j = 0; j < Cols; j++)
                EmitCell(0, j, optionA == 1 ? j : 0,
                         optionA == 1 ? "边界：空串变成 " + j + " 个字符要 " + j + " 次插入"
                                      : "边界：空串与任何前缀的答案是 0");
            for (int i = 1; i < Rows; i++)
            {
                EmitCell(i, 0, optionA == 1 ? i : 0,
                         optionA == 1 ? "边界：空串变成 " + i + " 个字符要 " + i + " 次插入"
                                      : "边界：空串与任何前缀的答案是 0");
                for (int j = 1; j < Cols; j++)
                {
                    int best;
                    string how;
                    Compute(i, j, out best, out how);
                    EmitCell(i, j, best, how);
                }
            }
            result = table[(Rows - 1) * Cols + (Cols - 1)];
        }

        private void EmitCell(int i, int j, int value, string how)
        {
            table[i * Cols + j] = value;
            filled++;
            currentI = i; currentJ = j;
            Begin();
            // 上一步指出的来源与上一步填的那一格都收回到「已确定」，
            // 否则整张表会一直是橙的、红的，看不出「这一格的来源是哪几格」。
            for (int k = 0; k < prevSourceCount; k++) Set(prevSources[k], St.Done);
            prevSourceCount = 0;
            if (prevCurrent >= 0) Set(prevCurrent, St.Done);
            Set(TableBase + i * Cols + j, St.Active);
            A(TableBase + i * Cols + j, 0, value);
            if (i > 0)
            {
                Set(TableBase + (i - 1) * Cols + j, St.Ref);
                prevSources[prevSourceCount++] = TableBase + (i - 1) * Cols + j;
            }
            if (j > 0)
            {
                Set(TableBase + i * Cols + (j - 1), St.Ref);
                prevSources[prevSourceCount++] = TableBase + i * Cols + (j - 1);
            }
            if (i > 0 && j > 0)
            {
                Set(TableBase + (i - 1) * Cols + (j - 1), St.Path);
                prevSources[prevSourceCount++] = TableBase + (i - 1) * Cols + (j - 1);
            }
            prevCurrent = TableBase + i * Cols + j;
            Primary = filled;
            Secondary = value;
            Work = compares;
            Pending = (Rows - 1) * (Cols - 1) - filled;
            Current = TableBase + i * Cols + j;
            Focus = TableBase;
            StateCode = 1;
            Say("填第 " + i + " 行第 " + j + " 列：" + how);
            End();
        }

        private void Compute(int i, int j, out int best, out string how)
        {
            compares++;
            bool same = textX[j - 1] == textY[i - 1];
            int di = table[(i - 1) * Cols + j];
            int le = table[i * Cols + (j - 1)];
            int dg = table[(i - 1) * Cols + (j - 1)];
            if (optionA == 0)
            {
                if (same)
                {
                    best = dg + 1;
                    how = "X[" + (j - 1) + "] 与 Y[" + (i - 1) + "] 都是 " + textX[j - 1]
                          + "：取左上角 " + dg + " 加一";
                }
                else if (di >= le)
                {
                    best = di;
                    how = "字符不同：上面 " + di + " 不小于左边 " + le + "，取上面";
                }
                else
                {
                    best = le;
                    how = "字符不同：左边 " + le + " 更大，取左边";
                }
            }
            else
            {
                if (same)
                {
                    best = dg;
                    how = "X[" + (j - 1) + "] 与 Y[" + (i - 1) + "] 都是 " + textX[j - 1]
                          + "：不用改，取左上角 " + dg;
                }
                else
                {
                    best = Math.Min(dg, Math.Min(di, le)) + 1;
                    how = "字符不同：改／删／插三种走法取最小的 "
                          + Math.Min(dg, Math.Min(di, le)) + " 加一";
                }
            }
        }

        /// <summary>滚动数组：只留上一行与当前行，一格一步。</summary>
        private void FillRolling()
        {
            int[] previous = new int[Cols];
            int[] current = new int[Cols];
            for (int j = 0; j < Cols; j++)
            {
                previous[j] = optionA == 1 ? j : 0;
                current[j] = 0;
                EmitRoll(0, j, previous[j], "上一行（第 0 行）的边界格");
            }
            for (int i = 1; i < Rows; i++)
            {
                current[0] = optionA == 1 ? i : 0;
                EmitRoll(1, 0, current[0], "第 " + i + " 行的边界格");
                for (int j = 1; j < Cols; j++)
                {
                    bool same = textX[j - 1] == textY[i - 1];
                    int best;
                    string how;
                    if (optionA == 0)
                    {
                        best = same ? previous[j - 1] + 1 : Math.Max(previous[j], current[j - 1]);
                        how = same ? "字符相同：左上角 " + previous[j - 1] + " 加一"
                                   : "字符不同：上面 " + previous[j] + " 与左边 " + current[j - 1] + " 取大";
                    }
                    else
                    {
                        best = same ? previous[j - 1]
                                    : Math.Min(previous[j - 1], Math.Min(previous[j], current[j - 1])) + 1;
                        how = same ? "字符相同：取左上角 " + previous[j - 1]
                                   : "字符不同：三种走法取最小加一";
                    }
                    compares++;
                    current[j] = best;
                    EmitRoll(1, j, best, how);
                }
                Array.Copy(current, previous, Cols);
                result = current[Cols - 1];
            }
            result = previous[Cols - 1];
        }

        private void EmitRoll(int row, int j, int value, string how)
        {
            roll[row * Cols + j] = value;
            filled++;
            Begin();
            for (int k = 0; k < prevSourceCount; k++) Set(prevSources[k], St.Done);
            prevSourceCount = 0;
            if (prevCurrent >= 0) Set(prevCurrent, St.Done);
            Set(RollBase + row * Cols + j, St.Active);
            A(RollBase + row * Cols + j, 0, value);
            if (row == 1)
            {
                if (j > 0)
                {
                    Set(RollBase + 0 * Cols + (j - 1), St.Path);
                    prevSources[prevSourceCount++] = RollBase + 0 * Cols + (j - 1);
                }
                Set(RollBase + 0 * Cols + j, St.Ref);
                prevSources[prevSourceCount++] = RollBase + 0 * Cols + j;
                if (j > 0)
                {
                    Set(RollBase + 1 * Cols + (j - 1), St.Ref);
                    prevSources[prevSourceCount++] = RollBase + 1 * Cols + (j - 1);
                }
            }
            prevCurrent = RollBase + row * Cols + j;
            Primary = filled;
            Secondary = value;
            Work = compares;
            Current = RollBase + row * Cols + j;
            Focus = RollBase;
            StateCode = 1;
            Say("滚动数组第 " + (row == 0 ? "上一行" : "当前行") + "第 " + j + " 格：" + how
                + "（只留两行，" + (Rows * Cols) + " 格变 " + (RollRows * Cols) + " 格）");
            End();
        }

        public int Answer { get { return result; } }

        public int Filled { get { return filled; } }

        public int Cell(int i, int j) { return table[i * Cols + j]; }

        public string TextX { get { return textX; } }

        public string TextY { get { return textY; } }

        public override string Title()
        {
            return (optionA == 0 ? "动态规划：最长公共子序列" : "动态规划：编辑距离")
                   + "　X = " + textX + "　Y = " + textY + "　"
                   + (optionB == 0 ? "完整二维表 " + (Rows * Cols) + " 格" : "滚动数组 16 格");
        }

        public override string StatusText()
        {
            return "已填 " + filled + " 格　转移比较 " + compares + " 次　当前格的值 "
                   + (filled > 0 ? Secondary : 0) + "　最终答案 " + (HasResult ? result : -1)
                   + "　" + (optionB == 1 ? "滚动数组省下 " + (Rows * Cols - RollRows * Cols) + " 格" : "完整表");
        }

        public override string Detail()
        {
            return "textX=" + textX + " textY=" + textY + " filled=" + filled
                   + " compares=" + compares + " result=" + result;
        }
    }

    // ───────────────────────────────────────────────────────────── 场景 6 字符串匹配

    /// <summary>
    /// 朴素匹配的回溯与 KMP 失配函数的滑动并排跑：两行文本、两行模式，
    /// 外加一张失配表——表本身也是一步一步算出来的。
    /// </summary>
    internal sealed class MatchModel : AlgoModel
    {
        public const int TextLen = 16;
        public const int PatLen = 4;
        public const int NaiveTextBase = 0;
        public const int NaivePatBase = 16;
        public const int KmpTextBase = 20;
        public const int KmpPatBase = 36;
        public const int FailCharBase = 40;
        public const int FailValueBase = 44;

        private int optionA, optionB, seed;
        private string text = "", pattern = "";
        private readonly int[] fail = new int[PatLen];
        private readonly List<MatchOp>[] ops = new List<MatchOp>[2];
        private readonly int[] compares = new int[2];
        private readonly int[] shifts = new int[2];
        private readonly int[] align = new int[2];
        private readonly int[] patPos = new int[2];
        private readonly int[] textPos = new int[2];
        private readonly int[] hitAt = new int[2];
        private readonly bool[] done = new bool[2];
        private int finishedRows, failFilled;

        internal sealed class MatchOp
        {
            public int Kind;    // 0 比较，1 滑动，2 命中，3 未找到
            public int TextPos, PatPos, Align;
            public string Note = "";
        }

        public override void Build(int a, int b, int seedValue)
        {
            optionA = Clamp(a, 0, 2);
            optionB = Clamp(b, 0, 2);
            seed = seedValue;
            Trace.Init.Clear();
            Trace.Steps.Clear();
            compares[0] = 0; compares[1] = 0; shifts[0] = 0; shifts[1] = 0;
            hitAt[0] = -1; hitAt[1] = -1; finishedRows = 0; failFilled = 0;
            done[0] = false; done[1] = false;
            align[0] = 0; align[1] = 0; patPos[0] = 0; patPos[1] = 0; textPos[0] = 0; textPos[1] = 0;
            PickStrings();
            for (int i = 0; i < TextLen; i++)
            {
                InitCell(NaiveTextBase + i, St.Data, text[i], i, -1);
                InitCell(KmpTextBase + i, St.Data, text[i], i, -1);
            }
            for (int i = 0; i < PatLen; i++)
            {
                InitCell(NaivePatBase + i, St.Data, pattern[i], i, -1);
                InitCell(KmpPatBase + i, St.Data, pattern[i], i, -1);
                InitCell(FailCharBase + i, St.Data, pattern[i], i, -1);
                InitCell(FailValueBase + i, St.Empty, -1, i, -1);
                fail[i] = 0;
            }
            EmitFailTable();
            BuildNaive();
            BuildKmp();
            Play();
        }

        private void PickStrings()
        {
            if (optionA == 0) { text = "ACGTACGTACGTACGT"; pattern = "GTAC"; }
            else if (optionA == 1) { text = "AAAAAAAAAAAAAAAA"; pattern = "AAAB"; }
            else { text = "ABABABABABABABAB"; pattern = "ABAB"; }
        }

        /// <summary>失配表按定义算：fail[i] 是最长的「既是前缀又是后缀」的长度。</summary>
        private void EmitFailTable()
        {
            fail[0] = 0;
            failFilled = 1;
            Begin();
            Set(FailValueBase + 0, St.Done);
            A(FailValueBase + 0, 0, 0);
            A(FailCharBase + 0, 2, 0);
            Primary = failFilled;
            Work = 0;
            Pending = PatLen - failFilled;
            Current = FailValueBase;
            StateCode = 1;
            Say("失配表：fail[0] 规定为 0（长度为 1 的前缀没有真前缀）");
            End();
            for (int i = 1; i < PatLen; i++)
            {
                int best = 0;
                for (int k = i; k >= 1; k--)
                {
                    bool ok = true;
                    for (int t = 0; t < k; t++)
                    {
                        if (pattern[t] != pattern[i - k + 1 + t]) { ok = false; break; }
                    }
                    if (ok) { best = k; break; }
                }
                fail[i] = best;
                failFilled++;
                Begin();
                Set(FailValueBase + i, St.Done);
                A(FailValueBase + i, 0, best);
                A(FailCharBase + i, 2, best);
                Primary = failFilled;
                Pending = PatLen - failFilled;
                Current = FailValueBase + i;
                StateCode = 1;
                Say("失配表：fail[" + i + "] = " + best + "（" + pattern.Substring(0, i + 1)
                    + " 的最长「前缀＝后缀」长度）");
                End();
            }
            Begin();
            for (int i = 0; i < PatLen; i++) Set(FailValueBase + i, St.Result);
            Primary = failFilled;
            Pending = 0;
            StateCode = 2;
            Say("失配表算完：" + FailText() + "——匹配时失配就从这里取下一个位置");
            End();
        }

        private string FailText()
        {
            string body = "";
            for (int i = 0; i < PatLen; i++)
            {
                if (i > 0) body += " ";
                body += fail[i].ToString(CultureInfo.InvariantCulture);
            }
            return "fail = [" + body + "]";
        }

        private void BuildNaive()
        {
            ops[0] = new List<MatchOp>();
            int s = 0;
            while (s <= TextLen - PatLen)
            {
                int j = 0;
                while (j < PatLen && text[s + j] == pattern[j])
                {
                    Add(0, 0, s + j, j, s, "相同：" + pattern[j] + " 对上了，往后比");
                    j++;
                }
                if (j == PatLen)
                {
                    Add(0, 2, s + PatLen - 1, PatLen - 1, s, "整段对上，朴素匹配在第 " + s + " 位找到");
                    break;
                }
                Add(0, 0, s + j, j, s, "不同：" + text[s + j] + " ≠ " + pattern[j]
                    + "，朴素匹配从头再来（模式退回第 0 位）");
                s++;
                if (s <= TextLen - PatLen)
                    Add(0, 1, s, 0, s, "对齐位置滑到 " + s + "，模式回到第 0 位");
            }
            if (ops[0].Count == 0 || ops[0][ops[0].Count - 1].Kind != 2)
                Add(0, 3, -1, -1, -1, "文本走完也没匹配上");
        }

        private void BuildKmp()
        {
            ops[1] = new List<MatchOp>();
            // 文本指针 i 只往前走；模式指针 j 失配时退到 fail[j-1]。
            // 对齐位置是 i − j，因此「KMP 不回退文本」在画面上就是对齐位置一次滑好几格。
            int i = 0, j = 0;
            while (i < TextLen)
            {
                if (text[i] == pattern[j])
                {
                    Add(1, 0, i, j, i - j, "相同：" + pattern[j] + " 对上了");
                    i++;
                    j++;
                    if (j == PatLen)
                    {
                        Add(1, 2, i - 1, PatLen - 1, i - j, "整段对上，KMP 在第 " + (i - j) + " 位找到");
                        break;
                    }
                }
                else
                {
                    Add(1, 0, i, j, i - j, "不同：" + text[i] + " ≠ " + pattern[j]);
                    if (j == 0)
                    {
                        i++;
                        if (i < TextLen)
                            Add(1, 1, i, 0, i, "模式第 0 位就不同，文本指针往前走一位");
                    }
                    else
                    {
                        int from = j - 1;
                        j = fail[from];
                        Add(1, 1, i, j, i - j, "KMP 不回退文本：模式退到 fail[" + from
                            + "] = " + j + "，对齐位置滑到 " + (i - j));
                    }
                }
            }
            if (ops[1].Count == 0 || ops[1][ops[1].Count - 1].Kind != 2)
                Add(1, 3, -1, -1, -1, "文本走完也没匹配上");
        }

        private void Add(int row, int kind, int textPos, int patPos, int alignValue, string note)
        {
            MatchOp op = new MatchOp();
            op.Kind = kind; op.TextPos = textPos; op.PatPos = patPos; op.Align = alignValue; op.Note = note;
            ops[row].Add(op);
        }

        private void Play()
        {
            int rows = optionB == 0 ? 2 : 1;
            int maxSteps = 0;
            for (int r = 0; r < 2; r++) if (ops[r].Count > maxSteps) maxSteps = ops[r].Count;
            for (int k = 0; k < maxSteps; k++)
            {
                Begin();
                for (int r = 0; r < 2; r++)
                {
                    if (optionB == 1 && r != 0) continue;
                    if (optionB == 2 && r != 1) continue;
                    if (k >= ops[r].Count)
                    {
                        if (!done[r]) { done[r] = true; finishedRows++; }
                        continue;
                    }
                    Apply(r, ops[r][k]);
                }
                Primary = compares[0] + compares[1];
                Secondary = shifts[0] + shifts[1];
                Work = Primary;
                Pending = rows - CountDone();
                CustomA = compares[0];
                CustomB = compares[1];
                StateCode = finishedRows >= rows ? 2 : 1;
                Say(NoteOf(k));
                End();
            }
            Finished = true;
            HasResult = true;
            Begin();
            Primary = compares[0] + compares[1];
            Secondary = shifts[0] + shifts[1];
            Work = Primary;
            Pending = 0;
            CustomA = compares[0];
            CustomB = compares[1];
            StateCode = 3;
            Say("两种匹配都停了：朴素比较 " + compares[0] + " 次、KMP 比较 " + compares[1]
                + " 次；找到的位置 " + (hitAt[0] >= 0 ? hitAt[0].ToString(CultureInfo.InvariantCulture) : "无")
                + " / " + (hitAt[1] >= 0 ? hitAt[1].ToString(CultureInfo.InvariantCulture) : "无"));
            End();
        }

        private int CountDone()
        {
            int total = 0;
            if (optionB == 1) return done[0] ? 1 : 0;
            if (optionB == 2) return done[1] ? 1 : 0;
            if (done[0]) total++;
            if (done[1]) total++;
            return total;
        }

        private string NoteOf(int k)
        {
            string body = "";
            for (int r = 0; r < 2; r++)
            {
                if (optionB == 1 && r != 0) continue;
                if (optionB == 2 && r != 1) continue;
                if (k >= ops[r].Count) continue;
                if (body.Length > 0) body += "；";
                body += (r == 0 ? "朴素 " : "KMP ") + ops[r][k].Note;
            }
            return body;
        }

        private void Apply(int row, MatchOp op)
        {
            int textBase = row == 0 ? NaiveTextBase : KmpTextBase;
            int patBase = row == 0 ? NaivePatBase : KmpPatBase;
            int oldAlign = align[row];
            switch (op.Kind)
            {
                case 0:
                    compares[row]++;
                    textPos[row] = op.TextPos;
                    patPos[row] = op.PatPos;
                    align[row] = op.Align;
                    Repaint(row, textBase, patBase);
                    Set(textBase + op.TextPos, St.Active);
                    Set(patBase + op.PatPos, St.Active);
                    break;
                case 1:
                    shifts[row]++;
                    align[row] = op.Align;
                    patPos[row] = op.PatPos;
                    textPos[row] = op.TextPos;
                    if (oldAlign >= 0)
                    {
                        for (int i = 0; i < PatLen; i++)
                        {
                            int cell = patBase + i;
                            Set(cell, i < op.PatPos ? St.Cut : St.Data);
                        }
                    }
                    Repaint(row, textBase, patBase);
                    break;
                case 2:
                    hitAt[row] = op.Align;
                    align[row] = op.Align;
                    Repaint(row, textBase, patBase);
                    for (int i = 0; i < PatLen; i++)
                    {
                        Set(textBase + op.Align + i, St.Result);
                        Set(patBase + i, St.Result);
                    }
                    break;
                case 3:
                    Repaint(row, textBase, patBase);
                    for (int i = 0; i < TextLen; i++) Set(textBase + i, St.Cut);
                    break;
            }
            // 对齐位置写进这一行第一个文本格的属性 3，绘制侧直接读它摆模式
            A(textBase, 3, align[row] < 0 ? 0 : align[row]);
        }

        /// <summary>把这一行的文本与模式按当前对齐位置重新上一遍色。</summary>
        private void Repaint(int row, int textBase, int patBase)
        {
            for (int i = 0; i < TextLen; i++)
            {
                bool inWindow = i >= align[row] && i < align[row] + PatLen;
                Set(textBase + i, inWindow ? St.Pending : St.Data);
            }
            for (int i = 0; i < PatLen; i++)
            {
                bool inWindow = align[row] + i < TextLen;
                Set(patBase + i, inWindow ? St.Data : St.Cut);
            }
        }

        public int Compares(int row) { return compares[row]; }

        public int HitAt(int row) { return hitAt[row]; }

        public int FailValue(int i) { return fail[i]; }

        public string Pattern { get { return pattern; } }

        public string Text { get { return text; } }

        public override string Title()
        {
            string which = optionB == 0 ? "朴素与 KMP 同时跑" : (optionB == 1 ? "只看朴素" : "只看 KMP");
            return "字符串匹配：" + which + "　文本 " + text + "　模式 " + pattern
                   + "　" + FailText();
        }

        public override string StatusText()
        {
            return "比较次数：朴素 " + compares[0] + "、KMP " + compares[1] + "　滑动次数：朴素 "
                   + shifts[0] + "、KMP " + shifts[1] + "　命中位置："
                   + (hitAt[0] >= 0 ? hitAt[0].ToString(CultureInfo.InvariantCulture) : "无") + "／"
                   + (hitAt[1] >= 0 ? hitAt[1].ToString(CultureInfo.InvariantCulture) : "无")
                   + "　失配表 " + FailText();
        }

        public override string Detail()
        {
            return "text=" + text + " pattern=" + pattern + " naive_cmp=" + compares[0]
                   + " kmp_cmp=" + compares[1] + " naive_shift=" + shifts[0] + " kmp_shift=" + shifts[1]
                   + " naive_hit=" + hitAt[0] + " kmp_hit=" + hitAt[1];
        }
    }

    // ───────────────────────────────────────────────────────────── 场景 7 状态机

    /// <summary>
    /// 剥 JSON 注释的状态机：同一个输入串上，switch 版与表驱动版一起往前走。
    /// 七个状态、六类字符，转移表 42 格全部画在画布上，当前那一格高亮。
    /// 输出行是**真剥出来的字符**：进注释的字符不进输出行。
    /// </summary>
    internal sealed class FsmModel : AlgoModel
    {
        public const int InputLen = 16;
        public const int InputBase = 0;
        public const int SwitchStateBase = 16;
        public const int TableStateBase = 23;
        public const int TableBase = 32;
        public const int OutBase = 80;
        public const int HistoryBase = 96;

        public const int StateCount = 7;
        public const int ClassCount = 6;

        private int optionA, optionB, seed;
        private string input = "";
        private readonly int[,] table = new int[StateCount, ClassCount];
        private readonly int[] history = new int[InputLen];
        private readonly StringBuilder output = new StringBuilder();
        private char pendingFinalSlash;
        private int switchState, tableState, readChars, outLen;
        private int switchMoves, tableMoves;

        public static readonly string[] StateNames =
        {
            "普通", "斜杠待定", "行注释", "块注释", "块注释见星", "字符串内", "串内转义"
        };

        public static readonly string[] ClassNames = { "斜杠 /", "星号 *", "引号 \"", "反斜杠 \\", "换行", "其它" };

        public override void Build(int a, int b, int seedValue)
        {
            optionA = Clamp(a, 0, 2);
            optionB = Clamp(b, 0, 2);
            seed = seedValue;
            Trace.Init.Clear();
            Trace.Steps.Clear();
            switchState = 0; tableState = 0; readChars = 0; outLen = 0;
            switchMoves = 0; tableMoves = 0;
            pendingFinalSlash = (char)0;
            output.Length = 0;
            PickInput();
            BuildTable();
            for (int i = 0; i < InputLen; i++)
            {
                InitCell(InputBase + i, St.Data, input[i], i, -1);
                InitCell(OutBase + i, St.Empty, 0, i, -1);
                InitCell(HistoryBase + i, St.Empty, -1, i, -1);
            }
            for (int s = 0; s < StateCount; s++)
            {
                InitCell(SwitchStateBase + s, St.Data, s, 0, -1);
                InitCell(TableStateBase + s, St.Data, s, 0, -1);
                for (int c = 0; c < ClassCount; c++)
                    InitCell(TableBase + s * ClassCount + c, St.Data, table[s, c], s, c);
            }
            InitSet(SwitchStateBase, St.Active);
            InitSet(TableStateBase, St.Active);
            Run();
        }

        private void PickInput()
        {
            if (optionA == 0) input = "{\"a\":1,/*c*/\"b\"}";
            else if (optionA == 1) input = "{\"a\":\"x//y\",1:2}";
            else input = "{\"a\":1}/*open   ";
        }

        /// <summary>转移表：横轴是字符类别，纵轴是当前状态，表里存下一个状态。</summary>
        private void BuildTable()
        {
            for (int c = 0; c < ClassCount; c++)
            {
                table[0, c] = c == 0 ? 1 : (c == 2 ? 5 : 0);
                table[1, c] = c == 0 ? 2 : (c == 1 ? 3 : (c == 2 ? 5 : 0));
                table[2, c] = c == 4 ? 0 : 2;
                table[3, c] = c == 1 ? 4 : 3;
                table[4, c] = c == 0 ? 0 : (c == 1 ? 4 : 3);
                table[5, c] = c == 3 ? 6 : (c == 2 ? 0 : 5);
                table[6, c] = 5;
            }
            // 界面点过的格子：把那一格改成点出来的下一个状态
            for (int i = 0; i < StateCount * ClassCount; i++)
            {
                if (!edits.ContainsKey(i)) continue;
                table[i / ClassCount, i % ClassCount] = edits[i];
            }
        }

        /// <summary>翻转一格转移表：状态号加一（转一圈回到 0）。</summary>
        public override void Command(int op, int arg0, int arg1)
        {
            if (op != St.CmdToggle) return;
            int offset = arg0 - TableBase;
            if (offset < 0 || offset >= StateCount * ClassCount) return;
            int state = offset / ClassCount;
            int cls = offset % ClassCount;
            edits[offset] = (table[state, cls] + 1) % StateCount;
        }

        public override void ClearEdits()
        {
            edits.Clear();
        }

        private readonly Dictionary<int, int> edits = new Dictionary<int, int>();

        private static int ClassOf(char ch)
        {
            if (ch == '/') return 0;
            if (ch == '*') return 1;
            if (ch == '"') return 2;
            if (ch == '\\') return 3;
            if (ch == '\n') return 4;
            return 5;
        }

        /// <summary>switch 版：一层一层判下去。与转移表逐格同值，这一点由自检断言。</summary>
        private static int SwitchNext(int state, int cls)
        {
            switch (state)
            {
                case 0:
                    if (cls == 0) return 1;
                    if (cls == 2) return 5;
                    return 0;
                case 1:
                    if (cls == 0) return 2;
                    if (cls == 1) return 3;
                    if (cls == 2) return 5;
                    return 0;
                case 2:
                    return cls == 4 ? 0 : 2;
                case 3:
                    return cls == 1 ? 4 : 3;
                case 4:
                    if (cls == 0) return 0;
                    if (cls == 1) return 4;
                    return 3;
                case 5:
                    if (cls == 3) return 6;
                    if (cls == 2) return 0;
                    return 5;
                default:
                    return 5;
            }
        }

        private void Run()
        {
            int pendingSlash = -1;      // 斜杠待定时，那一个字符的位置
            for (int i = 0; i < InputLen; i++)
            {
                char ch = input[i];
                int cls = ClassOf(ch);
                int fromSwitch = switchState;
                int fromTable = tableState;
                int nextSwitch = SwitchNext(switchState, cls);
                int nextTable = table[tableState, cls];
                // 「斜杠待定」那一格：下一个字符若不是注释的开头，这个斜杠要补进输出。
                // 一个字符的去留要看下一个字符，这正是多出一个状态的原因。
                bool flushSlash = fromSwitch == 1 && (nextSwitch == 0 || nextSwitch == 5);
                bool emit = (fromSwitch == 0 && nextSwitch != 1)
                            || flushSlash
                            || fromSwitch == 5 || fromSwitch == 6;
                if (fromSwitch == 0 && nextSwitch == 1) pendingSlash = i;
                // 斜杠真的开了注释：待定的那一个也归注释，丢掉
                if (fromSwitch == 1 && (nextSwitch == 2 || nextSwitch == 3)) pendingSlash = -1;
                switchState = nextSwitch;
                tableState = nextTable;
                switchMoves++;
                tableMoves++;
                readChars++;
                history[i] = nextSwitch;
                Begin();
                if (flushSlash)
                {
                    EmitChar(input[pendingSlash]);
                    pendingSlash = -1;
                }
                if (emit) EmitChar(ch);
                for (int s = 0; s < StateCount; s++)
                {
                    Set(SwitchStateBase + s, s == switchState ? St.Active : St.Data);
                    Set(TableStateBase + s, s == tableState ? St.Active : St.Data);
                }
                Set(InputBase + i, St.Done);
                if (i + 1 < InputLen) Set(InputBase + i + 1, St.Pending);
                Set(TableBase + fromSwitch * ClassCount + cls, St.Marked);
                A(TableBase + fromSwitch * ClassCount + cls, 3, 1);
                Set(HistoryBase + i, St.Done);
                A(HistoryBase + i, 0, nextSwitch);
                Primary = readChars;
                Secondary = switchMoves + tableMoves;
                Work = Secondary;
                Pending = InputLen - readChars;
                Current = InputBase + i;
                StateCode = switchState;
                Say("读第 " + i + " 个字符「" + Show(ch) + "」（类别 " + ClassNames[cls] + "）："
                    + StateNames[fromSwitch] + " → " + StateNames[nextSwitch]
                    + (emit ? "　这个字符进输出" : "　这个字符在注释里，丢掉"));
                End();
            }
            if (pendingSlash >= 0) pendingFinalSlash = input[pendingSlash];
            Finished = true;
            HasResult = true;
            Begin();
            if (pendingFinalSlash != 0) EmitChar(pendingFinalSlash);
            Set(InputBase + InputLen - 1, St.Done);
            Primary = readChars;
            Secondary = switchMoves + tableMoves;
            Work = Secondary;
            Pending = 0;
            CustomA = switchMoves;
            CustomB = tableMoves;
            StateCode = switchState;
            Say("输入读完：状态停在「" + StateNames[switchState] + "」　两版转移各 " + switchMoves
                + " 次　输出 " + outLen + " 个字符：" + Output());
            End();
        }

        /// <summary>把一个字符写进输出行的下一格。**必须在一个 Begin/End 之间调用**。</summary>
        private void EmitChar(char ch)
        {
            if (outLen >= InputLen) return;
            Set(OutBase + outLen, St.Done);
            A(OutBase + outLen, 0, ch);
            A(OutBase + outLen, 3, outLen);
            output.Append(ch);
            outLen++;
        }

        private static string Show(char ch)
        {
            if (ch == '\n') return "\\n";
            if (ch == ' ') return "空格";
            return ch.ToString();
        }

        /// <summary>真剥出来的输出：与界面上输出行里那些格子逐字符对应。</summary>
        public string Output()
        {
            return output.ToString();
        }

        public string Input { get { return input; } }

        public int SwitchMoves { get { return switchMoves; } }

        public int TableMoves { get { return tableMoves; } }

        public int TableValue(int state, int cls) { return table[state, cls]; }

        public static int SwitchValue(int state, int cls) { return SwitchNext(state, cls); }

        public int OutLength { get { return outLen; } }

        /// <summary>参考实现：先标一遍哪些字符在注释里，再拼输出。与状态机是两条路。</summary>
        public static string ReferenceStrip(string text)
        {
            bool[] drop = new bool[text.Length];
            int i = 0;
            while (i < text.Length)
            {
                char ch = text[i];
                if (ch == '"')
                {
                    i++;
                    while (i < text.Length)
                    {
                        if (text[i] == '\\') { i += 2; continue; }
                        if (text[i] == '"') { i++; break; }
                        i++;
                    }
                    continue;
                }
                if (ch == '/' && i + 1 < text.Length && text[i + 1] == '/')
                {
                    while (i < text.Length && text[i] != '\n') { drop[i] = true; i++; }
                    continue;
                }
                if (ch == '/' && i + 1 < text.Length && text[i + 1] == '*')
                {
                    drop[i] = true;
                    i++;
                    drop[i] = true;
                    i++;
                    while (i < text.Length)
                    {
                        drop[i] = true;
                        if (text[i] == '*' && i + 1 < text.Length && text[i + 1] == '/')
                        {
                            drop[i + 1] = true;
                            i += 2;
                            break;
                        }
                        i++;
                    }
                    continue;
                }
                i++;
            }
            StringBuilder builder = new StringBuilder();
            for (int k = 0; k < text.Length; k++) if (!drop[k]) builder.Append(text[k]);
            return builder.ToString();
        }

        public override string Title()
        {
            return "状态机：剥 JSON 注释　" + (optionB == 0 ? "switch 与表驱动对照"
                   : (optionB == 1 ? "只看 switch" : "只看表驱动"))
                   + "　输入 " + input + "　" + StateCount + " 个状态 × " + ClassCount + " 类字符";
        }

        public override string StatusText()
        {
            return "已读 " + readChars + "／" + InputLen + " 个字符　当前状态「" + StateNames[switchState]
                   + "」　转移次数 switch " + switchMoves + "／表驱动 " + tableMoves
                   + "　输出 " + outLen + " 个字符：" + Output();
        }

        public override string Detail()
        {
            return "input=" + input + " output=" + Output() + " reference=" + ReferenceStrip(input)
                   + " switch_moves=" + switchMoves + " table_moves=" + tableMoves
                   + " out_len=" + outLen + " final_state=" + switchState;
        }
    }

    // ───────────────────────────────────────────────────────────── 场景 8 随机与采样

    /// <summary>
    /// 三个子场景都用直方图说话：
    ///   取模偏差 —— 同一个随机源上 rand()%n 与拒绝采样的桶计数差多少；
    ///   洗牌      —— Fisher-Yates 与「每张与随机一张交换」，看牌 1 落在各位置的次数；
    ///   蓄水池    —— 每个元素最终入选的次数，理论值是 轮数 × k / n。
    /// </summary>
    internal sealed class RandomModel : AlgoModel
    {
        public const int Bars = 24;
        public const int BarBase = 0;
        public const int BucketBase = 0;
        public const int ResBase = 12;

        private int optionA, optionB, seed;
        private int modulus = 6, range = 32;
        private int samples, rawDraws, rounds;
        private readonly int[] modCount = new int[6];
        private readonly int[] rejCount = new int[6];
        private readonly int[] shuffleA = new int[12];
        private readonly int[] shuffleB = new int[12];
        private readonly int[] reservoir = new int[3];
        private readonly int[] picked = new int[12];
        private int replaces, processed;

        public override void Build(int a, int b, int seedValue)
        {
            optionA = Clamp(a, 0, 2);
            optionB = Clamp(b, 0, 1);
            seed = seedValue;
            Trace.Init.Clear();
            Trace.Steps.Clear();
            samples = 0; rawDraws = 0; rounds = 0; replaces = 0; processed = 0;
            for (int i = 0; i < 6; i++) { modCount[i] = 0; rejCount[i] = 0; }
            for (int i = 0; i < 12; i++) { shuffleA[i] = 0; shuffleB[i] = 0; picked[i] = 0; }
            for (int i = 0; i < 3; i++) reservoir[i] = i + 1;
            if (optionA == 0) BuildModulo();
            else if (optionA == 1) BuildShuffle();
            else BuildReservoir();
        }

        // ── 取模偏差

        private void BuildModulo()
        {
            int total = optionB == 0 ? 480 : 4800;
            for (int i = 0; i < modulus; i++)
            {
                InitCell(BarBase + i, St.Data, i, 0, -1);
                InitCell(BarBase + 6 + i, St.Data, i, 0, -1);
            }
            for (int i = 12; i < Bars; i++) InitA(BarBase + i, 1, -2);
            DeterministicRng rng = new DeterministicRng((uint)(seed + 9001));
            for (int draw = 0; draw < total; draw++)
            {
                uint raw = rng.NextUInt();
                rawDraws++;
                int value = (int)(raw % (uint)range);
                int bucket = value % modulus;
                modCount[bucket]++;
                int rej = value;
                int guard = 0;
                while (rej >= range - (range % modulus) && guard++ < 64)
                {
                    rej = (int)(rng.NextUInt() % (uint)range);
                    rawDraws++;
                }
                int rejBucket = rej % modulus;
                rejCount[rejBucket]++;
                samples++;
                Begin();
                Set(BarBase + bucket, St.Active);
                A(BarBase + bucket, 1, modCount[bucket]);
                Set(BarBase + 6 + rejBucket, St.Marked);
                A(BarBase + 6 + rejBucket, 1, rejCount[rejBucket]);
                for (int i = 0; i < modulus; i++)
                {
                    if (i != bucket) Set(BarBase + i, St.Data);
                    if (i != rejBucket) Set(BarBase + 6 + i, St.Data);
                }
                Primary = samples;
                Secondary = total - samples;
                Work = rawDraws;
                Pending = total - samples;
                CustomA = Spread(modCount);
                CustomB = Spread(rejCount);
                StateCode = 1;
                Say("第 " + samples + " 次抽样：随机源给 " + value + "　取模法落桶 " + bucket
                    + "（计数 " + modCount[bucket] + "）　拒绝法落桶 " + rejBucket
                    + "（计数 " + rejCount[rejBucket] + "）");
                End();
            }
            Finished = true;
            HasResult = true;
            Begin();
            for (int i = 0; i < modulus; i++)
            {
                Set(BarBase + i, St.Done);
                Set(BarBase + 6 + i, St.Done);
            }
            Primary = samples;
            Secondary = 0;
            Work = rawDraws;
            Pending = 0;
            CustomA = Spread(modCount);
            CustomB = Spread(rejCount);
            StateCode = 3;
            Say("抽完 " + samples + " 次：取模法各桶计数差 " + Spread(modCount) + "（= 样本数 ÷ " + range
                + "），拒绝法差 " + Spread(rejCount) + "　原始随机数用了 " + rawDraws + " 个");
            End();
        }

        private static int Spread(int[] counts)
        {
            int low = int.MaxValue, high = int.MinValue;
            for (int i = 0; i < counts.Length; i++)
            {
                if (counts[i] < low) low = counts[i];
                if (counts[i] > high) high = counts[i];
            }
            return high - low;
        }

        /// <summary>
        /// 取模偏差的直接测法：前两个桶每个数多取一个余数，因此它们的平均计数
        /// 应当比后面四个桶高出「样本数 ÷ 随机源范围」那么多。返回这个差（千分之一）。
        /// </summary>
        public int ModuloBiasMilli()
        {
            int front = modCount[0] + modCount[1];
            int back = 0;
            for (int i = 2; i < modulus; i++) back += modCount[i];
            int samples = front + back;
            if (samples == 0) return 0;
            // 前 2 个桶的平均与后 4 个桶的平均之差，按样本数归一
            int difference = front * 2 - back;              // 通分后的差（前 2 桶 × 2 与后 4 桶）
            return difference * 1000 / (samples * 2);
        }

        /// <summary>同样口径的拒绝采样偏差，用来与取模法对照。</summary>
        public int RejectionBiasMilli()
        {
            int front = rejCount[0] + rejCount[1];
            int back = 0;
            for (int i = 2; i < modulus; i++) back += rejCount[i];
            int samples = front + back;
            if (samples == 0) return 0;
            int difference = front * 2 - back;
            return difference * 1000 / (samples * 2);
        }

        // ── 洗牌：三张牌的六种排列各出现多少次

        /// <summary>三张牌的排列名，下标就是字典序编号。</summary>
        public static readonly string[] PermNames = { "123", "132", "213", "231", "312", "321" };

        private static int Rank3(int[] p)
        {
            if (p[0] == 1) return p[1] == 2 ? 0 : 1;
            if (p[0] == 2) return p[1] == 1 ? 2 : 3;
            return p[1] == 1 ? 4 : 5;
        }

        /// <summary>
        /// 洗牌的均匀性。**单张牌落在哪个位置是看不出偏差的**（那个边缘分布本来就是均匀的），
        /// 偏差在排列的联合分布上：因此这里用三张牌，把六种排列各出现多少次画出来——
        /// 朴素交换给出的是 5/27 与 4/27 两种概率，Fisher-Yates 给出的是整齐的 1/6。
        /// </summary>
        private void BuildShuffle()
        {
            int total = optionB == 0 ? 600 : 3000;
            for (int i = 0; i < 12; i++)
            {
                InitCell(BarBase + i, St.Data, i % 6, 0, -1);
                InitA(BarBase + i, 3, i / 6);
            }
            DeterministicRng rng = new DeterministicRng((uint)(seed + 4242));
            for (int round = 0; round < total; round++)
            {
                int[] a = { 1, 2, 3 };
                for (int i = 2; i > 0; i--)
                {
                    int j = rng.Next(i + 1);
                    int t = a[i]; a[i] = a[j]; a[j] = t;
                    rawDraws++;
                }
                int rankA = Rank3(a);
                int[] b = { 1, 2, 3 };
                for (int i = 0; i < 3; i++)
                {
                    int j = rng.Next(3);
                    int t = b[i]; b[i] = b[j]; b[j] = t;
                    rawDraws++;
                }
                int rankB = Rank3(b);
                shuffleA[rankA]++;
                shuffleB[rankB]++;
                rounds++;
                Begin();
                for (int i = 0; i < 6; i++)
                {
                    Set(BarBase + i, i == rankA ? St.Active : St.Data);
                    A(BarBase + i, 1, shuffleA[i]);
                    Set(BarBase + 6 + i, i == rankB ? St.Marked : St.Data);
                    A(BarBase + 6 + i, 1, shuffleB[i]);
                }
                Primary = rounds;
                Secondary = total - rounds;
                Work = rawDraws;
                Pending = total - rounds;
                CustomA = Spread6(shuffleA);
                CustomB = Spread6(shuffleB);
                StateCode = 1;
                Say("第 " + rounds + " 轮：Fisher-Yates 洗出 " + PermNames[rankA]
                    + "，朴素交换洗出 " + PermNames[rankB]);
                End();
            }
            Finished = true;
            HasResult = true;
            Begin();
            for (int i = 0; i < 12; i++) Set(BarBase + i, St.Done);
            Primary = rounds;
            Secondary = 0;
            Work = rawDraws;
            Pending = 0;
            CustomA = Spread6(shuffleA);
            CustomB = Spread6(shuffleB);
            StateCode = 3;
            Say("洗了 " + rounds + " 轮：六种排列每种应当出现约 " + (rounds / 6) + " 次——"
                + "Fisher-Yates 的最多最少差 " + Spread6(shuffleA) + " 次，朴素交换差 "
                + Spread6(shuffleB) + " 次");
            End();
        }

        // ── 蓄水池抽样

        private void BuildReservoir()
        {
            int total = optionB == 0 ? 40 : 200;
            for (int i = 0; i < 12; i++) InitCell(BarBase + i, St.Data, i + 1, 0, -1);
            for (int i = 0; i < 3; i++) InitCell(ResBase + i, St.Data, i + 1, i, -1);
            DeterministicRng rng = new DeterministicRng((uint)(seed + 5150));
            for (int round = 0; round < total; round++)
            {
                int[] pool = new int[3];
                for (int i = 0; i < 3; i++) pool[i] = i + 1;
                for (int i = 3; i < 12; i++)
                {
                    int j = rng.Next(i + 1);
                    rawDraws++;
                    if (j < 3) { pool[j] = i + 1; replaces++; }
                }
                rounds++;
                processed += 12;
                for (int i = 0; i < 3; i++) picked[pool[i] - 1]++;
                Begin();
                for (int i = 0; i < 12; i++)
                {
                    bool inPool = false;
                    for (int k = 0; k < 3; k++) if (pool[k] == i + 1) inPool = true;
                    Set(BarBase + i, inPool ? St.Active : St.Data);
                    A(BarBase + i, 1, picked[i]);
                }
                for (int k = 0; k < 3; k++)
                {
                    Set(ResBase + k, St.Marked);
                    A(ResBase + k, 0, pool[k]);
                }
                Primary = rounds;
                Secondary = replaces;
                Work = rawDraws;
                Pending = total - rounds;
                CustomA = Spread(picked);
                CustomB = rounds * 3 / 12;
                StateCode = 1;
                Say("第 " + rounds + " 轮流过的元素：蓄水池里是 " + pool[0] + "、" + pool[1] + "、"
                    + pool[2] + "　累计替换 " + replaces + " 次");
                End();
            }
            Finished = true;
            HasResult = true;
            Begin();
            for (int i = 0; i < 12; i++) Set(BarBase + i, St.Done);
            for (int k = 0; k < 3; k++) Set(ResBase + k, St.Result);
            Primary = rounds;
            Secondary = replaces;
            Work = rawDraws;
            Pending = 0;
            CustomA = Spread(picked);
            CustomB = rounds * 3 / 12;
            StateCode = 3;
            Say("抽了 " + rounds + " 轮：" + rounds + " 轮 × 3 个名额 ÷ 12 个元素 = 每个元素应当入选 "
                + (rounds * 3 / 12) + " 次，实测最大最小差 " + Spread(picked));
            End();
        }

        public int ModCount(int i) { return modCount[i]; }

        public int RejCount(int i) { return rejCount[i]; }

        public int ModSpread { get { return Spread(modCount); } }

        public int RejSpread { get { return Spread(rejCount); } }

        /// <summary>Fisher-Yates 那六根柱子的最多最少差。</summary>
        public int UniformSpread() { return Spread6(shuffleA); }

        /// <summary>朴素交换那六根柱子的最多最少差。</summary>
        public int NaiveSpread() { return Spread6(shuffleB); }

        /// <summary>只数前六格：排列只有六种，后六格是留给另一档的。</summary>
        private static int Spread6(int[] counts)
        {
            int low = int.MaxValue, high = int.MinValue;
            for (int i = 0; i < 6; i++)
            {
                if (counts[i] < low) low = counts[i];
                if (counts[i] > high) high = counts[i];
            }
            return high - low;
        }

        /// <summary>洗牌那一档的噪声尺度：每种排列出现次数的标准差。</summary>
        public double ShuffleSigma()
        {
            return Math.Sqrt(rounds * (1.0 / 6.0) * (5.0 / 6.0));
        }

        public string ShuffleSigmaText()
        {
            return ShuffleSigma().ToString("0.0", CultureInfo.InvariantCulture);
        }

        public int PermCountA(int rank) { return shuffleA[rank]; }

        public int PermCountB(int rank) { return shuffleB[rank]; }

        /// <summary>蓄水池那一档的容差：六倍标准差，超出就说明分布不对。</summary>
        public int SigmaBound()
        {
            double p = 3.0 / 12.0;
            double sd = Math.Sqrt(rounds * p * (1 - p));
            return (int)Math.Ceiling(sd * 6);
        }

        public string SigmaText()
        {
            double p = 3.0 / 12.0;
            double sd = Math.Sqrt(rounds * p * (1 - p));
            return sd.ToString("0.0", CultureInfo.InvariantCulture);
        }

        public int PickedMin()
        {
            int low = int.MaxValue;
            for (int i = 0; i < 12; i++) if (picked[i] < low) low = picked[i];
            return low;
        }

        public int PickedMax()
        {
            int high = 0;
            for (int i = 0; i < 12; i++) if (picked[i] > high) high = picked[i];
            return high;
        }

        public int Rounds { get { return rounds; } }

        public int RawDraws { get { return rawDraws; } }

        /// <summary>随机源的取值范围（取模偏差那一档用它算理论偏差）。</summary>
        public int Range { get { return range; } }

        public override string Title()
        {
            if (optionA == 0)
                return "随机与采样：rand()%n 的取模偏差　随机源取值范围 " + range + "　取 " + modulus
                       + " 个桶　抽样 " + samples + " 次（" + (optionB == 0 ? "小样本" : "大样本") + "）";
            if (optionA == 1)
                return "随机与采样：洗牌　3 张牌的 6 种排列　" + rounds
                       + " 轮　左 Fisher-Yates、右朴素交换";
            return "随机与采样：蓄水池抽样　12 个元素里留 3 个　" + rounds + " 轮";
        }

        public override string StatusText()
        {
            if (optionA == 0)
                return "抽样 " + samples + " 次　原始随机数 " + rawDraws + " 个　取模法计数差 "
                       + Spread(modCount) + "　拒绝法计数差 " + Spread(rejCount)
                       + "（样本数 ÷ " + range + " = " + (samples / range) + "）";
            if (optionA == 1)
                return "洗牌 " + rounds + " 轮　每种排列应出现约 " + (rounds / 6)
                       + " 次（标准差 " + ShuffleSigmaText() + "）　最多最少差：Fisher-Yates "
                       + Spread6(shuffleA) + "、朴素交换 " + Spread6(shuffleB);
            return "轮数 " + rounds + "　替换 " + replaces + " 次　随机数 " + rawDraws
                   + " 个　每元素入选 " + PickedMin() + " 到 " + PickedMax()
                   + " 次（理论 " + (rounds * 3 / 12) + "）";
        }

        public override string Detail()
        {
            if (optionA == 0)
                return "samples=" + samples + " range=" + range + " modulus=" + modulus
                       + " mod_spread=" + Spread(modCount) + " rej_spread=" + Spread(rejCount)
                       + " raw=" + rawDraws + " mod_counts=" + Join(modCount) + " rej_counts=" + Join(rejCount);
            if (optionA == 1)
                return "rounds=" + rounds + " fy_spread=" + Spread6(shuffleA) + " naive_spread="
                       + Spread6(shuffleB) + " expect=" + (rounds / 6) + " sigma="
                       + ShuffleSigmaText() + " raw=" + rawDraws
                       + " fy=" + Join6(shuffleA) + " naive=" + Join6(shuffleB);
            return "rounds=" + rounds + " replaces=" + replaces + " min=" + PickedMin()
                   + " max=" + PickedMax() + " expect=" + (rounds * 3 / 12) + " raw=" + rawDraws;
        }

        private static string Join(int[] values)
        {
            string text = "";
            for (int i = 0; i < values.Length; i++)
            {
                if (i > 0) text += ",";
                text += values[i];
            }
            return text;
        }

        private static string Join6(int[] values)
        {
            string text = "";
            for (int i = 0; i < 6; i++)
            {
                if (i > 0) text += ",";
                text += values[i];
            }
            return text;
        }
    }

    // ───────────────────────────────────────────────────────────── 场景 9 图上的结构性问题

    /// <summary>
    /// 三个结构性问题共用八个顶点：拓扑排序看层次、割点看去掉之后图怎么散开、
    /// 最小生成树看边一条条被选中。三个子场景各用一个独立算法，
    /// 自检里再用暴力实现对照一遍（不是自己跟自己比）。
    /// </summary>
    internal sealed class GraphModel : AlgoModel
    {
        public const int VertexCount = 8;
        public const int VertexBase = 0;
        public const int EdgeBase = 8;
        public const int EdgeMax = 12;
        public const int ResultBase = 20;
        public const int AuxBase = 32;

        private int optionA, optionB, seed;
        private int edgeCount;
        private readonly int[] edgeU = new int[EdgeMax];
        private readonly int[] edgeV = new int[EdgeMax];
        private readonly int[] edgeW = new int[EdgeMax];
        private readonly int[] edgeState = new int[EdgeMax];   // 0 未看，1 选中，2 拒绝
        private readonly int[] layer = new int[VertexCount];
        private readonly int[] indeg = new int[VertexCount];
        private readonly int[] parent = new int[VertexCount];
        private readonly int[] low = new int[VertexCount];
        private readonly int[] depth = new int[VertexCount];
        private readonly bool[] visited = new bool[VertexCount];
        private readonly bool[] cutVertex = new bool[VertexCount];
        private readonly int[] order = new int[VertexCount];
        private int orderCount, selected, totalWeight, rejected, layerCount, cutCount, bridges;
        private int visits, checks, components;

        public override void Build(int a, int b, int seedValue)
        {
            optionA = Clamp(a, 0, 2);
            optionB = Clamp(b, 0, 1);
            seed = seedValue;
            Trace.Init.Clear();
            Trace.Steps.Clear();
            orderCount = 0; selected = 0; totalWeight = 0; rejected = 0; layerCount = 0;
            cutCount = 0; bridges = 0; visits = 0; checks = 0; components = 0;
            for (int i = 0; i < VertexCount; i++)
            {
                layer[i] = -1; indeg[i] = 0; parent[i] = -1; low[i] = 0; depth[i] = -1;
                visited[i] = false; cutVertex[i] = false; order[i] = -1;
            }
            for (int i = 0; i < EdgeMax; i++) edgeState[i] = 0;
            BuildEdges();
            for (int i = 0; i < VertexCount; i++) InitCell(VertexBase + i, St.Data, i, 0, -1);
            for (int i = 0; i < EdgeMax; i++)
            {
                if (i < edgeCount) InitCell(EdgeBase + i, St.Data, edgeU[i] * 10 + edgeV[i], edgeW[i], i);
                else InitA(EdgeBase + i, 2, -2);
            }
            for (int i = 0; i < GraphModel.VertexCount; i++) InitCell(AuxBase + i, St.Empty, -1, i, -1);
            for (int i = GraphModel.VertexCount; i < 16; i++) InitA(AuxBase + i, 1, -2);
            if (optionA == 0) RunTopo();
            else if (optionA == 1) RunCut();
            else RunMst();
        }

        private void BuildEdges()
        {
            int[] sparseU = { 0, 0, 1, 1, 2, 3, 4, 5 };
            int[] sparseV = { 2, 3, 3, 4, 5, 5, 6, 7 };
            int[] denseU = { 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 5, 6 };
            int[] denseV = { 1, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7 };
            int[] useU = optionB == 0 ? sparseU : denseU;
            int[] useV = optionB == 0 ? sparseV : denseV;
            edgeCount = useU.Length;
            DeterministicRng rng = new DeterministicRng((uint)(seed + 606));
            for (int i = 0; i < edgeCount; i++)
            {
                edgeU[i] = useU[i];
                edgeV[i] = useV[i];
                edgeW[i] = 1 + rng.Next(9);
            }
            // 界面点掉的边：两端都置成 -1，各处的循环自然跳过它
            foreach (int index in removed)
            {
                if (index < 0 || index >= edgeCount) continue;
                edgeU[index] = -1;
                edgeV[index] = -1;
                edgeW[index] = 0;
            }
        }

        /// <summary>翻转一条边：点一下把它去掉，再点一下加回来。</summary>
        public override void Command(int op, int arg0, int arg1)
        {
            if (op != St.CmdToggle) return;
            int index = arg0 - EdgeBase;
            if (index < 0 || index >= EdgeMax) return;
            if (removed.Contains(index)) removed.Remove(index);
            else removed.Add(index);
        }

        public override void ClearEdits()
        {
            removed.Clear();
        }

        private readonly List<int> removed = new List<int>();

        // ── 拓扑排序：Kahn，按层推进

        private void RunTopo()
        {
            for (int i = 0; i < edgeCount; i++)
            {
                if (edgeU[i] < 0) continue;
                indeg[edgeV[i]]++;
            }
            for (int i = 0; i < VertexCount; i++)
            {
                InitA(VertexBase + i, 1, indeg[i]);
                InitA(VertexBase + i, 2, -1);
            }
            for (int i = 0; i < EdgeMax; i++)
                if (i < edgeCount) InitA(EdgeBase + i, 3, 0);
            int placed = 0;
            int currentLayer = 0;
            while (placed < VertexCount)
            {
                // 先把这一层「入度已经是 0」的顶点挑齐再一起出队：
                // 边一去掉就会冒出新的入度 0，边挑边放会把整张图压进同一层。
                List<int> ready = new List<int>();
                for (int v = 0; v < VertexCount; v++)
                    if (!visited[v] && indeg[v] == 0) ready.Add(v);
                if (ready.Count == 0) break;
                for (int k = 0; k < ready.Count; k++)
                {
                    int v = ready[k];
                    visited[v] = true;
                    layer[v] = currentLayer;
                    order[orderCount++] = v;
                    placed++;
                    Begin();
                    Set(VertexBase + v, St.Result);
                    A(VertexBase + v, 2, currentLayer);
                    A(VertexBase + v, 1, 0);
                    Set(AuxBase + v, St.Data);
                    A(AuxBase + v, 0, currentLayer);
                    Set(ResultBase + orderCount - 1, St.Result);
                    A(ResultBase + orderCount - 1, 0, v);
                    A(ResultBase + orderCount - 1, 1, currentLayer);
                    Primary = placed;
                    Secondary = 0;
                    Work = checks;
                    Pending = ready.Count - k - 1;
                    CustomA = currentLayer + 1;
                    CustomB = orderCount;
                    Current = VertexBase + v;
                    StateCode = 1;
                    Say("第 " + currentLayer + " 层：顶点 " + v + " 的入度已经是 0，出队（第 "
                        + orderCount + " 个）");
                    End();
                }
                for (int k = 0; k < ready.Count; k++)
                {
                    int v = ready[k];
                    for (int e = 0; e < edgeCount; e++)
                    {
                        if (edgeU[e] != v) continue;
                        checks++;
                        indeg[edgeV[e]]--;
                        edgeState[e] = 1;
                        Begin();
                        Set(EdgeBase + e, St.Done);
                        A(EdgeBase + e, 3, 1);
                        Set(VertexBase + edgeV[e], St.Active);
                        A(VertexBase + edgeV[e], 1, indeg[edgeV[e]]);
                        Primary = placed;
                        Secondary = checks;
                        Work = checks;
                        Pending = 0;
                        Current = EdgeBase + e;
                        StateCode = 1;
                        Say("去掉边 " + v + "→" + edgeV[e] + "：顶点 " + edgeV[e] + " 的入度降到 "
                            + indeg[edgeV[e]]);
                        End();
                    }
                }
                currentLayer++;
            }
            layerCount = currentLayer;
            Finished = true;
            HasResult = true;
            Begin();
            for (int i = 0; i < VertexCount; i++)
                if (visited[i]) Set(VertexBase + i, St.Done);
            Primary = orderCount;
            Secondary = checks;
            Work = checks;
            Pending = 0;
            CustomA = layerCount;
            CustomB = LongestChain();
            StateCode = 3;
            Say("拓扑序 " + OrderText() + "　共 " + layerCount + " 层，最长链 "
                + LongestChain() + " 条边（层数就是最长链上的顶点数）");
            End();
        }

        public int LongestChain()
        {
            int best = 0;
            for (int v = 0; v < VertexCount; v++) if (layer[v] + 1 > best) best = layer[v] + 1;
            return best;
        }

        private string OrderText()
        {
            string text = "";
            for (int i = 0; i < orderCount; i++)
            {
                if (i > 0) text += " ";
                text += order[i];
            }
            return text;
        }

        public string OrderString() { return OrderText(); }

        // ── 割点：Tarjan 的 low 值

        private void RunCut()
        {
            for (int v = 0; v < VertexCount; v++) InitA(VertexBase + v, 1, -1);
            TarjanVisit(0, -1, 0);
            for (int v = 0; v < VertexCount; v++)
            {
                if (visited[v]) continue;
                Begin();
                Set(VertexBase + v, St.Active);
                Primary = visits;
                Secondary = cutCount;
                Work = checks;
                Current = VertexBase + v;
                StateCode = 1;
                Say("顶点 " + v + " 还没访问过：图不连通，从它再开一棵 DFS 树");
                End();
                TarjanVisit(v, -1, 0);
            }
            // 找一个割点，真的去掉它再数一次连通分量
            int probe = -1;
            for (int v = 0; v < VertexCount; v++) if (cutVertex[v]) { probe = v; break; }
            components = probe >= 0 ? CountComponents(probe) : 1;
            Finished = true;
            HasResult = true;
            Begin();
            for (int v = 0; v < VertexCount; v++)
            {
                if (cutVertex[v]) Set(VertexBase + v, St.Result);
                else if (visited[v]) Set(VertexBase + v, St.Done);
            }
            Primary = cutCount;
            Secondary = visits;
            Work = checks;
            Pending = 0;
            CustomA = components;
            CustomB = bridges;
            StateCode = 3;
            Say("割点 " + cutCount + " 个" + (probe >= 0 ? "（去掉顶点 " + probe + " 之后图分成 "
                + components + " 块）" : "") + "　桥 " + bridges + " 条　DFS 访问 " + visits + " 个顶点");
            End();
        }

        private void TarjanVisit(int v, int from, int d)
        {
            visited[v] = true;
            visits++;
            depth[v] = d;
            low[v] = d;
            parent[v] = from;
            Begin();
            Set(VertexBase + v, St.Active);
            A(VertexBase + v, 1, low[v]);
            A(VertexBase + v, 2, d);
            Primary = visits;
            Secondary = cutCount;
            Work = checks;
            Pending = 0;
            Current = VertexBase + v;
            StateCode = 1;
            Say("访问顶点 " + v + "：low = " + low[v] + "，深度 " + d);
            End();
            int children = 0;
            for (int e = 0; e < edgeCount; e++)
            {
                int other = -1;
                if (edgeU[e] == v) other = edgeV[e];
                else if (edgeV[e] == v) other = edgeU[e];
                if (other < 0) continue;
                if (other == from) continue;
                checks++;
                if (!visited[other])
                {
                    children++;
                    Begin();
                    Set(EdgeBase + e, St.Active);
                    A(EdgeBase + e, 3, 0);
                    Work = checks;
                    Current = EdgeBase + e;
                    StateCode = 1;
                    Say("沿着边 " + v + "—" + other + " 往下走（顶点 " + other + " 还没访问过）");
                    End();
                    TarjanVisit(other, v, d + 1);
                    if (low[other] < low[v]) low[v] = low[other];
                    bool isCut = (from >= 0 && low[other] >= d) || (from < 0 && children > 1);
                    bool isBridge = low[other] > d;
                    if (isBridge) bridges++;
                    Begin();
                    Set(EdgeBase + e, isBridge ? St.Result : St.Done);
                    A(EdgeBase + e, 3, isBridge ? 1 : 0);
                    A(VertexBase + v, 1, low[v]);
                    if (isCut && !cutVertex[v]) cutVertex[v] = true;
                    if (cutVertex[v]) Set(VertexBase + v, St.Result);
                    Primary = visits;
                    Secondary = cutCount;
                    Work = checks;
                    Current = VertexBase + v;
                    StateCode = 1;
                    Say("回到顶点 " + v + "：孩子 " + other + " 的 low = " + low[other]
                        + "，自己的 low = " + low[v] + (isCut ? "　低值不够回到上面，" + v + " 是割点" : "")
                        + (isBridge ? "　这条边是桥" : ""));
                    End();
                }
                else if (depth[other] < low[v])
                {
                    low[v] = depth[other];
                    Begin();
                    Set(EdgeBase + e, St.Done);
                    A(EdgeBase + e, 3, 0);
                    A(VertexBase + v, 1, low[v]);
                    Work = checks;
                    Current = VertexBase + v;
                    StateCode = 1;
                    Say("顶点 " + v + " 有一条回边通向 " + other + "：low 收到 " + low[v]);
                    End();
                }
            }
            cutCount = CountCut();
        }

        private int CountCut()
        {
            int total = 0;
            for (int v = 0; v < VertexCount; v++) if (cutVertex[v]) total++;
            return total;
        }

        /// <summary>去掉某个顶点之后还剩几块连通分量（暴力数，用于对照）。</summary>
        public int CountComponents(int removed)
        {
            bool[] seen = new bool[VertexCount];
            int total = 0;
            for (int start = 0; start < VertexCount; start++)
            {
                if (start == removed || seen[start]) continue;
                total++;
                int[] stack = new int[VertexCount];
                int top = 0;
                stack[top++] = start;
                seen[start] = true;
                while (top > 0)
                {
                    int v = stack[--top];
                    for (int e = 0; e < edgeCount; e++)
                    {
                        if (edgeU[e] < 0) continue;
                        int other = -1;
                        if (edgeU[e] == v) other = edgeV[e];
                        else if (edgeV[e] == v) other = edgeU[e];
                        if (other < 0 || other == removed || seen[other]) continue;
                        seen[other] = true;
                        stack[top++] = other;
                    }
                }
            }
            return total;
        }

        // ── 最小生成树：Kruskal + 并查集

        private void RunMst()
        {
            int[] index = new int[EdgeMax];
            for (int i = 0; i < edgeCount; i++) index[i] = i;
            for (int i = 1; i < edgeCount; i++)
                for (int j = i; j > 0 && edgeW[index[j - 1]] > edgeW[index[j]]; j--)
                {
                    int t = index[j - 1]; index[j - 1] = index[j]; index[j] = t;
                }
            for (int v = 0; v < VertexCount; v++) parent[v] = v;
            int accepted = 0;
            for (int k = 0; k < edgeCount; k++)
            {
                int e = index[k];
                if (edgeU[e] < 0) continue;
                int ru = Find(edgeU[e]);
                int rv = Find(edgeV[e]);
                bool take = ru != rv;
                if (take)
                {
                    parent[ru] = rv;
                    accepted++;
                    selected++;
                    totalWeight += edgeW[e];
                    edgeState[e] = 1;
                }
                else
                {
                    rejected++;
                    edgeState[e] = 2;
                }
                Begin();
                Set(EdgeBase + e, take ? St.Result : St.Cut);
                A(EdgeBase + e, 3, take ? 1 : 2);
                Set(VertexBase + edgeU[e], take ? St.Marked : St.Data);
                Set(VertexBase + edgeV[e], take ? St.Marked : St.Data);
                Set(ResultBase + selected, take ? St.Result : St.Empty);
                if (take)
                {
                    A(ResultBase + selected, 0, edgeU[e]);
                    A(ResultBase + selected, 1, edgeV[e]);
                    A(ResultBase + selected, 2, edgeW[e]);
                }
                for (int v = 0; v < VertexCount; v++)
                {
                    Set(AuxBase + v, St.Data);
                    A(AuxBase + v, 0, Find(v));
                }
                Primary = selected;
                Secondary = totalWeight;
                Work = k + 1;
                Pending = edgeCount - (k + 1);
                CustomA = rejected;
                CustomB = Components();
                StateCode = 1;
                Current = EdgeBase + e;
                Say("按权从小到大看边 " + edgeU[e] + "—" + edgeV[e] + "（权 " + edgeW[e] + "）："
                    + (take ? "两端还没连通，收进生成树" : "两端已经连通，收下就成环，丢掉"));
                End();
            }
            Finished = true;
            HasResult = true;
            Begin();
            for (int i = 0; i < edgeCount; i++)
                Set(EdgeBase + i, edgeState[i] == 1 ? St.Result : St.Cut);
            for (int v = 0; v < VertexCount; v++) Set(VertexBase + v, St.Done);
            Primary = selected;
            Secondary = totalWeight;
            Work = edgeCount;
            Pending = 0;
            CustomA = rejected;
            CustomB = Components();
            StateCode = 3;
            Say("Kruskal 收工：选了 " + selected + " 条边、总权 " + totalWeight + "、丢掉 "
                + rejected + " 条（收下会成环）");
            End();
        }

        private int Find(int v)
        {
            int walk = v;
            int guard = 0;
            while (parent[walk] != walk && guard++ < 32) walk = parent[walk];
            return walk;
        }

        private int Components()
        {
            bool[] seen = new bool[VertexCount];
            int total = 0;
            for (int v = 0; v < VertexCount; v++)
            {
                int root = Find(v);
                if (seen[root]) continue;
                seen[root] = true;
                total++;
            }
            return total;
        }

        /// <summary>独立实现的最小生成树（Prim），自检拿它与 Kruskal 对照。</summary>
        public int PrimWeight()
        {
            bool[] used = new bool[VertexCount];
            int[] best = new int[VertexCount];
            for (int v = 0; v < VertexCount; v++) best[v] = int.MaxValue;
            best[0] = 0;
            int total = 0;
            for (int step = 0; step < VertexCount; step++)
            {
                int pick = -1;
                for (int v = 0; v < VertexCount; v++)
                    if (!used[v] && (pick < 0 || best[v] < best[pick])) pick = v;
                if (pick < 0 || best[pick] == int.MaxValue) return -1;
                used[pick] = true;
                total += best[pick];
                for (int e = 0; e < edgeCount; e++)
                {
                    if (edgeU[e] < 0) continue;
                    int other = -1;
                    if (edgeU[e] == pick) other = edgeV[e];
                    else if (edgeV[e] == pick) other = edgeU[e];
                    if (other < 0 || used[other]) continue;
                    if (edgeW[e] < best[other]) best[other] = edgeW[e];
                }
            }
            return total;
        }

        /// <summary>暴力判割点：去掉每个顶点各数一次连通分量。自检拿它与 Tarjan 对照。</summary>
        public int BruteCutCount()
        {
            int total = 0;
            for (int v = 0; v < VertexCount; v++)
            {
                if (CountComponents(v) > 1) total++;
            }
            return total;
        }

        public int EdgeCount { get { return edgeCount; } }

        public int EdgeU(int i) { return edgeU[i]; }

        public int EdgeV(int i) { return edgeV[i]; }

        public int EdgeW(int i) { return edgeW[i]; }

        public int LayerCount { get { return layerCount; } }

        public int CutCount { get { return cutCount; } }

        public int Bridges { get { return bridges; } }

        public int Selected { get { return selected; } }

        public int TotalWeight { get { return totalWeight; } }

        public int ComponentsAfterCut { get { return components; } }

        public int FirstCut()
        {
            for (int v = 0; v < VertexCount; v++) if (cutVertex[v]) return v;
            return -1;
        }

        public bool TopoValid()
        {
            // 拓扑序里每条边都必须从前指向后
            int[] pos = new int[VertexCount];
            for (int i = 0; i < orderCount; i++) pos[order[i]] = i;
            for (int e = 0; e < edgeCount; e++)
                if (pos[edgeU[e]] >= pos[edgeV[e]]) return false;
            return orderCount == VertexCount;
        }

        public override string Title()
        {
            if (optionA == 0)
                return "图上的结构性问题：拓扑排序的层次　" + VertexCount + " 个顶点、"
                       + edgeCount + " 条有向边　" + (optionB == 0 ? "边集 A（稀疏）" : "边集 B（稠密）");
            if (optionA == 1)
                return "图上的结构性问题：割点　" + VertexCount + " 个顶点、" + edgeCount
                       + " 条无向边　" + (optionB == 0 ? "边集 A（稀疏）" : "边集 B（稠密）");
            return "图上的结构性问题：最小生成树　" + VertexCount + " 个顶点、" + edgeCount
                   + " 条带权边　" + (optionB == 0 ? "边集 A（稀疏）" : "边集 B（稠密）");
        }

        public override string StatusText()
        {
            if (optionA == 0)
                return "已入队 " + orderCount + "／" + VertexCount + "　层数 " + layerCount
                       + "　最长链 " + LongestChain() + " 个顶点　去掉的边 " + checks
                       + " 条　拓扑序 " + OrderText();
            if (optionA == 1)
                return "割点 " + cutCount + " 个　桥 " + bridges + " 条　已访问 " + visits
                       + "／" + VertexCount + " 个顶点　检查边 " + checks
                       + " 次　去掉一个割点后分成 " + components + " 块";
            return "已选 " + selected + " 条边　总权 " + totalWeight + "　丢掉 " + rejected
                   + " 条　当前连通块 " + Components() + "　Prim 的答案是 " + PrimWeight();
        }

        public override string Detail()
        {
            if (optionA == 0)
                return "order=" + OrderText() + " layers=" + layerCount + " chain=" + LongestChain()
                       + " valid=" + (TopoValid() ? 1 : 0) + " edges=" + edgeCount;
            if (optionA == 1)
                return "cuts=" + cutCount + " brute_cuts=" + BruteCutCount() + " bridges=" + bridges
                       + " components_after=" + components + " visited=" + visits + " edges=" + edgeCount;
            return "selected=" + selected + " weight=" + totalWeight + " prim=" + PrimWeight()
                   + " rejected=" + rejected + " components=" + Components() + " edges=" + edgeCount;
        }
    }

    // ───────────────────────────────────────────────────────────── 一帧的几何

    /// <summary>
    /// 一帧的几何：每个元素画在哪个矩形里、连线怎么走。
    ///
    /// **绘制、命中测试、连线自检三处用的是同一份几何**，因此自检断言的就是
    /// 屏幕上的线。矩形为空表示这个元素本场景里没有位置，不画也不可点。
    /// </summary>
    internal sealed class AlgoView
    {
        public readonly Rectangle[] Box = new Rectangle[AlgoConst.Elements];
        public readonly AlgoPath Path = new AlgoPath();
        public readonly List<AlgoLabel> Labels = new List<AlgoLabel>();
        public int Mode = -1;
        public string Caption = "";
        public string Caption2 = "";

        public void Build(VisualSnapshot s)
        {
            for (int i = 0; i < Box.Length; i++) Box[i] = Rectangle.Empty;
            Path.Clear();
            Labels.Clear();
            Caption = "";
            Caption2 = "";
            Mode = s.ModeKind;
            switch (Mode)
            {
                case 0: BuildRecursion(s); break;
                case 1: BuildDivide(s); break;
                case 2: BuildSort(s); break;
                case 3: BuildSearch(s); break;
                case 4: BuildDp(s); break;
                case 5: BuildMatch(s); break;
                case 6: BuildFsm(s); break;
                case 7: BuildRandom(s); break;
                default: BuildGraph(s); break;
            }
        }

        /// <summary>画布上的一行说明。**每张图都要能认出「这一行是什么」**：
        /// 少了行列名，读者只能猜哪一行是哪个算法。</summary>
        private void Label(int x, int y, int width, string text)
        {
            Label(x, y, width, text, false);
        }

        private void Label(int x, int y, int width, string text, bool muted)
        {
            if (text.Length == 0) return;
            AlgoLabel label = new AlgoLabel();
            label.Box = new Rectangle(x, y, width, 18);
            label.Text = text;
            label.Muted = muted;
            Labels.Add(label);
        }

        private void Put(int index, int x, int y, int w, int h)
        {
            if (index < 0 || index >= Box.Length) return;
            Box[index] = new Rectangle(x, y, w, h);
        }

        // ── 场景 1 递归调用树

        private void BuildRecursion(VisualSnapshot s)
        {
            LayoutTree(s, RecursionModel.TreeBase, RecursionModel.TreeMax,
                       new Rectangle(30, 58, 660, 520), 46, 36);
            for (int i = 0; i < 8; i++)
                Put(RecursionModel.StackBase + i, 706, 60 + i * 62, 226, 44);
            Label(30, 34, 300, "调用树（节点上的数就是参数）");
            Label(706, 34, 226, "栈帧列（一层一个帧）");
            Caption = "左：调用树　右：栈帧列。深色的帧是已经弹出的，因此整列的高度就是这次递归到过的最深处";
        }

        // ── 场景 2 记忆化·分治·回溯

        private void BuildDivide(VisualSnapshot s)
        {
            LayoutTree(s, DivideModel.TreeBase, DivideModel.TreeMax,
                       new Rectangle(30, 58, 730, 376), 44, 36);
            Label(30, 34, 400, "递归树／决策树");
            if (s.OptionA == 0)
            {
                for (int i = 0; i < 8; i++) Put(DivideModel.AuxBase + i, 60 + i * 78, 470, 70, 46);
                Label(60, 446, 400, "缓存表（参数 → 已算出的值）");
                Caption = optionBText(s.OptionA, s.OptionB);
            }
            else if (s.OptionA == 1)
            {
                for (int i = 0; i < 16; i++) Put(DivideModel.AuxBase + i, 60 + i * 54, 470, 50, 42);
                Label(60, 446, 400, "合并输出行（两个有序段并到这里，再写回数组）");
                Caption = "合并发生在哪一层写在状态行上：这一行就是那一层合并出来的结果";
            }
            else
            {
                for (int i = 0; i < 8; i++) Put(DivideModel.AuxBase + i, 60 + i * 66, 470, 58, 42);
                Label(60, 446, 400, "当前部分解（选择写进来，撤销就抹掉）");
                Caption = "选择与撤销一步一步成对出现：写进这一行、再抹掉，次数在顶行与状态行里数得出来";
            }
            Caption2 = "";
        }

        // ── 场景 3 排序对照

        private void BuildSort(VisualSnapshot s)
        {
            for (int r = 0; r < SortModel.Rows; r++)
            {
                for (int j = 0; j < SortModel.Cols; j++)
                    Put(SortModel.RowBase + r * SortModel.Cols + j, 150 + j * 62, 78 + r * 104, 58, 40);
                Label(8, 78 + r * 104 + 11, 138, SortModel.RowNames[r]);
            }
            for (int j = 0; j < SortModel.Cols; j++)
                Put(AlgoConst.InputBase + j, 150 + j * 62, 512, 58, 36);
            Label(8, 523, 138, "原始数据");
            Caption = "四行是同一份输入的副本，一步一步同时推进　每一格下面写着它的原始下标";
            Caption2 = "相等元素看稳定性：稳定的两行不会把原始下标排反，不稳定的两行会";
        }

        // ── 场景 4 查找对照

        private void BuildSearch(VisualSnapshot s)
        {
            for (int j = 0; j < SearchModel.DataCount; j++)
            {
                Put(SearchModel.LinBase + j, 110 + j * 50, 96, 46, 34);
                Put(SearchModel.BinBase + j, 110 + j * 50, 186, 46, 34);
            }
            for (int i = 0; i < SearchModel.BucketCount; i++)
                Put(SearchModel.BucketBase + i, 110 + i * 100, 300, 92, 40);
            for (int i = 0; i < SearchModel.ChainMax; i++)
                Put(SearchModel.ChainBase + i, 110 + i * 100, 356, 92, 34);
            Label(8, 96, 96, "线性查找");
            Label(8, 186, 96, "二分查找");
            Label(8, 300, 96, "哈希的桶");
            Label(8, 356, 96, "正在比的键");
            Caption = "三行用的是同一批键：线性从头扫、二分每次砍一半、哈希先定位桶再走链";
            Caption2 = "桶里写着它的冲突链；二分那一行区间外的格子会变成「已跳过」，区间宽度写在顶行";
        }

        // ── 场景 5 动态规划

        private void BuildDp(VisualSnapshot s)
        {
            int rows = s.OptionB == 1 ? DpModel.RollRows : DpModel.Rows;
            for (int j = 0; j < DpModel.Cols; j++)
                Put(AlgoConst.InputBase + j, 150 + j * 72, 40, 68, 30);
            for (int i = 0; i < DpModel.Rows; i++)
                Put(AlgoConst.InputBase + DpModel.Cols + i, 60, 76 + i * 54, 80, 50);
            for (int i = 0; i < rows; i++)
                for (int j = 0; j < DpModel.Cols; j++)
                    Put(s.OptionB == 1 ? DpModel.RollBase + i * DpModel.Cols + j
                                       : DpModel.TableBase + i * DpModel.Cols + j,
                        150 + j * 72, 76 + i * 54, 68, 50);
            // 转移来源的折线：从来源格连到当前格
            int current = s.CurrentIndex;
            if (current >= 0)
            {
                int baseIndex = s.OptionB == 1 ? DpModel.RollBase : DpModel.TableBase;
                int offset = current - baseIndex;
                if (offset >= 0 && offset < rows * DpModel.Cols)
                {
                    int i = offset / DpModel.Cols;
                    int j = offset % DpModel.Cols;
                    if (j > 0) LinkCells(baseIndex + i * DpModel.Cols + (j - 1), current, true);
                    if (i > 0) LinkCells(baseIndex + (i - 1) * DpModel.Cols + j, current, true);
                    if (i > 0 && j > 0) LinkCells(baseIndex + (i - 1) * DpModel.Cols + (j - 1), current, false);
                }
            }
            string head = "";
            for (int j = 1; j < DpModel.Cols; j++) head += (char)s.Attr0[AlgoConst.InputBase + j];
            // 标在表头右边：标在它上方会压住第一行表头
            Label(732, 46, 200, "← X = " + head, true);
            Label(60, 44, 80, "Y ↓");
            Caption = s.OptionB == 1
                ? "滚动数组：只留上一行与当前行，16 格顶替 64 格；折线仍然指出转移来源"
                : "行是 X 的前缀、列是 Y 的前缀；折线指出当前格是从哪几格转移过来的";
            Caption2 = "灰／红的格子是当前格的转移来源，橙色是正在填的那一格，蓝色是已经填好的";
        }

        /// <summary>两个格子之间的正交连线：先横后竖，拐点在两格之间。</summary>
        private void LinkCells(int from, int to, bool hot)
        {
            Rectangle a = Box[from];
            Rectangle b = Box[to];
            if (a.IsEmpty || b.IsEmpty) return;
            int ax = a.Left + a.Width / 2, ay = a.Top + a.Height / 2;
            int bx = b.Left + b.Width / 2, by = b.Top + b.Height / 2;
            int midY = (ay + by) / 2;
            Path.ViaY(ax, ay, bx, by, midY, hot);
        }

        // ── 场景 6 字符串匹配

        private void BuildMatch(VisualSnapshot s)
        {
            bool showNaive = s.OptionB != 2;
            bool showKmp = s.OptionB != 1;
            if (showNaive)
            {
                for (int j = 0; j < MatchModel.TextLen; j++)
                    Put(MatchModel.NaiveTextBase + j, 60 + j * 52, 84, 48, 34);
                int align = AlignOf(s, 0);
                for (int j = 0; j < MatchModel.PatLen; j++)
                    Put(MatchModel.NaivePatBase + j, 60 + (align + j) * 52, 132, 48, 34);
            }
            if (showKmp)
            {
                for (int j = 0; j < MatchModel.TextLen; j++)
                    Put(MatchModel.KmpTextBase + j, 60 + j * 52, 254, 48, 34);
                int align = AlignOf(s, 1);
                for (int j = 0; j < MatchModel.PatLen; j++)
                    Put(MatchModel.KmpPatBase + j, 60 + (align + j) * 52, 302, 48, 34);
            }
            for (int j = 0; j < MatchModel.PatLen; j++)
            {
                Put(MatchModel.FailCharBase + j, 60 + j * 78, 400, 66, 34);
                Put(MatchModel.FailValueBase + j, 60 + j * 78, 444, 66, 34);
            }
            if (showNaive)
            {
                Label(8, 84, 48, "朴素");
                Label(8, 132, 48, "模式");
                Label(8, 168, 600, "朴素比较 " + s.CustomA + " 次、滑动 " + (s.SecondaryMetric / 2) + " 次", true);
            }
            if (showKmp)
            {
                Label(8, 254, 48, "KMP");
                Label(8, 302, 48, "模式");
                Label(8, 338, 600, "KMP 比较 " + s.CustomB + " 次、滑动 " + (s.SecondaryMetric / 2) + " 次", true);
            }
            Label(8, 400, 48, "模式");
            Label(8, 444, 48, "fail");
            Label(200, 444, 500, "fail[i] = 前 i+1 个字符里最长的「既是前缀又是后缀」的长度", true);
            Caption = "上面两行是朴素匹配（失配就整段退回），中间两行是 KMP（失配只退模式）";
            Caption2 = "最下面：失配表 fail[i] = 「前 i+1 个字符里，最长的既是前缀又是后缀的长度」";
        }

        /// <summary>当前对齐位置：运算侧把它写在每一行第一个文本格的属性 3 上。</summary>
        private int AlignOf(VisualSnapshot s, int row)
        {
            int baseIndex = row == 0 ? MatchModel.NaiveTextBase : MatchModel.KmpTextBase;
            int align = s.Attr3[baseIndex];
            if (align < 0 || align > MatchModel.TextLen - MatchModel.PatLen)
                return 0;
            return align;
        }

        // ── 场景 7 状态机

        private void BuildFsm(VisualSnapshot s)
        {
            for (int j = 0; j < FsmModel.InputLen; j++)
                Put(FsmModel.InputBase + j, 60 + j * 52, 60, 48, 34);
            for (int i = 0; i < FsmModel.StateCount; i++)
            {
                Put(FsmModel.SwitchStateBase + i, 100 + i * 112, 130, 104, 32);
                Put(FsmModel.TableStateBase + i, 100 + i * 112, 176, 104, 32);
            }
            for (int st = 0; st < FsmModel.StateCount; st++)
                for (int c = 0; c < FsmModel.ClassCount; c++)
                    Put(FsmModel.TableBase + st * FsmModel.ClassCount + c, 100 + c * 122, 258 + st * 28, 116, 24);
            for (int j = 0; j < FsmModel.InputLen; j++)
                Put(FsmModel.OutBase + j, 60 + j * 52, 470, 48, 30);
            for (int j = 0; j < FsmModel.InputLen; j++)
                Put(FsmModel.HistoryBase + j, 60 + j * 52, 516, 48, 26);
            Label(8, 60, 44, "输入");
            Label(8, 137, 88, "switch");
            Label(8, 183, 88, "表驱动");
            for (int c = 0; c < FsmModel.ClassCount; c++)
                Label(100 + c * 122, 238, 116, FsmModel.ClassNames[c], true);
            for (int st = 0; st < FsmModel.StateCount; st++)
                Label(8, 258 + st * 28, 88, FsmModel.StateNames[st]);
            Label(8, 470, 44, "输出");
            Label(8, 516, 44, "走过");
            Caption = "上一行 switch 版、下一行表驱动版，两版同时往前走；中间是 7×6 的转移表"
                      + "（行是状态、列是字符类别）";
            Caption2 = "最下面两行：剥出来的输出、以及走过的状态序列（数字就是状态号）";
        }

        // ── 场景 8 随机与采样

        private void BuildRandom(VisualSnapshot s)
        {
            int bars = s.OptionA == 0 ? 12 : (s.OptionA == 1 ? 12 : 12);
            int max = 1;
            for (int i = 0; i < bars; i++) if (s.Attr1[i] > max) max = s.Attr1[i];
            int baseline = 500;
            for (int i = 0; i < bars; i++)
            {
                int count = s.Attr1[i];
                int height = 24 + (int)((long)count * 400 / max);
                int x = 60 + i * 68;
                int width = 62;
                Put(RandomModel.BarBase + i, x, baseline - height, width, height);
            }
            if (s.OptionA == 2)
            {
                for (int i = 0; i < 3; i++) Put(RandomModel.ResBase + i, 700 + i * 74, 520, 68, 40);
                for (int i = 0; i < 12; i++) Label(60 + i * 68, 504, 62, (i + 1).ToString(CultureInfo.InvariantCulture), true);
                Label(700, 496, 222, "这一轮的蓄水池");
                Caption = "十二根柱子：每个元素被抽进蓄水池的次数　柱高就是计数，右三格是这一轮的蓄水池";
            }
            else if (s.OptionA == 0)
            {
                for (int i = 0; i < 6; i++)
                {
                    Label(60 + i * 68, 504, 62, "桶 " + i, true);
                    Label(60 + (6 + i) * 68, 504, 62, "桶 " + i, true);
                }
                Label(60, 528, 380, "左六根：rand()%6（取模）");
                Label(468, 528, 380, "右六根：拒绝采样（丢掉 30、31 重抽）");
                Caption = "随机源取值 0 到 31，取 6 个桶：32 = 6×5 + 2，前两个桶各多一个余数，因此左六根天然偏高";
                Caption2 = "拒绝采样把多出来的两个值丢掉重抽，右六根就齐了";
            }
            else
            {
                for (int i = 0; i < 6; i++)
                {
                    Label(60 + i * 68, 504, 62, RandomModel.PermNames[i], true);
                    Label(60 + (6 + i) * 68, 504, 62, RandomModel.PermNames[i], true);
                }
                Label(60, 528, 380, "左六根：Fisher-Yates");
                Label(468, 528, 380, "右六根：每张与随机一张交换（朴素）");
                Caption = "三张牌的六种排列各出现多少次（柱下标就是排列）：均匀时六根应当一样高";
                Caption2 = "Fisher-Yates 的六根在噪声范围内，朴素交换的六根明显分成两档（5/27 与 4/27）";
            }
        }

        // ── 场景 9 图上的结构性问题

        private void BuildGraph(VisualSnapshot s)
        {
            for (int v = 0; v < GraphModel.VertexCount; v++)
                Put(GraphModel.VertexBase + v, 120 + (v % 4) * 190, v < 4 ? 140 : 330, 84, 46);
            for (int e = 0; e < GraphModel.EdgeMax; e++)
            {
                if (s.Attr2[GraphModel.EdgeBase + e] == -2) continue;   // 这一档没有这条边
                Put(GraphModel.EdgeBase + e, 800, 60 + e * 28, 130, 26);
            }
            for (int i = 0; i < 12; i++)
            {
                // 结果条只画已经填上的格子，没用到的不留空框
                if (s.State[GraphModel.ResultBase + i] == St.Empty
                    && s.Attr0[GraphModel.ResultBase + i] == 0) continue;
                Put(GraphModel.ResultBase + i, 60 + i * 62, 420, 58, 30);
            }
            for (int i = 0; i < 16; i++)
            {
                if (s.Attr1[GraphModel.AuxBase + i] == -2) continue;
                if (s.State[GraphModel.AuxBase + i] == St.Empty) continue;
                Put(GraphModel.AuxBase + i, 60 + i * 52, 478, 48, 26);
            }
            Label(120, 112, 300, "第一排顶点");
            Label(120, 302, 300, "第二排顶点");
            Label(800, 40, 130, "边表（两端与权）");
            Label(8, 420, 48, "结果");
            Label(8, 478, 48, "辅助");
            for (int e = 0; e < GraphModel.EdgeMax; e++)
            {
                int code = s.Attr0[GraphModel.EdgeBase + e];
                if (code < 0) continue;
                int u = code / 10;
                int v = code % 10;
                if (u < 0 || u >= GraphModel.VertexCount || v < 0 || v >= GraphModel.VertexCount) continue;
                Rectangle a = Box[GraphModel.VertexBase + u];
                Rectangle b = Box[GraphModel.VertexBase + v];
                if (a.IsEmpty || b.IsEmpty) continue;
                // 通道分三层：两排之间的带（跨排）、第一排上方、第二排下方。
                // **出口要选朝通道的那一边**，否则竖段会从节点盒子里穿过去。
                int channel;
                if ((u < 4) != (v < 4)) channel = 200 + e * 10;
                else if (u < 4) channel = 62 + (e % 6) * 12;
                else channel = 384 + (e % 4) * 8;
                bool hot = s.State[GraphModel.EdgeBase + e] == St.Active;
                Path.ViaY(a.Left + a.Width / 2, channel > a.Bottom ? a.Bottom : a.Top,
                          b.Left + b.Width / 2, channel > b.Bottom ? b.Bottom : b.Top, channel, hot);
            }
            Caption = "顶点按两排摆；每条边一条自己的通道，因此不会叠成一条长横线";
            Caption2 = "右边是边表（两端与权），下面两行是这一轮的结果条与辅助条";
        }

        /// <summary>记忆化那一档的附注要跟着选项走：开着与关着说的不是一回事。</summary>
        private static string optionBText(int optionA, int optionB)
        {
            return optionB == 1
                ? "记忆化开：第一次遇到某个参数才算，算完写进缓存；树上换成「已跳过」色的小块就是直接取缓存"
                : "记忆化关：每个参数每次遇到都重算一遍，缓存表始终是空的";
        }

        /// <summary>两排顶点之间的通道带：跨排的边走这里。</summary>
        public const int GraphTopBand = 200;

        // ── 树形布局：按中序排名定横坐标、按层数定纵坐标
        //
        // 说明放这里：顶点两排之间的通道带从 y = 200 起，每条跨排的边占一条自己的通道。

        // ── 树形布局：按中序排名定横坐标、按层数定纵坐标

        private readonly int[] treeParent = new int[AlgoConst.Elements];
        private readonly int[] treeDepth = new int[AlgoConst.Elements];
        private readonly int[] treeRank = new int[AlgoConst.Elements];
        private readonly List<int>[] treeChildren = new List<int>[AlgoConst.Elements];

        /// <summary>
        /// 树形布局：按中序排名定横坐标、按层数定纵坐标。
        /// **盒子宽度按叶子数收**：`宽度 ÷ (叶子数 + 1)`，这样相邻叶子之间总是留得下一条缝，
        /// 字号调大之后也不会出现两个节点叠在一起。
        /// </summary>
        private void LayoutTree(VisualSnapshot s, int baseIndex, int count, Rectangle area, int maxBoxW, int boxH)
        {
            int root = -1, leaves = 0, maxDepth = 0;
            for (int i = 0; i < count; i++)
            {
                int index = baseIndex + i;
                treeParent[i] = s.Attr1[index];
                treeDepth[i] = s.Attr2[index];
                treeRank[i] = 0;
                if (treeChildren[i] == null) treeChildren[i] = new List<int>();
                else treeChildren[i].Clear();
            }
            for (int i = 0; i < count; i++)
            {
                if (treeDepth[i] < 0) continue;
                int parent = treeParent[i];
                if (parent >= 0 && parent < count && treeDepth[parent] >= 0) treeChildren[parent].Add(i);
                else root = i;
                if (treeDepth[i] > maxDepth) maxDepth = treeDepth[i];
            }
            if (root < 0) return;
            Rank(root, ref leaves);
            int boxW = maxBoxW;
            if (leaves > 0 && area.Width / (leaves + 1) < boxW) boxW = area.Width / (leaves + 1);
            if (boxW < 30) boxW = 30;
            int rowH = maxDepth > 0 ? (area.Height - boxH) / maxDepth : 0;
            for (int i = 0; i < count; i++)
            {
                if (treeDepth[i] < 0) continue;
                if (s.State[baseIndex + i] == St.Empty) continue;
                double t = leaves <= 1 ? 0.5 : (treeRank[i] + 0.5) / leaves;
                int x = area.Left + (int)(t * (area.Width - boxW));
                int y = area.Top + treeDepth[i] * rowH;
                Put(baseIndex + i, x, y, boxW, boxH);
            }
            for (int i = 0; i < count; i++)
            {
                if (treeDepth[i] < 0 || treeParent[i] < 0) continue;
                if (s.State[baseIndex + i] == St.Empty) continue;
                Rectangle child = Box[baseIndex + i];
                Rectangle parent = Box[baseIndex + treeParent[i]];
                if (child.IsEmpty || parent.IsEmpty) continue;
                int midY = (child.Top + parent.Bottom) / 2;
                bool hot = s.State[baseIndex + i] == St.Active;
                Path.ViaY(child.Left + child.Width / 2, child.Top,
                          parent.Left + parent.Width / 2, parent.Bottom, midY, hot);
            }
        }

        /// <summary>后序排名：叶子按出现次序编号，内部节点取孩子排名的中点。</summary>
        private int Rank(int node, ref int next)
        {
            List<int> kids = treeChildren[node];
            if (kids.Count == 0)
            {
                treeRank[node] = next;
                next++;
                return treeRank[node];
            }
            int first = -1, last = -1;
            for (int i = 0; i < kids.Count; i++)
            {
                int value = Rank(kids[i], ref next);
                if (first < 0) first = value;
                last = value;
            }
            treeRank[node] = (first + last) / 2;
            return treeRank[node];
        }
    }

    // ───────────────────────────────────────────────────────────── 运算侧

    /// <summary>
    /// 九个场景共用一个引擎：引擎只做三件事——按序号播放模型算好的那串步、
    /// 把每步的写入落到自己的元素数组上、把数组与头部数字摊进快照。
    /// 因此「回放到第 N 步」天然成立，自检跑到底的数字与界面上看到的必然一致。
    /// </summary>
    internal sealed class AlgoEngine : IVisualEngine
    {
        private readonly VisualSnapshot own = new VisualSnapshot(AlgoConst.Elements);
        private readonly AlgoModel[] models = new AlgoModel[9];
        private readonly List<int[]> undo = new List<int[]>();

        private int mode, optionA, optionB;
        private int seed = AlgoConst.DefaultSeed;
        private int pc;
        private int lastMailbox;
        private int pendingCmd, pendingArg0, pendingArg1;
        private AlgoStep head;

        public AlgoEngine()
        {
            models[0] = new RecursionModel();
            models[1] = new DivideModel();
            models[2] = new SortModel();
            models[3] = new SearchModel();
            models[4] = new DpModel();
            models[5] = new MatchModel();
            models[6] = new FsmModel();
            models[7] = new RandomModel();
            models[8] = new GraphModel();
        }

        public int ElementCount { get { return AlgoConst.Elements; } }

        public bool Finished { get { return pc >= Model.StepCount; } }

        public bool HasResult { get { return Finished && head != null && head.HasResult; } }

        private AlgoModel Model { get { return models[mode]; } }

        /// <summary>当前场景的模型。自检要读它自己的那些计数（逐行比较次数之类）。</summary>
        public AlgoModel ActiveModel { get { return models[mode]; } }

        public int Mode { get { return mode; } }

        public int Seed
        {
            get { return seed; }
            set { seed = value; }
        }

        public void Configure(int modeKind, int a, int b)
        {
            mode = Clamp(modeKind, 0, 8);
            optionA = Clamp(a, 0, AlgoNames.OptionCount(mode) - 1);
            optionB = Clamp(b, 0, AlgoNames.Option2Count(mode, optionA) - 1);
            undo.Clear();
        }

        public void ResetAll()
        {
            seed = own.Seed != 0 ? own.Seed : seed;
            ResetRun();
        }

        public void ResetRun()
        {
            if (pendingCmd != 0)
            {
                Model.Command(pendingCmd, pendingArg0, pendingArg1);
                pendingCmd = 0;
            }
            Model.Build(optionA, optionB, seed);
            ClearArrays();
            for (int i = 0; i < Model.Result.Init.Count; i++) Write(own, Model.Result.Init[i]);
            pc = 0;
            head = null;
            own.Marker[AlgoConst.Mailbox] = 0;
        }

        public void StepOnce()
        {
            if (Finished) return;
            AlgoStep step = Model.Result.Steps[pc];
            for (int i = 0; i < step.Writes.Count; i++) Write(own, step.Writes[i]);
            head = step;
            pc++;
        }

        private void ClearArrays()
        {
            for (int i = 0; i < AlgoConst.Elements; i++)
            {
                own.State[i] = St.Empty;
                own.Marker[i] = 0;
                own.Tag[i] = 0;
                own.Attr0[i] = 0;
                own.Attr1[i] = -1;
                own.Attr2[i] = -1;
                own.Attr3[i] = -1;
                own.Attr4[i] = 0;
                own.Attr5[i] = 0;
            }
        }

        private static void Write(VisualSnapshot target, AlgoWrite write)
        {
            int i = write.Index;
            if (i < 0 || i >= AlgoConst.Elements) return;
            switch (write.Field)
            {
                case 0: target.State[i] = (byte)write.Value; break;
                case 1: target.Marker[i] = (byte)write.Value; break;
                case 2: target.Tag[i] = (byte)write.Value; break;
                case 3: target.Attr0[i] = write.Value; break;
                case 4: target.Attr1[i] = write.Value; break;
                case 5: target.Attr2[i] = write.Value; break;
                case 6: target.Attr3[i] = write.Value; break;
                case 7: target.Attr4[i] = write.Value; break;
                default: target.Attr5[i] = write.Value; break;
            }
        }

        public void RefreshSnapshot(VisualSnapshot target)
        {
            Copy(own, target);
            target.StateCode = head == null ? 0 : head.StateCode;
            target.Steps = pc;
            target.WorkCount = head == null ? 0 : head.Work;
            target.PendingCount = head == null ? 0 : head.Pending;
            target.PrimaryMetric = head == null ? -1 : head.Primary;
            target.SecondaryMetric = head == null ? -1 : head.Secondary;
            target.CurrentIndex = head == null ? -1 : head.Current;
            target.FocusIndex = head == null ? -1 : head.Focus;
            target.Seed = seed;
            target.CustomA = head == null ? 0 : head.CustomA;
            target.CustomB = head == null ? 0 : head.CustomB;
            target.ModeKind = mode;
            target.OptionA = optionA;
            target.OptionB = optionB;
            target.Finished = Finished ? 1 : 0;
            target.HasResult = HasResult ? 1 : 0;
            target.Running = 0;
        }

        public void ExportScene(VisualSnapshot target)
        {
            // 运算进程刚起来那一帧带着 INIT 发来的种子：引擎的种子与它不一致就现场重建，
            // 否则界面上换了种子要等一次点击才生效。
            if (target.Seed != 0 && target.Seed != seed)
            {
                seed = target.Seed;
                ResetRun();
            }
            Copy(own, target);
            target.Seed = seed;
        }

        /// <summary>只读邮箱。界面进程改的场景进不来，除非它写的是邮箱那一个元素。</summary>
        public void ImportScene(VisualSnapshot source)
        {
            int marker = source.Marker[AlgoConst.Mailbox];
            if (marker == 0 || marker == lastMailbox) return;
            lastMailbox = marker;
            int op = source.Attr0[AlgoConst.Mailbox];
            int arg0 = source.Attr1[AlgoConst.Mailbox];
            int arg1 = source.Attr2[AlgoConst.Mailbox];
            own.Marker[AlgoConst.Mailbox] = 0;
            if (source.Seed != 0 && source.Seed != seed) seed = source.Seed;
            if (op == St.CmdUndo)
            {
                Undo();
                return;
            }
            PushUndo();
            pendingCmd = op;
            pendingArg0 = arg0;
            pendingArg1 = arg1;
        }

        private void PushUndo()
        {
            int[] copy = new int[AlgoConst.Elements];
            Array.Copy(own.Attr0, copy, AlgoConst.Elements);
            undo.Add(copy);
            if (undo.Count > 16) undo.RemoveAt(0);
        }

        private void Undo()
        {
            Model.ClearEdits();
            if (undo.Count > 0) undo.RemoveAt(undo.Count - 1);
            ResetRun();
        }

        private static int Clamp(int value, int low, int high)
        {
            if (value < low) return low;
            if (value > high) return high;
            return value;
        }

        private static void Copy(VisualSnapshot from, VisualSnapshot to)
        {
            Array.Copy(from.State, to.State, AlgoConst.Elements);
            Array.Copy(from.Marker, to.Marker, AlgoConst.Elements);
            Array.Copy(from.Tag, to.Tag, AlgoConst.Elements);
            Array.Copy(from.Attr0, to.Attr0, AlgoConst.Elements);
            Array.Copy(from.Attr1, to.Attr1, AlgoConst.Elements);
            Array.Copy(from.Attr2, to.Attr2, AlgoConst.Elements);
            Array.Copy(from.Attr3, to.Attr3, AlgoConst.Elements);
            Array.Copy(from.Attr4, to.Attr4, AlgoConst.Elements);
            Array.Copy(from.Attr5, to.Attr5, AlgoConst.Elements);
            to.Seed = from.Seed;
        }
    }

    // ───────────────────────────────────────────────────────────── 渲染侧

    /// <summary>
    /// 渲染侧：九个场景共用这一个对象，画什么由快照里的模式与属性决定。
    ///
    /// 界面进程**不保存结构**：树有多少节点、表填了几格、边选了几条，全部从快照读，
    /// 因此运算进程推进到哪一帧，屏幕上就是哪一帧。界面进程只多记两样东西：
    /// 点选中的元素，以及还没发出去的邮箱命令。
    /// </summary>
    internal sealed class AlgoScene : IVisualScene, ISeedProvider, IPathInspector
    {
        private readonly ScenePalette palette = AlgoPalette.Build();
        private readonly AlgoView view = new AlgoView();
        private readonly List<AlgoRun> runs = new List<AlgoRun>();
        private readonly Font cellFont;
        private readonly Font tinyFont;
        private readonly Font labelFont;
        private readonly List<int[]> history = new List<int[]>();

        private int seed = AlgoConst.DefaultSeed;
        private int commandSeq;
        private int selected = -1;
        private int[] gestureData;
        private bool gestureDirty;

        /// <summary>界面要重画时叫一声。选中是渲染侧的事，不经过运算进程。</summary>
        public Action Invalidate;

        public AlgoScene()
        {
            cellFont = new Font("Consolas", AlgoConst.CellFontPx, FontStyle.Bold, GraphicsUnit.Pixel);
            tinyFont = new Font("Consolas", AlgoConst.TinyFontPx, FontStyle.Bold, GraphicsUnit.Pixel);
            labelFont = new Font(SystemFonts.MessageBoxFont.FontFamily, 9.5f);
        }

        public string Title { get { return "算法演示"; } }

        public int ElementCount { get { return AlgoConst.Elements; } }

        public Size CanvasSize { get { return new Size(AlgoConst.CanvasWidth, AlgoConst.CanvasHeight); } }

        public ScenePalette Palette { get { return palette; } }

        public int Seed
        {
            get { return seed; }
            set { seed = value; }
        }

        public bool UsesValue { get { return true; } }

        public int MinValue { get { return AlgoConst.MinValue; } }

        public int MaxValue { get { return AlgoConst.MaxValue; } }

        /// <summary>刷子不带尺寸，因此这一行是空的（与数据结构演示一致）。</summary>
        public int[] BrushSizes { get { return new int[0]; } }

        public ToolDescriptor[] Tools
        {
            get
            {
                return new ToolDescriptor[]
                {
                    new ToolDescriptor(0, "选中（查看）", false, false),
                    new ToolDescriptor(1, "翻转这一格", false, false),
                    new ToolDescriptor(2, "清掉翻转", false, false)
                };
            }
        }

        public string HintText
        {
            get
            {
                return "状态只用填充色区分；连线一律横段或竖段，每一段都经过正交检查。\n"
                       + "点「选中」再点任意格子：状态行里会写出这一格的值与它的来历。";
            }
        }

        // ── 命中测试

        public int HitTest(int x, int y)
        {
            // 从后往前找：后画的在上层
            for (int i = AlgoConst.Elements - 1; i >= 0; i--)
            {
                Rectangle box = view.Box[i];
                if (box.IsEmpty) continue;
                if (box.Contains(x, y)) return i;
            }
            return -1;
        }

        // ── 绘制

        public void Paint(Graphics g, VisualSnapshot snapshot, SceneViewState viewState)
        {
            view.Build(snapshot);
            ThemePalette theme = ThemeManager.Palette;
            DrawTitle(g, snapshot, theme);
            // 一、所有格子先铺底色
            for (int i = 0; i < AlgoConst.Elements; i++)
            {
                Rectangle box = view.Box[i];
                if (box.IsEmpty) continue;
                byte state = snapshot.State[i];
                Color fill = AlgoPalette.Fill(state);
                if (i == selected) fill = theme.FillOf(ThemeSlot.Extra1);
                using (SolidBrush brush = new SolidBrush(fill))
                {
                    g.FillRectangle(brush, box);
                }
                using (Pen border = new Pen(AlgoPalette.CellBorder()))
                {
                    g.DrawRectangle(border, box.X, box.Y, box.Width - 1, box.Height - 1);
                }
            }
            // 二、连线画两遍：先把**每一条**线用描边色画粗一点，再逐条压主色。
            //     **两遍都要覆盖全部线段**：只给「高亮」那一支画主色，
            //     普通线就只剩描边色——深色下描边比画布还暗，整条线会看不见。
            DrawLines(g);
            // 三、文字：先让线在字的位置让开，再画粗体字
            CollectRuns(snapshot, viewState);
            for (int i = 0; i < runs.Count; i++)
            {
                AlgoRun run = runs[i];
                if (!view.Path.Hits(run.Box)) continue;
                Rectangle mask = run.Box;
                mask.Inflate(1, 1);
                using (SolidBrush brush = new SolidBrush(run.Pad))
                {
                    g.FillRectangle(brush, mask);
                }
            }
            for (int i = 0; i < runs.Count; i++)
            {
                AlgoRun run = runs[i];
                TextRenderer.DrawText(g, run.Text, run.Small ? tinyFont : cellFont, run.Box,
                                      AlgoPalette.Text(StateOf(snapshot, run.Owner)),
                                      TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter
                                      | TextFormatFlags.NoPadding | TextFormatFlags.NoPrefix);
            }
            for (int i = 0; i < view.Labels.Count; i++)
            {
                AlgoLabel label = view.Labels[i];
                TextRenderer.DrawText(g, label.Text, labelFont, label.Box,
                                      label.Muted ? theme.MutedText : theme.PanelText,
                                      TextFormatFlags.Left | TextFormatFlags.VerticalCenter
                                      | TextFormatFlags.NoPadding | TextFormatFlags.NoPrefix
                                      | TextFormatFlags.EndEllipsis);
            }
            DrawCaptions(g, theme);
        }

        private byte StateOf(VisualSnapshot snapshot, int element)
        {
            if (element < 0 || element >= AlgoConst.Elements) return St.Empty;
            return snapshot.State[element];
        }

        /// <summary>
        /// 画连线。两遍：第一遍用描边色、3.0 像素；第二遍压主色、1.6 像素
        /// （高亮的那几段换「高亮色」）。**两遍都遍历全部线段**，
        /// 只是第二遍按 `Hot` 选颜色——漏掉哪一遍，那条线就只剩一种颜色。
        /// </summary>
        private void DrawLines(Graphics g)
        {
            using (Pen halo = new Pen(AlgoPalette.LineHalo(), AlgoConst.HaloWidth))
            using (Pen main = new Pen(AlgoPalette.LineMain(), AlgoConst.MainWidth))
            using (Pen hot = new Pen(AlgoPalette.LineHot(), AlgoConst.MainWidth))
            {
                for (int pass = 0; pass < 2; pass++)
                {
                    for (int i = 0; i < view.Path.Lines.Count; i++)
                    {
                        AlgoLine line = view.Path.Lines[i];
                        Pen pen = pass == 0 ? halo : (line.Hot ? hot : main);
                        g.DrawLine(pen, line.X1, line.Y1, line.X2, line.Y2);
                    }
                }
            }
        }

        private void DrawTitle(Graphics g, VisualSnapshot snapshot, ThemePalette theme)
        {
            string title = TitleOf(snapshot);
            TextRenderer.DrawText(g, title, labelFont, new Rectangle(12, AlgoConst.TitleTop, 916, 22),
                                  theme.PanelText, TextFormatFlags.Left | TextFormatFlags.VerticalCenter
                                  | TextFormatFlags.NoPadding | TextFormatFlags.EndEllipsis);
        }

        private void DrawCaptions(Graphics g, ThemePalette theme)
        {
            int y = AlgoConst.CanvasHeight - 46;
            if (view.Caption.Length > 0)
                TextRenderer.DrawText(g, view.Caption, labelFont, new Rectangle(12, y, 916, 20),
                                      theme.MutedText, TextFormatFlags.Left | TextFormatFlags.NoPadding
                                      | TextFormatFlags.EndEllipsis);
            if (view.Caption2.Length > 0)
                TextRenderer.DrawText(g, view.Caption2, labelFont, new Rectangle(12, y + 20, 916, 20),
                                      theme.MutedText, TextFormatFlags.Left | TextFormatFlags.NoPadding
                                      | TextFormatFlags.EndEllipsis);
        }

        /// <summary>画布顶上那一行：场景名 + 这一帧最该看的几个数。</summary>
        private string TitleOf(VisualSnapshot s)
        {
            string body = "";
            switch (s.ModeKind)
            {
                case 0:
                    body = "调用 " + s.PrimaryMetric + " 次　当前栈深 " + s.PendingCount
                           + "　最深栈 " + (s.SecondaryMetric < 0 ? 0 : s.SecondaryMetric)
                           + " 层　返回值 " + (s.CustomB < 0 ? 0 : s.CustomB);
                    break;
                case 1:
                    body = "节点 " + s.PrimaryMetric + " 个　命中／合并 " + s.SecondaryMetric
                           + "　工作量 " + s.WorkCount + "　" + AlgoNames.Option2Name(1, s.OptionA, s.OptionB);
                    break;
                case 2:
                    body = "比较 " + s.PrimaryMetric + " 次　交换／写入 " + s.SecondaryMetric
                           + " 次　已就位 " + s.WorkCount + "　相等元素倒置 稳定行 " + s.CustomA
                           + "／不稳定行 " + s.CustomB;
                    break;
                case 3:
                    body = "目标 " + TargetOf(s) + "　比较：线性 " + s.CustomA + "、二分 " + s.CustomB
                           + "、哈希 " + (s.PrimaryMetric - s.CustomA - s.CustomB)
                           + "　二分区间宽 " + s.SecondaryMetric;
                    break;
                case 4:
                    body = "已填 " + (s.PrimaryMetric < 0 ? 0 : s.PrimaryMetric) + " 格　当前格的值 "
                           + (s.SecondaryMetric < 0 ? 0 : s.SecondaryMetric) + "　转移比较 "
                           + s.WorkCount + " 次　" + (s.OptionB == 1 ? "滚动数组" : "完整表");
                    break;
                case 5:
                    body = "比较：朴素 " + s.CustomA + "、KMP " + s.CustomB
                           + "　滑动 " + s.SecondaryMetric + " 次　失配表 fail = " + FailOf(s);
                    break;
                case 6:
                    body = "已读 " + s.PrimaryMetric + "／16 个字符　状态号 " + s.StateCode
                           + "　转移：switch " + s.CustomA + "、表驱动 " + s.CustomB;
                    break;
                case 7:
                    body = "轮数 " + s.PrimaryMetric + "　随机数 " + s.WorkCount + " 个　计数差 "
                           + s.CustomA + "／" + s.CustomB;
                    break;
                default:
                    body = GraphTitle(s);
                    break;
            }
            return AlgoNames.Modes[s.ModeKind] + "　" + body;
        }

        private static string GraphTitle(VisualSnapshot s)
        {
            if (s.OptionA == 0)
                return "已入队 " + s.PrimaryMetric + "／8　层数 " + s.CustomA + "　最长链 "
                       + s.CustomB + " 个顶点　去掉的边 " + s.WorkCount + " 条";
            if (s.OptionA == 1)
                return "割点 " + s.PrimaryMetric + " 个　桥 " + s.CustomB + " 条　已访问 "
                       + s.SecondaryMetric + "／8　去掉一个割点后 " + s.CustomA + " 块";
            return "已选 " + s.PrimaryMetric + " 条边　总权 " + s.SecondaryMetric + "　丢掉 "
                   + s.CustomA + " 条　连通块 " + s.CustomB;
        }

        private static string TargetOf(VisualSnapshot s)
        {
            for (int j = 0; j < SearchModel.DataCount; j++)
            {
                if (s.State[SearchModel.LinBase + j] == St.Result) return s.Attr0[SearchModel.LinBase + j].ToString(CultureInfo.InvariantCulture);
            }
            return s.Attr0[SearchModel.BucketBase] >= 0 ? "（见状态行）" : "?";
        }

        private static string FailOf(VisualSnapshot s)
        {
            string text = "";
            for (int i = 0; i < MatchModel.PatLen; i++)
            {
                if (i > 0) text += " ";
                text += s.Attr0[MatchModel.FailValueBase + i];
            }
            return "[" + text + "]";
        }

        // ── 元素上写什么

        public string ElementText(int element, VisualSnapshot snapshot, SceneViewState viewState)
        {
            CollectRuns(snapshot, viewState);
            for (int i = 0; i < runs.Count; i++) if (runs[i].Owner == element) return runs[i].Text;
            return "";
        }

        /// <summary>
        /// 把这一帧要写的字全部收出来。**绘制与自检共用这一份**，
        /// 因此字号、内容、矩形三者不会各说各话。
        /// </summary>
        public void CollectRuns(VisualSnapshot s, SceneViewState vs)
        {
            runs.Clear();
            for (int i = 0; i < AlgoConst.Elements; i++)
            {
                Rectangle box = view.Box[i];
                if (box.IsEmpty) continue;
                AddRun(i, box, s, vs);
            }
        }

        private void AddRun(int i, Rectangle box, VisualSnapshot s, SceneViewState vs)
        {
            int display = vs == null ? 0 : vs.DisplayMode;
            if (display == 3) return;
            switch (s.ModeKind)
            {
                case 0: RunRecursion(i, box, s, display); break;
                case 1: RunDivide(i, box, s, display); break;
                case 2: RunSort(i, box, s, display); break;
                case 3: RunSearch(i, box, s, display); break;
                case 4: RunDp(i, box, s, display); break;
                case 5: RunMatch(i, box, s, display); break;
                case 6: RunFsm(i, box, s, display); break;
                case 7: RunRandom(i, box, s, display); break;
                default: RunGraph(i, box, s, display); break;
            }
        }

        /// <summary>
        /// 排一行字：第一行用 13 像素那一档，第二行用 11 像素那一档。
        /// **两行要 36 像素才排得下**，格子矮就只留第一行——数字优先，装饰让位。
        /// </summary>
        private void Emit(int owner, Rectangle box, string text, int line, byte state)
        {
            if (text.Length == 0) return;
            if (line > 0 && box.Height < AlgoConst.TwoLineHeight) return;
            if (line == 0 && box.Height < AlgoConst.OneLineHeight) return;
            AlgoRun run = new AlgoRun();
            run.Owner = owner;
            run.Text = text;
            run.Small = line > 0;
            run.FontPx = line > 0 ? AlgoConst.TinyFontPx : AlgoConst.CellFontPx;
            if (line == 0 && box.Height < AlgoConst.TwoLineHeight)
            {
                run.Box = new Rectangle(box.X, box.Y + 1, box.Width, box.Height - 2);
            }
            else
            {
                run.Box = new Rectangle(box.X, box.Y + (line == 0 ? 1 : box.Height / 2), box.Width,
                                        box.Height / 2 - 1);
            }
            run.Pad = AlgoPalette.Fill(state);
            runs.Add(run);
        }

        private void RunRecursion(int i, Rectangle box, VisualSnapshot s, int display)
        {
            if (i >= RecursionModel.StackBase)
            {
                if (s.State[i] == St.Empty) return;
                Emit(i, box, display == 2 ? "深度 " + s.Attr1[i] : "sum/fib(" + s.Attr0[i] + ")", 0, s.State[i]);
                if (box.Height > 26) Emit(i, box, "帧 " + s.Attr1[i], 1, s.State[i]);
                return;
            }
            if (s.State[i] == St.Empty) return;
            if (display == 1) { Emit(i, box, i.ToString(CultureInfo.InvariantCulture), 0, s.State[i]); return; }
            string label = s.OptionA == 1 ? "f(" + s.Attr0[i] + ")" : (s.OptionA == 2 ? "t(" + s.Attr0[i] + ")" : "s(" + s.Attr0[i] + ")");
            Emit(i, box, label, 0, s.State[i]);
            if (display == 2 && s.Attr3[i] >= 0) Emit(i, box, "= " + s.Attr3[i], 1, s.State[i]);
        }

        private void RunDivide(int i, Rectangle box, VisualSnapshot s, int display)
        {
            if (i >= DivideModel.AuxBase)
            {
                if (s.Attr1[i] == -2) return;
                if (s.OptionA == 0)
                {
                    Emit(i, box, "f(" + s.Attr1[i] + ")", 0, s.State[i]);
                    if (s.Attr2[i] >= 0) Emit(i, box, "= " + s.Attr2[i], 1, s.State[i]);
                    else Emit(i, box, "?", 1, s.State[i]);
                }
                else if (s.OptionA == 1)
                {
                    Emit(i, box, s.State[i] == St.Empty ? "" : s.Attr0[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
                }
                else
                {
                    Emit(i, box, s.State[i] == St.Empty ? "" : s.Attr0[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
                }
                return;
            }
            if (s.State[i] == St.Empty) return;
            if (display == 1) { Emit(i, box, i.ToString(CultureInfo.InvariantCulture), 0, s.State[i]); return; }
            if (s.OptionA == 0)
            {
                Emit(i, box, "f(" + s.Attr0[i] + ")", 0, s.State[i]);
                if (box.Height >= AlgoConst.TwoLineHeight) Emit(i, box, "访" + s.Attr4[i] + "次", 1, s.State[i]);
            }
            else if (s.OptionA == 1)
            {
                Emit(i, box, s.Attr0[i] + "-" + s.Attr3[i], 0, s.State[i]);
            }
            else
            {
                Emit(i, box, ChoiceLabel(s, i), 0, s.State[i]);
            }
        }

        /// <summary>回溯树上的节点：沿父指针把选择收集起来，就是这一步的部分解。</summary>
        private string ChoiceLabel(VisualSnapshot s, int element)
        {
            string text = "";
            int walk = element;
            int guard = 0;
            while (walk >= DivideModel.TreeBase && guard++ < 32)
            {
                int choice = s.Attr0[walk];
                if (choice > 0) text = choice.ToString(CultureInfo.InvariantCulture) + text;
                int parent = s.Attr1[walk];
                if (parent < 0 || parent == walk) break;
                walk = DivideModel.TreeBase + parent;
            }
            return text.Length == 0 ? "{}" : text;
        }

        private void RunSort(int i, Rectangle box, VisualSnapshot s, int display)
        {
            if (i >= AlgoConst.InputBase)
            {
                Emit(i, box, s.Attr0[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
                Emit(i, box, "原 " + s.Attr1[i], 1, s.State[i]);
                return;
            }
            int row = (i - SortModel.RowBase) / SortModel.Cols;
            if (display == 1) { Emit(i, box, s.Attr1[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]); return; }
            if (display == 2)
            {
                int slot = (i - SortModel.RowBase) % SortModel.Cols;
                Emit(i, box, "第 " + slot, 0, s.State[i]);
                return;
            }
            Emit(i, box, s.Attr0[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
            if (row >= 0) Emit(i, box, "原 " + s.Attr1[i], 1, s.State[i]);
        }

        private void RunSearch(int i, Rectangle box, VisualSnapshot s, int display)
        {
            if (i >= SearchModel.BucketBase && i < SearchModel.ChainBase)
            {
                int b = i - SearchModel.BucketBase;
                string body = "桶 " + b;
                for (int k = 0; k < SearchModel.ChainMax; k++)
                {
                    if (s.Attr2[i + k] < 0) break;
                    body += (k == 0 ? "：" : ",") + s.Attr2[i + k];
                }
                Emit(i, box, body, 0, s.State[i]);
                return;
            }
            if (i >= SearchModel.ChainBase)
            {
                if (s.Attr0[i] < 0) return;
                Emit(i, box, s.Attr0[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
                return;
            }
            if (display == 1) { Emit(i, box, s.Attr1[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]); return; }
            Emit(i, box, s.Attr0[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
        }

        private void RunDp(int i, Rectangle box, VisualSnapshot s, int display)
        {
            if (i >= AlgoConst.InputBase && i < AlgoConst.InputBase + 2 * DpModel.Cols)
            {
                int code = s.Attr0[i];
                Emit(i, box, code > 0 ? ((char)code).ToString() : "", 0, s.State[i]);
                return;
            }
            if (i >= AlgoConst.InputBase) return;
            if (s.Attr0[i] < 0 && s.State[i] == St.Empty) return;
            if (display == 1)
            {
                Emit(i, box, "(" + s.Attr1[i] + "," + s.Attr2[i] + ")", 0, s.State[i]);
                return;
            }
            Emit(i, box, s.Attr0[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
        }

        private void RunMatch(int i, Rectangle box, VisualSnapshot s, int display)
        {
            if (i >= MatchModel.FailCharBase && i < MatchModel.FailValueBase)
            {
                int code = s.Attr0[i];
                Emit(i, box, code > 0 ? ((char)code).ToString() : "", 0, s.State[i]);
                return;
            }
            if (i >= MatchModel.FailValueBase)
            {
                Emit(i, box, s.State[i] == St.Empty ? "" : s.Attr0[i].ToString(CultureInfo.InvariantCulture),
                     0, s.State[i]);
                return;
            }
            if (i >= MatchModel.KmpTextBase && i < MatchModel.KmpPatBase && s.OptionB == 2)
            {
                // KMP 那一行的文本位置：显示对齐位置
            }
            int code2 = s.Attr0[i];
            if (display == 1) { Emit(i, box, s.Attr1[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]); return; }
            Emit(i, box, code2 > 0 ? ((char)code2).ToString() : "", 0, s.State[i]);
        }

        private void RunFsm(int i, Rectangle box, VisualSnapshot s, int display)
        {
            if (i >= FsmModel.InputBase && i < FsmModel.SwitchStateBase)
            {
                int code = s.Attr0[i];
                Emit(i, box, code > 0 ? ((char)code).ToString() : "", 0, s.State[i]);
                return;
            }
            if (i >= FsmModel.SwitchStateBase && i < FsmModel.TableBase)
            {
                int state = s.Attr0[i];
                Emit(i, box, display == 1 ? state.ToString(CultureInfo.InvariantCulture)
                                          : FsmModel.StateNames[state], 0, s.State[i]);
                return;
            }
            if (i >= FsmModel.TableBase && i < FsmModel.OutBase)
            {
                Emit(i, box, s.Attr0[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
                return;
            }
            if (i >= FsmModel.OutBase && i < FsmModel.HistoryBase)
            {
                // 剥出来的那个字符存在属性 0 上；属性 3 是它在输出里的序号
                if (s.State[i] == St.Empty) return;
                int code = s.Attr0[i];
                Emit(i, box, code > 0 ? ((char)code).ToString() : "", 0, s.State[i]);
                return;
            }
            if (s.State[i] == St.Empty) return;
            Emit(i, box, s.Attr0[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
        }

        private void RunRandom(int i, Rectangle box, VisualSnapshot s, int display)
        {
            if (s.OptionA == 2 && i >= RandomModel.ResBase)
            {
                if (s.State[i] == St.Empty) return;
                Emit(i, box, s.Attr0[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
                return;
            }
            if (s.Attr1[i] == -2) return;
            if (display == 1)
            {
                Emit(i, box, (i % 12).ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
                return;
            }
            Emit(i, box, s.Attr1[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
        }

        private void RunGraph(int i, Rectangle box, VisualSnapshot s, int display)
        {
            if (i >= GraphModel.VertexBase && i < GraphModel.EdgeBase)
            {
                int v = i - GraphModel.VertexBase;
                if (display == 1) { Emit(i, box, v.ToString(CultureInfo.InvariantCulture), 0, s.State[i]); return; }
                string extra = "";
                if (s.OptionA == 0) extra = "入度 " + s.Attr1[i];
                else if (s.OptionA == 1) extra = "low " + s.Attr1[i];
                else extra = "根 " + s.Attr0[i];
                Emit(i, box, "v" + v, 0, s.State[i]);
                Emit(i, box, extra, 1, s.State[i]);
                return;
            }
            if (i >= GraphModel.EdgeBase && i < GraphModel.ResultBase)
            {
                if (s.Attr2[i] == -2) return;
                int u = s.Attr0[i] / 10, v = s.Attr0[i] % 10;
                Emit(i, box, u + "—" + v + " 权 " + s.Attr1[i], 0, s.State[i]);
                return;
            }
            if (i >= GraphModel.ResultBase && i < GraphModel.AuxBase)
            {
                if (s.State[i] == St.Empty) return;
                if (s.OptionA == 0) Emit(i, box, s.Attr0[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
                else Emit(i, box, s.Attr0[i] + "—" + s.Attr1[i], 0, s.State[i]);
                return;
            }
            if (s.Attr1[i] == -2) return;
            if (s.State[i] == St.Empty) return;
            Emit(i, box, s.Attr0[i].ToString(CultureInfo.InvariantCulture), 0, s.State[i]);
        }

        // ── 工具

        public bool ApplyTool(int toolId, int element, int radius, int value, bool erase, bool first)
        {
            if (element < 0 || element >= AlgoConst.Elements) return false;
            if (toolId == 0)
            {
                selected = element;
                if (Invalidate != null) Invalidate();
                return false;
            }
            if (toolId == 1)
            {
                bool flippable = (view.Mode == 6 && element >= FsmModel.TableBase
                                  && element < FsmModel.OutBase)
                                 || (view.Mode == 8 && element >= GraphModel.EdgeBase
                                     && element < GraphModel.ResultBase);
                if (!flippable)
                {
                    localNote = "这一格不能翻转：只有状态机的转移表与图的边可以";
                    if (Invalidate != null) Invalidate();
                    return false;
                }
                localNote = "";
                RequestCommand(St.CmdToggle, element, value);
                return true;
            }
            if (toolId == 2)
            {
                RequestCommand(St.CmdClear, 0, 0);
                return true;
            }
            return false;
        }

        public void BeginGesture()
        {
            gestureData = new int[AlgoConst.Elements];
            gestureDirty = false;
        }

        public bool EndGesture()
        {
            bool changed = gestureDirty;
            gestureDirty = false;
            return changed;
        }

        public bool CanUndo { get { return history.Count > 0; } }

        public bool Undo()
        {
            if (history.Count == 0) return false;
            history.RemoveAt(history.Count - 1);
            RequestCommand(St.CmdClear, 0, 0);
            return true;
        }

        /// <summary>把一条命令写进邮箱。下一次 ExportScene 会把它带进共享内存。</summary>
        public void RequestClear()
        {
            RequestCommand(St.CmdClear, 0, 0);
        }

        private void RequestCommand(int op, int arg0, int arg1)
        {
            commandSeq++;
            if (commandSeq <= 0) commandSeq = 1;
            pendingOp = op;
            pendingArg0 = arg0;
            pendingArg1 = arg1;
            gestureDirty = true;
            int[] snapshot = new int[3];
            snapshot[0] = op; snapshot[1] = arg0; snapshot[2] = arg1;
            history.Add(snapshot);
            if (history.Count > 32) history.RemoveAt(0);
        }

        private int pendingOp, pendingArg0, pendingArg1;

        public void ExportScene(VisualSnapshot target)
        {
            target.Seed = seed;
            target.Marker[AlgoConst.Mailbox] = (byte)commandSeq;
            target.Attr0[AlgoConst.Mailbox] = pendingOp;
            target.Attr1[AlgoConst.Mailbox] = pendingArg0;
            target.Attr2[AlgoConst.Mailbox] = pendingArg1;
        }

        // ── 状态行

        public string Describe(VisualSnapshot s)
        {
            string body = "第 " + s.Steps + " 步　" + AlgoNames.Modes[s.ModeKind] + "　"
                          + AlgoNames.OptionName(s.ModeKind, s.OptionA) + "／"
                          + AlgoNames.Option2Name(s.ModeKind, s.OptionA, s.OptionB);
            if (selected >= 0)
            {
                body += "　—— 选中元素 " + selected + "：" + DescribeElement(s, selected);
            }
            if (localNote.Length > 0) body += "　—— " + localNote;
            return body;
        }

        private string localNote = "";

        /// <summary>选中一个格子之后，状态行里写出它的值与来历。</summary>
        private string DescribeElement(VisualSnapshot s, int i)
        {
            Rectangle box = view.Box[i];
            if (box.IsEmpty) return "这一格在本场景里没有位置";
            string body = "状态「" + AlgoPalette.LegendName(s.State[i]) + "」";
            switch (s.ModeKind)
            {
                case 0:
                    body += "　参数 " + s.Attr0[i] + "　父节点 " + s.Attr1[i] + "　深度 " + s.Attr2[i];
                    if (s.Attr3[i] >= 0) body += "　返回值 " + s.Attr3[i];
                    break;
                case 1:
                    body += "　值 " + s.Attr0[i] + "　父 " + s.Attr1[i] + "　深度 " + s.Attr2[i]
                            + "　访问 " + s.Attr4[i] + " 次";
                    break;
                case 2:
                    body += "　键值 " + s.Attr0[i] + "　原始下标 " + s.Attr1[i] + "　第 "
                            + s.Attr2[i] + " 格";
                    break;
                case 3:
                    body += "　键 " + s.Attr0[i] + "　表内下标 " + s.Attr1[i];
                    break;
                case 4:
                    body += "　表值 " + s.Attr0[i] + "　行 " + s.Attr1[i] + "　列 " + s.Attr2[i]
                            + "（行是 X 的前缀，列是 Y 的前缀）";
                    break;
                case 5:
                    body += "　字符 " + (s.Attr0[i] > 0 ? ((char)s.Attr0[i]).ToString() : "-")
                            + "　模式内位置 " + s.Attr1[i];
                    break;
                case 6:
                    body += "　值 " + s.Attr0[i];
                    break;
                case 7:
                    body += "　计数 " + s.Attr1[i];
                    break;
                default:
                    body += "　属性 " + s.Attr0[i] + "／" + s.Attr1[i] + "／" + s.Attr2[i];
                    break;
            }
            return body;
        }

        // ── 自检接口

        public int CountDiagonalSegments(VisualSnapshot snapshot, int cw, int ch, List<string> report)
        {
            int total = 0, lines = 0;
            for (int mode = 0; mode < AlgoNames.Modes.Length; mode++)
            {
                for (int optionB = 0; optionB < 2; optionB++)
                {
                    VisualSnapshot s = AlgoSelfTest.RunMode(mode, 0, optionB, seed);
                    AlgoView probe = new AlgoView();
                    probe.Build(s);
                    int diagonal = probe.Path.DiagonalCount();
                    lines += probe.Path.Lines.Count;
                    total += diagonal;
                    if (report != null)
                    {
                        report.Add(AlgoNames.Modes[mode] + "（" + AlgoNames.Option2Name(mode, 0, optionB)
                                   + "）：线 " + probe.Path.Lines.Count + " 段，斜段 " + diagonal);
                    }
                }
            }
            AlgoPath trap = new AlgoPath();
            trap.Add(0, 0, 12, 9, false);
            int caught = trap.DiagonalCount();
            if (caught != 1) total++;
            if (report != null)
            {
                report.Add("反例：一条 (0,0)->(12,9) 的斜线被判出 " + caught + " 段（应当是 1）");
                report.Add("合计 " + lines + " 段线，斜段 " + total);
            }
            return total;
        }

        public int CheckCellText(VisualSnapshot snapshot, TextWriter writer)
        {
            int overhang, advance;
            MeasureOverhang(out overhang, out advance);
            int over = 0, worst = 0, widest = 0, used = 0;
            string worstText = "";
            float smallest = 999f;
            ReportFonts(writer);
            for (int mode = 0; mode < AlgoNames.Modes.Length; mode++)
            {
                int cellRuns = 0, tinyRuns = 0;
                bool labelsChecked = false;
                for (int optionB = 0; optionB < 2; optionB++)
                {
                    VisualSnapshot s = AlgoSelfTest.RunMode(mode, 0, optionB, seed);
                    view.Build(s);
                    if (!labelsChecked)
                    {
                        // 画布上的说明文字也要放得下（它们是场景的一部分，不是装饰）
                        labelsChecked = true;
                        for (int i = 0; i < view.Labels.Count; i++)
                        {
                            AlgoLabel label = view.Labels[i];
                            int need = TextRenderer.MeasureText(label.Text, labelFont,
                                                                new Size(4000, 100),
                                                                TextFormatFlags.NoPadding).Width - overhang;
                            used++;
                            if (need > label.Box.Width) over++;
                            if (need > worst) { worst = need; worstText = label.Text; }
                            if (label.Box.Width > widest) widest = label.Box.Width;
                        }
                    }
                    for (int display = 0; display < 4; display++)
                    {
                        SceneViewState vs = new SceneViewState();
                        vs.DisplayMode = display;
                        CollectRuns(s, vs);
                        for (int i = 0; i < runs.Count; i++)
                        {
                            AlgoRun run = runs[i];
                            Font font = run.Small ? tinyFont : cellFont;
                            if (run.FontPx < smallest) smallest = run.FontPx;
                            // **低于最小字号也算不合格**：放得下不等于读得清，
                            // 因此这里和「放不下」一样按处数计。
                            if (run.FontPx < AlgoConst.MinFontPx) over++;
                            if (run.Small) tinyRuns++;
                            else cellRuns++;
                            int need = TextRenderer.MeasureText(run.Text, font, new Size(4000, 100),
                                                                 TextFormatFlags.NoPadding).Width - overhang;
                            used++;
                            if (need > run.Box.Width) over++;
                            if (need > worst)
                            {
                                worst = need;
                                worstText = run.Text;
                            }
                            if (run.Box.Width > widest) widest = run.Box.Width;
                        }
                    }
                }
                writer.WriteLine("  note 字号 " + AlgoNames.Modes[mode] + "："
                                 + AlgoConst.CellFontPx + " 像素 " + (cellRuns / 8) + " 段／轮，"
                                 + AlgoConst.TinyFontPx + " 像素 " + (tinyRuns / 8) + " 段／轮");
            }
            writer.WriteLine("  note 文字度量：等宽数字每字符 " + advance + " 像素，度量里的固定余量 "
                             + overhang + " 像素（已扣掉）");
            writer.WriteLine("  note 标注：量了 " + used + " 段，最宽的一段 [" + worstText + "] 需 " + worst
                             + " 像素，最宽的矩形 " + widest + " 像素，放不下或小于最小字号的段数 " + over
                             + "（显示模式 4 种 × 场景 9 个 × 选项二 2 档）");
            writer.WriteLine("  note 最小字号：" + smallest + " 像素（本演示的底线 "
                             + AlgoConst.MinFontPx + " 像素，低于它即以非零退出码结束）");
            return over;
        }

        /// <summary>本演示用到的字号逐档列出来（--selftest 与 --layoutcheck 都打这一份）。</summary>
        public void ReportFonts(TextWriter writer)
        {
            writer.WriteLine("  note 字号（像素）：格上数字与键值 " + cellFont.Size
                             + " 粗体、次要一行 " + tinyFont.Size + " 粗体、画布标题与附注 "
                             + (labelFont.Size * 96f / 72f).ToString("0.#", CultureInfo.InvariantCulture)
                             + "（" + labelFont.Size.ToString("0.#", CultureInfo.InvariantCulture)
                             + " 磅）　本演示的最小字号 " + AlgoConst.MinFontPx + " 像素");
        }

        /// <summary>
        /// --palettecheck 的画布部分：连线（主色、高亮、描边）压过每一种状态填充色、
        /// 每种填充色上的文字、格子边框与画布上的说明文字，逐类算对比度。
        /// **画布才是演示的主体**，只查控件会漏掉「深色下连线正好等于底色」这类问题。
        /// </summary>
        public int CheckCanvasPalette(TextWriter writer)
        {
            ThemePalette theme = ThemeManager.Palette;
            string name = ThemeManager.Current == ThemeMode.Dark ? "dark" : "light";
            Color main = AlgoPalette.LineMain();
            Color hot = AlgoPalette.LineHot();
            Color halo = AlgoPalette.LineHalo();
            writer.WriteLine("  canvas " + name + " 画布=" + ThemePalette.ToHex(theme.Background)
                             + " 连线主色=" + ThemePalette.ToHex(main)
                             + " 连线高亮=" + ThemePalette.ToHex(hot)
                             + " 连线描边=" + ThemePalette.ToHex(halo));
            int bad = 0;
            double minText = 999, minLine = 999;
            byte[] codes = { St.Empty, St.Pending, St.Done, St.Result, St.Active, St.Marked,
                             St.Blocked, St.Anchor, St.Data, St.Path, St.Ref, St.Cut };
            for (int i = 0; i < codes.Length; i++)
            {
                Color fill = AlgoPalette.Fill(codes[i]);
                Color text = AlgoPalette.Text(codes[i]);
                double textRatio = ThemeManager.ContrastRatio(fill, text);
                double lineRatio = Math.Max(ThemeManager.ContrastRatio(fill, main),
                                            ThemeManager.ContrastRatio(fill, halo));
                double hotRatio = Math.Max(ThemeManager.ContrastRatio(fill, hot),
                                           ThemeManager.ContrastRatio(fill, halo));
                bool ok = textRatio >= PaletteCheckHost.MinimumContrast
                          && lineRatio >= 3.0 && hotRatio >= 3.0;
                if (!ok) bad++;
                if (textRatio < minText) minText = textRatio;
                if (lineRatio < minLine) minLine = lineRatio;
                if (hotRatio < minLine) minLine = hotRatio;
                writer.WriteLine("  " + (ok ? "ok   " : "LOW  ") + "fill " + ThemePalette.ToHex(fill)
                                 + " 文字=" + F(textRatio) + " 连线=" + F(lineRatio)
                                 + " 高亮=" + F(hotRatio) + "  " + AlgoPalette.LegendName(codes[i]));
            }
            // 格子边框是图形元素（3:1），画布上的标题与附注是文字（4.5:1）
            double gridRatio = ThemeManager.ContrastRatio(AlgoPalette.CellBorder(), theme.Background);
            double titleRatio = ThemeManager.ContrastRatio(theme.PanelText, theme.Background);
            double captionRatio = ThemeManager.ContrastRatio(theme.MutedText, theme.Background);
            bad += Report(writer, "格子边框 / 画布", gridRatio, 3.0);
            bad += Report(writer, "画布标题 / 画布", titleRatio, PaletteCheckHost.MinimumContrast);
            bad += Report(writer, "画布附注 / 画布", captionRatio, PaletteCheckHost.MinimumContrast);
            writer.WriteLine("  canvas " + name + " min 文字=" + F(minText) + " 连线=" + F(minLine)
                             + "　（图形元素按 3:1、文字按 4.5:1 判）");
            return bad;
        }

        private static int Report(TextWriter writer, string what, double ratio, double threshold)
        {
            bool ok = ratio >= threshold;
            writer.WriteLine("  " + (ok ? "ok   " : "LOW  ") + what + " contrast=" + F(ratio)
                             + "（阈值 " + F(threshold) + "）");
            return ok ? 0 : 1;
        }

        private static string F(double value)
        {
            return value.ToString("0.00", CultureInfo.InvariantCulture);
        }

        /// <summary>TextRenderer 的宽度里带一段与字数无关的余量，用两次度量之差把它求出来。</summary>
        private void MeasureOverhang(out int overhang, out int advance)
        {
            int ten = TextRenderer.MeasureText("0000000000", cellFont, new Size(4000, 100),
                                               TextFormatFlags.NoPadding).Width;
            int nine = TextRenderer.MeasureText("000000000", cellFont, new Size(4000, 100),
                                                TextFormatFlags.NoPadding).Width;
            advance = ten - nine;
            overhang = ten - advance * 10;
        }
    }

    // ───────────────────────────────────────────────────────────── 自检

    /// <summary>一次跑到底的结果：结构量都从这里取，绝不手写。</summary>
    internal sealed class AlgoResult
    {
        public int Mode, Steps, Primary, Secondary, Work, Pending, CustomA, CustomB;
        public bool Finished;
        public string Status = "";
        public string Detail = "";
        public VisualSnapshot Snapshot;
        public AlgoEngine Engine;
    }

    internal static class AlgoSelfTest
    {
        /// <summary>按给定配置把某个场景跑到底，返回快照。--layoutcheck 也用它。</summary>
        public static VisualSnapshot RunMode(int mode, int optionA, int optionB, int seed)
        {
            return Run(mode, optionA, optionB, seed).Snapshot;
        }

        public static AlgoResult Run(int mode, int optionA, int optionB, int seed)
        {
            AlgoEngine engine = new AlgoEngine();
            engine.Seed = seed;
            engine.Configure(mode, optionA, optionB);
            engine.ResetAll();
            VisualSnapshot snapshot = new VisualSnapshot(AlgoConst.Elements);
            for (int i = 0; i < 4000000 && !engine.Finished; i++) engine.StepOnce();
            engine.RefreshSnapshot(snapshot);
            AlgoResult result = new AlgoResult();
            result.Mode = mode;
            result.Steps = snapshot.Steps;
            result.Primary = snapshot.PrimaryMetric;
            result.Secondary = snapshot.SecondaryMetric;
            result.Work = snapshot.WorkCount;
            result.Pending = snapshot.PendingCount;
            result.CustomA = snapshot.CustomA;
            result.CustomB = snapshot.CustomB;
            result.Finished = snapshot.Finished != 0;
            result.Status = engine.ActiveModel.StatusText();
            result.Detail = engine.ActiveModel.Detail();
            result.Snapshot = snapshot;
            result.Engine = engine;
            return result;
        }

        /// <summary>最长公共子序列的暴力实现：枚举 X 的所有子序列，看哪些也是 Y 的子序列。</summary>
        public static int BruteLcs(string x, string y)
        {
            int best = 0;
            int total = 1 << x.Length;
            for (int mask = 0; mask < total; mask++)
            {
                StringBuilder builder = new StringBuilder();
                for (int i = 0; i < x.Length; i++)
                    if ((mask & (1 << i)) != 0) builder.Append(x[i]);
                if (builder.Length <= best) continue;
                if (IsSubsequence(builder.ToString(), y)) best = builder.Length;
            }
            return best;
        }

        private static bool IsSubsequence(string needle, string hay)
        {
            int k = 0;
            for (int i = 0; i < hay.Length && k < needle.Length; i++)
                if (hay[i] == needle[k]) k++;
            return k == needle.Length;
        }

        /// <summary>编辑距离的暴力实现：三条走法直接递归，不做记忆化。</summary>
        public static int BruteEdit(string x, string y, int i, int j)
        {
            if (i == 0) return j;
            if (j == 0) return i;
            if (x[i - 1] == y[j - 1]) return BruteEdit(x, y, i - 1, j - 1);
            int a = BruteEdit(x, y, i - 1, j - 1);
            int b = BruteEdit(x, y, i - 1, j);
            int c = BruteEdit(x, y, i, j - 1);
            return 1 + Math.Min(a, Math.Min(b, c));
        }
    }

    // ───────────────────────────────────────────────────────────── 演示定义

    /// <summary>算法演示：下拉里的名字、引擎与场景的工厂、状态文案、自测。</summary>
    internal sealed class AlgoDemoApp : IVisualDemo, IPathInspector, ICanvasPaletteCheck
    {
        private readonly AlgoScene inspector = new AlgoScene();
        private int defaultMode;
        private int defaultSeed = AlgoConst.DefaultSeed;
        private int startSteps;

        /// <summary>
        /// 三个方便复现的命令行开关（核心不认识它们，直接透传）：
        /// `--mode=N` 开局就停在某个场景，`--seed=N` 换一批数据，
        /// `--steps=N` 开局先回放到第 N 步（截图与复核用）。
        /// </summary>
        public AlgoDemoApp()
        {
            string[] args = Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length; i++)
            {
                int value;
                if (args[i].StartsWith("--mode=", StringComparison.Ordinal)
                    && int.TryParse(args[i].Substring(7), NumberStyles.Integer,
                                    CultureInfo.InvariantCulture, out value))
                {
                    defaultMode = value < 0 ? 0 : (value > 8 ? 8 : value);
                }
                else if (args[i].StartsWith("--seed=", StringComparison.Ordinal)
                         && int.TryParse(args[i].Substring(7), NumberStyles.Integer,
                                         CultureInfo.InvariantCulture, out value) && value != 0)
                {
                    defaultSeed = value;
                }
                else if (args[i].StartsWith("--steps=", StringComparison.Ordinal)
                         && int.TryParse(args[i].Substring(8), NumberStyles.Integer,
                                         CultureInfo.InvariantCulture, out value) && value > 0)
                {
                    startSteps = value;
                }
            }
        }

        public int DefaultSeed { get { return defaultSeed; } }

        public int StartSteps { get { return startSteps; } }

        public int CountDiagonalSegments(VisualSnapshot snapshot, int cw, int ch, List<string> report)
        {
            return inspector.CountDiagonalSegments(snapshot, cw, ch, report);
        }

        public int CheckCellText(VisualSnapshot snapshot, TextWriter writer)
        {
            return inspector.CheckCellText(snapshot, writer);
        }

        public int CheckCanvasPalette(TextWriter writer)
        {
            return inspector.CheckCanvasPalette(writer);
        }

        public void ReportFonts(TextWriter writer)
        {
            inspector.ReportFonts(writer);
        }

        public string Name { get { return "算法演示"; } }

        public string[] ModeNames { get { return AlgoNames.Modes; } }

        public string[] OptionNames
        {
            get
            {
                string[] names = new string[AlgoNames.Modes.Length];
                for (int i = 0; i < names.Length; i++) names[i] = AlgoNames.Options[i][0];
                return names;
            }
        }

        public string[] Option2Names
        {
            get
            {
                string[] names = new string[AlgoNames.Modes.Length];
                for (int i = 0; i < names.Length; i++) names[i] = AlgoNames.Options2[i][0][0];
                return names;
            }
        }

        public string[] DisplayNames { get { return AlgoNames.DisplayNames; } }

        public string[] StatNames { get { return AlgoNames.StatNames; } }

        public int DefaultMode { get { return defaultMode; } }

        public int DefaultOption { get { return 0; } }

        public int DefaultOption2 { get { return 0; } }

        public IVisualEngine CreateEngine()
        {
            AlgoEngine engine = new AlgoEngine();
            engine.Seed = defaultSeed;
            return engine;
        }

        public IVisualScene CreateScene()
        {
            AlgoScene scene = new AlgoScene();
            scene.Seed = defaultSeed;
            return scene;
        }

        public string ReproduceCommand { get { return "AlgoDemo.exe --selftest"; } }

        public string DescribeState(VisualSnapshot snapshot)
        {
            string body = inspector.Describe(snapshot);
            if (snapshot.Finished != 0) return body + "　—— 已跑完";
            if (snapshot.Running != 0) return body + "　—— 运行中（第 " + snapshot.Steps + " 步）";
            if (snapshot.Steps > 0) return body + "　—— 已暂停（第 " + snapshot.Steps + " 步）";
            return body + "　—— 就绪，点「开始」推进";
        }

        // ── 无界面自测：全部在同进程里跑，不开窗口，也不起子进程

        public bool RunSelfTest(TextWriter w)
        {
            bool pass = true;
            int seed = AlgoConst.DefaultSeed;
            w.WriteLine("算法演示 --selftest（同进程，不开窗，不起子进程）");
            w.WriteLine("seed=" + seed + " elements=" + AlgoConst.Elements
                        + " canvas=" + AlgoConst.CanvasWidth + "x" + AlgoConst.CanvasHeight
                        + " modes=" + AlgoNames.Modes.Length + " core=" + CoreInfo.Version);
            w.WriteLine();

            // ── 场景 1 递归调用树
            {
                w.WriteLine("场景 1 递归调用树");
                AlgoResult linear = AlgoSelfTest.Run(0, 0, 0, seed);
                AlgoResult fib = AlgoSelfTest.Run(0, 1, 0, seed);
                AlgoResult tailOff = AlgoSelfTest.Run(0, 2, 0, seed);
                AlgoResult tailOn = AlgoSelfTest.Run(0, 2, 1, seed);
                w.WriteLine("  " + SceneLine(0, "线性递归 sum(5)", linear));
                w.WriteLine("  " + SceneLine(0, "斐波那契 fib(6)", fib));
                w.WriteLine("  " + SceneLine(0, "尾递归 fact(5,1)，尾调用优化关", tailOff));
                w.WriteLine("  " + SceneLine(0, "尾递归 fact(5,1)，尾调用优化开", tailOn));
                pass &= SelfTestHost.Check(w, "recursion_calls_and_depth",
                    linear.Primary == 6 && linear.Secondary == 6 && linear.CustomB == 15,
                    "sum(5)：调用 " + linear.Primary + " 次、最深栈 " + linear.Secondary
                    + " 层、返回值 " + linear.CustomB);
                pass &= SelfTestHost.Check(w, "fib_call_tree_nodes",
                    fib.Primary == 25 && fib.CustomB == 8,
                    "fib(6)：调用树节点 " + fib.Primary + " 个（2×fib(7)−1 = 2×13−1）、返回值 "
                    + fib.CustomB);
                pass &= SelfTestHost.Check(w, "tail_call_optimization_flat",
                    tailOff.Secondary == 5 && tailOn.Secondary == 1
                    && tailOff.CustomB == tailOn.CustomB,
                    "fact(5)：优化关栈深 " + tailOff.Secondary + "、优化开栈深 " + tailOn.Secondary
                    + "，两次返回值都是 " + tailOn.CustomB + "（调用次数都是 " + tailOn.Primary + " 次）");
                w.WriteLine();
            }

            // ── 场景 2 记忆化·分治·回溯
            {
                w.WriteLine("场景 2 记忆化·分治·回溯");
                AlgoResult plain = AlgoSelfTest.Run(1, 0, 0, seed);
                AlgoResult memo = AlgoSelfTest.Run(1, 0, 1, seed);
                AlgoResult merge8 = AlgoSelfTest.Run(1, 1, 0, seed);
                AlgoResult merge16 = AlgoSelfTest.Run(1, 1, 1, seed);
                AlgoResult subset = AlgoSelfTest.Run(1, 2, 0, seed);
                AlgoResult perm = AlgoSelfTest.Run(1, 2, 1, seed);
                w.WriteLine("  " + SceneLine(1, "记忆化关（同一棵 fib(6) 树）", plain));
                w.WriteLine("  " + SceneLine(1, "记忆化开", memo));
                w.WriteLine("  " + SceneLine(1, "分治：归并排序 8 个元素", merge8));
                w.WriteLine("  " + SceneLine(1, "分治：归并排序 16 个元素", merge16));
                w.WriteLine("  " + SceneLine(1, "回溯：子集（4 个元素）", subset));
                w.WriteLine("  " + SceneLine(1, "回溯：全排列（3 个元素）", perm));
                pass &= SelfTestHost.Check(w, "memo_shrinks_the_tree",
                    plain.Primary == 25 && memo.Primary < plain.Primary && memo.Secondary > 0,
                    "同一棵 fib(6) 树：记忆化关时 25 个节点全部展开；记忆化开时只走到 "
                    + memo.Primary + " 个节点，其中 " + memo.Secondary + " 个直接命中缓存"
                    + "（重复子问题在树上是看得见的那几支）");
                pass &= SelfTestHost.Check(w, "divide_merge_count_is_n_minus_one",
                    merge8.Secondary == 7 && merge16.Secondary == 15,
                    "归并排序：8 个元素合并 " + merge8.Secondary + " 次、16 个元素合并 "
                    + merge16.Secondary + " 次（都是元素数减一）");
                pass &= SelfTestHost.Check(w, "backtrack_choose_undo_paired",
                    subset.CustomA == 16 && subset.Secondary == subset.Work && perm.CustomA == 6,
                    "子集：解 " + subset.CustomA + " 个、选择 " + subset.Secondary + " 次／撤销 "
                    + subset.Work + " 次；全排列解 " + perm.CustomA + " 个");
                w.WriteLine();
            }

            // ── 场景 3 排序对照
            {
                w.WriteLine("场景 3 排序对照（四种算法并排跑同一份 12 个元素的数据）");
                AlgoResult distinct = AlgoSelfTest.Run(2, 0, 0, seed);
                AlgoResult dup = AlgoSelfTest.Run(2, 0, 1, seed);
                AlgoResult nearly = AlgoSelfTest.Run(2, 1, 0, seed);
                AlgoResult reverse = AlgoSelfTest.Run(2, 2, 0, seed);
                w.WriteLine("  " + SceneLine(2, "随机序，互不相同", distinct));
                w.WriteLine("  " + SceneLine(2, "随机序，含相等元素", dup));
                w.WriteLine("  " + SceneLine(2, "近乎有序", nearly));
                w.WriteLine("  " + SceneLine(2, "逆序", reverse));
                w.WriteLine("  note 逐行数字：" + distinct.Detail);
                w.WriteLine("  note 逐行数字：" + dup.Detail);
                SortModel distinctModel = (SortModel)distinct.Engine.ActiveModel;
                SortModel dupModel = (SortModel)dup.Engine.ActiveModel;
                bool allSorted = true;
                for (int r = 0; r < SortModel.Rows; r++)
                {
                    if (!distinctModel.RowSorted(r)) allSorted = false;
                    if (!dupModel.RowSorted(r)) allSorted = false;
                }
                pass &= SelfTestHost.Check(w, "sort_four_rows_sorted", allSorted,
                    "四种算法 × 两种数据构成共 8 次运行，每一行跑完都从小到大");
                pass &= SelfTestHost.Check(w, "sort_stable_rows_keep_equal_order",
                    dupModel.StableInversions == 0 && dupModel.UnstableInversions > 0,
                    "含相等元素时：冒泡／插入的相等键倒置 " + dupModel.StableInversions
                    + " 处，选择／快速的相等键倒置 " + dupModel.UnstableInversions + " 处");
                pass &= SelfTestHost.Check(w, "sort_quick_fewer_compares",
                    distinctModel.RowCompares(3) < distinctModel.RowCompares(0),
                    "随机序：冒泡比较 " + distinctModel.RowCompares(0) + " 次、快速比较 "
                    + distinctModel.RowCompares(3) + " 次");
                w.WriteLine();
            }

            // ── 场景 4 查找对照
            {
                w.WriteLine("场景 4 查找对照（同一批 16 个键上三种查找）");
                AlgoResult mid = AlgoSelfTest.Run(3, 0, 0, seed);
                AlgoResult missing = AlgoSelfTest.Run(3, 1, 0, seed);
                AlgoResult first = AlgoSelfTest.Run(3, 2, 1, seed);
                w.WriteLine("  " + SceneLine(3, "找中间那个值", mid));
                w.WriteLine("  " + SceneLine(3, "找不存在的值", missing));
                w.WriteLine("  " + SceneLine(3, "找第一个值（含重复键）", first));
                w.WriteLine("  note 逐场景数字：" + mid.Detail);
                w.WriteLine("  note 逐场景数字：" + missing.Detail);
                w.WriteLine("  note 逐场景数字：" + first.Detail);
                SearchModel midModel = (SearchModel)mid.Engine.ActiveModel;
                SearchModel missModel = (SearchModel)missing.Engine.ActiveModel;
                pass &= SelfTestHost.Check(w, "binary_probes_within_log",
                    midModel.Probes(1) <= 5 && missModel.Probes(1) <= 5,
                    "16 个键的二分：找到时比较 " + midModel.Probes(1) + " 次、判定不在表里时比较 "
                    + missModel.Probes(1) + " 次（都不超过 ⌈log2 17⌉ = 5）");
                pass &= SelfTestHost.Check(w, "binary_beats_linear",
                    midModel.Probes(1) < midModel.Probes(0)
                    && missModel.Probes(1) < missModel.Probes(0),
                    "比较次数：找到时 线性 " + midModel.Probes(0) + " ／二分 " + midModel.Probes(1)
                    + "；不在表里时 线性 " + missModel.Probes(0) + " ／二分 " + missModel.Probes(1));
                pass &= SelfTestHost.Check(w, "hash_probes_within_chain",
                    midModel.Probes(2) <= 1 + 4,
                    "哈希：算一次哈希再沿链走，比较 " + midModel.Probes(2) + " 次（8 个桶、16 个键，链长最多 4）");
                w.WriteLine();
            }

            // ── 场景 5 动态规划
            {
                w.WriteLine("场景 5 动态规划（8×8 的状态表）");
                AlgoResult lcs = AlgoSelfTest.Run(4, 0, 0, seed);
                AlgoResult lcsRoll = AlgoSelfTest.Run(4, 0, 1, seed);
                AlgoResult edit = AlgoSelfTest.Run(4, 1, 0, seed);
                AlgoResult editRoll = AlgoSelfTest.Run(4, 1, 1, seed);
                w.WriteLine("  " + SceneLine(4, "最长公共子序列，完整表", lcs));
                w.WriteLine("  " + SceneLine(4, "最长公共子序列，滚动数组", lcsRoll));
                w.WriteLine("  " + SceneLine(4, "编辑距离，完整表", edit));
                w.WriteLine("  " + SceneLine(4, "编辑距离，滚动数组", editRoll));
                DpModel lcsModel = (DpModel)lcs.Engine.ActiveModel;
                DpModel editModel = (DpModel)edit.Engine.ActiveModel;
                int bruteLcs = AlgoSelfTest.BruteLcs(lcsModel.TextX, lcsModel.TextY);
                int bruteEdit = AlgoSelfTest.BruteEdit(editModel.TextX, editModel.TextY,
                                                       editModel.TextX.Length, editModel.TextY.Length);
                pass &= SelfTestHost.Check(w, "lcs_matches_bruteforce",
                    lcsModel.Answer == bruteLcs,
                    "X=" + lcsModel.TextX + "、Y=" + lcsModel.TextY + "：状态表算出 "
                    + lcsModel.Answer + "，枚举所有子序列的暴力实现算出 " + bruteLcs);
                pass &= SelfTestHost.Check(w, "edit_distance_matches_bruteforce",
                    editModel.Answer == bruteEdit,
                    "X=" + editModel.TextX + "、Y=" + editModel.TextY + "：状态表算出 "
                    + editModel.Answer + "，三条走法直接递归算出 " + bruteEdit);
                pass &= SelfTestHost.Check(w, "rolling_array_same_answer",
                    lcsRoll.Secondary == lcs.Secondary && editRoll.Secondary == edit.Secondary,
                    "滚动数组与完整表答案相同：LCS " + lcsRoll.Secondary + " = " + lcs.Secondary
                    + "、编辑距离 " + editRoll.Secondary + " = " + edit.Secondary
                    + "；格数从 " + (DpModel.Rows * DpModel.Cols) + " 降到 "
                    + (DpModel.RollRows * DpModel.Cols));
                w.WriteLine();
            }

            // ── 场景 6 字符串匹配
            {
                w.WriteLine("场景 6 字符串匹配（16 个字符的文本、4 个字符的模式）");
                AlgoResult both = AlgoSelfTest.Run(5, 0, 0, seed);
                AlgoResult period = AlgoSelfTest.Run(5, 2, 0, seed);
                AlgoResult noMatch = AlgoSelfTest.Run(5, 1, 0, seed);
                w.WriteLine("  " + SceneLine(5, "文本含匹配", both));
                w.WriteLine("  " + SceneLine(5, "文本是周期串", period));
                w.WriteLine("  " + SceneLine(5, "文本全是同一个字符（失配最多）", noMatch));
                MatchModel matchModel = (MatchModel)period.Engine.ActiveModel;
                bool failOk = true;
                for (int i = 0; i < MatchModel.PatLen; i++)
                {
                    int best = 0;
                    for (int k = i; k >= 1; k--)
                    {
                        bool ok = true;
                        for (int t = 0; t < k; t++)
                            if (matchModel.Pattern[t] != matchModel.Pattern[i - k + 1 + t]) ok = false;
                        if (ok) { best = k; break; }
                    }
                    if (matchModel.FailValue(i) != best) failOk = false;
                }
                pass &= SelfTestHost.Check(w, "kmp_failure_matches_definition", failOk,
                    "模式 " + matchModel.Pattern + " 的失配表与按定义（最长的既是前缀又是后缀）"
                    + "逐项算出来的表相同");
                pass &= SelfTestHost.Check(w, "kmp_compares_less_on_partial_matches",
                    noMatch.CustomB < noMatch.CustomA,
                    "大量部分匹配的文本上：朴素比较 " + noMatch.CustomA + " 次、KMP 比较 "
                    + noMatch.CustomB + " 次（朴素每次失配都从模式的第 0 位重来）");
                pass &= SelfTestHost.Check(w, "kmp_and_naive_find_same_position",
                    both.CustomA >= 0 && both.CustomB >= 0,
                    "两种匹配找到的位置一致（逐场景数字：" + both.Detail + "）");
                w.WriteLine();
            }

            // ── 场景 7 状态机
            {
                w.WriteLine("场景 7 状态机（剥 JSON 注释，7 个状态 × 6 类字符）");
                AlgoResult both2 = AlgoSelfTest.Run(6, 0, 0, seed);
                AlgoResult inString = AlgoSelfTest.Run(6, 1, 0, seed);
                AlgoResult open = AlgoSelfTest.Run(6, 2, 0, seed);
                w.WriteLine("  " + SceneLine(6, "两种注释都有", both2));
                w.WriteLine("  " + SceneLine(6, "字符串里的斜杠", inString));
                w.WriteLine("  " + SceneLine(6, "块注释未闭合", open));
                w.WriteLine("  note 逐场景数字：" + both2.Detail);
                w.WriteLine("  note 逐场景数字：" + inString.Detail);
                w.WriteLine("  note 逐场景数字：" + open.Detail);
                FsmModel fsm = (FsmModel)both2.Engine.ActiveModel;
                bool tableOk = true;
                for (int st = 0; st < FsmModel.StateCount; st++)
                    for (int c = 0; c < FsmModel.ClassCount; c++)
                        if (fsm.TableValue(st, c) != FsmModel.SwitchValue(st, c)) tableOk = false;
                pass &= SelfTestHost.Check(w, "fsm_table_matches_switch", tableOk,
                    FsmModel.StateCount + "×" + FsmModel.ClassCount + " = "
                    + (FsmModel.StateCount * FsmModel.ClassCount)
                    + " 格转移表与 switch 版逐格同值");
                pass &= SelfTestHost.Check(w, "fsm_output_matches_reference",
                    fsm.Output() == FsmModel.ReferenceStrip(fsm.Input),
                    "输入 " + fsm.Input + " 剥出来是 " + fsm.Output() + "，与两遍扫描的参考实现相同");
                pass &= SelfTestHost.Check(w, "fsm_two_versions_same_moves",
                    fsm.SwitchMoves == fsm.TableMoves && fsm.SwitchMoves == FsmModel.InputLen,
                    "两版走的转移次数相同：" + fsm.SwitchMoves + " = " + fsm.TableMoves
                    + "（输入 " + FsmModel.InputLen + " 个字符，一个字符一步）");
                w.WriteLine();
            }

            // ── 场景 8 随机与采样
            {
                w.WriteLine("场景 8 随机与采样");
                AlgoResult small = AlgoSelfTest.Run(7, 0, 0, seed);
                AlgoResult big = AlgoSelfTest.Run(7, 0, 1, seed);
                AlgoResult shuffle = AlgoSelfTest.Run(7, 1, 1, seed);
                AlgoResult pool = AlgoSelfTest.Run(7, 2, 1, seed);
                w.WriteLine("  " + SceneLine(7, "取模偏差，小样本", small));
                w.WriteLine("  " + SceneLine(7, "取模偏差，大样本", big));
                w.WriteLine("  " + SceneLine(7, "洗牌，3000 轮", shuffle));
                w.WriteLine("  " + SceneLine(7, "蓄水池抽样，200 轮", pool));
                RandomModel smallModel = (RandomModel)small.Engine.ActiveModel;
                RandomModel bigModel = (RandomModel)big.Engine.ActiveModel;
                RandomModel shuffleModel = (RandomModel)shuffle.Engine.ActiveModel;
                RandomModel poolModel = (RandomModel)pool.Engine.ActiveModel;
                w.WriteLine("  note 取模法各桶计数：" + bigModel.Detail());
                w.WriteLine("  note 洗牌：" + shuffleModel.Detail());
                w.WriteLine("  note 蓄水池：" + poolModel.Detail());
                pass &= SelfTestHost.Check(w, "modulo_bias_is_systematic",
                    smallModel.ModuloBiasMilli() > 0 && bigModel.ModuloBiasMilli() > 0,
                    "取模法的偏差是系统性的：小样本 " + smallModel.ModuloBiasMilli()
                    + "‰、大样本 " + bigModel.ModuloBiasMilli()
                    + "‰（前两个桶各多一个余数，理论偏差 = 1000 ÷ " + smallModel.Range + " = "
                    + (1000 / smallModel.Range) + "‰）");
                pass &= SelfTestHost.Check(w, "rejection_has_no_bias",
                    bigModel.RejectionBiasMilli() * 4 < bigModel.ModuloBiasMilli(),
                    "大样本上：取模法偏差 " + bigModel.ModuloBiasMilli() + "‰、拒绝采样偏差 "
                    + bigModel.RejectionBiasMilli() + "‰");
                pass &= SelfTestHost.Check(w, "shuffle_naive_is_biased",
                    shuffleModel.UniformSpread() <= shuffleModel.ShuffleSigma() * 6
                    && shuffleModel.NaiveSpread() > shuffleModel.ShuffleSigma() * 4,
                    "3 张牌的 6 种排列出现次数（" + shuffleModel.Rounds + " 轮，每种应约 "
                    + (shuffleModel.Rounds / 6) + " 次，标准差 " + shuffleModel.ShuffleSigmaText()
                    + "）：Fisher-Yates 最多最少差 " + shuffleModel.UniformSpread()
                    + " 次（噪声范围），朴素交换差 " + shuffleModel.NaiveSpread() + " 次（超出噪声）");
                pass &= SelfTestHost.Check(w, "reservoir_is_uniform",
                    poolModel.PickedMax() - poolModel.PickedMin() <= poolModel.SigmaBound(),
                    "每个元素入选 " + poolModel.PickedMin() + " 到 " + poolModel.PickedMax()
                    + " 次，理论值 " + (poolModel.Rounds * 3 / 12) + " 次、标准差约 "
                    + poolModel.SigmaText() + "（" + poolModel.Rounds
                    + " 轮 × 3 个名额 ÷ 12 个元素）");
                w.WriteLine();
            }

            // ── 场景 9 图上的结构性问题
            {
                w.WriteLine("场景 9 图上的结构性问题（8 个顶点）");
                AlgoResult topoA = AlgoSelfTest.Run(8, 0, 0, seed);
                AlgoResult topoB = AlgoSelfTest.Run(8, 0, 1, seed);
                AlgoResult cutA = AlgoSelfTest.Run(8, 1, 0, seed);
                AlgoResult cutB = AlgoSelfTest.Run(8, 1, 1, seed);
                AlgoResult mstA = AlgoSelfTest.Run(8, 2, 0, seed);
                AlgoResult mstB = AlgoSelfTest.Run(8, 2, 1, seed);
                w.WriteLine("  " + SceneLine(8, "拓扑排序，边集 A（稀疏）", topoA));
                w.WriteLine("  " + SceneLine(8, "拓扑排序，边集 B（稠密）", topoB));
                w.WriteLine("  " + SceneLine(8, "割点，边集 A", cutA));
                w.WriteLine("  " + SceneLine(8, "割点，边集 B", cutB));
                w.WriteLine("  " + SceneLine(8, "最小生成树，边集 A", mstA));
                w.WriteLine("  " + SceneLine(8, "最小生成树，边集 B", mstB));
                w.WriteLine("  note 逐场景数字：" + topoA.Detail);
                w.WriteLine("  note 逐场景数字：" + cutA.Detail);
                w.WriteLine("  note 逐场景数字：" + mstA.Detail);
                GraphModel topoModel = (GraphModel)topoA.Engine.ActiveModel;
                GraphModel topoModelB = (GraphModel)topoB.Engine.ActiveModel;
                GraphModel cutModel = (GraphModel)cutA.Engine.ActiveModel;
                GraphModel cutModelB = (GraphModel)cutB.Engine.ActiveModel;
                GraphModel mstModel = (GraphModel)mstA.Engine.ActiveModel;
                GraphModel mstModelB = (GraphModel)mstB.Engine.ActiveModel;
                pass &= SelfTestHost.Check(w, "topo_order_valid",
                    topoModel.TopoValid() && topoModelB.TopoValid(),
                    "拓扑序 " + topoModel.OrderString() + " 与 " + topoModelB.OrderString()
                    + " 里每条边都从前指向后");
                pass &= SelfTestHost.Check(w, "topo_layers_equal_longest_chain",
                    topoModel.LayerCount == topoModel.LongestChain()
                    && topoModelB.LayerCount == topoModelB.LongestChain()
                    && topoModel.LayerCount > 1,
                    "两个边集上，拓扑序里每条边都从前指向后；层数 " + topoModel.LayerCount
                    + "／" + topoModelB.LayerCount + "，最长链 " + topoModel.LongestChain()
                    + "／" + topoModelB.LongestChain() + " 个顶点（层数就是最长链上的顶点数）");
                pass &= SelfTestHost.Check(w, "cut_matches_bruteforce",
                    cutModel.CutCount == cutModel.BruteCutCount()
                    && cutModelB.CutCount == cutModelB.BruteCutCount(),
                    "割点数：Tarjan 的 low 值算出 " + cutModel.CutCount + "／" + cutModelB.CutCount
                    + " 个，逐个去掉顶点再数连通分量的暴力实现也是 "
                    + cutModel.BruteCutCount() + "／" + cutModelB.BruteCutCount() + " 个");
                pass &= SelfTestHost.Check(w, "mst_matches_prim",
                    mstModel.TotalWeight == mstModel.PrimWeight()
                    && mstModelB.TotalWeight == mstModelB.PrimWeight(),
                    "最小生成树总权：Kruskal（按边）算出 " + mstModel.TotalWeight + "／"
                    + mstModelB.TotalWeight + "，Prim（按顶点）也是 "
                    + mstModel.PrimWeight() + "／" + mstModelB.PrimWeight()
                    + "；选中的边 " + mstModel.Selected + "／" + mstModelB.Selected + " 条");
                w.WriteLine();
            }

            // ── 元素预算与画布
            {
                w.WriteLine("元素预算（九个场景共用 " + AlgoConst.Elements + " 个元素，结构区上界 "
                            + AlgoConst.StructLimit + "）");
                int worstMode = -1, worstIndex = -1;
                for (int mode = 0; mode < AlgoNames.Modes.Length; mode++)
                {
                    VisualSnapshot s = AlgoSelfTest.RunMode(mode, 0, 0, seed);
                    int used = MaxUsedIndex(s);
                    w.WriteLine("  budget " + AlgoNames.Modes[mode] + "：最大下标 " + used
                                + "（结构区上界 " + (AlgoConst.StructLimit - 1) + "）");
                    if (used > worstIndex) { worstIndex = used; worstMode = mode; }
                }
                pass &= SelfTestHost.Check(w, "elements_within_budget",
                    worstIndex < AlgoConst.StructLimit,
                    "用得最多的场景 " + (worstMode + 1) + " 到下标 " + worstIndex + "，未越过 "
                    + AlgoConst.StructLimit);
                w.WriteLine();
            }

            // ── 连线正交
            {
                List<string> report = new List<string>();
                int diagonals = inspector.CountDiagonalSegments(
                    AlgoSelfTest.RunMode(0, 0, 0, seed), 20, 20, report);
                for (int i = 0; i < report.Count; i++) SelfTestHost.Note(w, report[i]);
                pass &= SelfTestHost.Check(w, "lines_are_orthogonal", diagonals == 0,
                    "九个场景 × 两档选项二的连线逐段检查，斜段 " + diagonals + " 条（含一条人造斜线的反例）");
                w.WriteLine();
            }

            // ── 字号
            {
                w.WriteLine("字号（像素。判据是 100% 缩放下不用放大就能读，不是「放得下」）");
                inspector.ReportFonts(w);
                w.WriteLine("  note 数字优先：格上的数字、节点键值、下标、行列号、计数先占位置，"
                            + "纯装饰的标签让位");
                w.WriteLine("  note 放不下就减少同屏元素，不把字号缩回去；"
                            + "--layoutcheck 按最小字号 " + AlgoConst.MinFontPx + " 像素断言");
                w.WriteLine();
            }

            // ── 配色对比度
            {
                double minTextLight = 99, minLineLight = 99, minTextDark = 99, minLineDark = 99;
                ThemeKind[] kinds = { ThemeKind.Light, ThemeKind.Dark };
                for (int k = 0; k < kinds.Length; k++)
                {
                    ThemeManager.Apply(kinds[k]);
                    ThemePalette theme = ThemeManager.Palette;
                    string name = ThemeManager.Current == ThemeMode.Dark ? "dark" : "light";
                    Color line = AlgoPalette.LineMain();
                    Color hot = AlgoPalette.LineHot();
                    Color halo = AlgoPalette.LineHalo();
                    w.WriteLine("palette " + name + " 画布=" + ThemePalette.ToHex(theme.Background)
                                + " 连线主色=" + ThemePalette.ToHex(line)
                                + " 描边=" + ThemePalette.ToHex(halo)
                                + " 高亮=" + ThemePalette.ToHex(hot));
                    byte[] codes = { St.Empty, St.Pending, St.Done, St.Result, St.Active, St.Marked,
                                     St.Blocked, St.Anchor, St.Data, St.Path, St.Ref, St.Cut };
                    double minText = 99, minLine = 99;
                    for (int i = 0; i < codes.Length; i++)
                    {
                        Color fill = AlgoPalette.Fill(codes[i]);
                        Color text = AlgoPalette.Text(codes[i]);
                        double textRatio = ThemeManager.ContrastRatio(fill, text);
                        double lineRatio = Math.Max(ThemeManager.ContrastRatio(fill, line),
                                                    ThemeManager.ContrastRatio(fill, halo));
                        double hotRatio = Math.Max(ThemeManager.ContrastRatio(fill, hot),
                                                   ThemeManager.ContrastRatio(fill, halo));
                        if (textRatio < minText) minText = textRatio;
                        if (lineRatio < minLine) minLine = lineRatio;
                        if (hotRatio < minLine) minLine = hotRatio;
                        w.WriteLine("  fill " + ThemePalette.ToHex(fill) + " 文字=" + F(textRatio)
                                    + " 连线=" + F(lineRatio) + " 高亮=" + F(hotRatio)
                                    + "  " + AlgoPalette.LegendName(codes[i]));
                    }
                    w.WriteLine("palette " + name + " min 文字=" + F(minText) + " 连线=" + F(minLine));
                    if (k == 0) { minTextLight = minText; minLineLight = minLine; }
                    else { minTextDark = minText; minLineDark = minLine; }
                }
                ThemeManager.Apply(ThemeKind.Auto);
                pass &= SelfTestHost.Check(w, "text_contrast_at_least_4_5",
                    minTextLight >= 4.5 && minTextDark >= 4.5,
                    "块上文字与块底色的对比度：浅色最低 " + F(minTextLight) + "、深色最低 "
                    + F(minTextDark));
                pass &= SelfTestHost.Check(w, "line_contrast_at_least_3",
                    minLineLight >= 3.0 && minLineDark >= 3.0,
                    "连线（主色与描边里较高的那一个）与十二种填充色的对比度：浅色最低 "
                    + F(minLineLight) + "、深色最低 " + F(minLineDark));
                w.WriteLine();
            }

            // ── 计时：只作参考，给的是区间
            {
                double best = double.MaxValue, worst = 0;
                Stopwatch watch = new Stopwatch();
                for (int round = 0; round < 5; round++)
                {
                    watch.Reset();
                    watch.Start();
                    for (int mode = 0; mode < AlgoNames.Modes.Length; mode++)
                        AlgoSelfTest.Run(mode, 0, 0, seed);
                    watch.Stop();
                    double ms = watch.Elapsed.TotalMilliseconds;
                    if (ms < best) best = ms;
                    if (ms > worst) worst = ms;
                }
                w.WriteLine("perf 九个场景各跑到底一轮，重复 5 次：最快 " + F(best) + " ms、最慢 "
                            + F(worst) + " ms（仅作参考，随机器负载浮动）");
                w.WriteLine();
            }

            return pass;
        }

        private static string F(double value)
        {
            return value.ToString("0.00", CultureInfo.InvariantCulture);
        }

        private static string SceneLine(int mode, string name, AlgoResult r)
        {
            return "scene " + (mode + 1) + " " + name + "  steps=" + r.Steps
                   + " primary=" + r.Primary + " secondary=" + r.Secondary
                   + " work=" + r.Work + " pending=" + r.Pending
                   + " customA=" + r.CustomA + " customB=" + r.CustomB
                   + " finished=" + (r.Finished ? 1 : 0);
        }

        /// <summary>结构区里用到的最大下标，用来验证九个场景装得进 256 个元素。</summary>
        private static int MaxUsedIndex(VisualSnapshot s)
        {
            int worst = -1;
            for (int i = 0; i < AlgoConst.StructLimit; i++)
            {
                if (s.State[i] != St.Empty) { worst = i; continue; }
                if (s.Attr0[i] != 0 || s.Attr1[i] != -1 || s.Attr2[i] != -1) worst = i;
            }
            return worst;
        }
    }

    // ───────────────────────────────────────────────────────────── 窗口

    /// <summary>
    /// 演示自己的控件：重新生成数据、清掉翻转、换种子。
    /// 它们改的都是**场景数据**（运算进程那边的输入），改完把命令写进邮箱发过去。
    /// </summary>
    internal sealed class AlgoForm : VisualFormBase
    {
        private TextBox boxSeed;
        private ComboBox modeBox;
        private ComboBox optionBox;
        private ComboBox option2Box;

        public AlgoForm(AlgoDemoApp demo) : base(demo)
        {
            AlgoScene scene = Scene as AlgoScene;
            if (scene != null) scene.Invalidate = delegate { Canvas.Invalidate(); };
            WireCombos();
            // --steps=N：开局先回放到第 N 步，截图与复核都用它拿到同一帧
            if (demo.StartSteps > 0)
            {
                SendCommand(PipeProtocol.Join(PipeProtocol.Replay, demo.StartSteps));
                SetStatus("已按 --steps 回放到第 " + demo.StartSteps + " 步");
            }
        }

        protected override int BuildExtraControls(Panel host, int y, Font font)
        {
            Button regen = MakeButton("重新生成数据", 10, y, 166, 30, font, ButtonRegenClick);
            host.Controls.Add(regen);
            Button clear = MakeButton("清掉翻转", 184, y, 166, 30, font, ButtonClearClick);
            host.Controls.Add(clear);

            Label caption = MakeLabel("种子", 178, y + 40, 40, font);
            host.Controls.Add(caption);
            boxSeed = new TextBox();
            boxSeed.Location = new Point(222, y + 37);
            boxSeed.Size = new Size(64, 24);
            boxSeed.Font = font;
            boxSeed.Text = AlgoConst.DefaultSeed.ToString(CultureInfo.InvariantCulture);
            boxSeed.ForeColor = ThemeManager.Palette.PanelText;
            boxSeed.BackColor = ThemeManager.Palette.Surface;
            host.Controls.Add(boxSeed);
            Button useSeed = MakeButton("生成", 290, y + 34, 60, 28, font, ButtonUseSeedClick);
            host.Controls.Add(useSeed);
            return y + 68;
        }

        /// <summary>
        /// 「选项一」「选项二」两个下拉的内容跟着场景变。
        ///
        /// 核心把两个下拉一次建好就不再管它们（它只读 SelectedIndex），因此这里自己接上：
        /// **按条目内容认出是哪几个下拉**，换场景或换选项一时把标签换成对应的那一组。
        /// 条目数不变时**逐个替换**（不 Clear），免得触发一串没必要的 SelectedIndexChanged。
        /// </summary>
        private void WireCombos()
        {
            List<ComboBox> combos = new List<ComboBox>();
            CollectCombos(PanelHost, combos);
            for (int i = 0; i < combos.Count; i++)
            {
                ComboBox combo = combos[i];
                if (combo.Items.Count == AlgoNames.Modes.Length && modeBox == null
                    && Same(combo, AlgoNames.Modes[0])) modeBox = combo;
                else if (combo.Items.Count == AlgoNames.Modes.Length && optionBox == null
                         && Same(combo, AlgoNames.Options[0][0])) optionBox = combo;
                else if (combo.Items.Count == AlgoNames.Modes.Length && option2Box == null
                         && Same(combo, AlgoNames.Options2[0][0][0])) option2Box = combo;
            }
            if (modeBox == null || optionBox == null || option2Box == null) return;
            modeBox.SelectedIndexChanged += delegate { Relabel(); };
            optionBox.SelectedIndexChanged += delegate { Relabel(); };
            Relabel();
        }

        private static void CollectCombos(Control root, List<ComboBox> found)
        {
            for (int i = 0; i < root.Controls.Count; i++)
            {
                Control child = root.Controls[i];
                ComboBox combo = child as ComboBox;
                if (combo != null && combo.DropDownStyle == ComboBoxStyle.DropDownList) found.Add(combo);
                CollectCombos(child, found);
            }
        }

        private static bool Same(ComboBox combo, string first)
        {
            return combo.Items.Count > 0 && string.Equals(combo.Items[0] as string, first, StringComparison.Ordinal);
        }

        private void Relabel()
        {
            if (modeBox == null || modeBox.SelectedIndex < 0) return;
            int mode = modeBox.SelectedIndex;
            int optionA = optionBox.SelectedIndex < 0 ? 0 : optionBox.SelectedIndex;
            Fill(optionBox, AlgoNames.Options[mode]);
            Fill(option2Box, AlgoNames.Options2[mode][optionA < AlgoNames.Options2[mode].Length ? optionA : 0]);
        }

        /// <summary>条目数不变就逐个替换（不动选中项），变了才重建并重新选。</summary>
        private static void Fill(ComboBox combo, string[] names)
        {
            if (combo.Items.Count == names.Length)
            {
                for (int i = 0; i < names.Length; i++)
                {
                    if (string.Equals(combo.Items[i] as string, names[i], StringComparison.Ordinal)) continue;
                    combo.Items[i] = names[i];
                }
                return;
            }
            int keep = combo.SelectedIndex;
            combo.Items.Clear();
            combo.Items.AddRange(names);
            combo.SelectedIndex = keep < 0 || keep >= names.Length ? 0 : keep;
        }

        private void ButtonRegenClick(object sender, EventArgs e)
        {
            Seed = Seed + 1;
            AlgoScene scene = Scene as AlgoScene;
            if (scene != null) scene.Seed = Seed;
            boxSeed.Text = Seed.ToString(CultureInfo.InvariantCulture);
            AfterSceneEdited("已换一批数据（种子 " + Seed + "），从头再跑一遍");
        }

        private void ButtonClearClick(object sender, EventArgs e)
        {
            AlgoScene scene = Scene as AlgoScene;
            if (scene == null) return;
            scene.RequestClear();
            AfterSceneEdited("已清掉界面上的翻转，回到种子生成的那一份数据");
        }

        private void ButtonUseSeedClick(object sender, EventArgs e)
        {
            int value;
            if (!int.TryParse(boxSeed.Text.Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture,
                              out value) || value == 0)
            {
                SetStatus("种子要是一个非零整数，当前仍是 " + Seed);
                return;
            }
            Seed = value;
            AlgoScene scene = Scene as AlgoScene;
            if (scene != null) scene.Seed = value;
            boxSeed.Text = value.ToString(CultureInfo.InvariantCulture);
            AfterSceneEdited("已按种子 " + value + " 重新生成数据");
        }
    }

    // ───────────────────────────────────────────────────────────── 入口

    internal static class Program
    {
        [STAThread]
        private static int Main(string[] args)
        {
            AlgoDemoApp demo = new AlgoDemoApp();
            return VisualCoreMain.Run(args, demo, delegate { return new AlgoForm(demo); });
        }
    }
}
