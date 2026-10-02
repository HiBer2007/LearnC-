// VisualCore —— 交互式可视化演示的共享核心（本教材配套程序）
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
 *  VisualCore.cs —— 交互式可视化演示的共享核心（单一来源）
 *
 *  这套教材的演示程序共用同一个框架：界面进程负责画与收命令，运算进程负责推进，
 *  两者靠共享内存传状态、靠命名管道传命令。搜索演示用它，数据结构演示也用同一份。
 *
 *  编译：本文件不是独立程序，要和某个演示自己的 .cs 一起交给编译器。
 *        C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /nologo /target:winexe ^
 *          /out:Demo.exe /r:System.Windows.Forms.dll /r:System.Drawing.dll ^
 *          ..\_common\VisualCore.cs Demo.cs
 *        dotnet SDK 侧用 <Compile Include="..\_common\VisualCore.cs" />。
 *        细节见 演示工具/README.md。
 *
 *  语法：C# 5。.NET Framework 自带的 csc.exe 只支持到 C# 5，
 *        因此这里不用字符串内插、空条件运算符、表达式体成员等写法。
 *
 *  ── 核心提供什么 ────────────────────────────────────────────────────────
 *
 *  1. 两进程架构
 *       VisualCoreMain 按命令行分派三种身份：默认是界面进程，--worker 是运算进程，
 *       --selftest 不开窗口也不起子进程。WorkerHost 负责运算进程的主循环，
 *       RenderLink 负责界面进程这一侧的管道与共享内存。
 *       界面进程一退出，管道断开，运算进程的读循环立刻结束，不留孤儿。
 *
 *  2. 速率控制 SpeedControl
 *       单位是 sps（每秒推进多少步），对数刻度，从 0.5 到不限速，带预设档。
 *       不限速 = 每一片都推进到用掉时间预算为止，不等任何一帧。
 *       限速 = 按“目标 sps × 这一片经过的时间”发步数配额，余量留到下一片。
 *       SpsMeter 用真实经过的时间与步数算出实测 sps。
 *
 *  3. 时间线 Timeline
 *       暂停／继续／单步／重置／回放到第 n 步。回放靠引擎的确定性：
 *       重置之后连续推进 n 步，得到的状态与当初第 n 步时相同。
 *
 *  4. 操作与工具 BrushTool
 *       ToolDescriptor 描述一个工具（要不要刷子尺寸、要不要值），
 *       BrushState 记住当前工具、刷子半径与值，UndoStack 存场景自己的快照。
 *
 *  5. 场景抽象 IVisualScene / IVisualEngine
 *       核心只认“元素”：每个元素有状态、标记、标签三个字节和六个整数属性。
 *       元素摆在哪里、怎么画、工具怎么落笔，全部由场景决定；
 *       因此方格、节点、桶、树节点都能用同一套壳。
 *
 *  6. 绘制基类 VisualFormBase
 *       颜色语义表 ScenePalette（状态 → 颜色）、图例、元素上标数值、
 *       坐标与命中测试的调用顺序都在这里；场景只实现画自己那部分。
 *
 *  7. 统计面板
 *       步数、工作量、待处理数、主指标、次指标、实测 sps，加两种运算模式的对照。
 *
 *  8. 可复现与自测骨架
 *       DeterministicRng 给出与运行时无关的固定种子随机序列；
 *       SelfTestHost 负责 --selftest 的标准输出、断言打印与退出码；
 *       演示只要实现自己的引擎与断言，这一套白拿。
 * ==========================================================================*/

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32;

namespace VisualCore
{
    // ───────────────────────────────────────────────────────────── 核心信息

    public static class CoreInfo
    {
        /// <summary>核心版本。接口有增减时改这个号，演示的 README 要跟着写。</summary>
        public const string Version = "1.0";

        /// <summary>共享内存布局版本。布局一改就加一，免得新旧两个进程对着读。</summary>
        public const int LayoutVersion = 1;
    }

    /// <summary>
    /// xorshift32。自己实现而不直接用 System.Random，是因为 Random 的内部算法
    /// 在不同 .NET 版本上不同，同一颗种子未必给出同一串数；
    /// 这里每一步都由位运算定义，换机器、换运行时结果都一样。
    /// </summary>
    public sealed class DeterministicRng
    {
        private uint state;

        public DeterministicRng(uint seed)
        {
            state = seed == 0u ? 0x9E3779B9u : seed;
        }

        public uint NextUInt()
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        /// <summary>返回 [0, bound) 之间的整数。</summary>
        public int Next(int bound)
        {
            if (bound <= 1) return 0;
            return (int)(NextUInt() % (uint)bound);
        }
    }

    // ───────────────────────────────────────────────────────────── 元素状态与配色

    /// <summary>
    /// 通用的元素状态码。场景可以只用其中几个，也可以自己加含义，
    /// 但配色表与图例按这套码来组织，后来者照着填就行。
    /// </summary>
    public static class ElementStates
    {
        public const byte Empty = 0;      // 还没被处理过
        public const byte Pending = 1;    // 在待处理集合里
        public const byte Done = 2;       // 已经处理过
        public const byte Result = 3;     // 属于最终结果
        public const byte Active = 4;     // 当前正在处理的元素
        public const byte Marked = 5;     // 场景自定义的标记（例如旋转中、刚分裂）
        public const byte Blocked = 6;    // 不可用（例如墙、空洞）
    }

    // ───────────────────────────────────────────────────────────── 主题与配色

    /// <summary>界面上可选的三种主题设置。</summary>
    public enum ThemeKind { Light = 0, Dark = 1, Auto = 2 }

    /// <summary>真正生效的两套主题。跟随系统最终也会落到其中一个。</summary>
    public enum ThemeMode { Light = 0, Dark = 1 }

    /// <summary>
    /// 语义色槽。场景按槽声明颜色，两套主题各自给出取值，
    /// 因此换主题不必改场景代码，将来加演示也只要挑槽。
    /// </summary>
    public enum ThemeSlot
    {
        Empty = 0, Blocked, Pending, Done, Active, Result, Start, Goal,
        Terrain, Extra1, Extra2, Extra3
    }

    /// <summary>
    /// 元素上的形状或纹理线索。颜色之外再给一层区分，
    /// 色觉差异的读者与把界面打印成黑白时同样能读。
    /// </summary>
    public enum CueKind
    {
        None = 0,       // 不加线索
        CornerDot,      // 左上角一个小方点
        Border,         // 整格加粗边框
        Triangle,       // 左上角一个三角
        Circle,         // 左上角一个圆
        Cross           // 左上角一个叉
    }

    /// <summary>
    /// 一套主题的色表。两套都是手工配的，不是把另一套反相：
    /// 深色下不用纯黑底，浅色下不用纯白底，块上小字与块底色的对比度都留足。
    /// </summary>
    public sealed class ThemePalette
    {
        private readonly Color[] fills = new Color[16];
        private readonly Color[] texts = new Color[16];

        public Color Background;        // 画布底色
        public Color Surface;           // 图例与面板的底色
        public Color PanelBackground;   // 控制面板底色
        public Color PanelText;         // 面板正文
        public Color MutedText;         // 面板上的次要说明
        public Color GridLine;          // 网格线
        public Color CueColor;          // 形状线索的颜色

        public Color FillOf(ThemeSlot slot) { return fills[(int)slot]; }
        public Color TextOf(ThemeSlot slot) { return texts[(int)slot]; }

        private void Set(ThemeSlot slot, string fill, string text)
        {
            fills[(int)slot] = ParseHex(fill);
            texts[(int)slot] = ParseHex(text);
        }

        /// <summary>把 #RRGGBB 解析成颜色。核心不引别的依赖，自己写十几行。</summary>
        public static Color ParseHex(string hex)
        {
            if (hex.Length > 0 && hex[0] == '#') hex = hex.Substring(1);
            int value = int.Parse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture);
            return Color.FromArgb((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
        }

        public static string ToHex(Color color)
        {
            return "#" + color.R.ToString("X2", CultureInfo.InvariantCulture)
                       + color.G.ToString("X2", CultureInfo.InvariantCulture)
                       + color.B.ToString("X2", CultureInfo.InvariantCulture);
        }

        /// <summary>浅色主题：底子浅但不纯白，块上小字一律用深色。</summary>
        public static ThemePalette CreateLight()
        {
            ThemePalette palette = new ThemePalette();
            palette.Background = ParseHex("#F4F4F4");
            palette.Surface = ParseHex("#FAFAFA");
            palette.PanelBackground = ParseHex("#F7F7F7");
            palette.PanelText = ParseHex("#1A1A1A");
            palette.MutedText = ParseHex("#5F5F5F");
            palette.GridLine = ParseHex("#BDBDBD");
            palette.CueColor = ParseHex("#37474F");
            palette.Set(ThemeSlot.Empty, "#FFFFFF", "#1A1A1A");
            palette.Set(ThemeSlot.Terrain, "#F2E3C0", "#1A1A1A");
            palette.Set(ThemeSlot.Blocked, "#37474F", "#FFFFFF");
            palette.Set(ThemeSlot.Pending, "#FFECB3", "#1A1A1A");
            palette.Set(ThemeSlot.Done, "#90CAF9", "#1A1A1A");
            palette.Set(ThemeSlot.Active, "#BF360C", "#FFFFFF");
            palette.Set(ThemeSlot.Result, "#6A1B9A", "#FFFFFF");
            palette.Set(ThemeSlot.Start, "#1B5E20", "#FFFFFF");
            palette.Set(ThemeSlot.Goal, "#B71C1C", "#FFFFFF");
            palette.Set(ThemeSlot.Extra1, "#CFD8DC", "#1A1A1A");
            palette.Set(ThemeSlot.Extra2, "#D1C4E9", "#1A1A1A");
            palette.Set(ThemeSlot.Extra3, "#C8E6C9", "#1A1A1A");
            return palette;
        }

        /// <summary>深色主题：底子接近黑但不是纯黑，块上小字改用浅色。</summary>
        public static ThemePalette CreateDark()
        {
            ThemePalette palette = new ThemePalette();
            palette.Background = ParseHex("#1B1B1B");
            palette.Surface = ParseHex("#252525");
            palette.PanelBackground = ParseHex("#202020");
            palette.PanelText = ParseHex("#E8E8E8");
            palette.MutedText = ParseHex("#A8A8A8");
            palette.GridLine = ParseHex("#3C3C3C");
            palette.CueColor = ParseHex("#E8E8E8");
            palette.Set(ThemeSlot.Empty, "#2B2B2B", "#F0F0F0");
            palette.Set(ThemeSlot.Terrain, "#4A3F2A", "#F0F0F0");
            palette.Set(ThemeSlot.Blocked, "#101418", "#E0E0E0");
            palette.Set(ThemeSlot.Pending, "#6D5200", "#FFE9A8");
            palette.Set(ThemeSlot.Done, "#14405C", "#BBDEFB");
            palette.Set(ThemeSlot.Active, "#FF8F00", "#241A00");
            palette.Set(ThemeSlot.Result, "#CE93D8", "#2A0033");
            palette.Set(ThemeSlot.Start, "#66BB6A", "#06280A");
            palette.Set(ThemeSlot.Goal, "#EF5350", "#2B0000");
            palette.Set(ThemeSlot.Extra1, "#37474F", "#ECEFF1");
            palette.Set(ThemeSlot.Extra2, "#4A3F6B", "#EDE7F6");
            palette.Set(ThemeSlot.Extra3, "#2E5339", "#E8F5E9");
            return palette;
        }
    }

    /// <summary>
    /// 主题的解析与切换。选择只记在本进程里，不写注册表、不落磁盘。
    /// 「跟随系统」读的是 HKCU 下 Themes\Personalize 的 AppsUseLightTheme：
    /// 0 表示系统用深色，1 表示浅色（本机实测该值存在且为 0）。
    /// </summary>
    public static class ThemeManager
    {
        public const string PersonalizeKey = @"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize";
        public const string LightThemeValue = "AppsUseLightTheme";

        private static readonly ThemePalette light = ThemePalette.CreateLight();
        private static readonly ThemePalette dark = ThemePalette.CreateDark();

        public static ThemeKind Requested = ThemeKind.Auto;
        public static ThemeMode Current = ThemeMode.Light;

        /// <summary>切换主题设置，返回解析后的实际主题。</summary>
        public static ThemeMode Apply(ThemeKind kind)
        {
            Requested = kind;
            Current = Resolve(kind);
            return Current;
        }

        public static ThemeMode Resolve(ThemeKind kind)
        {
            if (kind == ThemeKind.Light) return ThemeMode.Light;
            if (kind == ThemeKind.Dark) return ThemeMode.Dark;
            return SystemPrefersDark() ? ThemeMode.Dark : ThemeMode.Light;
        }

        public static ThemePalette Palette
        {
            get { return Current == ThemeMode.Dark ? dark : light; }
        }

        /// <summary>读系统设置。读不到就按浅色处理，不抛异常出去。</summary>
        public static bool SystemPrefersDark()
        {
            try
            {
                using (RegistryKey key = Registry.CurrentUser.OpenSubKey(PersonalizeKey))
                {
                    if (key == null) return false;
                    object value = key.GetValue(LightThemeValue);
                    if (value == null) return false;
                    return Convert.ToInt32(value, CultureInfo.InvariantCulture) == 0;
                }
            }
            catch (Exception)
            {
                return false;
            }
        }

        /// <summary>解析命令行里的 --theme light|dark|auto，默认跟随系统。</summary>
        public static bool TryParseKind(string text, out ThemeKind kind)
        {
            kind = ThemeKind.Auto;
            if (string.Equals(text, "light", StringComparison.OrdinalIgnoreCase)) { kind = ThemeKind.Light; return true; }
            if (string.Equals(text, "dark", StringComparison.OrdinalIgnoreCase)) { kind = ThemeKind.Dark; return true; }
            if (string.Equals(text, "auto", StringComparison.OrdinalIgnoreCase)) { kind = ThemeKind.Auto; return true; }
            return false;
        }

        public static string NameOf(ThemeKind kind)
        {
            if (kind == ThemeKind.Light) return "浅色";
            if (kind == ThemeKind.Dark) return "深色";
            return "跟随系统";
        }

        /// <summary>WCAG 的相对亮度。</summary>
        public static double RelativeLuminance(Color color)
        {
            return 0.2126 * Channel(color.R) + 0.7152 * Channel(color.G) + 0.0722 * Channel(color.B);
        }

        private static double Channel(int value)
        {
            double c = value / 255.0;
            return c <= 0.03928 ? c / 12.92 : Math.Pow((c + 0.055) / 1.055, 2.4);
        }

        /// <summary>两色的对比度，公式见 WCAG 2.x：(L1 + 0.05) / (L2 + 0.05)。</summary>
        public static double ContrastRatio(Color a, Color b)
        {
            double la = RelativeLuminance(a);
            double lb = RelativeLuminance(b);
            double hi = Math.Max(la, lb);
            double lo = Math.Min(la, lb);
            return (hi + 0.05) / (lo + 0.05);
        }
    }

    /// <summary>一种状态在界面上的名字、语义色槽与形状线索。</summary>
    public sealed class StateStyle
    {
        public byte Code;
        public string Name;
        public ThemeSlot Slot;
        public CueKind Cue;

        public StateStyle(byte code, string name, ThemeSlot slot, CueKind cue)
        {
            Code = code;
            Name = name;
            Slot = slot;
            Cue = cue;
        }
    }

    /// <summary>
    /// 颜色语义表：状态码到语义色槽，另加几个特殊标记与「只进图例」的条目。
    /// 具体颜色由当前主题给，场景不写死色值，因此换主题不必改场景。
    /// 图例直接照着这张表画，形状线索也一并画在色块上。
    /// </summary>
    public sealed class ScenePalette
    {
        public readonly List<StateStyle> States = new List<StateStyle>();
        public readonly List<StateStyle> Markers = new List<StateStyle>();
        public readonly List<StateStyle> Extra = new List<StateStyle>();

