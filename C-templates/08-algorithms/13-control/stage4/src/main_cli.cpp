/* main_cli.cpp —— 练习模板 13 的命令行验收程序（C++）—— 阶段 4
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
 * 两种用法：
 *     app_cli.exe              按阶段打印测量报告，总是返回 0
 *     app_cli.exe --selftest   跑判据，返回失败的项数（全过就是 0）
 *
 * 这个文件**不需要改**：它只调用 include/control.hpp 里的接口。
 * 你的实现写在 src/control.cpp 里。骨架状态下报告能打完、返回 0，
 * 但 --selftest 会报出一批 [FAIL]——那正是还没做完的部分。
 *
 * 构建与运行（在 13-control 目录下）：
 *     cmake -S stage4 -B %TEMP%\ctl-st4 -G Ninja -DCMAKE_CXX_COMPILER=g++
 *     cmake --build %TEMP%\ctl-st4
 *     %TEMP%\ctl-st4\bin\app_cli.exe
 *     %TEMP%\ctl-st4\bin\app_cli.exe --selftest
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 */
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "control.hpp"

namespace {

/* 标签列的宽度：本模板所有「标签 : 值」的行都用这一个宽度 */
const int kLabelWidth = 36;

/* 四位小数。绝对值小于半个最低位时归零，免得打出 -0.0000 */
std::string fmt4(double v)
{
    if (v > -0.00005 && v < 0.00005) {
        v = 0.0;
    }
    std::ostringstream os;
    os << std::fixed << std::setprecision(4) << v;
    return os.str();
}

void fline(const char *label, double v)
{
    std::cout << std::left << std::setw(kLabelWidth) << label << ": " << fmt4(v) << "\n";
}

/* 八位小数：Q16.16 的分辨率是 1.5e-5，四位小数看不出来 */
std::string fmt8(double v)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(8) << v;
    return os.str();
}

void fline8(const char *label, double v)
{
    std::cout << std::left << std::setw(kLabelWidth) << label << ": " << fmt8(v) << "\n";
}

void iline(const char *label, long long v)
{
    std::cout << std::left << std::setw(kLabelWidth) << label << ": " << v << "\n";
}

/* ---------------- 阶段 1 的固定数据 ---------------- */

const int kSteps = 4000;                 /* 每一遍跑多少拍 */
const double kSetpoint = 1.0;            /* 目标值 */

void stage1()
{
    ctl::PlantConfig c;

    std::cout << "=== Stage 1: the plant, open loop, and on-off control ===\n";

    std::cout << "--- the plant ---\n";
    fline("dt", c.dt);
    fline("tau", c.tau);
    fline("gain K", c.gain);
    std::cout << std::left << std::setw(kLabelWidth) << "command range"
              << ": " << fmt4(-c.u_max) << " .. " << fmt4(c.u_max) << "\n";
    fline("measurement noise amplitude", c.noise_amp);
    fline("output at rest", c.y0);

    std::cout << "--- open loop: one fixed command, and what is left ---\n";
    iline("steps", kSteps);
    fline("setpoint r", kSetpoint);
    fline("command u = r / gain", kSetpoint / c.gain);

    std::cout << std::left << std::setw(14) << "true gain"
              << std::right << std::setw(18) << "settled output"
              << std::setw(14) << "error" << "\n";

    const double gains[3] = {2.0, 2.5, 1.25};
    for (int i = 0; i < 3; ++i) {
        ctl::PlantConfig g = c;
        g.gain = gains[i];
        const double settled = ctl::run_open_loop(g, kSetpoint / c.gain, kSteps);
        std::cout << std::left << std::setw(14) << fmt4(gains[i])
                  << std::right << std::setw(18) << fmt4(settled)
                  << std::setw(14) << fmt4(kSetpoint - settled) << "\n";
    }

    std::cout << "--- open loop with the command beyond the actuator range ---\n";
    fline("commanded u", 5.0);
    fline("after the limit", ctl::clamp_command(5.0, c.u_max));
    fline("settled output", ctl::run_open_loop(c, 5.0, kSteps));

    std::cout << "--- on-off control, with and without hysteresis ---\n";
    iline("steps", kSteps);

    std::cout << std::left << std::setw(12) << "hysteresis"
              << std::right << std::setw(12) << "switches"
              << std::setw(12) << "on-steps"
              << std::setw(12) << "duty"
              << std::setw(16) << "tail ripple"
              << std::setw(14) << "final error" << "\n";

    const double bands[4] = {0.0, 0.05, 0.2, 0.5};
    for (int i = 0; i < 4; ++i) {
        const ctl::OnOffResult r = ctl::run_on_off(c, kSetpoint, bands[i], kSteps);
        std::cout << std::left << std::setw(12) << fmt4(bands[i])
                  << std::right << std::setw(12) << r.switches
                  << std::setw(12) << r.on_steps
                  << std::setw(12) << fmt4(r.duty)
                  << std::setw(16) << fmt4(r.tail_ripple)
                  << std::setw(14) << fmt4(r.final_error) << "\n";
    }
}

