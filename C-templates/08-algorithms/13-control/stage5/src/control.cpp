/* control.cpp —— 练习模板 13 的实现（C++）—— 阶段 5
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
 * 本阶段有 14 处 TODO，都在这个文件里：
 *
 *     阶段 1-1   Plant::step          一阶惯性 + 饱和执行器
 *     阶段 1-2   on_off_command       带滞回的开关控制
 *     阶段 2-1   pid_step             PID 三项
 *     阶段 3-1   q_pid_step           同一个控制器的定点版
 *     阶段 3-2   pid_step_aw 的钳位分支
 *     阶段 3-3   pid_step_aw 的反算分支
 *     阶段 4-1   low_pass_step        一阶低通
 *     阶段 4-2   moving_average_step  滑动平均
 *     阶段 4-3   speed_m              M 法测速
 *     阶段 4-4   speed_t              T 法测速
 *     阶段 4-5   speed_mt             M/T 法测速
 *     阶段 5-1   feedforward_command  前馈项
 *     阶段 5-2   lookup_command       反查执行器的表
 *     阶段 5-3   plan_trapezoid       梯形速度规划
 *
 * 每个 TODO 上面写明「要做什么」、几个要想清楚的问题、要打印哪些量，
 * 以及填完之后应当看到的数——那些数都由 src/main_cli.cpp 打印，
 * 逐个对得上才算做完。自检用 app_cli.exe --selftest，它返回失败的项数。
 */
#include "control.hpp"

#include <cmath>

