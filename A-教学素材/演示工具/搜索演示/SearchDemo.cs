// SearchDemo —— 搜索算法交互演示（本教材配套程序）
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
 *  SearchDemo.cs —— 搜索算法交互式演示（搜索演示自己的代码）
 *
 *  共享核心在 ..\_common\VisualCore.cs，两个文件一起交给编译器：
 *
 *    C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /nologo /target:winexe ^
 *      /out:SearchDemo.exe /r:System.Windows.Forms.dll /r:System.Drawing.dll ^
 *      ..\_common\VisualCore.cs SearchDemo.cs
 *
 *  用 dotnet SDK 时由 SearchDemo.csproj 里的 <Compile Include="..\_common\VisualCore.cs" /> 带进来。
 *  框架的说明见 演示工具/README.md。
 *
 *  运行：
 *    SearchDemo.exe               打开界面（默认拉起 --worker 运算进程）
 *    SearchDemo.exe --worker ...  运算进程身份，由界面进程自己拉起，不必手工启动
 *    SearchDemo.exe --selftest    同进程内跑一遍全部算法并逐行打印结果
 *
 *  地图规格：48×32 格，起点 (2,2)，终点 (45,29)，默认种子 12345。
 *  语法：C# 5。
 * ==========================================================================*/

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Windows.Forms;
using VisualCore;

namespace SearchDemo
{
    // ───────────────────────────────────────────────────────────── 全局常量

    internal static class Const
    {
        public const int DefaultWidth = 48;      // 地图宽（格）
        public const int DefaultHeight = 32;     // 地图高（格）
        public const int CellSize = 24;          // 每格像素边长（字号放大后 20 像素放不下三个字符，见 README「字号」一节）
        public const int StartX = 2, StartY = 2;
        public const int GoalX = 45, GoalY = 29;
        public const int DefaultSeed = 12345;

        // 代价单位取“毫代价”：一格的代价 1 记作 1000。
        // 斜步乘以 √2，内部取 1415（略大于 1000√2 ≈ 1414.214），
        // 这样齐次启发函数永远不高估，全部用整数运算，结果与浮点误差无关。
        public const int OrthoFactor = 1000;
        public const int DiagFactor = 1415;

        public const int MinCost = 1;
        public const int MaxCost = 9;

        // 元素标记：起终点用标记字节表示，颜色由配色表的 Markers 决定。
        public const byte MarkerStart = 1;
        public const byte MarkerGoal = 2;
    }

    // ───────────────────────────────────────────────────────────── 地图

    /// <summary>
    /// 地图：每格一个地形成本，取值 0 到 9。0 表示墙，不可通行；1 到 9 是通行代价。
    /// </summary>
    internal sealed class Grid
    {
        private readonly int width;
        private readonly int height;
        private readonly byte[] cost;

        public Grid(int width, int height)
        {
            this.width = width;
            this.height = height;
            cost = new byte[width * height];
            for (int i = 0; i < cost.Length; i++) cost[i] = 1;
        }

        private Grid(Grid source)
        {
            width = source.width;
            height = source.height;
            cost = (byte[])source.cost.Clone();
        }

        public Grid Clone()
        {
            return new Grid(this);
        }

        public int Width { get { return width; } }
        public int Height { get { return height; } }
        public int Count { get { return cost.Length; } }

        public int Index(int x, int y) { return y * width + x; }
        public int XOf(int index) { return index % width; }
        public int YOf(int index) { return index / width; }

        public bool InBounds(int x, int y)
        {
            return x >= 0 && y >= 0 && x < width && y < height;
        }

        public int CostOf(int index) { return cost[index]; }
        public int CostAt(int x, int y) { return cost[y * width + x]; }
        public bool IsWallIndex(int index) { return cost[index] == 0; }
        public bool IsWall(int x, int y) { return cost[y * width + x] == 0; }

        public void SetCostOf(int index, int value)
        {
            if (value < 0) value = 0;
            if (value > Const.MaxCost) value = Const.MaxCost;
            cost[index] = (byte)value;
        }

        public void SetCostAt(int x, int y, int value)
        {
            SetCostOf(y * width + x, value);
        }

        public void CopyCostsTo(byte[] destination) { Array.Copy(cost, destination, cost.Length); }
        public void CopyCostsFrom(byte[] source) { Array.Copy(source, cost, cost.Length); }
        public void CopyCostsFrom(Grid other) { Array.Copy(other.cost, cost, cost.Length); }

        /// <summary>全图可通行格的最小代价。齐次启发函数用它缩放，得到的一定是下界。</summary>
        public int MinTraversableCost()
        {
            int min = Const.MaxCost;
            for (int i = 0; i < cost.Length; i++)
            {
                if (cost[i] > 0 && cost[i] < min) min = cost[i];
            }
            return min;
        }

        /// <summary>把所有格子的地形成本改成同一个值，墙保持不动。用于无权图的对照实验。</summary>
        public void SetAllTraversableCosts(int value)
        {
            for (int i = 0; i < cost.Length; i++)
            {
                if (cost[i] != 0) cost[i] = (byte)value;
            }
        }
    }

    // ───────────────────────────────────────────────────────────── 地图生成

    /// <summary>
    /// 按固定种子生成伪随机地图。同一颗种子一定得到同一张地图：
    /// 每格的墙与代价只由这条随机数序列决定，生成过程中没有别的不确定因素。
    /// </summary>
    internal static class MapGen
    {
        public const int WallPermille = 280;   // 千分之几的格子是墙

        public static Grid Generate(int seed)
        {
            // 随机撒墙有可能把终点围死。这里最多重试若干次，取第一张连通的；
            // 重试次数与判据都是确定的，因此同一种子仍得到同一张地图。
            for (int attempt = 0; attempt < 200; attempt++)
            {
                DeterministicRng rng = new DeterministicRng(unchecked((uint)seed * 2654435761u
                                                              + (uint)attempt * 40503u + 1u));
                Grid grid = Build(rng);
                if (IsConnected(grid, Const.StartX, Const.StartY, Const.GoalX, Const.GoalY)) return grid;
            }

            // 兜底：一整张平地，保证起点与终点一定连通。
            return new Grid(Const.DefaultWidth, Const.DefaultHeight);
        }

        private static Grid Build(DeterministicRng rng)
        {
            Grid grid = new Grid(Const.DefaultWidth, Const.DefaultHeight);
            for (int y = 0; y < grid.Height; y++)
            {
                for (int x = 0; x < grid.Width; x++)
                {
                    int roll = rng.Next(1000);
                    int value;
                    if (roll < WallPermille) value = 0;              // 墙
                    else if (roll < 640) value = 1;                  // 平地
                    else if (roll < 760) value = 2;
                    else if (roll < 850) value = 3;
                    else if (roll < 920) value = 5;
                    else if (roll < 970) value = 7;
                    else value = 9;                                   // 最贵的地形
                    grid.SetCostAt(x, y, value);
                }
            }

            ClearArea(grid, Const.StartX, Const.StartY);
            ClearArea(grid, Const.GoalX, Const.GoalY);
            return grid;
        }

