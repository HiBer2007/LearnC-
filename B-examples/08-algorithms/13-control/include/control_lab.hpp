/**
 * control_lab.hpp —— 控制：让一个量停在目标上
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
 */

/**
 * 报告里的每个数字都由这里的仿真与计数器产出，固定参数重跑逐位相同：
 *
 *   轨迹       每个采样点上的参考、输出、控制量、误差与积分器状态
 *   超调       输出峰值减去目标值，单位与输出相同
 *   稳态误差   末尾一个窗口内输出均值与目标之差
 *   控制量总变化  Σ|u[k] − u[k−1]|，量测噪声经过微分项之后会把它顶上去
 *   切换次数   开关控制里加热器状态翻转的次数
 *   量化误差   测速三种方法在闸门相位上扫出来的最坏相对误差，单位 ppm
 *
 * 被控对象只有一台：位置伺服（转动惯量 J、粘性摩擦 b、负载力矩 load）。
 * 选它是因为它同时装得下这一章要讲的几件事——P 留下与负载成正比的稳态误差、
 * 积分项把误差消掉却带来超调、微分项加阻尼从而压住超调、执行器一限幅就出现饱和。
 * 所有仿真都用显式欧拉按采样周期推进一步，采样周期取得比对象时间常数小得多。
 */
#ifndef CONTROL_LAB_HPP
#define CONTROL_LAB_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ctrl {

/* ================= 被控对象与控制器 ================= */

/** 位置伺服：J·θ'' = gain·u − b·θ' − load，u 被限幅在 [u_min, u_max] 内。
    平衡点很有意思：静止时 θ' = 0，于是只需 u = load / gain 就能停在任意位置——
    停在哪儿由历史决定，这正是开环控制守不住位置的原因。 */
struct PlantParams {
    double inertia = 0.02;    /**< J，转动惯量 */
    double friction = 0.1;    /**< b，粘性摩擦系数 */
    double gain = 1.0;        /**< 力矩增益 */
    double load = 0.0;        /**< 常值负载力矩 */
    double u_min = 0.0;       /**< 执行器下限 */
    double u_max = 4.0;       /**< 执行器上限 */
};

/** 并联式 PID 的三个增益 */
struct PidGains {
    double kp = 0.0;
    double ki = 0.0;
    double kd = 0.0;
};

/** 积分饱和的三种处理 */
enum class AntiWindup {
    None,      /**< 不做处理：限幅只砍输出，积分器照涨 */
    Clamp,     /**< 钳位：误差把输出往饱和方向推时，这一步不积分 */
    BackCalc   /**< 反算：把「限幅前后之差」乘一个系数退回积分器 */
};

const char *anti_windup_name(AntiWindup aw);

/** 常规并联式 PID：
 *
 *     e     = r − y_measure
 *     I    += ki · e · Ts
 *     u_raw = kp · e + I − kd · d(y_measure)/dt + u_ff
 *     u     = clamp(u_raw, u_min, u_max)
 *
 * 微分作用在量测上（对 y 求导再取负），不是作用在误差上：目标值一跳变，
 * 对误差求导会得到一个冲激，执行器会挨一记「微分冲击」。
 * derivative_alpha 是微分项的一阶低通系数，0 表示不过滤，见第 13.6 节。
 */
class PidController {
public:
    PidController(const PidGains &gains, double ts, double u_min, double u_max,
                  AntiWindup aw, double kaw, double derivative_alpha);

    /** 推进一步，返回本步的控制量。feedforward 会与 PID 输出相加之后再限幅，
        抗饱和看到的也是相加之后的结果 */
    double update(double reference, double measurement, double feedforward = 0.0);

    double integral() const { return integral_; }

private:
    PidGains gains_;
    double ts_ = 0.001;
    double u_min_ = 0.0;
    double u_max_ = 1.0;
    double kaw_ = 0.0;
    double derivative_alpha_ = 0.0;
    AntiWindup aw_ = AntiWindup::None;

    double integral_ = 0.0;
    double derivative_ = 0.0;
    double y_prev_ = 0.0;
    bool has_prev_ = false;
};

/* ================= 仿真 ================= */

/** 一个采样点。误差按真实输出算，不按量测算——量测里可能加了噪声 */
struct Sample {
    double t = 0.0;         /**< 时间，秒 */
    double r = 0.0;         /**< 参考值 */
    double y = 0.0;         /**< 真实输出 */
    double u = 0.0;         /**< 控制量（限幅之后） */
    double e = 0.0;         /**< 参考减输出 */
    double integral = 0.0;  /**< 积分器状态 */
};

struct SimResult {
    double ts = 0.001;
    std::vector<Sample> samples;

    std::size_t size() const { return samples.size(); }
};

using TimeFunction = std::function<double(double)>;