/* ---------------- 阶段 2 的固定数据 ---------------- */

const int kLoopSteps = 2000;         /* 一次闭环跑多少拍 */
const double kLoopSetpoint = 1.0;    /* 阶跃目标 */

/* 四组增益：把某一项置 0，PID 就退化成两项或一项 */
struct GainSet {
    const char *name;
    ctl::PidGains gains;
};

const GainSet kGainSets[4] = {
    {"P",   {4.0, 0.0, 0.0}},
    {"PI",  {4.0, 8.0, 0.0}},
    {"PD",  {4.0, 0.0, 0.1}},
    {"PID", {4.0, 8.0, 0.1}}
};

void stage2()
{
    ctl::PlantConfig quiet;          /* 噪声关掉：看结构 */
    quiet.noise_amp = 0.0;

    const ctl::PlantConfig noisy;    /* 噪声打开：看噪声被放大成什么 */

    std::cout << "\n=== Stage 2: P, PI, PD, PID on one step ===\n";
    fline("setpoint", kLoopSetpoint);
    iline("steps", kLoopSteps);

    std::cout << "--- the four gain sets ---\n";
    std::cout << std::left << std::setw(14) << "controller"
              << std::right << std::setw(10) << "kp"
              << std::setw(10) << "ki"
              << std::setw(10) << "kd" << "\n";
    for (int i = 0; i < 4; ++i) {
        std::cout << std::left << std::setw(14) << kGainSets[i].name
                  << std::right << std::setw(10) << fmt4(kGainSets[i].gains.kp)
                  << std::setw(10) << fmt4(kGainSets[i].gains.ki)
                  << std::setw(10) << fmt4(kGainSets[i].gains.kd) << "\n";
    }

    std::cout << "--- measurement noise off: steady state, overshoot, settling ---\n";
    std::cout << std::left << std::setw(14) << "controller"
              << std::right << std::setw(14) << "final error"
              << std::setw(14) << "overshoot %"
              << std::setw(14) << "settle time"
              << std::setw(12) << "max |u|"
              << std::setw(12) << "saturated" << "\n";
    for (int i = 0; i < 4; ++i) {
        const ctl::LoopMetrics m = ctl::run_loop(quiet, kGainSets[i].gains,
                                                 kLoopSetpoint, kLoopSteps);
        std::cout << std::left << std::setw(14) << kGainSets[i].name
                  << std::right << std::setw(14) << fmt4(m.final_error)
                  << std::setw(14) << fmt4(m.overshoot)
                  << std::setw(14) << fmt4(m.settle_time)
                  << std::setw(12) << fmt4(m.u_peak)
                  << std::setw(12) << m.sat_steps << "\n";
    }

    std::cout << "--- measurement noise on: what the noise turns into ---\n";
    std::cout << std::left << std::setw(14) << "controller"
              << std::right << std::setw(14) << "final error"
              << std::setw(14) << "y ripple"
              << std::setw(14) << "u ripple"
              << std::setw(12) << "max |u|"
              << std::setw(12) << "saturated" << "\n";
    for (int i = 0; i < 4; ++i) {
        const ctl::LoopMetrics m = ctl::run_loop(noisy, kGainSets[i].gains,
                                                 kLoopSetpoint, kLoopSteps);
        std::cout << std::left << std::setw(14) << kGainSets[i].name
                  << std::right << std::setw(14) << fmt4(m.final_error)
                  << std::setw(14) << fmt4(m.y_ripple)
                  << std::setw(14) << fmt4(m.u_ripple)
                  << std::setw(12) << fmt4(m.u_peak)
                  << std::setw(12) << m.sat_steps << "\n";
    }
}

/* ---------------- 阶段 3 的固定数据 ---------------- */

const double kPidSetpoint = 1.0;                    /* 定点对照用的阶跃 */
const ctl::PidGains kPidGains = {4.0, 8.0, 0.1};    /* 一组调得还算稳的 PID */
const double kSamplingPeriods[5] = {0.01, 0.02, 0.05, 0.1, 0.2};
const double kSamplingTotal = 10.0;                 /* 五档都跑同样的总时长 */