        /// <summary>把一格与它周围一圈清成平地，避免起点或终点一开局就被墙贴住。</summary>
        private static void ClearArea(Grid grid, int cx, int cy)
        {
            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    int x = cx + dx, y = cy + dy;
                    if (grid.InBounds(x, y)) grid.SetCostAt(x, y, 1);
                }
            }
        }

        /// <summary>四方向洪水填充，判断两点是否连通。</summary>
        public static bool IsConnected(Grid grid, int sx, int sy, int gx, int gy)
        {
            if (grid.IsWall(sx, sy) || grid.IsWall(gx, gy)) return false;
            bool[] seen = new bool[grid.Count];
            int[] stack = new int[grid.Count];
            int top = 0;
            stack[top++] = grid.Index(sx, sy);
            seen[stack[0]] = true;
            int[] dx = { 0, 1, 0, -1 };
            int[] dy = { -1, 0, 1, 0 };
            while (top > 0)
            {
                int cur = stack[--top];
                if (cur == grid.Index(gx, gy)) return true;
                int x = grid.XOf(cur), y = grid.YOf(cur);
                for (int k = 0; k < 4; k++)
                {
                    int nx = x + dx[k], ny = y + dy[k];
                    if (!grid.InBounds(nx, ny) || grid.IsWall(nx, ny)) continue;
                    int ni = grid.Index(nx, ny);
                    if (seen[ni]) continue;
                    seen[ni] = true;
                    stack[top++] = ni;
                }
            }
            return false;
        }
    }

    // ───────────────────────────────────────────────────────────── 优先队列

    /// <summary>
    /// 二叉最小堆，Dijkstra、A* 与贪心最佳优先靠它取出优先级最小的格子。
    /// 排序键是（优先级，入堆序号）：优先级相同时先入堆的先出，
    /// 这样同一张地图上的展开顺序完全确定，不受堆内部实现影响。
    /// </summary>
    internal sealed class MinHeap
    {
        private int[] priority = new int[256];
        private int[] sequence = new int[256];
        private int[] item = new int[256];
        private int[] stamp = new int[256];     // 入堆时该格的 g，用来识别过期条目
        private int count;

        public int Count { get { return count; } }

        public void Clear() { count = 0; }

        public void Push(int prio, int seq, int index, int gAtPush)
        {
            if (count == priority.Length) Grow();
            int i = count++;
            priority[i] = prio;
            sequence[i] = seq;
            item[i] = index;
            stamp[i] = gAtPush;
            while (i > 0)
            {
                int parent = (i - 1) / 2;
                if (Less(i, parent)) { Swap(i, parent); i = parent; }
                else break;
            }
        }

        /// <summary>弹出优先级最小的条目，prio 与 gAtPush 一并交出。</summary>
        public int Pop(out int prio, out int gAtPush)
        {
            prio = priority[0];
            gAtPush = stamp[0];
            int result = item[0];
            count--;
            if (count > 0)
            {
                priority[0] = priority[count];
                sequence[0] = sequence[count];
                item[0] = item[count];
                stamp[0] = stamp[count];
                int i = 0;
                for (; ; )
                {
                    int left = 2 * i + 1, right = left + 1, smallest = i;
                    if (left < count && Less(left, smallest)) smallest = left;
                    if (right < count && Less(right, smallest)) smallest = right;
                    if (smallest == i) break;
                    Swap(i, smallest);
                    i = smallest;
                }
            }
            return result;
        }

        private bool Less(int a, int b)
        {
            if (priority[a] != priority[b]) return priority[a] < priority[b];
            return sequence[a] < sequence[b];
        }

        private void Swap(int a, int b)
        {
            int t = priority[a]; priority[a] = priority[b]; priority[b] = t;
            t = sequence[a]; sequence[a] = sequence[b]; sequence[b] = t;
            t = item[a]; item[a] = item[b]; item[b] = t;
            t = stamp[a]; stamp[a] = stamp[b]; stamp[b] = t;
        }

        private void Grow()
        {
            int n = priority.Length * 2;
            Array.Resize(ref priority, n);
            Array.Resize(ref sequence, n);
            Array.Resize(ref item, n);
            Array.Resize(ref stamp, n);
        }
    }

    // ───────────────────────────────────────────────────────────── 算法枚举

    internal enum AlgorithmKind { Bfs = 0, Dfs = 1, Dijkstra = 2, AStar = 3, Greedy = 4 }

    internal enum HeuristicKind { Manhattan = 0, Euclidean = 1, Octile = 2 }

    internal enum DisplayKind { Cost = 0, G = 1, H = 2, F = 3, VisitOrder = 4 }

    // ───────────────────────────────────────────────────────────── 搜索算法基类

    /// <summary>
    /// 所有搜索算法的共同部分：地图、起终点、路径回溯与统计量。
    /// 派生类只实现 Step()：推进一次，也就是展开一个节点。
    /// </summary>
    internal abstract class SearchAlgorithm
    {
        protected readonly Grid Map;
        protected readonly bool Diagonal;
        protected readonly int StartIndex;
        protected readonly int GoalIndex;
        protected readonly int MapMinCost;

        protected readonly int[] G;          // 从起点到该格的累计代价（毫代价），int.MaxValue 表示还不知道
        protected readonly int[] H;          // 到终点的启发式估计，-1 表示这个算法没算过
        protected readonly int[] F;          // f = g + h
        protected readonly int[] Parent;     // 回溯用的前驱格，-1 表示没有
        private readonly int[] visitOrder;   // 第几个被展开（从 1 开始，0 表示还没展开）
        private readonly bool[] closed;
        private readonly bool[] opened;
        private int[] path;

        protected int ExpandedCount;
        protected int OpenSize;
        protected int OpenMaxSeen;
        protected int Steps;
        protected int CurrentCell = -1;
        protected bool Finished;
        protected bool PathFound;

        // 邻居顺序固定，展开顺序才可复现：
        // 四方向按 上、右、下、左；八方向按 上、右上、右、右下、下、左下、左、左上。
        private static readonly int[] Dx4 = { 0, 1, 0, -1 };
        private static readonly int[] Dy4 = { -1, 0, 1, 0 };
        private static readonly int[] Dx8 = { 0, 1, 1, 1, 0, -1, -1, -1 };
        private static readonly int[] Dy8 = { -1, -1, 0, 1, 1, 1, 0, -1 };

        protected SearchAlgorithm(Grid map, bool diagonal, int startIndex, int goalIndex)
        {
            Map = map;
            Diagonal = diagonal;
            StartIndex = startIndex;
            GoalIndex = goalIndex;
            MapMinCost = map.MinTraversableCost();

            int n = map.Count;
            G = new int[n];
            H = new int[n];
            F = new int[n];
            Parent = new int[n];
            visitOrder = new int[n];
            closed = new bool[n];
            opened = new bool[n];
            for (int i = 0; i < n; i++)
            {
                G[i] = int.MaxValue;
                H[i] = -1;
                Parent[i] = -1;
            }

            G[startIndex] = 0;
            opened[startIndex] = true;
            OpenSize = 1;
            OpenMaxSeen = 1;
        }

        // ── 对外只读状态

        public int Expanded { get { return ExpandedCount; } }
        public int OpenCount { get { return OpenSize; } }
        public int OpenMax { get { return OpenMaxSeen; } }
        public int StepCount { get { return Steps; } }
        public int Current { get { return CurrentCell; } }
        public bool Done { get { return Finished; } }
        public bool Found { get { return PathFound; } }
        public int[] PathCells { get { return path; } }
        public int Start { get { return StartIndex; } }
        public int Goal { get { return GoalIndex; } }

        public bool IsClosed(int index) { return closed[index]; }
        public bool IsOpened(int index) { return opened[index]; }
        public int VisitOrderOf(int index) { return visitOrder[index]; }
        public int GCostOf(int index) { return G[index] == int.MaxValue ? -1 : G[index]; }
        public int HCostOf(int index) { return H[index]; }
        public int FCostOf(int index)
        {
            return H[index] < 0 || G[index] == int.MaxValue ? -1 : G[index] + H[index];
        }

        public int PathLength { get { return path == null ? -1 : path.Length - 1; } }

        /// <summary>最终路径按地图真实地形成本累加的代价，单位是毫代价。</summary>
        public int PathCostMilli()
        {
            if (path == null) return -1;
            int total = 0;
            for (int i = 1; i < path.Length; i++) total += StepCostBetween(path[i - 1], path[i]);
            return total;
        }

        // ── 给派生类用的小工具

        protected int DirectionCount { get { return Diagonal ? 8 : 4; } }
        protected int Dx(int k) { return Diagonal ? Dx8[k] : Dx4[k]; }
        protected int Dy(int k) { return Diagonal ? Dy8[k] : Dy4[k]; }

        protected static bool IsDiagonalStep(int k)
        {
            return Dx8[k] != 0 && Dy8[k] != 0;
        }

        /// <summary>进入某格的代价。斜步按 √2 倍计价，与从哪个方向进入有关，与从哪一格出发无关。</summary>
        protected int StepCostTo(int x, int y, bool diagonal)
        {
            return Map.CostAt(x, y) * (diagonal ? Const.DiagFactor : Const.OrthoFactor);
        }

        private int StepCostBetween(int from, int to)
        {
            int fx = Map.XOf(from), fy = Map.YOf(from);
            int tx = Map.XOf(to), ty = Map.YOf(to);
            bool diagonal = (fx != tx) && (fy != ty);
            return StepCostTo(tx, ty, diagonal);
        }

        /// <summary>把一个格子记为“已展开”，并记录它是第几个被展开的。</summary>
        protected void MarkExpanded(int index)
        {
            closed[index] = true;
            if (opened[index]) { opened[index] = false; OpenSize--; }
            ExpandedCount++;
            visitOrder[index] = ExpandedCount;
            CurrentCell = index;
        }

        /// <summary>把一个格子放进 open 表，并更新 open 表大小的峰值。</summary>
        protected void MarkOpened(int index)
        {
            if (opened[index]) return;
            opened[index] = true;
            OpenSize++;
            if (OpenSize > OpenMaxSeen) OpenMaxSeen = OpenSize;
        }

        protected void FinishFound()
        {
            Finished = true;
            PathFound = true;
            BuildPath();
        }

        protected void FinishNoPath()
        {
            Finished = true;
            PathFound = false;
            path = null;
        }

        private void BuildPath()
        {
            List<int> cells = new List<int>();
            int cur = GoalIndex;
            int guard = Map.Count + 1;
            while (cur != -1 && guard-- > 0)
            {
                cells.Add(cur);
                cur = Parent[cur];
            }
            cells.Reverse();
            path = cells.ToArray();
        }

        /// <summary>
        /// 启发函数：估计“从这一格走到终点还要花多少代价”。
        ///
        /// 齐次模式用全图最小格代价缩放，得到的是真实剩余代价的下界，因此可采纳；
        /// 非齐次模式改用当前格自身的代价缩放，各格不同，遇到比最小代价贵的格子就会高估，
        /// 此时 A* 不再保证最优。界面上两种模式都能选，正是为了把这一点演示出来。
        /// </summary>
        protected int HeuristicValue(int index, HeuristicKind kind, bool homogeneous)
        {
            if (H[index] >= 0) return H[index];

            int x = Map.XOf(index), y = Map.YOf(index);
            int dx = Math.Abs(x - Map.XOf(GoalIndex));
            int dy = Math.Abs(y - Map.YOf(GoalIndex));
            int lo = Math.Min(dx, dy);
            int hi = Math.Max(dx, dy);

            long distance;   // 距离，单位是“走一格代价 1”的千分之一
            switch (kind)
            {
                case HeuristicKind.Euclidean:
                    distance = IntegerSqrt((long)Const.OrthoFactor * Const.OrthoFactor
                                           * ((long)dx * dx + (long)dy * dy));
                    break;
                case HeuristicKind.Octile:
                    // 八方向：斜走 lo 步、正交走 (hi-lo) 步，恰好是均匀代价地图上的最短距离。
                    // 四方向没有斜走，取切比雪夫形式（只数最大的那个方向），是一个更松的下界。
                    distance = Diagonal
                        ? (long)Const.OrthoFactor * (hi - lo) + (long)Const.DiagFactor * lo
                        : (long)Const.OrthoFactor * hi;
                    break;
                default:
                    distance = (long)Const.OrthoFactor * (dx + dy);
                    break;
            }

            int scale = homogeneous ? MapMinCost : Map.CostOf(index);
            long value = scale * distance;
            if (value > int.MaxValue - 1) value = int.MaxValue - 1;
            H[index] = (int)value;
            return H[index];
        }

        private static long IntegerSqrt(long value)
        {
            if (value <= 0) return 0;
            long r = (long)Math.Sqrt(value);
            while (r > 0 && r * r > value) r--;
            while ((r + 1) * (r + 1) <= value) r++;
            return r;
        }

        /// <summary>推进一次：展开一个节点。跑到最后会把 Finished 置位。</summary>
        public abstract void Step();

        /// <summary>一直跑到结束。上限只是防御性的，正常地图远达不到。</summary>
        public void RunToEnd()
        {
            int guard = Map.Count * 64 + 1000000;
            while (!Finished && guard-- > 0) Step();
        }

        /// <summary>把毫代价写成人看的数字：整千写成整数，否则保留三位小数。</summary>
        public static string CostText(int milli)
        {
            if (milli < 0) return "-";
            if (milli % Const.OrthoFactor == 0)
                return (milli / Const.OrthoFactor).ToString(CultureInfo.InvariantCulture);
            return (milli / (double)Const.OrthoFactor).ToString("0.###", CultureInfo.InvariantCulture);
        }

        /// <summary>印在格子上的短数字，八方向下不会出现太长的串。</summary>
        public static string ShortCostText(int milli)
        {
            if (milli < 0) return "";
            if (milli % Const.OrthoFactor == 0)
                return (milli / Const.OrthoFactor).ToString(CultureInfo.InvariantCulture);
            return (milli / (double)Const.OrthoFactor).ToString("0.0", CultureInfo.InvariantCulture);
        }
    }

    // ───────────────────────────────────────────────────────────── 广度优先

    /// <summary>
    /// 广度优先搜索（BFS）。
    ///
    /// 它在做什么：从起点出发一圈一圈往外扩，先把离起点 1 步的格子全部看过，
    /// 再看 2 步的。用先进先出队列实现，先进入队列的格子先被展开，
    /// 因此第一次弹出终点时走过的步数一定最少。
    ///
    /// 代价口径：BFS 把每一步都当成代价 1，完全不看地形成本，
    /// 它保证的是“步数最少”而不是“代价最小”。统计里的路径代价是按地图
    /// 真实地形成本另算的，所以 BFS 的代价通常高于 Dijkstra。
    /// </summary>
    internal sealed class BfsAlgorithm : SearchAlgorithm
    {
        private readonly Queue<int> frontier = new Queue<int>();

        public BfsAlgorithm(Grid map, bool diagonal, int startIndex, int goalIndex)
            : base(map, diagonal, startIndex, goalIndex)
        {
            frontier.Enqueue(startIndex);
        }

        public override void Step()
        {
            if (Finished) return;
            Steps++;

            if (frontier.Count == 0) { FinishNoPath(); return; }

            int cur = frontier.Dequeue();
            if (IsClosed(cur)) return;
            MarkExpanded(cur);

            if (cur == GoalIndex) { FinishFound(); return; }

            int x = Map.XOf(cur), y = Map.YOf(cur);
            for (int k = 0; k < DirectionCount; k++)
            {
                int nx = x + Dx(k), ny = y + Dy(k);
                if (!Map.InBounds(nx, ny) || Map.IsWall(nx, ny)) continue;
                int ni = Map.Index(nx, ny);
                if (IsOpened(ni) || IsClosed(ni)) continue;
                Parent[ni] = cur;
                G[ni] = G[cur] + Const.OrthoFactor;     // 每步按 1 计，不看地形成本
                MarkOpened(ni);
                frontier.Enqueue(ni);
            }
        }
    }

    // ───────────────────────────────────────────────────────────── 深度优先

    /// <summary>
    /// 深度优先搜索（DFS）。
    ///
    /// 它在做什么：沿着一条路一直往前，撞墙或者撞到走过的格子才回头换一条分支。
    /// 用后进先出栈实现，邻居按固定顺序入栈（上、右、下、左），结果因此可复现。
    ///
    /// 它既不保证步数最少，也不保证代价最小，找到的只是“第一条走通的路”。
    /// 把它放进演示是为了和 Dijkstra、A* 对照：同一张地图上 DFS 的路径常常绕远。
    /// </summary>
    internal sealed class DfsAlgorithm : SearchAlgorithm
    {
        private readonly Stack<int> frontier = new Stack<int>();

        public DfsAlgorithm(Grid map, bool diagonal, int startIndex, int goalIndex)
            : base(map, diagonal, startIndex, goalIndex)
        {
            frontier.Push(startIndex);
        }

        public override void Step()
        {
            if (Finished) return;
            Steps++;

            while (frontier.Count > 0 && IsClosed(frontier.Peek())) frontier.Pop();
            if (frontier.Count == 0) { FinishNoPath(); return; }

            int cur = frontier.Pop();
            MarkExpanded(cur);

            if (cur == GoalIndex) { FinishFound(); return; }

            int x = Map.XOf(cur), y = Map.YOf(cur);
            // 倒着入栈，弹出的顺序才与邻居表的顺序一致。
            for (int k = DirectionCount - 1; k >= 0; k--)
            {
                int nx = x + Dx(k), ny = y + Dy(k);
                if (!Map.InBounds(nx, ny) || Map.IsWall(nx, ny)) continue;
                int ni = Map.Index(nx, ny);
                if (IsOpened(ni) || IsClosed(ni)) continue;
                Parent[ni] = cur;
                G[ni] = G[cur] + Const.OrthoFactor;
                MarkOpened(ni);
                frontier.Push(ni);
            }
        }
    }

    // ───────────────────────────────────────────────────────────── 最佳优先族

    /// <summary>
    /// Dijkstra、A* 与贪心最佳优先共用的一层：都靠优先队列取下一个要展开的格子，
    /// 区别只在优先级怎么算——Dijkstra 用 g，A* 用 g+h，贪心只用 h。
    /// </summary>
    internal abstract class BestFirstAlgorithm : SearchAlgorithm
    {
        protected readonly MinHeap Heap = new MinHeap();
        private int sequence;

        protected BestFirstAlgorithm(Grid map, bool diagonal, int startIndex, int goalIndex)
            : base(map, diagonal, startIndex, goalIndex)
        {
        }

        protected void PushToHeap(int index, int priority)
        {
            Heap.Push(priority, sequence++, index, G[index]);
        }

        /// <summary>取堆顶，跳过过期条目（该格后来找到了更便宜的路）。</summary>
        protected bool TryPop(out int index)
        {
            while (Heap.Count > 0)
            {
                int prio, gAtPush;
                int cur = Heap.Pop(out prio, out gAtPush);
                if (gAtPush != G[cur]) continue;   // 过期条目
                index = cur;
                return true;
            }
            index = -1;
            return false;
        }

        /// <summary>
        /// 松弛一条边：如果经 cur 走到 ni 更便宜，就更新 g 并把 ni 重新放进 open 表。
        /// 允许重新打开已经展开过的格子——启发函数不可采纳或前后不一致时，
        /// 只有允许重开才能保证“可采纳的 h 一定给出最优路径”这一结论成立。
        /// </summary>
        protected void Relax(int cur, int ni, int stepCost)
        {
            int candidate = G[cur] + stepCost;
            if (candidate >= G[ni]) return;
            G[ni] = candidate;
            Parent[ni] = cur;
            MarkOpened(ni);
            OnImproved(ni);
        }

        /// <summary>g 变好之后由派生类决定优先级怎么算并重新入堆。</summary>
        protected abstract void OnImproved(int index);

        public override void Step()
        {
            if (Finished) return;
            Steps++;

            int cur;
            if (!TryPop(out cur)) { FinishNoPath(); return; }

            MarkExpanded(cur);
            if (cur == GoalIndex) { FinishFound(); return; }

            int x = Map.XOf(cur), y = Map.YOf(cur);
            for (int k = 0; k < DirectionCount; k++)
            {
                int nx = x + Dx(k), ny = y + Dy(k);
                if (!Map.InBounds(nx, ny) || Map.IsWall(nx, ny)) continue;
                Relax(cur, Map.Index(nx, ny), StepCostTo(nx, ny, IsDiagonalStep(k)));
            }
        }
    }

    /// <summary>
    /// Dijkstra 算法（一致代价搜索）。
    ///
    /// 它在做什么：每一轮从 open 表里取出“从起点到它已经花的代价 g 最小”的格子展开，
    /// 再用真实地形成本去松弛它的邻居。因为总是先处理 g 最小的格子，
    /// 第一次弹出的终点一定已经是最便宜的那条路。
    ///
    /// 它不看终点在哪，展开的节点多，是“一定最优”的基准；本演示用它检验 A* 的结果。
    /// </summary>
    internal sealed class DijkstraAlgorithm : BestFirstAlgorithm
    {
        public DijkstraAlgorithm(Grid map, bool diagonal, int startIndex, int goalIndex)
            : base(map, diagonal, startIndex, goalIndex)
        {
            PushToHeap(startIndex, 0);
        }

        protected override void OnImproved(int index)
        {
            PushToHeap(index, G[index]);
        }
    }

    /// <summary>
    /// A* 算法。
    ///
    /// 它在做什么：在 Dijkstra 之上加一个启发函数 h，优先级取 f = g + h，
    /// 也就是“已经花掉的代价 + 还要花的代价的估计”。h 把搜索往终点方向拉，
    /// 展开的节点通常比 Dijkstra 少得多，而只要 h 不高估真实剩余代价（可采纳），
    /// 结果仍然最优。
    /// </summary>
    internal sealed class AStarAlgorithm : BestFirstAlgorithm
    {
        private readonly HeuristicKind kind;
        private readonly bool homogeneous;

        public AStarAlgorithm(Grid map, bool diagonal, int startIndex, int goalIndex,
                              HeuristicKind kind, bool homogeneous)
            : base(map, diagonal, startIndex, goalIndex)
        {
            this.kind = kind;
            this.homogeneous = homogeneous;
            F[startIndex] = HeuristicValue(startIndex, kind, homogeneous);
            PushToHeap(startIndex, F[startIndex]);
        }

        protected override void OnImproved(int index)
        {
            F[index] = G[index] + HeuristicValue(index, kind, homogeneous);
            PushToHeap(index, F[index]);
        }
    }

    /// <summary>
    /// 贪心最佳优先搜索（Greedy Best-First）。
    ///
    /// 它在做什么：只看 h，谁看起来离终点近就先展开谁，完全不看已经花掉的代价 g。
    /// 优先队列的键就是 h。
    ///
    /// 它通常展开的节点最少、跑得最快，但没有任何最优性保证：
    /// 只要把一条实际很贵的路估得离终点很近，它就会一头扎进去。
    /// </summary>
    internal sealed class GreedyAlgorithm : BestFirstAlgorithm
    {
        private readonly HeuristicKind kind;
        private readonly bool homogeneous;

        public GreedyAlgorithm(Grid map, bool diagonal, int startIndex, int goalIndex,
                               HeuristicKind kind, bool homogeneous)
            : base(map, diagonal, startIndex, goalIndex)
        {
            this.kind = kind;
            this.homogeneous = homogeneous;
            H[startIndex] = HeuristicValue(startIndex, kind, homogeneous);
            PushToHeap(startIndex, H[startIndex]);
        }

        protected override void OnImproved(int index)
        {
            H[index] = HeuristicValue(index, kind, homogeneous);
            PushToHeap(index, H[index]);
        }

        public override void Step()
        {
            if (Finished) return;
            Steps++;

            int cur;
            if (!TryPop(out cur)) { FinishNoPath(); return; }

            MarkExpanded(cur);
            if (cur == GoalIndex) { FinishFound(); return; }

            int x = Map.XOf(cur), y = Map.YOf(cur);
            for (int k = 0; k < DirectionCount; k++)
            {
                int nx = x + Dx(k), ny = y + Dy(k);
                if (!Map.InBounds(nx, ny) || Map.IsWall(nx, ny)) continue;
                int ni = Map.Index(nx, ny);
                if (IsOpened(ni) || IsClosed(ni)) continue;   // 贪心不重开已见过的格子
                G[ni] = G[cur] + StepCostTo(nx, ny, IsDiagonalStep(k));
                Parent[ni] = cur;
                MarkOpened(ni);
                OnImproved(ni);
            }
        }
    }

    // ───────────────────────────────────────────────────────────── 运算侧：搜索引擎

    /// <summary>
    /// 把五种搜索算法包成核心要的 IVisualEngine。
    ///
    /// 元素就是格子：属性 0 放地形成本（场景数据），属性 1 到 4 放 g、h、f、访问次序；
    /// 标记字节放起点与终点。核心不认识这些含义，它只管搬运。
    /// </summary>
    internal sealed class SearchEngine : IVisualEngine
    {
        public const int AttrCost = 0;
        public const int AttrG = 1;
        public const int AttrH = 2;
        public const int AttrF = 3;
        public const int AttrOrder = 4;

        private Grid map;
        private SearchAlgorithm algorithm;
        private int startIndex;
        private int goalIndex;
        private int seed = Const.DefaultSeed;

        private AlgorithmKind kind = AlgorithmKind.Bfs;
        private HeuristicKind heuristic = HeuristicKind.Manhattan;
        private bool homogeneous = true;
        private bool diagonal;

        public SearchEngine()
        {
            map = MapGen.Generate(seed);
            startIndex = map.Index(Const.StartX, Const.StartY);
            goalIndex = map.Index(Const.GoalX, Const.GoalY);
            Rebuild();
        }

        public int ElementCount { get { return map.Count; } }
        public SearchAlgorithm Algorithm { get { return algorithm; } }
        public int Seed { get { return seed; } set { seed = value; } }

        public void Configure(int mode, int optionA, int optionB)
        {
            kind = (AlgorithmKind)mode;
            heuristic = (HeuristicKind)(optionA / 2);
            homogeneous = optionA % 2 == 0;
            diagonal = optionB == 1;
            Rebuild();
        }

        /// <summary>场景数据由界面进程负责生成，这里只把算法重建一遍。</summary>
        public void ResetAll()
        {
            Rebuild();
        }

        public void ResetRun()
        {
            Rebuild();
        }

        public void StepOnce()
        {
            algorithm.Step();
        }

        public bool Finished { get { return algorithm.Done; } }
        public bool HasResult { get { return algorithm.Found; } }

        private void Rebuild()
        {
            switch (kind)
            {
                case AlgorithmKind.Dfs:
                    algorithm = new DfsAlgorithm(map, diagonal, startIndex, goalIndex);
                    break;
                case AlgorithmKind.Dijkstra:
                    algorithm = new DijkstraAlgorithm(map, diagonal, startIndex, goalIndex);
                    break;
                case AlgorithmKind.AStar:
                    algorithm = new AStarAlgorithm(map, diagonal, startIndex, goalIndex, heuristic, homogeneous);
                    break;
                case AlgorithmKind.Greedy:
                    algorithm = new GreedyAlgorithm(map, diagonal, startIndex, goalIndex, heuristic, homogeneous);
                    break;
                default:
                    algorithm = new BfsAlgorithm(map, diagonal, startIndex, goalIndex);
                    break;
            }
        }

        /// <summary>把地形成本与起终点写进快照的属性与标记。</summary>
        public void ExportScene(VisualSnapshot snapshot)
        {
            for (int i = 0; i < map.Count; i++)
            {
                snapshot.Attr0[i] = map.CostOf(i);
                snapshot.Marker[i] = 0;
            }
            snapshot.Marker[startIndex] = Const.MarkerStart;
            snapshot.Marker[goalIndex] = Const.MarkerGoal;
            snapshot.Seed = seed;
        }

        /// <summary>界面进程改过地图之后，从快照里读回地形成本与起终点。</summary>
        public void ImportScene(VisualSnapshot snapshot)
        {
            byte[] costs = new byte[map.Count];
            for (int i = 0; i < map.Count; i++) costs[i] = (byte)snapshot.Attr0[i];
            map.CopyCostsFrom(costs);

            int start = -1, goal = -1;
            for (int i = 0; i < map.Count; i++)
            {
                if (snapshot.Marker[i] == Const.MarkerStart) start = i;
                else if (snapshot.Marker[i] == Const.MarkerGoal) goal = i;
            }
            if (start >= 0) startIndex = start;
            if (goal >= 0) goalIndex = goal;
            seed = snapshot.Seed;
            Rebuild();
        }

        /// <summary>把运行状态摊进快照：每格的状态、g/h/f/访问次序，以及头部统计量。</summary>
        public void RefreshSnapshot(VisualSnapshot snapshot)
        {
            bool[] onPath = null;
            if (algorithm.Found && algorithm.PathCells != null)
            {
                onPath = new bool[map.Count];
                int[] path = algorithm.PathCells;
                for (int i = 0; i < path.Length; i++) onPath[path[i]] = true;
            }

            for (int i = 0; i < map.Count; i++)
            {
                byte state;
                if (map.CostOf(i) == 0) state = ElementStates.Blocked;
                else if (onPath != null && onPath[i]) state = ElementStates.Result;
                else if (algorithm.IsClosed(i)) state = ElementStates.Done;
                else if (algorithm.IsOpened(i)) state = ElementStates.Pending;
                else state = ElementStates.Empty;
                if (i == algorithm.Current && !algorithm.Done) state = ElementStates.Active;
                snapshot.State[i] = state;

                snapshot.Marker[i] = i == startIndex ? Const.MarkerStart
                                   : (i == goalIndex ? Const.MarkerGoal : (byte)0);
                snapshot.Attr0[i] = map.CostOf(i);
                snapshot.Attr1[i] = algorithm.GCostOf(i);
                snapshot.Attr2[i] = algorithm.HCostOf(i);
                snapshot.Attr3[i] = algorithm.FCostOf(i);
                snapshot.Attr4[i] = algorithm.VisitOrderOf(i);
            }

            snapshot.WorkCount = algorithm.Expanded;
            snapshot.PendingCount = algorithm.OpenCount;
            snapshot.PrimaryMetric = algorithm.Found ? algorithm.PathCostMilli() : -1;
            snapshot.SecondaryMetric = algorithm.PathLength;
            snapshot.CurrentIndex = algorithm.Current;
            snapshot.FocusIndex = startIndex;
            snapshot.CustomA = goalIndex;
            snapshot.CustomB = seed;
            snapshot.StateCode = algorithm.Done
                ? (algorithm.Found ? SearchStateKind.Found : SearchStateKind.NoPath)
                : (algorithm.StepCount > 0 ? SearchStateKind.Paused : SearchStateKind.Idle);
        }
    }

    internal static class SearchStateKind
    {
        public const int Idle = 0;
        public const int Paused = 1;
        public const int Found = 2;
        public const int NoPath = 3;
    }

    // ───────────────────────────────────────────────────────────── 渲染侧：搜索场景

    /// <summary>
    /// 搜索场景：方格网、每格一个色块、块上写数字，工具是墙壁／擦除／地形代价／起点／终点。
    /// 地图这一份由界面进程持有，改完通过共享内存交给运算进程。
    /// </summary>
    internal sealed class SearchScene : IVisualScene, ISeedProvider
    {
        private readonly Grid map;
        private readonly ScenePalette palette = new ScenePalette();
        private readonly ToolDescriptor[] tools;
        private readonly int[] brushSizes = { 1, 3, 5 };
        private readonly UndoStack undo = new UndoStack();
        // 格上标注统一用这一个字号，不再为塞下长数字而换小号字——
        // 空间从标注内容里省（见 Compact），不从字号里省。
        // 字号按像素给、由 VisualCore 的字号表统一管，--layoutcheck 会断言它不低于下限。
        private static readonly Font font = VisualFonts.DigitFont(13f);
        // 等宽字体的字符宽度：两次度量之差（.NET Framework 的度量带一段与字数无关的余量）
        private static readonly int CharWidth = MeasureCharWidth();

        private static int MeasureCharWidth()
        {
            int one = TextRenderer.MeasureText("0", font, new Size(4000, 100),
                                               TextFormatFlags.NoPadding).Width;
            int two = TextRenderer.MeasureText("00", font, new Size(4000, 100),
                                               TextFormatFlags.NoPadding).Width;
            return two - one;
        }

        private int startX = Const.StartX, startY = Const.StartY;
        private int goalX = Const.GoalX, goalY = Const.GoalY;
        private int seed = Const.DefaultSeed;
        private Grid gestureSnapshot;
        private int gestureStartX, gestureStartY, gestureGoalX, gestureGoalY;
        private bool gestureChanged;
        private int randomWallRound;

        public SearchScene()
        {
            map = MapGen.Generate(seed);

            // 状态只用填充色区分（作者 2026-10-02 决定）：不加叉、角点、三角、圆、边框这些线索。
            // 唯一保留的图形元素是最终路径的折线，见 DrawPathLine。
            palette.AddState(ElementStates.Empty, "空地 代价 1", ThemeSlot.Empty);
            palette.AddState(ElementStates.Pending, "待访问 open", ThemeSlot.Pending);
            palette.AddState(ElementStates.Done, "已访问 closed", ThemeSlot.Done);
            palette.AddState(ElementStates.Active, "当前展开的节点", ThemeSlot.Active);
            palette.AddState(ElementStates.Result, "最终路径", ThemeSlot.Result);
            palette.AddState(ElementStates.Blocked, "墙（不可通行）", ThemeSlot.Blocked);
            palette.AddMarker(Const.MarkerStart, "起点", ThemeSlot.Start, CueKind.None);
            palette.AddMarker(Const.MarkerGoal, "终点", ThemeSlot.Goal, CueKind.None);
            palette.AddExtra("空地 代价 2-9", ThemeSlot.Terrain, CueKind.None);
            palette.AddExtra("路径连线", ThemeSlot.Result, CueKind.None);

            tools = new ToolDescriptor[]
            {
                new ToolDescriptor(0, "墙壁", true, false),
                new ToolDescriptor(1, "擦除", true, false),
                new ToolDescriptor(2, "地形代价", true, true),
                new ToolDescriptor(3, "起点", false, false),
                new ToolDescriptor(4, "终点", false, false),
            };
        }

        public string Title { get { return "搜索算法演示"; } }
        public int ElementCount { get { return map.Count; } }
        public Size CanvasSize
        {
            get { return new Size(Const.DefaultWidth * Const.CellSize, Const.DefaultHeight * Const.CellSize); }
        }
        public ScenePalette Palette { get { return palette; } }
        public ToolDescriptor[] Tools { get { return tools; } }
        public int[] BrushSizes { get { return brushSizes; } }
        public bool UsesValue { get { return true; } }
        public int MinValue { get { return Const.MinCost; } }
        public int MaxValue { get { return Const.MaxCost; } }
        public int Seed { get { return seed; } }

        public string HintText
        {
            get
            {
                return "左键：按当前工具落笔，按住拖动连续生效    右键：擦除，"
                     + "「地形代价」工具下改为把该格代价循环 1-9\r\n"
                     + "工具：墙壁 / 擦除 / 地形代价 / 起点 / 终点，刷子 1x1、3x3、5x5，"
                     + "起点或终点落在墙上时自动清掉那一格的墙\r\n"
                     + "空格：开始或暂停    右方向键：单步    R：重置    Ctrl+Z：撤销    "
                     + "编辑地图会自动暂停，并清空上一次的搜索状态\r\n"
                     + "两进程模式：界面只管画，算法在 --worker 子进程里跑；"
                     + "两种运算模式的实测 sps 在统计面板里对照\r\n"
                     + "主题：浅色 / 深色 / 跟随系统，命令行可用 --theme light|dark|auto；"
                     + "署名在窗口左下角，窗口缩放与最大化时都留在底部";
            }
        }

        public int HitTest(int x, int y)
        {
            int cx = x / Const.CellSize, cy = y / Const.CellSize;
            if (!map.InBounds(cx, cy)) return -1;
            return map.Index(cx, cy);
        }

        /// <summary>按种子重新生成地图。同一种子两个进程各生成一份，结果必然相同。</summary>
        public void Regenerate(int newSeed)
        {
            seed = newSeed;
            Grid fresh = MapGen.Generate(newSeed);
            map.CopyCostsFrom(fresh);
            startX = Const.StartX;
            startY = Const.StartY;
            goalX = Const.GoalX;
            goalY = Const.GoalY;
            undo.Clear();
        }

        public void ClearWalls()
        {
            for (int i = 0; i < map.Count; i++)
            {
                if (map.CostOf(i) == 0) map.SetCostOf(i, Const.MinCost);
            }
        }

        public void ScatterWalls()
        {
            randomWallRound++;
            DeterministicRng rng = new DeterministicRng(unchecked((uint)seed * 2246822519u
                                                            + (uint)randomWallRound * 3266489917u + 7u));
            int start = StartIndex, goal = GoalIndex;
            for (int i = 0; i < map.Count; i++)
            {
                if (i == start || i == goal || map.IsWallIndex(i)) continue;
                if (rng.Next(1000) < 70) map.SetCostOf(i, 0);      // 约 7% 的格子变成墙
            }
        }

        private int StartIndex { get { return map.Index(startX, startY); } }
        private int GoalIndex { get { return map.Index(goalX, goalY); } }

        // ── 绘制

        public void Paint(Graphics g, VisualSnapshot snapshot, SceneViewState viewState)
        {
            int cw = Const.CellSize, ch = Const.CellSize;
            int width = Const.DefaultWidth, height = Const.DefaultHeight;
            ThemePalette theme = ThemeManager.Palette;
            using (SolidBrush background = new SolidBrush(theme.Background))
            {
                g.FillRectangle(background, 0, 0, width * cw, height * ch);
            }

            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    int index = y * width + x;
                    byte state = snapshot.State[index];
                    byte marker = snapshot.Marker[index];
                    StateStyle style = marker != 0 ? palette.FindMarker(marker) : palette.FindState(state);
                    Color fill = style == null ? theme.Background : theme.FillOf(style.Slot);
                    // 还没被访问过的格子按地形成本换底色：代价 1 与 2 到 9 分开。
                    // 被访问过的格子让状态颜色说话，地形成本改用格子上写的数字表达。
                    if (marker == 0 && state == ElementStates.Empty && snapshot.Attr0[index] > 1)
                    {
                        fill = theme.FillOf(ThemeSlot.Terrain);
                    }
                    using (SolidBrush brush = new SolidBrush(fill))
                    {
                        g.FillRectangle(brush, x * cw, y * ch, cw, ch);
                    }
                }
            }

            using (Pen line = new Pen(theme.GridLine))
            {
                for (int x = 0; x <= width; x++) g.DrawLine(line, x * cw, 0, x * cw, height * ch);
                for (int y = 0; y <= height; y++) g.DrawLine(line, 0, y * ch, width * cw, y * ch);
            }

            // 最终路径画成一条连线，不只是换颜色：打印成黑白时也认得出。
            DrawPathLine(g, snapshot, cw, ch, theme);

            TextFormatFlags flags = TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter
                                    | TextFormatFlags.NoPadding | TextFormatFlags.NoPrefix;
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    int index = y * width + x;
                    string text = ElementText(index, snapshot, viewState);
                    if (text.Length == 0) continue;
                    StateStyle style = snapshot.Marker[index] != 0
                        ? palette.FindMarker(snapshot.Marker[index])
                        : palette.FindState(snapshot.State[index]);
                    Color color = style == null ? theme.PanelText : theme.TextOf(style.Slot);
                    // 字号只有一个，不为塞下长数字换小号字；文字已在 Compact 里压到三字符以内
                    Rectangle box = new Rectangle(x * cw, y * ch, cw, ch);
                    // 路径格上的数字被折线从格心穿过。把数字画在折线之后仍不够：
                    // GDI 画字带抗锯齿，深色数字压在折线的深色描边上会糊成一团。
                    // 这里在数字上下各垫一份「与路径块同色」的字，把压在字上的那两道细描边
                    // 在字的位置断开（各让开 1 像素），数字就完整了，折线在字的两侧照常连着。
                    if (snapshot.State[index] == ElementStates.Result)
                    {
                        Color pad = theme.FillOf(ThemeSlot.Result);
                        TextRenderer.DrawText(g, text, font,
                            new Rectangle(box.X, box.Y - 1, box.Width, box.Height), pad, flags);
                        TextRenderer.DrawText(g, text, font,
                            new Rectangle(box.X, box.Y + 1, box.Width, box.Height), pad, flags);
                    }
                    TextRenderer.DrawText(g, text, font, box, color, flags);
                }
            }

            // 状态只用填充色区分（作者 2026-10-02 的决定）：不加叉、角点、三角、圆、
            // 加粗边框、斜线纹理这类线索。色表里每个条目的 Cue 都是 CueKind.None，
            // 因此下面这段循环实际不画任何东西；线索机制留在核心里备用，本演示不用。
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    int index = y * width + x;
                    byte marker = snapshot.Marker[index];
                    StateStyle style = marker != 0 ? palette.FindMarker(marker) : palette.FindState(snapshot.State[index]);
                    if (style == null || style.Cue == CueKind.None) continue;
                    LegendPanel.DrawCue(g, style.Cue, x * cw, y * ch, cw, ch, theme);
                }
            }
        }

        /// <summary>
        /// 把最终路径按格心连成折线。
        ///
        /// 两个要点：
        /// 一是**顺序**。路径格子必须按搜索走的次序连，不能按格子下标连——
        /// 按下标连会把不相邻的格子接起来，屏幕上就出现一段段斜线。
        /// 路径上每一步的 g 都严格递增（每步至少加最小代价），因此按 g 升序排就是路径次序。
        /// 二是**只许横竖**。相邻两格若落在斜方向上（八方向模式），
        /// 也拆成「先横后竖」两段，拐点在两格之间，绝不画斜段。
        /// </summary>
        private void DrawPathLine(Graphics g, VisualSnapshot snapshot, int cw, int ch, ThemePalette theme)
        {
            if (snapshot.PrimaryMetric < 0) return;
            List<PathSegment> segments = PathSegments(snapshot, cw, ch);
            if (segments.Count == 0) return;
            // 两遍描：先描边色，再压主色，总宽压在 4 像素以内
            // （描边 3.0、主色 1.8，露出来的描边每侧不到 1 像素）。
            // 折线会跨过空地、地形、已访问、墙这些不同底色，单色做不到在每种底色上都清楚，
            // 两色一起就能保证总有一色与底色分得开。主色与路径块的填充色接近，
            // 因此格上的数字压在折线上仍然读得动。
            using (Pen halo = new Pen(theme.PathHalo, 3.0f))
            using (Pen line = new Pen(theme.PathLine, 1.8f))
            {
                halo.LineJoin = System.Drawing.Drawing2D.LineJoin.Round;
                line.LineJoin = System.Drawing.Drawing2D.LineJoin.Round;
                for (int i = 0; i < segments.Count; i++)
                {
                    g.DrawLine(halo, segments[i].X1, segments[i].Y1, segments[i].X2, segments[i].Y2);
                }
                for (int i = 0; i < segments.Count; i++)
                {
                    g.DrawLine(line, segments[i].X1, segments[i].Y1, segments[i].X2, segments[i].Y2);
                }
            }
        }

        /// <summary>
        /// 自检用：把五种「元素上标」模式都走一遍，量最宽的标注文字，
        /// 返回放不下的格子数（0 表示都放得下）。字号不许为了塞下而调小，
        /// 放不下就该放大格子或缩短标注。
        /// </summary>
        public int CheckCellText(VisualSnapshot snapshot, TextWriter writer)
        {
            int cell = Const.CellSize;
            int over = 0;
            // .NET Framework 的 TextRenderer 度量里带一段与字数无关的余量（约 7 像素），
            // 空串又量成 0，因此不能靠减空串去掉。Consolas 是等宽字体，
            // 用「两个字符的宽度减一个字符的宽度」得到真正的字符宽度，再乘字数。
            int one = TextRenderer.MeasureText("0", font, new Size(4000, 100),
                                               TextFormatFlags.NoPadding).Width;
            int two = TextRenderer.MeasureText("00", font, new Size(4000, 100),
                                               TextFormatFlags.NoPadding).Width;
            int advance = two - one;
            int worst = 0;
            string worstText = "";
            SceneViewState view = new SceneViewState();
            for (int mode = 0; mode <= 4; mode++)
            {
                view.DisplayMode = mode;
                for (int i = 0; i < snapshot.Elements; i++)
                {
                    string text = ElementText(i, snapshot, view);
                    if (text.Length == 0) continue;
                    int width = text.Length * advance;
                    if (width > worst)
                    {
                        worst = width;
                        worstText = text;
                    }
                    if (width > cell) over++;
                }
            }
            writer.WriteLine("  note 字符宽度 = " + advance + " 像素（等宽字体，两次度量之差）");
            writer.WriteLine(VisualFonts.Line("格上数字", font, "，写在格子中央（格子 " + cell + " 像素）"));
            bool fontOk = SelfTestHost.Check(writer, "min_digit_font_at_least_"
                + VisualFonts.MinimumDigitPx.ToString("0.#", CultureInfo.InvariantCulture),
                VisualFonts.DigitOk(font),
                "本演示的最小数字字号 " + font.Size.ToString("0.#", CultureInfo.InvariantCulture) + " 像素（格上数字）");
            writer.WriteLine("  note 格上标注：格子 " + cell + " 像素，字体 " + font.Name + " "
                             + font.Size.ToString("0.#", CultureInfo.InvariantCulture) + " "
                             + font.Unit + "（行高 " + font.Height + "），最宽的一处 [" + worstText + "] 需 " + worst
                             + " 像素，放不下的格子数 " + over);
            return over + (fontOk ? 0 : 1);
        }

        /// <summary>路径点在画布上的坐标。</summary>
        public struct PathPoint
        {
            public int X;
            public int Y;
            public int Element;
        }

        /// <summary>
        /// 取出路径点列，顺序就是搜索走的次序。绘制与自检共用这一份，
        /// 因此自检断言的就是屏幕上画的那条线。
        /// </summary>
        public static List<PathPoint> PathPoints(VisualSnapshot snapshot, int cw, int ch)
        {
            List<PathPoint> points = new List<PathPoint>();
            for (int index = 0; index < snapshot.Elements; index++)
            {
                if (snapshot.State[index] != ElementStates.Result) continue;
                PathPoint point = new PathPoint();
                point.Element = index;
                point.X = index % Const.DefaultWidth * cw + cw / 2;
                point.Y = index / Const.DefaultWidth * ch + ch / 2;
                points.Add(point);
            }
            points.Sort(delegate(PathPoint a, PathPoint b)
            {
                return snapshot.Attr1[a.Element].CompareTo(snapshot.Attr1[b.Element]);
            });
            return points;
        }

        /// <summary>
        /// 实际画出去的线段。绘制与自检都用这一份，因此自检断言的就是屏幕上的线。
        /// 相邻两格落在斜方向上时，这里拆成「先横后竖」两段，绝不产出斜段。
        /// </summary>
        public static List<PathSegment> PathSegments(VisualSnapshot snapshot, int cw, int ch)
        {
            List<PathPoint> points = PathPoints(snapshot, cw, ch);
            List<PathSegment> segments = new List<PathSegment>();
            for (int i = 1; i < points.Count; i++)
            {
                PathPoint a = points[i - 1];
                PathPoint b = points[i];
                if (a.Y == b.Y || a.X == b.X)
                {
                    PathSegment straight = new PathSegment();
                    straight.X1 = a.X; straight.Y1 = a.Y; straight.X2 = b.X; straight.Y2 = b.Y;
                    straight.Split = false;
                    segments.Add(straight);
                }
                else
                {
                    PathSegment first = new PathSegment();
                    first.X1 = a.X; first.Y1 = a.Y; first.X2 = b.X; first.Y2 = a.Y;   // 先横
                    first.Split = true;
                    segments.Add(first);
                    PathSegment second = new PathSegment();
                    second.X1 = b.X; second.Y1 = a.Y; second.X2 = b.X; second.Y2 = b.Y;  // 后竖
                    second.Split = true;
                    segments.Add(second);
                }
            }
            return segments;
        }

        /// <summary>画出去的一段线：两点加一个「是不是斜走拆出来的」标记。</summary>
        public struct PathSegment
        {
            public int X1;
            public int Y1;
            public int X2;
            public int Y2;
            public bool Split;
        }

        /// <summary>
        /// 自检用：检查**实际画出去**的每一段是不是水平或垂直，返回斜段的条数。
        /// 顺便报出有多少处斜走被拆成了 L 形。
        /// </summary>
        public static int CountDiagonalSegments(VisualSnapshot snapshot, int cw, int ch, List<string> report)
        {
            List<PathSegment> segments = PathSegments(snapshot, cw, ch);
            int diagonal = 0;
            int split = 0;
            for (int i = 0; i < segments.Count; i++)
            {
                PathSegment segment = segments[i];
                bool horizontal = segment.Y1 == segment.Y2;
                bool vertical = segment.X1 == segment.X2;
                if (segment.Split) split++;
                if (!horizontal && !vertical)
                {
                    diagonal++;
                    if (report != null)
                    {
                        report.Add("seg " + (i + 1) + ": (" + segment.X1 + "," + segment.Y1 + ")->("
                                   + segment.X2 + "," + segment.Y2 + ") 斜段");
                    }
                }
                else if (report != null && i < 12)
                {
                    report.Add("seg " + (i + 1) + ": (" + segment.X1 + "," + segment.Y1 + ")->("
                               + segment.X2 + "," + segment.Y2 + ") " + (horizontal ? "水平" : "垂直"));
                }
            }
            if (report != null)
            {
                report.Add("共 " + segments.Count + " 段，其中 " + split / 2 + " 处斜走拆成了 L 形");
            }
            return diagonal;
        }

        /// <summary>
        /// 格子上写什么：选地形成本时写代价；
        /// 选 g、h、f、访问次序时，算过这一项的格子写那一项，没算过的格子仍旧写地形成本。
        /// </summary>
        public string ElementText(int element, VisualSnapshot snapshot, SceneViewState viewState)
        {
            int cost = snapshot.Attr0[element];
            if (cost == 0) return "";

            switch (viewState.DisplayMode)
            {
                case 1:
                    if (snapshot.Attr1[element] >= 0) return Compact(snapshot.Attr1[element]);
                    break;
                case 2:
                    if (snapshot.Attr2[element] >= 0) return Compact(snapshot.Attr2[element]);
                    break;
                case 3:
                    if (snapshot.Attr3[element] >= 0) return Compact(snapshot.Attr3[element]);
                    break;
                case 4:
                    if (snapshot.Attr4[element] > 0)
                    {
                        int order = snapshot.Attr4[element];
                        return order < 1000 ? order.ToString(CultureInfo.InvariantCulture) : "1k+";
                    }
                    break;
            }
            return cost.ToString(CultureInfo.InvariantCulture);
        }

        /// <summary>
        /// 格上标注一律压到**三个字符以内**：格子 20 像素、字号 11 像素，
        /// 三个字符刚好放得下，四个就会被裁。
        ///
        /// 空间是从标注内容里省的，不是从字号里省的：
        /// 代价用千分之一为单位，这里四舍五入到整数（`123.4` 写成 `123`）；
        /// 到一千以上再省就失真了，写成 `1k+`，表示「一千以上」。
        /// </summary>
        private static string Compact(int milli)
        {
            int value = (milli + 500) / 1000;
            if (value >= 1000) return "1k+";
            return value.ToString(CultureInfo.InvariantCulture);
        }

        // ── 工具

        public void BeginGesture()
        {
            gestureSnapshot = map.Clone();
            gestureStartX = startX;
            gestureStartY = startY;
            gestureGoalX = goalX;
            gestureGoalY = goalY;
            gestureChanged = false;
        }

        public bool EndGesture()
        {
            if (gestureChanged && gestureSnapshot != null)
            {
                undo.Push(new SceneSnapshot(gestureSnapshot, gestureStartX, gestureStartY, gestureGoalX, gestureGoalY));
            }
            gestureSnapshot = null;
            return gestureChanged;
        }

        public bool CanUndo { get { return undo.CanUndo; } }

        public bool Undo()
        {
            SceneSnapshot snapshot = undo.Pop() as SceneSnapshot;
            if (snapshot == null) return false;
            map.CopyCostsFrom(snapshot.Map);
            startX = snapshot.StartX;
            startY = snapshot.StartY;
            goalX = snapshot.GoalX;
            goalY = snapshot.GoalY;
            return true;
        }

        private sealed class SceneSnapshot
        {
            public readonly Grid Map;
            public readonly int StartX, StartY, GoalX, GoalY;

            public SceneSnapshot(Grid map, int startX, int startY, int goalX, int goalY)
            {
                Map = map;
                StartX = startX;
                StartY = startY;
                GoalX = goalX;
                GoalY = goalY;
            }
        }

        public bool ApplyTool(int toolId, int element, int radius, int value, bool erase, bool first)
        {
            bool changed;
            switch (toolId)
            {
                case 1:                                     // 擦除
                    changed = PaintArea(element, radius, Const.MinCost);
                    break;
                case 2:                                     // 地形代价
                    changed = erase ? CycleArea(element, radius) : PaintArea(element, radius, value);
                    break;
                case 3:                                     // 起点
                    changed = PlaceEndpoint(element, true);
                    break;
                case 4:                                     // 终点
                    changed = PlaceEndpoint(element, false);
                    break;
                default:                                    // 墙壁；右键当擦除用
                    changed = PaintArea(element, radius, erase ? Const.MinCost : 0);
                    break;
            }
            if (changed) gestureChanged = true;
            return changed;
        }

        /// <summary>把刷子覆盖的格子刷成同一个值。值为 0 表示墙，墙不落在起点或终点上。</summary>
        private bool PaintArea(int center, int radius, int value)
        {
            bool changed = false;
            int cx = map.XOf(center), cy = map.YOf(center);
            int start = StartIndex, goal = GoalIndex;
            for (int dy = -radius; dy <= radius; dy++)
            {
                for (int dx = -radius; dx <= radius; dx++)
                {
                    int x = cx + dx, y = cy + dy;
                    if (!map.InBounds(x, y)) continue;
                    int index = map.Index(x, y);
                    if (value == 0 && (index == start || index == goal)) continue;
                    if (map.CostOf(index) == value) continue;
                    map.SetCostOf(index, value);
                    changed = true;
                }
            }
            return changed;
        }

        /// <summary>把刷子覆盖的格子的地形成本各自循环加一，墙回到 1。</summary>
        private bool CycleArea(int center, int radius)
        {
            bool changed = false;
            int cx = map.XOf(center), cy = map.YOf(center);
            for (int dy = -radius; dy <= radius; dy++)
            {
                for (int dx = -radius; dx <= radius; dx++)
                {
                    int x = cx + dx, y = cy + dy;
                    if (!map.InBounds(x, y)) continue;
                    int index = map.Index(x, y);
                    int value = map.CostOf(index);
                    value = value >= Const.MaxCost ? Const.MinCost : value + 1;
                    if (map.CostOf(index) == value) continue;
                    map.SetCostOf(index, value);
                    changed = true;
                }
            }
            return changed;
        }

        /// <summary>
        /// 放置起点或终点。落在墙上时把那一格清成平地（这是本程序选定的规则，
        /// README 里写明了），落在另一端点所在格则不动。
        /// </summary>
        private bool PlaceEndpoint(int element, bool isStart)
        {
            int other = isStart ? GoalIndex : StartIndex;
            if (element == other) return false;
            int current = isStart ? StartIndex : GoalIndex;
            if (element == current) return false;

            bool changed = false;
            if (map.IsWallIndex(element))
            {
                map.SetCostOf(element, Const.MinCost);
                changed = true;
            }
            int x = map.XOf(element), y = map.YOf(element);
            if (isStart)
            {
                if (startX != x || startY != y) { startX = x; startY = y; changed = true; }
            }
            else
            {
                if (goalX != x || goalY != y) { goalX = x; goalY = y; changed = true; }
            }
            return changed;
        }

        /// <summary>把地图与起终点写进快照，交给运算进程。</summary>
        public void ExportScene(VisualSnapshot snapshot)
        {
            int start = StartIndex, goal = GoalIndex;
            for (int i = 0; i < map.Count; i++)
            {
                snapshot.Attr0[i] = map.CostOf(i);
                snapshot.Marker[i] = i == start ? Const.MarkerStart
                                   : (i == goal ? Const.MarkerGoal : (byte)0);
            }
            snapshot.Seed = seed;
            snapshot.FocusIndex = start;
            snapshot.CustomA = goal;
        }
    }

    // ───────────────────────────────────────────────────────────── 演示定义

    /// <summary>搜索演示：下拉里的名字、引擎与场景的工厂、状态文案、自测。</summary>
    internal sealed class SearchDemoApp : IVisualDemo, IPathInspector, ICellMetrics
    {
        /// <summary>折线回放按这个格子边长算坐标，打出来的点与屏幕一致。</summary>
        public int CellPixels { get { return Const.CellSize; } }

        /// <summary>把路径折线的自检转给场景：--layoutcheck 用它断言不出现斜段。</summary>
        public int CountDiagonalSegments(VisualSnapshot snapshot, int cw, int ch, List<string> report)
        {
            return SearchScene.CountDiagonalSegments(snapshot, cw, ch, report);
        }

        /// <summary>量格上标注：八方向下数值最长，这里量的是最坏情况。</summary>
        public int CheckCellText(VisualSnapshot snapshot, TextWriter writer)
        {
            return new SearchScene().CheckCellText(snapshot, writer);
        }

        public string Name { get { return "搜索算法演示"; } }

        public string[] ModeNames
        {
            get
            {
                return new string[]
                {
                    "广度优先（BFS）", "深度优先（DFS）", "Dijkstra（一致代价）",
                    "A*（f = g + h）", "贪心最佳优先（只看 h）"
                };
            }
        }

        public string[] OptionNames
        {
            get
            {
                return new string[]
                {
                    "曼哈顿（齐次，可采纳）", "曼哈顿（非齐次，会高估）",
                    "欧氏（齐次，可采纳）", "欧氏（非齐次，会高估）",
                    "对角（齐次，可采纳）", "对角（非齐次，会高估）"
                };
            }
        }

        public string[] Option2Names
        {
            get
            {
                return new string[]
                {
                    "四方向（上、右、下、左）", "八方向（含斜走，斜步按 1.415 倍计价）"
                };
            }
        }

        public string[] DisplayNames
        {
            get { return new string[] { "地形成本", "g（已花代价）", "h（到终点估计）", "f = g + h", "访问次序" }; }
        }

        public string[] StatNames
        {
            get
            {
                return new string[]
                {
                    "已推进步数", "已展开节点数", "open 表大小",
                    "路径代价", "路径长度（步）", "实测推进速率", "两种模式对照"
                };
            }
        }

        public int DefaultMode { get { return 0; } }
        public int DefaultOption { get { return 0; } }
        public int DefaultOption2 { get { return 0; } }
        public string ReproduceCommand { get { return "SearchDemo.exe --selftest"; } }

        public IVisualEngine CreateEngine()
        {
            return new SearchEngine();
        }

        public IVisualScene CreateScene()
        {
            return new SearchScene();
        }

        public string DescribeState(VisualSnapshot snapshot)
        {
            if (snapshot.Finished != 0)
            {
                if (snapshot.HasResult != 0)
                {
                    return "已找到路径：代价 " + SearchAlgorithm.CostText(snapshot.PrimaryMetric)
                           + "，长度 " + snapshot.SecondaryMetric + " 步";
                }
                return "确认无路：open 表已空，终点不可达";
            }
            if (snapshot.Running != 0) return "运行中：运算进程已推进 " + snapshot.Steps + " 步";
            if (snapshot.Steps > 0) return "已暂停：已推进 " + snapshot.Steps + " 步";
            return "就绪：尚未开始";
        }

        // ── 无界面自测：全部在同进程里跑，不开窗口，也不起子进程

        public bool RunSelfTest(TextWriter w)
        {
            bool passed = true;
            int seed = Const.DefaultSeed;
            Grid map = MapGen.Generate(seed);
            int start = map.Index(Const.StartX, Const.StartY);
            int goal = map.Index(Const.GoalX, Const.GoalY);

            w.WriteLine("seed=" + seed + " size=" + map.Width + "x" + map.Height
                        + " start=(" + Const.StartX + "," + Const.StartY + ")"
                        + " goal=(" + Const.GoalX + "," + Const.GoalY + ")");

            SearchAlgorithm bfs = RunOne(w, "BFS", new BfsAlgorithm(map, false, start, goal));
            SearchAlgorithm dfs = RunOne(w, "DFS", new DfsAlgorithm(map, false, start, goal));
            SearchAlgorithm dijkstra = RunOne(w, "DIJKSTRA", new DijkstraAlgorithm(map, false, start, goal));
            SearchAlgorithm astarManhattan = RunOne(w, "ASTAR_MANHATTAN",
                new AStarAlgorithm(map, false, start, goal, HeuristicKind.Manhattan, true));
            SearchAlgorithm astarEuclidean = RunOne(w, "ASTAR_EUCLIDEAN",
                new AStarAlgorithm(map, false, start, goal, HeuristicKind.Euclidean, true));
            SearchAlgorithm astarOctile = RunOne(w, "ASTAR_OCTILE",
                new AStarAlgorithm(map, false, start, goal, HeuristicKind.Octile, true));
            RunOne(w, "GREEDY", new GreedyAlgorithm(map, false, start, goal, HeuristicKind.Manhattan, true));

            // ── 断言一：有权图上，可采纳启发函数的 A* 与 Dijkstra 的路径代价必须相同
            int dCost = dijkstra.PathCostMilli();
            bool same = astarManhattan.PathCostMilli() == dCost
                     && astarEuclidean.PathCostMilli() == dCost
                     && astarOctile.PathCostMilli() == dCost;
            passed &= SelfTestHost.Check(w, "dijkstra_vs_astar_cost_same", same,
                "dijkstra=" + SearchAlgorithm.CostText(dCost)
                + " astar_manhattan=" + SearchAlgorithm.CostText(astarManhattan.PathCostMilli())
                + " astar_euclidean=" + SearchAlgorithm.CostText(astarEuclidean.PathCostMilli())
                + " astar_octile=" + SearchAlgorithm.CostText(astarOctile.PathCostMilli()));

            // ── 断言二：无权图上，BFS 的步数应与 Dijkstra 的步数一致
            Grid unweighted = map.Clone();
            unweighted.SetAllTraversableCosts(1);
            SearchAlgorithm bfsPlain = new BfsAlgorithm(unweighted, false, start, goal);
            bfsPlain.RunToEnd();
            SearchAlgorithm dijkstraPlain = new DijkstraAlgorithm(unweighted, false, start, goal);
            dijkstraPlain.RunToEnd();
            bool bfsSame = bfsPlain.PathLength == dijkstraPlain.PathLength;
            passed &= SelfTestHost.Check(w, "bfs_vs_dijkstra_steps_unweighted", bfsSame,
                "bfs_steps=" + bfsPlain.PathLength + " dijkstra_steps=" + dijkstraPlain.PathLength);

            // ── 断言三：八方向下齐次曼哈顿仍然可采纳，代价应与 Dijkstra 相同
            SearchAlgorithm dijkstra8 = new DijkstraAlgorithm(map, true, start, goal);
            dijkstra8.RunToEnd();
            SearchAlgorithm astarManhattan8 = new AStarAlgorithm(map, true, start, goal,
                                                                 HeuristicKind.Manhattan, true);
            astarManhattan8.RunToEnd();
            bool admissible8 = astarManhattan8.PathCostMilli() == dijkstra8.PathCostMilli();
            passed &= SelfTestHost.Check(w, "astar_homogeneous_manhattan_8dir_admissible", admissible8,
                "dijkstra=" + SearchAlgorithm.CostText(dijkstra8.PathCostMilli())
                + " astar=" + SearchAlgorithm.CostText(astarManhattan8.PathCostMilli()));

            // ── 演示：非齐次曼哈顿会高估，A* 因此可能返回更贵的路径（不参与断言）
            DemoNonHomogeneous(w, map, start, goal, false, "4dir", dijkstra.PathCostMilli());
            DemoNonHomogeneous(w, map, start, goal, true, "8dir", dijkstra8.PathCostMilli());

            w.WriteLine("note greedy is not optimal, no assertion on it");
            w.Flush();
            return passed;
        }

        private static void DemoNonHomogeneous(TextWriter w, Grid map, int start, int goal,
                                               bool diagonal, string label, int dijkstraCost)
        {
            SearchAlgorithm astar = new AStarAlgorithm(map, diagonal, start, goal,
                                                       HeuristicKind.Manhattan, false);
            astar.RunToEnd();
            int cost = astar.PathCostMilli();
            w.WriteLine("demo  astar_nonhomogeneous_manhattan_" + label + ": dijkstra="
                        + SearchAlgorithm.CostText(dijkstraCost) + " astar=" + SearchAlgorithm.CostText(cost)
                        + " overestimate=" + (cost > dijkstraCost ? "yes" : "no"));
        }

        private static SearchAlgorithm RunOne(TextWriter w, string name, SearchAlgorithm algorithm)
        {
            algorithm.RunToEnd();
            w.WriteLine(name.PadRight(17)
                        + "path_len=" + algorithm.PathLength
                        + " path_cost=" + SearchAlgorithm.CostText(algorithm.PathCostMilli())
                        + " expanded=" + algorithm.Expanded
                        + " open_max=" + algorithm.OpenMax);
            return algorithm;
        }
    }

    // ───────────────────────────────────────────────────────────── 窗口：加几个搜索专用的控件

    /// <summary>
    /// 搜索演示的窗口。通用外壳在 VisualFormBase 里，这里只补搜索专用的三件事：
    /// 换地图、清空墙、随机撒墙，以及地图种子。
    /// </summary>
    internal sealed class SearchForm : VisualFormBase
    {
        private readonly SearchScene scene;
        private TextBox boxSeed;

        public SearchForm(SearchDemoApp demo)
            : base(demo)
        {
            scene = (SearchScene)Scene;
            boxSeed.Text = Seed.ToString(CultureInfo.InvariantCulture);
        }

        protected override int BuildExtraControls(Panel host, int y, Font font)
        {
            // 三行按钮，全部排在这一块里。撤销按钮由通用外壳放在「工具」分组里，
            // 这里不再重复放一个，免得出现两个一样的按钮。
            Button newMap = MakeButton("换一张地图", 10, y, 166, 30, font, ButtonNewMapClick);
            host.Controls.Add(newMap);
            Button clearWalls = MakeButton("清空墙", 184, y, 166, 30, font, ButtonClearWallsClick);
            host.Controls.Add(clearWalls);
            Button scatter = MakeButton("随机撒墙", 10, y + 34, 166, 30, font, ButtonScatterClick);
            host.Controls.Add(scatter);

            Label seedLabel = MakeLabel("种子", 178, y + 40, 40, font);
            host.Controls.Add(seedLabel);
            boxSeed = new TextBox();
            boxSeed.Location = new Point(222, y + 37);
            boxSeed.Size = new Size(64, 24);
            boxSeed.Font = font;
            boxSeed.ForeColor = ThemeManager.Palette.PanelText;
            boxSeed.BackColor = ThemeManager.Palette.Surface;
            host.Controls.Add(boxSeed);
            Button useSeed = MakeButton("生成", 290, y + 34, 60, 28, font, ButtonUseSeedClick);
            host.Controls.Add(useSeed);

            return y + 68;
        }

        private void ButtonNewMapClick(object sender, EventArgs e)
        {
            Seed = unchecked(Seed + 1);
            scene.Regenerate(Seed);
            boxSeed.Text = Seed.ToString(CultureInfo.InvariantCulture);
            AfterSceneEdited("已换地图：种子 " + Seed);
        }

        private void ButtonClearWallsClick(object sender, EventArgs e)
        {
            scene.ClearWalls();
            AfterSceneEdited("已清空墙");
        }

        private void ButtonScatterClick(object sender, EventArgs e)
        {
            scene.ScatterWalls();
            AfterSceneEdited("已随机撒墙（可能把终点围住）");
        }

        private void ButtonUseSeedClick(object sender, EventArgs e)
        {
            int value;
            if (!int.TryParse(boxSeed.Text.Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture, out value))
            {
                MessageBox.Show("种子必须是整数。", "搜索算法演示");
                return;
            }
            Seed = value;
            scene.Regenerate(value);
            AfterSceneEdited("已按种子 " + value + " 生成地图");
        }
    }

    // ───────────────────────────────────────────────────────────── 入口

    internal static class Program
    {
        [STAThread]
        private static int Main(string[] args)
        {
            SearchDemoApp demo = new SearchDemoApp();
            return VisualCoreMain.Run(args, demo, delegate { return new SearchForm(demo); });
        }
    }
}