        public ScenePalette AddState(byte code, string name, ThemeSlot slot)
        {
            return AddState(code, name, slot, CueKind.None);
        }

        public ScenePalette AddState(byte code, string name, ThemeSlot slot, CueKind cue)
        {
            States.Add(new StateStyle(code, name, slot, cue));
            return this;
        }

        /// <summary>特殊标记：起止元素、选中元素这类，按元素记录里的标记字节取色。</summary>
        public ScenePalette AddMarker(byte marker, string name, ThemeSlot slot, CueKind cue)
        {
            Markers.Add(new StateStyle(marker, name, slot, cue));
            return this;
        }

        /// <summary>
        /// 只进图例、不参与取色的条目。场景里有些颜色不是由状态码决定的，
        /// 例如“同一状态但数值不同的格子换一种底色”，把这种颜色登记在这里，图例才完整。
        /// </summary>
        public ScenePalette AddExtra(string name, ThemeSlot slot, CueKind cue)
        {
            Extra.Add(new StateStyle(0, name, slot, cue));
            return this;
        }

        public StateStyle FindState(byte code)
        {
            for (int i = 0; i < States.Count; i++)
            {
                if (States[i].Code == code) return States[i];
            }
            return null;
        }

        public StateStyle FindMarker(byte marker)
        {
            for (int i = 0; i < Markers.Count; i++)
            {
                if (Markers[i].Code == marker) return Markers[i];
            }
            return null;
        }

        public Color FillOf(byte code)
        {
            StateStyle style = FindState(code);
            return style == null ? ThemeManager.Palette.Background : ThemeManager.Palette.FillOf(style.Slot);
        }

        public Color TextOf(byte code)
        {
            StateStyle style = FindState(code);
            return style == null ? ThemeManager.Palette.PanelText : ThemeManager.Palette.TextOf(style.Slot);
        }
    }

    // ───────────────────────────────────────────────────────────── 一帧快照

    /// <summary>
    /// 一帧的全部可视数据。界面进程拿它画图，运算进程拿它发布，
    /// 两边看到的是同一个结构，因此两种运算模式的画面必然一致。
    ///
    /// 头部那些整数字段的语义由演示定义（核心只负责搬运与显示），
    /// 建议的用法写在每个字段的注释里。
    /// </summary>
    public sealed class VisualSnapshot
    {
        public readonly int Elements;
        public readonly byte[] State;
        public readonly byte[] Marker;
        public readonly byte[] Tag;
        public readonly int[] Attr0;
        public readonly int[] Attr1;
        public readonly int[] Attr2;
        public readonly int[] Attr3;
        public readonly int[] Attr4;
        public readonly int[] Attr5;

        public readonly object Sync = new object();

        public int Seq = -1;
        public int StateCode;        // 整体状态：由演示定义，配合 IVisualDemo.DescribeState
        public int Steps;            // 已推进步数
        public int WorkCount;        // 工作量：搜索是已展开节点数，树是旋转次数之类
        public int PendingCount;     // 待处理集合的大小
        public int PrimaryMetric;    // 主指标：搜索是路径代价
        public int SecondaryMetric;  // 次指标：搜索是路径长度
        public int CurrentIndex;     // 当前正在处理的元素，-1 表示没有
        public int FocusIndex;       // 关注的元素：搜索的起点、树的根
        public int ModeKind;         // 模式编号
        public int OptionA;          // 选项一
        public int OptionB;          // 选项二
        public int OptionC;          // 选项三
        public int Seed;             // 固定种子，界面上要显示
        public int CustomA;          // 演示自定义
        public int CustomB;          // 演示自定义
        public int Finished;         // 0 未结束，1 已结束
        public int HasResult;        // 0 没结果，1 有结果
        public int Running;          // 0 停，1 正在推进
        public double MeasuredSps;   // 运算进程实测 sps

        public VisualSnapshot(int elements)
        {
            Elements = elements;
            State = new byte[elements];
            Marker = new byte[elements];
            Tag = new byte[elements];
            Attr0 = new int[elements];
            Attr1 = new int[elements];
            Attr2 = new int[elements];
            Attr3 = new int[elements];
            Attr4 = new int[elements];
            Attr5 = new int[elements];
            ClearRunState();
        }

        /// <summary>清掉运行状态，只留场景数据（Attr0 里通常放的就是场景数据）。</summary>
        public void ClearRunState()
        {
            for (int i = 0; i < Elements; i++)
            {
                State[i] = ElementStates.Empty;
                Marker[i] = 0;
                Tag[i] = 0;
                Attr1[i] = -1;
                Attr2[i] = -1;
                Attr3[i] = -1;
                Attr4[i] = 0;
                Attr5[i] = 0;
            }
            StateCode = 0;
            Steps = 0;
            WorkCount = 0;
            PendingCount = 0;
            PrimaryMetric = -1;
            SecondaryMetric = -1;
            CurrentIndex = -1;
            MeasuredSps = 0;
            Finished = 0;
            HasResult = 0;
            Running = 0;
        }
    }

    // ───────────────────────────────────────────────────────────── 共享内存

    /// <summary>
    /// 两个进程共用的定长内存块。运算进程写，界面进程按帧读，
    /// 每格（元素）的状态与属性都在这里，不必逐步走管道搬数据。
    ///
    /// `Text`
    ///
    /// ```text
    /// 偏移   类型              数量        内容
    /// 0      int32             1           布局版本号
    /// 4      int32             1           序号 seq，运算进程每发布一帧加一
    /// 8      int32             1           元素个数
    /// 12     int32             1           整体状态码
    /// 16     int32             1           已推进步数
    /// 20     int32             1           工作量
    /// 24     int32             1           待处理集合大小
    /// 28     int32             1           主指标
    /// 32     int32             1           次指标
    /// 36     int32             1           当前元素
    /// 40     int32             1           关注元素
    /// 44     int32             1           模式编号
    /// 48     int32             1           选项一
    /// 52     int32             1           选项二
    /// 56     int32             1           选项三
    /// 60     int32             1           固定种子
    /// 64     int32             1           演示自定义 A
    /// 68     int32             1           演示自定义 B
    /// 72     int32             1           目标 sps × 1000，0 表示不限速
    /// 76     int32             1           保留
    /// 80     double            1           实测 sps
    /// 88     int32             1           是否已结束
    /// 92     int32             1           是否有结果
    /// 96     int32             1           是否正在推进
    /// 100    int32             1           保留
    /// 104    int32             1           保留
    /// 108    int32             1           保留
    /// 112    byte[E]                     每元素状态
    /// 112+E  byte[E]                     每元素标记
    /// 112+2E byte[E]                     每元素标签
    /// 112+3E int32[E]                    每元素属性 0
    /// ...    int32[E]                    属性 1 到属性 5
    /// ```
    /// </summary>
    public static class VisualLayout
    {
        public const int HeaderBytes = 112;

        public const int OffsetLayoutVersion = 0;
        public const int OffsetSeq = 4;
        public const int OffsetElementCount = 8;
        public const int OffsetStateCode = 12;
        public const int OffsetSteps = 16;
        public const int OffsetWorkCount = 20;
        public const int OffsetPendingCount = 24;
        public const int OffsetPrimaryMetric = 28;
        public const int OffsetSecondaryMetric = 32;
        public const int OffsetCurrentIndex = 36;
        public const int OffsetFocusIndex = 40;
        public const int OffsetModeKind = 44;
        public const int OffsetOptionA = 48;
        public const int OffsetOptionB = 52;
        public const int OffsetOptionC = 56;
        public const int OffsetSeed = 60;
        public const int OffsetCustomA = 64;
        public const int OffsetCustomB = 68;
        public const int OffsetTargetSpsMilli = 72;
        public const int OffsetReserved = 76;
        public const int OffsetMeasuredSps = 80;
        public const int OffsetFinished = 88;
        public const int OffsetHasResult = 92;
        public const int OffsetRunning = 96;

        public static int StateOffset(int elements) { return HeaderBytes; }
        public static int MarkerOffset(int elements) { return HeaderBytes + elements; }
        public static int TagOffset(int elements) { return HeaderBytes + elements * 2; }
        public static int AttrOffset(int elements, int slot) { return HeaderBytes + elements * 3 + elements * 4 * slot; }
        public static int TotalBytes(int elements) { return HeaderBytes + elements * 3 + elements * 4 * 6; }
    }

    /// <summary>运算进程这一侧的共享内存写入端。</summary>
    public sealed class SharedStateWriter : IDisposable
    {
        private readonly MemoryMappedFile file;
        private readonly MemoryMappedViewAccessor accessor;
        private readonly int elements;
        private readonly byte[] lastState;
        private readonly byte[] lastMarker;
        private readonly byte[] lastTag;
        private readonly int[][] lastAttr;
        private int seq;

        public SharedStateWriter(string name, int elements)
        {
            this.elements = elements;
            file = MemoryMappedFile.CreateOrOpen(name, VisualLayout.TotalBytes(elements));
            accessor = file.CreateViewAccessor(0, VisualLayout.TotalBytes(elements));
            lastState = new byte[elements];
            lastMarker = new byte[elements];
            lastTag = new byte[elements];
            lastAttr = new int[6][];
            for (int slot = 0; slot < 6; slot++)
            {
                lastAttr[slot] = new int[elements];
                for (int i = 0; i < elements; i++) lastAttr[slot][i] = int.MinValue;
            }
            accessor.Write(VisualLayout.OffsetLayoutVersion, CoreInfo.LayoutVersion);
            accessor.Write(VisualLayout.OffsetElementCount, elements);
        }

        /// <summary>整块写场景数据（元素属性与标记），界面进程改过场景之后调用。</summary>
        public void WriteScene(VisualSnapshot snapshot)
        {
            accessor.WriteArray<byte>(VisualLayout.MarkerOffset(elements), snapshot.Marker, 0, elements);
            accessor.WriteArray<byte>(VisualLayout.TagOffset(elements), snapshot.Tag, 0, elements);
            for (int slot = 0; slot < 6; slot++)
            {
                accessor.WriteArray<int>(VisualLayout.AttrOffset(elements, slot), AttrOf(snapshot, slot), 0, elements);
            }
            accessor.Write(VisualLayout.OffsetSeed, snapshot.Seed);
            accessor.Write(VisualLayout.OffsetCustomA, snapshot.CustomA);
            accessor.Write(VisualLayout.OffsetCustomB, snapshot.CustomB);
            Array.Copy(snapshot.Marker, lastMarker, elements);
            Array.Copy(snapshot.Tag, lastTag, elements);
            for (int slot = 0; slot < 6; slot++)
            {
                Array.Copy(AttrOf(snapshot, slot), lastAttr[slot], elements);
            }
        }

        /// <summary>读回整帧。运算进程收到“场景变了”的命令之后用它。</summary>
        public void ReadInto(VisualSnapshot snapshot)
        {
            accessor.ReadArray<byte>(VisualLayout.StateOffset(elements), snapshot.State, 0, elements);
            accessor.ReadArray<byte>(VisualLayout.MarkerOffset(elements), snapshot.Marker, 0, elements);
            accessor.ReadArray<byte>(VisualLayout.TagOffset(elements), snapshot.Tag, 0, elements);
            for (int slot = 0; slot < 6; slot++)
            {
                accessor.ReadArray<int>(VisualLayout.AttrOffset(elements, slot), AttrOf(snapshot, slot), 0, elements);
            }
            snapshot.Seed = accessor.ReadInt32(VisualLayout.OffsetSeed);
            snapshot.CustomA = accessor.ReadInt32(VisualLayout.OffsetCustomA);
            snapshot.CustomB = accessor.ReadInt32(VisualLayout.OffsetCustomB);
        }

        /// <summary>把每元素的运行状态清零，并让下一次发布全量写出。</summary>
        public void ClearRunState()
        {
            accessor.WriteArray<byte>(VisualLayout.StateOffset(elements), new byte[elements], 0, elements);
            int[] minus = new int[elements];
            for (int i = 0; i < elements; i++) minus[i] = -1;
            accessor.WriteArray<int>(VisualLayout.AttrOffset(elements, 1), minus, 0, elements);
            accessor.WriteArray<int>(VisualLayout.AttrOffset(elements, 2), minus, 0, elements);
            accessor.WriteArray<int>(VisualLayout.AttrOffset(elements, 3), minus, 0, elements);
            accessor.WriteArray<int>(VisualLayout.AttrOffset(elements, 4), new int[elements], 0, elements);
            accessor.WriteArray<int>(VisualLayout.AttrOffset(elements, 5), new int[elements], 0, elements);
            for (int i = 0; i < elements; i++)
            {
                lastState[i] = 0;
                lastAttr[1][i] = -1;
                lastAttr[2][i] = -1;
                lastAttr[3][i] = -1;
                lastAttr[4][i] = 0;
                lastAttr[5][i] = 0;
            }
        }

        /// <summary>
        /// 发布一帧：每元素的状态与属性只写变化过的，头部字段每次全写，
        /// 最后把序号加一，界面进程据此判断要不要重画。
        /// </summary>
        public void Publish(VisualSnapshot snapshot)
        {
            for (int i = 0; i < elements; i++)
            {
                byte value = snapshot.State[i];
                if (value != lastState[i])
                {
                    accessor.Write(VisualLayout.StateOffset(elements) + i, value);
                    lastState[i] = value;
                }
                value = snapshot.Marker[i];
                if (value != lastMarker[i])
                {
                    accessor.Write(VisualLayout.MarkerOffset(elements) + i, value);
                    lastMarker[i] = value;
                }
                value = snapshot.Tag[i];
                if (value != lastTag[i])
                {
                    accessor.Write(VisualLayout.TagOffset(elements) + i, value);
                    lastTag[i] = value;
                }
                for (int slot = 0; slot < 6; slot++)
                {
                    int number = AttrOf(snapshot, slot)[i];
                    if (number != lastAttr[slot][i])
                    {
                        accessor.Write(VisualLayout.AttrOffset(elements, slot) + i * 4, number);
                        lastAttr[slot][i] = number;
                    }
                }
            }

            accessor.Write(VisualLayout.OffsetStateCode, snapshot.StateCode);
            accessor.Write(VisualLayout.OffsetSteps, snapshot.Steps);
            accessor.Write(VisualLayout.OffsetWorkCount, snapshot.WorkCount);
            accessor.Write(VisualLayout.OffsetPendingCount, snapshot.PendingCount);
            accessor.Write(VisualLayout.OffsetPrimaryMetric, snapshot.PrimaryMetric);
            accessor.Write(VisualLayout.OffsetSecondaryMetric, snapshot.SecondaryMetric);
            accessor.Write(VisualLayout.OffsetCurrentIndex, snapshot.CurrentIndex);
            accessor.Write(VisualLayout.OffsetFocusIndex, snapshot.FocusIndex);
            accessor.Write(VisualLayout.OffsetModeKind, snapshot.ModeKind);
            accessor.Write(VisualLayout.OffsetOptionA, snapshot.OptionA);
            accessor.Write(VisualLayout.OffsetOptionB, snapshot.OptionB);
            accessor.Write(VisualLayout.OffsetOptionC, snapshot.OptionC);
            accessor.Write(VisualLayout.OffsetSeed, snapshot.Seed);
            accessor.Write(VisualLayout.OffsetCustomA, snapshot.CustomA);
            accessor.Write(VisualLayout.OffsetCustomB, snapshot.CustomB);
            accessor.Write(VisualLayout.OffsetMeasuredSps, snapshot.MeasuredSps);
            accessor.Write(VisualLayout.OffsetFinished, snapshot.Finished);
            accessor.Write(VisualLayout.OffsetHasResult, snapshot.HasResult);
            accessor.Write(VisualLayout.OffsetRunning, snapshot.Running);
            seq++;
            accessor.Write(VisualLayout.OffsetSeq, seq);
        }