/** 一次仿真的全部输入 */
struct Scenario {
    PlantParams plant;
    PidGains gains;
    double ts = 0.002;                       /**< 采样周期，秒 */
    std::size_t steps = 1000;                /**< 步数 */
    double y0 = 0.0;                         /**< 初始位置 */
    double velocity0 = 0.0;                  /**< 初始速度 */
    AntiWindup aw = AntiWindup::None;
    double kaw = 5.0;                        /**< 反算系数，单位 1/s */
    double derivative_alpha = 0.0;           /**< 微分项低通系数 */
    const std::vector<double> *noise = nullptr;  /**< 量测噪声，按样本下标取 */
    TimeFunction reference;                  /**< 参考值，默认恒为 1 */
    TimeFunction load;                       /**< 负载，默认恒为 plant.load */
    TimeFunction feedforward;                /**< 前馈力矩，默认 0 */
};

SimResult simulate(const Scenario &scenario);

/* ================= 轨迹上的结构量 ================= */

struct TraceStats {
    std::size_t samples = 0;
    double peak = 0.0;                  /**< 输出最大值 */
    double overshoot = 0.0;             /**< 峰值减目标，负的截成 0 */
    double final_mean = 0.0;            /**< 末尾窗口内输出的均值 */
    double final_error = 0.0;           /**< 目标减 final_mean */
    double final_max_abs_error = 0.0;   /**< 末尾窗口内 |误差| 的最大值 */
    double max_abs_error = 0.0;         /**< 全程 |误差| 的最大值 */
    double iae = 0.0;                   /**< ∫|e|dt，秒 */
    double control_tv = 0.0;            /**< Σ|Δu| */
    std::size_t rise_index = 0;         /**< 首次达到目标的 90% 的下标 */
    std::size_t settle_index = 0;       /**< 首次进入 ±2% 且此后不再出去的下标 */
    bool settled = false;
    double max_abs_integral = 0.0;      /**< 积分器绝对值的最大者 */
    double ripple = 0.0;                /**< 末尾窗口内输出的极差 */
    double mean = 0.0;                  /**< 全程输出均值 */
};

/** 轨迹上的结构量。window 是末尾取多少个样本算稳态量（0 表示全程） */
TraceStats analyze(const SimResult &run, double target, std::size_t window);

/** 截取一段轨迹，下标是 [from, from + count) */
SimResult window_of(const SimResult &run, std::size_t from, std::size_t count);

/* ================= 一、开环与闭环 ================= */

struct OpenClosedRow {
    std::string name;
    double open_error;          /**< 窗口末尾的误差 */
    double open_max_error;      /**< 窗口内 |误差| 的最大值 */
    double open_iae;
    double closed_error;
    double closed_max_error;
    double closed_iae;
    bool closed_recovered;      /**< 闭环是否回到 ±2% */
    std::size_t closed_recover_samples;
};

struct OpenClosedReport {
    double ts = 0.002;
    double window_seconds = 2.0;
    PidGains gains;
    PlantParams plant;
    std::vector<OpenClosedRow> rows;
};

OpenClosedReport probe_open_vs_closed();

/* ================= 二、开关控制与滞回 ================= */

struct SwitchRow {
    double band = 0.0;            /**< 滞回带宽度，摄氏度 */
    std::uint64_t switches = 0;   /**< 加热器状态翻转次数 */
    double ripple = 0.0;          /**< 后半程温度极差 */
    double mean = 0.0;            /**< 后半程温度均值 */
};

struct SwitchReport {
    double ts = 0.1;
    std::size_t samples = 0;
    double setpoint = 60.0;
    std::vector<SwitchRow> rows;
};

SwitchReport probe_hysteresis();

/* ================= 三、PID 的三项 ================= */

struct PidTermRow {
    std::string name;
    double step_overshoot = 0.0;      /**< 阶跃段（负载加入之前）的超调 */
    double recover_overshoot = 0.0;   /**< 负载加入之后的恢复超调 */
    double final_error = 0.0;         /**< 末尾稳态误差 */
    double control_tv = 0.0;          /**< 控制量总变化 */
    double settle_seconds = 0.0;      /**< 恢复时间 */
    bool settled = false;
};

struct PidTermsReport {
    double ts = 0.002;
    double seconds = 0.0;            /**< 每一条跑多久，秒 */
    double load_time = 2.0;          /**< 负载加入的时刻，秒 */
    double load_value = 0.2;
    double u_max = 0.0;
    std::vector<PidTermRow> clean;   /**< 量测干净 */
    std::vector<PidTermRow> noisy;   /**< 量测带噪声 */
    double noise_amplitude = 0.0;
};

PidTermsReport probe_pid_terms();

/* ================= 四、离散化与定点 ================= */

struct TsRow {
    double ts = 0.0;
    double overshoot = 0.0;
    double settle_seconds = 0.0;   /**< 未收敛时是 0，用 settled 区分 */
    bool settled = false;
    bool diverged = false;         /**< 输出冲到天上去了，超调这一列改成打印「发散」 */
    double travel_per_sample = 0.0;
};