const double kAwSetpoint = 3.0;                     /* 大到一定把执行器顶住 */
const int kAwSteps = 1000;                          /* 10 s */
const double kTrackingTime = 0.05;                  /* 反算用的跟踪时间常数，s */
const int kLoadFrom = 500;                          /* 第 5 s 加上负载 */
const int kLoadUntil = 600;                         /* 第 6 s 撤掉 */
const double kLoad = -1.0;                          /* 大到把执行器顶住 */

const ctl::AntiWindup kModes[3] = {ctl::AntiWindup::kLimitOnly,
                                   ctl::AntiWindup::kClamp,
                                   ctl::AntiWindup::kBackCalc};
const char *const kModeNames[3] = {"limit only", "clamp", "back calculation"};

void stage3()
{
    ctl::PlantConfig quiet;      /* 噪声关掉：这几张表看的是结构 */
    quiet.noise_amp = 0.0;
    const ctl::PlantConfig noisy;

    std::cout << "\n=== Stage 3: fixed point, limiting, and the integral ===\n";

    /* ---------------- 定点与浮点 ---------------- */
    std::cout << "--- one controller, two number formats ---\n";
    fline("setpoint", kPidSetpoint);
    iline("steps", kLoopSteps);

    const ctl::LoopMetrics fl = ctl::run_loop(quiet, kPidGains, kPidSetpoint, kLoopSteps);
    const ctl::LoopMetrics fx = ctl::run_loop_fixed(quiet, kPidGains, kPidSetpoint, kLoopSteps);
    const ctl::LoopMetrics nl = ctl::run_loop(noisy, kPidGains, kPidSetpoint, kLoopSteps);
    const ctl::LoopMetrics nx = ctl::run_loop_fixed(noisy, kPidGains, kPidSetpoint, kLoopSteps);

    std::cout << std::left << std::setw(22) << ""
              << std::right << std::setw(14) << "float"
              << std::setw(14) << "Q16.16" << "\n";
    std::cout << std::left << std::setw(22) << "final error"
              << std::right << std::setw(14) << fmt4(fl.final_error)
              << std::setw(14) << fmt4(fx.final_error) << "\n";
    std::cout << std::left << std::setw(22) << "overshoot %"
              << std::right << std::setw(14) << fmt4(fl.overshoot)
              << std::setw(14) << fmt4(fx.overshoot) << "\n";
    std::cout << std::left << std::setw(22) << "settle time"
              << std::right << std::setw(14) << fmt4(fl.settle_time)
              << std::setw(14) << fmt4(fx.settle_time) << "\n";
    std::cout << std::left << std::setw(22) << "max |u|"
              << std::right << std::setw(14) << fmt4(fl.u_peak)
              << std::setw(14) << fmt4(fx.u_peak) << "\n";
    std::cout << std::left << std::setw(22) << "u ripple, noise on"
              << std::right << std::setw(14) << fmt4(nl.u_ripple)
              << std::setw(14) << fmt4(nx.u_ripple) << "\n";
    std::cout << std::left << std::setw(22) << "y ripple, noise on"
              << std::right << std::setw(14) << fmt4(nl.y_ripple)
              << std::setw(14) << fmt4(nx.y_ripple) << "\n";
    iline("integral clamps, Q16.16", fx.integ_clamps);
    fline8("quantization step (1 / 65536)", 1.0 / 65536.0);
    fline("value range, lower end", ctl::q_to_double(-2147483647 - 1));
    fline("value range, upper end", ctl::q_to_double(2147483647));

    /* ---------------- 采样周期 ---------------- */
    std::cout << "--- the same controller at five sampling periods ---\n";
    fline("total time", kSamplingTotal);
    std::cout << std::left << std::setw(10) << "dt"
              << std::right << std::setw(8) << "steps"
              << std::setw(14) << "final error"
              << std::setw(14) << "overshoot %"
              << std::setw(14) << "settle time"
              << std::setw(12) << "max |u|" << "\n";
    for (int i = 0; i < 5; ++i) {
        ctl::PlantConfig c = quiet;
        c.dt = kSamplingPeriods[i];
        const int steps = static_cast<int>(kSamplingTotal / c.dt + 0.5);
        const ctl::LoopMetrics m = ctl::run_loop(c, kPidGains, kPidSetpoint, steps);
        std::cout << std::left << std::setw(10) << fmt4(c.dt)
                  << std::right << std::setw(8) << steps
                  << std::setw(14) << fmt4(m.final_error)
                  << std::setw(14) << fmt4(m.overshoot)
                  << std::setw(14) << fmt4(m.settle_time)
                  << std::setw(12) << fmt4(m.u_peak) << "\n";
    }

    /* ---------------- 限幅与积分饱和 ---------------- */
    std::cout << "--- a setpoint step into the actuator limit ---\n";
    fline("setpoint", kAwSetpoint);
    fline("actuator lower end", -quiet.u_max);
    fline("actuator upper end", quiet.u_max);
    iline("steps", kAwSteps);

    std::cout << std::left << std::setw(18) << "anti-windup"
              << std::right << std::setw(14) << "overshoot %"
              << std::setw(14) << "settle time"
              << std::setw(16) << "max |integral|"
              << std::setw(12) << "saturated"
              << std::setw(14) << "final error" << "\n";
    for (int i = 0; i < 3; ++i) {
        const ctl::AwResult a = ctl::run_aw(quiet, kPidGains, kAwSetpoint, kAwSteps,
                                            kModes[i], kTrackingTime);
        std::cout << std::left << std::setw(18) << kModeNames[i]
                  << std::right << std::setw(14) << fmt4(a.overshoot)
                  << std::setw(14) << fmt4(a.settle_time)
                  << std::setw(16) << fmt4(a.max_integ)
                  << std::setw(12) << a.sat_steps
                  << std::setw(14) << fmt4(a.final_error) << "\n";
    }

    std::cout << "--- a load step, and how long the error takes to come back ---\n";
    fline("load added to the command", kLoad);
    fline("added at", kLoadFrom * quiet.dt);
    fline("removed at", kLoadUntil * quiet.dt);
    std::cout << std::left << std::setw(18) << "anti-windup"
              << std::right << std::setw(16) << "recovery time"
              << std::setw(14) << "final error" << "\n";
    for (int i = 0; i < 3; ++i) {
        const ctl::LoadResult lr = ctl::run_load_recovery(quiet, kPidGains, kAwSetpoint,
                                                          kAwSteps, kModes[i],
                                                          kTrackingTime, kLoadFrom,
                                                          kLoadUntil, kLoad);
        std::cout << std::left << std::setw(18) << kModeNames[i]
                  << std::right << std::setw(16) << fmt4(lr.recovery_time)
                  << std::setw(14) << fmt4(lr.final_error) << "\n";
    }
}