        public void SetTargetSpsMilli(int milli)
        {
            accessor.Write(VisualLayout.OffsetTargetSpsMilli, milli);
        }

        public void Dispose()
        {
            accessor.Dispose();
            file.Dispose();
        }

        private static int[] AttrOf(VisualSnapshot snapshot, int slot)
        {
            switch (slot)
            {
                case 0: return snapshot.Attr0;
                case 1: return snapshot.Attr1;
                case 2: return snapshot.Attr2;
                case 3: return snapshot.Attr3;
                case 4: return snapshot.Attr4;
                default: return snapshot.Attr5;
            }
        }
    }

    /// <summary>界面进程这一侧的共享内存读取端。</summary>
    public sealed class SharedStateReader : IDisposable
    {
        private readonly MemoryMappedFile file;
        private readonly MemoryMappedViewAccessor accessor;
        private readonly int elements;

        public SharedStateReader(string name, int elements)
        {
            this.elements = elements;
            file = MemoryMappedFile.OpenExisting(name);
            accessor = file.CreateViewAccessor(0, VisualLayout.TotalBytes(elements));
        }

        public int LayoutVersion { get { return accessor.ReadInt32(VisualLayout.OffsetLayoutVersion); } }

        /// <summary>序号变了才整帧读一遍，没变就什么都不做。</summary>
        public bool TryRead(VisualSnapshot snapshot)
        {
            int seq = accessor.ReadInt32(VisualLayout.OffsetSeq);
            if (seq == snapshot.Seq) return false;

            lock (snapshot.Sync)
            {
                snapshot.Seq = seq;
                snapshot.StateCode = accessor.ReadInt32(VisualLayout.OffsetStateCode);
                snapshot.Steps = accessor.ReadInt32(VisualLayout.OffsetSteps);
                snapshot.WorkCount = accessor.ReadInt32(VisualLayout.OffsetWorkCount);
                snapshot.PendingCount = accessor.ReadInt32(VisualLayout.OffsetPendingCount);
                snapshot.PrimaryMetric = accessor.ReadInt32(VisualLayout.OffsetPrimaryMetric);
                snapshot.SecondaryMetric = accessor.ReadInt32(VisualLayout.OffsetSecondaryMetric);
                snapshot.CurrentIndex = accessor.ReadInt32(VisualLayout.OffsetCurrentIndex);
                snapshot.FocusIndex = accessor.ReadInt32(VisualLayout.OffsetFocusIndex);
                snapshot.ModeKind = accessor.ReadInt32(VisualLayout.OffsetModeKind);
                snapshot.OptionA = accessor.ReadInt32(VisualLayout.OffsetOptionA);
                snapshot.OptionB = accessor.ReadInt32(VisualLayout.OffsetOptionB);
                snapshot.OptionC = accessor.ReadInt32(VisualLayout.OffsetOptionC);
                snapshot.Seed = accessor.ReadInt32(VisualLayout.OffsetSeed);
                snapshot.CustomA = accessor.ReadInt32(VisualLayout.OffsetCustomA);
                snapshot.CustomB = accessor.ReadInt32(VisualLayout.OffsetCustomB);
                snapshot.MeasuredSps = accessor.ReadDouble(VisualLayout.OffsetMeasuredSps);
                snapshot.Finished = accessor.ReadInt32(VisualLayout.OffsetFinished);
                snapshot.HasResult = accessor.ReadInt32(VisualLayout.OffsetHasResult);
                snapshot.Running = accessor.ReadInt32(VisualLayout.OffsetRunning);
                accessor.ReadArray<byte>(VisualLayout.StateOffset(elements), snapshot.State, 0, elements);
                accessor.ReadArray<byte>(VisualLayout.MarkerOffset(elements), snapshot.Marker, 0, elements);
                accessor.ReadArray<byte>(VisualLayout.TagOffset(elements), snapshot.Tag, 0, elements);
                for (int slot = 0; slot < 6; slot++)
                {
                    accessor.ReadArray<int>(VisualLayout.AttrOffset(elements, slot), AttrOf(snapshot, slot), 0, elements);
                }
            }
            return true;
        }

        public void Dispose()
        {
            accessor.Dispose();
            file.Dispose();
        }

        private static int[] AttrOf(VisualSnapshot snapshot, int slot)
        {
            switch (slot)
            {
                case 0: return snapshot.Attr0;
                case 1: return snapshot.Attr1;
                case 2: return snapshot.Attr2;
                case 3: return snapshot.Attr3;
                case 4: return snapshot.Attr4;
                default: return snapshot.Attr5;
            }
        }
    }

    // ───────────────────────────────────────────────────────────── 速率控制

    /// <summary>
    /// sps（每秒推进多少步）的刻度换算、预设档与时间预算推进。
    /// 对数刻度：0.5 到 50000 步每秒铺在滑块上，滑块顶端是不限速。
    /// </summary>
    public static class SpeedControl
    {
        public const double MinSps = 0.5;
        public const double MaxFiniteSps = 50000.0;
        public const double DefaultBudgetMs = 12.0;

        public static readonly double[] PresetSps =
        {
            0.5, 1.0, 10.0, 100.0, 1000.0, 10000.0, double.PositiveInfinity
        };

        public static readonly string[] PresetNames = { "0.5", "1", "10", "100", "1k", "10k", "不限速" };

        public static double SpsForPosition(int position)
        {
            if (position >= 1000) return double.PositiveInfinity;
            return MinSps * Math.Pow(MaxFiniteSps / MinSps, position / 999.0);
        }

        public static int PositionForSps(double sps)
        {
            if (double.IsPositiveInfinity(sps) || sps >= MaxFiniteSps) return 1000;
            if (sps <= MinSps) return 0;
            return (int)Math.Round(999.0 * Math.Log(sps / MinSps) / Math.Log(MaxFiniteSps / MinSps));
        }

        public static string FormatSps(double sps)
        {
            if (double.IsPositiveInfinity(sps)) return "不限速";
            if (sps >= 1000) return (sps / 1000.0).ToString("0.#", CultureInfo.InvariantCulture) + "k";
            if (sps >= 10) return sps.ToString("0", CultureInfo.InvariantCulture);
            return sps.ToString("0.#", CultureInfo.InvariantCulture);
        }

        public static string FormatMeasured(double sps)
        {
            if (sps <= 0) return "-";
            if (sps >= 1000) return sps.ToString("N0", CultureInfo.InvariantCulture);
            if (sps >= 10) return sps.ToString("0.0", CultureInfo.InvariantCulture);
            return sps.ToString("0.00", CultureInfo.InvariantCulture);
        }

        /// <summary>目标 sps 转成“毫 sps”，0 表示不限速。走管道就用这个整数。</summary>
        public static int ToMilli(double sps)
        {
            if (double.IsPositiveInfinity(sps)) return 0;
            return (int)Math.Round(sps * 1000.0);
        }

        public static double FromMilli(int milli)
        {
            return milli <= 0 ? double.PositiveInfinity : milli / 1000.0;
        }
    }

    /// <summary>用真实经过的时间与步数算实测 sps，暂停期间不计时。</summary>
    public sealed class SpsMeter
    {
        private readonly Stopwatch watch = new Stopwatch();
        private double stoppedSeconds;
        private int stepsAtStart;

        public double MeasuredSps;

        public void StartRun(int stepsNow)
        {
            stepsAtStart = stepsNow;
            stoppedSeconds = 0;
            watch.Reset();
            watch.Start();
            MeasuredSps = 0;
        }

        public void Pause()
        {
            if (watch.IsRunning)
            {
                stoppedSeconds += watch.Elapsed.TotalSeconds;
                watch.Stop();
            }
        }

        public void Report(int stepsNow)
        {
            double seconds = stoppedSeconds + (watch.IsRunning ? watch.Elapsed.TotalSeconds : 0.0);
            int steps = stepsNow - stepsAtStart;
            // 阈值取得很小：不限速时整场搜索可能不到一毫秒就跑完，
            // 那时候也要给出实测值，否则界面上永远是个短横。
            MeasuredSps = (seconds > 0.0005 && steps > 0) ? steps / seconds : 0.0;
        }

        public void Reset()
        {
            watch.Reset();
            stoppedSeconds = 0;
            stepsAtStart = 0;
            MeasuredSps = 0;
        }
    }

    // ───────────────────────────────────────────────────────────── 时间线

    /// <summary>
    /// 暂停／继续／单步／重置／回放到第 n 步。
    /// 回放依赖引擎的确定性：重置之后连推 n 步，得到的就是当初第 n 步的状态。
    /// </summary>
    public static class Timeline
    {
        /// <summary>
        /// 回放到第 targetStep 步。返回实际推进的步数；
        /// 超过 maxSteps 或者用满时间预算就停下，免得界面被一次回放卡住。
        /// </summary>
        public static int ReplayTo(IVisualEngine engine, int targetStep, int maxSteps, double budgetMs)
        {
            if (engine == null) return 0;
            engine.ResetRun();
            int limit = targetStep < maxSteps ? targetStep : maxSteps;
            Stopwatch watch = Stopwatch.StartNew();
            int done = 0;
            while (done < limit && !engine.Finished)
            {
                engine.StepOnce();
                done++;
                if ((done & 255) == 0 && watch.Elapsed.TotalMilliseconds >= budgetMs) break;
            }
            return done;
        }
    }

    // ───────────────────────────────────────────────────────────── 工具与撤销

    /// <summary>一个工具：名字、要不要刷子尺寸、要不要值（代价、键值之类）。</summary>
    public sealed class ToolDescriptor
    {
        public int Id;
        public string Name;
        public bool UsesRadius;
        public bool UsesValue;

        public ToolDescriptor(int id, string name, bool usesRadius, bool usesValue)
        {
            Id = id;
            Name = name;
            UsesRadius = usesRadius;
            UsesValue = usesValue;
        }
    }

    /// <summary>当前选中的工具、刷子半径与值。</summary>
    public sealed class BrushState
    {
        public int ToolId;
        public int Radius;      // 0 表示 1x1，1 表示 3x3，2 表示 5x5
        public int Value = 1;

        public static readonly int[] RadiusSizes = { 1, 3, 5 };

        public static string RadiusName(int radius)
        {
            if (radius < 0 || radius >= RadiusSizes.Length) return "1x1";
            return RadiusSizes[radius] + "x" + RadiusSizes[radius];
        }
    }

    /// <summary>通用撤销栈，存的是场景自己的快照对象。</summary>
    public sealed class UndoStack
    {
        public const int MaxDepth = 64;
        private readonly List<object> items = new List<object>();

        public bool CanUndo { get { return items.Count > 0; } }
        public int Count { get { return items.Count; } }

        public void Push(object snapshot)
        {
            if (snapshot == null) return;
            if (items.Count >= MaxDepth) items.RemoveAt(0);
            items.Add(snapshot);
        }

        public object Pop()
        {
            if (items.Count == 0) return null;
            object item = items[items.Count - 1];
            items.RemoveAt(items.Count - 1);
            return item;
        }

        public void Clear()
        {
            items.Clear();
        }
    }

    // ───────────────────────────────────────────────────────────── 场景与引擎

    /// <summary>界面上一次画图要用到的选择状态。</summary>
    public sealed class SceneViewState
    {
        public int DisplayMode;     // 元素上标什么，由演示定义
        public int ToolId = -1;
        public int Radius;
        public int Value = 1;
    }

    /// <summary>
    /// 运算侧：核心只要求它能配置、能重置、能推进一步、能把内部状态摊成快照。
    /// 引擎可以有自己的数据结构，核心不假设元素之间是什么关系。
    /// </summary>
    public interface IVisualEngine
    {
        int ElementCount { get; }

        /// <summary>按界面上的三个下拉配置自己。参数语义由演示定义。</summary>
        void Configure(int mode, int optionA, int optionB);

        /// <summary>回到初始状态：场景数据与运行状态都重来。</summary>
        void ResetAll();

        /// <summary>只清运行状态，场景数据保留。</summary>
        void ResetRun();

        /// <summary>推进一步。</summary>
        void StepOnce();

        bool Finished { get; }
        bool HasResult { get; }

        /// <summary>把场景数据写进快照（属性 0 通常放场景数据）。</summary>
        void ExportScene(VisualSnapshot snapshot);

        /// <summary>从快照读回场景数据：界面进程改过之后走这里。</summary>
        void ImportScene(VisualSnapshot snapshot);

        /// <summary>把运行状态摊进快照。</summary>
        void RefreshSnapshot(VisualSnapshot snapshot);
    }

    /// <summary>
    /// 渲染侧：命中测试、绘制、元素上写什么字、工具怎么落笔、撤销。
    /// 元素摆在哪里完全由场景决定，核心不假设是方格。
    /// </summary>
    public interface IVisualScene
    {
        string Title { get; }
        int ElementCount { get; }
        Size CanvasSize { get; }
        ScenePalette Palette { get; }
        ToolDescriptor[] Tools { get; }
        int[] BrushSizes { get; }        // 空数组表示这个场景不用刷子
        bool UsesValue { get; }
        int MinValue { get; }
        int MaxValue { get; }
        string HintText { get; }

        /// <summary>坐标 → 元素号，-1 表示没命中。</summary>
        int HitTest(int x, int y);

        /// <summary>画整个场景。元素的状态、属性都在快照里。</summary>
        void Paint(Graphics g, VisualSnapshot snapshot, SceneViewState viewState);

        /// <summary>元素上写什么字，空串表示不写。</summary>
        string ElementText(int element, VisualSnapshot snapshot, SceneViewState viewState);

        /// <summary>落笔。first 表示这是本次拖动的第一下。返回场景是否被改动。</summary>
        bool ApplyTool(int toolId, int element, int radius, int value, bool erase, bool first);

        /// <summary>一次拖动开始：场景在这里记下自己的快照。</summary>
        void BeginGesture();

        /// <summary>一次拖动结束：有改动就把快照压进撤销栈，返回是否有改动。</summary>
        bool EndGesture();

        bool CanUndo { get; }
        bool Undo();

        /// <summary>把场景数据写进快照，交给运算进程。</summary>
        void ExportScene(VisualSnapshot snapshot);
    }

    /// <summary>场景自己管种子时实现这个接口，界面外壳会把种子取出来显示。</summary>
    public interface ISeedProvider
    {
        int Seed { get; }
    }

    /// <summary>一个演示：下拉里的名字、工厂方法、状态文案、自测。</summary>
    public interface IVisualDemo
    {
        string Name { get; }
        string[] ModeNames { get; }
        string[] OptionNames { get; }
        string[] Option2Names { get; }
        string[] DisplayNames { get; }
        string[] StatNames { get; }         // 统计面板的七个标题
        int DefaultMode { get; }
        int DefaultOption { get; }
        int DefaultOption2 { get; }

        IVisualEngine CreateEngine();
        IVisualScene CreateScene();

        /// <summary>状态栏文案：把快照里的整体状态码翻译成一句人话。</summary>
        string DescribeState(VisualSnapshot snapshot);

        /// <summary>自测：在同一个进程里跑完，逐行打印，返回是否全部通过。</summary>
        bool RunSelfTest(TextWriter writer);

        /// <summary>复现命令，显示在界面上也写进 README。</summary>
        string ReproduceCommand { get; }
    }

    // ───────────────────────────────────────────────────────────── 管道协议

