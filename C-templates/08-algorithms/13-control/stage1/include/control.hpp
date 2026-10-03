/* control.hpp —— 练习模板 13 的核心接口（C++）—— 阶段 1
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
 * 控制这一件事拆成 5 个阶段，每个阶段的实现写在 src/control.cpp 里：
 *
 *     阶段 1  被控对象、开环误差、带滞回的开关控制
 *     阶段 2  PID 三项：P、PI、PD、PID
 *     阶段 3  离散化与定点：Q 格式的同一控制器，拉长采样周期
 *     阶段 4  限幅与积分饱和：钳位与反算两种抗饱和
 *     阶段 5  滤波、测速，以及不该用 PID 的时候
 *
 * 本模板所有的测量噪声都由一个固定种子的线性同余发生器产生，
 * 因此同一个可执行文件重跑多少次，输出都逐位相同——这是本模板
 * 的判据能成立的前提。
 */
#ifndef CONTROL_HPP
#define CONTROL_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace ctl {

/* ==================================================================
 * 已给出：固定种子的噪声源
 * ================================================================== */

/* 本模板唯一的种子。改这个数，全部判据数字都会变，因此不要改。 */
inline constexpr std::uint32_t kSeed = 20261013u;

/* 测量噪声源：一个线性同余发生器（LCG）。
 * 用它而不是 <random>，是因为它的输出逐位可复现：换编译器、
 * 换标准库、换平台拿到的都是同一串数。本模板所有「重跑逐位相同」
 * 的判据都建立在这一条上。 */
struct Noise {
    std::uint32_t state;

    explicit Noise(std::uint32_t seed) : state(seed) {}

    /* 下一个落在 [-1, 1) 上的数，每调用一次状态前进一步 */
    double next_unit();
};

/* ==================================================================
 * 阶段 1：被控对象
 * ================================================================== */

/* 被控对象的参数。下面的默认值就是本模板全部实测用的那一组。 */
struct PlantConfig {
    double dt = 0.01;        /* 采样周期，s */
    double tau = 0.5;        /* 一阶惯性的时间常数，s */
    double gain = 2.0;       /* 静态增益 K：输入 1 个单位，稳态输出 K */
    double u_max = 2.0;      /* 执行器的饱和边界：命令被夹到 [-u_max, u_max] */
    double noise_amp = 0.05; /* 测量噪声幅度：噪声落在 [-amp, amp) */
    double y0 = 0.0;         /* 初始输出 */
};

/* 被控对象：一阶惯性 + 饱和执行器 + 带噪声的测量。
 * y 是真值，measure() 给出测量值，step() 推进一个采样周期。 */
struct Plant {
    PlantConfig cfg;
    double y;
    Noise noise;

    explicit Plant(const PlantConfig &c)
        : cfg(c), y(c.y0), noise(Noise(kSeed)) {}

    /* 测量值 = 真值 + 噪声。控制器只准看这一个。 */
    double measure();

    /* 推进一个采样周期，返回新的真值。u 是控制器给出的命令。
     * TODO（阶段 1-1）在这个函数上，见 src/control.cpp。
     * 判据：《配置步骤.md》阶段 1 的前三张表。 */
    double step(double u);
};

/* 已给出：把命令夹到执行器的范围里 */
double clamp_command(double u, double u_max);

/* 已给出：开环跑 steps 拍，命令始终不变，返回末段的平均真值 */
double run_open_loop(const PlantConfig &cfg, double u, int steps);

/* ==================================================================
 * 阶段 1：带滞回的开关控制
 * ================================================================== */

/* 一遍开关控制跑完之后的几个数 */
struct OnOffResult {
    long long switches = 0;   /* 命令发生变化的次数 */
    long long on_steps = 0;   /* 命令为 u_max 的拍数 */
    double duty = 0.0;        /* on_steps / steps */
    double tail_ripple = 0.0; /* 后半段里 max |真值 - 目标| */
    double final_error = 0.0; /* 末段平均真值减去目标 */
};

/* 开关控制一步。给定测量值、目标、滞回宽度与上一拍是不是开着，
 * 决定这一拍给多少。命令只允许取两个值：0 与 u_max。
 * TODO（阶段 1-2）在这个函数上，见 src/control.cpp。
 * 判据：《配置步骤.md》阶段 1 的第四张表。 */
double on_off_command(double y_measured, double setpoint, double hysteresis,
                      bool was_on, double u_max);

/* 已给出：跑一遍开关控制，数出切换次数与死区宽度 */
OnOffResult run_on_off(const PlantConfig &cfg, double setpoint,
                       double hysteresis, int steps);

} /* namespace ctl */

#endif /* CONTROL_HPP */
