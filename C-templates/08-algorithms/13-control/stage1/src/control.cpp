/* control.cpp —— 练习模板 13 的实现（C++）—— 阶段 1
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
 * 本阶段有 2 处 TODO，都在这个文件里：
 *
 *     阶段 1-1   Plant::step        一阶惯性 + 饱和执行器
 *     阶段 1-2   on_off_command     带滞回的开关控制
 *
 * 每个 TODO 上面写明「要做什么」、几个要想清楚的问题、要打印哪些量，
 * 以及填完之后应当看到的数——那些数都由 src/main_cli.cpp 打印，
 * 逐个对得上才算做完。自检用 app_cli.exe --selftest，它返回失败的项数。
 */
#include "control.hpp"

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

} /* namespace ctl */