    /// <summary>两个进程之间的命令行文。一行一条，参数用空格分开。</summary>
    public static class PipeProtocol
    {
        public const string Init = "INIT";       // INIT 模式 选项一 选项二 种子
        public const string Sync = "SYNC";       // 场景数据被界面进程改过
        public const string Config = "CONFIG";   // CONFIG 模式 选项一 选项二
        public const string Rate = "RATE";       // RATE 毫sps，0 表示不限速
        public const string Run = "RUN";
        public const string Pause = "PAUSE";
        public const string Step = "STEP";
        public const string Reset = "RESET";
        public const string Replay = "REPLAY";   // REPLAY 目标步数
        public const string Scene = "SCENE";     // 让运算进程把场景数据重新发布一遍
        public const string Quit = "QUIT";

        public static string Join(string command, params int[] values)
        {
            StringBuilder builder = new StringBuilder(command);
            for (int i = 0; i < values.Length; i++)
            {
                builder.Append(' ');
                builder.Append(values[i].ToString(CultureInfo.InvariantCulture));
            }
            return builder.ToString();
        }

        public static int Value(string[] parts, int index, int fallback)
        {
            if (index >= parts.Length) return fallback;
            int value;
            if (!int.TryParse(parts[index], NumberStyles.Integer, CultureInfo.InvariantCulture, out value))
                return fallback;
            return value;
        }
    }

    // ───────────────────────────────────────────────────────────── 运算进程

    /// <summary>临时诊断开关：--trace 时把运算进程每一片的状态写到临时文件。</summary>
    public static class WorkerTrace
    {
        public static bool Enabled;
        private static readonly object Sync = new object();
        private static readonly Stopwatch Clock = Stopwatch.StartNew();
        private static string path;

        public static void Log(string text)
        {
            if (!Enabled) return;
            lock (Sync)
            {
                if (path == null) path = Path.Combine(Path.GetTempPath(), "visualcore_trace.log");
                try
                {
                    File.AppendAllText(path, Clock.ElapsedMilliseconds.ToString(CultureInfo.InvariantCulture)
                                             + " ms  " + text + Environment.NewLine);
                }
                catch (IOException) { }
            }
        }
    }

    /// <summary>
    /// --worker 这一侧：不开窗口，只从管道收命令、按速率推进引擎、把状态写进共享内存。
    /// 界面进程一死，管道断开，这里的读循环立刻结束并退出，不会留下孤儿进程。
    /// </summary>
    public static class WorkerHost
    {
        public static int Run(int port, string shmName, int parentPid, IVisualEngine engine)
        {
            if (engine == null) return 2;

            TcpClient client = new TcpClient();
            WorkerTrace.Log("worker start, connecting to port " + port);
            try
            {
                client.Connect(IPAddress.Loopback, port);
                client.NoDelay = true;
            }
            catch (SocketException ex)
            {
                WorkerTrace.Log("connect failed " + ex.Message);
                return 2;
            }
            WorkerTrace.Log("connected");

            NetworkStream stream = client.GetStream();
            StreamReader reader = new StreamReader(stream, Encoding.UTF8);
            StreamWriter writer = new StreamWriter(stream, new UTF8Encoding(false));
            writer.AutoFlush = true;
            object writeLock = new object();

            ConcurrentQueue<string> commands = new ConcurrentQueue<string>();
            Thread readerThread = new Thread(delegate()
            {
                try
                {
                    string line;
                    while ((line = reader.ReadLine()) != null) commands.Enqueue(line);
                }
                catch (IOException) { }
                catch (ObjectDisposedException) { }
                commands.Enqueue(PipeProtocol.Quit);
            });
            readerThread.IsBackground = true;
            readerThread.Start();

            VisualSnapshot snapshot = new VisualSnapshot(engine.ElementCount);
            WorkerTrace.Log("worker: snapshot ready, elements=" + engine.ElementCount);
            SharedStateWriter shared = new SharedStateWriter(shmName, engine.ElementCount);
            WorkerTrace.Log("worker: shared memory ready");
            SpsMeter meter = new SpsMeter();
            SpeedControlState speed = new SpeedControlState();
            bool quit = false;
            bool running = false;
            int mode = 0, optionA = 0, optionB = 0;
            int steps = 0;                     // 步数由核心记，引擎只管推进
            Stopwatch statWatch = Stopwatch.StartNew();
            Stopwatch sliceWatch = Stopwatch.StartNew();
            double lastSlice = 0;
            int idleTicks = 0;
            int loopTicks = 0;

            Send(writer, writeLock, "READY " + Process.GetCurrentProcess().Id);
            WorkerTrace.Log("worker: READY sent");

            while (!quit)
            {
                loopTicks++;
                if ((loopTicks % 200) == 1)
                {
                    WorkerTrace.Log("loop tick=" + loopTicks + " running=" + running
                                    + " finished=" + engine.Finished + " queued=" + commands.Count);
                }
                string command;
                while (commands.TryDequeue(out command))
                {
                    Handle(command, engine, snapshot, shared, meter, speed, ref running,
                           ref mode, ref optionA, ref optionB, ref steps, writer, writeLock, ref quit);
                }

                if (running && !engine.Finished)
                {
                    double now = sliceWatch.Elapsed.TotalSeconds;
                    double elapsed = now - lastSlice;
                    lastSlice = now;
                    if (WorkerTrace.Enabled)
                    {
                        WorkerTrace.Log("slice running=" + running + " unlimited=" + speed.Unlimited
                                        + " sps=" + speed.TargetSps.ToString("0.###", CultureInfo.InvariantCulture)
                                        + " elapsed=" + elapsed.ToString("0.######", CultureInfo.InvariantCulture)
                                        + " credit=" + speed.Credit.ToString("0.###", CultureInfo.InvariantCulture)
                                        + " steps=" + steps);
                    }

                    int done = 0;
                    if (speed.Unlimited)
                    {
                        Stopwatch budget = Stopwatch.StartNew();
                        while (!engine.Finished && budget.Elapsed.TotalMilliseconds < speed.BudgetMs)
                        {
                            engine.StepOnce();
                            done++;
                        }
                    }
                    else
                    {
                        speed.Credit += speed.TargetSps * elapsed;
                        int quota = (int)Math.Floor(speed.Credit);
                        speed.Credit -= quota;
                        if (quota > 4000000) quota = 4000000;
                        Stopwatch budget = Stopwatch.StartNew();
                        while (done < quota && !engine.Finished)
                        {
                            engine.StepOnce();
                            done++;
                            if ((done & 1023) == 0 && budget.Elapsed.TotalMilliseconds >= speed.BudgetMs) break;
                        }
                    }

                    steps += done;
                    if (done > 0) meter.Report(steps);
                    PublishAll(engine, snapshot, shared, meter, speed, running, mode, optionA, optionB, steps);
                    if (statWatch.ElapsedMilliseconds >= 100)
                    {
                        statWatch.Restart();
                        SendStat(writer, writeLock, snapshot);
                    }
                    if (!speed.Unlimited) Thread.Sleep(speed.TargetSps >= 1000 ? 1 : 5);
                    else Thread.Sleep(0);
                }
                else
                {
                    if (running && engine.Finished)
                    {
                        running = false;
                        meter.Pause();
                        meter.Report(steps);
                        PublishAll(engine, snapshot, shared, meter, speed, running, mode, optionA, optionB, steps);
                        SendStat(writer, writeLock, snapshot);
                    }
                    lastSlice = sliceWatch.Elapsed.TotalSeconds;
                    Thread.Sleep(3);
                    idleTicks++;
                    if ((idleTicks & 127) == 0)
                    {
                        WorkerTrace.Log("idle running=" + running + " finished=" + engine.Finished
                                        + " ticks=" + idleTicks);
                    }
                    if ((idleTicks & 255) == 0 && !ParentAlive(parentPid)) quit = true;
                }
            }

            try { Send(writer, writeLock, "BYE"); }
            catch (Exception) { }
            shared.Dispose();
            try { client.Close(); }
            catch (Exception) { }
            return 0;
        }

        /// <summary>速率设置。放在这里是为了让运算进程自己掌握时钟与配额。</summary>
        private sealed class SpeedControlState
        {
            public bool Unlimited;
            public double TargetSps = 10.0;
            public double Credit;
            public double BudgetMs = SpeedControl.DefaultBudgetMs;
        }

        private static void Handle(string command, IVisualEngine engine, VisualSnapshot snapshot,
                                   SharedStateWriter shared, SpsMeter meter, SpeedControlState speed,
                                   ref bool running, ref int mode, ref int optionA, ref int optionB,
                                   ref int steps, StreamWriter writer, object writeLock, ref bool quit)
        {
            string[] parts = command.Split(' ');
            WorkerTrace.Log("cmd " + command);
            switch (parts[0])
            {
                case PipeProtocol.Init:
                    mode = PipeProtocol.Value(parts, 1, 0);
                    optionA = PipeProtocol.Value(parts, 2, 0);
                    optionB = PipeProtocol.Value(parts, 3, 0);
                    snapshot.Seed = PipeProtocol.Value(parts, 4, 0);
                    engine.Configure(mode, optionA, optionB);
                    engine.ResetAll();
                    engine.ExportScene(snapshot);
                    shared.ClearRunState();
                    running = false;
                    steps = 0;
                    meter.Reset();
                    PublishAll(engine, snapshot, shared, meter, speed, running, mode, optionA, optionB, steps);
                    SendStat(writer, writeLock, snapshot);
                    break;
                case PipeProtocol.Sync:
                    shared.ReadInto(snapshot);
                    engine.ImportScene(snapshot);
                    engine.ResetRun();
                    running = false;
                    steps = 0;
                    meter.Reset();
                    shared.ClearRunState();
                    PublishAll(engine, snapshot, shared, meter, speed, running, mode, optionA, optionB, steps);
                    SendStat(writer, writeLock, snapshot);
                    break;
                case PipeProtocol.Config:
                    mode = PipeProtocol.Value(parts, 1, 0);
                    optionA = PipeProtocol.Value(parts, 2, 0);
                    optionB = PipeProtocol.Value(parts, 3, 0);
                    engine.Configure(mode, optionA, optionB);
                    engine.ResetRun();
                    running = false;
                    steps = 0;
                    meter.Reset();
                    shared.ClearRunState();
                    PublishAll(engine, snapshot, shared, meter, speed, running, mode, optionA, optionB, steps);
                    SendStat(writer, writeLock, snapshot);
                    break;
                case PipeProtocol.Rate:
                    {
                        int milli = PipeProtocol.Value(parts, 1, 10000);
                        speed.Unlimited = milli <= 0;
                        speed.TargetSps = milli <= 0 ? 0 : milli / 1000.0;
                        speed.Credit = 0;
                        shared.SetTargetSpsMilli(milli);
                    }
                    break;
                case PipeProtocol.Run:
                    if (!engine.Finished)
                    {
                        running = true;
                        meter.StartRun(steps);
                        PublishAll(engine, snapshot, shared, meter, speed, running, mode, optionA, optionB, steps);
                        SendStat(writer, writeLock, snapshot);
                    }
                    break;
                case PipeProtocol.Pause:
                    running = false;
                    meter.Pause();
                    meter.Report(steps);
                    PublishAll(engine, snapshot, shared, meter, speed, running, mode, optionA, optionB, steps);
                    SendStat(writer, writeLock, snapshot);
                    break;
                case PipeProtocol.Step:
                    running = false;
                    meter.Pause();
                    if (!engine.Finished)
                    {
                        engine.StepOnce();
                        steps++;
                    }
                    meter.Reset();
                    shared.ClearRunState();
                    PublishAll(engine, snapshot, shared, meter, speed, running, mode, optionA, optionB, steps);
                    SendStat(writer, writeLock, snapshot);
                    break;
                case PipeProtocol.Reset:
                    running = false;
                    meter.Reset();
                    engine.ResetRun();
                    steps = 0;
                    shared.ClearRunState();
                    PublishAll(engine, snapshot, shared, meter, speed, running, mode, optionA, optionB, steps);
                    SendStat(writer, writeLock, snapshot);
                    break;
                case PipeProtocol.Replay:
                    {
                        int target = PipeProtocol.Value(parts, 1, 0);
                        running = false;
                        meter.Reset();
                        steps = Timeline.ReplayTo(engine, target, 2000000, speed.BudgetMs * 40);
                        shared.ClearRunState();
                        PublishAll(engine, snapshot, shared, meter, speed, running, mode, optionA, optionB, steps);
                        SendStat(writer, writeLock, snapshot);
                    }
                    break;
                case PipeProtocol.Scene:
                    engine.ExportScene(snapshot);
                    shared.ClearRunState();
                    PublishAll(engine, snapshot, shared, meter, speed, running, mode, optionA, optionB, steps);
                    SendStat(writer, writeLock, snapshot);
                    break;
                case PipeProtocol.Quit:
                    quit = true;
                    break;
            }
        }

        private static void PublishAll(IVisualEngine engine, VisualSnapshot snapshot, SharedStateWriter shared,
                                       SpsMeter meter, SpeedControlState speed, bool running,
                                       int mode, int optionA, int optionB, int steps)
        {
            engine.RefreshSnapshot(snapshot);
            snapshot.Steps = steps;
            snapshot.ModeKind = mode;
            snapshot.OptionA = optionA;
            snapshot.OptionB = optionB;
            snapshot.MeasuredSps = meter.MeasuredSps;
            snapshot.Running = running ? 1 : 0;
            snapshot.Finished = engine.Finished ? 1 : 0;
            snapshot.HasResult = engine.HasResult ? 1 : 0;
            shared.Publish(snapshot);
        }

        private static void Send(StreamWriter writer, object writeLock, string line)
        {
            WorkerTrace.Log("send begin " + line);
            lock (writeLock)
            {
                writer.WriteLine(line);
            }
            WorkerTrace.Log("send done " + line);
        }

        /// <summary>回一条小消息：统计数字都在里面，界面进程用它确认命令已生效。</summary>
        private static void SendStat(StreamWriter writer, object writeLock, VisualSnapshot snapshot)
        {
            Send(writer, writeLock,
                 "STAT " + snapshot.StateCode
                 + " " + snapshot.Steps
                 + " " + snapshot.WorkCount
                 + " " + snapshot.PendingCount
                 + " " + snapshot.PrimaryMetric
                 + " " + snapshot.SecondaryMetric
                 + " " + snapshot.CurrentIndex
                 + " " + (int)Math.Round(snapshot.MeasuredSps * 1000.0));
        }

        private static bool ParentAlive(int pid)
        {
            if (pid <= 0) return true;
            try
            {
                Process.GetProcessById(pid);
                return true;
            }
            catch (ArgumentException)
            {
                return false;
            }
        }
    }

    // ───────────────────────────────────────────────────────────── 自测骨架

