/* main_cli.cpp —— 练习模板 13 的命令行验收程序（C++）—— 阶段 1
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
 *     app_cli.exe              打印本阶段的测量报告，总是返回 0
 *     app_cli.exe --selftest   跑判据，返回失败的项数（全过就是 0）
 *
 * 这个文件**不需要改**：它只调用 include/control.hpp 里的接口。
 * 你的实现写在 src/control.cpp 里。骨架状态下报告能打完、返回 0，
 * 但 --selftest 会报出一批 [FAIL]——那正是还没做完的部分。
 *
 * 构建与运行（在 13-control 目录下）：
 *     cmake -S stage1 -B %TEMP%\ctl-st1 -G Ninja -DCMAKE_CXX_COMPILER=g++
 *     cmake --build %TEMP%\ctl-st1
 *     %TEMP%\ctl-st1\bin\app_cli.exe
 *     %TEMP%\ctl-st1\bin\app_cli.exe --selftest
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
    return 0;
}
