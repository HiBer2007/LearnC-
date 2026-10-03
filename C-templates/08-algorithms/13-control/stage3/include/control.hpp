/* control.hpp —— 练习模板 13 的核心接口（C++）—— 阶段 3
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

/* ==================================================================
 * 阶段 2：PID 三项
 * ================================================================== */

/* 三项各自的增益。把某一项置 0 就退化成两项或一项：
 *   ki = 0、kd = 0  就是 P；kd = 0 就是 PI；ki = 0 就是 PD。 */
struct PidGains {
    double kp = 0.0;
    double ki = 0.0;
    double kd = 0.0;
};

/* 控制器两次调用之间要留下的东西 */
struct PidState {
    double integ = 0.0;     /* 积分累加器 */
    double prev_meas = 0.0; /* 上一拍的测量值 */
    bool has_prev = false;  /* 上一拍存在吗 */
};

/* 一次闭环跑完之后的几个数。全部由已给出的 summarize() 算出。 */
struct LoopMetrics {
    double final_error = 0.0;  /* 末段平均真值减去目标 */
    double overshoot = 0.0;    /* (全程最大真值 - 目标) / 目标 * 100，没超就是 0 */
    double settle_time = -1.0; /* 进入正负 2% 带并不再出来的时刻，s；没进去就是 -1 */
    double u_ripple = 0.0;     /* 后半段相邻两拍 |命令之差| 的平均值 */
    double y_ripple = 0.0;     /* 后半段相邻两拍 |真值之差| 的平均值 */
    double u_peak = 0.0;       /* 全程 max |命令|，未经执行器削的那一个 */
    long long sat_steps = 0;   /* 命令顶到执行器边界的拍数 */
    long long integ_clamps = 0;/* 定点版积分器贴边的次数；浮点版恒为 0 */
};

/* PID 一拍。给定增益、状态、目标与测量值，返回这一拍的命令。
 * TODO（阶段 2-1）在这个函数上，见 src/control.cpp。
 * 判据：《配置步骤.md》阶段 2 的两张表。 */
double pid_step(const PidGains &g, PidState &s, double setpoint,
                double y_measured, double dt);

/* 已给出：闭环跑一遍，把每一拍的命令与真值记下来 */
LoopMetrics run_loop(const PlantConfig &cfg, const PidGains &g,
                     double setpoint, int steps);

/* 已给出：把记下来的两条序列折算成上面那几个数 */
LoopMetrics summarize(const std::vector<double> &cmd, const std::vector<double> &out,
                      double setpoint, double dt, double u_max);

/* ==================================================================
 * 阶段 3：离散化与定点
 * ================================================================== */

/* Q16.16 定点数：低 16 位是小数。1.0 就是 65536，
 * 能表示的范围是 -32768.0 到 32767.99998，分辨率是 1 / 65536。 */
using q_t = std::int32_t;

inline constexpr int kQFrac = 16;
inline constexpr q_t kQOne = 1 << kQFrac;

/* 积分累加器留出的余量：1 << 26 就是 1024.0。
 * 离 int32 的边界很远，又比任何一组正常增益需要的积分量大得多。 */
inline constexpr q_t kQIntegLimit = 1 << 26;

/* 已给出：换算、乘除与饱和。乘法的中间结果是 Q32.32，
 * 必须用 64 位放宽再移回来；除法同理，先把被除数抬高 16 位。
 * 除以 0 返回 0，不抛异常，也不产生未定义行为。 */
q_t q_from_double(double v);
double q_to_double(q_t v);
q_t q_mul(q_t a, q_t b);
q_t q_div(q_t a, q_t b);
q_t q_sat(std::int64_t v);
q_t q_clamp(q_t v, q_t lo, q_t hi);

/* 定点版的增益，也是 Q16.16 */
struct PidGainsQ {
    q_t kp = 0;
    q_t ki = 0;
    q_t kd = 0;
};