    /// <summary>
    /// --selftest 的公共部分：标准输出的取法、断言打印、退出码。
    /// 自测一律在同进程里跑，不开窗口，也不起子进程。
    /// </summary>
    public static class SelfTestHost
    {
        private const int AttachParentProcess = -1;
        private const int StdOutputHandle = -11;

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool AttachConsole(int processId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool AllocConsole();

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr GetStdHandle(int handle);

        /// <summary>
        /// 取一个能写出字的标准输出流。
        ///
        /// 程序以 winexe 子系统编译，不带自己的控制台：从资源管理器双击启动时，
        /// 标准输出是无效句柄，写进去什么也看不到。这里的顺序是
        /// “先用继承来的句柄 → 没有就附到父进程的控制台 → 再没有就自己开一个”。
        /// 被重定向到文件或管道时第一个分支就命中，输出照常进文件。
        /// </summary>
        public static TextWriter OpenStandardOutput()
        {
            IntPtr handle = GetStdHandle(StdOutputHandle);
            if (handle == IntPtr.Zero || handle == new IntPtr(-1)) AttachConsole(AttachParentProcess);
            handle = GetStdHandle(StdOutputHandle);
            if (handle == IntPtr.Zero || handle == new IntPtr(-1)) AllocConsole();
            handle = GetStdHandle(StdOutputHandle);

            if (handle != IntPtr.Zero && handle != new IntPtr(-1))
            {
                try
                {
                    StreamWriter writer = new StreamWriter(Console.OpenStandardOutput(), new UTF8Encoding(false));
                    writer.AutoFlush = true;
                    return writer;
                }
                catch (IOException)
                {
                }
            }
            return Console.Out;
        }

        /// <summary>打印一条断言。通过打 check，不通过打 FAIL，两种都带详情。</summary>
        public static bool Check(TextWriter writer, string name, bool passed, string detail)
        {
            writer.WriteLine((passed ? "check " : "FAIL ") + name + ": " + (passed ? "PASS" : "FAIL")
                             + " (" + detail + ")");
            return passed;
        }

        public static void Note(TextWriter writer, string text)
        {
            writer.WriteLine("note  " + text);
        }

        public static void Result(TextWriter writer, bool passed)
        {
            writer.WriteLine(passed ? "selftest result=PASS" : "selftest result=FAIL");
            writer.Flush();
        }
    }

    /// <summary>
    /// 窗口标题栏（非客户区）的深浅。WinForms 没有对应属性，只能走 DWM。
    ///
    /// 属性号在两个范围里取过值：Windows 10 1809 到 1903 用 19，
    /// 1909 起用 20（DWMWA_USE_IMMERSIVE_DARK_MODE）。这里不猜，两个都设一遍，
    /// 再用 DwmGetWindowAttribute 读回来，看系统认的是哪一个。
    /// </summary>
    public static class DarkTitleBar
    {
        public const int AttributeNew = 20;     // 1909 及以后
        public const int AttributeOld = 19;     // 1809 到 1903

        [DllImport("dwmapi.dll")]
        private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int size);

        [DllImport("dwmapi.dll")]
        private static extern int DwmGetWindowAttribute(IntPtr hwnd, int attribute, out int value, int size);

        /// <summary>最后成功读回的那一个属性号，0 表示两个都没读通。</summary>
        public static int WorkingAttribute;

        /// <summary>设一次深浅，返回读回来的值（读不通返回 -1）。</summary>
        public static int Apply(IntPtr handle, bool dark)
        {
            if (handle == IntPtr.Zero) return -1;
            int value = dark ? 1 : 0;
            int readBack = -1;
            try
            {
                DwmSetWindowAttribute(handle, AttributeNew, ref value, sizeof(int));
                readBack = Read(handle, AttributeNew);
                if (readBack >= 0) WorkingAttribute = AttributeNew;
                if (readBack != value)
                {
                    // 新属性号不认，试旧的
                    DwmSetWindowAttribute(handle, AttributeOld, ref value, sizeof(int));
                    int old = Read(handle, AttributeOld);
                    if (old >= 0) { readBack = old; WorkingAttribute = AttributeOld; }
                }
            }
            catch (DllNotFoundException)
            {
                return -1;
            }
            catch (EntryPointNotFoundException)
            {
                return -1;
            }
            return readBack;
        }

        private static int Read(IntPtr handle, int attribute)
        {
            int value;
            int hr = DwmGetWindowAttribute(handle, attribute, out value, sizeof(int));
            return hr == 0 ? value : -1;
        }

        /// <summary>两个属性号各读一次，返回可以直接打印的核实结果。</summary>
        public static string Probe(IntPtr handle)
        {
            if (handle == IntPtr.Zero) return "无窗口句柄，未核实";
            int value = 1;
            DwmSetWindowAttribute(handle, AttributeNew, ref value, sizeof(int));
            int newRead = Read(handle, AttributeNew);
            value = 1;
            DwmSetWindowAttribute(handle, AttributeOld, ref value, sizeof(int));
            int oldRead = Read(handle, AttributeOld);
            return "attr" + AttributeNew + " readback=" + (newRead < 0 ? "读不到" : newRead.ToString(CultureInfo.InvariantCulture))
                   + " attr" + AttributeOld + " readback=" + (oldRead < 0 ? "读不到" : oldRead.ToString(CultureInfo.InvariantCulture));
        }
    }

    /// <summary>
    /// --palettecheck 的公共部分：只建窗体对象、不显示，递归走一遍控件，
    /// 对每个控件算（前景色，背景色）的对比度，低于 4.5:1 的判为不合格。
    /// 「黑字配深底」这类组合的对比度接近 1，会直接暴露出来，不必靠人看。
    /// </summary>
    public static class PaletteCheckHost
    {
        public const double MinimumContrast = 4.5;

        public static int Run(IVisualDemo demo, Func<Form> factory, TextWriter writer)
        {
            int failures = 0;
            ThemeKind[] kinds = new ThemeKind[] { ThemeKind.Light, ThemeKind.Dark };
            for (int i = 0; i < kinds.Length; i++)
            {
                ThemeKind kind = kinds[i];
                ThemeManager.Apply(kind);
                using (Form form = factory())
                {
                    // 构造完再按当前主题刷一遍；不是本框架的窗口就跳过这一步
                    VisualFormBase shell = form as VisualFormBase;
                    if (shell != null) shell.ApplyTheme();
                    IntPtr handle = form.Handle;       // 建句柄但不显示，标题栏属性才有地方设
                    int dwm = DarkTitleBar.Apply(handle, ThemeManager.Current == ThemeMode.Dark);
                    int count = 0, bad = 0;
                    writer.WriteLine("theme=" + (ThemeManager.Current == ThemeMode.Dark ? "dark" : "light")
                                     + " controls=" + Count(form) + " titlebar=" + (dwm < 0 ? "未核实" : dwm.ToString(CultureInfo.InvariantCulture))
                                     + " attr=" + DarkTitleBar.WorkingAttribute);
                    writer.WriteLine("  dwm " + DarkTitleBar.Probe(handle));
                    Walk(form, writer, ref count, ref bad);
                    writer.WriteLine("theme=" + (ThemeManager.Current == ThemeMode.Dark ? "dark" : "light")
                                     + " checked=" + count + " low_contrast=" + bad);
                    failures += bad;
                }
            }
            writer.WriteLine(failures == 0
                ? "palettecheck result=PASS (全部控件的对比度都不低于 4.5:1)"
                : "palettecheck result=FAIL (有 " + failures + " 个控件的对比度低于 4.5:1)");
            writer.Flush();
            return failures == 0 ? 0 : 1;
        }

        private static int Count(Control root)
        {
            int total = 1;
            for (int i = 0; i < root.Controls.Count; i++) total += Count(root.Controls[i]);
            return total;
        }

        private static void Walk(Control root, TextWriter writer, ref int count, ref int bad)
        {
            for (int i = 0; i < root.Controls.Count; i++)
            {
                Control child = root.Controls[i];
                Color fore = child.ForeColor;
                Color back = child.BackColor;
                double ratio = ThemeManager.ContrastRatio(fore, back);
                bool ok = ratio >= MinimumContrast;
                count++;
                if (!ok) bad++;
                string text = child.Text;
                if (text.Length > 18) text = text.Substring(0, 18) + "…";
                text = text.Replace("\r", " ").Replace("\n", " ");
                writer.WriteLine((ok ? "  ok   " : "  LOW  ")
                                 + child.GetType().Name.PadRight(14)
                                 + " fore=" + ThemePalette.ToHex(fore)
                                 + " back=" + ThemePalette.ToHex(back)
                                 + " contrast=" + ratio.ToString("0.00", CultureInfo.InvariantCulture)
                                 + (text.Length > 0 ? "  [" + text + "]" : ""));
                Walk(child, writer, ref count, ref bad);
            }
        }
    }

    // ───────────────────────────────────────────────────────────── 界面外壳

    /// <summary>
    /// 所有演示共用的窗口外壳：画布、图例、下拉、速率滑块与预设、时间线按钮、
    /// 工具盒、统计面板、状态栏，外加两进程与同进程的切换。
    /// 演示只要给一个 IVisualDemo 与一个 IVisualScene，其余不用重写。
    /// </summary>
    public class VisualFormBase : Form
    {
        protected readonly IVisualDemo Demo;
        protected readonly VisualSnapshot Snapshot;
        protected IVisualScene Scene;

        private readonly System.Windows.Forms.Timer frameTimer = new System.Windows.Forms.Timer();
        private readonly UndoStack undo = new UndoStack();
        private readonly SceneViewState viewState = new SceneViewState();
        private readonly BrushState brush = new BrushState();

        private bool twoProcess = true;
        private string shmName;
        private Process worker;
        private TcpListener listener;
        private TcpClient client;
        private StreamReader pipeReader;
        private StreamWriter pipeWriter;
        private readonly object pipeLock = new object();
        private Thread pipeThread;
        private SharedStateWriter sharedWriter;
        private SharedStateReader sharedReader;
        private volatile bool workerAlive;
        private volatile string workerNote = "";
        private int frameSeq;

        private IVisualEngine localEngine;
        private Thread localThread;
        private volatile bool localStop;
        private readonly SpsMeter localMeter = new SpsMeter();
        private double localLastSlice;
        private readonly Stopwatch localClock = Stopwatch.StartNew();

        private double targetSps = 10.0;
        private bool unlimited;
        private double lastSpsTwoProcess;
        private double lastSpsInProcess;
        private string lastStatusText = "";

        private bool dragging;
        private bool dragErase;
        private int lastDragElement = -1;
        private bool gestureChanged;
        private int seed = 12345;

        protected GridCanvas Canvas;
        private LegendPanel legend;
        private ComboBox comboMode;
        private ComboBox comboOption;
        private ComboBox comboOption2;
        private ComboBox comboDisplay;
        private RadioButton radioTwoProcess;
        private RadioButton radioInProcess;
        private FlatSlider trackRate;
        private Label labelRate;
        private Button buttonRun;
        private Button buttonStep;
        private Button buttonReset;
        private Button buttonReplay;
        private NumericUpDown numericReplay;
        private Button buttonUndo;
        private GroupBox groupTools;
        private RadioButton[] toolButtons;
        private RadioButton[] brushButtons;
        private NumericUpDown numericValue;
        private Label labelStatus;
        private Label labelModeHint;
        private Label[] statValues;
        private Font cellFont;
        private Panel panel;
        private SignatureBar signature;
        private ComboBox comboTheme;
        private ToolTip statusTip;
        private Label hintLabel;

        protected int ExtraTop = 352;      // 演示自己的控件从这一行往下加

        public VisualFormBase(IVisualDemo demo)
        {
            Demo = demo;
            Scene = demo.CreateScene();
            seed = Scene is ISeedProvider ? ((ISeedProvider)Scene).Seed : 12345;
            Snapshot = new VisualSnapshot(Scene.ElementCount);
            BuildUi();
            StartTwoProcess();
            SendInit();
            frameTimer.Interval = 16;      // 界面进程固定约 60 帧每秒，不参与限速
            frameTimer.Tick += FrameTick;
            frameTimer.Start();
        }

        /// <summary>场景想自己管种子时实现 ISeedProvider，界面外壳会把种子取出来。</summary>
        public int Seed
        {
            get { return seed; }
            set { seed = value; }
        }

        protected Panel PanelHost { get { return panel; } }
        protected SceneViewState ViewState { get { return viewState; } }
        protected bool TwoProcess { get { return twoProcess; } }

        // ── 界面搭建

        private void BuildUi()
        {
            Text = Demo.Name + " —— 渲染进程（运算在 --worker 子进程里）";
            AutoScaleMode = AutoScaleMode.None;
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            KeyPreview = true;
            Size canvasSize = Scene.CanvasSize;
            // 底部留出 26 像素给署名条；控制面板与画布都在它上方，互不遮挡。
            ClientSize = new Size(12 + canvasSize.Width + 12 + 360 + 12, 12 + 800 + 12 + 26);
            MinimumSize = new Size(560, 420);
            BackColor = ThemeManager.Palette.PanelBackground;

            Canvas = new GridCanvas(this);
            Canvas.Location = new Point(12, 12);
            Canvas.Size = canvasSize;
            Controls.Add(Canvas);

            legend = new LegendPanel(Scene.Palette);
            legend.Location = new Point(12, 12 + canvasSize.Height + 8);
            legend.Size = new Size(canvasSize.Width, 44);
            Controls.Add(legend);

            Label hint = new Label();
            hint.Text = Scene.HintText;
            hint.Location = new Point(12, 12 + canvasSize.Height + 56);
            hint.Size = new Size(canvasSize.Width, 80);
            hint.Font = new Font(SystemFonts.MessageBoxFont.FontFamily, 8.5f);
            hintLabel = hint;
            Controls.Add(hint);

            panel = new Panel();
            panel.Location = new Point(12 + canvasSize.Width + 12, 12);
            panel.Size = new Size(360, 800);
            panel.BackColor = ThemeManager.Palette.Surface;
            panel.BorderStyle = BorderStyle.FixedSingle;
            Controls.Add(panel);

            // 署名条：Dock 到底部，随窗口缩放与最大化一直贴在左下角，不压画布也不压面板。
            signature = new SignatureBar();
            Controls.Add(signature);
            signature.BringToFront();

            cellFont = new Font("Consolas", 8f, GraphicsUnit.Pixel);
            Font uiFont = SystemFonts.MessageBoxFont;

            comboMode = AddComboRow("模式", 8, Demo.ModeNames, Demo.DefaultMode, uiFont);
            comboMode.SelectedIndexChanged += delegate { ApplyConfig(); };
            comboOption = AddComboRow("选项一", 40, Demo.OptionNames, Demo.DefaultOption, uiFont);
            comboOption.SelectedIndexChanged += delegate { ApplyConfig(); };
            comboOption2 = AddComboRow("选项二", 72, Demo.Option2Names, Demo.DefaultOption2, uiFont);
            comboOption2.SelectedIndexChanged += delegate { ApplyConfig(); };
            comboDisplay = AddComboRow("元素上标", 104, Demo.DisplayNames, 0, uiFont);
            comboDisplay.SelectedIndexChanged += delegate
            {
                viewState.DisplayMode = comboDisplay.SelectedIndex;
                Canvas.Invalidate();
            };

            Label labelMode = MakeLabel("运算", 10, 139, 46, uiFont);
            panel.Controls.Add(labelMode);
            radioTwoProcess = new RadioButton();
            radioTwoProcess.Text = "两进程";
            radioTwoProcess.Font = uiFont;
            radioTwoProcess.Location = new Point(58, 136);
            radioTwoProcess.Size = new Size(78, 22);
            radioTwoProcess.Checked = true;
            radioTwoProcess.CheckedChanged += delegate { if (radioTwoProcess.Checked) SwitchMode(true); };
            panel.Controls.Add(radioTwoProcess);
            radioInProcess = new RadioButton();
            radioInProcess.Text = "同进程";
            radioInProcess.Font = uiFont;
            radioInProcess.Location = new Point(140, 136);
            radioInProcess.Size = new Size(78, 22);
            radioInProcess.CheckedChanged += delegate { if (radioInProcess.Checked) SwitchMode(false); };

            labelModeHint = MakeLabel("主题", 206, 139, 36, uiFont);
            panel.Controls.Add(labelModeHint);
            comboTheme = MakeThemeCombo(244, 136, 106, uiFont);
            comboTheme.SelectedIndexChanged += delegate
            {
                ThemeManager.Apply((ThemeKind)comboTheme.SelectedIndex);
                ApplyTheme();
            };
            panel.Controls.Add(comboTheme);
            panel.Controls.Add(radioInProcess);

            labelRate = MakeLabel("推进速率", 10, 164, 340, uiFont);
            panel.Controls.Add(labelRate);
            trackRate = new FlatSlider();
            trackRate.Minimum = 0;
            trackRate.Maximum = 1000;
            trackRate.TickFrequency = 100;
            trackRate.SmallChange = 5;
            trackRate.LargeChange = 25;
            trackRate.Location = new Point(6, 184);
            trackRate.Size = new Size(348, 36);
            trackRate.Value = SpeedControl.PositionForSps(targetSps);
            trackRate.ValueChanged += delegate { ApplyRate(); };
            panel.Controls.Add(trackRate);

            for (int i = 0; i < SpeedControl.PresetNames.Length; i++)
            {
                int width, x, y;
                if (i < 4) { width = 82; x = 10 + i * 86; y = 224; }
                else { width = 110; x = 10 + (i - 4) * 115; y = 254; }
                double sps = SpeedControl.PresetSps[i];
                Button button = MakeButton(SpeedControl.PresetNames[i], x, y, width, 26, uiFont, null);
                button.Click += delegate { SetRate(sps); };
                panel.Controls.Add(button);
            }

            buttonRun = MakeButton("开始", 10, 286, 166, 30, uiFont, ButtonRunClick);
            panel.Controls.Add(buttonRun);
            buttonStep = MakeButton("单步推进", 184, 286, 166, 30, uiFont, ButtonStepClick);
            panel.Controls.Add(buttonStep);
            buttonReset = MakeButton("重置到初始状态", 10, 320, 166, 30, uiFont, ButtonResetClick);
            panel.Controls.Add(buttonReset);
            numericReplay = new NumericUpDown();
            numericReplay.Minimum = 0;
            numericReplay.Maximum = 1000000;
            numericReplay.Value = 100;
            numericReplay.Location = new Point(184, 324);
            numericReplay.Size = new Size(56, 24);
            numericReplay.Font = uiFont;
            panel.Controls.Add(numericReplay);
            buttonReplay = MakeButton("回放到第 N 步", 244, 320, 106, 30, uiFont, ButtonReplayClick);
            panel.Controls.Add(buttonReplay);

            ExtraTop = BuildExtraControls(panel, ExtraTop, uiFont);

            BuildToolBox(uiFont);
            BuildStats(uiFont);
        }