/* ---------------- 阶段 4 的固定数据 ---------------- */

const double kFilterDt = 0.001;        /* 采样周期 1 ms */
const int kFilterSteps = 8000;         /* 一共 8 s */
const double kFilterStepAt = 0.5;      /* 阶跃发生在第 0.5 s */
const double kFilterNoise = 0.05;

struct FilterCase {
    const char *name;
    ctl::FilterKind kind;
    double param;
};

const FilterCase kFilterCases[5] = {
    {"none",               ctl::FilterKind::kNone,    0.0},
    {"low pass tf=0.0100", ctl::FilterKind::kLowPass, 0.01},
    {"low pass tf=0.0500", ctl::FilterKind::kLowPass, 0.05},
    {"average N=16",       ctl::FilterKind::kAverage, 16.0},
    {"average N=128",      ctl::FilterKind::kAverage, 128.0}
};

const double kCpr = 4096.0;            /* 编码器每转的计数个数 */
const double kSpeedWindow = 0.01;      /* 采样窗口 10 ms */
const double kClock = 10000000.0;      /* 测时间的时钟 10 MHz */
const int kSpeedWindows = 100;         /* 每行统计 100 个窗口 */
const double kSpeeds[2] = {0.5, 50.0}; /* 30 rpm 与 3000 rpm */

