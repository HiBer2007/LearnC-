// 数据结构演示 —— 十个结构场景的交互式可视化（本教材配套程序）
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
 *  DsDemo.cs —— 数据结构演示：一个程序 + 一个结构选择器，覆盖十个场景
 *
 *  1  动态数组的增长    容量跳变、搬迁、旧地址失效、reserve 前后对照
 *  2  链表              节点与指针、插入删除改了哪几个指针、哨兵
 *  3  哈希表            桶数组、冲突链、负载因子、再哈希瞬间
 *  4  搜索树 → 平衡      朴素 BST 退化成链；AVL／红黑旋转时坐标按元素序号重排
 *  5  堆                同一份数组画两次（数组行 + 树形），下沉／上浮同步
 *  6  B 树              一个节点多个槽；分裂与合并
 *  7  trie              一层一个单位；前缀高亮；中文按码点／按字节对照
 *  8  图                邻接矩阵与邻接表两种画法；遍历时高亮当前边
 *  9  并查集            父指针连线；路径压缩回放
 *  10 排序过程          比较、交换、已就位；四种算法对照
 *
 *  编译（两种都实测通过，见 README）：
 *    csc.exe /nologo /target:winexe /out:DsDemo.exe /r:System.Windows.Forms.dll ^
 *      /r:System.Drawing.dll ..\_common\VisualCore.cs DsDemo.cs
 *    dotnet build -c Release
 *
 *  语法：C# 5。.NET Framework 自带的 csc.exe 只支持到 C# 5，
 *        因此这里不用字符串内插、空条件运算符、表达式体成员这些写法。
 *
 *  ── 三条自检 ──────────────────────────────────────────────────────────
 *    --selftest      不开窗、同进程，十个场景各给一组可复现的结构量
 *    --palettecheck  两套主题逐控件算对比度，低于 4.5:1 非零退出（核心提供）
 *    --layoutcheck   文字放不下为 0，且连线每一段只许横或只许竖（本演示实现 IPathInspector）
 *
 *  ── 两条与核心的约定 ──────────────────────────────────────────────────
 *  一、**十个场景共用同一份元素数组**。核心的 Snapshot 与共享内存按
 *      Scene.ElementCount 一次定长（见 VisualFormBase 的构造函数），
 *      中途换不来长度，因此每个场景各用自己那一段下标，元素总数恒为 256。
 *  二、**界面进程的改动只能通过共享内存的场景区传给运算进程**。
 *      WriteScene 写的是 Marker／Tag／Attr0..5／Seed，因此这里留出最后一个
 *      元素当「命令邮箱」：Marker 是命令序号，Attr0..2 是命令码与两个参数。
 *      运算进程按序号去重，处理完把序号清 0 并回写。
 * ==========================================================================*/

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Windows.Forms;
using VisualCore;

namespace DsDemo
{
    // ───────────────────────────────────────────────────────────── 常量

    internal static class DsConst
    {
        /// <summary>元素总数。十个场景共用这一份，长度不能再变（见文件头）。</summary>
        public const int Elements = 256;

        /// <summary>命令邮箱用的元素下标，画布上永远不画它。</summary>
        public const int Mailbox = 255;

        /// <summary>结构单元区 [0,128)：节点、槽、桶、格。</summary>
        public const int StructBase = 0;
        public const int StructCount = 128;

        /// <summary>输入区 [128,255)：场景数据（要插入的键值、要连的边、要排的数）。</summary>
        public const int InputBase = 128;
        public const int InputCount = 127;

        public const int DefaultSeed = 20261002;

        public const int CanvasWidth = 940;
        public const int CanvasHeight = 620;

        /// <summary>画布顶部标题行的基线，正文从 TitleBottom 往下画。</summary>
        public const int TitleTop = 6;
        public const int TitleBottom = 30;

        /// <summary>键值（刷子值）的取值范围。</summary>
        public const int MinKey = 1;
        public const int MaxKey = 99;

        /// <summary>哈希表的负载因子上限，超过就再哈希。用千分之一表示。</summary>
        public const int LoadFactorMilli = 750;

        /// <summary>线宽（像素）。数量过，不是凭感觉写的，见 README 的「线宽」一节。</summary>
        public const float HaloWidth = 3.0f;
        public const float MainWidth = 1.6f;
    }

    // ───────────────────────────────────────────────────────────── 状态码

    /// <summary>
    /// 全局语义状态码。**每个码在所有场景里含义一致**，因为图例的调色板
    /// 在建窗口时绑定一次（LegendPanel 的 palette 是只读字段），
    /// 十个场景只能共用一套色表，因此不能一个码两种意思。
    /// </summary>
    internal static class Ds
    {
        public const byte Empty = ElementStates.Empty;       // 0 空位、未使用
        public const byte Pending = ElementStates.Pending;   // 1 待处理、待比较
        public const byte Done = ElementStates.Done;         // 2 已处理、已就位
        public const byte Result = ElementStates.Result;     // 3 最终结果、命中
        public const byte Active = ElementStates.Active;     // 4 当前
        public const byte Marked = ElementStates.Marked;     // 5 刚变更
        public const byte Blocked = ElementStates.Blocked;   // 6 已失效、不可用
        public const byte Anchor = 7;                        // 7 结构锚点（根、哨兵、桶）
        public const byte Data = 8;                          // 8 数据单元
        public const byte Path = 9;                          // 9 路径、前缀
        public const byte Ref = 10;                          // 10 参照元素
        public const byte Cut = 11;                          // 11 已压缩、旁路

        /// <summary>命令码：界面进程写进邮箱，运算进程照着做。</summary>
        public const int CmdNone = 0;
        public const int CmdInsert = 1;
        public const int CmdDelete = 2;
        public const int CmdFind = 3;
        public const int CmdSelect = 4;
        public const int CmdClear = 5;
        public const int CmdRegenerate = 6;
        public const int CmdAppendRandom = 7;
        public const int CmdUndo = 8;
    }

    /// <summary>十个场景的名字、选项名与元素上标名。下拉里的字都在这里。</summary>
    internal static class DsNames
    {
        public static readonly string[] Modes =
        {
            "1 动态数组的增长", "2 链表", "3 哈希表", "4 搜索树 → 平衡", "5 堆",
            "6 B 树", "7 trie（前缀树）", "8 图", "9 并查集", "10 排序过程"
        };

        public static readonly string[][] Options =
        {
            new string[] { "逐步 push", "先 reserve(32)", "先 reserve(8)" },
            new string[] { "有序插入", "尾部接上", "头部插入" },
            new string[] { "取模 k % B", "乘法散列（取高位）" },
            new string[] { "朴素 BST", "AVL（高度平衡）", "红黑树" },
            new string[] { "自底向上建堆", "从空堆逐个 push" },
            new string[] { "只插入", "只删除", "插入后删除" },
            new string[] { "按码点建", "按字节建（UTF-8）" },
            new string[] { "邻接矩阵", "邻接表", "边表" },
            new string[] { "什么都不加", "只按秩合并", "只路径压缩", "按秩 + 路径压缩" },
            new string[] { "冒泡排序", "插入排序", "选择排序", "快速排序（Lomuto）" }
        };

        public static readonly string[][] Options2 =
        {
            new string[] { "12 个元素", "20 个元素", "24 个元素" },
            new string[] { "12 个键", "8 个键", "16 个键" },
            new string[] { "初始 8 桶", "初始 16 桶" },
            new string[] { "随机序", "升序（会退化）", "锯齿（中、小、大）" },
            new string[] { "15 个元素", "31 个元素" },
            new string[] { "阶 4（每节点最多 3 键）", "阶 5（每节点最多 4 键）" },
            new string[] { "英文单词", "中文词", "中英混合" },
            new string[] { "广度优先 BFS", "深度优先 DFS" },
            new string[] { "16 个元素", "12 个元素" },
            new string[] { "随机", "近乎有序", "逆序" }
        };

        public static readonly string[] DisplayNames =
        {
            "键值", "下标／编号", "访问次序", "不写"
        };

        public static readonly string[] StatNames =
        {
            "已推进步数", "本场景工作量", "待处理集合", "主指标", "次指标",
            "实测推进速率", "两种模式对照"
        };
    }

    // ───────────────────────────────────────────────────────────── 色表

    /// <summary>
    /// 语义色表。场景只按状态码取色，色值由主题给；这里给出图例的条目。
    /// 十一个状态码 + 一个「已压缩」，正好把十二个色槽用完，
    /// 因此每个码在屏幕上都有自己的一种填充色，不必靠形状区分。
    /// </summary>
    internal static class DsPalette
    {
        public static ScenePalette Build()
        {
            ScenePalette p = new ScenePalette();
            p.AddState(Ds.Empty, "空位（未使用）", ThemeSlot.Empty);
            p.AddState(Ds.Pending, "待处理／待比较", ThemeSlot.Pending);
            p.AddState(Ds.Done, "已处理／已就位", ThemeSlot.Done);
            p.AddState(Ds.Result, "最终结果／命中", ThemeSlot.Result);
            p.AddState(Ds.Active, "当前", ThemeSlot.Active);
            p.AddState(Ds.Marked, "刚变更", ThemeSlot.Extra2);
            p.AddState(Ds.Blocked, "已失效／不可用", ThemeSlot.Blocked);
            p.AddState(Ds.Anchor, "结构锚点（根／哨兵／桶）", ThemeSlot.Start);
            p.AddState(Ds.Data, "数据单元", ThemeSlot.Terrain);
            p.AddState(Ds.Path, "路径／前缀", ThemeSlot.Goal);
            p.AddState(Ds.Ref, "参照元素", ThemeSlot.Extra1);
            p.AddState(Ds.Cut, "已压缩／旁路", ThemeSlot.Extra3);
            return p;
        }

        public static Color Fill(byte state) { return ThemeManager.Palette.FillOf(Slot(state)); }
        public static Color Text(byte state) { return ThemeManager.Palette.TextOf(Slot(state)); }

        /// <summary>图例上的名字。自检打印对比度表时也用它，两边不会各写一套。</summary>
        public static string LegendName(byte state)
        {
            switch (state)
            {
                case Ds.Empty: return "空位（未使用）";
                case Ds.Pending: return "待处理／待比较";
                case Ds.Done: return "已处理／已就位";
                case Ds.Result: return "最终结果／命中";
                case Ds.Active: return "当前";
                case Ds.Marked: return "刚变更";
                case Ds.Blocked: return "已失效／不可用";
                case Ds.Anchor: return "结构锚点";
                case Ds.Data: return "数据单元";
                case Ds.Path: return "路径／前缀";
                case Ds.Ref: return "参照元素";
                case Ds.Cut: return "已压缩／旁路";
                default: return "未知";
            }
        }