namespace ctl {

/* ==================================================================
 * 已给出：噪声源
 * ================================================================== */

double Noise::next_unit()
{
    /* 经典的 LCG 参数（Numerical Recipes）。取高 24 位映射到 [-1, 1)：
     * 每一步都是 2 的负整数次幂的倍数，因此定点与浮点上算出来一样。 */
    state = state * 1664525u + 1013904223u;
    const std::uint32_t v = (state >> 8) & 0xFFFFFFu;
    return static_cast<double>(v) / 8388608.0 - 1.0;
}

/* ==================================================================
 * 阶段 1：被控对象
 * ================================================================== */

double clamp_command(double u, double u_max)
{
    if (u > u_max) {
        return u_max;
    }
    if (u < -u_max) {
        return -u_max;
    }
    return u;
}

double Plant::measure()
{
    return y + cfg.noise_amp * noise.next_unit();
}

double Plant::step(double u)
{
    /* TODO（阶段 1-1）：推进一个采样周期。
     * 被控对象是一阶惯性：给它一个不变的输入，输出不会立刻跳到稳态值，
     * 而是朝着那个稳态值一步一步走过去，每一拍走完剩余距离的一部分。
     * 四件事要想清楚：
     *   1. 稳态值由输入与哪一个参数决定？输入为 1 个单位时的稳态输出
     *      就是那个参数本身。
     *   2. 「每一拍走完剩余距离的一部分」里的那一部分，由哪两个量决定？
     *      它必须是无量纲的，而且不能大于 1，否则会走过头。
     *   3. 命令在进入对象之前要先经过执行器。执行器不理想：它有上下边界，
     *      越过去的部分削掉。这一刀切在哪里——切在对象里，还是切在
     *      调用者那里？两种做法在饱和时给出的结果一样吗？
     *   4. 返回的是真值，不是测量值。噪声只出现在 measure() 里，
     *      对象自己不该看见它——控制器只准看测量值，对象不该作弊。
     * 记数契约：本阶段打印 settled output（末段平均真值）与 error 两列，
     *   三行对照用的命令都是 u = r / gain，只是对象真正的增益不同。
     * 判据（《配置步骤.md》阶段 1）：真值增益 2.0 / 2.5 / 1.25 三行，
     *   settled output 分别是 1.0000 / 1.2500 / 0.6250，
     *   四位小数逐位对上，不是「差不多」；
     *   把命令给到 5.0 时，落到对象上的只有 2.0，稳态仍然是 4.0000。 */
    (void)u;
    return y;                 /* 占位实现：不推进，输出停在初值上 */
}

double run_open_loop(const PlantConfig &cfg, double u, int steps)
{
    Plant p(cfg);
    const int tail = (steps > 10) ? steps / 10 : 1;

    double sum = 0.0;
    for (int k = 0; k < steps; ++k) {
        const double y = p.step(u);
        if (k >= steps - tail) {
            sum += y;
        }
    }
    return sum / static_cast<double>(tail);
}

/* ==================================================================
 * 阶段 1：带滞回的开关控制
 * ================================================================== */

double on_off_command(double y_measured, double setpoint, double hysteresis,
                      bool was_on, double u_max)
{
    /* TODO（阶段 1-2）：带滞回的开关控制。
     * 命令只有两个取值：开着给 u_max，关着给 0。上一拍是开还是关，
     * 由参数 was_on 告诉你。要决定的是这一拍开还是关。
     * 四件事要想清楚：
     *   1. 滞回的意思是「开」与「关」的阈值不重合。不重合的写法是
     *      让阈值随当前状态变：开着的时候，测量值高到什么程度才关？
     *      关着的时候，测量值低到什么程度才开？这两个阈值到目标点的
     *      距离分别是多少？hysteresis 为 0 时它们重合成一条线，
     *      会发生什么？
     *   2. 比较用的是测量值，不是真值——这个函数只拿得到测量值。
     *   3. 测量噪声的幅度是 noise_amp。hysteresis 比它小的时候，
     *      噪声自己就能把测量值推过阈值；比它大的时候推不过去。
     *      切换次数与滞回宽度因此是什么关系？
     *   4. 死区宽了，真值离目标就更远。这两个代价是同一个旋钮的两端，
     *      验收表把它们并排印出来。
     * 记数契约：本阶段打印四行，每行是 hysteresis 取 0.0000、0.0500、
     *   0.2000、0.5000 时的 switches（命令变化的次数）、on-steps、
     *   duty、tail ripple（后半段 max |真值 - 目标|）与 final error。
     * 判据（《配置步骤.md》阶段 1）：无滞回那一行的 switches 最大，
     *   滞回越宽切换越少（0.0000 / 0.0500 / 0.2000 / 0.5000 四行一路降下来）；
     *   tail ripple 反过来一路升上去，有滞回的行大致等于滞回宽度本身
     *   （0.5000 那行在 0.5 附近），无滞回那行不到 0.15；
     *   duty 在四行里都落在 0.15 到 0.35 之间——稳态下执行器的平均出力
     *   必须正好抵消对象的衰减，这个占空比是算得出来的。 */
    (void)y_measured;
    (void)setpoint;
    (void)hysteresis;
    (void)was_on;
    return u_max;             /* 占位实现：一直开着，一次也不切换 */
}

OnOffResult run_on_off(const PlantConfig &cfg, double setpoint,
                       double hysteresis, int steps)
{
    Plant p(cfg);
    OnOffResult r;

    bool was_on = false;
    double prev = -1.0;        /* 上一拍的命令，负数表示还没有 */
    std::vector<double> tail;

    for (int k = 0; k < steps; ++k) {
        const double u = on_off_command(p.measure(), setpoint, hysteresis, was_on, cfg.u_max);

        if (u > 0.0) {
            ++r.on_steps;
            was_on = true;
        } else {
            was_on = false;
        }
        if (prev >= 0.0 && u != prev) {
            ++r.switches;
        }
        prev = u;

        const double y = p.step(u);
        if (k >= steps / 2) {
            tail.push_back(y);
        }
    }

    r.duty = static_cast<double>(r.on_steps) / static_cast<double>(steps);

    double worst = 0.0;
    for (std::size_t i = 0; i < tail.size(); ++i) {
        const double d = tail[i] - setpoint;
        const double ad = (d < 0.0) ? -d : d;
        if (ad > worst) {
            worst = ad;
        }
    }
    r.tail_ripple = worst;

    const std::size_t last = (tail.size() > 100) ? 100 : tail.size();
    double sum = 0.0;
    for (std::size_t i = tail.size() - last; i < tail.size(); ++i) {
        sum += tail[i];
    }
    r.final_error = ((last > 0) ? (sum / static_cast<double>(last)) : 0.0) - setpoint;

    return r;
}

/* ==================================================================
 * 阶段 2：PID 三项
 * ================================================================== */

double pid_step(const PidGains &g, PidState &s, double setpoint,
                double y_measured, double dt)
{
    /* TODO（阶段 2-1）：PID 一拍。
     * 输入是这一拍的目标、这一拍的测量值与采样周期，输出是这一拍的命令。
     * 状态 s 是两次调用之间留下来的东西，随你怎么用。
     * 五个问题要想清楚，每一个都会改变验收表里的数：
     *   1. 误差怎么定义？用测量值还是真值？控制器只拿得到其中一个。
     *   2. 三项分别是「误差的什么」。比例项与误差本身成正比；积分项
     *      与误差随时间的累加成比例（采样周期在这里起什么作用）；
     *      微分项与误差变化的快慢成比例。
     *   3. 微分项对哪一个量求变化率——误差，还是测量值？两者在
     *      目标值不动时完全一样，目标值一跳就不一样了：对误差求
     *      变化率的话，第一拍会冒出一个与跳变量成正比的尖峰，
     *      执行器会照着这个尖峰动作一下。验收表里 PI 与 PID 的
     *      超调就是这一条与积分共同作用出来的。
     *   4. 第一拍没有「上一拍」，微分项怎么办？让它等于 0，
     *      还是让它等于某个初值？状态里那个 has_prev 就是为这件事准备的。
     *   5. 三项相加之后要不要在这里夹住？这一层先不夹：命令照原样
     *      交给对象，对象的执行器自己会削（阶段 1 已经写好了那一刀）。
     *      把它留着不夹，阶段 4 才看得见积分饱和是怎么来的。
     * 记数契约：本阶段打印两张表，行是 P、PI、PD、PID 四组增益。
     *   第一张（测量噪声关掉）列出 final error、overshoot %、
     *   settle time、max |u|；第二张（噪声打开）列出 final error、
     *   y ripple、u ripple、max |u|。settle time 是 -1.0000 表示
     *   到结束都没进过正负 2% 带。
     * 判据（《配置步骤.md》阶段 2）：四组增益的差别必须是结构性的——
     *   P 与 PD 的 final error 不为 0（约 0.11）而且 settle time 是 -1；
     *   PI 与 PID 的 final error 是 0.0000 且 settle time 是个正数；
     *   overshoot 只在带积分的那两组上非零；
     *   噪声打开那张表里 u ripple 从 P 到 PID 一路变大，
     *   带微分项的两组最大——微分把测量噪声放大成了命令的抖动。 */
    (void)g;
    (void)s;
    (void)setpoint;
    (void)y_measured;
    (void)dt;
    return 0.0;               /* 占位实现：命令恒为 0，对象自己衰减到 0 */
}

LoopMetrics run_loop(const PlantConfig &cfg, const PidGains &g,
                     double setpoint, int steps)
{
    Plant p(cfg);
    PidState s;

    std::vector<double> cmd;
    std::vector<double> out;
    cmd.reserve(static_cast<std::size_t>(steps));
    out.reserve(static_cast<std::size_t>(steps));

    for (int k = 0; k < steps; ++k) {
        const double u = pid_step(g, s, setpoint, p.measure(), cfg.dt);
        cmd.push_back(u);
        out.push_back(p.step(u));
    }
    return summarize(cmd, out, setpoint, cfg.dt, cfg.u_max);
}

LoopMetrics summarize(const std::vector<double> &cmd, const std::vector<double> &out,
                      double setpoint, double dt, double u_max)
{
    LoopMetrics m;
    const std::size_t n = out.size();
    if (n == 0) {
        return m;
    }

    /* 末段平均：稳态误差看的是这一段，不是最后一拍 */
    const std::size_t last = (n > 100) ? 100 : n;
    double sum = 0.0;
    for (std::size_t i = n - last; i < n; ++i) {
        sum += out[i];
    }
    m.final_error = sum / static_cast<double>(last) - setpoint;

    /* 超调：全程最大真值高出目标多少 */
    double top = out[0];
    for (std::size_t i = 1; i < n; ++i) {
        if (out[i] > top) {
            top = out[i];
        }
    }
    m.overshoot = (top > setpoint) ? (top - setpoint) / setpoint * 100.0 : 0.0;

    /* 调节时间：最后一次离开正负 2% 带的下一拍。一个样本都没出去过就是 0，
     * 到结束还在带外就是 -1。 */
    const double band = 0.02 * setpoint;
    std::size_t last_bad = 0;
    bool any_bad = false;
    for (std::size_t i = 0; i < n; ++i) {
        const double d = out[i] - setpoint;
        if ((d > band) || (d < -band)) {
            last_bad = i + 1;
            any_bad = true;
        }
    }
    if (!any_bad) {
        m.settle_time = 0.0;
    } else if (last_bad >= n) {
        m.settle_time = -1.0;
    } else {
        m.settle_time = static_cast<double>(last_bad) * dt;
    }

    /* 抖动：后半段相邻两拍之差的平均值，命令与真值各一份 */
    const std::size_t half = n / 2;
    double su = 0.0;
    double sy = 0.0;
    long long cnt = 0;
    for (std::size_t i = half + 1; i < n; ++i) {
        su += std::fabs(cmd[i] - cmd[i - 1]);
        sy += std::fabs(out[i] - out[i - 1]);
        ++cnt;
    }
    if (cnt > 0) {
        m.u_ripple = su / static_cast<double>(cnt);
        m.y_ripple = sy / static_cast<double>(cnt);
    }

    /* 命令的峰值与顶到边界的拍数 */
    for (std::size_t i = 0; i < n; ++i) {
        const double au = std::fabs(cmd[i]);
        if (au > m.u_peak) {
            m.u_peak = au;
        }
        if (au >= u_max) {
            ++m.sat_steps;
        }
    }

    return m;
}

/* ==================================================================
 * 阶段 3：离散化与定点
 * ================================================================== */

q_t q_sat(std::int64_t v)
{
    if (v > 2147483647LL) {
        return 2147483647;
    }
    if (v < -2147483648LL) {
        return -2147483647 - 1;
    }
    return static_cast<q_t>(v);
}

q_t q_from_double(double v)
{
    const double scaled = v * 65536.0;
    if (scaled >= 2147483647.0) {
        return 2147483647;
    }
    if (scaled <= -2147483648.0) {
        return -2147483647 - 1;
    }
    /* 四舍五入到最近的一格，负数往绝对值大的方向舍 */
    return static_cast<q_t>(scaled + ((scaled >= 0.0) ? 0.5 : -0.5));
}

double q_to_double(q_t v)
{
    return static_cast<double>(v) / 65536.0;
}

q_t q_mul(q_t a, q_t b)
{
    /* 两个 Q16.16 相乘得到 Q32.32，先放进 64 位再移回 16 位。
     * 直接写成 int32 相乘会在几千这个量级上就溢出。 */
    const std::int64_t wide = static_cast<std::int64_t>(a) * static_cast<std::int64_t>(b);
    return q_sat(wide >> kQFrac);
}

q_t q_div(q_t a, q_t b)
{
    if (b == 0) {
        return 0;
    }
    const std::int64_t wide = static_cast<std::int64_t>(a) << kQFrac;
    return q_sat(wide / static_cast<std::int64_t>(b));
}

q_t q_clamp(q_t v, q_t lo, q_t hi)
{
    if (v > hi) {
        return hi;
    }
    if (v < lo) {
        return lo;
    }
    return v;
}

q_t q_pid_step(const PidGainsQ &g, PidStateQ &s, q_t setpoint_q, q_t y_measured_q,
               q_t dt_q)
{
    /* TODO（阶段 3-1）：同一个控制器，换成定点再写一遍。
     * 输入输出都是 Q16.16，算法与阶段 2 的 pid_step 完全一样；
     * 变的是每一个中间结果放在什么格式里。五件事要想清楚：
     *   1. 误差是「目标减测量值」，两个 Q16.16 相减还是 Q16.16，
     *      但差值可能越过 int32 的范围，减法要不要先放宽再用 q_sat 夹回来？
     *   2. 比例项：Q16.16 的增益乘 Q16.16 的误差，结果是 Q32.32。
     *      q_mul 已经替你把它移回 Q16.16 了。积分项要多乘一个采样周期
     *      dt_q（也是 Q16.16），因此那一项要乘两次——先乘什么、后乘什么，
     *      结果都一样，但中间那一步的格式要想清楚。
     *   3. 积分累加器是 Q16.16 的 int32，它会一直加下去。累加之前先看
     *      加上去会不会越过 kQIntegLimit 与 -kQIntegLimit：越过了贴边，
     *      并且把 s.integ_clamped 加一。贴边与回绕是两件不同的事，
     *      回绕会让控制器突然往反方向猛推。
     *   4. 微分项要除以采样周期，用 q_div。第一拍没有上一拍的值，
     *      这一项给 0——理由与阶段 2 那条一样。
     *   5. 这一层与阶段 2 一样不夹输出：命令照原样交出去，对象的执行器
     *      自己会削。三项相加可能越过 int32，加完用 q_sat 收一次。
     * 记数契约：本阶段打印一张两列的表，左列是浮点版、右列是定点版，
     *   行是 final error、overshoot %、settle time、max |u|，
     *   再加上噪声打开时的 u ripple 与 y ripple；另有
     *   integral clamps 一行（定点版的累加器贴边次数，正常应当是 0）。
     * 判据（《配置步骤.md》阶段 3）：两列的数在四位小数上相同，
     *   最多差最后一两位（差的来源只有量化，没有别的）；
     *   定点版若与浮点版差出很多，多半是某一项忘了乘 dt_q、
     *   或者中间结果没有放宽就移了位。 */
    (void)g;
    (void)s;
    (void)setpoint_q;
    (void)y_measured_q;
    (void)dt_q;
    return 0;                 /* 占位实现：命令恒为 0，对象自己衰减到 0 */
}

LoopMetrics run_loop_fixed(const PlantConfig &cfg, const PidGains &g,
                           double setpoint, int steps)
{
    Plant p(cfg);
    PidStateQ s;

    PidGainsQ gq;
    gq.kp = q_from_double(g.kp);
    gq.ki = q_from_double(g.ki);
    gq.kd = q_from_double(g.kd);

    const q_t rq = q_from_double(setpoint);
    const q_t dtq = q_from_double(cfg.dt);

    std::vector<double> cmd;
    std::vector<double> out;
    cmd.reserve(static_cast<std::size_t>(steps));
    out.reserve(static_cast<std::size_t>(steps));

    for (int k = 0; k < steps; ++k) {
        const q_t uq = q_pid_step(gq, s, rq, q_from_double(p.measure()), dtq);
        const double u = q_to_double(uq);
        cmd.push_back(u);
        out.push_back(p.step(u));
    }

    LoopMetrics m = summarize(cmd, out, setpoint, cfg.dt, cfg.u_max);
    m.integ_clamps = s.integ_clamped;
    return m;
}

/* ==================================================================
 * 阶段 3：限幅与积分饱和
 * ================================================================== */

double pid_step_aw(const PidGains &g, PidState &s, double setpoint, double y_measured,
                   double dt, double u_max, AntiWindup mode, double tt)
{
    const double e = setpoint - y_measured;

    /* 已给出：三项照阶段 2 的写法算出来，只是这一版自己把输出夹住，
     * 因此它知道「夹之前」与「夹之后」差了多少。 */
    const double p = g.kp * e;

    double d = 0.0;
    if (s.has_prev) {
        d = -g.kd * (y_measured - s.prev_meas) / dt;
    }
    s.prev_meas = y_measured;
    s.has_prev = true;

    const double step = g.ki * e * dt;      /* 这一拍积分器本来要加的量 */
    const double raw = p + s.integ + step + d;
    const double u = clamp_command(raw, u_max);

    if (mode == AntiWindup::kClamp) {
        /* TODO（阶段 3-2）：钳位法（条件积分）。
         * 输出已经顶到边界时，积分器还要不要继续累加？
         * 三件事要想清楚：
         *   1. 顶到边界有两个方向：raw 高过 u_max，或者低过 -u_max。
         *      两种情形下，「误差」的符号与「顶出去的方向」各是什么关系？
         *      哪几种组合会让积分器越积越远、离回来需要的值越来越远？
         *   2. 剩下那几种组合（顶出去了，但误差已经在往回拉）要不要
         *      停？停了会发生什么——积分器还回不回得来？
         *   3. 没顶到边界时照常累加；这一条与 kLimitOnly 的行为必须
         *      完全一样，差别只在顶住的那几拍。
         * 记数契约：本阶段打印一张三行的表，行是 limit only、clamp、
         *   back calculation；列是 overshoot %、settle time、
         *   max |integral|、saturated、final error。
         * 判据（《配置步骤.md》阶段 3）：钳位那一行的 max |integral|
         *   比 limit only 那一行小几倍（1.5000 对 6.5107），
         *   overshoot % 从 22.7225 掉到 0.0000；
         *   三行的 final error 都接近 0。 */
        s.integ += step;      /* 占位：钳位分支还没写，先照常累加 */
    } else if (mode == AntiWindup::kBackCalc) {
        /* TODO（阶段 3-3）：反算（back calculation）。
         * 钳位法靠「停下来」避免积分器跑远，这一种是让它自己走回来：
         * 拿夹住前后的差（u 减 raw）算一个修正量，加到积分器的累加里。
         * 四件事要想清楚：
         *   1. 差值是正是负，积分器应当往哪一边被拉？把两种方向都代进去，
         *      看修正量能不能把积分器拉回「刚好不越界」的位置。
         *   2. 参数 tt 是一个时间常数（单位是秒）。它与采样周期 dt 一起
         *      出现在修正量里：tt 越大，这一条拉得越快还是越慢？
         *      极限情况下 tt 取很小，会退化成哪一种做法？
         *   3. 修正量里 dt 与 tt 的位置：两者相除是无量纲的，
         *      乘在差值上得到的量纲与积分器（命令的单位）一致。
         *      位置放反了，量纲就不对了。
         *   4. 没顶到边界时差值是 0，这一条自动失效，不必单独判断。
         * 记数契约：同钳位那一条，看 back calculation 那一行。
         * 判据（《配置步骤.md》阶段 3）：反算那一行的 overshoot % 与钳位
         *   那一行一样是 0.0000，饱和拍数比 limit only 少得多
         *   （15 对 124）；它的 max |integral| 峰值并不比 limit only 小
         *   （5.9165 对 6.5107），差别在轨迹上：反算在输出离开饱和之后
         *   立刻把积分拉回来，因此恢复得快。扰动恢复那一张表里，
         *   两种抗饱和的恢复时间是 0.3000 与 1.1500，
         *   而 limit only 那一行是 2.2400。 */
        s.integ += step;      /* 占位：反算分支还没写，先照常累加 */
        (void)tt;
    } else {
        s.integ += step;      /* 对照组：只夹输出，积分照常累加 */
    }

    return u;
}

AwResult run_aw(const PlantConfig &cfg, const PidGains &g, double setpoint, int steps,
                AntiWindup mode, double tt)
{
    Plant p(cfg);
    PidState s;

    std::vector<double> cmd;
    std::vector<double> out;
    cmd.reserve(static_cast<std::size_t>(steps));
    out.reserve(static_cast<std::size_t>(steps));

    double max_integ = 0.0;
    for (int k = 0; k < steps; ++k) {
        const double u = pid_step_aw(g, s, setpoint, p.measure(), cfg.dt, cfg.u_max, mode, tt);
        cmd.push_back(u);
        out.push_back(p.step(u));

        const double ai = std::fabs(s.integ);
        if (ai > max_integ) {
            max_integ = ai;
        }
    }

    const LoopMetrics m = summarize(cmd, out, setpoint, cfg.dt, cfg.u_max);

    AwResult r;
    r.overshoot = m.overshoot;
    r.settle_time = m.settle_time;
    r.max_integ = max_integ;
    r.sat_steps = m.sat_steps;
    r.final_error = m.final_error;
    return r;
}

LoadResult run_load_recovery(const PlantConfig &cfg, const PidGains &g, double setpoint,
                             int steps, AntiWindup mode, double tt,
                             int load_from, int load_until, double load)
{
    Plant p(cfg);
    PidState s;

    std::vector<double> out(static_cast<std::size_t>(steps), 0.0);
    for (int k = 0; k < steps; ++k) {
        const double u = pid_step_aw(g, s, setpoint, p.measure(), cfg.dt, cfg.u_max, mode, tt);
        const bool loaded = (k >= load_from) && (k < load_until);
        out[static_cast<std::size_t>(k)] = p.step(u + (loaded ? load : 0.0));
    }

    LoadResult r;

    const std::size_t last = (out.size() > 100) ? 100 : out.size();
    double sum = 0.0;
    for (std::size_t i = out.size() - last; i < out.size(); ++i) {
        sum += out[i];
    }
    r.final_error = ((last > 0) ? (sum / static_cast<double>(last)) : 0.0) - setpoint;

    /* 恢复时间从扰动撤掉那一刻算起：之后最后一次离开正负 2% 带的下一拍 */
    const double band = 0.02 * setpoint;
    std::size_t last_bad = 0;
    bool any_bad = false;
    for (std::size_t i = static_cast<std::size_t>(load_until); i < out.size(); ++i) {
        const double d = out[i] - setpoint;
        if ((d > band) || (d < -band)) {
            last_bad = i + 1;
            any_bad = true;
        }
    }
    if (!any_bad) {
        r.recovery_time = 0.0;
    } else if (last_bad >= static_cast<std::size_t>(steps)) {
        r.recovery_time = -1.0;
    } else {
        r.recovery_time = static_cast<double>(last_bad - static_cast<std::size_t>(load_until)) * cfg.dt;
    }
    return r;
}

/* ==================================================================
 * 阶段 4：滤波
 * ================================================================== */

double low_pass_step(LowPass &f, double x, double dt)
{
    /* TODO（阶段 4-1）：一阶低通一步。
     * 它做的事情是「输出朝着输入走，但每一步只走完剩余距离的一部分」——
     * 与阶段 1 那个一阶惯性是同一个形状，只是这里滤波器的输入是
     * 测量值，输出是给控制器看的测量值。
     * 四件事要想清楚：
     *   1. 「每一步走完剩余距离的一部分」里的那一部分由哪两个量决定？
     *      它是无量纲的，而且必须小于 1，否则会振荡。
     *   2. tf 是这个滤波器的时间常数，单位是秒。tf 越大，输出越平滑，
     *      也越跟不上输入——噪声与滞后是同一个旋钮的两端。
     *   3. 第一拍没有上一次的输出，f.y 从 0 起步会让输出从 0 慢慢爬上来。
     *      状态里那个 started 是为这件事准备的：第一拍直接取输入，
     *      还是照常从 0 爬？两种做法在「阶跃时刻的输出」上差很多，
     *      验收表里 t50 那一列看得出来。
     *   4. 时间常数 tf 与采样周期 dt 的比值决定一切；dt 大于 tf 时
     *      那个比例会超过 1，输出就会来回振。
     * 记数契约：本阶段打印一张五行的表，行是 none、低通 tf=0.0100、
     *   低通 tf=0.0500、平均 N=16、平均 N=128；列是 noise rms、
     *   t50 与 lag（t50 减去阶跃时刻 0.5 s）。
     * 判据（《配置步骤.md》阶段 4）：低通两行的 noise rms 随 tf 变小，
     *   lag 随 tf 变大；tf=0.0100 那一行与平均 N=16 那一行的两个数
     *   都落在同一个量级上（噪声约 0.007，滞后约 0.007 s）；
     *   none 那一行的 noise rms 约 0.029，lag 是 0.0000。 */
    (void)f;
    (void)x;
    (void)dt;
    return x;                 /* 占位实现：原样放行，一点也没滤 */
}

double moving_average_step(MovingAverage &f, double x)
{
    /* TODO（阶段 4-2）：滑动平均一步。
     * 窗口里放着最近 n 个输入，输出是它们的平均。窗口是环形的：
     * 写满之后每来一个新样本，就要把最老的那个挤出去。
     * 四件事要想清楚：
     *   1. 每来一个样本，谁进来、谁出去？出去的那个值还留在缓冲里，
     *      因此求和时要把它的贡献减掉。
     *   2. 维护一个「缓冲里那些数之和」比每次重新遍历一遍便宜得多；
     *      这个和在状态里（f.sum），进位与退位时各改一次。
     *   3. 缓冲还没写满时，分母是已经写进去的个数（f.count），
     *      不是窗口长度。拿窗口长度去除会让输出从偏低的地方慢慢爬上来。
     *   4. 写位置要绕回开头（取模），因此它是一个环形缓冲，不是队列。
     * 记数契约：同上面那一张表，看平均 N=16 与平均 N=128 两行。
     * 判据（《配置步骤.md》阶段 4）：两行的 noise rms 约等于不滤波时的
     *   0.029 除以 sqrt(N)（也就是 0.0072 与 0.0026），
     *   lag 约等于 (N-1)/2 个采样周期（也就是 0.0075 s 与 0.0635 s）。 */
    if (f.window.empty()) {
        return x;             /* 占位实现：窗口是空的，原样放行 */
    }
    return x;                 /* 占位实现：原样放行，一点也没滤 */
}

FilterMetrics run_filter(FilterKind kind, double param, double dt, int steps, double step_at)
{
    Noise nz(kSeed);

    LowPass lp;
    lp.tf = param;

    MovingAverage ma;
    const std::size_t n = (param >= 1.0) ? static_cast<std::size_t>(param) : 1;
    ma.window.assign(n, 0.0);

    std::vector<double> out(static_cast<std::size_t>(steps), 0.0);
    std::vector<double> truth(static_cast<std::size_t>(steps), 0.0);

    for (int k = 0; k < steps; ++k) {
        const double t = static_cast<double>(k) * dt;
        const double s = (t + 1e-9 >= step_at) ? 1.0 : 0.0;
        const double x = s + 0.05 * nz.next_unit();

        truth[static_cast<std::size_t>(k)] = s;
        if (kind == FilterKind::kNone) {
            out[static_cast<std::size_t>(k)] = x;
        } else if (kind == FilterKind::kLowPass) {
            out[static_cast<std::size_t>(k)] = low_pass_step(lp, x, dt);
        } else {
            out[static_cast<std::size_t>(k)] = moving_average_step(ma, x);
        }
    }

    FilterMetrics m;

    /* 第一次到 0.5 的时刻；一直没到就是跑完的那一刻 */
    m.t50 = static_cast<double>(steps) * dt;
    for (int k = 0; k < steps; ++k) {
        if (out[static_cast<std::size_t>(k)] >= 0.5) {
            m.t50 = static_cast<double>(k) * dt;
            break;
        }
    }
    m.lag = m.t50 - step_at;

    /* 从阶跃之后半秒起量到结束：这一段里滤波器早就跟上了，
     * 剩下的差别全是噪声压掉了多少。样本取长一点，
     * 免得几百个样本的抽样涨落把两行的差别盖住。 */
    const int from = static_cast<int>((step_at + 0.5) / dt + 0.5);
    const int start = (from < steps) ? from : 0;
    double acc = 0.0;
    int cnt = 0;
    for (int k = start; k < steps; ++k) {
        const double d = out[static_cast<std::size_t>(k)] - truth[static_cast<std::size_t>(k)];
        acc += d * d;
        ++cnt;
    }
    m.noise_rms = (cnt > 0) ? std::sqrt(acc / static_cast<double>(cnt)) : 0.0;

    return m;
}

/* ==================================================================
 * 阶段 4：测速
 * ================================================================== */

long long Encoder::count_before(double t) const
{
    /* 第 i 个脉冲在 (i + 0.5) / (omega * cpr)：把它早于 t 的条件
     * 整理一下就是 i < omega * cpr * t - 0.5，满足的非负整数个数
     * 正是那个数的上取整。 */
    const double n = omega * cpr * t - 0.5;
    if (n <= 0.0) {
        return 0;
    }
    return static_cast<long long>(std::ceil(n));
}

double Encoder::pulse_time(long long i) const
{
    return (static_cast<double>(i) + 0.5) / (omega * cpr);
}

double speed_m(const Encoder &e, double t0, double ts)
{
    /* TODO（阶段 4-3）：M 法。
     * 在 [t0, t0 + ts) 这个固定窗口里数编码器的脉冲个数，再折算成转速。
     * 三件事要想清楚：
     *   1. 窗口里有多少个脉冲？Encoder::count_before 给的是「严格早于 t」
     *      的累计个数，窗口里的个数就是两个累计值之差。
     *   2. 一个计数对应转角的多少？cpr 是每转的计数个数。转速的单位是
     *      转每秒，因此个数、窗口长度与 cpr 三者要放在一起。
     *   3. 这个方法的分辨率就是一个脉冲。窗口里只有二十来个脉冲时，
     *      差一个就是百分之几——低速为什么不准，原因就在这一句上。
     * 记数契约：本阶段打印一张六行的表，行是两种转速（0.5000 与
     *   50.0000 转每秒）乘三种方法；列是 mean estimate、mean error %
     *   与 max error %，误差一律是相对值。
     * 判据（《配置步骤.md》阶段 4）：M 法在 0.5000 那一行的误差是
     *   百分之几，在 50.0000 那一行接近 0；
     *   每行都要与真实转速对得上，恒返回 0 会让误差停在 100%。 */
    (void)e;
    (void)t0;
    (void)ts;
    return 0.0;               /* 占位实现：转速恒为 0 */
}

double speed_t(const Encoder &e, double t0, double f_clk)
{
    /* TODO（阶段 4-4）：T 法。
     * 量两个相邻脉冲之间有多少个时钟周期，用时钟频率去除。
     * 四件事要想清楚：
     *   1. 窗口起点的第一个脉冲下标是几？count_before(t0) 给的正是它，
     *      下一个脉冲的下标再加一；两者的时刻相减就是脉冲间隔。
     *   2. 时钟周期数只能数整数个：把「间隔乘以时钟频率」向下取整。
     *      硬件数出来的就是整周期，四舍五入会凭空多算半拍。
     *   3. 一个脉冲间隔对应转过 1 / cpr 圈，因此转速 = 频率除以
     *      「周期数乘 cpr」。
     *   4. 高速时一个脉冲间隔里只有几十个时钟周期，差一个就是百分之
     *      几——高速为什么不准，原因在这一句上。
     * 记数契约：同上面那一张表，看 T 法那两行。
     * 判据（《配置步骤.md》阶段 4）：T 法在 0.5000 那一行的误差
     *   不到 0.1%，在 50.0000 那一行是百分之几。 */
    (void)e;
    (void)t0;
    (void)f_clk;
    return 0.0;               /* 占位实现：转速恒为 0 */
}

double speed_mt(const Encoder &e, double t0, double ts, double f_clk)
{
    /* TODO（阶段 4-5）：M/T 法。
     * 先从窗口里的第一个脉冲数起，数到窗口结束为止有 m1 个脉冲；
     * 再量这 m1 个脉冲间隔一共花了多少个时钟周期 m2，两个数一起用。
     * 四件事要想清楚：
     *   1. 这一段的起点是窗口里第一个脉冲的时刻，终点不是窗口结束，
     *      而是「再往后数 m1 个脉冲」的那个时刻——这样段长正好是 m1 个
     *      脉冲间隔，m1 与 m2 才对应同一段时长。终点若取窗口结束，
     *      段长里就多出半截，误差回到 M 法那个量级。
     *   2. m1 是窗口里的脉冲个数，用两次 count_before 相减。
     *   3. m2 是段长乘时钟频率向下取整。
     *   4. 转速 = m1 乘频率除以「m2 乘 cpr」。m1 与 m2 都大的时候，
     *      两边各差一个带来的误差都很小——两头都准的原因在这里。
     * 记数契约：同上面那一张表，看 M/T 法那两行。
     * 判据（《配置步骤.md》阶段 4）：M/T 法两行的误差都不到 0.01%，
     *   明显小于 M 法在低速、T 法在高速的误差。 */
    (void)e;
    (void)t0;
    (void)ts;
    (void)f_clk;
    return 0.0;               /* 占位实现：转速恒为 0 */
}

SpeedMetrics run_speed(SpeedMethod method, double omega, double cpr,
                       double ts, double f_clk, int windows)
{
    Encoder e;
    e.cpr = cpr;
    e.omega = omega;

    SpeedMetrics m;
    m.true_speed = omega;

    const int count = (windows > 0) ? windows : 1;
    double sum_est = 0.0;
    double sum_err = 0.0;
    double worst = 0.0;
    double t0 = 1.0;          /* 从第 1 s 起步，避开启动那一段 */
    for (int w = 0; w < count; ++w) {
        double est = 0.0;
        if (method == SpeedMethod::kM) {
            est = speed_m(e, t0, ts);
        } else if (method == SpeedMethod::kT) {
            est = speed_t(e, t0, f_clk);
        } else {
            est = speed_mt(e, t0, ts, f_clk);
        }
        const double err = (est - omega) / omega * 100.0;
        sum_est += est;
        sum_err += err;
        if (std::fabs(err) > worst) {
            worst = std::fabs(err);
        }
        t0 += ts;
    }

    m.mean_estimate = sum_est / count;
    m.mean_error = sum_err / count;
    m.max_error = worst;
    return m;
}

/* ==================================================================
 * 阶段 5：不该用 PID 的时候
 * ================================================================== */

double feedforward_command(const FeedForward &m, const ReferencePoint &ref)
{
    /* TODO（阶段 5-1）：前馈项。
     * 反馈要等误差出现才动作，前馈不必等：目标的走法事先就知道，
     * 因此可以先把「照这个走法需要出多少力」算出来，直接加到命令上。
     * 四件事要想清楚：
     *   1. 对象不动的时候（变化率为 0），要让输出停在 r 上，命令该是多少？
     *      这一条只用到模型的静态增益。
     *   2. 输出要以每秒 dr 的速度往前走的时候，命令要比上面多出多少？
     *      多出来的那一份与模型的时间常数是什么关系？
     *      想不清楚就代进去验算：把命令写进对象的一阶方程，看输出是不是
     *      正好等于 r(t) = dr * t。算出来的命令应当让等式两边完全相等，
     *      一项不多一项不少。
     *   3. 两项的单位要一致：一项的量纲是「命令」，另一项是
     *      「时间乘变化率再除增益」，也是命令。
     *   4. 这里用的模型参数是控制器以为的那一组，不是对象的真值。
     *      两者不一样时剩下的误差由反馈补——验收表里有两行专门看这件事。
     * 记数契约：本阶段打印一张四行的表，行是「只有反馈」「前馈加反馈」
     *   与「对象增益换成 2.4000」之后的同样两行；列是 rms error 与
     *   max error。参考轨迹是 r(t) = 0.4000 * t。
     * 判据（《配置步骤.md》阶段 5）：模型对得上时，前馈那一行的两个误差
     *   都是 0.0000，而只有反馈那一行是 0.0250（正好是 v 除以 K 乘 ki）；
     *   对象增益偏大 20% 之后，前馈那一行剩下 0.0042，
     *   正好是同一行只有反馈时的两成。 */
    (void)m;
    (void)ref;
    return 0.0;               /* 占位实现：一点前馈也不给 */
}

TrackResult run_tracking(const PlantConfig &cfg, const PidGains &g, const FeedForward &model,
                         bool use_ff, double speed, int steps)
{
    PidState s;
    Noise nz(kSeed);
    double y = cfg.y0;

    std::vector<double> err(static_cast<std::size_t>(steps), 0.0);

    for (int k = 0; k < steps; ++k) {
        const double t = static_cast<double>(k) * cfg.dt;

        ReferencePoint ref;
        ref.r = speed * t;
        ref.dr = speed;

        const double meas = y + cfg.noise_amp * nz.next_unit();
        const double fb = pid_step(g, s, ref.r, meas, cfg.dt);
        const double ff = use_ff ? feedforward_command(model, ref) : 0.0;
        const double u = fb + ff;

        y += (cfg.gain * clamp_command(u, cfg.u_max) - y) * (cfg.dt / cfg.tau);

        /* 这一拍的命令是照着 ref.r 算的，对象走完这一拍落在下一拍的时刻上，
         * 因此误差要拿「走完之后的真值」与「下一拍的参考」比。 */
        err[static_cast<std::size_t>(k)] = y - speed * static_cast<double>(k + 1) * cfg.dt;
    }

    TrackResult r;
    double acc = 0.0;
    double worst = 0.0;
    for (int k = 0; k < steps; ++k) {
        const double e = err[static_cast<std::size_t>(k)];
        acc += e * e;
        if (std::fabs(e) > worst) {
            worst = std::fabs(e);
        }
    }
    r.rms_error = std::sqrt(acc / static_cast<double>(steps));
    r.max_error = worst;
    return r;
}

double actuator_drive(double u, double u_max)
{
    /* 死区之内什么也不出；出了死区之后出力按平方走，
     * 因此工作点越高，增量增益越大。 */
    const double a = std::fabs(u);
    if (a <= kActuatorDeadZone) {
        return 0.0;
    }
    const double over = a - kActuatorDeadZone;
    const double d = over * over / u_max;
    return (u > 0.0) ? d : -d;
}

ActuatorTable calibrate_actuator(double u_max, int entries)
{
    ActuatorTable t;
    const int n = (entries > 1) ? entries : 2;
    for (int i = 0; i < n; ++i) {
        const double u = -u_max + 2.0 * u_max * static_cast<double>(i) / static_cast<double>(n - 1);
        t.cmd.push_back(u);
        t.drive.push_back(actuator_drive(u, u_max));
    }
    return t;
}

double lookup_command(const ActuatorTable &t, double want_drive)
{
    /* TODO（阶段 5-2）：查表。
     * 表里放着「命令 → 实际出力」的对应关系，现在要反过来用：
     * 手里有一个想要的出力，问该下什么命令。
     * 五件事要想清楚：
     *   1. 表里的出力是随命令单调增的（特性本身单调），因此可以顺着
     *      表找第一个不小于 want_drive 的点，答案落在它前一个点与它之间。
     *   2. 两个点之间用线性插值：先算 want_drive 在这一小段里占多少比例
     *      （分母是两个出力之差），再用同一个比例去分两个命令之差。
     *      分母为 0 的那一段要跳过，不要让除法出问题。
     *   3. 想要的出力超出表的范围时，取表两端的命令——那已经是执行器
     *      能给到的极限，再往外查没有意义。
     *   4. 表里有一段平的死区（出力一直是 0）。反查在死区里不唯一，
     *      落在哪一端都行，只要下下去以后执行器真的不出力。
     *   5. 表是「量出来的」，不是解析式：真机上没有公式可代，
     *      只有这一张点表。这也是为什么要查表而不是算反函数。
     * 记数契约：本阶段打印一张五行的表，行是五个目标值
     *   （0.2000 / 0.5000 / 1.0000 / 1.8000 / 2.5000）；
     *   列是「不补偿的调节时间」与「查表补偿的调节时间」。
     * 判据（《配置步骤.md》阶段 5）：静态那张表里，不补偿那一列在低工作点
     *   上一点力也给不出（想要 0.1000 与 0.3000，拿到的都是 0.0000），
     *   高工作点上又少给（想要 1.4000 只拿到 0.6050）；查表那一列
     *   五个点与想要的都差不到 0.001。调节时间那张表里，不补偿那一列
     *   从 1.8200 一路降到 0.1000（小信号上环路增益低，慢；大信号上
     *   增益高，快，差出十几倍）；查表把执行器折成了直线，
     *   五行的 final error 都是 0.0000。 */
    (void)t;
    (void)want_drive;
    return want_drive;        /* 占位实现：想要多少就直接下多少，等于不补偿 */
}

NonlinearResult run_nonlinear(const PlantConfig &cfg, const PidGains &g, double setpoint,
                              int steps, const ActuatorTable *table)
{
    PidState s;
    Noise nz(kSeed);
    double y = cfg.y0;

    std::vector<double> out(static_cast<std::size_t>(steps), 0.0);

    for (int k = 0; k < steps; ++k) {
        const double meas = y + cfg.noise_amp * nz.next_unit();
        const double want = pid_step(g, s, setpoint, meas, cfg.dt);
        const double u = (table != 0) ? lookup_command(*table, want) : want;
        const double drive = actuator_drive(u, cfg.u_max);

        y += (cfg.gain * drive - y) * (cfg.dt / cfg.tau);
        out[static_cast<std::size_t>(k)] = y;
    }

    NonlinearResult r;

    const std::size_t last = (out.size() > 100) ? 100 : out.size();
    double sum = 0.0;
    for (std::size_t i = out.size() - last; i < out.size(); ++i) {
        sum += out[i];
    }
    r.final_error = ((last > 0) ? (sum / static_cast<double>(last)) : 0.0) - setpoint;

    const double band = 0.02 * setpoint;
    std::size_t last_bad = 0;
    bool any_bad = false;
    for (std::size_t i = 0; i < out.size(); ++i) {
        const double d = out[i] - setpoint;
        if ((d > band) || (d < -band)) {
            last_bad = i + 1;
            any_bad = true;
        }
    }
    if (!any_bad) {
        r.settle_time = 0.0;
    } else if (last_bad >= out.size()) {
        r.settle_time = -1.0;
    } else {
        r.settle_time = static_cast<double>(last_bad) * cfg.dt;
    }
    return r;
}

Trapezoid plan_trapezoid(double dist, double v_max, double a_max)
{
    Trapezoid p;
    p.dist = dist;
    p.v_max = v_max;
    p.a_max = a_max;

    /* TODO（阶段 5-3）：梯形速度规划。
     * 一段点到点的运动分三段：在加速度上限内把速度拉到速度上限，
     * 匀速走一段，再在加速度上限内减到停，正好停在 dist 处。
     * 五件事要想清楚：
     *   1. 加速段要多久才能从 0 到 v_max？它只由 v_max 与 a_max 决定。
     *      减速段与它对称，因此 td 等于 ta。
     *   2. 加速段走过多少距离？（初速 0、匀加速，距离是时间乘平均速度。）
     *      加、减速两段一共占掉多少距离？
     *   3. 剩下的距离交给匀速段：tc 等于剩余距离除以 v_max。
     *      剩余距离若是负的，说明这段路根本来不及加到 v_max——
     *      那就是三角形的情况：加速段与减速段各占一半距离，
     *      能到达的最高速度由 dist 与 a_max 决定（令两段距离之和等于 dist
     *      解出来），ta 再由这个速度与 a_max 算出，tc 是 0。
     *   4. v_peak 是实际到达的最高速度：够长就是 v_max，太短就是三角形
     *      那个速度。trapezoid_position 用它算匀速段的位置。
     *   5. total 是三段之和。位置函数靠 ta、tc、total 与 v_peak 四个数
     *      把 t 映到位置上，少一个都对不上。
     * 记数契约：本阶段先打印一行三段时长（ta / tc / td / total），
     *   再打印一张两行的表，行是「阶跃参考」与「梯形参考」；
     *   列是 move time、max |error|、rms error、max |u|、saturated。
     * 判据（《配置步骤.md》阶段 5）：距离 3.0000、速度上限 0.5000、
     *   加速度上限 1.0000 时，三段是 ta 0.5000 s、tc 5.5000 s、
     *   td 0.5000 s、total 6.5000 s，v peak 0.5000；
     *   梯形那一行的 max |error| 是 0.0377，阶跃那一行是 2.9200，
     *   小两个数量级；梯形那一行的 saturated 是 0，阶跃那一行是 124。 */
    return p;                 /* 占位实现：三段时长都是 0，参考一直停在终点 */
}

double trapezoid_position(const Trapezoid &p, double t)
{
    if (t <= 0.0) {
        return 0.0;
    }
    if (t >= p.total) {
        return p.dist;
    }
    if (t < p.ta) {
        return 0.5 * p.a_max * t * t;
    }
    if (t < p.ta + p.tc) {
        const double d1 = 0.5 * p.a_max * p.ta * p.ta;
        return d1 + p.v_peak * (t - p.ta);
    }
    const double back = p.total - t;
    return p.dist - 0.5 * p.a_max * back * back;
}

MoveResult run_move(const PlantConfig &cfg, const PidGains &g, double dist,
                    double v_max, double a_max, int steps, bool use_trapezoid)
{
    const Trapezoid p = plan_trapezoid(dist, v_max, a_max);

    PidState s;
    Noise nz(kSeed);
    double y = cfg.y0;

    std::vector<double> out(static_cast<std::size_t>(steps), 0.0);
    std::vector<double> ref(static_cast<std::size_t>(steps), 0.0);
    std::vector<double> cmd(static_cast<std::size_t>(steps), 0.0);

    for (int k = 0; k < steps; ++k) {
        const double t = static_cast<double>(k) * cfg.dt;
        const double r = use_trapezoid ? trapezoid_position(p, t) : dist;

        /* 参考记的是「走完这一拍之后」的时刻：命令照着这一拍的参考算，
         * 对象落在下一拍上，两者对齐了误差才有意义。 */
        ref[static_cast<std::size_t>(k)] =
            use_trapezoid ? trapezoid_position(p, t + cfg.dt) : dist;

        const double meas = y + cfg.noise_amp * nz.next_unit();
        const double u = pid_step(g, s, r, meas, cfg.dt);
        cmd[static_cast<std::size_t>(k)] = u;

        y += (cfg.gain * clamp_command(u, cfg.u_max) - y) * (cfg.dt / cfg.tau);
        out[static_cast<std::size_t>(k)] = y;
    }

    MoveResult m;
    double acc = 0.0;
    for (int k = 0; k < steps; ++k) {
        const double e = out[static_cast<std::size_t>(k)] - ref[static_cast<std::size_t>(k)];
        acc += e * e;
        if (std::fabs(e) > m.max_error) {
            m.max_error = std::fabs(e);
        }
        const double au = std::fabs(cmd[static_cast<std::size_t>(k)]);
        if (au > m.max_u) {
            m.max_u = au;
        }
        if (au >= cfg.u_max) {
            ++m.sat_steps;
        }
    }
    m.rms_error = std::sqrt(acc / static_cast<double>(steps));

    const double band = 0.02 * dist;
    std::size_t last_bad = 0;
    bool any_bad = false;
    for (std::size_t i = 0; i < out.size(); ++i) {
        const double d = out[i] - dist;
        if ((d > band) || (d < -band)) {
            last_bad = i + 1;
            any_bad = true;
        }
    }
    if (!any_bad) {
        m.move_time = 0.0;
    } else if (last_bad >= out.size()) {
        m.move_time = -1.0;
    } else {
        m.move_time = static_cast<double>(last_bad) * cfg.dt;
    }
    return m;
}

} /* namespace ctl */
