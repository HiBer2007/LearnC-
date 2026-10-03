/**
 * control_lab.cpp —— 控制：让一个量停在目标上
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

#include "control_lab.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace ctrl {

namespace {

/* ================= 全篇共用的一组参数 ================= */

constexpr double kInertia = 0.02;     /**< J */
constexpr double kFriction = 0.1;     /**< b */
constexpr double kUmax = 4.0;         /**< 常规实验的执行器上限 */
constexpr double kTs = 0.002;         /**< 常规实验的采样周期，秒 */

const PidGains kGainsP{0.5, 0.0, 0.0};
const PidGains kGainsPi{0.5, 0.5, 0.0};
const PidGains kGainsPid{0.5, 0.5, 0.05};

PlantParams plant_of(double u_max)
{
    PlantParams p;
    p.inertia = kInertia;
    p.friction = kFriction;
    p.gain = 1.0;
    p.load = 0.0;
    /* 力矩可正可负：惯性要被刹住就得反向出力。
       只给单向限幅的话，输出冲过目标之后只能靠摩擦慢慢停下来，
       超调会凭空多出两成，那不是控制器的毛病，是执行器的毛病。 */
    p.u_min = -u_max;
    p.u_max = u_max;
    return p;
}

TimeFunction step_reference(double target)
{
    return [target](double) { return target; };
}

/* ================= 文本工具 ================= */

/** UTF-8 文本占几个字符宽度：汉字与全角符号算两列，其余算一列。
    表格靠它对齐——按字节数补空格的话，中文列会歪。 */
std::size_t display_width(const std::string &text)
{
    std::size_t width = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            width += 1;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            width += 1;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            width += 2;
            i += 3;
        } else {
            width += 2;
            i += 4;
        }
    }
    return width;
}

std::string pad_left(const std::string &text, std::size_t width)
{
    const std::size_t w = display_width(text);
    return w >= width ? text : std::string(width - w, ' ') + text;
}

std::string pad_right(const std::string &text, std::size_t width)
{
    const std::size_t w = display_width(text);
    return w >= width ? text : text + std::string(width - w, ' ');
}

/** 一行表格：标签左对齐，数值各占一列、右对齐、列前留两个空格 */
std::string table_row(const std::string &label, std::size_t label_width,
                      const std::vector<std::string> &values, std::size_t column_width)
{
    std::string line = pad_right(label, label_width);
    for (const std::string &value : values) {
        line += "  ";
        line += pad_left(value, column_width);
    }
    return line;
}

std::string integer_text(long long value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

/** 位置量一律打三位小数：表格里的列才对得齐，也避免浮点尾巴露出来 */
std::string milli_text(double value)
{
    return format_fixed(value, 3);
}

std::string seconds_text(double value)
{
    return milli_text(value) + u8" s";
}

}   /* namespace */

/* ================= 控制器 ================= */

const char *anti_windup_name(AntiWindup aw)
{
    switch (aw) {
    case AntiWindup::None:     return u8"不做处理";
    case AntiWindup::Clamp:    return u8"钳位";
    case AntiWindup::BackCalc: return u8"反算";
    }
    return u8"未知";
}

PidController::PidController(const PidGains &gains, double ts, double u_min, double u_max,
                             AntiWindup aw, double kaw, double derivative_alpha)
    : gains_(gains), ts_(ts), u_min_(u_min), u_max_(u_max), kaw_(kaw),
      derivative_alpha_(derivative_alpha), aw_(aw)
{
}

double PidController::update(double reference, double measurement, double feedforward)
{
    const double error = reference - measurement;

    double d_measure = 0.0;
    if (has_prev_) {
        const double raw = (measurement - y_prev_) / ts_;
        d_measure = derivative_alpha_ > 0.0
                        ? derivative_ + derivative_alpha_ * (raw - derivative_)
                        : raw;
        derivative_ = d_measure;
    }
    y_prev_ = measurement;
    has_prev_ = true;

    const double increment = gains_.ki * error * ts_;
    integral_ += increment;
    const double raw = gains_.kp * error + integral_ - gains_.kd * d_measure + feedforward;
    const double clamped = std::min(std::max(raw, u_min_), u_max_);

    switch (aw_) {
    case AntiWindup::None:
        break;
    case AntiWindup::Clamp:
        /* 误差把输出往饱和方向推，这一步就不积分；反方向时不冻结，
           否则积分器会卡在饱和值上下不来 */
        if ((raw > u_max_ && error > 0.0) || (raw < u_min_ && error < 0.0)) {
            integral_ -= increment;
        }
        break;
    case AntiWindup::BackCalc:
        integral_ += kaw_ * (clamped - raw) * ts_;
        break;
    }

    return clamped;
}

/* ================= 仿真 ================= */

SimResult simulate(const Scenario &scenario)
{
    PidController pid(scenario.gains, scenario.ts, scenario.plant.u_min, scenario.plant.u_max,
                      scenario.aw, scenario.kaw, scenario.derivative_alpha);

    SimResult out;
    out.ts = scenario.ts;
    out.samples.reserve(scenario.steps);

    double y = scenario.y0;
    double v = scenario.velocity0;

    for (std::size_t k = 0; k < scenario.steps; ++k) {
        const double t = static_cast<double>(k) * scenario.ts;
        const double r = scenario.reference ? scenario.reference(t) : 1.0;
        const double load = scenario.load ? scenario.load(t) : scenario.plant.load;
        const double ff = scenario.feedforward ? scenario.feedforward(t) : 0.0;
        const double measured = (scenario.noise != nullptr && k < scenario.noise->size())
                                    ? y + (*scenario.noise)[k]
                                    : y;

        const double u = pid.update(r, measured, ff);

        Sample sample;
        sample.t = t;
        sample.r = r;
        sample.y = y;
        sample.u = u;
        sample.e = r - y;
        sample.integral = pid.integral();
        out.samples.push_back(sample);

        /* 对象：J·dv/dt = gain·u − b·v − load。
           位置按匀加速的精确式子走（v·Ts + a·Ts²/2），速度走显式欧拉。
           位置那一步只写 v·Ts 就是一阶格式：本示例 2 ms 的周期下两者差不到 1%，
           但周期一粗（第四段最后几档）位置这一步的数值误差会盖过控制器本身的差别。 */
        const double accel =
            (scenario.plant.gain * u - scenario.plant.friction * v - load) / scenario.plant.inertia;
        y += v * scenario.ts + 0.5 * accel * scenario.ts * scenario.ts;
        v += accel * scenario.ts;
    }

    return out;
}

/* ================= 轨迹上的结构量 ================= */

TraceStats analyze(const SimResult &run, double target, std::size_t window)
{
    TraceStats stats;
    stats.samples = run.size();
    if (run.size() == 0) {
        return stats;
    }

    std::size_t win = window;
    if (win == 0 || win > run.size()) {
        win = run.size();
    }
    const double tolerance = std::fabs(target) * 0.02;

    stats.peak = run.samples[0].y;
    stats.rise_index = run.size();
    bool has_bad = false;
    std::size_t last_bad = 0;

    for (std::size_t i = 0; i < run.size(); ++i) {
        const Sample &s = run.samples[i];
        stats.peak = std::max(stats.peak, s.y);
        stats.mean += s.y;
        stats.max_abs_error = std::max(stats.max_abs_error, std::fabs(s.e));
        stats.iae += std::fabs(s.e) * run.ts;
        stats.max_abs_integral = std::max(stats.max_abs_integral, std::fabs(s.integral));
        if (i > 0) {
            stats.control_tv += std::fabs(s.u - run.samples[i - 1].u);
        }
        if (stats.rise_index == run.size() && s.y >= 0.9 * target) {
            stats.rise_index = i;
        }
        if (std::fabs(s.e) > tolerance) {
            has_bad = true;
            last_bad = i;
        }
    }
    stats.mean /= static_cast<double>(run.size());
    stats.overshoot = std::max(0.0, stats.peak - target);
    /* 「稳定」指末尾一个窗口内不再越界；settle_index 是最后一次越界的下一个样本，
       也就是从这一刻起一直待在 ±2% 里。只看有没有越界过是不够的——
       起步时误差必然越界，那样永远都判成不稳定。 */
    stats.settle_index = has_bad ? last_bad + 1 : 0;
    stats.settled = !has_bad || (last_bad + win < run.size());

    const std::size_t start = run.size() - win;
    double low = run.samples[start].y;
    double high = low;
    double sum = 0.0;
    double worst = 0.0;
    for (std::size_t i = start; i < run.size(); ++i) {
        low = std::min(low, run.samples[i].y);
        high = std::max(high, run.samples[i].y);
        sum += run.samples[i].y;
        worst = std::max(worst, std::fabs(run.samples[i].e));
    }
    stats.final_mean = sum / static_cast<double>(win);
    stats.final_error = target - stats.final_mean;
    stats.final_max_abs_error = worst;
    stats.ripple = high - low;
    return stats;
}

SimResult window_of(const SimResult &run, std::size_t from, std::size_t count)
{
    SimResult out;
    out.ts = run.ts;
    if (from >= run.size() || count == 0) {
        return out;
    }
    const std::size_t end = std::min(run.size(), from + count);
    out.samples.assign(run.samples.begin() + static_cast<std::ptrdiff_t>(from),
                       run.samples.begin() + static_cast<std::ptrdiff_t>(end));
    return out;
}