void stage4()
{
    std::cout << "\n=== Stage 4: filtering the measurement, and measuring speed ===\n";

    std::cout << "--- a step plus noise, filtered two ways ---\n";
    fline("sample period", kFilterDt);
    fline("step at", kFilterStepAt);
    fline("noise amplitude", kFilterNoise);
    iline("samples", kFilterSteps);

    std::cout << std::left << std::setw(20) << "filter"
              << std::right << std::setw(14) << "noise rms"
              << std::setw(14) << "t50"
              << std::setw(14) << "lag" << "\n";
    for (int i = 0; i < 5; ++i) {
        const ctl::FilterMetrics m = ctl::run_filter(kFilterCases[i].kind, kFilterCases[i].param,
                                                     kFilterDt, kFilterSteps, kFilterStepAt);
        std::cout << std::left << std::setw(20) << kFilterCases[i].name
                  << std::right << std::setw(14) << fmt4(m.noise_rms)
                  << std::setw(14) << fmt4(m.t50)
                  << std::setw(14) << fmt4(m.lag) << "\n";
    }

    std::cout << "--- the same shaft, three ways to measure its speed ---\n";
    fline("counts per turn", kCpr);
    fline("window", kSpeedWindow);
    fline("clock frequency", kClock);
    iline("windows per row", kSpeedWindows);

    const ctl::SpeedMethod methods[3] = {ctl::SpeedMethod::kM,
                                         ctl::SpeedMethod::kT,
                                         ctl::SpeedMethod::kMT};
    const char *const names[3] = {"M", "T", "M/T"};

    std::cout << std::left << std::setw(14) << "true speed"
              << std::setw(8) << "method"
              << std::right << std::setw(16) << "mean estimate"
              << std::setw(16) << "mean error %"
              << std::setw(16) << "max error %" << "\n";
    for (int s = 0; s < 2; ++s) {
        for (int i = 0; i < 3; ++i) {
            const ctl::SpeedMetrics m = ctl::run_speed(methods[i], kSpeeds[s], kCpr,
                                                       kSpeedWindow, kClock, kSpeedWindows);
            std::cout << std::left << std::setw(14) << fmt4(kSpeeds[s])
                      << std::setw(8) << names[i]
                      << std::right << std::setw(16) << fmt4(m.mean_estimate)
                      << std::setw(16) << fmt4(m.mean_error)
                      << std::setw(16) << fmt4(m.max_error) << "\n";
        }
    }
}

/* ---------------- 自检 ---------------- */

int g_pass = 0;
int g_fail = 0;

void check(bool ok, const std::string &name)
{
    if (ok) {
        ++g_pass;
        std::cout << "  [ok]   " << name << "\n";
    } else {
        ++g_fail;
        std::cout << "  [FAIL] " << name << "\n";
    }
}

bool nearly(double a, double b, double tol)
{
    return std::fabs(a - b) <= tol;
}