/* 定点版的控制器状态 */
struct PidStateQ {
    q_t integ = 0;              /* 积分累加器，Q16.16 */
    q_t prev_meas = 0;          /* 上一拍的测量值，Q16.16 */
    bool has_prev = false;
    long long integ_clamped = 0; /* 累加器贴到 kQIntegLimit 的次数 */
};

/* 定点版 PID 一拍。目标、测量值与采样周期都是 Q16.16，返回值也是，
 * 与阶段 2 的 pid_step 一样**不夹输出**——对象的执行器自己会削那一刀。
 * 三项相加若越过 Q16.16 的范围，用 q_sat 收边（饱和，不是回绕）。
 * TODO（阶段 3-1）在这个函数上，见 src/control.cpp。
 * 判据：《配置步骤.md》阶段 3 的第一张表。 */
q_t q_pid_step(const PidGainsQ &g, PidStateQ &s, q_t setpoint_q, q_t y_measured_q,
               q_t dt_q);

/* 已给出：用定点控制器跑一遍闭环。命令与真值都换算回 double 再统计，
 * 这样它与 run_loop 的数可以直接并排看。 */
LoopMetrics run_loop_fixed(const PlantConfig &cfg, const PidGains &g,
                           double setpoint, int steps);

/* ==================================================================
 * 阶段 3：限幅与积分饱和
 * ================================================================== */

/* 命令顶到执行器边界时，积分器怎么办 */
enum class AntiWindup {
    kLimitOnly,  /* 只把输出夹住，积分照常累加——不加抗饱和的对照 */
    kClamp,      /* 顶到边界而且误差还在往同一边推时，这一拍不累加 */
    kBackCalc    /* 用夹住前后的差算一个修正量，加进积分器的累加里 */
};

/* 一遍带抗饱和的闭环跑完之后的几个数 */
struct AwResult {
    double overshoot = 0.0;    /* 阶跃带来的超调，% */
    double settle_time = -1.0; /* 进入正负 2% 带并不再出来的时刻，s；没进去就是 -1 */
    double max_integ = 0.0;    /* 全程 max |积分器| */
    long long sat_steps = 0;   /* 命令顶到边界的拍数 */
    double final_error = 0.0;  /* 末段平均真值减去目标 */
};

/* 带限幅与抗饱和的一拍。u_max 是执行器的边界，tt 是反算用的跟踪时间常数
 * （只有 kBackCalc 用得上）。
 * TODO（阶段 3-2 与阶段 3-3）在这个函数上：kClamp 与 kBackCalc 两个分支。
 * 判据：《配置步骤.md》阶段 3 的第二、三张表。 */
double pid_step_aw(const PidGains &g, PidState &s, double setpoint, double y_measured,
                   double dt, double u_max, AntiWindup mode, double tt);

/* 已给出：跑一遍阶跃，把超调、调节时间、积分器的峰值与饱和拍数都记下来 */
AwResult run_aw(const PlantConfig &cfg, const PidGains &g, double setpoint, int steps,
                AntiWindup mode, double tt);

/* 加了一次负载扰动之后的两个数 */
struct LoadResult {
    double recovery_time = -1.0; /* 从扰动加上那一刻算起的恢复时间，s；没回来就是 -1 */
    double final_error = 0.0;    /* 末段平均真值减去目标 */
};

/* 已给出：跑一遍阶跃，中途加一段负载扰动，量误差回到带内要多久。
 * 扰动直接加在命令上（输入扰动），从 load_from 那一拍加到 load_until 之前，
 * 对象自己那一刀仍然管着范围。恢复时间从扰动撤掉那一刻算起。 */
LoadResult run_load_recovery(const PlantConfig &cfg, const PidGains &g, double setpoint,
                             int steps, AntiWindup mode, double tt,
                             int load_from, int load_until, double load);

} /* namespace ctl */

#endif /* CONTROL_HPP */