        /// <summary>演示自己的控件加在这里，返回新的起始行。</summary>
        protected virtual int BuildExtraControls(Panel host, int y, Font font)
        {
            return y;
        }

        private void BuildToolBox(Font uiFont)
        {
            ToolDescriptor[] tools = Scene.Tools;
            int height = 132;
            groupTools = new GroupBox();
            groupTools.Location = new Point(10, 456);
            groupTools.Size = new Size(340, height);
            groupTools.Font = uiFont;
            panel.Controls.Add(groupTools);

            toolButtons = new RadioButton[tools.Length];
            for (int i = 0; i < tools.Length; i++)
            {
                RadioButton button = new RadioButton();
                button.Text = tools[i].Name;
                button.Font = uiFont;
                button.Size = new Size(105, 22);
                button.Location = new Point(10 + (i % 3) * 108, 20 + (i / 3) * 24);
                int captured = i;
                button.CheckedChanged += delegate
                {
                    if (toolButtons[captured].Checked) SetTool(tools[captured]);
                };
                groupTools.Controls.Add(button);
                toolButtons[i] = button;
            }

            buttonUndo = MakeButton("撤销 Ctrl+Z", 226, 44, 105, 22, uiFont, ButtonUndoClick);
            buttonUndo.Enabled = false;
            groupTools.Controls.Add(buttonUndo);

            Label labelBrush = MakeLabel("刷子", 10, 70, 40, uiFont);
            groupTools.Controls.Add(labelBrush);
            int[] sizes = Scene.BrushSizes;
            brushButtons = new RadioButton[sizes.Length];
            for (int i = 0; i < sizes.Length; i++)
            {
                RadioButton button = new RadioButton();
                button.Text = sizes[i] + "x" + sizes[i];
                button.Font = uiFont;
                button.Size = new Size(56, 22);
                button.Location = new Point(52 + i * 60, 68);
                int captured = i;
                button.CheckedChanged += delegate
                {
                    if (brushButtons[captured].Checked)
                    {
                        brush.Radius = captured;
                        viewState.Radius = captured;
                        UpdateToolCaption();
                    }
                };
                groupTools.Controls.Add(button);
                brushButtons[i] = button;
            }

            Label labelValue = MakeLabel("刷子值", 10, 94, 72, uiFont);
            groupTools.Controls.Add(labelValue);
            numericValue = new NumericUpDown();
            numericValue.Minimum = Scene.MinValue;
            numericValue.Maximum = Scene.MaxValue;
            numericValue.Value = Scene.MinValue;
            numericValue.Location = new Point(84, 92);
            numericValue.Size = new Size(50, 22);
            numericValue.Font = uiFont;
            numericValue.ValueChanged += delegate
            {
                brush.Value = (int)numericValue.Value;
                viewState.Value = brush.Value;
            };
            groupTools.Controls.Add(numericValue);

            if (tools.Length > 0)
            {
                toolButtons[0].Checked = true;
                SetTool(tools[0]);
            }
            if (sizes.Length > 0) brushButtons[0].Checked = true;
            numericValue.Enabled = Scene.UsesValue;
            UpdateToolCaption();
        }

        private void BuildStats(Font uiFont)
        {
            GroupBox groupStats = new GroupBox();
            groupStats.Text = "实时统计（运算进程实测）";
            groupStats.Location = new Point(10, 592);
            groupStats.Size = new Size(340, 170);
            groupStats.Font = uiFont;
            panel.Controls.Add(groupStats);

            statValues = new Label[Demo.StatNames.Length];
            for (int i = 0; i < Demo.StatNames.Length; i++)
            {
                Label caption = MakeLabel(Demo.StatNames[i] + "：", 10, 18 + i * 20, 132, uiFont);
                caption.AutoEllipsis = true;
                groupStats.Controls.Add(caption);
                Label value = MakeLabel("-", 146, 18 + i * 20, 184, uiFont);
                value.AutoEllipsis = true;          // 放不下就省略号，不硬裁
                value.Font = new Font("Consolas", 9f);   // 等宽，数字跳动时宽度不抖
                groupStats.Controls.Add(value);
                statValues[i] = value;
            }

            labelStatus = MakeLabel("就绪", 10, 756, 340, uiFont);
            labelStatus.Font = new Font(uiFont, FontStyle.Bold);
            labelStatus.AutoEllipsis = true;      // 文案再长也只是省略号，不会半个字被切掉
            labelStatus.ForeColor = Color.FromArgb(20, 20, 120);
            panel.Controls.Add(labelStatus);
            statusTip = new ToolTip();
            statusTip.SetToolTip(labelStatus, "就绪");
            // 构造完立刻按当前主题刷一遍：控件建出来时带的是框架默认色，
            // 不在这一处统一接管，深色下就会留下浅底黑字或白字的控件。
            ApplyTheme();
            if (ThemeManager.Requested == ThemeKind.Auto) comboTheme.SelectedIndex = (int)ThemeKind.Auto;
        }

        /// <summary>把当前主题应用到窗口与全部控件上。切换主题时也走这里。</summary>
        public void ApplyTheme()
        {
            ThemePalette theme = ThemeManager.Palette;
            BackColor = theme.PanelBackground;
            ForeColor = theme.PanelText;
            if (panel != null) panel.BackColor = theme.Surface;
            if (legend != null) legend.BackColor = theme.Surface;
            if (Canvas != null) Canvas.BackColor = theme.Background;
            if (signature != null) signature.ApplyTheme(theme);
            // ToolTip 是独立的窗口，不跟着父控件继承颜色，也要显式给
            if (statusTip != null)
            {
                statusTip.BackColor = theme.Surface;
                statusTip.ForeColor = theme.PanelText;
            }
            ApplyThemeToControls(this, theme);
            // 画布与图例有自己的底色，放在递归之后设，免得被统一刷成面板色
            if (Canvas != null) Canvas.BackColor = theme.Background;
            if (legend != null) legend.BackColor = theme.Surface;
            ApplyTitleBarTheme();
            Invalidate(true);
            if (Canvas != null) Canvas.Invalidate();
            if (legend != null) legend.Invalidate();
        }

        /// <summary>
        /// 递归接管每个控件的两个颜色。凡是不显式赋值的控件，在 WinForms 里会继承
        /// 框架默认色（浅色），深色主题下就成了浅底白字，因此一个都不留。
        /// </summary>
        private static void ApplyThemeToControls(Control root, ThemePalette theme)
        {
            for (int i = 0; i < root.Controls.Count; i++)
            {
                Control child = root.Controls[i];
                if (child is SignatureBar) continue;
                child.ForeColor = theme.PanelText;
                child.BackColor = child is GroupBox ? theme.Surface : theme.Surface;
                if (child is Button)
                {
                    // 默认的 Button 用系统视觉样式画，会忽略 BackColor，深色下仍是浅色按钮
                    Button button = (Button)child;
                    button.UseVisualStyleBackColor = false;
                    button.FlatStyle = FlatStyle.Flat;
                    button.FlatAppearance.BorderColor = theme.GridLine;
                    button.BackColor = theme.Surface;
                    button.ForeColor = theme.PanelText;
                }
                else if (child is ComboBox)
                {
                    // DropDownList 只有 Flat 样式才认 BackColor
                    ComboBox combo = (ComboBox)child;
                    combo.FlatStyle = FlatStyle.Flat;
                    combo.BackColor = theme.Surface;
                    combo.ForeColor = theme.PanelText;
                }
                else if (child is TextBox || child is NumericUpDown)
                {
                    child.BackColor = theme.Surface;
                    child.ForeColor = theme.PanelText;
                }
                else if (child is FlatSlider)
                {
                    // 自绘滑块，颜色由它自己按主题画，这里只把两个色交过去
                    child.BackColor = theme.Surface;
                    child.ForeColor = theme.PanelText;
                }
                else if (child is GroupBox)
                {
                    child.BackColor = theme.Surface;
                    child.ForeColor = theme.PanelText;
                }
                else if (child is Label)
                {
                    // 面板与分组框的底色就是 Surface，标签用实色而不是透明，
                    // 这样「有没有漏掉的控件」可以逐个查出来。
                    child.BackColor = theme.Surface;
                    child.ForeColor = theme.PanelText;
                }
                ApplyThemeToControls(child, theme);
            }
        }

        /// <summary>标题栏（非客户区）也跟着主题走，见 DarkTitleBar。</summary>
        private void ApplyTitleBarTheme()
        {
            if (!IsHandleCreated) return;
            DarkTitleBar.Apply(Handle, ThemeManager.Current == ThemeMode.Dark);
        }

        protected override void OnHandleCreated(EventArgs e)
        {
            base.OnHandleCreated(e);
            ApplyTitleBarTheme();
        }

        private ComboBox AddComboRow(string caption, int y, string[] items, int selected, Font font)
        {
            panel.Controls.Add(MakeLabel(caption, 10, y + 3, 76, font));
            ComboBox combo = new ComboBox();
            combo.DropDownStyle = ComboBoxStyle.DropDownList;
            combo.Location = new Point(90, y);
            combo.Size = new Size(260, 24);
            combo.Font = font;
            combo.Items.AddRange(items);
            combo.SelectedIndex = selected;
            panel.Controls.Add(combo);
            return combo;
        }

        /// <summary>主题下拉：宽度按「跟随系统」四个字留够，不靠省略号。</summary>
        private ComboBox MakeThemeCombo(int x, int y, int width, Font font)
        {
            ComboBox combo = new ComboBox();
            combo.DropDownStyle = ComboBoxStyle.DropDownList;
            combo.Location = new Point(x, y);
            combo.Size = new Size(width, 24);
            combo.Font = font;
            combo.Items.AddRange(new object[] { "浅色", "深色", "跟随系统" });
            combo.SelectedIndex = (int)ThemeManager.Requested;
            return combo;
        }

        protected static Label MakeLabel(string text, int x, int y, int width, Font font)
        {
            Label label = new Label();
            label.Text = text;
            label.Location = new Point(x, y);
            label.Size = new Size(width, 18);
            label.Font = font;
            label.TextAlign = ContentAlignment.MiddleLeft;
            return label;
        }

        protected static Button MakeButton(string text, int x, int y, int width, int height, Font font,
                                           EventHandler onClick)
        {
            Button button = new Button();
            button.Text = text;
            button.Location = new Point(x, y);
            button.Size = new Size(width, height);
            button.Font = font;
            if (onClick != null) button.Click += onClick;
            return button;
        }

        private void SetTool(ToolDescriptor descriptor)
        {
            brush.ToolId = descriptor.Id;
            viewState.ToolId = descriptor.Id;
            bool hand = !descriptor.UsesRadius;
            Canvas.Cursor = hand ? Cursors.Hand : Cursors.Cross;
            UpdateToolCaption();
        }

        private void UpdateToolCaption()
        {
            ToolDescriptor current = null;
            ToolDescriptor[] tools = Scene.Tools;
            for (int i = 0; i < tools.Length; i++)
            {
                if (tools[i].Id == brush.ToolId) current = tools[i];
            }
            string caption = "工具";
            if (current != null)
            {
                caption += "（当前：" + current.Name;
                if (current.UsesRadius && Scene.BrushSizes.Length > 0) caption += "，刷子 " + BrushState.RadiusName(brush.Radius);
                caption += "）";
            }
            groupTools.Text = caption;
        }

        // ── 两个进程的连接

        private void StartTwoProcess()
        {
            shmName = "VisualCoreShm." + Guid.NewGuid().ToString("N");
            sharedWriter = new SharedStateWriter(shmName, Snapshot.Elements);
            sharedReader = new SharedStateReader(shmName, Snapshot.Elements);
            if (!SpawnWorker()) return;
            pipeThread = new Thread(PipeReadLoop);
            pipeThread.IsBackground = true;
            pipeThread.Start();
        }

        private bool SpawnWorker()
        {
            try
            {
                // 命令通道走环回 TCP。命名管道在本机实测会被攒批：
                // 运算进程写出去的一行要好几秒才被读到，换成环回套接字之后即时到达。
                listener = new TcpListener(IPAddress.Loopback, 0);
                listener.Start();
                int port = ((IPEndPoint)listener.LocalEndpoint).Port;

                ProcessStartInfo info = new ProcessStartInfo();
                info.FileName = Application.ExecutablePath;
                info.Arguments = "--worker --port=" + port + " --shm=" + shmName
                                 + " --parent=" + Process.GetCurrentProcess().Id
                                 + (WorkerTrace.Enabled ? " --trace" : "");
                info.UseShellExecute = false;
                info.CreateNoWindow = true;
                worker = Process.Start(info);

                ManualResetEvent connected = new ManualResetEvent(false);
                Thread acceptThread = new Thread(delegate()
                {
                    try
                    {
                        client = listener.AcceptTcpClient();
                        client.NoDelay = true;
                        connected.Set();
                    }
                    catch (Exception ex)
                    {
                        WorkerTrace.Log("render: accept failed " + ex.GetType().Name + " " + ex.Message);
                    }
                });
                acceptThread.IsBackground = true;
                acceptThread.Start();
                if (!connected.WaitOne(8000))
                {
                    workerNote = "运算进程没有连上命令通道";
                    return false;
                }

                pipeReader = new StreamReader(client.GetStream(), Encoding.UTF8);
                pipeWriter = new StreamWriter(client.GetStream(), new UTF8Encoding(false));
                pipeWriter.AutoFlush = true;
                workerAlive = true;
                workerNote = "";
                WorkerTrace.Log("render: worker connected on port " + port);
                return true;
            }
            catch (Exception ex)
            {
                workerNote = "运算进程启动失败：" + ex.Message;
                WorkerTrace.Log("render: spawn failed " + ex);
                return false;
            }
        }