        public static ThemeSlot Slot(byte state)
        {
            switch (state)
            {
                case Ds.Empty: return ThemeSlot.Empty;
                case Ds.Pending: return ThemeSlot.Pending;
                case Ds.Done: return ThemeSlot.Done;
                case Ds.Result: return ThemeSlot.Result;
                case Ds.Active: return ThemeSlot.Active;
                case Ds.Marked: return ThemeSlot.Extra2;
                case Ds.Blocked: return ThemeSlot.Blocked;
                case Ds.Anchor: return ThemeSlot.Start;
                case Ds.Data: return ThemeSlot.Terrain;
                case Ds.Path: return ThemeSlot.Goal;
                case Ds.Ref: return ThemeSlot.Extra1;
                case Ds.Cut: return ThemeSlot.Extra3;
                default: return ThemeSlot.Empty;
            }
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
        /// 高亮连线（当前边、当前指针）：取该主题下与画布底色对比度更高的那一支——
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
    internal sealed class DsLine
    {
        public int X1, Y1, X2, Y2;
        public bool Hot;        // 当前边／当前指针

        public bool Horizontal { get { return Y1 == Y2; } }
        public bool Vertical { get { return X1 == X2; } }
    }

    /// <summary>
    /// 连线集合。**只提供正交的加法**：横段、竖段，以及把一个任意方向的连接
    /// 拆成「先横后竖」两段。因此产出斜段的唯一可能是有人直接 Add 一个斜的，
    /// 自检里专门造了一条这样的线当反例，证明检查抓得住。
    /// </summary>
    internal sealed class DsPath
    {
        public readonly List<DsLine> Lines = new List<DsLine>();

        public void Clear() { Lines.Clear(); }

        public void Add(int x1, int y1, int x2, int y2, bool hot)
        {
            if (x1 == x2 && y1 == y2) return;
            DsLine line = new DsLine();
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
                DsLine line = Lines[i];
                bool h = line.Y1 == line.Y2;
                bool v = line.X1 == line.X2;
                if (!h && !v) bad++;
            }
            return bad;
        }

        /// <summary>某段线是否穿过这个矩形（自检与「线在字的位置让开」都要用）。</summary>
        public bool Hits(Rectangle box)
        {
            for (int i = 0; i < Lines.Count; i++)
            {
                DsLine line = Lines[i];
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
    internal sealed class DsRun
    {
        public int Owner = -1;
        public string Text;
        public Rectangle Box;
        public Color Pad;
    }

    // ───────────────────────────────────────────────────────────── 布局

    /// <summary>
    /// 一帧的几何：每个元素画在哪个矩形里、连线怎么走、文字写在哪。
    ///
    /// **绘制、命中测试、连线自检三处用的是同一份几何**，因此自检断言的就是
    /// 屏幕上的线。布局跟着结构走（树按中序排名排 x、B 树按子节点居中），
    /// 所以旋转一次坐标就重排一次。
    ///
    /// 提示：矩形宽为 0 表示这个元素本场景里没有位置，不画也不可点。
    /// </summary>
    internal sealed class DsView
    {
        public readonly Rectangle[] Box = new Rectangle[DsConst.Elements];
        public readonly int[] Order = new int[DsConst.Elements];
        public readonly DsPath Path = new DsPath();
        public int Mode = -1;
        public int OptionA;
        public int OptionB;
        public int Count;                       // 本场景用到的元素个数

        // 布局用的临时数组，一次分配反复用，免得每帧产生垃圾
        private readonly int[] xOf = new int[DsConst.Elements];

        public void Clear()
        {
            for (int i = 0; i < Box.Length; i++) { Box[i] = Rectangle.Empty; Order[i] = -1; }
            Path.Clear();
            Count = 0;
        }

        public void Build(VisualSnapshot s)
        {
            Clear();
            Mode = s.ModeKind;
            OptionA = s.OptionA;
            OptionB = s.OptionB;
            switch (Mode)
            {
                case 0: LayoutArray(s); break;
                case 1: LayoutList(s); break;
                case 2: LayoutHash(s); break;
                case 3: LayoutTree(s); break;
                case 4: LayoutHeap(s); break;
                case 5: LayoutBTree(s); break;
                case 6: LayoutTrie(s); break;
                case 7: LayoutGraph(s); break;
                case 8: LayoutDsu(s); break;
                default: LayoutSort(s); break;
            }
        }

        // ── 场景 1：动态数组。新块两行 64 槽，旧块一行 32 槽

        private const int ArrayCellW = 26;
        private const int ArrayCellH = 26;
        private const int ArrayX = 24;
        private const int ArrayY = 116;
        private const int GhostY = 262;

        private void LayoutArray(VisualSnapshot s)
        {
            Count = 96;
            for (int i = 0; i < 64; i++)
            {
                int col = i % 32, row = i / 32;
                Box[i] = new Rectangle(ArrayX + col * ArrayCellW, ArrayY + row * 30,
                                       ArrayCellW - 1, ArrayCellH - 1);
            }
            for (int j = 0; j < 32; j++)
            {
                Box[64 + j] = new Rectangle(ArrayX + j * ArrayCellW, GhostY,
                                            ArrayCellW - 1, ArrayCellH - 1);
            }
            for (int i = 0; i < 64; i++) Order[i] = i;          // 结构编号＝槽位下标
            for (int j = 0; j < 32; j++) Order[64 + j] = j;
            // 搬迁中的那一个：从旧块列心走到新块列心，中间走一条正交折线
            int moving = s.CurrentIndex;
            if (moving >= 0 && moving < 32 && Box[64 + moving].Width > 0 && Box[moving].Width > 0)
            {
                int x1 = Box[64 + moving].Left + ArrayCellW / 2;
                int x2 = Box[moving].Left + ArrayCellW / 2;
                int y1 = Box[64 + moving].Top;
                int y2 = Box[moving].Bottom;
                Path.ViaY(x1, y1, x2, y2, (Box[moving].Bottom + GhostY) / 2, true);
            }
        }

        // ── 场景 2：链表。十六个盒子两行，指针走正交折线

        private const int ListNodeW = 96;
        private const int ListNodeH = 36;

        private static Rectangle ListNodeBox(int index)
        {
            int col = index % 8, row = index / 8;
            return new Rectangle(20 + col * 116, 150 + row * 90, ListNodeW, ListNodeH);
        }

        private void LayoutList(VisualSnapshot s)
        {
            Count = 16;
            Box[0] = ListNodeBox(0);                       // 哨兵一直都在
            for (int i = 1; i < Count; i++)
            {
                if (s.Attr0[i] <= 0) continue;             // 键为 0 表示这个节点还没出生
                Box[i] = ListNodeBox(i);
            }
            int walkOrder = Attr(s, 0, 1), rank = 0, guard = 0;   // 顺着 next 数一遍链上的次序
            while (walkOrder >= 0 && walkOrder < Count && guard++ < Count)
            {
                Order[walkOrder] = rank++;
                walkOrder = Attr(s, walkOrder, 1);
            }
            for (int i = 0; i < Count; i++)
            {
                if (Box[i].Width == 0) continue;
                int next = s.Attr1[i];
                if (next < 0 || next >= Count || Box[next].Width == 0) continue;
                bool hot = s.CurrentIndex == i;
                Rectangle a = Box[i], b = Box[next];
                if (a.Top == b.Top && b.Left > a.Right)
                {
                    Path.H(a.Top + ListNodeH / 2, a.Right, b.Left, hot);
                }
                else if (a.Top == b.Top)
                {
                    Path.ViaY(b.Right - 10, b.Top + ListNodeH, a.Right - 10, a.Top + ListNodeH,
                              a.Bottom + 26, hot);
                }
                else
                {
                    Path.ViaY(a.Right - 10, a.Bottom, b.Left + 10, b.Top, a.Bottom + 26, hot);
                }
            }
        }

        // ── 场景 3：哈希表。桶两列，冲突链向右伸

        private const int HashBucketW = 54;
        private const int HashBucketH = 22;
        private const int HashNodeW = 46;
        private const int HashNodeH = 22;

        private void LayoutHash(VisualSnapshot s)
        {
            int buckets = s.PrimaryMetric;
            if (buckets < 4) buckets = 4;
            Count = 32 + 64;
            int rows = buckets <= 16 ? buckets : 16;
            for (int b = 0; b < 32; b++)
            {
                if (b >= buckets) continue;
                int col = b / rows, row = b % rows;
                Box[b] = new Rectangle(16 + col * 424, 104 + row * 28, HashBucketW, HashBucketH);
            }
            for (int n = 0; n < 64; n++)
            {
                int index = 32 + n;
                int b = s.Attr2[index];
                if (b < 0 || b >= buckets) continue;
                int col = b / rows, row = b % rows;
                int depth = 0;
                Box[index] = new Rectangle(16 + col * 424 + HashBucketW + 8 + depth * 52,
                                           104 + row * 28, HashNodeW, HashNodeH);
            }
            // 每桶的链：桶 → 第一个节点 → 第二个 → …，深度按链上的次序数出来
            for (int b = 0; b < buckets && b < 32; b++)
            {
                int col = b / rows, row = b % rows;
                int node = s.Attr1[b];
                int prev = -1;
                int depth = 0;
                int guard = 0;
                while (node >= 32 && node < 96 && guard++ < 64)
                {
                    Rectangle box = new Rectangle(16 + col * 424 + HashBucketW + 8 + depth * 52,
                                                  104 + row * 28, HashNodeW, HashNodeH);
                    Box[node] = box;
                    Order[node] = depth;                        // 结构编号＝在链上的第几个
                    bool hot = s.CurrentIndex == node;
                    if (prev < 0)
                    {
                        Rectangle bucket = Box[b];
                        Path.H(bucket.Top + HashBucketH / 2, bucket.Right, box.Left, hot);
                    }
                    else
                    {
                        Path.H(box.Top + HashNodeH / 2, Box[prev].Right, box.Left, hot);
                    }
                    prev = node;
                    node = s.Attr1[node];
                    depth++;
                }
            }
        }

        // ── 场景 4：搜索树。x 按中序排名，旋转一次就重排一次

        private const int TreeNodeW = 40;
        private const int TreeNodeH = 24;
        private const int TreeStepX = 52;
        private const int TreeStepY = 32;

        private int rankCounter;

        private void RankTree(VisualSnapshot s, int node)
        {
            if (node < 0 || node >= 64) return;
            RankTree(s, s.Attr1[node]);
            xOf[node] = rankCounter++;
            RankTree(s, s.Attr2[node]);
        }

        private void LayoutTree(VisualSnapshot s)
        {
            Count = 64;
            rankCounter = 0;
            for (int i = 0; i < 64; i++) xOf[i] = -1;
            RankTree(s, s.FocusIndex);
            int columns = rankCounter > 0 ? rankCounter : 1;
            for (int i = 0; i < 64; i++)
            {
                if (s.Attr0[i] <= 0 || xOf[i] < 0) continue;
                int depth = DepthByAttr(s, i, 3);
                int width = DsConst.CanvasWidth - 40;
                int step = columns > 1 ? Math.Min(TreeStepX, (width - TreeNodeW) / (columns - 1)) : TreeStepX;
                if (step < 20) step = 20;
                int x = 20 + (width - (columns - 1) * step - TreeNodeW) / 2 + xOf[i] * step;
                Box[i] = new Rectangle(x, 60 + depth * TreeStepY, TreeNodeW, TreeNodeH);
                Order[i] = xOf[i];                          // 结构编号＝中序排名
            }
            for (int i = 0; i < 64; i++)
            {
                if (Box[i].Width == 0) continue;
                LinkChild(Box[i], s.Attr1[i], s, i);
                LinkChild(Box[i], s.Attr2[i], s, i);
            }
        }

        private void LinkChild(Rectangle parent, int child, VisualSnapshot s, int owner)
        {
            if (child < 0 || child >= 64 || Box[child].Width == 0) return;
            Rectangle kid = Box[child];
            bool hot = s.CurrentIndex == owner || s.CurrentIndex == child;
            Path.ViaY(parent.Left + TreeNodeW / 2, parent.Bottom,
                      kid.Left + TreeNodeW / 2, kid.Top,
                      parent.Bottom + (TreeStepY - TreeNodeH) / 2, hot);
        }

        /// <summary>顺着某一列父指针往上数层数。attr 是父指针所在的属性槽。</summary>
        private int DepthByAttr(VisualSnapshot s, int node, int attr)
        {
            int depth = 0;
            int walk = Attr(s, node, attr);
            int guard = 0;
            while (walk >= 0 && walk < DsConst.Elements && guard++ < 128)
            {
                depth++;
                walk = Attr(s, walk, attr);
            }
            return depth;
        }

        private static int Attr(VisualSnapshot s, int index, int slot)
        {
            switch (slot)
            {
                case 0: return s.Attr0[index];
                case 1: return s.Attr1[index];
                case 2: return s.Attr2[index];
                case 3: return s.Attr3[index];
                case 4: return s.Attr4[index];
                default: return s.Attr5[index];
            }
        }

        // ── 场景 5：堆。同一份数组画两次：上面树形，下面数组行

        private const int HeapNodeW = 40;
        private const int HeapNodeH = 24;
        private const int HeapCellW = 28;
        private const int HeapCellH = 28;

        private void LayoutHeap(VisualSnapshot s)
        {
            Count = 32;
            int size = s.PrimaryMetric >= 0 ? s.PrimaryMetric : 0;
            for (int i = 0; i < 32; i++)
            {
                if (i >= size) continue;
                int depth = DepthOfIndex(i);
                int first = (1 << depth) - 1;
                int pos = i - first;
                int span = 1 << depth;
                int slot = (int)Math.Round((pos + 0.5) * (DsConst.CanvasWidth - 60) / span);
                Box[i] = new Rectangle(24 + slot - HeapNodeW / 2, 58 + depth * 52, HeapNodeW, HeapNodeH);
                Order[i] = i;                               // 结构编号＝数组下标
            }
            for (int i = 0; i < 32; i++)
            {
                if (Box[i].Width == 0) continue;
                int left = 2 * i + 1, right = 2 * i + 2;
                if (left < size && Box[left].Width > 0) LinkHeap(Box[i], Box[left], s, i);
                if (right < size && Box[right].Width > 0) LinkHeap(Box[i], Box[right], s, i);
            }
        }

        private void LinkHeap(Rectangle parent, Rectangle child, VisualSnapshot s, int owner)
        {
            bool hot = s.CurrentIndex == owner;
            Path.ViaY(parent.Left + HeapNodeW / 2, parent.Bottom,
                      child.Left + HeapNodeW / 2, child.Top, parent.Bottom + 14, hot);
        }

        /// <summary>数组行那一次绘制用的矩形（与树形共用元素，位置在下方）。</summary>
        public static Rectangle HeapArrayCell(int index)
        {
            return new Rectangle(24 + index * HeapCellW, 430, HeapCellW - 1, HeapCellH - 1);
        }

        private static int DepthOfIndex(int index)
        {
            int depth = 0;
            while (index > 0) { index = (index - 1) / 2; depth++; }
            return depth;
        }

        // ── 场景 6：B 树。一个节点多个槽，子节点居中排在父节点下面

        public const int BTreeSlotW = 30;
        public const int BTreeSlotH = 26;
        public const int BTreeNodeW = BTreeSlotW * 3 + 8;
        public const int BTreeNodeH = 34;
        private int btreeSlot;

        private int PlaceBTree(VisualSnapshot s, int node)
        {
            if (node < 0 || node >= 24) return -1;
            int element = node * 2;                       // 偶数号元素是看得见的节点盒
            if (s.Tag[element] <= 0) return -1;
            int keys = s.Tag[element];
            bool leaf = s.Attr3[element] == 1;
            if (leaf)
            {
                xOf[node] = 40 + btreeSlot * 130;
                btreeSlot++;
                return xOf[node];
            }
            int first = -1, last = -1;
            for (int c = 0; c <= keys; c++)
            {
                int kid = Attr(s, element + 1, c);        // 奇数号元素是孩子表
                int cx = PlaceBTree(s, kid);
                if (cx < 0) continue;
                if (first < 0) first = cx;
                last = cx;
            }
            if (first < 0) { xOf[node] = 40 + btreeSlot * 130; btreeSlot++; }
            else xOf[node] = (first + last) / 2;
            return xOf[node];
        }

        private void LayoutBTree(VisualSnapshot s)
        {
            Count = 48;                                   // 24 个节点 × 2（节点盒 + 孩子表）
            btreeSlot = 0;
            for (int i = 0; i < 24; i++) xOf[i] = -1;
            PlaceBTree(s, s.FocusIndex / 2);
            for (int i = 0; i < 24; i++)
            {
                if (xOf[i] < 0) continue;
                int element = i * 2;
                int depth = DepthByAttr(s, element, 4);
                Box[element] = new Rectangle(xOf[i], 56 + depth * 82, BTreeNodeW, BTreeNodeH);
                Order[element] = i;                         // 结构编号＝节点号
            }
            for (int i = 0; i < 24; i++)
            {
                int element = i * 2;
                if (Box[element].Width == 0) continue;
                int keys = s.Tag[element];
                if (s.Attr3[element] == 1) continue;
                for (int c = 0; c <= keys; c++)
                {
                    int kid = Attr(s, element + 1, c);
                    if (kid < 0 || kid >= 24 || Box[kid * 2].Width == 0) continue;
                    Path.ViaY(Box[element].Left + BTreeNodeW / 2, Box[element].Bottom,
                              Box[kid * 2].Left + BTreeNodeW / 2, Box[kid * 2].Top,
                              Box[element].Bottom + 24, s.CurrentIndex == element);
                }
            }
        }

        // ── 场景 7：trie。一层一个单位（按码点或按字节），兄弟连成一条链

        private const int TrieNodeW = 36;
        private const int TrieNodeH = 26;
        private int trieSlot;

        private int PlaceTrie(VisualSnapshot s, int node)
        {
            if (node < 0 || node >= 64) return -1;
            int first = -1, last = -1;
            int guard = 0;
            for (int c = s.Attr2[node]; c >= 0 && c < 64 && guard++ < 64; c = s.Attr3[c])
            {
                int cx = PlaceTrie(s, c);
                if (cx < 0) continue;
                if (first < 0) first = cx;
                last = cx;
            }
            if (first < 0) { xOf[node] = 30 + trieSlot * 46; trieSlot++; }
            else xOf[node] = (first + last) / 2;
            return xOf[node];
        }

        private void LayoutTrie(VisualSnapshot s)
        {
            Count = 64;
            trieSlot = 0;
            for (int i = 0; i < 64; i++) xOf[i] = -1;
            PlaceTrie(s, s.FocusIndex);
            for (int i = 0; i < 64; i++)
            {
                if (xOf[i] < 0 || s.Attr4[i] < 0) continue;
                Box[i] = new Rectangle(xOf[i], 50 + s.Attr4[i] * 46, TrieNodeW, TrieNodeH);
                Order[i] = s.Attr4[i];                      // 结构编号＝层号
            }
            for (int i = 0; i < 64; i++)
            {
                if (Box[i].Width == 0) continue;
                int guard = 0;
                for (int c = s.Attr2[i]; c >= 0 && c < 64 && guard++ < 64; c = s.Attr3[c])
                {
                    if (Box[c].Width == 0) continue;
                    Path.ViaY(Box[i].Left + TrieNodeW / 2, Box[i].Bottom,
                              Box[c].Left + TrieNodeW / 2, Box[c].Top,
                              Box[i].Bottom + 12, s.CurrentIndex == c);
                }
            }
        }

        // ── 场景 8：图。顶点两行排，边走上下两条行之间的通道

        public const int GraphVertexW = 56;
        public const int GraphVertexH = 40;
        public const int GraphChannelY = 250;
        public const int GraphChannelBase = 152;

        public static Rectangle GraphVertexBox(int v)
        {
            int col = v % 4, row = v / 4;
            return new Rectangle(56 + col * 76, 100 + row * 210, GraphVertexW, GraphVertexH);
        }

        public static Rectangle GraphMatrixCell(int u, int v)
        {
            return new Rectangle(480 + v * 28, 116 + u * 26, 27, 25);
        }

        public static Rectangle GraphListEntry(int u, int slot)
        {
            return new Rectangle(540 + slot * 56, 116 + u * 30, 52, 26);
        }

        private void LayoutGraph(VisualSnapshot s)
        {
            Count = 104;
            for (int v = 0; v < 8; v++)
            {
                Box[v] = GraphVertexBox(v);
                Order[v] = s.Attr3[v];                    // 结构编号＝访问次序
            }
            // 三种存法只画选中的那一种，否则矩阵与邻接表会叠在同一块地方
            for (int u = 0; u < 8; u++)
            {
                for (int v = 0; v < 8; v++)
                {
                    if (s.OptionA != 0) continue;
                    Box[8 + u * 8 + v] = GraphMatrixCell(u, v);
                    Order[8 + u * 8 + v] = u * 8 + v;
                }
                for (int k = 0; k < 4; k++)
                {
                    int slot = 72 + u * 4 + k;
                    if (s.OptionA != 1 || s.Attr2[slot] != u) continue;
                    Box[slot] = GraphListEntry(u, k);
                    Order[slot] = k;
                }
            }
            if (s.OptionA == 1)
            {
                for (int u = 0; u < 8; u++)
                {
                    int previous = -1;
                    for (int k = 0; k < 4; k++)
                    {
                        int slot = 72 + u * 4 + k;
                        if (s.Attr2[slot] != u) continue;
                        if (previous < 0) Path.H(Box[slot].Top + 13, 534, Box[slot].Left, false);
                        else Path.H(Box[slot].Top + 13, Box[previous].Right, Box[slot].Left, false);
                        previous = slot;
                    }
                }
            }
            // 边：从 u 的盒边走到通道，再横着走到 v 那一列，最后进 v 的盒边。
            // 每条边分一条通道，免得十来条边叠成一条长横线。
            int hotU = s.CustomA, hotV = s.CustomB;
            int guard = 0;
            for (int e = 0; e < 60 && guard++ < 60; e++)
            {
                int u = s.Attr0[DsConst.InputBase + e * 2];
                if (u <= 0) break;
                int v = s.Attr0[DsConst.InputBase + e * 2 + 1];
                if (u > 7 || v > 7 || u == v) continue;
                Rectangle a = Box[u], b = Box[v];
                int ax = a.Left + GraphVertexW / 2, bx = b.Left + GraphVertexW / 2;
                int ay = a.Top > GraphChannelY ? a.Top : a.Bottom;
                int by = b.Top > GraphChannelY ? b.Top : b.Bottom;
                int channel = GraphChannelBase + e * 12;   // 一条边一条通道，互不叠在一起
                bool hot = (u == hotU && v == hotV) || (u == hotV && v == hotU);
                Path.ViaY(ax, ay, bx, by, channel, hot);
            }
        }

        // ── 场景 9：并查集。父指针走正交折线，根画成锚点色

        public const int DsuNodeW = 56;
        public const int DsuNodeH = 28;

        public static Rectangle DsuNodeBox(int i)
        {
            int col = i % 4, row = i / 4;
            return new Rectangle(60 + col * 150, 80 + row * 118, DsuNodeW, DsuNodeH);
        }

        private void LayoutDsu(VisualSnapshot s)
        {
            // 这一轮只用前 n 个元素（选项二决定），其余的连盒子都不给
            int used = s.OptionB == 0 ? 16 : 12;
            Count = 16;
            for (int i = 0; i < used && i < 16; i++)
            {
                Box[i] = DsuNodeBox(i);
                Order[i] = s.Attr1[i];                      // 结构编号＝父节点
            }
            for (int i = 0; i < Count; i++)
            {
                int parent = s.Attr1[i];
                if (parent == i || parent < 0 || parent >= Count) continue;
                Rectangle a = Box[i], b = Box[parent];
                int ax = a.Left + DsuNodeW / 2, bx = b.Left + DsuNodeW / 2;
                bool hot = s.CurrentIndex == i;
                if (a.Top == b.Top)
                {
                    Path.ViaY(ax, a.Top, bx, b.Top, a.Top - 26, hot);
                }
                else if (a.Top > b.Top)
                {
                    Path.ViaY(ax, a.Top, bx, b.Bottom, (a.Top + b.Bottom) / 2, hot);
                }
                else
                {
                    Path.ViaY(ax, a.Bottom, bx, b.Top, (a.Bottom + b.Top) / 2, hot);
                }
            }
        }

        // ── 场景 10：排序。四十八根柱子，值写成柱顶的数字

        public const int SortBarW = 16;
        public const int SortBarGap = 3;
        public const int SortBaseY = 566;

        public static Rectangle SortBarBox(int i, int value)
        {
            int h = 16 + value * 4;
            return new Rectangle(20 + i * (SortBarW + SortBarGap), SortBaseY - h, SortBarW, h);
        }

        private void LayoutSort(VisualSnapshot s)
        {
            Count = 48;
            for (int i = 0; i < Count; i++)
            {
                int value = s.Attr0[i];
                if (value <= 0) continue;
                Box[i] = SortBarBox(i, value);
                Order[i] = i;                               // 结构编号＝下标
            }
        }
    }
    // ───────────────────────────────────────────────────────────── 模型基类

    /// <summary>
    /// 一个场景的算法本体。**引擎自己的数据放在这里的数组里，算法直接改数组，
    /// Flush() 再摊进快照**：算法不必每步去动快照的六个属性槽，写起来清楚，
    /// 也让「结构量」与「画出来的东西」只有一个来源。
    ///
    /// 场景数据（要插入的键、要连的边、要排的数）一律放在输入区
    /// At[0][128..254)，它不受 ResetRun 影响，只有界面进程改得动它。
    /// </summary>
    internal abstract class DsModel
    {
        protected readonly VisualSnapshot S;
        protected readonly byte[] St = new byte[DsConst.Elements];
        protected readonly byte[] Mk = new byte[DsConst.Elements];
        protected readonly byte[] Tg = new byte[DsConst.Elements];
        protected readonly int[][] At = new int[6][];

        public int OptionA;
        public int OptionB;
        public int Seed = DsConst.DefaultSeed;

        protected DsModel(VisualSnapshot s)
        {
            S = s;
            for (int k = 0; k < 6; k++)
            {
                At[k] = new int[DsConst.Elements];
                for (int i = 0; i < DsConst.Elements; i++) At[k][i] = -1;
            }
        }

        /// <summary>把数组摊进快照。引擎每次发布前调一次。</summary>
        public virtual void Flush()
        {
            for (int i = 0; i < DsConst.Elements; i++)
            {
                S.State[i] = St[i];
                S.Marker[i] = Mk[i];
                S.Tag[i] = Tg[i];
                S.Attr0[i] = At[0][i];
                S.Attr1[i] = At[1][i];
                S.Attr2[i] = At[2][i];
                S.Attr3[i] = At[3][i];
                S.Attr4[i] = At[4][i];
                S.Attr5[i] = At[5][i];
            }
        }

        public abstract void BuildTarget();
        public abstract void ResetRun();
        public abstract bool Step();
        public abstract string StatusText();
        public abstract string Title();

        public virtual void Command(int op, int key, int target) { }

        /// <summary>场景数据（输入区就在这条数组里），撤销栈存的就是它。</summary>
        public int[] SceneData { get { return At[0]; } }

        public virtual int Primary { get { return -1; } }
        public virtual int Secondary { get { return -1; } }
        public virtual int Work { get { return 0; } }
        public virtual int Pending { get { return 0; } }

        /// <summary>关注元素（树的根、图的起点）与当前元素，画布靠它决定谁高亮。</summary>
        protected int currentIndex = -1;
        protected int focusIndex = -1;

        public virtual int Current { get { return currentIndex; } }
        public virtual int Focus { get { return focusIndex; } }
        public virtual int CustomA { get { return 0; } }
        public virtual int CustomB { get { return 0; } }

        // ── 运行状态与输入区的小工具

        /// <summary>清掉运行状态：所有状态、标记、属性 1..5 归零，属性 0 不动。</summary>
        protected void ClearRun()
        {
            for (int i = 0; i < DsConst.Elements; i++)
            {
                St[i] = Ds.Empty;
                Mk[i] = 0;
                Tg[i] = 0;
                for (int k = 1; k < 6; k++) At[k][i] = -1;
            }
        }

        /// <summary>清掉结构区的键值（属性 0 的前 128 个），输入区不动。</summary>
        protected void ClearStructKeys()
        {
            for (int i = 0; i < DsConst.StructCount; i++) At[0][i] = 0;
        }

        protected int InLen()
        {
            int n = 0;
            while (n < DsConst.InputCount && At[0][DsConst.InputBase + n] > 0) n++;
            return n;
        }

        protected int InAt(int i) { return At[0][DsConst.InputBase + i]; }

        protected void InClear()
        {
            for (int i = 0; i < DsConst.InputCount; i++) At[0][DsConst.InputBase + i] = 0;
        }

        protected bool InAppend(int value)
        {
            int n = InLen();
            if (n >= DsConst.InputCount) return false;
            At[0][DsConst.InputBase + n] = value;
            return true;
        }

        protected bool InRemoveAt(int index)
        {
            int n = InLen();
            if (index < 0 || index >= n) return false;
            for (int k = index; k < n - 1; k++) At[0][DsConst.InputBase + k] = At[0][DsConst.InputBase + k + 1];
            At[0][DsConst.InputBase + n - 1] = 0;
            return true;
        }

        protected bool InRemoveLast()
        {
            int n = InLen();
            if (n == 0) return false;
            At[0][DsConst.InputBase + n - 1] = 0;
            return true;
        }

        protected void FillInput(int count, int salt)
        {
            InClear();
            DeterministicRng rng = new DeterministicRng(unchecked((uint)Seed * 2654435761u + (uint)salt));
            for (int i = 0; i < count && i < DsConst.InputCount; i++)
            {
                At[0][DsConst.InputBase + i] = 1 + rng.Next(DsConst.MaxKey);
            }
        }

        /// <summary>界面进程发来的批量插入：按种子推出来的几个键，可复现。</summary>
        protected void AppendRandomKeys(int count)
        {
            int have = InLen();
            DeterministicRng rng = new DeterministicRng(unchecked((uint)Seed * 40503u + (uint)(have * 7919 + 13)));
            for (int i = 0; i < count; i++)
            {
                if (!InAppend(1 + rng.Next(DsConst.MaxKey))) return;
            }
        }
    }

    // ───────────────────────────────────────────────────────────── 场景 1：动态数组

    /// <summary>
    /// 动态数组的增长：容量从 0 起按 1、2、4、8… 跳，每跳一次把旧块整个搬到新块，
    /// 旧地址随即失效。选「先 reserve」时一开始就把容量要够，对照着看扩容次数。
    /// </summary>
    internal sealed class ArrayModel : DsModel
    {
        private int size, capacity, phase, cursor, ghostCount;
        private int copies, expands, lastCapacity;

        public ArrayModel(VisualSnapshot s) : base(s) { }

        public override void BuildTarget()
        {
            int count = OptionB == 0 ? 12 : (OptionB == 1 ? 20 : 24);
            FillInput(count, 101);
        }

        public override void ResetRun()
        {
            ClearRun();
            ClearStructKeys();
            size = 0;
            copies = 0;
            expands = 0;
            lastCapacity = 0;
            cursor = 0;
            ghostCount = 0;
            phase = 0;
            capacity = OptionA == 1 ? 32 : (OptionA == 2 ? 8 : 0);
            if (capacity > 64) capacity = 64;
            PaintSlots();
            Flush();
        }

        /// <summary>按容量给槽上色：容量内空着、容量外不可用。</summary>
        private void PaintSlots()
        {
            for (int i = 0; i < 64; i++)
            {
                if (i < size) St[i] = Ds.Data;
                else St[i] = i < capacity ? Ds.Empty : Ds.Blocked;
            }
            for (int j = 0; j < 32; j++) St[64 + j] = Ds.Empty;
        }

        public override bool Step()
        {
            int count = InLen();
            switch (phase)
            {
                case 0:
                    if (size >= count) return false;
                    if (size >= capacity) { phase = 1; currentIndex = 0; return true; }   // 搬迁通道先亮出来
                    Place();
                    return true;
                case 1:
                    lastCapacity = capacity;
                    capacity = capacity == 0 ? 1 : capacity * 2;
                    if (capacity > 64) capacity = 64;
                    expands++;
                    ghostCount = size;
                    for (int j = 0; j < 32; j++) St[64 + j] = Ds.Empty;
                    for (int j = 0; j < ghostCount; j++)
                    {
                        At[1][64 + j] = j;                 // 旧块里的第几个槽
                        At[2][64 + j] = At[0][j];          // 旧块里存的值
                        St[64 + j] = Ds.Pending;           // 还没搬走
                    }
                    for (int i = 0; i < 64; i++)
                    {
                        if (i < size) St[i] = Ds.Marked;
                        else St[i] = i < capacity ? Ds.Empty : Ds.Blocked;
                    }
                    cursor = 0;
                    phase = 2;
                    return true;
                case 2:
                    if (cursor < ghostCount)
                    {
                        if (cursor > 0)
                        {
                            St[64 + cursor - 1] = Ds.Blocked;   // 旧地址从此失效
                            St[cursor - 1] = Ds.Data;
                        }
                        St[64 + cursor] = Ds.Active;
                        St[cursor] = Ds.Marked;
                        currentIndex = cursor;
                        copies++;
                        cursor++;
                        return true;
                    }
                    for (int j = 0; j < 32; j++) if (St[64 + j] != Ds.Empty) St[64 + j] = Ds.Blocked;
                    for (int i = 0; i < size; i++) St[i] = Ds.Data;
                    phase = 3;
                    return true;
                default:
                    Place();
                    return true;
            }
        }

        private void Place()
        {
            int value = InAt(size);
            if (value <= 0) return;
            At[0][size] = value;
            St[size] = Ds.Data;
            currentIndex = size;
            size++;
            phase = 0;
        }

        public override string StatusText()
        {
            if (Stopped) return "动态数组：size=" + size + "，capacity=" + capacity
                                 + "，扩容 " + expands + " 次，搬迁 " + copies + " 个元素";
            return "动态数组：size=" + size + "／capacity=" + capacity
                   + "，上一次容量 " + lastCapacity;
        }

        private bool Stopped { get { return size >= InLen() && phase == 0; } }

        public override string Title()
        {
            return "容量跳变 0→" + capacity + "，扩容 " + expands + " 次，共搬迁 " + copies
                   + " 个元素；灰色槽是容量内还没用的，深色槽是容量外没分配的";
        }

        public override void Command(int op, int key, int target)
        {
            if (op == Ds.CmdInsert) InAppend(ClampKey(key));
            else if (op == Ds.CmdDelete)
            {
                if (target >= 0 && target < 64) InRemoveAt(target);
                else InRemoveLast();
            }
            else if (op == Ds.CmdClear) InClear();
            else if (op == Ds.CmdRegenerate) BuildTarget();
            else if (op == Ds.CmdAppendRandom) AppendRandomKeys(5);
        }

        private static int ClampKey(int key)
        {
            if (key < DsConst.MinKey) return DsConst.MinKey;
            if (key > DsConst.MaxKey) return DsConst.MaxKey;
            return key;
        }

        public override int Primary { get { return capacity; } }
        public override int Secondary { get { return size; } }
        public override int Work { get { return copies; } }
        public override int Pending { get { return capacity; } }
        public override int CustomA { get { return expands; } }
        public override int CustomB { get { return copies; } }
    }

    // ───────────────────────────────────────────────────────────── 场景 2：链表

    /// <summary>
    /// 带哨兵的链表：节点下标 1 起，0 号是头哨兵。插入分「定位」与「写指针」两段，
    /// 每一步都记下改了哪几个指针；删除只改一个指针，被摘掉的节点标成失效。
    /// </summary>
    internal sealed class ListModel : DsModel
    {
        private const int Sentinel = 0;
        private const int Capacity = 15;

        private int inserted, phase, cursor, compares, pointerWrites, deletes;
        private int walk, prev, fresh, linkStep, selKey;

        public ListModel(VisualSnapshot s) : base(s) { }

        public override void BuildTarget()
        {
            int count = OptionB == 0 ? 12 : (OptionB == 1 ? 8 : 16);
            int[] values = new int[16];
            DeterministicRng rng = new DeterministicRng(unchecked((uint)Seed * 2654435761u + 211u));
            for (int i = 0; i < count; i++) values[i] = 1 + rng.Next(90);
            if (OptionA == 0)
            {
                // 有序插入：先把要插的键排一遍，插入位置才好预测，看的是指针不是比较
                for (int i = 1; i < count; i++)
                {
                    int key = values[i], j = i - 1;
                    while (j >= 0 && values[j] > key) { values[j + 1] = values[j]; j--; }
                    values[j + 1] = key;
                }
            }
            InClear();
            for (int i = 0; i < count && i < Capacity; i++) InAppend(values[i]);
        }

        public override void ResetRun()
        {
            ClearRun();
            ClearStructKeys();
            inserted = 0;
            phase = 0;
            cursor = 0;
            compares = 0;
            pointerWrites = 0;
            deletes = 0;
            walk = Sentinel;
            prev = Sentinel;
            linkStep = 0;
            selKey = -1;
            St[Sentinel] = Ds.Anchor;
            At[1][Sentinel] = -1;
            At[2][Sentinel] = -1;
            Flush();
        }

        public override bool Step()
        {
            int count = InLen();
            switch (phase)
            {
                case 0:
                    if (inserted >= count)
                    {
                        if (deletes >= 2 || At[1][Sentinel] < 0) return false;
                        phase = 4;
                        return true;
                    }
                    fresh = FreeNode();
                    if (fresh < 0) return false;
                    At[0][fresh] = InAt(inserted);
                    St[fresh] = Ds.Pending;
                    At[4][fresh] = inserted;
                    walk = Sentinel;
                    prev = Sentinel;
                    phase = 1;
                    return true;
                case 1:
                    cursor = At[1][walk];
                    if (OptionA == 1 && cursor >= 0)          // 尾部接上：一路走到尾
                    {
                        compares++;
                        St[walk] = Ds.Done;
                        walk = cursor;
                        St[walk] = Ds.Active;
                        currentIndex = walk;
                        return true;
                    }
                    if (OptionA == 2) { phase = 2; return true; }   // 头部插入：不必找
                    if (cursor < 0) { phase = 2; return true; }
                    compares++;
                    St[cursor] = Ds.Ref;
                    currentIndex = cursor;
                    if (At[0][cursor] < At[0][fresh])
                    {
                        prev = cursor;
                        walk = cursor;
                        return true;
                    }
                    phase = 2;
                    return true;
                case 2:
                    if (linkStep == 0)
                    {
                        At[1][fresh] = At[1][prev];           // new->next = prev->next
                        At[2][fresh] = prev;
                        pointerWrites++;
                        Mk[fresh] = 1;
                        St[fresh] = Ds.Marked;
                        currentIndex = fresh;
                        linkStep = 1;
                        return true;
                    }
                    At[1][prev] = fresh;                      // prev->next = new
                    At[2][fresh] = prev;
                    pointerWrites++;
                    At[3][fresh] = pointerWrites;
                    if (At[1][fresh] >= 0) At[2][At[1][fresh]] = fresh;
                    St[fresh] = Ds.Done;
                    Mk[fresh] = 0;
                    if (prev == Sentinel) St[Sentinel] = Ds.Anchor;
                    St[prev] = prev == Sentinel ? Ds.Anchor : Ds.Done;
                    currentIndex = fresh;
                    linkStep = 0;
                    inserted++;
                    phase = 0;
                    return true;
                default:
                    return StepDelete();
            }
        }

        /// <summary>删除头节点：只改一个指针，被摘掉的节点标成失效。</summary>
        private bool StepDelete()
        {
            int victim = At[1][Sentinel];
            if (victim < 0) return false;
            At[1][Sentinel] = At[1][victim];
            if (At[1][victim] >= 0) At[2][At[1][victim]] = Sentinel;
            pointerWrites++;
            St[victim] = Ds.Blocked;
            Mk[victim] = 0;
            At[3][victim] = -1;
            deletes++;
            currentIndex = victim;
            if (deletes >= 2) return false;
            return true;
        }

        private int FreeNode()
        {
            for (int i = 1; i <= Capacity; i++) if (At[0][i] <= 0) return i;
            return -1;
        }

        public override string StatusText()
        {
            int live = 0;
            for (int i = 1; i <= Capacity; i++) if (At[0][i] > 0 && St[i] != Ds.Blocked) live++;
            return "链表：节点 " + live + " 个，插入 " + inserted + " 次，指针写入 "
                   + pointerWrites + " 次，比较 " + compares + " 次";
        }

        public override string Title()
        {
            return "0 号是头哨兵；插入分两步写指针（new->next、prev->next），删除只改一个指针";
        }

        public override void Command(int op, int key, int target)
        {
            if (op == Ds.CmdInsert) InAppend(ClampKey(key));
            else if (op == Ds.CmdDelete)
            {
                if (target > 0 && At[0][target] > 0) selKey = At[0][target];
                InRemoveLast();
            }
            else if (op == Ds.CmdClear) InClear();
            else if (op == Ds.CmdRegenerate) BuildTarget();
            else if (op == Ds.CmdAppendRandom) AppendRandomKeys(3);
            else if (op == Ds.CmdFind) selKey = key;
        }

        private static int ClampKey(int key)
        {
            if (key < DsConst.MinKey) return DsConst.MinKey;
            if (key > DsConst.MaxKey) return DsConst.MaxKey;
            return key;
        }

        public override int Primary { get { return inserted; } }
        public override int Secondary { get { return pointerWrites; } }
        public override int Work { get { return compares; } }
        public override int Pending { get { return Math.Max(0, InLen() - inserted); } }
        public override int CustomA { get { return deletes; } }
        public override int CustomB { get { return selKey; } }
    }

    // ───────────────────────────────────────────────────────────── 场景 3：哈希表

    /// <summary>
    /// 拉链法哈希表：桶数组 + 冲突链。每次插入都要走一遍「算哈希 → 走到桶 →
    /// 沿链比较 → 挂上」，负载因子越过 0.75 就再哈希：桶数翻倍，键一个一个搬。
    /// </summary>
    internal sealed class HashModel : DsModel
    {
        private int buckets, count, phase, cursor, compares, rehashes, moves;
        private int walk, fresh, bucket, maxChain, lastMoved = -1;
        private int nextKey;

        public HashModel(VisualSnapshot s) : base(s) { }

        public override void BuildTarget()
        {
            // 键取成互不相同的：哈希表演示里重复的键只会变成一次「已存在」，
            // 看的是散列与冲突，不是去重
            InClear();
            bool[] used = new bool[DsConst.MaxKey + 1];
            DeterministicRng rng = new DeterministicRng(unchecked((uint)Seed * 2654435761u + 307u));
            for (int i = 0; i < 24; i++)
            {
                int key = 1 + rng.Next(DsConst.MaxKey);
                while (used[key]) key = 1 + (key % DsConst.MaxKey);
                used[key] = true;
                At[0][DsConst.InputBase + i] = key;
            }
        }

        public override void ResetRun()
        {
            ClearRun();
            ClearStructKeys();
            buckets = OptionB == 0 ? 8 : 16;
            if (buckets < 4) buckets = 4;
            count = 0;
            nextKey = 0;
            phase = 0;
            compares = 0;
            rehashes = 0;
            moves = 0;
            maxChain = 0;
            cursor = 0;
            walk = -1;
            fresh = -1;
            bucket = -1;
            for (int b = 0; b < 32; b++)
            {
                At[1][b] = -1;
                St[b] = b < buckets ? Ds.Anchor : Ds.Empty;
            }
            for (int n = 32; n < 96; n++) { At[0][n] = 0; At[2][n] = -1; }
            Flush();
        }

        private int Hash(int key, int mod)
        {
            if (OptionA == 1)
            {
                // 乘法散列（Fibonacci）：先乘一个奇数常数，再取高位，最后对桶数取模
                uint mixed = unchecked((uint)key * 2654435761u);
                return (int)(mixed >> 16) % mod;
            }
            return key % mod;
        }

        public override bool Step()
        {
            int total = InLen();
            switch (phase)
            {
                case 0:
                    if (nextKey >= total) return false;
                    bucket = Hash(InAt(nextKey), buckets);
                    St[bucket] = Ds.Active;
                    currentIndex = bucket;
                    walk = At[1][bucket];
                    phase = 1;
                    return true;
                case 1:
                    if (walk < 0) { phase = 2; return true; }
                    compares++;
                    St[walk] = Ds.Ref;
                    currentIndex = walk;
                    if (At[0][walk] == InAt(nextKey)) { phase = 3; return true; }   // 已经有这个键
                    walk = At[1][walk];
                    return true;
                case 2:
                    fresh = FreeNode();
                    if (fresh < 0) return false;
                    At[0][fresh] = InAt(nextKey);
                    At[2][fresh] = bucket;
                    At[1][fresh] = At[1][bucket];
                    At[1][bucket] = fresh;
                    St[fresh] = Ds.Marked;
                    Mk[fresh] = 1;
                    count++;
                    currentIndex = fresh;
                    phase = 3;
                    return true;
                case 3:
                    St[bucket] = Ds.Anchor;
                    for (int i = 32; i < 96; i++) if (St[i] == Ds.Ref || St[i] == Ds.Marked) St[i] = i == fresh ? Ds.Done : Ds.Data;
                    fresh = -1;
                    nextKey++;
                    Mk[0] = 0;
                    maxChain = ChainMax();
                    if (count * 1000 > buckets * DsConst.LoadFactorMilli) { phase = 4; return true; }
                    phase = 0;
                    return true;
                case 4:
                    buckets = buckets * 2;
                    if (buckets > 32) buckets = 32;
                    rehashes++;
                    for (int b = 0; b < 32; b++)
                    {
                        At[1][b] = -1;
                        St[b] = b < buckets ? Ds.Pending : Ds.Empty;
                    }
                    for (int i = 32; i < 96; i++)
                    {
                        if (At[0][i] <= 0) continue;
                        At[2][i] = -1;
                        St[i] = Ds.Pending;      // 还没搬，仍挂在旧桶号上
                    }
                    cursor = 32;
                    phase = 5;
                    return true;
                default:
                    while (cursor < 96 && At[0][cursor] <= 0) cursor++;
                    if (cursor >= 96)
                    {
                        // 搬完了：桶回到锚点色，节点回到数据色，运行状态里不留搬运痕迹
                        for (int b = 0; b < 32; b++) St[b] = b < buckets ? Ds.Anchor : Ds.Empty;
                        for (int i = 32; i < 96; i++)
                        {
                            if (At[0][i] > 0) { St[i] = Ds.Data; Mk[i] = 0; }
                        }
                        currentIndex = -1;
                        fresh = -1;
                        phase = 0;
                        maxChain = ChainMax();
                        return true;
                    }
                    {
                        int key = At[0][cursor];
                        int target = Hash(key, buckets);
                        At[1][cursor] = At[1][target];
                        At[1][target] = cursor;
                        At[2][cursor] = target;
                        if (lastMoved >= 0) { St[lastMoved] = Ds.Data; Mk[lastMoved] = 0; }
                        St[cursor] = Ds.Marked;
                        Mk[cursor] = 1;
                        St[target] = Ds.Active;
                        lastMoved = cursor;
                        currentIndex = cursor;
                        moves++;
                        cursor++;
                        if (cursor > 96) cursor = 96;
                        return true;
                    }
            }
        }

        private int FreeNode()
        {
            for (int i = 32; i < 96; i++) if (At[0][i] <= 0) return i;
            return -1;
        }

        private int ChainMax()
        {
            int best = 0;
            for (int b = 0; b < buckets && b < 32; b++)
            {
                int n = 0, node = At[1][b], guard = 0;
                while (node >= 32 && node < 96 && guard++ < 64) { n++; node = At[1][node]; }
                if (n > best) best = n;
            }
            return best;
        }

        public override string StatusText()
        {
            int load = buckets > 0 ? count * 1000 / buckets : 0;
            return "哈希表：桶 " + buckets + " 个，键 " + count + " 个，负载因子 "
                   + (load / 1000.0).ToString("0.00", CultureInfo.InvariantCulture)
                   + "，最长链 " + maxChain + "，再哈希 " + rehashes + " 次";
        }

        public override string Title()
        {
            return "负载因子越过 0.75 就再哈希：桶数翻倍，键一个一个搬（搬运 " + moves + " 次）";
        }

        public override void Command(int op, int key, int target)
        {
            if (op == Ds.CmdInsert) InAppend(ClampKey(key));
            else if (op == Ds.CmdDelete) InRemoveLast();
            else if (op == Ds.CmdClear) InClear();
            else if (op == Ds.CmdRegenerate) BuildTarget();
            else if (op == Ds.CmdAppendRandom) AppendRandomKeys(4);
        }

        private static int ClampKey(int key)
        {
            if (key < DsConst.MinKey) return DsConst.MinKey;
            if (key > DsConst.MaxKey) return DsConst.MaxKey;
            return key;
        }

        public override int Primary { get { return buckets; } }
        public override int Secondary { get { return count; } }
        public override int Work { get { return compares; } }
        public override int Pending { get { return maxChain; } }
        public override int CustomA { get { return rehashes; } }
        public override int CustomB { get { return moves; } }
    }

    // ───────────────────────────────────────────────────────────── 场景 4：搜索树 → 平衡

    /// <summary>
    /// 搜索树：朴素 BST、AVL、红黑树三种。节点属性：Attr0 键、Attr1 左、Attr2 右、
    /// Attr3 父、Attr4 高度、Attr5 颜色（0 红 1 黑）或平衡因子。
    /// 旋转只改指针，坐标由画布按中序排名重新算，因此一次旋转屏幕上就重排一次。
    /// </summary>
    internal sealed class TreeModel : DsModel
    {
        private const int Red = 0;
        private const int Black = 1;

        private int root, count, phase, compares, rotations, recolors;
        private int walk, parentWalk, fresh, fixNode, lastKey;
        private int[] keys = new int[64];
        private int[] left = new int[64];
        private int[] right = new int[64];
        private int[] up = new int[64];
        private int[] height = new int[64];
        private int[] color = new int[64];

        public TreeModel(VisualSnapshot s) : base(s) { }

        public override void BuildTarget()
        {
            int count2 = 16;
            int[] values = new int[count2];
            bool[] used = new bool[64];
            DeterministicRng rng = new DeterministicRng(unchecked((uint)Seed * 2654435761u + 409u));
            for (int i = 0; i < count2; i++)
            {
                int value = 1 + rng.Next(60);
                while (used[value]) value = 1 + (value % 60);
                used[value] = true;
                values[i] = value;
            }
            if (OptionB == 1)
            {
                // 升序：朴素 BST 会退化成一条链
                Array.Sort(values);
            }
            else if (OptionB == 2)
            {
                // 锯齿：先中间，再左半中间，再右半中间 —— 插出来天生就平衡
                Array.Sort(values);
                int[] ordered = new int[count2];
                int at = 0;
                MidOrder(values, 0, count2 - 1, ordered, ref at);
                values = ordered;
            }
            InClear();
            for (int i = 0; i < count2; i++) InAppend(values[i]);
        }

        private static void MidOrder(int[] src, int lo, int hi, int[] dst, ref int at)
        {
            if (lo > hi) return;
            int mid = (lo + hi) / 2;
            dst[at++] = src[mid];
            MidOrder(src, lo, mid - 1, dst, ref at);
            MidOrder(src, mid + 1, hi, dst, ref at);
        }

        public override void ResetRun()
        {
            ClearRun();
            ClearStructKeys();
            root = -1;
            count = 0;
            phase = 0;
            compares = 0;
            rotations = 0;
            recolors = 0;
            walk = -1;
            fresh = -1;
            fixNode = -1;
            lastKey = 0;
            for (int i = 0; i < 64; i++)
            {
                keys[i] = 0; left[i] = -1; right[i] = -1; up[i] = -1;
                height[i] = 0; color[i] = Black;
            }
            Flush();
        }

        public override bool Step()
        {
            int total = InLen();
            switch (phase)
            {
                case 0:
                    if (count >= total) return false;
                    lastKey = InAt(count);
                    walk = root;
                    parentWalk = -1;
                    phase = 1;
                    return true;
                case 1:
                    if (walk < 0)
                    {
                        fresh = Alloc();
                        if (fresh < 0) return false;
                        keys[fresh] = lastKey;
                        up[fresh] = parentWalk;
                        height[fresh] = 1;
                        color[fresh] = Red;
                        if (parentWalk < 0) root = fresh;
                        else if (lastKey < keys[parentWalk]) left[parentWalk] = fresh;
                        else right[parentWalk] = fresh;
                        count++;
                        St[fresh] = Ds.Marked;
                        fixNode = parentWalk;
                        phase = OptionA == 0 ? 4 : 2;
                        return true;
                    }
                    compares++;
                    parentWalk = walk;
                    if (lastKey < keys[walk]) { walk = left[walk]; St[parentWalk] = Ds.Active; }
                    else if (lastKey > keys[walk]) { walk = right[walk]; St[parentWalk] = Ds.Active; }
                    else { phase = 3; return true; }        // 键重复，不插
                    currentIndex = parentWalk;
                    return true;
                case 2:
                    if (OptionA == 2) return StepRedBlack();
                    return StepAvl();
                default:
                    PaintTree();
                    currentIndex = fresh;
                    phase = 0;
                    return true;
            }
        }

        // ── AVL：沿着路径往上一层层更新高度，碰到失衡就转

        private bool StepAvl()
        {
            if (fixNode < 0) { phase = 3; return true; }
            UpdateHeight(fixNode);
            int bf = Balance(fixNode);
            if (bf > 1)
            {
                if (Balance(left[fixNode]) < 0)
                {
                    int child = left[fixNode];
                    RotateLeft(child);
                    rotations++;
                    currentIndex = fixNode;
                    St[fixNode] = Ds.Marked;
                    St[child] = Ds.Marked;
                    return true;
                }
                int newRoot = RotateRight(fixNode);
                rotations++;
                currentIndex = newRoot;
                St[newRoot] = Ds.Marked;
                fixNode = up[newRoot];
                return true;
            }
            if (bf < -1)
            {
                if (Balance(right[fixNode]) > 0)
                {
                    int child = right[fixNode];
                    RotateRight(child);
                    rotations++;
                    currentIndex = fixNode;
                    St[fixNode] = Ds.Marked;
                    St[child] = Ds.Marked;
                    return true;
                }
                int newRoot = RotateLeft(fixNode);
                rotations++;
                currentIndex = newRoot;
                St[newRoot] = Ds.Marked;
                fixNode = up[newRoot];
                return true;
            }
            currentIndex = fixNode;
            St[fixNode] = Ds.Done;
            fixNode = up[fixNode];
            return true;
        }

        // ── 红黑树：变色一步、旋转一步

        private bool StepRedBlack()
        {
            if (fixNode < 0 || color[fixNode] == Black)
            {
                if (root >= 0) color[root] = Black;
                phase = 3;
                return true;
            }
            int p = up[fixNode];
            if (p < 0 || color[p] == Black) { phase = 3; return true; }
            int g = up[p];
            if (g < 0) { color[p] = Black; recolors++; phase = 3; return true; }
            int uncle = left[g] == p ? right[g] : left[g];
            if (uncle >= 0 && color[uncle] == Red)
            {
                color[p] = Black;
                color[uncle] = Black;
                color[g] = Red;
                recolors++;
                St[p] = Ds.Marked;
                St[uncle] = Ds.Marked;
                St[g] = Ds.Marked;
                currentIndex = g;
                fixNode = g;
                return true;
            }
            if (left[g] == p && right[p] == fixNode)
            {
                RotateLeft(p);
                rotations++;
                currentIndex = g;
                St[g] = Ds.Marked;
                return true;
            }
            if (right[g] == p && left[p] == fixNode)
            {
                RotateRight(p);
                rotations++;
                currentIndex = g;
                St[g] = Ds.Marked;
                return true;
            }
            if (left[g] == p)
            {
                RotateRight(g);
                color[p] = Black;
                color[g] = Red;
                recolors++;
                rotations++;
                St[p] = Ds.Marked;
                St[g] = Ds.Marked;
                currentIndex = p;
                fixNode = up[p];
                return true;
            }
            RotateLeft(g);
            color[p] = Black;
            color[g] = Red;
            recolors++;
            rotations++;
            St[p] = Ds.Marked;
            St[g] = Ds.Marked;
            currentIndex = p;
            fixNode = up[p];
            return true;
        }

        // ── 旋转与高度

        private int RotateLeft(int x)
        {
            int y = right[x];
            if (y < 0) return x;
            right[x] = left[y];
            if (left[y] >= 0) up[left[y]] = x;
            up[y] = up[x];
            if (up[x] < 0) root = y;
            else if (left[up[x]] == x) left[up[x]] = y;
            else right[up[x]] = y;
            left[y] = x;
            up[x] = y;
            UpdateHeight(x);
            UpdateHeight(y);
            return y;
        }

        private int RotateRight(int x)
        {
            int y = left[x];
            if (y < 0) return x;
            left[x] = right[y];
            if (right[y] >= 0) up[right[y]] = x;
            up[y] = up[x];
            if (up[x] < 0) root = y;
            else if (left[up[x]] == x) left[up[x]] = y;
            else right[up[x]] = y;
            right[y] = x;
            up[x] = y;
            UpdateHeight(x);
            UpdateHeight(y);
            return y;
        }

        private void UpdateHeight(int node)
        {
            if (node < 0) return;
            int lh = left[node] >= 0 ? height[left[node]] : 0;
            int rh = right[node] >= 0 ? height[right[node]] : 0;
            height[node] = 1 + (lh > rh ? lh : rh);
        }

        private int Balance(int node)
        {
            if (node < 0) return 0;
            int lh = left[node] >= 0 ? height[left[node]] : 0;
            int rh = right[node] >= 0 ? height[right[node]] : 0;
            return lh - rh;
        }

        /// <summary>
        /// 整棵树的高度重算一遍。AVL 的旋转会顺手维护高度，朴素 BST 不会，
        /// 「树高」这个数两种树都要给，所以这里统一从叶子往上算一次。
        /// </summary>
        private int RecalcHeights(int node, int depth)
        {
            if (node < 0 || depth > 64) return 0;
            int lh = RecalcHeights(left[node], depth + 1);
            int rh = RecalcHeights(right[node], depth + 1);
            height[node] = 1 + (lh > rh ? lh : rh);
            return height[node];
        }

        private int Alloc()
        {
            for (int i = 0; i < 64; i++) if (keys[i] == 0) return i;
            return -1;
        }

        /// <summary>把运行状态摊进元素数组：树上的节点是数据色，当前路径上的换色。</summary>
        private void PaintTree()
        {
            RecalcHeights(root, 0);
            for (int i = 0; i < 64; i++)
            {
                if (keys[i] == 0) { St[i] = Ds.Empty; At[0][i] = 0; continue; }
                At[0][i] = keys[i];
                At[1][i] = left[i];
                At[2][i] = right[i];
                At[3][i] = up[i];
                At[4][i] = height[i];
                At[5][i] = OptionA == 2 ? color[i] : Balance(i) + 10;
                if (St[i] == Ds.Empty) St[i] = Ds.Data;
                if (i == root) St[i] = Ds.Anchor;
                if (i == Current) St[i] = Ds.Active;
            }
        }

        public override string StatusText()
        {
            string name = OptionA == 0 ? "朴素 BST" : (OptionA == 1 ? "AVL" : "红黑树");
            return name + "：节点 " + count + " 个，树高 " + Height()
                   + "，比较 " + compares + " 次，旋转 " + rotations + " 次"
                   + (OptionA == 2 ? "，变色 " + recolors + " 次" : "");
        }

        public override string Title()
        {
            if (OptionA == 0)
            {
                return OptionB == 1
                    ? "升序插入：每个新键都挂在最右边，树退化成一条高 " + Height() + " 的链"
                    : "朴素 BST：没有任何平衡手段；把「选项二」换成升序就能看到退化";
            }
            return (OptionA == 1 ? "AVL" : "红黑树") + "：旋转只改指针，节点坐标按中序排名重排，"
                   + "因此一次旋转屏幕上就重排一次";
        }

        public override void Command(int op, int key, int target)
        {
            if (op == Ds.CmdInsert) InAppend(Clamp(key));
            else if (op == Ds.CmdDelete) InRemoveLast();
            else if (op == Ds.CmdClear) InClear();
            else if (op == Ds.CmdRegenerate) BuildTarget();
            else if (op == Ds.CmdAppendRandom) AppendRandomKeys(4);
        }

        private static int Clamp(int key)
        {
            if (key < 1) return 1;
            if (key > 60) return 60;
            return key;
        }

        public override int Primary { get { return Height(); } }
        public override int Secondary { get { return rotations; } }
        public override int Work { get { return compares; } }
        public override int Pending { get { return count; } }
        public override int Focus { get { return root; } }
        public override int CustomA { get { return recolors; } }
        public override int CustomB { get { return lastKey; } }

        private int Height() { return root < 0 ? 0 : height[root]; }
    }

    // ───────────────────────────────────────────────────────────── 场景 5：堆

    /// <summary>
    /// 二叉堆：**同一份数组画两次**——上面按完全二叉树排成树形，下面按存储顺序排成数组行，
    /// 两边用的是同一批元素，因此下沉／上浮时两边同时换色。
    /// 先自底向上建堆（下沉），再 push 两个（上浮），再 pop 两个（下沉）。
    /// </summary>
    internal sealed class HeapModel : DsModel
    {
        private int size, phase, action, buildAt, siftAt, pushAt, popCount;
        private int compares, swaps;
        private int[] val = new int[32];

        public HeapModel(VisualSnapshot s) : base(s) { }

        public override void BuildTarget()
        {
            FillInput(OptionB == 0 ? 15 : 31, 503);
        }

        public override void ResetRun()
        {
            ClearRun();
            ClearStructKeys();
            int total = InLen();
            int build = total - 2;
            if (build < 1) build = 1;
            size = 0;
            for (int i = 0; i < build; i++)
            {
                val[i] = InAt(i);
                At[0][i] = val[i];
                St[i] = Ds.Data;
                size++;
            }
            for (int i = size; i < 32; i++) { At[0][i] = 0; St[i] = Ds.Empty; }
            phase = 0;
            action = 0;
            buildAt = size / 2 - 1;
            siftAt = -1;
            pushAt = build;
            popCount = 0;
            compares = 0;
            swaps = 0;
            currentIndex = -1;
            Flush();
        }

        public override bool Step()
        {
            int total = InLen();
            switch (phase)
            {
                case 0:
                    if (action == 0)
                    {
                        if (buildAt < 0) { phase = 1; return true; }
                        siftAt = buildAt;
                        action = 1;
                        St[siftAt] = Ds.Active;
                        currentIndex = siftAt;
                        return true;
                    }
                    if (SiftDown()) return true;
                    action = 0;
                    St[siftAt] = Ds.Done;
                    buildAt--;
                    return true;
                case 1:
                    if (action == 0)
                    {
                        if (pushAt >= total || size >= 32) { phase = 2; return true; }
                        val[size] = InAt(pushAt);
                        At[0][size] = val[size];
                        St[size] = Ds.Marked;
                        size++;
                        pushAt++;
                        siftAt = size - 1;
                        action = 2;
                        currentIndex = siftAt;
                        return true;
                    }
                    if (SiftUp()) return true;
                    action = 0;
                    St[siftAt] = Ds.Done;
                    return true;
                default:
                    if (action == 0)
                    {
                        if (popCount >= 2 || size <= 1) return false;
                        Swap(0, size - 1);
                        swaps++;
                        St[size - 1] = Ds.Blocked;      // 弹出的元素留在画面上，标成失效
                        size--;
                        siftAt = 0;
                        action = 1;
                        popCount++;
                        St[0] = Ds.Active;
                        currentIndex = 0;
                        return true;
                    }
                    if (SiftDown()) return true;
                    action = 0;
                    St[siftAt] = Ds.Done;
                    return true;
            }
        }

        /// <summary>下沉一步：与两个孩子比一遍，该换就换一次。</summary>
        private bool SiftDown()
        {
            int l = siftAt * 2 + 1, r = l + 1, small = siftAt;
            if (l < size) { compares++; if (val[l] < val[small]) small = l; }
            if (r < size) { compares++; if (val[r] < val[small]) small = r; }
            if (small == siftAt) return false;
            Swap(siftAt, small);
            swaps++;
            St[siftAt] = Ds.Done;
            St[small] = Ds.Active;
            currentIndex = small;
            siftAt = small;
            return true;
        }

        /// <summary>上浮一步：与父节点比一次，该换就换一次。</summary>
        private bool SiftUp()
        {
            if (siftAt <= 0) return false;
            int p = (siftAt - 1) / 2;
            compares++;
            if (val[p] <= val[siftAt]) return false;
            Swap(p, siftAt);
            swaps++;
            St[p] = Ds.Done;
            St[siftAt] = Ds.Active;
            currentIndex = siftAt;
            siftAt = p;
            return true;
        }

        private void Swap(int a, int b)
        {
            int t = val[a]; val[a] = val[b]; val[b] = t;
            At[0][a] = val[a];
            At[0][b] = val[b];
        }

        public override string StatusText()
        {
            return "堆：堆大小 " + size + "，比较 " + compares + " 次，交换 " + swaps
                   + " 次，已弹出 " + popCount + " 个";
        }

        public override string Title()
        {
            return "同一份数组画两次：上面是完全二叉树形状，下面是存储顺序（下标 0 起）；"
                   + "两边同步换色，堆顶永远是最小的那个";
        }

        public override void Command(int op, int key, int target)
        {
            if (op == Ds.CmdInsert) InAppend(Clamp(key));
            else if (op == Ds.CmdDelete) InRemoveLast();
            else if (op == Ds.CmdClear) InClear();
            else if (op == Ds.CmdRegenerate) BuildTarget();
            else if (op == Ds.CmdAppendRandom) AppendRandomKeys(4);
        }

        private static int Clamp(int key)
        {
            if (key < 1) return 1;
            if (key > 60) return 60;
            return key;
        }

        public override int Primary { get { return size; } }
        public override int Secondary { get { return swaps; } }
        public override int Work { get { return compares; } }
        public override int Pending { get { return pushAt; } }
        public override int CustomA { get { return popCount; } }
        public override int CustomB { get { return size > 0 ? val[0] : -1; } }
    }

    // ───────────────────────────────────────────────────────────── 场景 6：B 树

    /// <summary>
    /// B 树（阶 4，每节点最多 3 个键）。**每个节点占两个元素**：一个是看得见的节点盒
    /// （Attr0..2 三个键槽、Tag 键数、Attr3 叶子标志、Attr4 父、Attr5 在父里的孩子序号），
    /// 紧挨着的下一个是看不见的孩子表（Attr0..3 四个孩子）。这样一拆，
    /// 「一个节点多个槽」在画面上就是三个格子，孩子指针也放得下。
    /// </summary>
    internal sealed class BTreeModel : DsModel
    {
        private const int MaxNodes = 24;
        private const int MaxKeys = 3;
        private const int MinKeys = 1;

        private int root, count, phase, cur, targetKey;
        private int compares, splits, merges, borrows, deletes;
        private bool deleted;
        private int lastMarked = -1;

        public BTreeModel(VisualSnapshot s) : base(s) { }

        private static int Node(int id) { return id * 2; }
        private static int Table(int id) { return id * 2 + 1; }

        public override void BuildTarget()
        {
            int n = OptionB == 0 ? 8 : (OptionB == 1 ? 10 : 12);
            int[] values = new int[n];
            bool[] used = new bool[100];
            DeterministicRng rng = new DeterministicRng(unchecked((uint)Seed * 2654435761u + 601u));
            for (int i = 0; i < n; i++)
            {
                int value = 1 + rng.Next(90);
                while (used[value]) value = 1 + (value % 90);
                used[value] = true;
                values[i] = value;
            }
            InClear();
            for (int i = 0; i < n; i++) InAppend(values[i]);
        }

        public override void ResetRun()
        {
            ClearRun();
            ClearStructKeys();
            for (int i = 0; i < MaxNodes * 2; i++)
            {
                At[0][i] = 0; At[3][i] = 0;
                for (int k = 0; k < 6; k++) At[k][i] = -1;
                Tg[i] = 0;
            }
            root = AllocNode();
            At[3][Node(root)] = 1;
            Tg[Node(root)] = 0;
            St[Node(root)] = Ds.Anchor;
            count = 0;
            phase = 0;
            cur = -1;
            compares = 0;
            splits = 0;
            merges = 0;
            borrows = 0;
            deletes = 0;
            deleted = false;
            currentIndex = -1;
            focusIndex = Node(root);
            if (OptionA == 1)
            {
                // 只删除：先把树安静地建好，再动画地删
                int guard = 0;
                while (phase != 2 && guard++ < 40000) { if (!Step()) break; }
            }
            Flush();
        }

        /// <summary>刚变更的节点只留最新那一个，免得整棵树都停在上一次的紫色上。</summary>
        private void Mark(int id)
        {
            if (lastMarked >= 0 && lastMarked != id && St[Node(lastMarked)] == Ds.Marked)
            {
                St[Node(lastMarked)] = Ds.Data;
            }
            St[Node(id)] = Ds.Marked;
            lastMarked = id;
        }

        private int AllocNode()
        {
            for (int i = 0; i < MaxNodes; i++) if (St[Node(i)] == Ds.Empty && Tg[Node(i)] == 0) return i;
            return -1;
        }

        private int KeyCount(int id) { return Tg[Node(id)]; }
        private int KeyAt(int id, int i) { return At[i][Node(id)]; }
        private void SetKey(int id, int i, int value) { At[i][Node(id)] = value; }
        private int ChildAt(int id, int c) { return At[c][Table(id)]; }
        private void SetChild(int id, int c, int value) { At[c][Table(id)] = value; }
        private bool IsLeaf(int id) { return At[3][Node(id)] == 1; }

        public override bool Step()
        {
            int total = InLen();
            switch (phase)
            {
                case 0:
                    if (count >= total)
                    {
                        if (OptionA >= 1 && !deleted) { phase = 2; cur = -1; return true; }
                        return false;
                    }
                    if (KeyCount(root) >= MaxKeys) { SplitNode(root); return true; }
                    cur = root;
                    phase = 1;
                    return true;
                case 1:
                    if (cur < 0) { phase = 0; return true; }
                    if (IsLeaf(cur)) { InsertIntoLeaf(cur, InAt(count)); count++; phase = 0; return true; }
                    {
                        int key = InAt(count);
                        int c = 0;
                        while (c < KeyCount(cur) && key > KeyAt(cur, c)) { c++; compares++; }
                        compares++;
                        int child = ChildAt(cur, c);
                        if (child < 0) { phase = 0; return true; }
                        if (KeyCount(child) >= MaxKeys) { SplitNode(child); return true; }
                        St[Node(cur)] = Ds.Done;
                        cur = child;
                        St[Node(cur)] = Ds.Active;
                        currentIndex = Node(cur);
                        return true;
                    }
                default:
                    return StepDelete();
            }
        }

        private void InsertIntoLeaf(int id, int key)
        {
            int k = KeyCount(id);
            int at = 0;
            while (at < k && KeyAt(id, at) < key) at++;
            for (int i = k; i > at; i--) SetKey(id, i, KeyAt(id, i - 1));
            SetKey(id, at, key);
            Tg[Node(id)] = (byte)(k + 1);
            Mark(id);
            currentIndex = Node(id);
        }

        /// <summary>
        /// 分裂一个满节点：中位键上移，右半另起一个节点。
        /// 约定：Attr4 一律存**父节点的元素下标**（＝节点号 × 2），
        /// 与树、并查集那些场景一致，画布按它数层数才不会走岔。
        /// </summary>
        private void SplitNode(int id)
        {
            int p = At[4][Node(id)] < 0 ? -1 : At[4][Node(id)] / 2;
            int median = KeyAt(id, 1);
            int m = AllocNode();
            Mark(m);
            Tg[Node(m)] = 0;
            SetKey(m, 0, KeyAt(id, 2));
            Tg[Node(m)] = 1;
            At[3][Node(m)] = At[3][Node(id)];
            if (!IsLeaf(id))
            {
                SetChild(m, 0, ChildAt(id, 2));
                SetChild(m, 1, ChildAt(id, 3));
                SetChild(id, 2, -1);
                SetChild(id, 3, -1);
                for (int c = 0; c < 2; c++)
                {
                    int kid = ChildAt(m, c);
                    if (kid >= 0) { At[4][Node(kid)] = Node(m); At[5][Node(kid)] = c; }
                }
            }
            SetKey(id, 2, 0);
            Tg[Node(id)] = 1;
            if (p < 0)
            {
                int nr = AllocNode();
                SetKey(nr, 0, median);
                Tg[Node(nr)] = 1;
                At[3][Node(nr)] = 0;
                SetChild(nr, 0, id);
                SetChild(nr, 1, m);
                At[4][Node(id)] = Node(nr); At[5][Node(id)] = 0;
                At[4][Node(m)] = Node(nr); At[5][Node(m)] = 1;
                root = nr;
                Mark(nr);
                focusIndex = Node(nr);
            }
            else
            {
                int at = At[5][Node(id)];
                int k = KeyCount(p);
                for (int i = k; i > at; i--) SetKey(p, i, KeyAt(p, i - 1));
                SetKey(p, at, median);
                for (int i = k; i >= at + 1; i--) SetChild(p, i + 1, ChildAt(p, i));
                SetChild(p, at + 1, m);
                Tg[Node(p)] = (byte)(k + 1);
                At[4][Node(m)] = Node(p); At[5][Node(m)] = at + 1;
                St[Node(p)] = Ds.Active;
            }
            splits++;
            currentIndex = Node(id);
        }

        // ── 删除：只删最大的那个键，它一定在最右边的叶子里，
        //    因此下降时一路走最右的孩子，路上碰到「将空」的节点就借或合并

        private bool StepDelete()
        {
            if (cur < 0)
            {
                targetKey = MaxKey(root);
                if (targetKey < 0) return false;
                cur = root;
                return true;
            }
            if (IsLeaf(cur))
            {
                RemoveKey(cur, targetKey);
                deletes++;
                // 删到发生一次合并就停：合并是这一段最值得看的一步
                deleted = deletes >= 3 || merges > 0;
                cur = -1;
                if (deleted) return false;
                return true;
            }
            int c = KeyCount(cur);
            int child = ChildAt(cur, c);
            if (child < 0) return false;
            if (KeyCount(child) <= MinKeys) { FixChild(cur, c); return true; }
            St[Node(cur)] = Ds.Done;
            cur = child;
            St[Node(cur)] = Ds.Active;
            currentIndex = Node(cur);
            return true;
        }

        private int MaxKey(int id)
        {
            if (id < 0) return -1;
            int k = KeyCount(id);
            if (k <= 0) return -1;
            if (IsLeaf(id)) return KeyAt(id, k - 1);
            return MaxKey(ChildAt(id, k));
        }

        private void RemoveKey(int id, int key)
        {
            int k = KeyCount(id);
            int at = -1;
            for (int i = 0; i < k; i++) if (KeyAt(id, i) == key) at = i;
            if (at < 0) return;
            for (int i = at; i < k - 1; i++) SetKey(id, i, KeyAt(id, i + 1));
            SetKey(id, k - 1, 0);
            Tg[Node(id)] = (byte)(k - 1);
            Mark(id);
            currentIndex = Node(id);
        }

        /// <summary>孩子将空：先从左右兄弟借一个，借不到就与兄弟合并。</summary>
        private void FixChild(int p, int c)
        {
            int child = ChildAt(p, c);
            // 左兄弟还有富余 → 借
            if (c > 0)
            {
                int left = ChildAt(p, c - 1);
                if (KeyCount(left) > MinKeys)
                {
                    int k = KeyCount(child);
                    for (int i = k; i > 0; i--) SetKey(child, i, KeyAt(child, i - 1));
                    SetKey(child, 0, KeyAt(p, c - 1));
                    Tg[Node(child)] = (byte)(k + 1);
                    SetKey(p, c - 1, KeyAt(left, KeyCount(left) - 1));
                    SetKey(left, KeyCount(left) - 1, 0);
                    Tg[Node(left)] = (byte)(KeyCount(left) - 1);
                    if (!IsLeaf(left))
                    {
                        int kid = ChildAt(left, KeyCount(left) + 1);
                        for (int i = KeyCount(child); i > 0; i--) SetChild(child, i, ChildAt(child, i - 1));
                        SetChild(child, 0, kid);
                        SetChild(left, KeyCount(left) + 1, -1);
                        if (kid >= 0) At[4][Node(kid)] = Node(child);
                    }
                    borrows++;
                    St[Node(left)] = Ds.Marked;
                    St[Node(child)] = Ds.Marked;
                    currentIndex = Node(child);
                    return;
                }
            }
            // 右兄弟还有富余 → 借
            if (c < KeyCount(p))
            {
                int right = ChildAt(p, c + 1);
                if (KeyCount(right) > MinKeys)
                {
                    int k = KeyCount(child);
                    SetKey(child, k, KeyAt(p, c));
                    Tg[Node(child)] = (byte)(k + 1);
                    SetKey(p, c, KeyAt(right, 0));
                    for (int i = 0; i < KeyCount(right) - 1; i++) SetKey(right, i, KeyAt(right, i + 1));
                    SetKey(right, KeyCount(right) - 1, 0);
                    Tg[Node(right)] = (byte)(KeyCount(right) - 1);
                    if (!IsLeaf(right))
                    {
                        int kid = ChildAt(right, 0);
                        SetChild(child, KeyCount(child), kid);
                        for (int i = 0; i < KeyCount(right) + 1; i++) SetChild(right, i, ChildAt(right, i + 1));
                        SetChild(right, KeyCount(right) + 1, -1);
                        if (kid >= 0) At[4][Node(kid)] = Node(child);
                    }
                    borrows++;
                    St[Node(right)] = Ds.Marked;
                    St[Node(child)] = Ds.Marked;
                    currentIndex = Node(child);
                    return;
                }
            }
            // 借不到 → 合并（并到左兄弟上）
            if (c > 0) Merge(p, c - 1);
            else Merge(p, c);
        }

        /// <summary>把 p 的第 c 个孩子并进第 c+1 个孩子：父节点的分隔键沉下来。</summary>
        private void Merge(int p, int c)
        {
            int left = ChildAt(p, c);
            int right = ChildAt(p, c + 1);
            int k = KeyCount(left);
            SetKey(left, k, KeyAt(p, c));
            Tg[Node(left)] = (byte)(k + 1);
            for (int i = 0; i < KeyCount(right); i++)
            {
                SetKey(left, k + 1 + i, KeyAt(right, i));
            }
            if (!IsLeaf(right))
            {
                for (int i = 0; i <= KeyCount(right); i++)
                {
                    int kid = ChildAt(right, i);
                    SetChild(left, k + 1 + i, kid);
                    if (kid >= 0) { At[4][Node(kid)] = Node(left); At[5][Node(kid)] = k + 1 + i; }
                }
            }
            Tg[Node(left)] = (byte)(k + 1 + KeyCount(right));
            // 父节点里删掉分隔键与右孩子
            for (int i = c; i < KeyCount(p) - 1; i++) SetKey(p, i, KeyAt(p, i + 1));
            SetKey(p, KeyCount(p) - 1, 0);
            for (int i = c + 1; i < KeyCount(p); i++) SetChild(p, i, ChildAt(p, i + 1));
            SetChild(p, KeyCount(p), -1);
            Tg[Node(p)] = (byte)(KeyCount(p) - 1);
            // 右节点作废
            for (int i = 0; i < 6; i++) At[i][Node(right)] = -1;
            for (int i = 0; i < 4; i++) At[i][Table(right)] = -1;
            Tg[Node(right)] = 0;
            St[Node(right)] = Ds.Empty;
            merges++;
            St[Node(left)] = Ds.Marked;
            St[Node(p)] = Ds.Active;
            currentIndex = Node(left);
            if (p == root && KeyCount(p) == 0)
            {
                root = left;
                At[4][Node(left)] = -1;
                St[Node(left)] = Ds.Anchor;
                focusIndex = Node(left);
            }
        }

        public override void Flush()
        {
            for (int id = 0; id < MaxNodes; id++)
            {
                if (St[Node(id)] == Ds.Empty) continue;
                if (id == root) St[Node(id)] = Ds.Anchor;
            }
            base.Flush();
        }

        public override string StatusText()
        {
            return "B 树（阶 4）：键 " + count + " 个，树高 " + Height()
                   + "，分裂 " + splits + " 次，合并 " + merges + " 次，借用 " + borrows + " 次";
        }

        public override string Title()
        {
            if (OptionA == 1) return "每个节点最多 3 个键槽；删除最大的键，一路上「将空」的节点先借后并";
            return "每个节点最多 3 个键槽；插入到满就先分裂，中位键升到父节点";
        }

        private int Height()
        {
            int depth = 0, walk = root, guard = 0;
            while (walk >= 0 && guard++ < MaxNodes)
            {
                depth++;
                if (IsLeaf(walk)) break;
                walk = ChildAt(walk, 0);
            }
            return depth;
        }

        public override void Command(int op, int key, int target)
        {
            if (op == Ds.CmdInsert) InAppend(Clamp(key));
            else if (op == Ds.CmdDelete) InRemoveLast();
            else if (op == Ds.CmdClear) InClear();
            else if (op == Ds.CmdRegenerate) BuildTarget();
            else if (op == Ds.CmdAppendRandom) AppendRandomKeys(2);
        }

        private static int Clamp(int key)
        {
            if (key < 1) return 1;
            if (key > 90) return 90;
            return key;
        }

        public override int Primary { get { return Height(); } }
        public override int Secondary { get { return splits; } }
        public override int Work { get { return compares; } }
        public override int Pending { get { return count; } }
        public override int Focus { get { return focusIndex; } }
        public override int CustomA { get { return merges; } }
        public override int CustomB { get { return borrows; } }
    }

    // ───────────────────────────────────────────────────────────── 场景 7：trie

    /// <summary>词的编码：按码点算一个单位，还是按 UTF-8 字节算一个单位。</summary>
    internal static class DsText
    {
        public static int[] Units(string text, bool byByte)
        {
            if (byByte)
            {
                byte[] bytes = System.Text.Encoding.UTF8.GetBytes(text);
                int[] units = new int[bytes.Length];
                for (int i = 0; i < bytes.Length; i++) units[i] = bytes[i];
                return units;
            }
            int[] points = new int[text.Length];
            int n = 0;
            for (int i = 0; i < text.Length; i++)
            {
                if (char.IsHighSurrogate(text[i]) && i + 1 < text.Length && char.IsLowSurrogate(text[i + 1]))
                {
                    points[n++] = char.ConvertToUtf32(text[i], text[i + 1]);
                    i++;
                }
                else points[n++] = text[i];
            }
            int[] trimmed = new int[n];
            Array.Copy(points, trimmed, n);
            return trimmed;
        }

        public static string Render(int unit, bool isByte)
        {
            if (isByte) return unit.ToString("X2", CultureInfo.InvariantCulture);
            return char.ConvertFromUtf32(unit);
        }

        /// <summary>词库。英文与中文各一组，中文那组的看点是一个字按字节要占三层。</summary>
        public static string[] Bank(int which, int seed)
        {
            string[] english = { "app", "apt", "bat", "car", "cart", "cat" };
            string[] chinese = { "中国", "中间", "中文", "中心", "人民", "人间" };
            string[] mixed = { "ab", "中国", "abc", "中文", "car", "人" };
            string[] source = which == 0 ? english : (which == 1 ? chinese : mixed);
            string[] copy = new string[source.Length];
            Array.Copy(source, copy, source.Length);
            DeterministicRng rng = new DeterministicRng(unchecked((uint)seed * 97u + 11u));
            for (int i = copy.Length - 1; i > 0; i--)
            {
                int j = rng.Next(i + 1);
                string t = copy[i]; copy[i] = copy[j]; copy[j] = t;
            }
            return copy;
        }
    }

    /// <summary>
    /// trie：**一层一个单位**（按码点是一个字一层，按字节是一个 UTF-8 字节一层），
    /// 子节点用「第一个孩子 + 下一个兄弟」表示。中文那组切换「选项一」就能看到
    /// 同一个词按字节要深三倍。
    /// </summary>
    internal sealed class TrieModel : DsModel
    {
        private const int MaxNodes = 64;

        private int nodes, phase, word, unitAt, cur, depth, created;
        private int[] units = new int[16];
        private int prefixHits, findWord = -1, findNode = -1;

        public TrieModel(VisualSnapshot s) : base(s) { }

        private bool ByByte { get { return OptionA == 1; } }

        public override void BuildTarget()
        {
            string[] bank = DsText.Bank(OptionB, Seed);
            InClear();
            int at = 0;
            int take = bank.Length <= 5 ? bank.Length : 5;
            for (int w = 0; w < take; w++)
            {
                int[] code = DsText.Units(bank[w], ByByte);
                if (at + 1 + code.Length >= DsConst.InputCount) break;
                At[0][DsConst.InputBase + at] = code.Length;
                at++;
                for (int i = 0; i < code.Length; i++) At[0][DsConst.InputBase + at + i] = code[i];
                at += code.Length;
            }
            At[0][DsConst.InputBase + at] = 0;
        }

        /// <summary>输入区里按「长度 + 各单位」编码，长度 0 表示列表结束。</summary>
        private int WordCount()
        {
            int at = 0, words = 0;
            while (at < DsConst.InputCount && At[0][DsConst.InputBase + at] > 0)
            {
                int len = At[0][DsConst.InputBase + at];
                at += 1 + len;
                words++;
            }
            return words;
        }

        private int[] WordAt(int index)
        {
            int at = 0;
            for (int w = 0; w < index; w++)
            {
                int len = At[0][DsConst.InputBase + at];
                if (len <= 0) return new int[0];
                at += 1 + len;
            }
            int n = At[0][DsConst.InputBase + at];
            if (n <= 0) return new int[0];
            int[] result = new int[n];
            for (int i = 0; i < n; i++) result[i] = At[0][DsConst.InputBase + at + 1 + i];
            return result;
        }

        private void AppendWord(int[] code)
        {
            int at = 0;
            while (at < DsConst.InputCount - 2 && At[0][DsConst.InputBase + at] > 0)
            {
                at += 1 + At[0][DsConst.InputBase + at];
            }
            if (at + 1 + code.Length >= DsConst.InputCount) return;
            At[0][DsConst.InputBase + at] = code.Length;
            for (int i = 0; i < code.Length; i++) At[0][DsConst.InputBase + at + 1 + i] = code[i];
            At[0][DsConst.InputBase + at + 1 + code.Length] = 0;
        }

        public override void ResetRun()
        {
            ClearRun();
            ClearStructKeys();
            for (int i = 0; i < MaxNodes; i++)
            {
                At[0][i] = 0; At[1][i] = -1; At[2][i] = -1; At[3][i] = -1;
                At[4][i] = -1; At[5][i] = 0; Tg[i] = 0;
            }
            At[0][0] = '*';                      // 根：一个不会出现在词里的记号
            At[4][0] = 0;
            Tg[0] = 1;                           // 根上的记号按字符画，不按十六进制字节画
            St[0] = Ds.Anchor;
            cur = 0;
            nodes = 1;
            phase = 0;
            word = 0;
            unitAt = 0;
            created = 0;
            depth = 0;
            prefixHits = 0;
            if (findWord >= 0 || findNode >= 0) Highlight();
            Flush();
        }

        public override bool Step()
        {
            int words = WordCount();
            switch (phase)
            {
                case 0:
                    if (word >= words)
                    {
                        if (findWord < 0) return false;
                        phase = 3;
                        return true;
                    }
                    units = WordAt(word);
                    if (units.Length == 0) { word++; return true; }
                    unitAt = 0;
                    cur = 0;
                    phase = 1;
                    return true;
                case 1:
                    if (unitAt >= units.Length) { phase = 2; return true; }
                    {
                        int child = FindChild(cur, units[unitAt]);
                        if (child < 0)
                        {
                            child = Alloc();
                            if (child < 0) return false;
                            At[0][child] = units[unitAt];
                            Tg[child] = ByByte ? (byte)0 : (byte)1;
                            At[1][child] = cur;
                            At[4][child] = At[4][cur] + 1;
                            At[3][child] = At[2][cur];
                            At[2][cur] = child;
                            St[child] = Ds.Marked;
                            nodes++;
                            created++;
                        }
                        else St[child] = Ds.Active;
                        cur = child;
                        currentIndex = cur;
                        depth = At[4][cur];
                        unitAt++;
                        return true;
                    }
                case 2:
                    At[5][cur] = 1;
                    St[cur] = Ds.Done;
                    word++;
                    phase = 0;
                    return true;
                default:
                    Highlight();
                    phase = 0;
                    return false;
            }
        }

        private int FindChild(int node, int unit)
        {
            int guard = 0;
            for (int c = At[2][node]; c >= 0 && c < MaxNodes && guard++ < MaxNodes; c = At[3][c])
            {
                if (At[0][c] == unit) return c;
            }
            return -1;
        }

        private int Alloc()
        {
            for (int i = 1; i < MaxNodes; i++) if (At[0][i] == 0) return i;
            return -1;
        }

        /// <summary>前缀高亮：把一个键（或一个节点的路径）上的节点标成路径色。</summary>
        private void Highlight()
        {
            for (int i = 0; i < MaxNodes; i++)
            {
                if (At[0][i] == 0) continue;
                if (St[i] != Ds.Anchor) St[i] = Ds.Data;
                Mk[i] = 0;
            }
            int node = -1;
            if (findNode >= 0) node = findNode;
            else if (findWord >= 0)
            {
                int[] code = WordAt(findWord);
                int walk = 0;
                for (int i = 0; i < code.Length; i++)
                {
                    walk = FindChild(walk, code[i]);
                    if (walk < 0) break;
                }
                node = walk;
            }
            if (node < 0) { prefixHits = -1; return; }
            int at = node;
            int guard = 0;
            while (at >= 0 && guard++ < MaxNodes)
            {
                St[at] = at == node ? Ds.Result : Ds.Path;
                at = At[1][at];
            }
            // 命中这个前缀的键数：子树里词尾标记的个数
            prefixHits = 0;
            CountTerminals(node, ref prefixHits);
        }

        private void CountTerminals(int node, ref int total)
        {
            if (node < 0) return;
            if (At[5][node] == 1) total++;
            int guard = 0;
            for (int c = At[2][node]; c >= 0 && guard++ < MaxNodes; c = At[3][c]) CountTerminals(c, ref total);
        }

        public override void Command(int op, int key, int target)
        {
            string[] bank = DsText.Bank(OptionB, Seed);
            if (op == Ds.CmdInsert)
            {
                int words = WordCount();
                if (words < bank.Length) AppendWord(DsText.Units(bank[words], ByByte));
                findWord = -1;
                findNode = -1;
            }
            else if (op == Ds.CmdDelete) RemoveLastWord();
            else if (op == Ds.CmdClear) { InClear(); findWord = -1; findNode = -1; }
            else if (op == Ds.CmdRegenerate) { BuildTarget(); findWord = -1; findNode = -1; }
            else if (op == Ds.CmdFind) { findWord = Math.Max(0, Math.Min(bank.Length - 1, key - 1)); findNode = -1; }
            else if (op == Ds.CmdSelect) { findNode = target; findWord = -1; }
        }

        private void RemoveLastWord()
        {
            int at = 0, last = 0;
            while (at < DsConst.InputCount && At[0][DsConst.InputBase + at] > 0)
            {
                last = at;
                at += 1 + At[0][DsConst.InputBase + at];
            }
            if (at == 0) return;
            At[0][DsConst.InputBase + last] = 0;
        }

        public override string StatusText()
        {
            return "trie（" + (ByByte ? "按字节" : "按码点") + "）：节点 " + nodes
                   + " 个，最大深度 " + MaxDepth() + "，词 " + WordCount() + " 个"
                   + (prefixHits >= 0 ? "，前缀命中 " + prefixHits + " 个键" : "");
        }

        public override string Title()
        {
            return "一层一个" + (ByByte ? "UTF-8 字节（一个汉字三层）" : "码点（一个汉字一层）")
                   + "；把「选项一」切过去，同一个词的层数立刻差三倍";
        }

        private int MaxDepth()
        {
            int best = 0;
            for (int i = 0; i < MaxNodes; i++) if (At[0][i] != 0 && At[4][i] > best) best = At[4][i];
            return best;
        }

        public override int Primary { get { return nodes; } }
        public override int Secondary { get { return MaxDepth(); } }
        public override int Work { get { return created; } }
        public override int Pending { get { return WordCount(); } }
        public override int Focus { get { return 0; } }
        public override int CustomA { get { return prefixHits; } }
        public override int CustomB { get { return depth; } }
    }

    // ───────────────────────────────────────────────────────────── 场景 8：图

    /// <summary>
    /// 图：八个顶点，三种存法（邻接矩阵／邻接表／边表），两种遍历（BFS／DFS）。
    /// **每一步只检查一条边**，当前边在图上、在矩阵格里、在邻接表项上同时高亮。
    /// 邻接矩阵是双向写的，无向图的两格一起亮。
    /// </summary>
    internal sealed class GraphModel : DsModel
    {
        private const int V = 8;
        private const int MatrixBase = 8;
        private const int ListBase = 72;

        private int phase, cur, curEntry, orderCount, edgesChecked, pops, start;
        private int hotU = -1, hotV = -1;
        private int[] visitOrder = new int[V];
        private int[] cont = new int[32];
        private int contHead, contCount;

        public GraphModel(VisualSnapshot s) : base(s) { }

        private bool Dfs { get { return OptionB == 1; } }

        public override void BuildTarget()
        {
            InClear();
            DeterministicRng rng = new DeterministicRng(unchecked((uint)Seed * 2654435761u + 811u));
            bool[,] used = new bool[V, V];
            int at = 0;
            for (int v = 1; v < V; v++)                      // 先连一棵生成树，保证连通
            {
                int u = rng.Next(v);
                used[u, v] = true; used[v, u] = true;
                At[0][DsConst.InputBase + at] = u + 1;
                At[0][DsConst.InputBase + at + 1] = v + 1;
                at += 2;
            }
            int extra = 0, guard = 0;
            while (extra < 5 && guard++ < 400)               // 再加五条，出来是一张疏密适中的图
            {
                int u = rng.Next(V), v = rng.Next(V);
                if (u == v || used[u, v]) continue;
                used[u, v] = true; used[v, u] = true;
                At[0][DsConst.InputBase + at] = u + 1;
                At[0][DsConst.InputBase + at + 1] = v + 1;
                at += 2;
                extra++;
            }
            At[0][DsConst.InputBase + at] = 0;
        }

        private int EdgeCount()
        {
            int n = 0;
            while (n < 60 && At[0][DsConst.InputBase + n * 2] > 0) n++;
            return n;
        }

        public override void ResetRun()
        {
            ClearRun();
            ClearStructKeys();
            int edges = EdgeCount();
            for (int u = 0; u < V; u++)
            {
                At[0][u] = 'A' + u;
                At[1][u] = 0;
                At[3][u] = 0;
                At[4][u] = -1;
                St[u] = Ds.Empty;
                for (int v = 0; v < V; v++)
                {
                    At[0][MatrixBase + u * V + v] = 0;
                    St[MatrixBase + u * V + v] = Ds.Empty;
                }
                for (int k = 0; k < 4; k++)
                {
                    At[0][ListBase + u * 4 + k] = 0;
                    At[1][ListBase + u * 4 + k] = -1;
                    At[2][ListBase + u * 4 + k] = -1;
                    St[ListBase + u * 4 + k] = Ds.Empty;
                }
            }
            for (int e = 0; e < edges; e++)
            {
                int u = At[0][DsConst.InputBase + e * 2] - 1;
                int v = At[0][DsConst.InputBase + e * 2 + 1] - 1;
                if (u < 0 || v < 0 || u >= V || v >= V) continue;
                At[0][MatrixBase + u * V + v] = 1;
                At[0][MatrixBase + v * V + u] = 1;
            }
            // 邻接表：每个顶点的邻居按编号排好，链序与槽位一致，画出来的连线才顺
            for (int u = 0; u < V; u++)
            {
                int k = 0;
                for (int v = 0; v < V && k < 4; v++)
                {
                    if (At[0][MatrixBase + u * V + v] == 0) continue;
                    int slot = ListBase + u * 4 + k;
                    At[0][slot] = v;
                    At[2][slot] = u;
                    At[1][slot] = k + 1 < 4 ? -2 : -1;    // 先占位，下一轮补链
                    k++;
                }
                for (int j = 0; j < k; j++)
                {
                    int slot = ListBase + u * 4 + j;
                    At[1][slot] = j + 1 < k ? ListBase + u * 4 + j + 1 : -1;
                }
                At[1][u] = k;
                At[3][u] = 0;
            }
            for (int u = 0; u < V; u++)
            {
                for (int v = 0; v < V; v++)
                {
                    if (At[0][MatrixBase + u * V + v] == 1) St[MatrixBase + u * V + v] = Ds.Data;
                }
                for (int k = 0; k < 4; k++)
                {
                    int slot = ListBase + u * 4 + k;
                    if (At[2][slot] == u) St[slot] = Ds.Data;
                }
            }
            phase = 0;
            cur = -1;
            curEntry = -1;
            orderCount = 0;
            edgesChecked = 0;
            pops = 0;
            hotU = -1;
            hotV = -1;
            contHead = 0;
            contCount = 0;
            start = 0;
            for (int i = 0; i < V; i++) visitOrder[i] = 0;
            Flush();
        }

        public override bool Step()
        {
            switch (phase)
            {
                case 0:
                    visitOrder[start] = 1;
                    orderCount = 1;
                    At[3][start] = 1;
                    At[4][start] = 0;
                    St[start] = Ds.Anchor;
                    Push(start);
                    currentIndex = start;
                    phase = 1;
                    return true;
                case 1:
                    if (contCount == 0)
                    {
                        phase = 3;
                        return true;
                    }
                    cur = Take();
                    pops++;
                    curEntry = At[1][cur] > 0 ? ListBase + cur * 4 : -1;
                    St[cur] = Ds.Active;
                    currentIndex = cur;
                    phase = 2;
                    return true;
                case 2:
                    if (curEntry < 0)
                    {
                        St[cur] = cur == start ? Ds.Anchor : Ds.Done;   // 起点一直保持锚点色
                        phase = 1;
                        return true;
                    }
                    {
                        int slot = curEntry;
                        int w = At[0][slot];
                        curEntry = At[1][slot];
                        edgesChecked++;
                        hotU = cur;
                        hotV = w;
                        PaintEdge(cur, w, Ds.Active);
                        if (visitOrder[w] == 0)
                        {
                            orderCount++;
                            visitOrder[w] = orderCount;
                            At[3][w] = orderCount;
                            At[4][w] = At[4][cur] + 1;
                            St[w] = Ds.Pending;
                            Push(w);
                            PaintEdge(cur, w, Ds.Done);
                        }
                        else PaintEdge(cur, w, Ds.Ref);
                        currentIndex = w;
                        return true;
                    }
                default:
                    for (int v = 0; v < V; v++) if (visitOrder[v] > 0 && St[v] != Ds.Anchor) St[v] = Ds.Done;
                    hotU = -1;
                    hotV = -1;
                    return false;
            }
        }

        private void Push(int v)
        {
            if (contCount >= 32) return;
            if (Dfs) cont[contCount++] = v;                       // 栈：后进先出
            else cont[(contHead + contCount++) % 32] = v;         // 队列：先进先出
        }

        private int Take()
        {
            if (Dfs) return cont[--contCount];
            int v = cont[contHead % 32];
            contHead++;
            contCount--;
            return v;
        }

        /// <summary>一条边在三处同时上色：两个顶点之间的连线（靠 CustomA/B）与矩阵两格。</summary>
        private void PaintEdge(int u, int v, byte state)
        {
            if (u < 0 || v < 0 || u >= V || v >= V) return;
            if (state == Ds.Data) return;
            St[MatrixBase + u * V + v] = state;
            St[MatrixBase + v * V + u] = state;
            for (int k = 0; k < 4; k++)
            {
                int slot = ListBase + u * 4 + k;
                if (At[2][slot] == u && At[0][slot] == v) St[slot] = state;
                int back = ListBase + v * 4 + k;
                if (At[2][back] == v && At[0][back] == u) St[back] = state;
            }
        }

        public override string StatusText()
        {
            return "图（" + (OptionB == 1 ? "DFS" : "BFS") + "，"
                   + (OptionA == 0 ? "邻接矩阵" : (OptionA == 1 ? "邻接表" : "边表")) + "）：顶点 "
                   + orderCount + "／8，检查边 " + edgesChecked + " 条，出队 " + pops + " 次";
        }

        public override string Title()
        {
            return "每一步只走一条边：图上那条边、矩阵里两格、邻接表里那一项，同时换色";
        }

        public override void Command(int op, int key, int target)
        {
            if (op == Ds.CmdInsert && target >= 0 && target < V)
            {
                int u = Math.Max(0, Math.Min(V - 1, key - 1));
                AddEdge(u, target);
            }
            else if (op == Ds.CmdDelete && target >= 0 && target < V)
            {
                int u = Math.Max(0, Math.Min(V - 1, key - 1));
                RemoveEdge(u, target);
            }
            else if (op == Ds.CmdClear) InClear();
            else if (op == Ds.CmdRegenerate) BuildTarget();
            else if (op == Ds.CmdSelect && target >= 0 && target < V) start = target;
        }

        private void AddEdge(int u, int v)
        {
            if (u == v) return;
            int n = EdgeCount();
            if (n >= 24) return;
            for (int e = 0; e < n; e++)
            {
                int a = At[0][DsConst.InputBase + e * 2] - 1;
                int b = At[0][DsConst.InputBase + e * 2 + 1] - 1;
                if ((a == u && b == v) || (a == v && b == u)) return;
            }
            At[0][DsConst.InputBase + n * 2] = u + 1;
            At[0][DsConst.InputBase + n * 2 + 1] = v + 1;
            At[0][DsConst.InputBase + n * 2 + 2] = 0;
        }

        private void RemoveEdge(int u, int v)
        {
            int n = EdgeCount();
            for (int e = 0; e < n; e++)
            {
                int a = At[0][DsConst.InputBase + e * 2] - 1;
                int b = At[0][DsConst.InputBase + e * 2 + 1] - 1;
                if (!((a == u && b == v) || (a == v && b == u))) continue;
                for (int k = e; k < n - 1; k++)
                {
                    At[0][DsConst.InputBase + k * 2] = At[0][DsConst.InputBase + (k + 1) * 2];
                    At[0][DsConst.InputBase + k * 2 + 1] = At[0][DsConst.InputBase + (k + 1) * 2 + 1];
                }
                At[0][DsConst.InputBase + (n - 1) * 2] = 0;
                At[0][DsConst.InputBase + (n - 1) * 2 + 1] = 0;
                return;
            }
        }

        public override int Primary { get { return orderCount; } }
        public override int Secondary { get { return EdgeCount(); } }
        public override int Work { get { return edgesChecked; } }
        public override int Pending { get { return contCount; } }
        public override int Focus { get { return start; } }
        public override int CustomA { get { return hotU; } }
        public override int CustomB { get { return hotV; } }
    }

    // ───────────────────────────────────────────────────────────── 场景 9：并查集

    /// <summary>
    /// 并查集：父指针画成连线，根画成锚点色。四种组合（什么都不加／只按秩／只压缩／都加）
    /// 摆在一起对照；最后做一次 find，先数压缩前走了几跳，压缩后再数一次。
    /// </summary>
    internal sealed class DsuModel : DsModel
    {
        private int n, phase, pairIndex, pairs, walk, hops, rootA, rootB;
        private int unions, sets, findSteps, findBefore, findAfter, findTarget, compressAt;
        private int[] parent = new int[16];
        private int[] rank = new int[16];
        private int[] path = new int[16];
        private int pathLen;

        public DsuModel(VisualSnapshot s) : base(s) { }

        private bool ByRank { get { return OptionA == 1 || OptionA == 3; } }
        private bool Compress { get { return OptionA == 2 || OptionA == 3; } }

        public override void BuildTarget()
        {
            n = OptionB == 0 ? 16 : 12;
            InClear();
            int[] order = new int[n];
            for (int i = 0; i < n - 1; i++) order[i] = i;
            DeterministicRng rng = new DeterministicRng(unchecked((uint)Seed * 2654435761u + 907u));
            for (int i = n - 2; i > 0; i--)
            {
                int j = rng.Next(i + 1);
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }
            for (int i = 0; i < n - 1; i++)         // 相邻两个合并
            {
                // 故意把「小的接到大的上」这个顺序留着：不加按秩合并时就会连长链，
                // 加上按秩或路径压缩以后链立刻变短，四种组合的差别才看得出来
                int a = order[i];
                InAppend(a + 2);
                InAppend(a + 1);
            }
        }

        public override void ResetRun()
        {
            ClearRun();
            ClearStructKeys();
            for (int i = 0; i < 16; i++)
            {
                parent[i] = i;
                rank[i] = 0;
                At[0][i] = i;
                At[1][i] = i;
                At[2][i] = 0;
                At[3][i] = 1;
                At[4][i] = i;
                St[i] = i < n ? Ds.Anchor : Ds.Empty;
            }
            pairs = InLen() / 2;
            pairIndex = 0;
            phase = 0;
            walk = -1;
            hops = 0;
            unions = 0;
            sets = n;
            findSteps = 0;
            findBefore = -1;
            findAfter = -1;
            pathLen = 0;
            compressAt = 0;
            findTarget = n > 0 ? n - 1 : 0;
            currentIndex = -1;
            Flush();
        }

        public override bool Step()
        {
            switch (phase)
            {
                case 0:
                    if (pairIndex >= pairs)
                    {
                        phase = 4;
                        findTarget = DeepestNode();
                        walk = findTarget;
                        hops = 0;
                        pathLen = 0;
                        path[pathLen++] = findTarget;      // 被查的节点自己也要进压缩名单
                        return true;
                    }
                    walk = InAt(pairIndex * 2) - 1;
                    hops = 0;
                    currentIndex = walk;
                    St[walk] = Ds.Active;
                    phase = 1;
                    return true;
                case 1:
                    if (parent[walk] == walk) { rootA = walk; walk = InAt(pairIndex * 2 + 1) - 1; hops = 0; currentIndex = walk; St[walk] = Ds.Active; phase = 2; return true; }
                    findSteps++;
                    walk = parent[walk];
                    hops++;
                    currentIndex = walk;
                    St[walk] = Ds.Ref;
                    return true;
                case 2:
                    if (parent[walk] == walk) { rootB = walk; phase = 3; return true; }
                    findSteps++;
                    walk = parent[walk];
                    hops++;
                    currentIndex = walk;
                    St[walk] = Ds.Ref;
                    return true;
                case 3:
                    Link(rootA, rootB);
                    pairIndex++;
                    phase = 0;
                    return true;
                case 4:
                    if (parent[walk] == walk) { findBefore = hops; phase = 5; compressAt = 0; return true; }
                    findSteps++;
                    hops++;
                    walk = parent[walk];
                    path[pathLen++] = walk;
                    currentIndex = walk;
                    St[walk] = Ds.Path;
                    return true;
                case 5:
                    if (!Compress || compressAt >= pathLen) { phase = 6; walk = findTarget; hops = 0; return true; }
                    {
                        int node = path[compressAt];
                        if (parent[node] != walk) { parent[node] = walk; St[node] = Ds.Cut; }
                        At[1][node] = parent[node];
                        compressAt++;
                        currentIndex = node;
                        return true;
                    }
                default:
                    if (parent[walk] == walk) { findAfter = hops; Publish(); return false; }
                    findSteps++;
                    hops++;
                    walk = parent[walk];
                    currentIndex = walk;
                    St[walk] = Ds.Cut;
                    return true;
            }
        }

        /// <summary>最深的那个节点：最后那次 find 从它出发，压缩前后的差才看得出来。</summary>
        private int DeepestNode()
        {
            int best = 0, bestHops = -1;
            for (int i = 0; i < n; i++)
            {
                int hops = 0, walk = i, guard = 0;
                while (parent[walk] != walk && guard++ < 64) { walk = parent[walk]; hops++; }
                if (hops > bestHops) { bestHops = hops; best = i; }
            }
            return best;
        }

        private void Link(int a, int b)        {
            if (a == b) return;
            if (ByRank)
            {
                if (rank[a] < rank[b]) { int t = a; a = b; b = t; }
                parent[b] = a;
                if (rank[a] == rank[b]) rank[a]++;
                At[1][b] = a;
                St[b] = Ds.Marked;
            }
            else
            {
                parent[b] = a;
                At[1][b] = a;
                St[b] = Ds.Marked;
            }
            unions++;
            sets--;
            currentIndex = b;
        }

        private void Publish()
        {
            for (int i = 0; i < 16; i++)
            {
                if (i >= n) continue;
                At[1][i] = parent[i];
                At[2][i] = rank[i];
                At[3][i] = parent[i] == i ? 1 : 0;
                if (parent[i] == i) St[i] = Ds.Anchor;
                else if (St[i] == Ds.Anchor) St[i] = Ds.Data;
            }
        }

        public override void Flush()
        {
            Publish();
            base.Flush();
        }

        public override string StatusText()
        {
            string mode = OptionA == 0 ? "什么都不加" : (OptionA == 1 ? "只按秩合并"
                          : (OptionA == 2 ? "只路径压缩" : "按秩 + 路径压缩"));
            string tail = findBefore >= 0
                ? "，这次 find 压缩前 " + findBefore + " 跳、压缩后 " + findAfter + " 跳" : "";
            return "并查集（" + mode + "）：集合 " + sets + " 个，合并 " + unions
                   + " 次，find 共走 " + findSteps + " 步" + tail;
        }

        public override string Title()
        {
            return "父指针就是连线：箭头指向自己的是根。切换「选项一」看四种组合，"
                   + "最后那次 find 会把路径压平";
        }

        public override void Command(int op, int key, int target)
        {
            if (op == Ds.CmdInsert)
            {
                InAppend(Clamp(key, n));
                InAppend(Clamp(key + 1, n));
            }
            else if (op == Ds.CmdDelete)
            {
                int len = InLen();
                if (len >= 2) { InRemoveAt(len - 1); InRemoveAt(len - 2); }
            }
            else if (op == Ds.CmdClear) InClear();
            else if (op == Ds.CmdRegenerate) BuildTarget();
            else if (op == Ds.CmdAppendRandom) AppendRandomKeys(2);
        }

        private static int Clamp(int value, int n)
        {
            if (value < 1) return 1;
            if (value > n) return n;
            return value;
        }

        public override int Primary { get { return sets; } }
        public override int Secondary { get { return findBefore; } }
        public override int Work { get { return findSteps; } }
        public override int Pending { get { return Math.Max(0, pairs - pairIndex); } }
        public override int CustomA { get { return findBefore; } }
        public override int CustomB { get { return findAfter; } }
    }

    // ───────────────────────────────────────────────────────────── 场景 10：排序

    /// <summary>
    /// 排序过程：比较、交换、已就位三种状态分开上色，四种算法共用一个状态机骨架。
    /// 每一步只做一件事——比一次，或者换一次。
    /// </summary>
    internal sealed class SortModel : DsModel
    {
        public const int Size = 40;

        private int phase, i, j, minAt, compares, writes, settled, swapPending;
        private int pivot, lo, hi, partitionAt;
        private int[] val = new int[Size];
        private int[] stackLo = new int[64];
        private int[] stackHi = new int[64];
        private int stackTop;

        public SortModel(VisualSnapshot s) : base(s) { }

        public override void BuildTarget()
        {
            int[] values = new int[Size];
            DeterministicRng rng = new DeterministicRng(unchecked((uint)Seed * 2654435761u + 1009u));
            for (int k = 0; k < Size; k++) values[k] = 1 + rng.Next(DsConst.MaxKey);
            if (OptionB == 1)
            {
                // 近乎有序：排好之后再换乱三对相邻的
                Array.Sort(values);
                for (int k = 0; k < 3; k++)
                {
                    int at = rng.Next(Size - 1);
                    int t = values[at]; values[at] = values[at + 1]; values[at + 1] = t;
                }
            }
            else if (OptionB == 2)
            {
                Array.Sort(values);
                Array.Reverse(values);
            }
            InClear();
            for (int k = 0; k < Size; k++) InAppend(values[k]);
        }

        public override void ResetRun()
        {
            ClearRun();
            ClearStructKeys();
            int n = InLen();
            if (n > Size) n = Size;
            for (int k = 0; k < n; k++)
            {
                val[k] = InAt(k);
                At[0][k] = val[k];
                St[k] = Ds.Data;
            }
            for (int k = n; k < Size; k++) { At[0][k] = 0; St[k] = Ds.Empty; }
            phase = 1;
            i = 0;
            j = 1;
            minAt = 0;
            compares = 0;
            writes = 0;
            settled = 0;
            swapPending = 0;
            stackTop = 0;
            partitionAt = 0;
            currentIndex = -1;
            if (OptionA == 3 && n > 1) { stackLo[stackTop] = 0; stackHi[stackTop] = n - 1; stackTop++; }
            Flush();
        }

        public override bool Step()
        {
            int n = InLen();
            if (n > Size) n = Size;
            if (phase != 1) return false;
            switch (OptionA)
            {
                case 0: return StepBubble(n);
                case 1: return StepInsert(n);
                case 2: return StepSelect(n);
                default: return StepQuick(n);
            }
        }

        private bool StepBubble(int n)
        {
            if (i >= n - 1) { Finish(n); return false; }
            if (j >= n - 1 - i) { St[n - 1 - i] = Ds.Done; settled++; i++; j = 0; return true; }
            if (swapPending != 0)
            {
                Swap(j, j + 1);
                writes++;
                swapPending = 0;
                j++;
                return true;
            }
            compares++;
            St[j] = Ds.Active;
            St[j + 1] = Ds.Ref;
            currentIndex = j;
            if (val[j] > val[j + 1]) { swapPending = 1; return true; }
            j++;
            return true;
        }

        private bool StepInsert(int n)
        {
            if (i >= n) { Finish(n); return false; }
            if (j > 0 && val[j - 1] > val[j])
            {
                if (swapPending != 0)
                {
                    Swap(j - 1, j);
                    writes++;
                    swapPending = 0;
                    j--;
                    return true;
                }
                compares++;
                St[j - 1] = Ds.Ref;
                St[j] = Ds.Active;
                currentIndex = j;
                swapPending = 1;
                return true;
            }
            if (j > 0) compares++;
            for (int k = 0; k <= i && k < n; k++) St[k] = Ds.Done;
            settled = i + 1;
            i++;
            j = i;
            return true;
        }

        private bool StepSelect(int n)
        {
            if (i >= n - 1) { Finish(n); return false; }
            if (j < n)
            {
                compares++;
                if (val[j] < val[minAt]) minAt = j;
                St[j] = Ds.Ref;
                St[minAt] = Ds.Active;
                currentIndex = minAt;
                j++;
                return true;
            }
            Swap(i, minAt);
            writes++;
            St[i] = Ds.Done;
            settled++;
            i++;
            j = i + 1;
            minAt = i;
            return true;
        }

        private bool StepQuick(int n)
        {
            if (stackTop == 0 && partitionAt == 0) { Finish(n); return false; }
            if (partitionAt == 0)
            {
                int at = stackTop - 1;
                lo = stackLo[at];
                hi = stackHi[at];
                stackTop--;
                if (lo >= hi)
                {
                    if (lo == hi) { St[lo] = Ds.Done; settled++; }
                    return true;
                }
                pivot = val[hi];
                i = lo;
                j = lo;
                partitionAt = 1;
                St[hi] = Ds.Pending;
                currentIndex = hi;
                return true;
            }
            if (j < hi)
            {
                compares++;
                St[j] = Ds.Ref;
                if (val[j] < pivot)
                {
                    if (i != j) { Swap(i, j); writes++; }
                    i++;
                }
                St[i] = Ds.Active;
                currentIndex = j;
                j++;
                return true;
            }
            if (i != hi) { Swap(i, hi); writes++; }
            St[i] = Ds.Done;
            settled++;
            if (i + 1 < hi) { stackLo[stackTop] = i + 1; stackHi[stackTop] = hi; stackTop++; }
            if (lo < i - 1) { stackLo[stackTop] = lo; stackHi[stackTop] = i - 1; stackTop++; }
            partitionAt = 0;
            return true;
        }

        private void Finish(int n)
        {
            for (int k = 0; k < n; k++) St[k] = Ds.Done;
            settled = n;
            currentIndex = -1;
            phase = 2;
        }

        private void Swap(int a, int b)
        {
            int t = val[a]; val[a] = val[b]; val[b] = t;
            At[0][a] = val[a];
            At[0][b] = val[b];
            St[a] = Ds.Marked;
            St[b] = Ds.Marked;
        }

        public override string StatusText()
        {
            string name = OptionA == 0 ? "冒泡" : (OptionA == 1 ? "插入" : (OptionA == 2 ? "选择" : "快速"));
            return name + "排序：比较 " + compares + " 次，交换／写入 " + writes
                   + " 次，已就位 " + settled + "／" + InLen();
        }

        public override string Title()
        {
            return "比较中的两个、刚换过的两个、已就位的，三种颜色分开；"
                   + "换一种算法再跑，比较次数差得很明显";
        }

        public override void Command(int op, int key, int target)
        {
            if (op == Ds.CmdInsert) InAppend(Clamp(key));
            else if (op == Ds.CmdDelete) InRemoveLast();
            else if (op == Ds.CmdClear) InClear();
            else if (op == Ds.CmdRegenerate) BuildTarget();
            else if (op == Ds.CmdAppendRandom) AppendRandomKeys(4);
        }

        private static int Clamp(int key)
        {
            if (key < 1) return 1;
            if (key > DsConst.MaxKey) return DsConst.MaxKey;
            return key;
        }

        public override int Primary { get { return compares; } }
        public override int Secondary { get { return writes; } }
        public override int Work { get { return settled; } }
        public override int Pending { get { return Math.Max(0, InLen() - settled); } }
    }

    // ───────────────────────────────────────────────────────────── 引擎

    /// <summary>
    /// 运算侧。十个模型各管一个场景，引擎负责：切场景、推进、把模型摊进快照、
    /// 以及处理界面进程从命令邮箱发来的直接操作。
    ///
    /// **引擎的数据是权威**：界面进程那一侧的快照只是上一帧的回声，
    /// 因此 ImportScene 只认邮箱里的命令，其余字段一律不读——
    /// 否则界面进程一次迟到的回写就会把运算进程的结构顶回几步之前。
    /// </summary>
    internal sealed class DsEngine : IVisualEngine
    {
        private readonly VisualSnapshot own = new VisualSnapshot(DsConst.Elements);
        private readonly DsModel[] models = new DsModel[10];
        private readonly List<int[]> undo = new List<int[]>();

        private int mode, optionA, optionB;
        private int seed = DsConst.DefaultSeed;
        private int steps;
        private bool finished;
        private bool hasResult;
        private int lastMailbox;

        public DsEngine()
        {
            models[0] = new ArrayModel(own);
            models[1] = new ListModel(own);
            models[2] = new HashModel(own);
            models[3] = new TreeModel(own);
            models[4] = new HeapModel(own);
            models[5] = new BTreeModel(own);
            models[6] = new TrieModel(own);
            models[7] = new GraphModel(own);
            models[8] = new DsuModel(own);
            models[9] = new SortModel(own);
            ApplyOptions();
        }

        private DsModel Model { get { return models[mode]; } }

        public int ElementCount { get { return DsConst.Elements; } }

        public bool Finished { get { return finished; } }
        public bool HasResult { get { return hasResult; } }

        /// <summary>种子。自检与界面两边都能改它，改完下一次 ResetAll 就用新种子生成数据。</summary>
        public int Seed
        {
            get { return seed; }
            set { seed = value; ApplyOptions(); }
        }

        public string StatusText() { return Model.StatusText(); }
        public string TitleText() { return Model.Title(); }

        public void Configure(int modeKind, int a, int b)
        {
            mode = modeKind < 0 ? 0 : (modeKind > 9 ? 9 : modeKind);
            optionA = a < 0 ? 0 : a;
            optionB = b < 0 ? 0 : b;
            ApplyOptions();
            undo.Clear();
        }

        private void ApplyOptions()
        {
            for (int i = 0; i < models.Length; i++)
            {
                models[i].OptionA = optionA;
                models[i].OptionB = optionB;
                models[i].Seed = seed;
            }
        }

        public void ResetAll()
        {
            seed = own.Seed != 0 ? own.Seed : seed;
            ApplyOptions();
            Model.BuildTarget();
            Model.ResetRun();
            steps = 0;
            finished = false;
            hasResult = false;
            lastMailbox = 0;
            own.Marker[DsConst.Mailbox] = 0;
            FlushModel();
        }

        public void ResetRun()
        {
            Model.ResetRun();
            steps = 0;
            finished = false;
            hasResult = false;
            FlushModel();
        }

        public void StepOnce()
        {
            if (finished) return;
            bool more = Model.Step();
            steps++;
            if (!more)
            {
                finished = true;
                hasResult = Model.Primary >= 0;
            }
            FlushModel();
        }

        private void FlushModel()
        {
            Model.Flush();
            own.Marker[DsConst.Mailbox] = 0;
        }

        /// <summary>把引擎自己的快照摊进目标快照。头部字段由调用方再补。</summary>
        public void RefreshSnapshot(VisualSnapshot target)
        {
            FlushModel();
            Copy(own, target);
            target.StateCode = finished ? 2 : (steps > 0 ? 1 : 0);
            target.Steps = steps;
            target.WorkCount = Model.Work;
            target.PendingCount = Model.Pending;
            target.PrimaryMetric = Model.Primary;
            target.SecondaryMetric = Model.Secondary;
            target.CurrentIndex = Model.Current;
            target.FocusIndex = Model.Focus;
            target.Seed = seed;
            target.CustomA = Model.CustomA;
            target.CustomB = Model.CustomB;
            target.ModeKind = mode;
            target.OptionA = optionA;
            target.OptionB = optionB;
            target.Finished = finished ? 1 : 0;
            target.HasResult = hasResult ? 1 : 0;
            target.Running = 0;
        }

        public void ExportScene(VisualSnapshot target)
        {
            // 运算进程刚起来的那一帧带着 INIT 发来的种子。引擎的种子与它不一致就
            // 现场按新种子重新生成一遍数据——否则界面上换了种子要等一次点击才生效。
            if (target.Seed != 0 && target.Seed != seed)
            {
                seed = target.Seed;
                ApplyOptions();
                Model.BuildTarget();
                Model.ResetRun();
            }
            FlushModel();
            Copy(own, target);
            target.Seed = seed;
        }

        /// <summary>只读邮箱。界面进程改的结构进不来，除非它写的是邮箱。</summary>
        public void ImportScene(VisualSnapshot source)
        {
            int marker = source.Marker[DsConst.Mailbox];
            if (marker == 0 || marker == lastMailbox) return;
            lastMailbox = marker;
            int op = source.Attr0[DsConst.Mailbox];
            int key = source.Attr1[DsConst.Mailbox];
            int target = source.Attr2[DsConst.Mailbox];
            own.Marker[DsConst.Mailbox] = 0;
            if (source.Seed != 0 && source.Seed != seed)
            {
                seed = source.Seed;
                ApplyOptions();
            }
            if (op == Ds.CmdUndo)
            {
                Undo();
                return;
            }
            PushUndo();
            Model.Command(op, key, target);
        }

        private void PushUndo()
        {
            int[] copy = new int[DsConst.Elements];
            Array.Copy(Model.SceneData, copy, DsConst.Elements);
            undo.Add(copy);
            if (undo.Count > 16) undo.RemoveAt(0);
        }

        private void Undo()
        {
            if (undo.Count == 0) return;
            int[] data = undo[undo.Count - 1];
            undo.RemoveAt(undo.Count - 1);
            Array.Copy(data, Model.SceneData, DsConst.Elements);
            Model.ResetRun();
            FlushModel();
        }

        private static void Copy(VisualSnapshot from, VisualSnapshot to)
        {
            Array.Copy(from.State, to.State, DsConst.Elements);
            Array.Copy(from.Marker, to.Marker, DsConst.Elements);
            Array.Copy(from.Tag, to.Tag, DsConst.Elements);
            Array.Copy(from.Attr0, to.Attr0, DsConst.Elements);
            Array.Copy(from.Attr1, to.Attr1, DsConst.Elements);
            Array.Copy(from.Attr2, to.Attr2, DsConst.Elements);
            Array.Copy(from.Attr3, to.Attr3, DsConst.Elements);
            Array.Copy(from.Attr4, to.Attr4, DsConst.Elements);
            Array.Copy(from.Attr5, to.Attr5, DsConst.Elements);
            to.Seed = from.Seed;
        }
    }

    // ───────────────────────────────────────────────────────────── 场景（渲染侧）

    /// <summary>
    /// 渲染侧：十个场景共用这一个对象，画什么由快照里的模式与属性决定。
    ///
    /// 界面进程**不保存结构**：树有多少节点、链指向谁、桶里有几个键，全部从快照读，
    /// 因此运算进程推进到哪一帧，屏幕上就是哪一帧。界面进程只多记两样东西：
    /// 点选中的元素，以及还没发出去的邮箱命令。
    /// </summary>
    internal sealed class DsScene : IVisualScene, ISeedProvider, IPathInspector
    {
        private readonly ScenePalette palette = DsPalette.Build();
        private readonly DsView view = new DsView();
        private readonly List<DsRun> runs = new List<DsRun>();
        // 字号按像素给，由 VisualCore 的字号表统一管：数字不许低于 MinimumDigitPx，
        // --layoutcheck 会把这两档打出来并断言。放不下时的做法是减少同屏元素，
        // 不是把字号缩回去；tiny 只降一档，且仍在数字字号下限之上。
        private readonly Font cell = VisualFonts.DigitFont(13f);
        private readonly Font tiny = VisualFonts.DigitFont(12f);
        private readonly Font note = VisualFonts.UiFont();

        private int selected = -1;
        private int pendingOp, pendingKey, pendingTarget;
        private int lastOp, lastKey, lastTarget;
        private int mailSeq, lastWritten;
        private int seed = DsConst.DefaultSeed;

        public string Title { get { return "数据结构演示"; } }
        public int ElementCount { get { return DsConst.Elements; } }
        public Size CanvasSize { get { return new Size(DsConst.CanvasWidth, DsConst.CanvasHeight); } }
        public ScenePalette Palette { get { return palette; } }
        public bool UsesValue { get { return true; } }
        public int MinValue { get { return DsConst.MinKey; } }
        public int MaxValue { get { return DsConst.MaxKey; } }
        public int[] BrushSizes { get { return new int[0]; } }

        public int Seed
        {
            get { return seed; }
            set { seed = value; }
        }

        public string HintText
        {
            get
            {
                return "工具「插入」「删除」「查找」都用右边「刷子值」里的数字当键值；「选中」只标记你点中的元素。\n"
                     + "一次点击只作用在第一个元素上（拖动不会连发）。改结构会清空运行状态，从头再跑一遍。\n"
                     + "状态只用颜色区分；连线每一段都只横或只竖，线在文字的位置让开。";
            }
        }

        public ToolDescriptor[] Tools
        {
            get
            {
                return new ToolDescriptor[]
                {
                    new ToolDescriptor(0, "插入", false, true),
                    new ToolDescriptor(1, "删除", false, true),
                    new ToolDescriptor(2, "查找", false, true),
                    new ToolDescriptor(3, "选中", false, false),
                    new ToolDescriptor(4, "清空", false, false)
                };
            }
        }

        // ── 命中测试

        public int HitTest(int x, int y)
        {
            for (int i = 0; i < view.Count && i < DsConst.Elements; i++)
            {
                Rectangle box = view.Box[i];
                if (box.Width > 0 && box.Contains(x, y)) return i;
                if (view.Mode == 4 && i < 32)
                {
                    Rectangle row = DsView.HeapArrayCell(i);
                    if (row.Contains(x, y)) return i;
                }
            }
            return -1;
        }

        // ── 落笔：只记命令，真正的改动在运算进程那边做

        public bool ApplyTool(int toolId, int element, int radius, int value, bool erase, bool first)
        {
            if (!first) return false;
            int op = Ds.CmdNone;
            switch (toolId)
            {
                case 0: op = Ds.CmdInsert; break;
                case 1: op = Ds.CmdDelete; break;
                case 2: op = Ds.CmdFind; break;
                case 3: op = Ds.CmdSelect; break;
                case 4: op = Ds.CmdClear; break;
                default: return false;
            }
            if (erase && toolId == 0) op = Ds.CmdDelete;      // 右键＝反着来
            if (toolId == 3) selected = element;
            pendingOp = op;
            pendingKey = value;
            pendingTarget = element;
            return true;
        }

        public void BeginGesture() { }

        public bool EndGesture() { return false; }

        /// <summary>撤销由运算进程做（它手里才有结构的快照），这里只发一条命令。</summary>
        public bool CanUndo { get { return true; } }

        public bool Undo()
        {
            pendingOp = Ds.CmdUndo;
            pendingKey = 0;
            pendingTarget = -1;
            return true;
        }

        /// <summary>面板上的按钮也叫这条路径：只记命令，ExportScene 时写进邮箱。</summary>
        public void RequestCommand(int op, int key, int target)
        {
            pendingOp = op;
            pendingKey = key;
            pendingTarget = target;
        }

        /// <summary>把邮箱写进快照。命令序号单调增，运算进程按序号去重。</summary>
        public void ExportScene(VisualSnapshot snapshot)
        {
            if (pendingOp != 0)
            {
                mailSeq = mailSeq % 250 + 1;
                lastWritten = mailSeq;
                lastOp = pendingOp;
                lastKey = pendingKey;
                lastTarget = pendingTarget;
                pendingOp = 0;
            }
            snapshot.Marker[DsConst.Mailbox] = (byte)lastWritten;
            snapshot.Attr0[DsConst.Mailbox] = lastOp;
            snapshot.Attr1[DsConst.Mailbox] = lastKey;
            snapshot.Attr2[DsConst.Mailbox] = lastTarget;
            snapshot.Seed = seed;
        }

        // ── 元素上写什么

        public string ElementText(int element, VisualSnapshot snapshot, SceneViewState viewState)
        {
            CollectRuns(snapshot, viewState, element);
            if (runs.Count == 0) return "";
            string text = runs[0].Text;
            runs.Clear();
            return text;
        }

        /// <summary>
        /// 一个元素上要写的全部文字。**同一份列表既用来画，也被自检用来量宽度**，
        /// 因此自检量的就是屏幕上画出去的字。
        /// </summary>
        private void CollectRuns(VisualSnapshot snapshot, SceneViewState viewState, int only)
        {
            runs.Clear();
            int mode = snapshot.ModeKind;
            for (int i = 0; i < view.Count && i < DsConst.Elements; i++)
            {
                if (only >= 0 && i != only) continue;
                Rectangle box = view.Box[i];
                if (box.Width == 0) continue;
                if (snapshot.ModeKind == 0 && i >= 64 && snapshot.Attr2[i] <= 0) continue;
                byte state = StateOf(snapshot, i);
                Color pad = FillOf(state);
                switch (mode)
                {
                    case 0:
                        if (i >= 64)
                        {
                            // 旧块：搬走的那些仍然写着值，只是底色换成了「已失效」
                            if (snapshot.Attr2[i] > 0) AddRun(i, Text(snapshot.Attr2[i]), box, pad);
                        }
                        else if (state != Ds.Empty && state != Ds.Blocked && snapshot.Attr0[i] > 0)
                        {
                            AddRun(i, Text(snapshot.Attr0[i]), box, pad);
                        }
                        break;
                    case 1:
                        if (i == 0) AddRun(i, "哨兵", new Rectangle(box.X, box.Y + 10, box.Width, 16), pad);
                        else if (snapshot.Attr0[i] > 0)
                        {
                            AddRun(i, Text(snapshot.Attr0[i]),
                                   new Rectangle(box.X, box.Y + 1, box.Width, 15), pad);
                            int next = snapshot.Attr1[i];
                            AddRun(i, next >= 0 ? "next " + next : "next 空",
                                   new Rectangle(box.X, box.Y + 18, box.Width, 15), pad);
                        }
                        break;
                    case 2:
                        if (i < 32)
                        {
                            if (state == Ds.Empty) break;
                            AddRun(i, Text(i), box, pad);
                        }
                        else if (snapshot.Attr0[i] > 0) AddRun(i, Text(snapshot.Attr0[i]), box, pad);
                        break;
                    case 3:
                        if (snapshot.Attr0[i] <= 0) break;
                        AddRun(i, snapshot.OptionA == 2
                               ? Text(snapshot.Attr0[i]) + (snapshot.Attr5[i] == 0 ? "R" : "B")
                               : Text(snapshot.Attr0[i]), box, pad);
                        break;
                    case 4:
                        if (snapshot.Attr0[i] <= 0 || state == Ds.Empty) break;
                        AddRun(i, Text(snapshot.Attr0[i]), box, pad);
                        AddRun(i, Text(snapshot.Attr0[i]), DsView.HeapArrayCell(i), pad);
                        break;
                    case 5:
                        {
                            int keys = snapshot.Tag[i];
                            for (int k = 0; k < keys && k < 3; k++)
                            {
                                int value = k == 0 ? snapshot.Attr0[i] : (k == 1 ? snapshot.Attr1[i] : snapshot.Attr2[i]);
                                if (value <= 0) continue;
                                Rectangle slot = new Rectangle(box.X + 4 + k * DsView.BTreeSlotW,
                                                               box.Y + 4, DsView.BTreeSlotW, 26);
                                AddRun(i, Text(value), slot, pad);
                            }
                        }
                        break;
                    case 6:
                        if (snapshot.Attr0[i] > 0)
                        {
                            AddRun(i, DsText.Render(snapshot.Attr0[i], snapshot.Tag[i] == 0), box, pad);
                        }
                        break;
                    case 7:
                        if (i < 8)
                        {
                            AddRun(i, char.ConvertFromUtf32(snapshot.Attr0[i]),
                                   new Rectangle(box.X, box.Y + 2, box.Width, 16), pad);
                            if (snapshot.Attr3[i] > 0)
                            {
                                AddRun(i, "#" + snapshot.Attr3[i],
                                       new Rectangle(box.X, box.Y + 20, box.Width, 16), pad);
                            }
                        }
                        else if (i >= 72 && snapshot.Attr2[i] >= 0)
                        {
                            AddRun(i, char.ConvertFromUtf32('A' + snapshot.Attr0[i]), box, pad);
                        }
                        break;
                    case 8:
                        if (snapshot.Attr1[i] == i) AddRun(i, i + " 根", box, pad);
                        else AddRun(i, i + "→" + snapshot.Attr1[i], box, pad);
                        break;
                    default:
                        if (snapshot.Attr0[i] > 0)
                        {
                            AddRun(i, Text(snapshot.Attr0[i]),
                                   new Rectangle(box.X, box.Y + 1, box.Width, 14), pad);
                        }
                        break;
                }
            }
            // 「元素上标」那一档：不换字号、不换位置，只换写上去的内容，
            // 因此放不放得下由自检统一量一遍
            if (viewState != null && viewState.DisplayMode != 0)
            {
                if (viewState.DisplayMode == 3) runs.Clear();
                else
                {
                    for (int i = 0; i < runs.Count; i++)
                    {
                        DsRun run = runs[i];
                        if (viewState.DisplayMode == 1) run.Text = run.Owner.ToString(CultureInfo.InvariantCulture);
                        else
                        {
                            int order = view.Order[run.Owner];
                            run.Text = order >= 0 ? order.ToString(CultureInfo.InvariantCulture) : "-";
                        }
                    }
                }
            }
        }

        private void AddRun(int owner, string text, Rectangle box, Color pad)
        {
            if (text.Length == 0 || box.Width <= 0) return;
            DsRun run = new DsRun();
            run.Owner = owner;
            run.Text = text;
            run.Box = box;
            run.Pad = pad;
            runs.Add(run);
        }

        private static string Text(int value)
        {
            return value.ToString(CultureInfo.InvariantCulture);
        }

        // ── 绘制

        public void Paint(Graphics g, VisualSnapshot snapshot, SceneViewState viewState)
        {
            ThemePalette theme = ThemeManager.Palette;
            view.Build(snapshot);
            using (SolidBrush background = new SolidBrush(theme.Background))
            {
                g.FillRectangle(background, 0, 0, DsConst.CanvasWidth, DsConst.CanvasHeight);
            }
            DrawHeader(g, snapshot, theme);
            FillPass(g, snapshot, theme);
            DrawLines(g, theme);
            DrawLabels(g, snapshot, theme);
            DrawInput(g, snapshot, theme);
            TextPass(g, snapshot, viewState, theme);
        }

        private byte StateOf(VisualSnapshot snapshot, int index)
        {
            if (index == selected) return Ds.Ref;            // 点中的元素用「参照元素」那一档
            return snapshot.State[index];
        }

        private Color FillOf(byte state)
        {
            return DsPalette.Fill(state);
        }

        private void FillPass(Graphics g, VisualSnapshot snapshot, ThemePalette theme)
        {
            for (int i = 0; i < view.Count && i < DsConst.Elements; i++)
            {
                Rectangle box = view.Box[i];
                if (box.Width == 0) continue;
                // 旧块里没搬过东西的槽不画：旧块只该显示它当时真正装着的那些元素
                if (snapshot.ModeKind == 0 && i >= 64 && snapshot.Attr2[i] <= 0) continue;
                using (SolidBrush brush = new SolidBrush(FillOf(StateOf(snapshot, i))))
                {
                    g.FillRectangle(brush, box);
                }
                if (view.Mode == 4 && i < 32)
                {
                    using (SolidBrush brush = new SolidBrush(FillOf(StateOf(snapshot, i))))
                    {
                        g.FillRectangle(brush, DsView.HeapArrayCell(i));
                    }
                }
            }
            // B 树的键槽分隔线：这是格子线不是连线，用网格线的颜色画，也不进 DsPath，
            // 因此不参与「连线只许横或竖」那条断言
            if (snapshot.ModeKind == 5)
            {
                using (Pen grid = new Pen(theme.GridLine))
                {
                    for (int i = 0; i < 24; i++)
                    {
                        Rectangle node = view.Box[i * 2];
                        if (node.Width == 0) continue;
                        for (int k = 1; k < 3; k++)
                        {
                            int x = node.X + 4 + k * DsView.BTreeSlotW - 3;
                            g.DrawLine(grid, x, node.Y + 4, x, node.Bottom - 4);
                        }
                    }
                }
            }
        }

        /// <summary>
        /// 连线两遍描：先描边色再压主色。**高亮那一支换的是颜色，不是粗细**——
        /// 每一段仍然是横段或竖段，`--layoutcheck` 逐段断言。
        /// </summary>
        private void DrawLines(Graphics g, ThemePalette theme)
        {
            using (Pen halo = new Pen(DsPalette.LineHalo(), DsConst.HaloWidth))
            using (Pen plain = new Pen(DsPalette.LineMain(), DsConst.MainWidth))
            using (Pen hot = new Pen(DsPalette.LineHot(), DsConst.MainWidth))
            {
                halo.LineJoin = System.Drawing.Drawing2D.LineJoin.Round;
                for (int pass = 0; pass < 2; pass++)
                {
                    for (int i = 0; i < view.Path.Lines.Count; i++)
                    {
                        DsLine line = view.Path.Lines[i];
                        if (pass == 0) g.DrawLine(halo, line.X1, line.Y1, line.X2, line.Y2);
                        else g.DrawLine(line.Hot ? hot : plain, line.X1, line.Y1, line.X2, line.Y2);
                    }
                }
            }
        }

        /// <summary>
        /// 文字。**压在连线上的字要先让线让开**：用所在块的填充色把字位盖一遍再画字，
        /// 只对真正与线相交的字位做，因此折线在字的两侧照常连着。字一律粗体。
        /// </summary>
        private void TextPass(Graphics g, VisualSnapshot snapshot, SceneViewState viewState, ThemePalette theme)
        {
            CollectRuns(snapshot, viewState, -1);
            TextFormatFlags flags = TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter
                                    | TextFormatFlags.NoPadding | TextFormatFlags.NoPrefix;
            for (int i = 0; i < runs.Count; i++)
            {
                DsRun run = runs[i];
                Color color = run.Owner >= 0 ? DsPalette.Text(StateOf(snapshot, run.Owner)) : theme.PanelText;
                if (view.Path.Hits(run.Box))
                {
                    using (SolidBrush pad = new SolidBrush(run.Pad))
                    {
                        g.FillRectangle(pad, run.Box.X - 1, run.Box.Y - 1, run.Box.Width + 2, run.Box.Height + 2);
                    }
                }
                TextRenderer.DrawText(g, run.Text, run.Box.Height <= 16 ? tiny : cell, run.Box, color, flags);
            }
        }

        /// <summary>画布顶部那一行：模式、种子、以及本场景最该看的几个数。</summary>
        private void DrawHeader(Graphics g, VisualSnapshot snapshot, ThemePalette theme)
        {
            TextFormatFlags flags = TextFormatFlags.Left | TextFormatFlags.VerticalCenter
                                    | TextFormatFlags.NoPadding | TextFormatFlags.NoPrefix;
            string line = DsNames.Modes[snapshot.ModeKind] + "　seed=" + snapshot.Seed + "　" + Describe(snapshot);
            TextRenderer.DrawText(g, line, note, new Rectangle(16, 4, DsConst.CanvasWidth - 32, 22),
                                  theme.PanelText, flags);
        }

        /// <summary>画布上那些不是元素文字的说明文字。</summary>
        private void DrawLabels(Graphics g, VisualSnapshot snapshot, ThemePalette theme)
        {
            TextFormatFlags flags = TextFormatFlags.Left | TextFormatFlags.VerticalCenter
                                    | TextFormatFlags.NoPadding | TextFormatFlags.NoPrefix;
            TextFormatFlags middle = TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter
                                     | TextFormatFlags.NoPadding | TextFormatFlags.NoPrefix;
            Color muted = theme.MutedText;
            switch (snapshot.ModeKind)
            {
                case 0:
                    TextRenderer.DrawText(g, "新块（容量 " + snapshot.PrimaryMetric + "）",
                                          note, new Rectangle(16, 92, 420, 20), muted, flags);
                    TextRenderer.DrawText(g, "旧块（地址已失效，里面的值还看得见）",
                                          note, new Rectangle(16, 240, 420, 20), muted, flags);
                    break;
                case 1:
                    TextRenderer.DrawText(g, "下标 0 是头哨兵；每条连线是一个 next 指针",
                                          note, new Rectangle(20, 118, 600, 20), muted, flags);
                    break;
                case 2:
                    TextRenderer.DrawText(g, "桶 " + snapshot.PrimaryMetric + " 个，键 " + snapshot.SecondaryMetric
                                          + " 个；每个桶右边挂的是冲突链",
                                          note, new Rectangle(16, 80, 700, 20), muted, flags);
                    break;
                case 3:
                    TextRenderer.DrawText(g, "x 坐标＝键的排名（中序），y 坐标＝层数；旋转一次坐标就重排一次"
                                          + (snapshot.OptionA == 2 ? "；键值后面的 R／B 是红黑颜色" : ""),
                                          note, new Rectangle(16, 34, 900, 20), muted, flags);
                    break;
                case 4:
                    TextRenderer.DrawText(g, "数组行（存储顺序，下标 0 起）",
                                          note, new Rectangle(16, 404, 500, 20), muted, flags);
                    for (int i = 0; i < 32; i++)
                    {
                        Rectangle cell = DsView.HeapArrayCell(i);
                        TextRenderer.DrawText(g, i.ToString(CultureInfo.InvariantCulture), note,
                                              new Rectangle(cell.X, 462, cell.Width, 16), muted, middle);
                    }
                    break;
                case 5:
                    TextRenderer.DrawText(g, "一个节点三个键槽；子节点排在父节点下面，连线拐两个直角",
                                          note, new Rectangle(16, 34, 900, 20), muted, flags);
                    break;
                case 6:
                    TextRenderer.DrawText(g, "一层一个单位：" + (snapshot.OptionA == 1 ? "一个汉字占三层（UTF-8 三字节）" : "一个汉字占一层（一个码点）"),
                                          note, new Rectangle(16, DsConst.CanvasHeight - 44, 900, 20), muted, flags);
                    break;
                case 7:
                    TextRenderer.DrawText(g, snapshot.OptionA == 0 ? "右：邻接矩阵（行是起点、列是终点）"
                                          : (snapshot.OptionA == 1 ? "右：邻接表（每个顶点一条链）" : "右：边表（一条边一项）"),
                                          note, new Rectangle(470, 62, 460, 20), muted, flags);
                    if (snapshot.OptionA == 0)
                    {
                        for (int v = 0; v < 8; v++)
                        {
                            Rectangle cell = DsView.GraphMatrixCell(0, v);
                            TextRenderer.DrawText(g, char.ConvertFromUtf32('A' + v), note,
                                                  new Rectangle(cell.X, 92, cell.Width, 20), muted, middle);
                        }
                        for (int u = 0; u < 8; u++)
                        {
                            Rectangle cell = DsView.GraphMatrixCell(u, 0);
                            TextRenderer.DrawText(g, char.ConvertFromUtf32('A' + u), note,
                                                  new Rectangle(452, cell.Y, 24, cell.Height), muted, middle);
                        }
                    }
                    else if (snapshot.OptionA == 1)
                    {
                        for (int u = 0; u < 8; u++)
                        {
                            Rectangle cell = DsView.GraphListEntry(u, 0);
                            TextRenderer.DrawText(g, char.ConvertFromUtf32('A' + u), note,
                                                  new Rectangle(480, cell.Y, 48, cell.Height), muted, middle);
                        }
                    }
                    else DrawEdgeTable(g, snapshot, muted);
                    break;
                case 8:
                    TextRenderer.DrawText(g, "父指针画成连线；自己指向自己的是根",
                                          note, new Rectangle(16, DsConst.CanvasHeight - 44, 700, 20), muted, flags);
                    break;
                default:
                    TextRenderer.DrawText(g, "柱高＝键值；比较中的两根、刚换过的两根、已就位的分开上色",
                                          note, new Rectangle(16, DsConst.CanvasHeight - 48, 900, 20), muted, flags);
                    break;
            }
        }

        /// <summary>边表：把输入区里的边原样列出来，边上带通行次序。</summary>
        private void DrawEdgeTable(Graphics g, VisualSnapshot snapshot, Color muted)
        {
            TextFormatFlags flags = TextFormatFlags.Left | TextFormatFlags.VerticalCenter
                                    | TextFormatFlags.NoPadding | TextFormatFlags.NoPrefix;
            int guard = 0;
            for (int e = 0; e < 14 && guard++ < 14; e++)
            {
                int u = snapshot.Attr0[DsConst.InputBase + e * 2];
                if (u <= 0) break;
                int v = snapshot.Attr0[DsConst.InputBase + e * 2 + 1];
                if (u > 7 || v > 7) continue;
                bool hot = (u - 1 == snapshot.CustomA && v - 1 == snapshot.CustomB)
                           || (u - 1 == snapshot.CustomB && v - 1 == snapshot.CustomA);
                string text = e + "：" + char.ConvertFromUtf32('A' + u - 1) + " — "
                              + char.ConvertFromUtf32('A' + v - 1);
                TextRenderer.DrawText(g, text, note, new Rectangle(492, 96 + e * 28, 200, 24),
                                      hot ? DsPalette.LineHot() : muted, flags);
            }
        }

        /// <summary>画布最下面一行：场景数据本身。直接操作的按钮改的就是它。</summary>
        private void DrawInput(Graphics g, VisualSnapshot snapshot, ThemePalette theme)
        {
            TextFormatFlags flags = TextFormatFlags.Left | TextFormatFlags.VerticalCenter
                                    | TextFormatFlags.NoPadding | TextFormatFlags.NoPrefix
                                    | TextFormatFlags.EndEllipsis;
            TextRenderer.DrawText(g, InputSummary(snapshot), note,
                                  new Rectangle(16, DsConst.CanvasHeight - 20, DsConst.CanvasWidth - 32, 18),
                                  theme.MutedText, flags);
        }

        private string InputSummary(VisualSnapshot snapshot)
        {
            int mode = snapshot.ModeKind;
            if (mode == 6)
            {
                System.Text.StringBuilder words = new System.Text.StringBuilder("场景数据（词表）：");
                int at = 0, count = 0;
                bool byByte = snapshot.OptionA == 1;
                while (at < DsConst.InputCount && snapshot.Attr0[DsConst.InputBase + at] > 0 && count < 6)
                {
                    int len = snapshot.Attr0[DsConst.InputBase + at];
                    System.Text.StringBuilder word = new System.Text.StringBuilder();
                    for (int i = 0; i < len; i++)
                    {
                        int unit = snapshot.Attr0[DsConst.InputBase + at + 1 + i];
                        if (!byByte) word.Append(DsText.Render(unit, false));
                        else if (unit < 0x80) word.Append((char)unit);
                    }
                    if (word.Length == 0) word.Append("（" + len + " 字节）");
                    words.Append(word).Append(' ');
                    at += 1 + len;
                    count++;
                }
                return words.ToString();
            }
            if (mode == 7)
            {
                System.Text.StringBuilder edges = new System.Text.StringBuilder("场景数据（边表）：");
                int guard = 0;
                for (int e = 0; e < 20 && guard++ < 20; e++)
                {
                    int u = snapshot.Attr0[DsConst.InputBase + e * 2];
                    if (u <= 0) break;
                    int v = snapshot.Attr0[DsConst.InputBase + e * 2 + 1];
                    edges.Append(char.ConvertFromUtf32('A' + u - 1)).Append('-')
                         .Append(char.ConvertFromUtf32('A' + v - 1)).Append(' ');
                }
                return edges.ToString();
            }
            if (mode == 8)
            {
                System.Text.StringBuilder pairs = new System.Text.StringBuilder("场景数据（合并的对）：");
                int guard = 0;
                for (int e = 0; e < 20 && guard++ < 20; e++)
                {
                    int a = snapshot.Attr0[DsConst.InputBase + e * 2];
                    if (a <= 0) break;
                    int b = snapshot.Attr0[DsConst.InputBase + e * 2 + 1];
                    pairs.Append(a - 1).Append('-').Append(b - 1).Append(' ');
                }
                return pairs.ToString();
            }
            System.Text.StringBuilder numbers = new System.Text.StringBuilder(
                mode == 9 ? "场景数据（待排序的数）：" : "场景数据（要插入的键）：");
            int shown = 0;
            for (int i = 0; i < DsConst.InputCount && shown < 32; i++)
            {
                int value = snapshot.Attr0[DsConst.InputBase + i];
                if (value <= 0) break;
                numbers.Append(value).Append(' ');
                shown++;
            }
            if (numbers.Length > 40) return numbers.ToString();
            return numbers.ToString();
        }

        /// <summary>状态行与画布标题共用的一句话：本场景当下最该看的数。</summary>
        public string Describe(VisualSnapshot snapshot)
        {
            switch (snapshot.ModeKind)
            {
                case 0:
                    return "size=" + snapshot.SecondaryMetric + "／capacity=" + snapshot.PrimaryMetric
                           + "　扩容 " + snapshot.CustomA + " 次　搬迁 " + snapshot.CustomB + " 个元素";
                case 1:
                    return "插入 " + snapshot.PrimaryMetric + " 次　指针写入 " + snapshot.SecondaryMetric
                           + " 次　比较 " + snapshot.WorkCount + " 次　删除 " + snapshot.CustomA + " 次";
                case 2:
                    return "桶 " + snapshot.PrimaryMetric + " 个　键 " + snapshot.SecondaryMetric
                           + " 个　最长链 " + snapshot.PendingCount + "　再哈希 " + snapshot.CustomA
                           + " 次　搬迁 " + snapshot.CustomB + " 次";
                case 3:
                    return "树高 " + snapshot.PrimaryMetric + "　比较 " + snapshot.WorkCount
                           + " 次　旋转 " + snapshot.SecondaryMetric + " 次　变色 " + snapshot.CustomA + " 次";
                case 4:
                    return "堆大小 " + snapshot.PrimaryMetric + "　比较 " + snapshot.WorkCount
                           + " 次　交换 " + snapshot.SecondaryMetric + " 次　弹出 " + snapshot.CustomA + " 个";
                case 5:
                    return "键 " + snapshot.PendingCount + " 个　树高 " + snapshot.PrimaryMetric
                           + "　分裂 " + snapshot.SecondaryMetric + " 次　合并 " + snapshot.CustomA
                           + " 次　借用 " + snapshot.CustomB + " 次";
                case 6:
                    return "节点 " + snapshot.PrimaryMetric + " 个　最大深度 " + snapshot.SecondaryMetric
                           + "　词 " + snapshot.PendingCount + " 个　前缀命中 " + snapshot.CustomA + " 个";
                case 7:
                    return "已访问顶点 " + snapshot.PrimaryMetric + "／8　边 " + snapshot.SecondaryMetric
                           + " 条　检查 " + snapshot.WorkCount + " 次　队列／栈 " + snapshot.PendingCount;
                case 8:
                    return "集合 " + snapshot.PrimaryMetric + " 个　find 共走 " + snapshot.WorkCount
                           + " 步　上一次 find 压缩前 " + snapshot.CustomA + " 跳／压缩后 " + snapshot.CustomB + " 跳";
                default:
                    return "比较 " + snapshot.PrimaryMetric + " 次　交换／写入 " + snapshot.SecondaryMetric
                           + " 次　已就位 " + snapshot.WorkCount + "／" + snapshot.PendingCount;
            }
        }

            // ── 连线自检与文字自检（--layoutcheck 用这两个）

        /// <summary>
        /// 十个场景各跑到底，逐个量**实际画出去**的线段，返回斜段的条数。
        /// 另外造一条斜线当反例：检查抓不住反例才是真问题，
        /// 因此反例没被数出来也要记一笔。
        /// </summary>
        public int CountDiagonalSegments(VisualSnapshot snapshot, int cw, int ch, List<string> report)
        {
            int total = 0, lines = 0;
            for (int mode = 0; mode < 10; mode++)
            {
                VisualSnapshot s = DsSelfTest.RunMode(mode, snapshot.OptionA, snapshot.OptionB, Seed);
                DsView probe = new DsView();
                probe.Build(s);
                int diagonal = probe.Path.DiagonalCount();
                lines += probe.Path.Lines.Count;
                total += diagonal;
                if (report != null)
                {
                    report.Add(DsNames.Modes[mode] + "：线 "
                               + probe.Path.Lines.Count + " 段，斜段 " + diagonal);
                }
            }
            DsPath trap = new DsPath();
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

        /// <summary>
        /// 量元素上的每一段文字，返回放不下的段数。**量的是屏幕上真画的那一份**：
        /// 与绘制共用 CollectRuns，因此字号、内容、矩形三者不会各说各话。
        /// </summary>
        public int CheckCellText(VisualSnapshot snapshot, TextWriter writer)
        {
            int overhang, advance;
            TextRenderer.MeasureText("0", cell, new Size(4000, 100), TextFormatFlags.NoPadding);
            MeasureOverhang(out overhang, out advance);
            int over = 0, worst = 0, widest = 0, used = 0;
            string worstText = "";
            for (int mode = 0; mode < 10; mode++)
            {
                VisualSnapshot s = DsSelfTest.RunMode(mode, snapshot.OptionA, snapshot.OptionB, Seed);
                view.Build(s);
                for (int display = 0; display < 4; display++)
                {
                    SceneViewState vs = new SceneViewState();
                    vs.DisplayMode = display;
                    CollectRuns(s, vs, -1);
                    for (int i = 0; i < runs.Count; i++)
                    {
                        DsRun run = runs[i];
                        Font font = run.Box.Height <= 16 ? tiny : cell;
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
            writer.WriteLine("  note 文字度量：等宽数字每字符 " + advance + " 像素，度量里的固定余量 "
                             + overhang + " 像素（已扣掉）");
            writer.WriteLine(VisualFonts.Line("节点键值", cell, "，写在结构单元里（盒高大于 16 像素）"));
            writer.WriteLine(VisualFonts.Line("小格数字", tiny, "，盒高不超过 16 像素时用这一档"));
            bool fontOk = SelfTestHost.Check(writer, "min_digit_font_at_least_"
                + VisualFonts.MinimumDigitPx.ToString("0.#", CultureInfo.InvariantCulture),
                VisualFonts.DigitOk(cell) && VisualFonts.DigitOk(tiny),
                "本演示的最小数字字号 " + tiny.Size.ToString("0.#", CultureInfo.InvariantCulture)
                + " 像素（小格数字），节点键值 " + cell.Size.ToString("0.#", CultureInfo.InvariantCulture) + " 像素");
            writer.WriteLine("  note 标注：量了 " + used + " 段，最宽的一段 [" + worstText + "] 需 " + worst
                             + " 像素，最宽的矩形 " + widest + " 像素，放不下的段数 " + over + "（显示模式 4 种 × 场景 10 个）");
            return over + (fontOk ? 0 : 1);
        }

        /// <summary>TextRenderer 的宽度里带一段与字数无关的余量，用两次度量之差把它求出来。</summary>
        private void MeasureOverhang(out int overhang, out int advance)
        {
            int one = TextRenderer.MeasureText("0", cell, new Size(4000, 100),
                                               TextFormatFlags.NoPadding).Width;
            int ten = TextRenderer.MeasureText("0000000000", cell, new Size(4000, 100),
                                               TextFormatFlags.NoPadding).Width;
            int nine = TextRenderer.MeasureText("000000000", cell, new Size(4000, 100),
                                                TextFormatFlags.NoPadding).Width;
            advance = ten - nine;
            overhang = ten - advance * 10;
        }
    }

    // ───────────────────────────────────────────────────────────── 自检

    /// <summary>一次跑到底的结果：结构量都从这里取，绝不手写。</summary>
    internal sealed class DsResult
    {
        public int Mode, Steps, Primary, Secondary, Work, Pending, CustomA, CustomB;
        public bool Finished;
        public string Status = "";
        public VisualSnapshot Snapshot;
        public DsEngine Engine;
    }

    internal static class DsSelfTest
    {
        /// <summary>按给定配置把某个场景跑到底，返回快照。--layoutcheck 也用它。</summary>
        public static VisualSnapshot RunMode(int mode, int optionA, int optionB, int seed)
        {
            DsResult r = Run(mode, optionA, optionB, seed);
            return r.Snapshot;
        }

        public static DsResult Run(int mode, int optionA, int optionB, int seed)
        {
            DsEngine engine = new DsEngine();
            engine.Seed = seed;
            engine.Configure(mode, optionA, optionB);
            engine.ResetAll();
            VisualSnapshot snapshot = new VisualSnapshot(DsConst.Elements);
            for (int i = 0; i < 4000000 && !engine.Finished; i++) engine.StepOnce();
            engine.RefreshSnapshot(snapshot);
            DsResult result = new DsResult();
            result.Mode = mode;
            result.Steps = snapshot.Steps;
            result.Primary = snapshot.PrimaryMetric;
            result.Secondary = snapshot.SecondaryMetric;
            result.Work = snapshot.WorkCount;
            result.Pending = snapshot.PendingCount;
            result.CustomA = snapshot.CustomA;
            result.CustomB = snapshot.CustomB;
            result.Finished = snapshot.Finished != 0;
            result.Status = engine.StatusText();
            result.Snapshot = snapshot;
            result.Engine = engine;
            return result;
        }

        private static string Line(int mode, string name, string body)
        {
            return "scene " + (mode + 1) + " " + name + "  " + body;
        }
    }

    // ───────────────────────────────────────────────────────────── 演示定义

    /// <summary>数据结构演示：下拉里的名字、引擎与场景的工厂、状态文案、自测。</summary>
    internal sealed class DsDemoApp : IVisualDemo, IPathInspector
    {
        private readonly DsScene inspector = new DsScene();
        private int defaultMode;
        private int defaultSeed = DsConst.DefaultSeed;

        /// <summary>
        /// 两个方便复现的命令行开关（核心不认识它们，直接透传）：
        /// `--mode=N` 开局就停在某个场景，`--seed=N` 换一批数据。
        /// </summary>
        public DsDemoApp()
        {
            string[] args = Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length; i++)
            {
                int value;
                if (args[i].StartsWith("--mode=", StringComparison.Ordinal)
                    && int.TryParse(args[i].Substring(7), NumberStyles.Integer,
                                    CultureInfo.InvariantCulture, out value))
                {
                    defaultMode = value < 0 ? 0 : (value > 9 ? 9 : value);
                }
                else if (args[i].StartsWith("--seed=", StringComparison.Ordinal)
                         && int.TryParse(args[i].Substring(7), NumberStyles.Integer,
                                         CultureInfo.InvariantCulture, out value) && value != 0)
                {
                    defaultSeed = value;
                }
            }
        }

        public int CountDiagonalSegments(VisualSnapshot snapshot, int cw, int ch, List<string> report)
        {
            return inspector.CountDiagonalSegments(snapshot, cw, ch, report);
        }

        public int CheckCellText(VisualSnapshot snapshot, TextWriter writer)
        {
            return inspector.CheckCellText(snapshot, writer);
        }

        public string Name { get { return "数据结构演示"; } }
        public string[] ModeNames { get { return DsNames.Modes; } }

        public string[] OptionNames
        {
            get
            {
                string[] names = new string[DsNames.Modes.Length];
                for (int i = 0; i < names.Length; i++) names[i] = DsNames.Options[i][0];
                return names;
            }
        }

        public string[] Option2Names
        {
            get
            {
                string[] names = new string[DsNames.Modes.Length];
                for (int i = 0; i < names.Length; i++) names[i] = DsNames.Options2[i][0];
                return names;
            }
        }

        public string[] DisplayNames { get { return DsNames.DisplayNames; } }
        public string[] StatNames { get { return DsNames.StatNames; } }
        public int DefaultMode { get { return defaultMode; } }
        public int DefaultOption { get { return 0; } }
        public int DefaultOption2 { get { return 1; } }

        public IVisualEngine CreateEngine() { return new DsEngine(); }

        public IVisualScene CreateScene()
        {
            DsScene scene = new DsScene();
            scene.Seed = defaultSeed;
            return scene;
        }
        public string ReproduceCommand { get { return "DsDemo.exe --selftest"; } }

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
            int seed = DsConst.DefaultSeed;
            w.WriteLine("数据结构演示 --selftest（同进程，不开窗，不起子进程）");
            w.WriteLine("seed=" + seed + " elements=" + DsConst.Elements
                        + " canvas=" + DsConst.CanvasWidth + "x" + DsConst.CanvasHeight
                        + " modes=" + DsNames.Modes.Length + " core=" + CoreInfo.Version);
            w.WriteLine();

            // ── 场景 1 动态数组
            {
                w.WriteLine("场景 1 动态数组的增长");
                DsResult push = DsSelfTest.Run(0, 0, 1, seed);
                DsResult reserve32 = DsSelfTest.Run(0, 1, 1, seed);
                DsResult reserve8 = DsSelfTest.Run(0, 2, 1, seed);
                w.WriteLine("  " + SceneLine(0, "逐步 push（20 个元素）", push));
                w.WriteLine("  " + SceneLine(0, "先 reserve(32)", reserve32));
                w.WriteLine("  " + SceneLine(0, "先 reserve(8)", reserve8));
                pass &= SelfTestHost.Check(w, "array_grow_copies_31",
                    push.CustomA == 6 && push.CustomB == 31 && push.Primary == 32,
                    "扩容 " + push.CustomA + " 次、搬迁 " + push.CustomB + " 个、容量 " + push.Primary
                    + "（1+2+4+8+16＝31）");
                pass &= SelfTestHost.Check(w, "array_reserve_no_grow",
                    reserve32.CustomA == 0 && reserve32.CustomB == 0 && reserve32.Primary == 32,
                    "reserve(32) 后扩容 " + reserve32.CustomA + " 次、搬迁 " + reserve32.CustomB
                    + " 个、容量 " + reserve32.Primary);
                pass &= SelfTestHost.Check(w, "array_reserve8_grows_less",
                    reserve8.CustomA > 0 && reserve8.CustomA < push.CustomA,
                    "reserve(8) 扩容 " + reserve8.CustomA + " 次 < 逐步 push 的 " + push.CustomA + " 次");
                w.WriteLine();
            }

            // ── 场景 2 链表
            {
                w.WriteLine("场景 2 链表");
                DsResult list = DsSelfTest.Run(1, 0, 0, seed);
                w.WriteLine("  " + SceneLine(1, "有序插入 12 键 + 删两个头", list));
                pass &= SelfTestHost.Check(w, "list_pointer_writes_formula",
                    list.Secondary == 2 * list.Primary + list.CustomA,
                    "指针写入 " + list.Secondary + " = 2×插入 " + list.Primary + " + 删除 " + list.CustomA);
                w.WriteLine();
            }

            // ── 场景 3 哈希表
            {
                w.WriteLine("场景 3 哈希表");
                DsResult h8 = DsSelfTest.Run(2, 0, 0, seed);
                DsResult h16 = DsSelfTest.Run(2, 0, 1, seed);
                w.WriteLine("  " + SceneLine(2, "取模，初始 8 桶，24 键", h8));
                w.WriteLine("  " + SceneLine(2, "取模，初始 16 桶，24 键", h16));
                pass &= SelfTestHost.Check(w, "hash_rehash_keeps_load_under_limit",
                    h8.Secondary * 1000 <= h8.Primary * DsConst.LoadFactorMilli,
                    "8 桶起：键 " + h8.Secondary + " 落在 " + h8.Primary + " 桶里，负载因子 "
                    + (h8.Secondary * 1000 / h8.Primary) / 1000.0 + " ≤ 0.75");
                pass &= SelfTestHost.Check(w, "hash_rehash_twice_from_8",
                    h8.CustomA == 2 && h8.Primary == 32 && h16.CustomA == 1,
                    "8 桶起再哈希 " + h8.CustomA + " 次到 " + h8.Primary + " 桶；16 桶起 "
                    + h16.CustomA + " 次到 " + h16.Primary + " 桶");
                w.WriteLine();
            }

            // ── 场景 4 搜索树
            {
                w.WriteLine("场景 4 搜索树 → 平衡");
                DsResult bst = DsSelfTest.Run(3, 0, 1, seed);
                DsResult avl = DsSelfTest.Run(3, 1, 1, seed);
                DsResult rb = DsSelfTest.Run(3, 2, 1, seed);
                DsResult avlRand = DsSelfTest.Run(3, 1, 0, seed);
                w.WriteLine("  " + SceneLine(3, "朴素 BST，升序插入 16 键", bst));
                w.WriteLine("  " + SceneLine(3, "AVL，同一串键", avl));
                w.WriteLine("  " + SceneLine(3, "红黑树，同一串键", rb));
                w.WriteLine("  " + SceneLine(3, "AVL，随机序", avlRand));
                pass &= SelfTestHost.Check(w, "tree_bst_degenerates_avl_does_not",
                    bst.Primary == 16 && avl.Primary <= 6 && avl.Primary < bst.Primary,
                    "升序插入：BST 高 " + bst.Primary + "（＝键数 16，退化成链），AVL 高 " + avl.Primary);
                pass &= SelfTestHost.Check(w, "tree_avl_rotates_on_ascending",
                    avl.Secondary > 0 && rb.Secondary > 0,
                    "升序插入：AVL 旋转 " + avl.Secondary + " 次、红黑树旋转 " + rb.Secondary
                    + " 次、变色 " + rb.CustomA + " 次");
                w.WriteLine();
            }

            // ── 场景 5 堆
            {
                w.WriteLine("场景 5 堆");
                DsResult heap = DsSelfTest.Run(4, 0, 0, seed);
                w.WriteLine("  " + SceneLine(4, "15 个元素：建堆 → push 2 → pop 2", heap));
                bool heapOk = HeapPropertyHolds(heap.Snapshot);
                pass &= SelfTestHost.Check(w, "heap_property_holds", heapOk,
                    "逐个检查 父 ≤ 子（大小 " + heap.Primary + "）；数组行与树形用的是同一批元素");
                w.WriteLine();
            }

            // ── 场景 6 B 树
            {
                w.WriteLine("场景 6 B 树（阶 4，每节点最多 3 键）");
                DsResult ins = DsSelfTest.Run(5, 0, 2, seed);
                DsResult del = DsSelfTest.Run(5, 2, 2, seed);
                w.WriteLine("  " + SceneLine(5, "只插入 12 键", ins));
                w.WriteLine("  " + SceneLine(5, "插入后删到出现合并", del));
                string why = "";
                bool ok = BTreeInvariants(ins.Snapshot, out why);
                pass &= SelfTestHost.Check(w, "btree_invariants_after_insert", ok, why);
                ok = BTreeInvariants(del.Snapshot, out why);
                pass &= SelfTestHost.Check(w, "btree_invariants_after_delete", ok, why);
                pass &= SelfTestHost.Check(w, "btree_split_and_merge",
                    ins.Secondary > 0 && (del.CustomA > 0 || del.CustomB > 0),
                    "插入时分裂 " + ins.Secondary + " 次；删除时合并 " + del.CustomA
                    + " 次、借用 " + del.CustomB + " 次");
                w.WriteLine();
            }

            // ── 场景 7 trie
            {
                w.WriteLine("场景 7 trie：中文按码点与按字节的对照");
                DsResult point = DsSelfTest.Run(6, 0, 1, seed);
                DsResult byByte = DsSelfTest.Run(6, 1, 1, seed);
                w.WriteLine("  " + SceneLine(6, "中文词，按码点建", point));
                w.WriteLine("  " + SceneLine(6, "中文词，按 UTF-8 字节建", byByte));
                pass &= SelfTestHost.Check(w, "trie_cjk_byte_depth_is_three_times",
                    byByte.Secondary == 3 * point.Secondary && point.Secondary > 0,
                    "同一个两字词：按码点深 " + point.Secondary + " 层、按字节深 " + byByte.Secondary
                    + " 层（一个汉字三层）");
                pass &= SelfTestHost.Check(w, "trie_byte_build_has_more_nodes",
                    byByte.Primary > point.Primary,
                    "按字节建出 " + byByte.Primary + " 个节点，按码点只有 " + point.Primary + " 个");
                w.WriteLine();
            }

            // ── 场景 8 图
            {
                w.WriteLine("场景 8 图（八个顶点）");
                DsResult bfs = DsSelfTest.Run(7, 0, 0, seed);
                DsResult dfs = DsSelfTest.Run(7, 0, 1, seed);
                w.WriteLine("  " + SceneLine(7, "邻接矩阵 + BFS", bfs));
                w.WriteLine("  " + SceneLine(7, "邻接矩阵 + DFS", dfs));
                pass &= SelfTestHost.Check(w, "graph_traversal_visits_all",
                    bfs.Primary == 8 && dfs.Primary == 8,
                    "BFS 访问 " + bfs.Primary + " 个顶点、DFS 访问 " + dfs.Primary + " 个（生成树保证连通）");
                pass &= SelfTestHost.Check(w, "graph_edge_count",
                    bfs.Secondary == dfs.Secondary && bfs.Secondary >= 7,
                    "两种遍历看的是同一张图：" + bfs.Secondary + " 条边");
                w.WriteLine();
            }

            // ── 场景 9 并查集
            {
                w.WriteLine("场景 9 并查集：四种组合的压缩前后对比");
                DsResult none = DsSelfTest.Run(8, 0, 0, seed);
                DsResult rankOnly = DsSelfTest.Run(8, 1, 0, seed);
                DsResult zipOnly = DsSelfTest.Run(8, 2, 0, seed);
                DsResult both = DsSelfTest.Run(8, 3, 0, seed);
                w.WriteLine("  " + SceneLine(8, "什么都不加", none));
                w.WriteLine("  " + SceneLine(8, "只按秩合并", rankOnly));
                w.WriteLine("  " + SceneLine(8, "只路径压缩", zipOnly));
                w.WriteLine("  " + SceneLine(8, "按秩 + 路径压缩", both));
                pass &= SelfTestHost.Check(w, "dsu_no_compression_keeps_depth",
                    none.CustomB == none.CustomA,
                    "什么都不加：同一次 find 压缩前 " + none.CustomA + " 跳、压缩后仍 " + none.CustomB + " 跳");
                pass &= SelfTestHost.Check(w, "dsu_compression_flattens",
                    both.CustomB <= 1 && zipOnly.CustomB <= 1 && both.CustomB < both.CustomA,
                    "按秩 + 压缩：压缩前 " + both.CustomA + " 跳、压缩后 " + both.CustomB
                    + " 跳；只压缩：" + zipOnly.CustomA + " → " + zipOnly.CustomB);
                pass &= SelfTestHost.Check(w, "dsu_union_by_rank_shallower",
                    rankOnly.CustomA <= none.CustomA,
                    "只按秩：压缩前 " + rankOnly.CustomA + " 跳 ≤ 什么都不加的 " + none.CustomA + " 跳");
                w.WriteLine();
            }

            // ── 场景 10 排序
            {
                w.WriteLine("场景 10 排序过程（40 个元素）");
                int[] expected = new int[12];
                int at = 0;
                string[] algoName = { "冒泡", "插入", "选择", "快速" };
                int quickCompares = 0, bubbleCompares = 0;
                for (int algo = 0; algo < 4; algo++)
                {
                    for (int order = 0; order < 3; order++)
                    {
                        DsResult sort = DsSelfTest.Run(9, algo, order, seed);
                        expected[at++] = sort.Primary;
                        if (algo == 0 && order == 0) bubbleCompares = sort.Primary;
                        if (algo == 3 && order == 0) quickCompares = sort.Primary;
                        w.WriteLine("  " + SceneLine(9, algoName[algo] + "，"
                                    + (order == 0 ? "随机" : (order == 1 ? "近乎有序" : "逆序")), sort));
                        if (order == 0)
                        {
                            pass &= SelfTestHost.Check(w, "sort_" + algo + "_result_sorted",
                                IsSorted(sort.Snapshot), algoName[algo] + " 跑完以后元素从小到大");
                        }
                    }
                }
                bool allSorted = true;
                for (int algo = 0; algo < 4; algo++)
                {
                    for (int order = 0; order < 3; order++)
                    {
                        DsResult sort = DsSelfTest.Run(9, algo, order, seed);
                        if (!IsSorted(sort.Snapshot)) allSorted = false;
                    }
                }
                pass &= SelfTestHost.Check(w, "sort_all_twelve_runs_sorted", allSorted,
                    "四种算法 × 三种初始次序共 12 次运行，结果都从小到大");
                pass &= SelfTestHost.Check(w, "sort_quick_compares_less_than_bubble",
                    quickCompares < bubbleCompares,
                    "随机序：冒泡比较 " + bubbleCompares + " 次、快速比较 " + quickCompares + " 次");
                w.WriteLine();
            }

            // ── 元素预算与画布
            {
                w.WriteLine("元素预算（十个场景共用 256 个元素，结构区只有 128 个）");
                int worstMode = -1, worstIndex = -1;
                for (int mode = 0; mode < 10; mode++)
                {
                    VisualSnapshot s = DsSelfTest.RunMode(mode, 0, OptionFor(mode), seed);
                    int used = MaxUsedIndex(s);
                    w.WriteLine("  budget " + DsNames.Modes[mode]
                                + "：最大下标 " + used + "（结构区上界 " + (DsConst.StructCount - 1) + "）");
                    if (used > worstIndex) { worstIndex = used; worstMode = mode; }
                }
                pass &= SelfTestHost.Check(w, "elements_within_budget",
                    worstIndex < DsConst.StructCount,
                    "用得最多的场景 " + (worstMode + 1) + " 到下标 " + worstIndex + "，未越过 "
                    + DsConst.StructCount);
                w.WriteLine();
            }

            // ── 连线正交
            {
                List<string> report = new List<string>();
                int diagonals = inspector.CountDiagonalSegments(
                    DsSelfTest.RunMode(0, 0, 0, seed), 20, 20, report);
                for (int i = 0; i < report.Count; i++) SelfTestHost.Note(w, report[i]);
                pass &= SelfTestHost.Check(w, "lines_are_orthogonal", diagonals == 0,
                    "十个场景的连线逐段检查，斜段 " + diagonals + " 条（含一条人造斜线的反例）");
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
                    Color line = DsPalette.LineMain();
                    Color hot = DsPalette.LineHot();
                    Color halo = DsPalette.LineHalo();
                    w.WriteLine("palette " + name + " 画布=" + ThemePalette.ToHex(theme.Background)
                                + " 连线主色=" + ThemePalette.ToHex(line)
                                + " 描边=" + ThemePalette.ToHex(halo)
                                + " 高亮=" + ThemePalette.ToHex(hot));
                    byte[] codes = { Ds.Empty, Ds.Pending, Ds.Done, Ds.Result, Ds.Active, Ds.Marked,
                                     Ds.Blocked, Ds.Anchor, Ds.Data, Ds.Path, Ds.Ref, Ds.Cut };
                    double minText = 99, minLine = 99;
                    for (int i = 0; i < codes.Length; i++)
                    {
                        Color fill = DsPalette.Fill(codes[i]);
                        Color text = DsPalette.Text(codes[i]);
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
                                    + "  " + DsPalette.LegendName(codes[i]));
                    }
                    w.WriteLine("palette " + name + " min 文字=" + F(minText) + " 连线=" + F(minLine));
                    if (k == 0) { minTextLight = minText; minLineLight = minLine; }
                    else { minTextDark = minText; minLineDark = minLine; }
                }
                ThemeManager.Apply(ThemeKind.Auto);
                pass &= SelfTestHost.Check(w, "text_contrast_at_least_4_5",
                    minTextLight >= 4.5 && minTextDark >= 4.5,
                    "块上文字与块底色的对比度：浅色最低 " + F(minTextLight) + "、深色最低 " + F(minTextDark));
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
                    for (int mode = 0; mode < 10; mode++) DsSelfTest.Run(mode, 0, OptionFor(mode), seed);
                    watch.Stop();
                    double ms = watch.Elapsed.TotalMilliseconds;
                    if (ms < best) best = ms;
                    if (ms > worst) worst = ms;
                }
                w.WriteLine("perf 十个场景各跑到底一轮，重复 5 次：最快 " + F(best) + " ms、最慢 "
                            + F(worst) + " ms（仅作参考，随机器负载浮动）");
                w.WriteLine();
            }

            return pass;
        }

        private static int OptionFor(int mode)
        {
            return mode == 5 ? 2 : 0;
        }

        private static string F(double value)
        {
            return value.ToString("0.00", CultureInfo.InvariantCulture);
        }

        private static string SceneLine(int mode, string name, DsResult r)
        {
            return "scene " + (mode + 1) + " " + name + "  steps=" + r.Steps
                   + " primary=" + r.Primary + " secondary=" + r.Secondary
                   + " work=" + r.Work + " pending=" + r.Pending
                   + " customA=" + r.CustomA + " customB=" + r.CustomB
                   + " finished=" + (r.Finished ? 1 : 0);
        }

        /// <summary>堆不变式：父节点的值不大于两个孩子。</summary>
        private static bool HeapPropertyHolds(VisualSnapshot s)
        {
            int size = s.PrimaryMetric;
            for (int i = 0; i < size; i++)
            {
                int left = 2 * i + 1, right = 2 * i + 2;
                if (left < size && s.Attr0[i] > s.Attr0[left]) return false;
                if (right < size && s.Attr0[i] > s.Attr0[right]) return false;
            }
            return size > 0;
        }

        private static bool IsSorted(VisualSnapshot s)
        {
            int n = 0;
            while (n < 127 && s.Attr0[DsConst.InputBase + n] > 0) n++;
            for (int i = 1; i < n; i++)
            {
                if (s.Attr0[i - 1] > s.Attr0[i]) return false;
            }
            return n > 1;
        }

        /// <summary>B 树不变式：键升序、非根节点键数不少于 1、每个节点最多 3 键、所有叶子同深。</summary>
        private static bool BTreeInvariants(VisualSnapshot s, out string why)
        {
            why = "";
            int root = s.FocusIndex / 2;
            if (root < 0 || root >= 24) { why = "找不到根"; return false; }
            int leafDepth = -1, nodes = 0;
            for (int id = 0; id < 24; id++)
            {
                int element = id * 2;
                if (s.Tag[element] <= 0) continue;
                nodes++;
                int keys = s.Tag[element];
                if (keys > 3) { why = "节点 " + id + " 有 " + keys + " 个键，超过 3"; return false; }
                for (int k = 1; k < keys; k++)
                {
                    int previous = k == 1 ? s.Attr0[element] : s.Attr1[element];
                    int current = k == 1 ? s.Attr1[element] : s.Attr2[element];
                    if (previous >= current) { why = "节点 " + id + " 的键不是升序"; return false; }
                }
                if (s.Attr3[element] == 1)
                {
                    // Attr4 存的是父节点的**元素下标**，因此一路往上走就是层数
                    int depth = 0, walk = element;
                    int guard = 0;
                    while (walk >= 0 && guard++ < 24)
                    {
                        walk = s.Attr4[walk];
                        if (walk < 0) break;
                        depth++;
                    }
                    if (leafDepth < 0) leafDepth = depth;
                    else if (leafDepth != depth) { why = "叶子不在同一层：" + leafDepth + " 与 " + depth; return false; }
                }
            }
            why = "节点 " + nodes + " 个、全部叶子同深（" + leafDepth + "）、每个节点不超过 3 个键且键升序";
            return nodes > 0;
        }

        /// <summary>结构区里用到的最大下标，用来验证十个场景装得进 128 个元素。</summary>
        private static int MaxUsedIndex(VisualSnapshot s)
        {
            int worst = -1;
            for (int i = 0; i < DsConst.StructCount; i++)
            {
                if (s.State[i] != Ds.Empty) { worst = i; continue; }
                if (s.Attr0[i] != 0 || s.Attr1[i] != -1 || s.Attr2[i] != -1) worst = i;
            }
            return worst;
        }
    }

    // ───────────────────────────────────────────────────────────── 窗口

    /// <summary>
    /// 演示自己的几个按钮：重新生成、追加随机键、清空、换种子。
    /// 它们改的都是**场景数据**（运算进程那边的输入区），改完把命令写进邮箱发过去。
    /// </summary>
    internal sealed class DsForm : VisualFormBase
    {
        private TextBox boxSeed;

        public DsForm(DsDemoApp demo) : base(demo)
        {
        }

        protected override int BuildExtraControls(Panel host, int y, Font font)
        {
            Button regen = MakeButton("重新生成数据", 10, y, 166, 30, font, ButtonRegenClick);
            host.Controls.Add(regen);
            Button random = MakeButton("追加 5 个随机键", 184, y, 166, 30, font, ButtonRandomClick);
            host.Controls.Add(random);
            Button clear = MakeButton("清空结构", 10, y + 34, 166, 30, font, ButtonClearClick);
            host.Controls.Add(clear);

            Label caption = MakeLabel("种子", 178, y + 40, 40, font);
            host.Controls.Add(caption);
            boxSeed = new TextBox();
            boxSeed.Location = new Point(222, y + 37);
            boxSeed.Size = new Size(64, 24);
            boxSeed.Font = font;
            boxSeed.Text = DsConst.DefaultSeed.ToString(CultureInfo.InvariantCulture);
            boxSeed.ForeColor = ThemeManager.Palette.PanelText;
            boxSeed.BackColor = ThemeManager.Palette.Surface;
            host.Controls.Add(boxSeed);
            Button useSeed = MakeButton("生成", 290, y + 34, 60, 28, font, ButtonUseSeedClick);
            host.Controls.Add(useSeed);
            return y + 68;
        }

        private DsScene Target { get { return Scene as DsScene; } }

        private void Send(int op, int key, int target)
        {
            DsScene scene = Target;
            if (scene == null) return;
            scene.RequestCommand(op, key, target);
            Snapshot.Seed = Seed;
            AfterSceneEdited("已把改动发给运算进程，从头再跑一遍");
        }

        private void ButtonRegenClick(object sender, EventArgs e)
        {
            Send(Ds.CmdRegenerate, 0, -1);
        }

        private void ButtonRandomClick(object sender, EventArgs e)
        {
            Send(Ds.CmdAppendRandom, 5, -1);
        }

        private void ButtonClearClick(object sender, EventArgs e)
        {
            Send(Ds.CmdClear, 0, -1);
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
            DsScene scene = Target;
            if (scene != null) scene.Seed = value;
            boxSeed.Text = value.ToString(CultureInfo.InvariantCulture);
            Send(Ds.CmdRegenerate, 0, -1);
        }
    }

    // ───────────────────────────────────────────────────────────── 入口

    internal static class Program
    {
        [STAThread]
        private static int Main(string[] args)
        {
            DsDemoApp demo = new DsDemoApp();
            return VisualCoreMain.Run(args, demo, delegate { return new DsForm(demo); });
        }
    }
}
