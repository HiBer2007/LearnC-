/* sim_irq.cpp —— 中断模型的实现（素材，不要改） */
#include "sim_irq.hpp"

namespace sim {
namespace {

Handler            g_handler = nullptr;
bool               g_pending = false;
unsigned           g_disable_depth = 0;
unsigned           g_isr_calls = 0;
unsigned long long g_poll_calls = 0;

void dispatch(void)
{
    g_pending = false;
    ++g_isr_calls;
    if (g_handler != nullptr) {
        g_handler();
    }
}

}  // namespace

void reset(Handler handler)
{
    g_handler = handler;
    g_pending = false;
    g_disable_depth = 0;
    g_isr_calls = 0;
    g_poll_calls = 0;
}

void request_irq(void)
{
    g_pending = true;
}

void poll(void)
{
    ++g_poll_calls;
    if (g_pending && g_disable_depth == 0u) {
        dispatch();
    }
}

bool irq_enabled(void)
{
    return g_disable_depth == 0u;
}

void disable_irq(void)
{
    ++g_disable_depth;
}

void enable_irq(void)
{
    if (g_disable_depth > 0u) {
        --g_disable_depth;
        if (g_disable_depth == 0u && g_pending) {
            /* 真板上清掉 PRIMASK 之后，挂着的中断马上就会被取走 */
            dispatch();
        }
    }
}

bool pending(void)
{
    return g_pending;
}

unsigned isr_calls(void)
{
    return g_isr_calls;
}

unsigned long long poll_calls(void)
{
    return g_poll_calls;
}

void clear_stats(void)
{
    g_isr_calls = 0;
    g_poll_calls = 0;
}

}  // namespace sim