        protected void SendCommand(string command)
        {
            StreamWriter writer = pipeWriter;
            if (writer == null) return;
            lock (pipeLock)
            {
                try
                {
                    writer.WriteLine(command);
                }
                catch (IOException)
                {
                    workerAlive = false;
                    workerNote = "运算进程已断开";
                }
                catch (ObjectDisposedException)
                {
                    workerAlive = false;
                }
            }
        }

        private void PipeReadLoop()
        {
            // 先把引用取到局部：界面线程重新拉起运算进程时会把字段置空，
            // 循环里再用字段就会撞上空引用。
            StreamReader reader = pipeReader;
            WorkerTrace.Log("render: read thread start");
            try
            {
                string line;
                while (reader != null && (line = reader.ReadLine()) != null)
                {
                    WorkerTrace.Log("render: got " + line);
                    if (line.StartsWith("BYE", StringComparison.Ordinal)) break;
                }
            }
            catch (Exception ex)
            {
                // 读线程一死，运算进程的写就会堵在管道里，界面表现为“不动”。
                // 把异常写进状态栏，免得只看到“没有反应”。
                workerNote = "读运算进程的消息失败：" + ex.GetType().Name + " " + ex.Message;
                WorkerTrace.Log("render: read thread error " + ex);
            }
            workerAlive = false;
            WorkerTrace.Log("render: read thread end");
            if (workerNote.Length == 0) workerNote = "运算进程已退出";
        }

        protected bool EnsureWorker()
        {
            if (workerAlive && worker != null && !worker.HasExited) return true;
            CloseWorker();
            if (!SpawnWorker()) return false;
            pipeThread = new Thread(PipeReadLoop);
            pipeThread.IsBackground = true;
            pipeThread.Start();
            SendInit();
            return true;
        }

        private void CloseWorker()
        {
            try
            {
                if (pipeWriter != null) pipeWriter.WriteLine(PipeProtocol.Quit);
            }
            catch (Exception) { }
            try
            {
                if (worker != null && !worker.WaitForExit(1500)) worker.Kill();
            }
            catch (Exception) { }
            try
            {
                if (client != null) client.Close();
            }
            catch (Exception) { }
            try
            {
                if (listener != null) listener.Stop();
            }
            catch (Exception) { }
            pipeWriter = null;
            pipeReader = null;
            client = null;
            listener = null;
            worker = null;
            workerAlive = false;
        }

        private void SendInit()
        {
            SendCommand(PipeProtocol.Join(PipeProtocol.Init, comboMode.SelectedIndex,
                                          comboOption.SelectedIndex, comboOption2.SelectedIndex, seed));
            SendCommand(PipeProtocol.Join(PipeProtocol.Rate, SpeedControl.ToMilli(targetSps)));
        }

        private void ApplyConfig()
        {
            viewState.DisplayMode = comboDisplay.SelectedIndex;
            if (twoProcess) SendCommand(PipeProtocol.Join(PipeProtocol.Config, comboMode.SelectedIndex,
                                                          comboOption.SelectedIndex, comboOption2.SelectedIndex));
            else if (localEngine != null)
            {
                localEngine.Configure(comboMode.SelectedIndex, comboOption.SelectedIndex, comboOption2.SelectedIndex);
                localEngine.ResetRun();
            }
            Snapshot.ClearRunState();
            buttonRun.Text = "开始";
            labelStatus.Text = "已切换设置：搜索回到初始状态";
            Canvas.Invalidate();
        }

        // ── 同进程模式

        private void SwitchMode(bool useTwoProcess)
        {
            twoProcess = useTwoProcess;
            StopLocalEngine();
            if (twoProcess)
            {
                if (EnsureWorker())
                {
                    SendInit();
                    // 同进程模式里改过的场景要带过去，否则切回来会看到旧地图。
                    AfterSceneEdited("已切到两进程：算法在 --worker 子进程里跑");
                    return;
                }
                labelStatus.Text = workerNote;
            }
            else
            {
                StartLocalEngine();
                labelStatus.Text = "已切到同进程：算法在界面进程的后台线程里跑";
            }
            Snapshot.ClearRunState();
            buttonRun.Text = "开始";
            Canvas.Invalidate();
        }

        private void StartLocalEngine()
        {
            localEngine = Demo.CreateEngine();
            localEngine.Configure(comboMode.SelectedIndex, comboOption.SelectedIndex, comboOption2.SelectedIndex);
            localEngine.ResetAll();
            localMeter.Reset();
            localLastSlice = localClock.Elapsed.TotalSeconds;
            localStop = false;
            localThread = new Thread(LocalLoop);
            localThread.IsBackground = true;
            localThread.Start();
        }

        private void StopLocalEngine()
        {
            localStop = true;
            if (localThread != null && !localThread.Join(1000)) localThread.Abort();
            localThread = null;
            localEngine = null;
        }

        private void LocalLoop()
        {
            while (!localStop)
            {
                IVisualEngine engine = localEngine;
                if (engine == null) return;
                if (engine.Finished) { Thread.Sleep(3); continue; }
                // 暂停时一步也不走，不限速这一档同样受这一条约束
                if (!localRunning) { Thread.Sleep(3); continue; }

                double now = localClock.Elapsed.TotalSeconds;
                double elapsed = now - localLastSlice;
                localLastSlice = now;

                int steps = 0;
                if (unlimited)
                {
                    Stopwatch budget = Stopwatch.StartNew();
                    while (!engine.Finished && budget.Elapsed.TotalMilliseconds < SpeedControl.DefaultBudgetMs)
                    {
                        engine.StepOnce();
                        steps++;
                    }
                }
                else
                {
                    localCredit += targetSps * elapsed;
                    int quota = (int)Math.Floor(localCredit);
                    localCredit -= quota;
                    if (quota > 4000000) quota = 4000000;
                    Stopwatch budget = Stopwatch.StartNew();
                    while (steps < quota && !engine.Finished)
                    {
                        engine.StepOnce();
                        steps++;
                        if ((steps & 1023) == 0 && budget.Elapsed.TotalMilliseconds >= SpeedControl.DefaultBudgetMs) break;
                    }
                }

                if (steps > 0)
                {
                    localSteps += steps;            // 步数在这里累加，界面读的就是这个数
                    localMeter.Report(localSteps);
                }
                Thread.Sleep(unlimited ? 0 : (targetSps >= 1000 ? 1 : 5));
            }
        }

        private bool localRunning;
        private int localSteps;
        private double localCredit;

        // ── 速率

        private void SetRate(double sps)
        {
            trackRate.Value = SpeedControl.PositionForSps(sps);
            ApplyRate();
        }

        private void ApplyRate()
        {
            unlimited = trackRate.Value >= 1000;
            targetSps = SpeedControl.SpsForPosition(trackRate.Value);
            labelRate.Text = unlimited
                ? "推进速率：不限速（运算进程全力跑）"
                : "推进速率：" + SpeedControl.FormatSps(targetSps) + " 步/秒（sps）";
            if (twoProcess) SendCommand(PipeProtocol.Join(PipeProtocol.Rate, SpeedControl.ToMilli(targetSps)));
        }

        // ── 时间线按钮

        private void ButtonRunClick(object sender, EventArgs e)
        {
            if (twoProcess)
            {
                if (!EnsureWorker()) { labelStatus.Text = workerNote; return; }
                if (Snapshot.Finished != 0)
                {
                    SendCommand(PipeProtocol.Reset);
                    Snapshot.ClearRunState();
                    SendCommand(PipeProtocol.Run);
                    buttonRun.Text = "暂停";
                }
                else if (Snapshot.Running != 0)
                {
                    SendCommand(PipeProtocol.Pause);
                    buttonRun.Text = "继续";
                }
                else
                {
                    SendCommand(PipeProtocol.Run);
                    buttonRun.Text = "暂停";
                }
            }
            else
            {
                IVisualEngine engine = localEngine;
                if (engine == null) return;
                if (engine.Finished)
                {
                    engine.ResetRun();
                    Snapshot.ClearRunState();
                    localMeter.StartRun(localSteps);
                    localRunning = true;
                    buttonRun.Text = "暂停";
                }
                else if (localRunning)
                {
                    localRunning = false;
                    localMeter.Pause();
                    localMeter.Report(localSteps);
                    buttonRun.Text = "继续";
                }
                else
                {
                    localMeter.StartRun(localSteps);
                    localRunning = true;
                    buttonRun.Text = "暂停";
                }
            }
        }

        private void ButtonStepClick(object sender, EventArgs e)
        {
            if (twoProcess)
            {
                if (!EnsureWorker()) { labelStatus.Text = workerNote; return; }
                if (Snapshot.Finished != 0) SendCommand(PipeProtocol.Reset);
                SendCommand(PipeProtocol.Pause);
                SendCommand(PipeProtocol.Step);
            }
            else
            {
                IVisualEngine engine = localEngine;
                if (engine == null) return;
                localRunning = false;
                localMeter.Pause();
                if (engine.Finished) engine.ResetRun();
                engine.StepOnce();
                localSteps++;
                localMeter.Reset();
            }
            buttonRun.Text = "继续";
            Canvas.Invalidate();
        }

        private void ButtonResetClick(object sender, EventArgs e)
        {
            if (twoProcess) SendCommand(PipeProtocol.Reset);
            else if (localEngine != null)
            {
                localEngine.ResetRun();
                localRunning = false;
                localMeter.Reset();
                localSteps = 0;
            }
            Snapshot.ClearRunState();
            buttonRun.Text = "开始";
            labelStatus.Text = "就绪：已重置到初始状态";
            Canvas.Invalidate();
        }

        private void ButtonReplayClick(object sender, EventArgs e)
        {
            int target = (int)numericReplay.Value;
            if (twoProcess)
            {
                if (!EnsureWorker()) { labelStatus.Text = workerNote; return; }
                SendCommand(PipeProtocol.Join(PipeProtocol.Replay, target));
            }
            else if (localEngine != null)
            {
                localRunning = false;
                localMeter.Reset();
                localSteps = Timeline.ReplayTo(localEngine, target, 2000000, 400);
            }
            buttonRun.Text = "继续";
            Snapshot.ClearRunState();
            labelStatus.Text = "已回放到第 " + target + " 步";
            Canvas.Invalidate();
        }

        private void ButtonUndoClick(object sender, EventArgs e)
        {
            if (!Scene.CanUndo)
            {
                labelStatus.Text = "没有可撤销的操作";
                return;
            }
            if (!Scene.Undo()) return;
            buttonUndo.Enabled = Scene.CanUndo;
            AfterSceneEdited("已撤销上一步地图改动");
        }

        /// <summary>场景被改动之后的统一处理：清搜索状态，并把新场景交给运算进程。</summary>
        protected void AfterSceneEdited(string message)
        {
            Scene.ExportScene(Snapshot);
            if (twoProcess)
            {
                if (sharedWriter != null) sharedWriter.WriteScene(Snapshot);
                SendCommand(PipeProtocol.Sync);
            }
            else if (localEngine != null)
            {
                localEngine.ImportScene(Snapshot);
                localEngine.ResetRun();
            }
            Snapshot.ClearRunState();
            buttonRun.Text = "开始";
            labelStatus.Text = message;
            Canvas.Invalidate();
        }

        // ── 每帧刷新

        private void FrameTick(object sender, EventArgs e)
        {
            bool changed = false;
            if (twoProcess)
            {
                if (sharedReader != null) changed = sharedReader.TryRead(Snapshot);
            }
            else if (localEngine != null)
            {
                lock (Snapshot.Sync)
                {
                    localEngine.RefreshSnapshot(Snapshot);
                    Snapshot.Steps = localSteps;      // 步数由核心记，引擎只管推进
                    Snapshot.Seq = ++frameSeq;
                    Snapshot.MeasuredSps = localMeter.MeasuredSps;
                    Snapshot.Running = localRunning ? 1 : 0;
                    Snapshot.Finished = localEngine.Finished ? 1 : 0;
                    Snapshot.HasResult = localEngine.HasResult ? 1 : 0;
                    Snapshot.ModeKind = comboMode.SelectedIndex;
                    Snapshot.OptionA = comboOption.SelectedIndex;
                    Snapshot.OptionB = comboOption2.SelectedIndex;
                }
                changed = true;
            }

            if (Snapshot.MeasuredSps > 0)
            {
                if (twoProcess) lastSpsTwoProcess = Snapshot.MeasuredSps;
                else lastSpsInProcess = Snapshot.MeasuredSps;
            }

            UpdateStats();
            UpdateStatus();
            if (changed) Canvas.Invalidate();
        }

        /// <summary>
        /// 窗口尺寸变化时重新排一遍：控制面板与提示行都收在署名条上方，
        /// 因此窗口缩小时是内容被裁，而不是署名条盖住内容。
        /// </summary>
        protected override void OnResize(EventArgs e)
        {
            base.OnResize(e);
            LayoutContent();
        }

        private void LayoutContent()
        {
            if (signature == null) return;
            int bottom = ClientSize.Height - signature.Height - 12;
            if (panel != null) panel.Height = Math.Max(120, bottom - panel.Top);
            if (hintLabel != null)
            {
                hintLabel.Width = Math.Max(240, ClientSize.Width - panel.Width - 48);
            }
        }

        private void UpdateStats()
        {
            statValues[0].Text = Snapshot.Steps.ToString(CultureInfo.InvariantCulture);
            statValues[1].Text = Snapshot.WorkCount.ToString(CultureInfo.InvariantCulture);
            statValues[2].Text = Snapshot.PendingCount.ToString(CultureInfo.InvariantCulture);
            statValues[3].Text = Snapshot.PrimaryMetric >= 0
                ? Snapshot.PrimaryMetric.ToString(CultureInfo.InvariantCulture) : "-";
            statValues[4].Text = Snapshot.SecondaryMetric >= 0
                ? Snapshot.SecondaryMetric.ToString(CultureInfo.InvariantCulture) : "-";
            statValues[5].Text = Snapshot.MeasuredSps > 0
                ? SpeedControl.FormatMeasured(Snapshot.MeasuredSps) + " 步/秒" : "-";
            // 这一行两个数字并排，用 k 缩写，免得超出标签宽度被裁断。
            statValues[6].Text = "两进程 " + ShortSps(lastSpsTwoProcess)
                                 + " / 同进程 " + ShortSps(lastSpsInProcess);
        }

        /// <summary>对照那一行用的短写法：超过一千就写成 859k，保证两个数字并排也放得下。</summary>
        private static string ShortSps(double sps)
        {
            if (sps <= 0) return "-";
            if (sps >= 1000) return (sps / 1000.0).ToString("0", CultureInfo.InvariantCulture) + "k";
            return SpeedControl.FormatMeasured(sps);
        }

        private void UpdateStatus()
        {
            string text;
            if (twoProcess && !workerAlive)
            {
                text = workerNote.Length > 0 ? workerNote + "（点「开始」会重新拉起）" : "运算进程未连接";
            }
            else
            {
                text = Demo.DescribeState(Snapshot);
            }
            if (text != lastStatusText)
            {
                lastStatusText = text;
                labelStatus.Text = text;
                if (statusTip != null) statusTip.SetToolTip(labelStatus, text);
            }
        }

        // ── 鼠标

        internal void CanvasMouseDown(int px, int py, MouseButtons button)
        {
            int element = Scene.HitTest(px, py);
            if (element < 0) return;
            if (twoProcess) SendCommand(PipeProtocol.Pause);
            else localRunning = false;

            Scene.BeginGesture();
            dragging = true;
            dragErase = button == MouseButtons.Right;
            lastDragElement = element;
            gestureChanged = Scene.ApplyTool(brush.ToolId, element, brush.Radius, brush.Value, dragErase, true);
            if (gestureChanged)
            {
                Scene.ExportScene(Snapshot);
                if (twoProcess)
                {
                    if (sharedWriter != null) sharedWriter.WriteScene(Snapshot);
                    SendCommand(PipeProtocol.Sync);
                }
                Snapshot.ClearRunState();
                Canvas.Invalidate();
            }
        }