struct QuantRow {
    std::string name;
    double final_error = 0.0;          /**< 末尾窗口内误差的均值 */
    double final_max_abs_error = 0.0;  /**< 末尾窗口内 |误差| 的最大值 */
};

struct DiscretizationReport {
    std::vector<TsRow> ts_rows;
    std::vector<QuantRow> quant_rows;
    double lsb = 0.0;              /**< 传感器与执行器的一个最低位，位置单位 */
    double frac_lsb = 0.0;         /**< 定点实现里积分器的一个最低位 */
    int adc_bits = 12;
    long long kp_q = 0;            /**< 定点里 kp 的 Q16 取值 */
    long long ki_q = 0;            /**< 定点里 ki·Ts 的 Q16 取值 */
    long long kd_q = 0;            /**< 定点里 kd/Ts 的 Q16 取值 */
};

DiscretizationReport probe_discretization();

/* ================= 五、限幅与积分饱和 ================= */

struct AntiWindupRow {
    std::string name;
    double overshoot = 0.0;
    double settle_seconds = 0.0;
    bool settled = false;
    std::uint64_t saturated_samples = 0;   /**< 输出贴着上限的样本数 */
    double max_integral = 0.0;             /**< 饱和期间积分器的最大者 */
    double min_u = 0.0;                    /**< 全程控制量的最小者 */
    double max_u = 0.0;                    /**< 全程控制量的最大者 */
};

struct AntiWindupReport {
    double ts = 0.002;
    double u_max = 0.2;
    double kaw = 5.0;
    std::vector<AntiWindupRow> rows;
};

AntiWindupReport probe_anti_windup();

/* ================= 六、滤波 ================= */

struct FilterRow {
    std::string name;
    double noise_std = 0.0;        /**< 输出端噪声标准差 */
    double suppression = 0.0;      /**< 输入标准差除以输出标准差 */
    double theory_std = 0.0;       /**< 理论标准差 */
    double lag_samples = 0.0;      /**< 阶跃响应到 50% 的样本数（可为小数：线性插值） */
};

struct FilterReport {
    double ts = 0.001;
    double input_std = 0.0;
    std::vector<FilterRow> rows;
};

FilterReport probe_filters();

/* ================= 七、测速 ================= */

struct SpeedRow {
    double rpm = 0.0;
    double count_period_ticks = 0.0;   /**< 一个计数周期的时钟节拍数（精确值） */
    double counts_per_gate = 0.0;      /**< 闸门时间里平均有几个计数 */
    double m_worst_ppm = 0.0;          /**< M 法扫相位的最坏相对误差 */
    double t_worst_ppm = 0.0;          /**< T 法的最坏相对误差 */
    double mt_worst_ppm = 0.0;         /**< M/T 法扫相位的最坏相对误差 */
    std::uint64_t m_zero_phases = 0;   /**< M 法测出 0 的相位数 */
    std::uint64_t phases = 0;
};

struct SpeedReport {
    double clock_hz = 10e6;
    double gate_seconds = 0.01;
    double counts_per_rev = 4096.0;
    std::vector<SpeedRow> rows;
};

SpeedReport probe_speed_methods();

/* ================= 八、前馈加反馈 ================= */

struct FeedRow {
    std::string name;
    double max_error = 0.0;
    double rms_error = 0.0;
    double cruise_error = 0.0;    /**< 匀速段平均误差 */
    double final_error = 0.0;     /**< 停下之后再等一段的误差 */
};

struct FeedforwardReport {
    double ts = 0.002;
    double cruise_velocity = 1.0;
    double accel = 2.0;
    double displacement = 0.0;     /**< 梯形规划的总位移，解析值是 1.000 */
    double profile_seconds = 0.0;
    std::vector<FeedRow> rows;
};

FeedforwardReport probe_feedforward();

/* ================= 噪声与文本工具 ================= */

/** 固定种子的确定性噪声：[−amplitude, amplitude) 上的均匀分布。
    取高 24 位再除以 2^24，全程整数与 2 的幂，不依赖标准库的分布实现 */
std::vector<double> make_noise(std::size_t count, std::uint32_t seed, double amplitude);

/** 定小数位的文本。绝对值小于半个最低位时输出 0，避免出现 "-0.000" */
std::string format_fixed(double value, int decimals);

/** 比值转百分数，例如 0.163 输出 "16.3%" */
std::string format_percent(double ratio, int decimals);

/* ================= 报告与自测 ================= */

struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;   /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;
};

/** 项目输出：八段，对应这一章的八节。返回多行 UTF-8 文本，末尾带一个换行 */
std::string build_report();

/** 逐项核对被控对象、三段参数各自的账、量化误差与可复现性 */
CheckResult run_self_tests();

}   /* namespace ctrl */

#endif /* CONTROL_LAB_HPP */