/* ================= 噪声与文本 ================= */

std::vector<double> make_noise(std::size_t count, std::uint32_t seed, double amplitude)
{
    std::vector<double> noise(count);
    std::uint32_t state = seed;
    const double unit = static_cast<double>(1u << 24);
    for (std::size_t i = 0; i < count; ++i) {
        state = state * 1664525u + 1013904223u;      /* 线性同余，常数取自 Numerical Recipes */
        const std::uint32_t top = state >> 8;        /* 取高 24 位，低位质量差 */
        noise[i] = (static_cast<double>(top) / unit * 2.0 - 1.0) * amplitude;
    }
    return noise;
}

std::string format_fixed(double value, int decimals)
{
    double scale = 1.0;
    for (int i = 0; i < decimals; ++i) {
        scale *= 10.0;
    }
    if (std::fabs(value) < 0.5 / scale) {
        value = 0.0;                                  /* 避免打印出 "-0.000" */
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(decimals) << value;
    return out.str();
}

std::string format_percent(double ratio, int decimals)
{
    return format_fixed(ratio * 100.0, decimals) + "%";
}

std::string CheckResult::summary() const
{
    std::string text = integer_text(static_cast<long long>(total)) + u8" 项中 " +
                       integer_text(static_cast<long long>(passed)) + u8" 项通过";
    text += failed == 0 ? u8"，全部通过"
                        : u8"，" + integer_text(static_cast<long long>(failed)) + u8" 项未通过";
    return text;
}

/* ================= 一、开环与闭环 ================= */

OpenClosedReport probe_open_vs_closed()
{
    OpenClosedReport report;
    report.ts = kTs;
    report.window_seconds = 5.0;
    report.gains = kGainsPi;
    report.plant = plant_of(kUmax);

    const std::size_t steps = static_cast<std::size_t>(report.window_seconds / report.ts);
    const std::size_t half = static_cast<std::size_t>(1.0 / report.ts);   /* 扰动在 1.000 s */
    const std::size_t tail = 100;      /* 末尾 0.2 s 的均值当作「末值」 */

    struct Case {
        const char *name;
        double open_torque;    /**< 开环给的常值力矩 */
        double load_before;
        double load_after;
        std::size_t from;      /**< 统计窗口的起点 */
    };
    const Case cases[2] = {
        {u8"负载阶跃 0.200 → 0.400", 0.2, 0.2, 0.4, half},
        {u8"标定偏差 25%（按 0.250 标定）", 0.25, 0.2, 0.2, 0},
    };

    for (const Case &item : cases) {
        Scenario scenario;
        scenario.plant = plant_of(kUmax);
        scenario.ts = report.ts;
        scenario.steps = steps;
        scenario.y0 = 1.0;
        scenario.aw = AntiWindup::None;
        scenario.reference = step_reference(1.0);
        scenario.load = [&item](double t) {
            return t >= 1.0 ? item.load_after : item.load_before;
        };

        /* 开环：力矩固定，控制器三个增益全零，力矩全靠前馈给 */
        scenario.gains = PidGains{0.0, 0.0, 0.0};
        scenario.feedforward = [&item](double) { return item.open_torque; };
        const SimResult open_run = simulate(scenario);
        const TraceStats open_stats =
            analyze(window_of(open_run, item.from, steps - item.from), 1.0, tail);

        /* 闭环：同一份扰动，PI 只看得见量测 */
        scenario.gains = report.gains;
        scenario.feedforward = TimeFunction();
        const SimResult closed_run = simulate(scenario);
        const TraceStats closed_stats =
            analyze(window_of(closed_run, item.from, steps - item.from), 1.0, tail);

        OpenClosedRow row;
        row.name = item.name;
        row.open_error = open_stats.final_error;
        row.open_max_error = open_stats.max_abs_error;
        row.open_iae = open_stats.iae;
        row.closed_error = closed_stats.final_error;
        row.closed_max_error = closed_stats.max_abs_error;
        row.closed_iae = closed_stats.iae;
        row.closed_recovered = closed_stats.settled;
        row.closed_recover_samples = closed_stats.settled ? closed_stats.settle_index : 0;
        report.rows.push_back(row);
    }

    return report;
}

/* ================= 二、开关控制与滞回 ================= */

SwitchReport probe_hysteresis()
{
    SwitchReport report;
    report.ts = 0.1;
    report.samples = 20000;
    report.setpoint = 60.0;

    const double ambient = 20.0;      /* 环境温度 */
    const double heater = 80.0;       /* 加热器全开时比环境高出的温度 */
    const double tau = 30.0;          /* 热时间常数，秒 */
    const double start = 59.0;
    const double bands[5] = {0.0, 0.5, 1.0, 2.0, 5.0};

    for (double band : bands) {
        double x = start;
        int on = 0;
        std::uint64_t switches = 0;
        double low = 0.0;
        double high = 0.0;
        double sum = 0.0;
        std::size_t counted = 0;
        const std::size_t tail_from = report.samples / 2;

        for (std::size_t k = 0; k < report.samples; ++k) {
            const double half = band * 0.5;
            const int before = on;
            if (x < report.setpoint - half) {
                on = 1;
            } else if (x > report.setpoint + half) {
                on = 0;
            }
            if (on != before) {
                ++switches;
            }
            if (k >= tail_from) {
                if (counted == 0) {
                    low = x;
                    high = x;
                }
                low = std::min(low, x);
                high = std::max(high, x);
                sum += x;
                ++counted;
            }
            x += (ambient + heater * static_cast<double>(on) - x) * report.ts / tau;
        }

        SwitchRow row;
        row.band = band;
        row.switches = switches;
        row.ripple = high - low;
        row.mean = sum / static_cast<double>(counted);
        report.rows.push_back(row);
    }

    return report;
}

/* ================= 三、PID 的三项 ================= */

namespace {

PidTermRow run_pid_row(const std::string &name, const PidGains &gains, double alpha,
                       const std::vector<double> *noise, std::size_t steps, double ts,
                       std::size_t load_index, std::size_t tail, double load_value)
{
    const double load_time = static_cast<double>(load_index) * ts;
    Scenario sc;
    sc.plant = plant_of(kUmax);
    sc.gains = gains;
    sc.ts = ts;
    sc.steps = steps;
    sc.aw = AntiWindup::Clamp;
    sc.derivative_alpha = alpha;
    sc.noise = noise;
    sc.reference = step_reference(1.0);
    sc.load = [load_time, load_value](double t) { return t >= load_time ? load_value : 0.0; };

    const SimResult run = simulate(sc);
    const TraceStats step_part = analyze(window_of(run, 0, load_index), 1.0, load_index);
    const TraceStats recover_part =
        analyze(window_of(run, load_index, steps - load_index), 1.0, tail);

    PidTermRow row;
    row.name = name;
    row.step_overshoot = step_part.overshoot;
    row.recover_overshoot = recover_part.overshoot;
    row.final_error = recover_part.final_error;
    row.control_tv = recover_part.control_tv;
    row.settled = recover_part.settled;
    row.settle_seconds =
        recover_part.settled ? static_cast<double>(recover_part.settle_index) * ts : 0.0;
    return row;
}

}   /* namespace */

PidTermsReport probe_pid_terms()
{
    PidTermsReport report;
    report.ts = kTs;
    report.seconds = 8.0;
    report.load_time = 2.0;
    report.load_value = 0.2;
    report.u_max = kUmax;
    report.noise_amplitude = 0.005;

    const std::size_t steps = static_cast<std::size_t>(report.seconds / report.ts);
    const std::size_t load_index = static_cast<std::size_t>(report.load_time / report.ts);
    const std::size_t tail = 250;                                        /* 0.5 s */
    const std::vector<double> noise = make_noise(steps, 20261003u, report.noise_amplitude);

    report.clean.push_back(run_pid_row(u8"P（kp = 0.500）", kGainsP, 0.0, nullptr, steps,
                                       report.ts, load_index, tail, report.load_value));
    report.clean.push_back(run_pid_row(u8"PI（ki = 0.500）", kGainsPi, 0.0, nullptr, steps,
                                       report.ts, load_index, tail, report.load_value));
    report.clean.push_back(run_pid_row(u8"PID（kd = 0.050）", kGainsPid, 0.0, nullptr, steps,
                                       report.ts, load_index, tail, report.load_value));

    report.noisy.push_back(run_pid_row(u8"PI，量测带噪声", kGainsPi, 0.0, &noise, steps,
                                       report.ts, load_index, tail, report.load_value));
    report.noisy.push_back(run_pid_row(u8"PID，量测带噪声", kGainsPid, 0.0, &noise, steps,
                                       report.ts, load_index, tail, report.load_value));
    report.noisy.push_back(run_pid_row(u8"PID 加微分低通 α = 0.200", kGainsPid, 0.2, &noise,
                                       steps, report.ts, load_index, tail, report.load_value));

    return report;
}

/* ================= 四、离散化与定点 ================= */

namespace {

/** 定点控制器：位置与力矩都按 12 位格点（1 格 = 1/1024）走整数，增益与积分器用 Q16 存放。
    右移手写成向零取整，不依赖负数右移的实现定义行为 */
class FixedPointController {
public:
    static constexpr int kShift = 16;
    static constexpr long long kOne = 1LL << kShift;

    FixedPointController(const PidGains &gains, double ts, long long duty_min, long long duty_max)
        : kp_(to_q(gains.kp)), ki_(to_q(gains.ki * ts)), kd_(to_q(gains.kd / ts)),
          duty_min_(duty_min), duty_max_(duty_max)
    {
    }

    long long update(long long setpoint, long long measurement)
    {
        const long long error = setpoint - measurement;
        long long derivative = 0;
        if (has_prev_) {
            derivative = shift(kd_ * (measurement - prev_));
        }
        prev_ = measurement;
        has_prev_ = true;

        const long long increment = ki_ * error;
        integral_ += increment;
        const long long raw = shift(kp_ * error) + shift(integral_) - derivative;
        const long long duty = std::min(std::max(raw, duty_min_), duty_max_);

        if ((raw > duty_max_ && error > 0) || (raw < duty_min_ && error < 0)) {
            integral_ -= increment;      /* 钳位：与浮点版的规则一致 */
        }
        return duty;
    }

private:
    static long long to_q(double value)
    {
        return std::llround(value * static_cast<double>(kOne));
    }

    /** 除以 2^16，向零取整 */
    static long long shift(long long value)
    {
        return value >= 0 ? (value >> kShift) : -((-value) >> kShift);
    }

    long long kp_ = 0;
    long long ki_ = 0;
    long long kd_ = 0;
    long long duty_min_ = 0;
    long long duty_max_ = 0;
    long long integral_ = 0;
    long long prev_ = 0;
    bool has_prev_ = false;
};

/** 三条路线各跑一遍：0 浮点不量化、1 浮点加 12 位量化、2 定点加 12 位量化 */
QuantRow run_quant_row(const std::string &name, int mode, double ts, std::size_t steps,
                       std::size_t load_index, std::size_t tail, double load_value, double lsb)
{
    const double load_time = static_cast<double>(load_index) * ts;
    const long long duty_max = static_cast<long long>(std::llround(kUmax / lsb));
    PidController pid(kGainsPi, ts, -kUmax, kUmax, AntiWindup::Clamp, 5.0, 0.0);
    FixedPointController fixed(kGainsPi, ts, -duty_max, duty_max);

    double y = 0.0;
    double v = 0.0;
    SimResult run;
    run.ts = ts;
    run.samples.reserve(steps);

    for (std::size_t k = 0; k < steps; ++k) {
        const double t = static_cast<double>(k) * ts;
        const double load = t >= load_time ? load_value : 0.0;
        double u = 0.0;

        if (mode == 0) {
            u = pid.update(1.0, y);                       /* 浮点、什么都不量化 */
        } else if (mode == 1) {
            const double measured = std::round(y / lsb) * lsb;
            u = std::round(pid.update(1.0, measured) / lsb) * lsb;
        } else {
            const long long setpoint = static_cast<long long>(std::llround(1.0 / lsb));
            const long long measurement = static_cast<long long>(std::llround(y / lsb));
            u = static_cast<double>(fixed.update(setpoint, measurement)) * lsb;
        }

        Sample sample;
        sample.t = t;
        sample.r = 1.0;
        sample.y = y;
        sample.u = u;
        sample.e = 1.0 - y;
        run.samples.push_back(sample);

        const double accel = (u - kFriction * v - load) / kInertia;
        y += v * ts + 0.5 * accel * ts * ts;
        v += accel * ts;
    }

    const TraceStats stats = analyze(run, 1.0, tail);
    QuantRow row;
    row.name = name;
    row.final_error = stats.final_error;
    row.final_max_abs_error = stats.final_max_abs_error;
    return row;
}

}   /* namespace */

DiscretizationReport probe_discretization()
{
    DiscretizationReport report;
    report.adc_bits = 12;
    report.lsb = 1.0 / 1024.0;                     /* 位置与力矩共用 1/1024 的格点 */
    report.frac_lsb = 1.0 / (1024.0 * 65536.0);    /* 定点积分器的一格，位置单位 */
    report.kp_q = std::llround(kGainsPi.kp * 65536.0);
    report.ki_q = std::llround(kGainsPi.ki * kTs * 65536.0);
    report.kd_q = std::llround(kGainsPid.kd / kTs * 65536.0);

    /* 采样周期：同一组 PI 增益，只改采样周期。最后两档已经粗到不能用了 */
    const double periods[8] = {0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5};
    const double horizon = 4.0;
    for (double ts : periods) {
        Scenario sc;
        sc.plant = plant_of(kUmax);
        sc.gains = kGainsPi;
        sc.ts = ts;
        sc.steps = static_cast<std::size_t>(horizon / ts);
        sc.aw = AntiWindup::Clamp;
        sc.reference = step_reference(1.0);
        const SimResult run = simulate(sc);

        const std::size_t window = std::max<std::size_t>(1, static_cast<std::size_t>(1.0 / ts));
        const TraceStats stats = analyze(run, 1.0, window);

        double biggest_jump = 0.0;
        for (std::size_t i = 1; i < run.size(); ++i) {
            biggest_jump =
                std::max(biggest_jump, std::fabs(run.samples[i].y - run.samples[i - 1].y));
        }

        TsRow row;
        row.ts = ts;
        row.diverged = stats.peak > 100.0;
        row.overshoot = row.diverged ? 0.0 : stats.overshoot;
        row.settled = stats.settled && !row.diverged;
        row.settle_seconds = row.settled ? static_cast<double>(stats.settle_index) * ts : 0.0;
        row.travel_per_sample = biggest_jump;
        report.ts_rows.push_back(row);
    }

    /* 浮点与定点：同一条对象、同一组增益，只换控制律的实现方式。
       跑够 16 s：负载加入之后振铃要衰到远小于一个最低位，才能拿两者相比 */
    const std::size_t steps = 8000;
    const std::size_t load_index = 1000;
    const std::size_t tail = 250;
    report.quant_rows.push_back(run_quant_row(u8"浮点，量测与输出都不量化", 0, kTs, steps,
                                              load_index, tail, 0.2, report.lsb));
    report.quant_rows.push_back(run_quant_row(u8"浮点，量测与输出 12 位量化", 1, kTs, steps,
                                              load_index, tail, 0.2, report.lsb));
    report.quant_rows.push_back(run_quant_row(u8"定点 Q16.16，同样 12 位量化", 2, kTs, steps,
                                              load_index, tail, 0.2, report.lsb));

    return report;
}

/* ================= 五、限幅与积分饱和 ================= */

AntiWindupReport probe_anti_windup()
{
    AntiWindupReport report;
    report.ts = kTs;
    report.u_max = 0.2;      /* 比走完 1 个单位所需要的力矩小得多，一开头就饱和 */
    report.kaw = 5.0;

    const AntiWindup modes[3] = {AntiWindup::None, AntiWindup::Clamp, AntiWindup::BackCalc};

    for (AntiWindup aw : modes) {
        Scenario sc;
        sc.plant = plant_of(report.u_max);
        sc.gains = kGainsPi;
        sc.ts = report.ts;
        sc.steps = 2500;                  /* 5 s：不做处理的那一路要振铃很久才肯停 */
        sc.aw = aw;
        sc.kaw = report.kaw;
        sc.reference = step_reference(1.0);
        const SimResult run = simulate(sc);
        const TraceStats stats = analyze(run, 1.0, 250);

        AntiWindupRow row;
        row.name = anti_windup_name(aw);
        row.overshoot = stats.overshoot;
        row.settled = stats.settled;
        row.settle_seconds =
            stats.settled ? static_cast<double>(stats.settle_index) * report.ts : 0.0;
        row.min_u = run.samples[0].u;
        row.max_u = run.samples[0].u;
        bool released = false;
        for (const Sample &s : run.samples) {
            row.min_u = std::min(row.min_u, s.u);
            row.max_u = std::max(row.max_u, s.u);
            if (s.u >= report.u_max - 1e-12) {
                ++row.saturated_samples;
                if (!released) {
                    row.max_integral = std::max(row.max_integral, s.integral);
                }
            } else {
                released = true;
            }
        }
        report.rows.push_back(row);
    }

    return report;
}

/* ================= 六、滤波 ================= */

namespace {

/** 一阶低通：y += α·(x − y) */
struct LowPass {
    double alpha = 0.0;
    double y = 0.0;
    double operator()(double x)
    {
        y += alpha * (x - y);
        return y;
    }
};

/** 滑动平均：最近 n 个样本的均值，缓冲先填 0（阶跃之前的值） */
struct MovingAverage {
    std::vector<double> buffer;
    std::size_t index = 0;
    double sum = 0.0;
    explicit MovingAverage(std::size_t n) : buffer(n, 0.0) {}
    double operator()(double x)
    {
        sum -= buffer[index];
        buffer[index] = x;
        sum += x;
        index = (index + 1) % buffer.size();
        return sum / static_cast<double>(buffer.size());
    }
};

double standard_deviation(const std::vector<double> &values, std::size_t skip)
{
    double mean = 0.0;
    std::size_t count = 0;
    for (std::size_t i = skip; i < values.size(); ++i) {
        mean += values[i];
        ++count;
    }
    mean /= static_cast<double>(count);
    double variance = 0.0;
    for (std::size_t i = skip; i < values.size(); ++i) {
        const double d = values[i] - mean;
        variance += d * d;
    }
    variance /= static_cast<double>(count);
    return std::sqrt(variance);
}

/** 阶跃响应到 50% 的时刻，线性插值成小数，单位是样本 */
double step_lag(const std::vector<double> &response)
{
    for (std::size_t i = 1; i < response.size(); ++i) {
        if (response[i] >= 0.5 && response[i - 1] < 0.5) {
            const double span = response[i] - response[i - 1];
            return static_cast<double>(i - 1) + (0.5 - response[i - 1]) / span;
        }
    }
    return static_cast<double>(response.size());
}

}   /* namespace */

FilterReport probe_filters()
{
    FilterReport report;
    report.ts = 0.001;

    const std::size_t samples = 200000;
    const double amplitude = 0.02;
    const std::vector<double> noise = make_noise(samples, 20261004u, amplitude);
    report.input_std = standard_deviation(noise, 0);

    struct Pair {
        const char *low_pass_name;
        double alpha;
        const char *average_name;
        std::size_t window;
    };
    const Pair pairs[2] = {
        {u8"一阶低通 α = 0.250", 0.25, u8"滑动平均 N = 7", 7},
        {u8"一阶低通 α = 0.100", 0.10, u8"滑动平均 N = 19", 19},
    };

    for (const Pair &pair : pairs) {
        std::vector<double> low_pass_out;
        std::vector<double> average_out;
        low_pass_out.reserve(samples);
        average_out.reserve(samples);

        LowPass low_pass;
        low_pass.alpha = pair.alpha;
        MovingAverage average(pair.window);
        for (std::size_t i = 0; i < samples; ++i) {
            low_pass_out.push_back(low_pass(1.0 + noise[i]));
            average_out.push_back(average(1.0 + noise[i]));
        }

        const std::size_t skip = pair.window * 4;
        const double low_pass_std = standard_deviation(low_pass_out, skip);
        const double average_std = standard_deviation(average_out, skip);

        LowPass step_low;
        step_low.alpha = pair.alpha;
        MovingAverage step_average(pair.window);
        std::vector<double> low_pass_step;
        std::vector<double> average_step;
        for (std::size_t i = 0; i < 64; ++i) {
            low_pass_step.push_back(step_low(1.0));
            average_step.push_back(step_average(1.0));
        }

        FilterRow low_row;
        low_row.name = pair.low_pass_name;
        low_row.noise_std = low_pass_std;
        low_row.suppression = report.input_std / low_pass_std;
        low_row.theory_std = report.input_std * std::sqrt(pair.alpha / (2.0 - pair.alpha));
        low_row.lag_samples = step_lag(low_pass_step);
        report.rows.push_back(low_row);

        FilterRow avg_row;
        avg_row.name = pair.average_name;
        avg_row.noise_std = average_std;
        avg_row.suppression = report.input_std / average_std;
        avg_row.theory_std = report.input_std / std::sqrt(static_cast<double>(pair.window));
        avg_row.lag_samples = step_lag(average_step);
        report.rows.push_back(avg_row);
    }

    return report;
}

/* ================= 七、测速 ================= */

SpeedReport probe_speed_methods()
{
    SpeedReport report;
    report.gate_seconds = 0.012;           /* 12 ms：与计数周期不成整数倍，M 法才不会白捡一个 0 误差 */
    const double rpms[5] = {1.0, 10.0, 100.0, 1000.0, 3000.0};
    const std::uint64_t phases = 211;      /* 质数，扫闸门相位时不会与计数周期共振 */
    const double gate_ticks = report.gate_seconds * report.clock_hz;

    for (double rpm : rpms) {
        SpeedRow row;
        row.rpm = rpm;
        row.phases = phases;
        const double period = report.clock_hz * 60.0 / (rpm * report.counts_per_rev);
        row.count_period_ticks = period;
        row.counts_per_gate = gate_ticks / period;

        double worst_m = 0.0;
        double worst_t = 0.0;
        double worst_mt = 0.0;
        const double scale = 60.0 / (report.counts_per_rev * report.gate_seconds);

        for (std::uint64_t p = 0; p < phases; ++p) {
            const double t0 = period * static_cast<double>(p) / static_cast<double>(phases);
            const double t1 = t0 + gate_ticks;

            /* 闸门内的计数边沿：i·period ∈ [t0, t1) */
            const long long first = static_cast<long long>(std::ceil(t0 / period - 1e-9));
            const long long last = static_cast<long long>(std::ceil(t1 / period - 1e-9)) - 1;
            const long long pulses = last - first + 1;

            /* M 法：数闸门里有多少个计数，一个计数都不给就只能报 0 */
            if (pulses <= 0) {
                ++row.m_zero_phases;
                worst_m = 1.0;
            } else {
                const double measured = static_cast<double>(pulses) * scale;
                worst_m = std::max(worst_m, std::fabs(measured - rpm) / rpm);
            }

            /* T 法：量一个计数周期，读数只能是整节拍，于是有 ±1 拍的模糊 */
            for (long long i = first; i < first + 4; ++i) {
                const double a = static_cast<double>(i) * period;
                const double b = a + period;
                const double ticks = std::floor(b) - std::floor(a);
                if (ticks > 0.0) {
                    const double measured =
                        60.0 * report.clock_hz / (report.counts_per_rev * ticks);
                    worst_t = std::max(worst_t, std::fabs(measured - rpm) / rpm);
                }
            }

            /* M/T 法：闸门两端对齐到计数边沿，用时钟把这段跨度量准。
               至少要两个边沿才构成一段；低速下一个也数不到，就往后多等一个边沿 */
            long long mt_first = first;
            long long mt_last = last;
            if (mt_last <= mt_first) {
                mt_last = mt_first + 1;
            }
            const double edge_first = static_cast<double>(mt_first) * period;
            const double edge_last = static_cast<double>(mt_last) * period;
            const double span = std::floor(edge_last) - std::floor(edge_first);
            if (span > 0.0) {
                const double measured = static_cast<double>(mt_last - mt_first) * 60.0 *
                                        report.clock_hz / (report.counts_per_rev * span);
                worst_mt = std::max(worst_mt, std::fabs(measured - rpm) / rpm);
            }
        }

        row.m_worst_ppm = worst_m * 1e6;
        row.t_worst_ppm = worst_t * 1e6;
        row.mt_worst_ppm = worst_mt * 1e6;
        report.rows.push_back(row);
    }

    return report;
}

/* ================= 八、前馈加反馈 ================= */

namespace {

/** 梯形速度规划：匀加速 → 匀速 → 匀减速 */
struct Profile {
    double v_max = 1.0;
    double accel = 2.0;
    double ta = 0.5;      /**< 加（减）速时间 */
    double tc = 0.5;      /**< 匀速时间 */

    double total() const { return v_max * (ta + tc); }
    double duration() const { return 2.0 * ta + tc; }

    double position(double t) const
    {
        if (t <= 0.0) {
            return 0.0;
        }
        if (t < ta) {
            return 0.5 * accel * t * t;
        }
        const double ramp = 0.5 * accel * ta * ta;
        if (t < ta + tc) {
            return ramp + v_max * (t - ta);
        }
        if (t < duration()) {
            const double s = t - ta - tc;
            return ramp + v_max * tc + v_max * s - 0.5 * accel * s * s;
        }
        return total();
    }

    double velocity(double t) const
    {
        if (t <= 0.0 || t >= duration()) {
            return 0.0;
        }
        if (t < ta) {
            return accel * t;
        }
        if (t < ta + tc) {
            return v_max;
        }
        return v_max - accel * (t - ta - tc);
    }

    double acceleration(double t) const
    {
        /* 边界按「左闭」处理：t = 0 属于第一段，否则起步那一拍的加速度会算成 0，
           前馈少给一份力矩，跟踪误差要过 0.2 s 才自己消掉 */
        if (t < 0.0 || t >= duration()) {
            return 0.0;
        }
        if (t < ta) {
            return accel;
        }
        if (t < ta + tc) {
            return 0.0;
        }
        return -accel;
    }
};

FeedRow run_feed_row(const std::string &name, double plant_inertia, double plant_load,
                     const PlantParams &nominal, bool use_feedforward, const Profile &profile,
                     double ts, std::size_t steps, std::size_t profile_end,
                     std::size_t cruise_from, std::size_t cruise_to)
{
    Scenario sc;
    sc.plant = nominal;
    sc.plant.inertia = plant_inertia;
    sc.gains = kGainsPi;
    sc.ts = ts;
    sc.steps = steps;
    sc.aw = AntiWindup::Clamp;
    sc.reference = [&profile](double t) { return profile.position(t); };
    sc.load = [plant_load](double) { return plant_load; };
    if (use_feedforward) {
        sc.feedforward = [&profile, &nominal, plant_load](double t) {
            return nominal.inertia * profile.acceleration(t) +
                   nominal.friction * profile.velocity(t) + plant_load;
        };
    }

    const SimResult run = simulate(sc);

    double max_error = 0.0;
    double square_sum = 0.0;
    for (std::size_t i = 0; i < profile_end && i < run.size(); ++i) {
        max_error = std::max(max_error, std::fabs(run.samples[i].e));
        square_sum += run.samples[i].e * run.samples[i].e;
    }
    const double rms = std::sqrt(square_sum / static_cast<double>(profile_end));

    double cruise_sum = 0.0;
    for (std::size_t i = cruise_from; i < cruise_to && i < run.size(); ++i) {
        cruise_sum += run.samples[i].e;
    }

    FeedRow row;
    row.name = name;
    row.max_error = max_error;
    row.rms_error = rms;
    row.cruise_error = cruise_sum / static_cast<double>(cruise_to - cruise_from);
    row.final_error = std::fabs(run.samples[run.size() - 1].e);
    return row;
}

}   /* namespace */

FeedforwardReport probe_feedforward()
{
    FeedforwardReport report;
    report.ts = kTs;
    report.cruise_velocity = 1.0;
    report.accel = 2.0;

    Profile profile;
    profile.v_max = report.cruise_velocity;
    profile.accel = report.accel;
    profile.ta = report.cruise_velocity / report.accel;
    profile.tc = 0.5;
    report.displacement = profile.total();
    report.profile_seconds = profile.duration();

    const std::size_t steps = 2000;                                   /* 4 s */
    const std::size_t profile_end = static_cast<std::size_t>(profile.duration() / report.ts);
    const std::size_t cruise_from = static_cast<std::size_t>(profile.ta / report.ts);
    const std::size_t cruise_to = static_cast<std::size_t>((profile.ta + profile.tc) / report.ts);

    const PlantParams nominal = plant_of(kUmax);
    const double plant_inertia = kInertia * 1.1;      /* 真实对象比标称重 10% */
    const double plant_load = 0.22;                   /* 真实负载比标称大 10% */

    report.rows.push_back(run_feed_row(u8"纯反馈 PI", plant_inertia, plant_load, nominal, false,
                                       profile, report.ts, steps, profile_end, cruise_from,
                                       cruise_to));
    report.rows.push_back(run_feed_row(u8"前馈（标称模型）+ PI", plant_inertia, plant_load,
                                       nominal, true, profile, report.ts, steps, profile_end,
                                       cruise_from, cruise_to));
    report.rows.push_back(run_feed_row(u8"前馈（模型完全准确）+ PI", kInertia, 0.2, nominal,
                                       true, profile, report.ts, steps, profile_end, cruise_from,
                                       cruise_to));

    return report;
}

/* ================= 报告 ================= */

std::string build_report()
{
    std::ostringstream out;

    /* ---------- 一 ---------- */
    const OpenClosedReport open_closed = probe_open_vs_closed();
    out << u8"一、开环与闭环：同一份扰动下的误差\n";
    out << u8"  被控对象：位置伺服 J = " << milli_text(open_closed.plant.inertia)
        << u8"，b = " << milli_text(open_closed.plant.friction) << u8"，力矩范围 ["
        << milli_text(open_closed.plant.u_min) << u8", " << milli_text(open_closed.plant.u_max)
        << u8"]\n";
    out << u8"  开环给的是标定出来的常值力矩：它把对象推到了哪儿，自己并不知道\n";
    out << u8"  闭环用 PI：kp = " << milli_text(open_closed.gains.kp)
        << u8"，ki = " << milli_text(open_closed.gains.ki) << u8"；采样周期 "
        << milli_text(open_closed.ts) << u8" s，每个情形跑 "
        << milli_text(open_closed.window_seconds) << u8" s\n";
    out << u8"  末误差取窗口末尾 0.200 s 的均值，最大误差与 IAE 取整个窗口\n";
    out << table_row(u8"情形", 32,
                     {u8"开环末误差", u8"开环最大误差", u8"开环 IAE", u8"闭环末误差",
                      u8"闭环最大误差", u8"闭环 IAE", u8"闭环恢复"},
                     14)
        << "\n";
    for (const OpenClosedRow &row : open_closed.rows) {
        out << table_row(row.name, 32,
                         {milli_text(row.open_error), milli_text(row.open_max_error),
                          milli_text(row.open_iae), milli_text(row.closed_error),
                          milli_text(row.closed_max_error), milli_text(row.closed_iae),
                          row.closed_recovered
                              ? seconds_text(static_cast<double>(row.closed_recover_samples) *
                                             open_closed.ts)
                              : std::string(u8"没有恢复")},
                         14)
            << "\n";
    }
    out << u8"  IAE 是窗口内 |误差| 对时间的积分，单位是 秒\n";
    out << u8"  开环那一列是发散的：负载一旦超过给定力矩，对象就朝反方向匀速跑掉\n";

    /* ---------- 二 ---------- */
    const SwitchReport hysteresis = probe_hysteresis();
    out << u8"\n二、开关控制与滞回：切换次数与温度波动\n";
    out << u8"  被控对象：一阶热模型，环境 20.000 度，加热器全开稳态 100.000 度，"
        << u8"时间常数 30.000 s\n";
    out << u8"  控制律：低于（目标 − 带宽/2）开，高于（目标 + 带宽/2）关；带宽为 0 时按目标值切换\n";
    out << u8"  目标 60.000 度，采样周期 " << milli_text(hysteresis.ts) << u8" s，跑 "
        << integer_text(static_cast<long long>(hysteresis.samples)) << u8" 个样本\n";
    out << table_row(u8"滞回带（度）", 16, {u8"切换次数", u8"后半程温度极差", u8"后半程均值"}, 16)
        << "\n";
    for (const SwitchRow &row : hysteresis.rows) {
        out << table_row(milli_text(row.band), 16,
                         {integer_text(static_cast<long long>(row.switches)),
                          milli_text(row.ripple), milli_text(row.mean)},
                         16)
            << "\n";
    }
    out << u8"  带宽为 0 时每一个样本都在翻转：这是抖振，不是控制\n";
    out << u8"  带宽一加，切换次数成倍下降，代价是温度波动跟着变大，这就是滞回的取舍\n";

    /* ---------- 三 ---------- */
    const PidTermsReport terms = probe_pid_terms();
    out << u8"\n三、PID 的三项：P 的稳态误差、I 消掉它的过程、D 与噪声\n";
    out << u8"  被控对象同上，力矩上限 " << milli_text(terms.u_max) << u8"；参考是 0 到 1.000 的阶跃，"
        << u8"第 " << milli_text(terms.load_time) << u8" s 加入负载 " << milli_text(terms.load_value)
        << u8"\n";
    out << u8"  采样周期 " << milli_text(terms.ts) << u8" s，跑 " << milli_text(terms.seconds)
        << u8" s；恢复超调指负载加入之后的峰值，控制量总变化是 Σ|Δu|\n";
    out << table_row(u8"控制器", 26,
                     {u8"阶跃超调", u8"恢复超调", u8"末尾误差", u8"恢复时间", u8"控制量总变化"}, 14)
        << "\n";
    for (const PidTermRow &row : terms.clean) {
        out << table_row(row.name, 26,
                         {milli_text(row.step_overshoot), milli_text(row.recover_overshoot),
                          milli_text(row.final_error),
                          row.settled ? seconds_text(row.settle_seconds)
                                      : std::string(u8"没有恢复"),
                          milli_text(row.control_tv)},
                         14)
            << "\n";
    }
    out << u8"  P 的稳态误差 = 负载 / kp = " << milli_text(terms.load_value) << u8" / 0.500 = "
        << milli_text(terms.load_value / 0.5) << u8"，与实测一致；\n";
    out << u8"  积分项把它压到 0，代价是阶跃超调从 " << milli_text(terms.clean[0].step_overshoot)
        << u8" 涨到 " << milli_text(terms.clean[1].step_overshoot) << u8"\n";
    out << u8"  量测噪声：固定种子的均匀分布，幅度 ±" << milli_text(terms.noise_amplitude) << u8"\n";
    out << table_row(u8"控制器（量测带噪声）", 26,
                     {u8"阶跃超调", u8"恢复超调", u8"末尾误差", u8"恢复时间", u8"控制量总变化"}, 14)
        << "\n";
    for (const PidTermRow &row : terms.noisy) {
        out << table_row(row.name, 26,
                         {milli_text(row.step_overshoot), milli_text(row.recover_overshoot),
                          milli_text(row.final_error),
                          row.settled ? seconds_text(row.settle_seconds)
                                      : std::string(u8"没有恢复"),
                          milli_text(row.control_tv)},
                         14)
            << "\n";
    }
    out << u8"  微分项把噪声的逐样本跳变放大，控制量总变化这个数最先涨上去；\n";
    out << u8"  给微分项加一阶低通（α = 0.200）能把它压回去，代价是微分滞后一点\n";

    /* ---------- 四 ---------- */
    const DiscretizationReport discrete = probe_discretization();
    out << u8"\n四、离散化与定点：采样周期与字长各值多少\n";
    out << u8"  同一组 PI 增益（kp = 0.500，ki = 0.500），只改采样周期，参考是 0 到 1.000 的阶跃\n";
    out << table_row(u8"采样周期（s）", 16, {u8"超调", u8"稳定时间", u8"相邻样本最大跳变"}, 18) << "\n";
    for (const TsRow &row : discrete.ts_rows) {
        out << table_row(milli_text(row.ts), 16,
                         {row.diverged ? std::string(u8"发散") : milli_text(row.overshoot),
                          row.settled ? seconds_text(row.settle_seconds)
                                      : std::string(u8"没稳定"),
                          milli_text(row.travel_per_sample)},
                         18)
            << "\n";
    }
    out << u8"  采样周期一长，控制器拿到的就是旧信息：同样的增益，超调从 "
        << milli_text(discrete.ts_rows.front().overshoot) << u8" 一路涨上去，拉到 "
        << milli_text(discrete.ts_rows.back().ts) << u8" s 直接发散\n";

    out << u8"  定点：位置与力矩都走 1/1024 的格点（位置 12 位盖住 [0, 4)，力矩 13 位盖住 [−4, 4]），"
        << u8"一格 = " << format_fixed(discrete.lsb, 6) << u8"（位置单位）\n";
    out << u8"  定点增益：kp = " << integer_text(discrete.kp_q) << u8"/65536，ki·Ts = "
        << integer_text(discrete.ki_q) << u8"/65536，kd/Ts = " << integer_text(discrete.kd_q)
        << u8"/65536\n";
    out << table_row(u8"控制器实现", 30, {u8"末尾平均误差", u8"末尾最大误差"}, 16) << "\n";
    for (const QuantRow &row : discrete.quant_rows) {
        out << table_row(row.name, 30,
                         {milli_text(row.final_error), milli_text(row.final_max_abs_error)}, 16)
            << "\n";
    }
    out << u8"  末尾窗口是最后 0.500 s；定点那一版的积分器一格是 "
        << format_fixed(discrete.frac_lsb, 9) << u8"，比传感器细四个数量级，\n";
    out << u8"  所以瓶颈在量测与执行器的位数，不在运算用几位\n";

    /* ---------- 五 ---------- */
    const AntiWindupReport windup = probe_anti_windup();
    out << u8"\n五、限幅与积分饱和：不做处理、钳位、反算\n";
    out << u8"  参考是 0 到 1.000 的阶跃，力矩上限压到 ±" << milli_text(windup.u_max)
        << u8"：一开头就需要更大的力矩，输出贴着上限好一阵\n";
    out << u8"  反算系数 kaw = " << milli_text(windup.kaw) << u8" /s；采样周期 "
        << milli_text(windup.ts) << u8" s；饱和期间积分器峰值只统计贴限幅的那一段\n";
    out << table_row(u8"抗饱和做法", 14,
                     {u8"超调", u8"稳定时间", u8"贴限幅的样本", u8"饱和期积分器峰值", u8"控制量范围"},
                     18)
        << "\n";
    for (const AntiWindupRow &row : windup.rows) {
        out << table_row(row.name, 14,
                         {milli_text(row.overshoot),
                          row.settled ? seconds_text(row.settle_seconds)
                                      : std::string(u8"没稳定"),
                          integer_text(static_cast<long long>(row.saturated_samples)),
                          milli_text(row.max_integral),
                          milli_text(row.min_u) + u8" ~ " + milli_text(row.max_u)},
                         18)
            << "\n";
    }
    out << u8"  不做处理时积分器在饱和期间一直涨，退出饱和之后还得花时间把它放掉\n";

    /* ---------- 六 ---------- */
    const FilterReport filters = probe_filters();
    out << u8"\n六、滤波：噪声抑制与相位滞后是同一笔账\n";
    out << u8"  噪声是固定种子的均匀分布，幅度 ±0.020，长 200000 个样本；采样周期 "
        << milli_text(filters.ts) << u8" s\n";
    out << u8"  输入噪声标准差 " << format_fixed(filters.input_std, 6) << u8"\n";
    out << table_row(u8"滤波器", 20,
                     {u8"输出标准差", u8"抑制倍数", u8"理论标准差", u8"阶跃 50% 滞后"},
                     18)
        << "\n";
    for (const FilterRow &row : filters.rows) {
        out << table_row(row.name, 20,
                         {format_fixed(row.noise_std, 6), format_fixed(row.suppression, 3),
                          format_fixed(row.theory_std, 6),
                          format_fixed(row.lag_samples, 3) + u8" 样本"},
                         18)
            << "\n";
    }
    out << u8"  两组滤波器是配好对的：α/(2−α) 与 1/N 相等，抑制倍数就相等\n";
    out << u8"  抑制一样时，一阶低通的滞后比同样长度的滑动平均小：它把权重压在当前样本上\n";

    /* ---------- 七 ---------- */
    const SpeedReport speed = probe_speed_methods();
    out << u8"\n七、测速：M 法、T 法、M/T 法在低速与高速下的量化误差\n";
    out << u8"  编码器每转 " << integer_text(static_cast<long long>(speed.counts_per_rev))
        << u8" 个计数，时钟 " << format_fixed(speed.clock_hz / 1e6, 1) << u8" MHz，闸门 "
        << milli_text(speed.gate_seconds * 1000.0) << u8" ms\n";
    out << u8"  闸门起点在一个计数周期内扫 "
        << integer_text(static_cast<long long>(speed.rows[0].phases))
        << u8" 个相位，表里是各个相位上最坏的那次相对误差\n";
    out << table_row(u8"转速（rpm）", 14,
                     {u8"计数周期（节拍）", u8"闸门内计数", u8"M 法（ppm）", u8"T 法（ppm）",
                      u8"M/T 法（ppm）"},
                     17)
        << "\n";
    for (const SpeedRow &row : speed.rows) {
        out << table_row(format_fixed(row.rpm, 0), 14,
                         {format_fixed(row.count_period_ticks, 3),
                          format_fixed(row.counts_per_gate, 3),
                          integer_text(static_cast<long long>(std::llround(row.m_worst_ppm))),
                          integer_text(static_cast<long long>(std::llround(row.t_worst_ppm))),
                          integer_text(static_cast<long long>(std::llround(row.mt_worst_ppm)))},
                         17)
            << "\n";
    }
    out << u8"  ppm 是百万分之一，1000000 ppm 就是 100%\n";
    out << u8"  1 rpm 那一行里，M 法在 "
        << integer_text(static_cast<long long>(speed.rows[0].m_zero_phases)) << u8" / "
        << integer_text(static_cast<long long>(speed.rows[0].phases))
        << u8" 个相位上数到的计数是 0，读数只能报 0\n";
    out << u8"  M 法在低速吃亏、T 法在高速吃亏；M/T 法把闸门两端对齐到计数边沿，"
        << u8"五个转速都稳在 10 ppm 以内\n";

    /* ---------- 八 ---------- */
    const FeedforwardReport feed = probe_feedforward();
    out << u8"\n八、前馈加反馈：梯形速度规划的跟踪误差\n";
    out << u8"  规划：加速到 " << milli_text(feed.cruise_velocity) << u8" 单位/s（加速度 "
        << milli_text(feed.accel) << u8"），匀速 0.500 s，再减到 0；总位移 "
        << milli_text(feed.displacement) << u8"（解析值 1.000），总时长 "
        << milli_text(feed.profile_seconds) << u8" s\n";
    out << u8"  真实对象比标称重 10%、负载比标称大 10%；前馈用标称模型算，反馈用 PI\n";
    out << table_row(u8"控制器", 26,
                     {u8"最大跟踪误差", u8"RMS 跟踪误差", u8"匀速段平均误差", u8"停下后误差"}, 16)
        << "\n";
    for (const FeedRow &row : feed.rows) {
        out << table_row(row.name, 26,
                         {milli_text(row.max_error), milli_text(row.rms_error),
                          milli_text(row.cruise_error), milli_text(row.final_error)},
                         16)
            << "\n";
    }
    out << u8"  纯反馈要靠误差才能产生力矩，跟一个动着的参考就必然落后一拍\n";
    out << u8"  前馈把模型里知道的那部分一次给足，剩下的误差只是模型没算准的残差\n";

    return out.str();
}

/* ================= 自测 ================= */

namespace {

/** 逐项记结果：每一条都要真断言，不能只打印 */
class Checker {
public:
    void check(bool ok, const std::string &text)
    {
        ++result_.total;
        if (ok) {
            ++result_.passed;
        } else {
            ++result_.failed;
        }
        result_.lines.push_back(std::string(ok ? u8"[通过] " : u8"[失败] ") +
                                integer_text(static_cast<long long>(result_.total)) + u8". " +
                                text);
    }

    const CheckResult &result() const { return result_; }

private:
    CheckResult result_;
};

bool monotone_non_increasing(const std::vector<double> &values)
{
    for (std::size_t i = 1; i < values.size(); ++i) {
        if (values[i] > values[i - 1] + 1e-12) {
            return false;
        }
    }
    return true;
}

bool monotone_non_decreasing(const std::vector<double> &values)
{
    for (std::size_t i = 1; i < values.size(); ++i) {
        if (values[i] < values[i - 1] - 1e-12) {
            return false;
        }
    }
    return true;
}

}   /* namespace */

CheckResult run_self_tests()
{
    Checker checker;

    /* ---------- 被控对象 ---------- */
    {
        Scenario sc;
        sc.plant = plant_of(kUmax);
        sc.gains = PidGains{0.0, 0.0, 0.0};
        sc.ts = kTs;
        sc.steps = 1000;
        sc.y0 = 1.0;
        sc.reference = step_reference(1.0);
        sc.load = [](double) { return 0.2; };
        sc.feedforward = [](double) { return 0.2; };
        const SimResult run = simulate(sc);
        const double drift = std::fabs(run.samples[run.size() - 1].y - 1.0);
        checker.check(drift < 0.001,
                      u8"常值力矩 0.200 配负载 0.200 是平衡点：2.000 s 内位置漂移"
                      u8"不到 1 毫（实测 " + format_fixed(drift, 6) + u8"）");

        sc.load = [](double) { return 0.4; };
        const SimResult pushed = simulate(sc);
        const double moved = 1.0 - pushed.samples[pushed.size() - 1].y;
        checker.check(moved > 1.0,
                      u8"负载涨到 0.400 之后同一个力矩守不住：位置朝反方向跑掉 "
                      + format_fixed(moved, 3) + u8"（开环没有回头的机制）");
    }

    /* ---------- 13.1 开环与闭环 ---------- */
    const OpenClosedReport open_closed = probe_open_vs_closed();
    {
        const OpenClosedRow &load_step = open_closed.rows[0];
        checker.check(load_step.open_error > 1.0 && std::fabs(load_step.closed_error) < 0.02,
                      u8"负载阶跃：开环末误差 " + format_fixed(load_step.open_error, 3) +
                          u8"，闭环 " + format_fixed(load_step.closed_error, 3) +
                          u8"（不到开环的百分之一）");
        checker.check(load_step.closed_recovered && load_step.closed_iae < load_step.open_iae / 10.0,
                      u8"负载阶跃：闭环在 " +
                          format_fixed(static_cast<double>(load_step.closed_recover_samples) *
                                           open_closed.ts, 3) +
                          u8" s 内回到 ±2%，IAE " + format_fixed(load_step.closed_iae, 3) +
                          u8" 不到开环 " + format_fixed(load_step.open_iae, 3) + u8" 的十分之一");

        const OpenClosedRow &calib = open_closed.rows[1];
        checker.check(std::fabs(calib.open_error) > 0.5 && std::fabs(calib.closed_error) < 0.005,
                      u8"标定偏差 25%：开环 2.000 s 后偏出 " + format_fixed(calib.open_error, 3) +
                          u8"，闭环 " + format_fixed(calib.closed_error, 3) +
                          u8"（反馈不需要标定值）");
    }

    /* ---------- 13.2 滞回 ---------- */
    const SwitchReport hysteresis = probe_hysteresis();
    {
        std::vector<double> counts;
        std::vector<double> ripples;
        for (const SwitchRow &row : hysteresis.rows) {
            counts.push_back(static_cast<double>(row.switches));
            ripples.push_back(row.ripple);
        }
        const SwitchRow &none = hysteresis.rows[0];
        const SwitchRow &half = hysteresis.rows[1];
        const SwitchRow &wide = hysteresis.rows[4];

        checker.check(none.switches >= hysteresis.samples * 9 / 10,
                      u8"带宽为 0 时抖振：切换 " +
                          integer_text(static_cast<long long>(none.switches)) + u8" 次，样本数 " +
                          integer_text(static_cast<long long>(hysteresis.samples)) +
                          u8"（每个样本都在翻转）");
        checker.check(half.switches * 4 <= none.switches,
                      u8"带宽 0.500 度就把切换次数压到 " +
                          integer_text(static_cast<long long>(half.switches)) + u8" 次，是无滞回的 1/" +
                          integer_text(static_cast<long long>(none.switches / half.switches)) +
                          u8" 上下");
        checker.check(monotone_non_increasing(counts) && monotone_non_decreasing(ripples),
                      u8"切换次数随带宽单调不增、温度极差单调不减：带宽加大只是把抖振换成了波动");
        checker.check(wide.switches * 20 <= none.switches && wide.ripple >= none.ripple * 20.0,
                      u8"带宽 5.000 度：切换 " +
                          integer_text(static_cast<long long>(wide.switches)) + u8" 次（不到无滞回的 1/20），"
                          u8"温度极差 " + format_fixed(wide.ripple, 3) + u8" 度（是无滞回的 " +
                          format_fixed(wide.ripple / none.ripple, 1) + u8" 倍）");
        bool mean_ok = true;
        for (const SwitchRow &row : hysteresis.rows) {
            mean_ok = mean_ok && std::fabs(row.mean - hysteresis.setpoint) < 0.5;
        }
        checker.check(mean_ok, u8"五个带宽下后半程温度均值都在目标 ±0.500 度内："
                               u8"滞回改的是波动，不是均值");
    }

    /* ---------- 13.3 PID 的三项 ---------- */
    const PidTermsReport terms = probe_pid_terms();
    {
        const PidTermRow &p_row = terms.clean[0];
        const PidTermRow &pi_row = terms.clean[1];
        const PidTermRow &pid_row = terms.clean[2];
        const double theory = terms.load_value / 0.5;
        checker.check(std::fabs(p_row.final_error - theory) < 0.002,
                      u8"P 的稳态误差等于 负载/kp = " + format_fixed(theory, 3) + u8"，实测 " +
                          format_fixed(p_row.final_error, 3) + u8"（差 " +
                          format_fixed(std::fabs(p_row.final_error - theory), 6) + u8"）");
        checker.check(!p_row.settled && pi_row.settled && std::fabs(pi_row.final_error) < 0.005,
                      u8"P 永远回不到 ±2%（稳态误差 " + format_fixed(p_row.final_error, 3) +
                          u8"），PI 把末尾误差压到 " + format_fixed(pi_row.final_error, 3));
        checker.check(pi_row.step_overshoot > p_row.step_overshoot * 1.5,
                      u8"积分项冲过头：PI 的阶跃超调 " + format_fixed(pi_row.step_overshoot, 3) +
                          u8"，比 P 的 " + format_fixed(p_row.step_overshoot, 3) +
                          u8" 高出一大截——消稳态误差是用超调换来的");
        checker.check(pid_row.step_overshoot < pi_row.step_overshoot * 0.75,
                      u8"微分项加阻尼：PID 阶跃超调 " + format_fixed(pid_row.step_overshoot, 3) +
                          u8" 不到 PI " + format_fixed(pi_row.step_overshoot, 3) + u8" 的四分之三");
        checker.check(pid_row.recover_overshoot < 0.1 && pi_row.recover_overshoot < 0.1 &&
                          std::fabs(pid_row.final_error) < 0.005 &&
                          std::fabs(pi_row.final_error) < 0.005,
                      u8"负载段两条路线都把稳态误差消掉：恢复超调 PID " +
                          format_fixed(pid_row.recover_overshoot, 3) + u8"、PI " +
                          format_fixed(pi_row.recover_overshoot, 3) + u8"，末尾误差 " +
                          format_fixed(pid_row.final_error, 3) + u8" 与 " +
                          format_fixed(pi_row.final_error, 3));

        const PidTermRow &noisy_pi = terms.noisy[0];
        const PidTermRow &noisy_pid = terms.noisy[1];
        const PidTermRow &filtered = terms.noisy[2];
        checker.check(noisy_pid.control_tv > noisy_pi.control_tv * 3.0,
                      u8"量测带噪声时微分项放大噪声：控制量总变化 " +
                          format_fixed(noisy_pid.control_tv, 3) + u8" 是 PI 的 " +
                          format_fixed(noisy_pid.control_tv / noisy_pi.control_tv, 2) + u8" 倍");
        checker.check(filtered.control_tv < noisy_pid.control_tv * 0.7,
                      u8"给微分项加 α = 0.200 的低通：控制量总变化回到 " +
                          format_fixed(filtered.control_tv, 3) + u8"，是未滤波的 " +
                          format_fixed(filtered.control_tv / noisy_pid.control_tv, 2) + u8" 倍");
    }

    /* ---------- 13.4 离散化与定点 ---------- */
    const DiscretizationReport discrete = probe_discretization();
    {
        std::vector<double> overshoots;
        for (const TsRow &row : discrete.ts_rows) {
            overshoots.push_back(row.diverged ? 1e9 : row.overshoot);
        }
        std::size_t last_ok = 0;
        for (std::size_t i = 0; i < discrete.ts_rows.size(); ++i) {
            if (!discrete.ts_rows[i].diverged) {
                last_ok = i;
            }
        }
        const double fastest = discrete.ts_rows.front().overshoot;
        const double slowest = discrete.ts_rows[last_ok].overshoot;
        checker.check(monotone_non_decreasing(overshoots) && discrete.ts_rows.back().diverged &&
                          slowest >= fastest * 1.5,
                      u8"采样周期越长超调越大：" + format_fixed(discrete.ts_rows.front().ts, 3) +
                          u8" s 时 " + format_fixed(fastest, 3) + u8"，涨到 " +
                          format_fixed(discrete.ts_rows[last_ok].ts, 3) + u8" s 时 " +
                          format_fixed(slowest, 3) + u8"，再往上（" +
                          format_fixed(discrete.ts_rows.back().ts, 3) + u8" s）直接发散");

        const QuantRow &ideal = discrete.quant_rows[0];
        const QuantRow &quantized = discrete.quant_rows[1];
        const QuantRow &fixed = discrete.quant_rows[2];
        checker.check(fixed.final_max_abs_error > ideal.final_max_abs_error * 100.0,
                      u8"定点（12 位格点）的末尾最大误差 " +
                          format_fixed(fixed.final_max_abs_error, 6) + u8" 是浮点不量化 " +
                          format_fixed(ideal.final_max_abs_error, 9) + u8" 的 " +
                          integer_text(static_cast<long long>(
                              fixed.final_max_abs_error / std::max(ideal.final_max_abs_error, 1e-12))) +
                          u8" 倍");
        checker.check(std::fabs(fixed.final_max_abs_error - quantized.final_max_abs_error) <=
                          2.0 * discrete.lsb,
                      u8"定点与「浮点 + 同样量化」之差 "
                      + format_fixed(std::fabs(fixed.final_max_abs_error -
                                               quantized.final_max_abs_error), 6) +
                          u8"，不超过 2 个最低位（" + format_fixed(2.0 * discrete.lsb, 6) +
                          u8"）：瓶颈在格点，不在运算位数");
        checker.check(std::fabs(ideal.final_error) < discrete.lsb &&
                          std::fabs(quantized.final_error) < 2.0 * discrete.lsb &&
                          std::fabs(fixed.final_error) < 2.0 * discrete.lsb,
                      u8"三条路线的末尾平均误差都在两个最低位（" +
                          format_fixed(2.0 * discrete.lsb, 6) + u8"）之内，"
                          u8"积分项照样把平均误差消掉；量化过的那两条只剩格点带来的零点几毫");
    }

    /* ---------- 13.5 限幅与积分饱和 ---------- */
    const AntiWindupReport windup = probe_anti_windup();
    {
        const AntiWindupRow &none = windup.rows[0];
        const AntiWindupRow &clamp = windup.rows[1];
        const AntiWindupRow &back = windup.rows[2];

        checker.check(none.overshoot > clamp.overshoot * 3.0 && none.settled && clamp.settled &&
                          none.settle_seconds > clamp.settle_seconds * 1.4,
                      u8"不加抗饱和：超调 " + format_fixed(none.overshoot, 3) + u8"（钳位 " +
                          format_fixed(clamp.overshoot, 3) + u8"），稳定时间 " +
                          format_fixed(none.settle_seconds, 3) + u8" s（钳位 " +
                          format_fixed(clamp.settle_seconds, 3) + u8" s）");
        checker.check(none.max_integral > clamp.max_integral * 5.0 &&
                          none.max_integral > back.max_integral * 5.0,
                      u8"饱和期间积分器峰值：不做处理 " + format_fixed(none.max_integral, 3) +
                          u8"，钳位 " + format_fixed(clamp.max_integral, 3) + u8"，反算 " +
                          format_fixed(back.max_integral, 3) + u8"；前者的积分器涨上去就下不来");
        checker.check(back.overshoot <= clamp.overshoot && back.settled,
                      u8"反算把超调压得比钳位还低一点：" + format_fixed(back.overshoot, 3) +
                          u8" 对 " + format_fixed(clamp.overshoot, 3) + u8"，稳定时间 " +
                          format_fixed(back.settle_seconds, 3) + u8" s");
        bool in_range = true;
        for (const AntiWindupRow &row : windup.rows) {
            in_range = in_range && row.min_u >= -windup.u_max - 1e-9 &&
                       row.max_u <= windup.u_max + 1e-9;
        }
        checker.check(in_range,
                      u8"三种做法的控制量都落在 ±" + format_fixed(windup.u_max, 3) +
                          u8" 内：限幅本身是真的生效了，差别只在积分器怎么处理");
    }

    /* ---------- 13.6 滤波 ---------- */
    const FilterReport filters = probe_filters();
    {
        bool pair_ok = true;
        bool theory_ok = true;
        bool lag_ok = true;
        for (std::size_t i = 0; i + 1 < filters.rows.size(); i += 2) {
            const FilterRow &low = filters.rows[i];
            const FilterRow &avg = filters.rows[i + 1];
            pair_ok = pair_ok &&
                      std::fabs(low.noise_std - avg.noise_std) / avg.noise_std < 0.05;
            theory_ok = theory_ok &&
                        std::fabs(low.noise_std - low.theory_std) / low.theory_std < 0.05 &&
                        std::fabs(avg.noise_std - avg.theory_std) / avg.theory_std < 0.05;
            lag_ok = lag_ok && low.lag_samples < avg.lag_samples;
        }
        const FilterRow &first_low = filters.rows[0];
        const FilterRow &first_avg = filters.rows[1];
        checker.check(pair_ok && theory_ok,
                      u8"配好对的两组滤波器输出标准差相对差不到 5%，"
                      u8"与 α/(2−α)、1/N 两个理论值也差不到 5%：抑制倍数确实相等");
        checker.check(first_low.suppression > 2.0 && first_avg.suppression > 2.0 && lag_ok,
                      u8"抑制一样时低通滞后更小：α = 0.250 的 50% 滞后 " +
                          format_fixed(first_low.lag_samples, 3) + u8" 个样本，N = 7 的滑动平均 " +
                          format_fixed(first_avg.lag_samples, 3) + u8" 个样本");
    }

    /* ---------- 13.7 测速 ---------- */
    const SpeedReport speed = probe_speed_methods();
    {
        std::vector<double> m_errors;
        std::vector<double> t_errors;
        bool mt_ok = true;
        for (const SpeedRow &row : speed.rows) {
            m_errors.push_back(row.m_worst_ppm);
            t_errors.push_back(row.t_worst_ppm);
            const double better = std::min(row.m_worst_ppm, row.t_worst_ppm);
            mt_ok = mt_ok && row.mt_worst_ppm <= 50.0 &&
                    (better <= 0.0 || row.mt_worst_ppm <= better * 4.0);
        }
        const SpeedRow &low = speed.rows.front();
        const SpeedRow &high = speed.rows.back();
        checker.check(low.m_worst_ppm > low.t_worst_ppm * 1000.0,
                      u8"低速 " + format_fixed(low.rpm, 0) + u8" rpm：M 法最坏误差 " +
                          integer_text(static_cast<long long>(low.m_worst_ppm)) +
                          u8" ppm，T 法只有 " +
                          integer_text(static_cast<long long>(low.t_worst_ppm)) + u8" ppm，差 " +
                          integer_text(static_cast<long long>(low.m_worst_ppm /
                                                              std::max(low.t_worst_ppm, 1.0))) +
                          u8" 倍");
        checker.check(high.t_worst_ppm > high.m_worst_ppm * 10.0,
                      u8"高速 " + format_fixed(high.rpm, 0) + u8" rpm：T 法最坏误差 " +
                          integer_text(static_cast<long long>(high.t_worst_ppm)) +
                          u8" ppm，M 法只有 " +
                          integer_text(static_cast<long long>(high.m_worst_ppm)) + u8" ppm，反过来了");
        checker.check(monotone_non_increasing(m_errors) && monotone_non_decreasing(t_errors),
                      u8"M 法最坏误差随转速单调不增、T 法单调不减：一个数脉冲、一个数时钟");
        checker.check(mt_ok,
                      u8"M/T 法在五个转速上都不超过 50 ppm，也不超过两法中较好者的 4 倍："
                      u8"两端都稳");
        checker.check(low.m_zero_phases > 0 && low.m_zero_phases < low.phases,
                      u8"低速下 M 法有一段相位一个计数都数不到：" +
                          integer_text(static_cast<long long>(low.m_zero_phases)) + u8" / " +
                          integer_text(static_cast<long long>(low.phases)) + u8" 个相位读数是 0");
    }

    /* ---------- 13.8 前馈加反馈 ---------- */
    const FeedforwardReport feed = probe_feedforward();
    {
        checker.check(std::fabs(feed.displacement - 1.0) < 0.001 && feed.profile_seconds > 0.0,
                      u8"梯形规划的面积等于解析值：位移 " + format_fixed(feed.displacement, 3) +
                          u8"，时长 " + format_fixed(feed.profile_seconds, 3) + u8" s（解析值 1.500）");
        const FeedRow &plain = feed.rows[0];
        const FeedRow &with_ff = feed.rows[1];
        const FeedRow &perfect = feed.rows[2];
        checker.check(with_ff.max_error < plain.max_error * 0.3 &&
                          with_ff.rms_error < plain.rms_error * 0.3,
                      u8"前馈加反馈把最大跟踪误差从 " + format_fixed(plain.max_error, 3) +
                          u8" 压到 " + format_fixed(with_ff.max_error, 3) + u8"，RMS 从 " +
                          format_fixed(plain.rms_error, 3) + u8" 压到 " +
                          format_fixed(with_ff.rms_error, 3));
        checker.check(perfect.max_error < 1e-6,
                      u8"模型完全准确时前馈的跟踪误差只剩 " +
                          format_fixed(perfect.max_error, 9) + u8"：模型知道的部分不用等误差");
    }

    /* ---------- 可复现 ---------- */
    {
        const std::string first = build_report();
        const std::string second = build_report();
        checker.check(first == second && !first.empty(),
                      u8"同一组参数把报告跑两遍，两次文本逐字节相同（长度 " +
                          integer_text(static_cast<long long>(first.size())) + u8" 字节）");
    }

    return checker.result();
}

}   /* namespace ctrl */