int run_selftest()
{
    std::cout << "=== selftest ===\n";

    const ctl::PlantConfig c;   /* 默认参数就是验收用的那一组 */

    /* 阶段 1-1：一阶惯性 + 饱和执行器 */
    check(nearly(ctl::run_open_loop(c, 0.5, kSteps), 1.0, 1e-6),
          "a constant command of 0.5 settles at K * u = 1.0");

    ctl::PlantConfig hi = c;
    hi.gain = 2.5;
    check(nearly(ctl::run_open_loop(hi, 0.5, kSteps), 1.25, 1e-6),
          "with a true gain of 2.5 the same command settles at 1.25");

    ctl::PlantConfig lo = c;
    lo.gain = 1.25;
    check(nearly(ctl::run_open_loop(lo, 0.5, kSteps), 0.625, 1e-6),
          "with a true gain of 1.25 the same command settles at 0.625");

    check(nearly(ctl::run_open_loop(c, 5.0, kSteps), 4.0, 1e-6),
          "a command of 5.0 is cut to 2.0 and settles at 4.0");
    check(nearly(ctl::run_open_loop(c, -5.0, kSteps), -4.0, 1e-6),
          "the same cut holds on the negative side");

    ctl::Plant p(c);
    const double y1 = p.step(1.0);
    check(y1 > 0.0 && y1 < 0.3,
          "one step moves a small part of the way, not all of it");

    /* 阶段 1-2：带滞回的开关控制 */
    const ctl::OnOffResult r0 = ctl::run_on_off(c, kSetpoint, 0.0, kSteps);
    const ctl::OnOffResult r2 = ctl::run_on_off(c, kSetpoint, 0.2, kSteps);
    const ctl::OnOffResult r5 = ctl::run_on_off(c, kSetpoint, 0.5, kSteps);

    check(r5.switches > 0,
          "with a 0.5 band the command still switches");
    check(r0.switches > r2.switches && r2.switches > r5.switches,
          "a wider band always means fewer switches");
    check(r0.switches > 5 * r5.switches,
          "without a band the command switches far more often");
    check(r5.tail_ripple > 0.40 && r5.tail_ripple < 0.65,
          "a 0.5 band holds the true value within about 0.5");
    check(r2.tail_ripple > 0.10 && r2.tail_ripple < 0.35,
          "a 0.2 band holds the true value within about 0.2");
    check(r0.tail_ripple < 0.15,
          "without a band the true value hugs the setpoint");
    check(r0.duty > 0.15 && r0.duty < 0.35,
          "the duty cycle sits near r / (K * u_max) = 0.25");
    check(r5.duty > 0.15 && r5.duty < 0.35,
          "the duty cycle stays there when the band widens");

    /* 阶段 2：PID 三项 */
    ctl::PlantConfig quiet;
    quiet.noise_amp = 0.0;
    const ctl::PlantConfig noisy;

    const ctl::LoopMetrics mp = ctl::run_loop(quiet, kGainSets[0].gains, kLoopSetpoint, kLoopSteps);
    const ctl::LoopMetrics mpi = ctl::run_loop(quiet, kGainSets[1].gains, kLoopSetpoint, kLoopSteps);
    const ctl::LoopMetrics mpd = ctl::run_loop(quiet, kGainSets[2].gains, kLoopSetpoint, kLoopSteps);
    const ctl::LoopMetrics mpid = ctl::run_loop(quiet, kGainSets[3].gains, kLoopSetpoint, kLoopSteps);

    /* P 只剩一个比例项：命令等于 kp 乘误差，误差要是 0 命令就是 0，
     * 而维持住输出恰恰需要一个非零命令，因此必然留下稳态误差。
     * 这个数是算得出来的：y = K * kp * (r - y)，解出 r - y = r / (1 + K * kp)。 */
    check(nearly(mp.final_error, -1.0 / 9.0, 5e-3),
          "P leaves a steady state error of about 0.111");
    check(mp.settle_time < 0.0,
          "P never enters the 2 percent band");
    check(mp.overshoot < 1e-9,
          "a first order plant under P alone does not overshoot");

    /* 积分项的作用就是把上面那个误差消掉 */
    check(nearly(mpi.final_error, 0.0, 2e-3),
          "PI drives the steady state error to zero");
    check(mpi.settle_time > 0.0,
          "PI does enter the 2 percent band");
    check(mpi.overshoot > 0.5,
          "PI overshoots, because the integral keeps filling while the command is cut");

    /* 微分项不改变稳态，只改变过程 */
    check(nearly(mpd.final_error, -1.0 / 9.0, 5e-3),
          "PD leaves the same steady state error as P");
    check(mpd.overshoot < 1e-9,
          "PD does not overshoot either");

    check(nearly(mpid.final_error, 0.0, 2e-3),
          "PID drives the steady state error to zero as well");
    check(mpid.settle_time > 0.0,
          "PID does enter the 2 percent band");

    /* 微分项若对误差求变化率，目标一跳就会冒出一个尖峰：
     * kd * 1.0 / dt 是 10 这个量级，命令的峰值会顶到十几。 */
    check(mp.u_peak < 6.0 && mpd.u_peak < 6.0 && mpid.u_peak < 6.0,
          "the first command stays near kp, with no derivative kick");

    /* 噪声打开：微分项把测量噪声放大成命令的抖动 */
    const ctl::LoopMetrics np = ctl::run_loop(noisy, kGainSets[0].gains, kLoopSetpoint, kLoopSteps);
    const ctl::LoopMetrics npi = ctl::run_loop(noisy, kGainSets[1].gains, kLoopSetpoint, kLoopSteps);
    const ctl::LoopMetrics npd = ctl::run_loop(noisy, kGainSets[2].gains, kLoopSetpoint, kLoopSteps);
    const ctl::LoopMetrics npid = ctl::run_loop(noisy, kGainSets[3].gains, kLoopSetpoint, kLoopSteps);

    check(npd.u_ripple > 1.5 * np.u_ripple,
          "adding a derivative term makes the command ripple bigger");
    check(npid.u_ripple > npi.u_ripple,
          "PID ripples more than PI");
    check(npd.u_ripple > 3.0 * npi.u_ripple,
          "the two sets with a derivative term ripple several times more");
    check(np.y_ripple > 0.0 && np.y_ripple < 0.1,
          "with P alone the noise reaches the output but stays small");

    /* 阶段 3：定点与浮点并排 */
    ctl::PlantConfig stage3quiet;
    stage3quiet.noise_amp = 0.0;
    const ctl::PlantConfig stage3noisy;

    const ctl::LoopMetrics fl = ctl::run_loop(stage3quiet, kPidGains, kPidSetpoint, kLoopSteps);
    const ctl::LoopMetrics fx = ctl::run_loop_fixed(stage3quiet, kPidGains, kPidSetpoint, kLoopSteps);
    const ctl::LoopMetrics nl = ctl::run_loop(stage3noisy, kPidGains, kPidSetpoint, kLoopSteps);
    const ctl::LoopMetrics nx = ctl::run_loop_fixed(stage3noisy, kPidGains, kPidSetpoint, kLoopSteps);

    check(nearly(fx.final_error, fl.final_error, 1e-3),
          "Q16.16 reaches the same steady state as float");
    check(nearly(fx.overshoot, fl.overshoot, 0.05),
          "Q16.16 overshoots by the same amount as float");
    check(nearly(fx.settle_time, fl.settle_time, 0.02),
          "Q16.16 settles at the same time as float");
    check(nearly(fx.u_peak, fl.u_peak, 0.01),
          "Q16.16 uses the same peak command as float");
    check(nearly(nx.u_ripple, nl.u_ripple, 0.01),
          "Q16.16 ripples as much as float when the noise is on");
    check(fx.integ_clamps == 0,
          "the fixed point accumulator never hits its guard");

    /* 阶段 3：采样周期拉长 */
    ctl::PlantConfig fast = stage3quiet;
    fast.dt = 0.01;
    ctl::PlantConfig slow = stage3quiet;
    slow.dt = 0.2;
    const ctl::LoopMetrics mfast = ctl::run_loop(fast, kPidGains, kPidSetpoint, 1000);
    const ctl::LoopMetrics mslow = ctl::run_loop(slow, kPidGains, kPidSetpoint, 50);

    check(mfast.settle_time > 0.0, "at 100 Hz the loop settles");
    check(mslow.overshoot > mfast.overshoot,
          "a 20 times longer sampling period overshoots far more");
    check(mslow.settle_time < 0.0,
          "and at 5 Hz it never settles inside the band");

    /* 阶段 3：限幅与积分饱和 */
    const ctl::AwResult awl = ctl::run_aw(stage3quiet, kPidGains, kAwSetpoint, kAwSteps,
                                          ctl::AntiWindup::kLimitOnly, kTrackingTime);
    const ctl::AwResult awc = ctl::run_aw(stage3quiet, kPidGains, kAwSetpoint, kAwSteps,
                                          ctl::AntiWindup::kClamp, kTrackingTime);
    const ctl::AwResult awb = ctl::run_aw(stage3quiet, kPidGains, kAwSetpoint, kAwSteps,
                                          ctl::AntiWindup::kBackCalc, kTrackingTime);

    check(awl.overshoot > 10.0,
          "limiting the command alone still overshoots badly");
    check(awc.max_integ < 0.5 * awl.max_integ,
          "clamping keeps the integral several times smaller");
    check(awc.overshoot < 0.5 * awl.overshoot,
          "clamping cuts the overshoot");
    check(awb.overshoot < 0.5 * awl.overshoot,
          "back calculation cuts the overshoot too");
    check(awb.sat_steps < awl.sat_steps,
          "back calculation spends fewer steps against the limit");
    check(awc.settle_time > 0.0 && awc.settle_time < awl.settle_time,
          "clamping settles earlier than limiting alone");
    check(nearly(awc.final_error, 0.0, 5e-3) && nearly(awb.final_error, 0.0, 5e-3),
          "both anti-windup forms still reach the target");

    const ctl::LoadResult lrl = ctl::run_load_recovery(stage3quiet, kPidGains, kAwSetpoint,
                                                       kAwSteps, ctl::AntiWindup::kLimitOnly,
                                                       kTrackingTime, kLoadFrom, kLoadUntil, kLoad);
    const ctl::LoadResult lrc = ctl::run_load_recovery(stage3quiet, kPidGains, kAwSetpoint,
                                                       kAwSteps, ctl::AntiWindup::kClamp,
                                                       kTrackingTime, kLoadFrom, kLoadUntil, kLoad);
    const ctl::LoadResult lrb = ctl::run_load_recovery(stage3quiet, kPidGains, kAwSetpoint,
                                                       kAwSteps, ctl::AntiWindup::kBackCalc,
                                                       kTrackingTime, kLoadFrom, kLoadUntil, kLoad);

    check(lrc.recovery_time >= 0.0 && lrc.recovery_time < lrl.recovery_time,
          "after a load step, clamping recovers sooner than limiting alone");
    check(lrb.recovery_time > 0.0 && lrb.recovery_time < lrl.recovery_time,
          "back calculation recovers sooner as well");

    /* 阶段 4：滤波 */
    const double noise_std = kFilterNoise / std::sqrt(3.0);   /* 均匀分布的均方根 */

    const ctl::FilterMetrics f_none = ctl::run_filter(ctl::FilterKind::kNone, 0.0,
                                                      kFilterDt, kFilterSteps, kFilterStepAt);
    const ctl::FilterMetrics f_lp1 = ctl::run_filter(ctl::FilterKind::kLowPass, 0.01,
                                                     kFilterDt, kFilterSteps, kFilterStepAt);
    const ctl::FilterMetrics f_lp5 = ctl::run_filter(ctl::FilterKind::kLowPass, 0.05,
                                                     kFilterDt, kFilterSteps, kFilterStepAt);
    const ctl::FilterMetrics f_av16 = ctl::run_filter(ctl::FilterKind::kAverage, 16.0,
                                                      kFilterDt, kFilterSteps, kFilterStepAt);
    const ctl::FilterMetrics f_av128 = ctl::run_filter(ctl::FilterKind::kAverage, 128.0,
                                                       kFilterDt, kFilterSteps, kFilterStepAt);

    check(nearly(f_none.noise_rms, noise_std, 0.002),
          "with no filter the noise keeps its own rms of 0.029");
    check(f_none.lag == 0.0,
          "and there is no lag at all");

    /* 一阶低通把白噪声的方差压到 a / (2 - a) 倍，a 是 dt 与 tf 的比值 */
    check(nearly(f_lp1.noise_rms, noise_std * std::sqrt(0.1 / 1.9), 0.0015),
          "a 10 ms time constant cuts the noise to about a quarter");
    check(nearly(f_lp5.noise_rms, noise_std * std::sqrt(0.02 / 1.98), 0.0010),
          "a 50 ms time constant cuts it to about a tenth");
    check(f_lp5.lag > f_lp1.lag,
          "a longer time constant lags more");

    /* 滑动平均把白噪声的均方根压到 1 / sqrt(N) */
    check(nearly(f_av16.noise_rms, noise_std / 4.0, 0.0015),
          "a 16 sample average cuts the noise to about a quarter");
    check(nearly(f_av128.noise_rms, noise_std / std::sqrt(128.0), 0.0010),
          "a 128 sample average cuts it to about a ninth");
    check(nearly(f_av16.lag, 0.0075, 0.002),
          "the lag of a 16 sample average is about half a window");
    check(nearly(f_av128.lag, 0.0635, 0.002),
          "the lag of a 128 sample average is about half a window");
    check(f_lp1.noise_rms < 1.5 * f_av16.noise_rms && f_lp1.lag < f_av16.lag,
          "the two filters sit on the same noise against lag trade off");

    /* 阶段 4：测速 */
    const ctl::SpeedMetrics s_mlow = ctl::run_speed(ctl::SpeedMethod::kM, 0.5, kCpr,
                                                    kSpeedWindow, kClock, kSpeedWindows);
    const ctl::SpeedMetrics s_tlow = ctl::run_speed(ctl::SpeedMethod::kT, 0.5, kCpr,
                                                    kSpeedWindow, kClock, kSpeedWindows);
    const ctl::SpeedMetrics s_xlow = ctl::run_speed(ctl::SpeedMethod::kMT, 0.5, kCpr,
                                                    kSpeedWindow, kClock, kSpeedWindows);
    const ctl::SpeedMetrics s_mhigh = ctl::run_speed(ctl::SpeedMethod::kM, 50.0, kCpr,
                                                     kSpeedWindow, kClock, kSpeedWindows);
    const ctl::SpeedMetrics s_thigh = ctl::run_speed(ctl::SpeedMethod::kT, 50.0, kCpr,
                                                     kSpeedWindow, kClock, kSpeedWindows);
    const ctl::SpeedMetrics s_xhigh = ctl::run_speed(ctl::SpeedMethod::kMT, 50.0, kCpr,
                                                     kSpeedWindow, kClock, kSpeedWindows);

    check(s_mlow.max_error > 2.0,
          "M at 0.5 turns per second is off by a few percent in the worst window");
    check(s_mhigh.max_error < 0.05,
          "M at 50 turns per second is exact");
    check(s_tlow.max_error < 0.1,
          "T at 0.5 turns per second is almost exact");
    check(s_thigh.max_error > 1.0,
          "T at 50 turns per second is off by more than a percent");
    check(s_xlow.max_error < 0.05 && s_xhigh.max_error < 0.05,
          "M/T is accurate at both ends");
    check(nearly(s_mhigh.mean_estimate, 50.0, 0.01),
          "M reads 50 turns per second exactly");
    check(nearly(s_xlow.mean_estimate, 0.5, 0.001),
          "M/T reads 0.5 to within a thousandth");

    std::cout << "\n" << (g_pass + g_fail) << " checks, " << g_pass << " passed";
    if (g_fail == 0) {
        std::cout << ", all passed\n";
    } else {
        std::cout << ", " << g_fail << " failed\n";
    }
    return g_fail;
}

} /* namespace */

int main(int argc, char **argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) {
        return run_selftest();
    }

    stage1();
    stage2();
    stage3();
    stage4();
    return 0;
}