        internal void CanvasMouseMove(int px, int py)
        {
            if (!dragging) return;
            int element = Scene.HitTest(px, py);
            if (element < 0 || element == lastDragElement) return;
            lastDragElement = element;
            if (Scene.ApplyTool(brush.ToolId, element, brush.Radius, brush.Value, dragErase, false))
            {
                gestureChanged = true;
                Scene.ExportScene(Snapshot);
                if (twoProcess && sharedWriter != null) sharedWriter.WriteScene(Snapshot);
                Canvas.Invalidate();
            }
        }

        internal void CanvasMouseUp()
        {
            if (!dragging) return;
            dragging = false;
            lastDragElement = -1;
            bool changed = Scene.EndGesture() || gestureChanged;
            gestureChanged = false;
            buttonUndo.Enabled = Scene.CanUndo;
            if (changed) AfterSceneEdited("场景已修改：运行状态已清空，请重新运行");
        }

        internal void PaintCanvas(Graphics g)
        {
            lock (Snapshot.Sync)
            {
                Scene.Paint(g, Snapshot, viewState);
            }
        }

        internal string CanvasElementText(int element)
        {
            lock (Snapshot.Sync)
            {
                return Scene.ElementText(element, Snapshot, viewState);
            }
        }

        internal Font CellFont { get { return cellFont; } }

        // ── 快捷键与退出

        protected override void OnKeyDown(KeyEventArgs e)
        {
            base.OnKeyDown(e);
            if (e.Control && e.KeyCode == Keys.Z)
            {
                ButtonUndoClick(this, EventArgs.Empty);
                e.Handled = true;
            }
            else if (e.KeyCode == Keys.Space)
            {
                ButtonRunClick(this, EventArgs.Empty);
                e.Handled = true;
            }
            else if (e.KeyCode == Keys.Right)
            {
                ButtonStepClick(this, EventArgs.Empty);
                e.Handled = true;
            }
            else if (e.KeyCode == Keys.R)
            {
                ButtonResetClick(this, EventArgs.Empty);
                e.Handled = true;
            }
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            frameTimer.Stop();
            StopLocalEngine();
            CloseWorker();
            if (sharedWriter != null) sharedWriter.Dispose();
            if (sharedReader != null) sharedReader.Dispose();
            base.OnFormClosing(e);
        }
    }

    /// <summary>画元素的控件。所有绘制都交给窗口，鼠标事件也转交过去。</summary>
    public sealed class GridCanvas : Control
    {
        private readonly VisualFormBase owner;

        public GridCanvas(VisualFormBase owner)
        {
            this.owner = owner;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer
                     | ControlStyles.UserPaint, true);
            BackColor = Color.White;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            owner.PaintCanvas(e.Graphics);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            owner.CanvasMouseDown(e.X, e.Y, e.Button);
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            owner.CanvasMouseMove(e.X, e.Y);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            owner.CanvasMouseUp();
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            owner.CanvasMouseUp();
        }
    }

    /// <summary>
    /// 自绘滑块，用来替掉 WinForms 的 TrackBar。
    ///
    /// 换掉的原因：TrackBar 的 ForeColor 是空实现——赋什么值读回来都还是 WindowText，
    /// 深色主题下刻度与文字就是黑的，怎么设都没用（本机实测过）。
    /// 自己画就没有这个限制：轨道、已选段、滑块、刻度全部用 ThemePalette 的颜色。
    /// </summary>
    public sealed class FlatSlider : Control
    {
        private int minimum;
        private int maximum = 100;
        private int current;
        private int tickFrequency = 10;
        private bool dragging;

        public event EventHandler ValueChanged;

        public FlatSlider()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer
                     | ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
            BackColor = ThemeManager.Palette.Surface;
            ForeColor = ThemeManager.Palette.PanelText;
            TabStop = true;
        }

        public int Minimum
        {
            get { return minimum; }
            set { minimum = value; if (current < minimum) Value = minimum; Invalidate(); }
        }

        public int Maximum
        {
            get { return maximum; }
            set { maximum = value; if (current > maximum) Value = maximum; Invalidate(); }
        }

        public int TickFrequency
        {
            get { return tickFrequency; }
            set { tickFrequency = Math.Max(1, value); Invalidate(); }
        }

        public int SmallChange;
        public int LargeChange;

        public int Value
        {
            get { return current; }
            set
            {
                int clamped = value < minimum ? minimum : (value > maximum ? maximum : value);
                if (clamped == current) return;
                current = clamped;
                Invalidate();
                if (ValueChanged != null) ValueChanged(this, EventArgs.Empty);
            }
        }

        private int HandleWidth { get { return 10; } }

        private int TrackLeft { get { return HandleWidth / 2; } }

        private int TrackWidth { get { return Math.Max(1, Width - HandleWidth); } }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            ThemePalette theme = ThemeManager.Palette;
            using (SolidBrush background = new SolidBrush(theme.Surface))
            {
                g.FillRectangle(background, 0, 0, Width, Height);
            }

            int middle = Height / 2;
            int left = TrackLeft;
            int width = TrackWidth;
            int span = Math.Max(1, maximum - minimum);
            int filled = (int)Math.Round((current - minimum) * (double)width / span);

            using (Pen track = new Pen(theme.GridLine, 3f))
            {
                g.DrawLine(track, left, middle, left + width, middle);
            }
            using (Pen done = new Pen(theme.TextOf(ThemeSlot.Done), 3f))
            {
                g.DrawLine(done, left, middle, left + filled, middle);
            }
            // 刻度线：用前景色画，深色主题下是浅色，看得见
            using (Pen tick = new Pen(theme.PanelText))
            {
                for (int v = minimum; v <= maximum; v += tickFrequency)
                {
                    int x = left + (int)Math.Round((v - minimum) * (double)width / span);
                    g.DrawLine(tick, x, middle + 6, x, middle + 10);
                }
            }
            // 滑块
            int handleX = left + filled - HandleWidth / 2;
            using (SolidBrush handle = new SolidBrush(theme.PanelText))
            {
                g.FillRectangle(handle, handleX, middle - 9, HandleWidth, 18);
            }
            using (Pen border = new Pen(theme.Surface))
            {
                g.DrawRectangle(border, handleX, middle - 9, HandleWidth - 1, 17);
            }
            if (Focused)
            {
                using (Pen focus = new Pen(theme.TextOf(ThemeSlot.Pending)))
                {
                    g.DrawRectangle(focus, 0, 0, Width - 1, Height - 1);
                }
            }
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            base.OnMouseDown(e);
            if (e.Button != MouseButtons.Left) return;
            dragging = true;
            Focus();
            SetFromX(e.X);
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            base.OnMouseMove(e);
            if (dragging) SetFromX(e.X);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            base.OnMouseUp(e);
            dragging = false;
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            base.OnKeyDown(e);
            int small = SmallChange > 0 ? SmallChange : 1;
            int large = LargeChange > 0 ? LargeChange : small * 5;
            if (e.KeyCode == Keys.Left) Value = current - small;
            else if (e.KeyCode == Keys.Right) Value = current + small;
            else if (e.KeyCode == Keys.PageDown) Value = current - large;
            else if (e.KeyCode == Keys.PageUp) Value = current + large;
            else if (e.KeyCode == Keys.Home) Value = minimum;
            else if (e.KeyCode == Keys.End) Value = maximum;
        }

        private void SetFromX(int x)
        {
            int span = Math.Max(1, maximum - minimum);
            double ratio = (x - TrackLeft) / (double)TrackWidth;
            if (ratio < 0) ratio = 0;
            if (ratio > 1) ratio = 1;
            Value = minimum + (int)Math.Round(ratio * span);
        }
    }

    /// <summary>颜色图例，按场景的配色表画，色块上同时画出形状线索。</summary>
    public sealed class LegendPanel : Control
    {
        private readonly ScenePalette palette;

        public LegendPanel(ScenePalette palette)
        {
            this.palette = palette;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer
                     | ControlStyles.UserPaint, true);
            BackColor = ThemeManager.Palette.Surface;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            ThemePalette theme = ThemeManager.Palette;
            g.Clear(theme.Surface);
            using (Pen border = new Pen(theme.GridLine))
            {
                g.DrawRectangle(border, 0, 0, Width - 1, Height - 1);
            }

            List<StateStyle> entries = new List<StateStyle>();
            entries.AddRange(palette.States);
            entries.AddRange(palette.Markers);
            entries.AddRange(palette.Extra);

            Font font = new Font(SystemFonts.MessageBoxFont.FontFamily, 8.5f);
            try
            {
                int perRow = 5;
                int columnWidth = (Width - 16) / perRow;
                int rowHeight = 19;
                for (int i = 0; i < entries.Count; i++)
                {
                    int row = i / perRow, col = i % perRow;
                    int x = 10 + col * columnWidth;
                    int y = 4 + row * rowHeight;
                    Color fill = theme.FillOf(entries[i].Slot);
                    using (SolidBrush brush = new SolidBrush(fill))
                    {
                        g.FillRectangle(brush, x, y + 3, 13, 12);
                    }
                    using (Pen pen = new Pen(theme.GridLine))
                    {
                        g.DrawRectangle(pen, x, y + 3, 13, 12);
                    }
                    DrawCue(g, entries[i].Cue, x, y + 3, 13, 12, theme);
                    TextRenderer.DrawText(g, entries[i].Name, font, new Point(x + 18, y + 2),
                                          theme.PanelText,
                                          TextFormatFlags.NoPadding | TextFormatFlags.NoPrefix
                                          | TextFormatFlags.EndEllipsis);
                }
            }
            finally
            {
                font.Dispose();
            }
        }

        /// <summary>把形状线索画在色块上。图例与画布用同一套画法，读者才对得上。</summary>
        public static void DrawCue(Graphics g, CueKind cue, int x, int y, int width, int height,
                                   ThemePalette theme)
        {
            if (cue == CueKind.None) return;
            Color color = theme.CueColor;
            int size = Math.Max(3, Math.Min(width, height) / 4);
            switch (cue)
            {
                case CueKind.CornerDot:
                    using (SolidBrush brush = new SolidBrush(color))
                    {
                        g.FillRectangle(brush, x + 2, y + 2, size, size);
                    }
                    break;
                case CueKind.Border:
                    using (Pen pen = new Pen(color, 2f))
                    {
                        g.DrawRectangle(pen, x + 1, y + 1, width - 3, height - 3);
                    }
                    break;
                case CueKind.Triangle:
                    using (SolidBrush brush = new SolidBrush(color))
                    {
                        g.FillPolygon(brush, new Point[]
                        {
                            new Point(x + 2, y + 2), new Point(x + 2 + size * 2, y + 2),
                            new Point(x + 2, y + 2 + size * 2)
                        });
                    }
                    break;
                case CueKind.Circle:
                    using (Pen pen = new Pen(color, 1.6f))
                    {
                        g.DrawEllipse(pen, x + 2, y + 2, size * 2, size * 2);
                    }
                    break;
                case CueKind.Cross:
                    using (Pen pen = new Pen(color, 1.6f))
                    {
                        g.DrawLine(pen, x + 2, y + 2, x + 2 + size * 2, y + 2 + size * 2);
                        g.DrawLine(pen, x + 2 + size * 2, y + 2, x + 2, y + 2 + size * 2);
                    }
                    break;
            }
        }
    }

    /// <summary>
    /// 窗口左下角的署名条。做成基类窗口的一部分，将来每个演示自动带上。
    /// 文字固定为 Written By HiBer2007，随窗口缩放与最大化停在底部。
    /// </summary>
    public sealed class SignatureBar : Panel
    {
        public const string SignatureText = "Written By HiBer2007";

        private readonly Label label;

        public SignatureBar()
        {
            Dock = DockStyle.Bottom;
            Height = 26;
            ThemePalette theme = ThemeManager.Palette;
            BackColor = theme.Surface;
            label = new Label();
            label.Text = SignatureText;
            label.AutoSize = true;
            label.Location = new Point(12, 5);
            label.Font = new Font(SystemFonts.MessageBoxFont.FontFamily, 8.5f);
            label.ForeColor = theme.PanelText;
            label.BackColor = theme.Surface;
            Controls.Add(label);
        }

        public void ApplyTheme(ThemePalette theme)
        {
            BackColor = theme.Surface;
            label.ForeColor = theme.PanelText;
            label.BackColor = theme.Surface;
            Invalidate();
        }
    }

    // ───────────────────────────────────────────────────────────── 入口

    /// <summary>
    /// 命令行分派：默认是界面进程，--worker 是运算进程，--selftest 在同进程里跑自测。
    /// 演示的 Program.Main 只要转调这里。
    /// </summary>
    public static class VisualCoreMain
    {
        public static int Run(string[] args, IVisualDemo demo, Func<Form> formFactory)
        {
            bool selfTest = false;
            bool paletteCheck = false;
            bool worker = false;
            int port = 0;
            string shmName = "";
            int parentPid = 0;

            for (int i = 0; i < args.Length; i++)
            {
                string argument = args[i];
                if (string.Equals(argument, "--selftest", StringComparison.OrdinalIgnoreCase)) selfTest = true;
                else if (string.Equals(argument, "--palettecheck", StringComparison.OrdinalIgnoreCase)) paletteCheck = true;
                else if (string.Equals(argument, "--worker", StringComparison.OrdinalIgnoreCase)) worker = true;
                else if (string.Equals(argument, "--trace", StringComparison.OrdinalIgnoreCase)) WorkerTrace.Enabled = true;
                else if (argument.StartsWith("--theme=", StringComparison.Ordinal))
                {
                    ThemeKind kind;
                    if (ThemeManager.TryParseKind(argument.Substring(8), out kind)) ThemeManager.Apply(kind);
                }
                else if (string.Equals(argument, "--theme", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
                {
                    ThemeKind kind;
                    if (ThemeManager.TryParseKind(args[++i], out kind)) ThemeManager.Apply(kind);
                }
                else if (argument.StartsWith("--port=", StringComparison.Ordinal))
                {
                    int.TryParse(argument.Substring(7), NumberStyles.Integer, CultureInfo.InvariantCulture, out port);
                }
                else if (argument.StartsWith("--shm=", StringComparison.Ordinal)) shmName = argument.Substring(6);
                else if (argument.StartsWith("--parent=", StringComparison.Ordinal))
                {
                    int.TryParse(argument.Substring(9), NumberStyles.Integer, CultureInfo.InvariantCulture, out parentPid);
                }
            }

            if (selfTest)
            {
                TextWriter writer = SelfTestHost.OpenStandardOutput();
                bool passed = demo.RunSelfTest(writer);
                SelfTestHost.Result(writer, passed);
                writer.Flush();
                return passed ? 0 : 1;
            }

            if (paletteCheck)
            {
                TextWriter writer = SelfTestHost.OpenStandardOutput();
                writer.WriteLine("palettecheck 只建窗体对象，不显示窗口，也不起运算进程");
                int code = PaletteCheckHost.Run(demo, formFactory, writer);
                writer.Flush();
                return code;
            }

            if (worker)
            {
                if (port <= 0 || shmName.Length == 0) return 2;
                return WorkerHost.Run(port, shmName, parentPid, demo.CreateEngine());
            }

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            // 主题要在建窗口之前定下来，控件一建出来就是对的颜色。
            ThemeManager.Apply(ThemeManager.Requested);
            Application.Run(formFactory());
            return 0;
        }
    }
}
