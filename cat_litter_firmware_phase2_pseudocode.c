/*
 * WooHuloo 猫砂盆固件：第二批恢复伪代码（教学注释版）
 *
 * 固件样本：cat_litter_firmware.bin
 * 架构：ARM Cortex-M / Thumb-2；活动应用位于约 0x08004000～0x080223EB。
 *
 * 重要边界：
 * 1. 这不是厂商遗失的 C 源码，而是依据机器码控制流、常量、字符串和 RAM/外设访问
 *    重写的、可阅读的等价伪 C。
 * 2. [确认] 表示可以直接回指到指令或常量；[推断] 表示业务名称来自行为解释；
 *    [未完成] 表示还没有把所有分支还原到同等精度。
 * 3. 文件用于学习和 clean-room 行为规格，不应直接拿去驱动真实电机或阀。
 */

#include <stdbool.h>
#include <stdint.h>

/* ---------- 原固件中的固定地址：仅用于说明证据，不可移植到 ESP32-S3 ---------- */

#define SCHEDULER_PENDING_ADDR   0x20000004u
#define CLEAN_TICK_ADDR          0x2000A91Cu
#define INPUT_ARRAY_ADDR         0x2000A890u
#define LIQUID_MONITOR_ADDR      0x2000A8B0u
#define SOLENOID_CONTROL_ADDR    0x2000A8B8u
#define DEVICE_STATE_ADDR        0x2000B500u
#define LIQUID_TIMER_ADDR        0x2000B49Cu
#define SOLENOID_TIMER_ADDR      0x2000B4CCu
#define GPIOC_BASE_ADDR          0x40010C00u

/* [确认] TIM2 输入时钟按固件的 72 MHz 时钟配置解释。 */
#define TIM2_INPUT_HZ            72000000u
#define TIM2_PRESCALER           71u
#define TIM2_PERIOD              9999u

/* [确认] 72 MHz / (71 + 1) / (9999 + 1) = 100 Hz，即每 tick 10 ms。 */
#define SCHEDULER_TICK_MS         10u

/* ---------- 仅用于表达原固件副作用的外部接口 ---------- */

extern void gpio_set_bits(uint32_t gpio_base, uint16_t pins);
extern void gpio_reset_bits(uint32_t gpio_base, uint16_t pins);
extern void debug_log(const char *message);
extern uint32_t firmware_time_now(void);
extern bool shared_device_idle_gate(void);
extern bool liquid_input_stable(void);
extern void publish_dp_u8(uint8_t dp_id, uint8_t value);
extern void publish_dp_u16(uint8_t dp_id, uint16_t value);
extern void publish_dp_u32(uint8_t dp_id, uint32_t value);
extern void publish_dp_string(uint8_t dp_id, const char *value);
extern const char *build_device_text_value(void);

/* ========================================================================== */
/* 一、统一 10 ms 时基                                                     */
/* ========================================================================== */

/*
 * [确认] 0x0800C3B8 的等价初始化。
 * 原函数还包含清中断标志、设置 NVIC 通道 0x1C 等外设库调用，这里只保留业务关键值。
 */
static void tim2_scheduler_init(void)
{
    uint32_t prescaler = TIM2_PRESCALER;       /* 预分频寄存器写 71，即除以 72。 */
    uint32_t period = TIM2_PERIOD;             /* 自动重装写 9999，即每 10000 个计数溢出。 */
    uint32_t timer_counter_hz =                /* 72 MHz 经预分频后得到 1 MHz。 */
        TIM2_INPUT_HZ / (prescaler + 1u);
    uint32_t update_hz =                       /* 1 MHz 每 10000 个计数更新一次。 */
        timer_counter_hz / (period + 1u);

    (void)update_hz;                           /* 结果为 100 Hz；这里只作为解释性计算。 */
    /* 原固件随后启用 TIM2 更新中断和 NVIC IRQ 0x1C。 */
}

/*
 * [确认] 0x0800C6D0：TIM2 更新中断。
 * 中断本身不跑清理状态机，只投递一个“有新 tick”标记并维护少数底层计数器。
 */
static volatile uint8_t scheduler_pending;
static volatile uint16_t irq_counter_14;
static volatile uint16_t irq_counter_16;
static volatile uint16_t irq_counter_1a;
static volatile uint16_t irq_counter_1c;

static void TIM2_IRQHandler_equivalent(void)
{
    /* 原指令先检查 TIM2 更新标志；没有更新标志就直接返回。 */
    /* 原指令再清除更新标志，防止同一个中断被重复处理。 */
    scheduler_pending++;                       /* 0x20000004：告诉主循环有 tick 待处理。 */
    irq_counter_14++;                          /* 0x20000014：底层周期计数器。 */
    irq_counter_16++;                          /* 0x20000016：底层周期计数器。 */
    irq_counter_1a++;                          /* 0x2000001A：底层周期计数器。 */
    irq_counter_1c++;                          /* 0x2000001C：底层周期计数器。 */
    /* 原函数末尾还调用若干底层周期处理器；其业务名仍未确认。 */
}

typedef struct {
    uint8_t protection_a;                      /* [推断] 保护/暂停条件 A。 */
    uint8_t protection_b;                      /* [推断] 保护/暂停条件 B。 */
    uint8_t protection_c;                      /* [推断] 保护/暂停条件 C。 */
    uint32_t clean_tick;                       /* 0x2000A91C：22 步清理状态机的计时。 */
    uint32_t roller_run_ticks;                 /* 电机连续运行时间。 */
    uint32_t cat_absent_ticks;                 /* 猫离开后经过的时间。 */
    uint8_t slow_divider;                      /* 每 100 tick 触发一次慢任务。 */
} SchedulerBusinessCounters;

/* [确认] 0x0800C418 的关键业务等价物。 */
static void scheduler_consume_tick(SchedulerBusinessCounters *counters)
{
    if (scheduler_pending == 0u)               /* 主循环没有收到新 tick。 */
        return;                                /* 什么也不做，避免时间凭空前进。 */

    scheduler_pending = 0u;                    /* 原固件按字节清零，消费本轮 tick。 */

    /* [确认] 三个保护字节都为 0 时，清理状态机的计时才会继续。 */
    if (counters->protection_a == 0u &&
        counters->protection_b == 0u &&
        counters->protection_c == 0u) {
        counters->clean_tick++;                /* 保护生效时不是重置，而是“冻结清理时钟”。 */
    }

    counters->roller_run_ticks++;              /* 电机监控计时继续更新。 */
    counters->cat_absent_ticks++;              /* 猫离开计时在该函数末尾无条件增加。 */
    counters->slow_divider++;                  /* 累积慢任务分频。 */

    if (counters->slow_divider >= 100u) {      /* 100 × 10 ms = 1 秒。 */
        counters->slow_divider = 0u;           /* 开始下一秒的计数。 */
        /* 原固件在这里调用多个低频维护函数。 */
    }
}

/* ========================================================================== */
/* 二、两路输入消抖                                                         */
/* ========================================================================== */

typedef struct {
    uint8_t unused_00;                         /* 原结构偏移 +0，意义尚未确认。 */
    uint8_t raw_equal_active;                  /* +1：原始电平与配置有效极性比较后的结果。 */
    uint8_t unused_02;                         /* +2：本函数未使用。 */
    uint8_t state;                             /* +3：0=稳定低，1=确认高，2=稳定高。 */
    uint8_t stable;                            /* +4：交给业务状态机读取的去抖值。 */
    uint8_t unused_05;                         /* +5：本函数未使用。 */
    uint16_t counter;                          /* +6：进入候选状态后的 10 ms 计数。 */
    uint8_t remaining[8];                      /* 每通道总步长 16 字节。 */
} DebouncedInput;

/* [确认] 0x08015330 对索引 0、1 各执行一次该状态机。 */
static void debounce_one_input(DebouncedInput *input)
{
    switch (input->state) {                    /* 根据当前消抖阶段选择处理分支。 */
    case 0u:                                   /* 当前认为输入稳定为低。 */
        if (input->raw_equal_active != 0u) {   /* 第一次看见有效高电平。 */
            input->state = 1u;                 /* 转入“确认高”状态。 */
        } else {
            input->stable = 0u;                /* 仍为低，输出保持低。 */
        }
        break;

    case 1u:                                   /* 正在确认高电平是否持续。 */
        if (input->raw_equal_active == 0u) {   /* 高电平没有持续，属于抖动。 */
            input->stable = 0u;                /* 对业务层仍报告低。 */
            input->state = 0u;                 /* 回到稳定低。 */
            input->counter = 0u;               /* 清除本轮确认时间。 */
        } else if (input->counter > 10u) {     /* 有效高超过约 100 ms。 */
            input->stable = 1u;                /* 对业务层正式报告高。 */
            input->state = 2u;                 /* 进入稳定高。 */
        }
        break;

    case 2u:                                   /* 当前认为输入稳定为高。 */
        if (input->raw_equal_active == 0u) {   /* [确认] 下降沿不再等待 100 ms。 */
            input->stable = 0u;                /* 立即对业务层报告低。 */
            input->state = 0u;                 /* 回到稳定低。 */
            input->counter = 0u;               /* 为下次上升沿重新计时。 */
        }
        break;

    default:                                   /* RAM 异常或未初始化。 */
        input->state = 0u;                     /* 伪代码采用安全回退；原跳转表会返回。 */
        break;
    }
}

/* [确认] 计时更新函数会给两路输入的 +6 计数器递增。 */
static void debounce_tick(DebouncedInput inputs[2])
{
    inputs[0].counter++;                       /* 第一路候选状态时间。 */
    inputs[1].counter++;                       /* 第二路候选状态时间。 */
}

/* ========================================================================== */
/* 三、除臭液余量状态机                                                     */
/* ========================================================================== */

typedef enum {
    LIQUID_NORMAL = 0,                         /* 传感器正常。 */
    LIQUID_INSUFFICIENT = 1,                   /* 连续缺液 3 秒。 */
    LIQUID_EXHAUSTED = 2                       /* 缺液累计 72 分钟。 */
} LiquidStatus;

typedef struct {
    uint8_t request_or_unused;                 /* +0：该函数未完全解释。 */
    uint8_t state;                             /* +1：0/1/2 三态状态机。 */
    uint8_t exhausted_log_latch;               /* +2：控制 exhausted 日志只报一次。 */
} LiquidMonitor;

typedef struct {
    uint32_t counter;                          /* +0：连续输入确认计数。 */
    uint32_t insufficient_since;               /* +4：进入缺液状态的时间戳。 */
    uint8_t active;                            /* +8：计时活动标志。 */
} LiquidTimer;

typedef struct {
    uint8_t bytes[0x96];                       /* 用偏移保持与固件证据一一对应。 */
} DeviceStateRaw;

static uint8_t *device_u8(DeviceStateRaw *device, uint32_t offset)
{
    return &device->bytes[offset];             /* 教学辅助：访问原 RAM 结构的某个字节偏移。 */
}

static uint32_t device_u32(const DeviceStateRaw *device, uint32_t offset)
{
    const uint8_t *p = &device->bytes[offset]; /* Cortex-M 小端序读取 4 个连续字节。 */
    return (uint32_t)p[0] |                     /* 最低有效字节。 */
           ((uint32_t)p[1] << 8u) |             /* 次低字节。 */
           ((uint32_t)p[2] << 16u) |            /* 次高字节。 */
           ((uint32_t)p[3] << 24u);             /* 最高有效字节。 */
}

/* [确认] 0x0800F608 的主分支；调用入口位于 0x0800F5D8。 */
static void liquid_monitor_step(LiquidMonitor *monitor,
                                LiquidTimer *timer,
                                DeviceStateRaw *device)
{
    bool liquid_ok = liquid_input_stable();    /* 读取已经消抖的液位输入。 */
    uint8_t *liquid_status = device_u8(device, 0x55u); /* +0x55：上报用液位状态。 */

    switch (monitor->state) {                  /* 根据液位状态机阶段运行。 */
    case 0u:                                   /* 正常监测。 */
        if (!liquid_ok) {                      /* 第一次观察到缺液。 */
            monitor->state = 1u;               /* 转入 3 秒确认。 */
            timer->counter = 0u;               /* 从零开始计时。 */
        } else {
            *liquid_status = LIQUID_NORMAL;    /* 对外状态保持正常。 */
            timer->active = 0u;                /* 没有长期缺液计时。 */
        }
        break;

    case 1u:                                   /* 确认是否真的缺液。 */
        if (liquid_ok) {                       /* 3 秒内恢复，判定为短暂波动。 */
            monitor->state = 0u;               /* 回到正常监测。 */
            debug_log("liquid_insuff exit");   /* 固件中的原始日志字符串。 */
        } else if (timer->counter >= 300u) {   /* 300 × 10 ms = 3 秒。 */
            *liquid_status = LIQUID_INSUFFICIENT; /* 标记“液体不足”。 */
            timer->insufficient_since = firmware_time_now(); /* 记录不足开始时刻。 */
            timer->active = 1u;                /* 启动长期缺液计时。 */
            debug_log("liquid_insuff");        /* 固件中的原始日志字符串。 */
            monitor->state = 2u;               /* 进入长期缺液监测。 */
        }
        break;

    case 2u:                                   /* 已确认液体不足。 */
        if (liquid_ok && timer->counter >= 300u) { /* 恢复也要求连续约 3 秒。 */
            monitor->state = 0u;               /* 回到正常状态。 */
            monitor->exhausted_log_latch = 1u; /* 允许下一轮 exhausted 日志。 */
            debug_log("liquid_exh exit");      /* 固件中的原始日志字符串。 */
        } else if (!liquid_ok &&
                   firmware_time_now() - timer->insufficient_since >= 0x69780u) {
            *liquid_status = LIQUID_EXHAUSTED; /* 432000 tick = 72 分钟。 */

            if (*device_u8(device, 0x18u) == 1u) /* [确认] 某项作业若为 1 就被取消。 */
                *device_u8(device, 0x18u) = 0u; /* [推断] 可能是除臭动作许可。 */

            if (monitor->exhausted_log_latch != 0u) { /* 只记录一次耗尽事件。 */
                monitor->exhausted_log_latch = 0u; /* 消耗日志闩锁。 */
                debug_log("liquid_exh");       /* 固件中的原始日志字符串。 */
            }
        }
        break;

    default:
        monitor->state = 0u;                   /* 教学版安全回退。 */
        break;
    }
}

/* ========================================================================== */
/* 四、电磁阀状态机                                                         */
/* ========================================================================== */

#define SOLENOID_PC5_PIN          0x0020u       /* [确认] GPIOC bit 5。 */
#define SOLENOID_PC13_PIN         0x2000u       /* [确认] GPIOC bit 13。 */
#define SOLENOID_DURATION_TICKS   3000u         /* [确认] 3000 × 10 ms = 30 秒。 */

typedef struct {
    uint8_t manual_request;                    /* +0：某一路开阀请求。 */
    uint8_t last_special_state;                /* +1：保存上次特殊状态。 */
    uint8_t state;                             /* +2：0=空闲，1=阀已开启。 */
} SolenoidControl;

typedef struct {
    uint8_t active;                            /* +0：计时是否活动。 */
    uint8_t padding[7];                        /* 保持 ticks 位于 +8。 */
    uint32_t ticks;                            /* +8：开阀持续时间。 */
} SolenoidTimer;

static void solenoid_write(bool on)
{
    uint16_t both_pins = SOLENOID_PC5_PIN | SOLENOID_PC13_PIN; /* 两个输出同步动作。 */

    if (on) {                                  /* 请求开启电磁阀。 */
        gpio_set_bits(GPIOC_BASE_ADDR, both_pins); /* [确认] 原固件同时置位 PC5/PC13。 */
        debug_log("SOLENOID_VALVE_ON");         /* 原始日志证据。 */
    } else {                                   /* 请求关闭电磁阀。 */
        gpio_reset_bits(GPIOC_BASE_ADDR, both_pins); /* [确认] 同时复位 PC5/PC13。 */
        debug_log("SOLENOID_VALVE_OFF");        /* 原始日志证据。 */
    }
}

/* [确认到分支层级] 0x080151BC 的核心控制流程。 */
static void solenoid_step(SolenoidControl *control,
                          SolenoidTimer *timer,
                          bool special_state_changed)
{
    if (control->state == 0u) {                /* 当前阀门空闲。 */
        bool requested = control->manual_request != 0u || special_state_changed;

        if (!requested)                        /* 没有人工请求，也没有特殊状态变化。 */
            return;                            /* 阀保持关闭。 */
        if (!shared_device_idle_gate())        /* 多项设备忙碌/故障位必须全部为 0。 */
            return;                            /* 不满足许可条件，拒绝启动。 */

        timer->ticks = 0u;                     /* 开阀计时从零开始。 */
        timer->active = 1u;                    /* 标记计时器工作中。 */
        solenoid_write(true);                  /* 实际驱动 PC5 和 PC13。 */
        control->state = 1u;                   /* 记住阀已开启。 */
        control->manual_request = 0u;          /* 消费一次性人工请求。 */
        return;
    }

    if (control->state == 1u && timer->ticks >= SOLENOID_DURATION_TICKS) {
        solenoid_write(false);                 /* 30 秒到，关闭两个输出。 */
        timer->active = 0u;                    /* 停止计时。 */
        control->state = 0u;                   /* 回到空闲。 */
    }
}

/* ========================================================================== */
/* 五、称重与猫进出事件：已恢复主判定，细节修正分支仍在继续                    */
/* ========================================================================== */

typedef struct {
    float baseline_before;                     /* +0x28：[推断] 进入判断使用的基线。 */
    float baseline_after;                      /* +0x2C：[推断] 离开判断使用的基线。 */
    float stable_weight;                       /* +0x30：[确认] 当前稳定重量。 */
    float detected_cat_weight;                 /* +0x38：[确认] 进盆时保存的猫重量。 */
    float weight_change;                       /* +0x3C：[确认] 本轮重量变化。 */
} WeightWorkView;

typedef enum {
    WEIGHT_NO_EVENT = 0,
    WEIGHT_CAT_ENTERED = 1,
    WEIGHT_CAT_LEFT = 2,
    WEIGHT_CHANGED = 3
} WeightEvent;

/*
 * [部分确认] 0x08006660 的教学化主干。
 * 原函数还有平均、四舍五入、异常重量修正和多组上报分支，不能假装已经逐指令恢复完。
 */
static WeightEvent classify_weight_event(const WeightWorkView *weight,
                                         float configured_threshold_grams,
                                         uint8_t transition_state,
                                         bool event_inhibit)
{
    float threshold_kg = configured_threshold_grams / 1000.0f; /* 固件常量 1000.0。 */

    if (event_inhibit)                         /* device +0x56 不为 0 时抑制进盆事件。 */
        return WEIGHT_NO_EVENT;                /* 不改变猫状态。 */

    if (transition_state == 3u &&              /* [确认] 状态 3 处理重量增加。 */
        weight->stable_weight > weight->baseline_before &&
        weight->stable_weight - weight->baseline_before > threshold_kg) {
        return WEIGHT_CAT_ENTERED;             /* 对应固件日志 CAT_INTO。 */
    }

    if (transition_state == 4u &&              /* [确认] 状态 4 处理重量下降/变化。 */
        weight->baseline_after > weight->stable_weight) {
        float decrease = weight->baseline_after - weight->stable_weight;
        float mismatch = decrease - weight->detected_cat_weight;

        if (mismatch < 0.0f)                   /* 取绝对值，比较 0.3 kg 容差。 */
            mismatch = -mismatch;
        if (mismatch <= 0.3f)                  /* 固件浮点常量 0.3。 */
            return WEIGHT_CAT_LEFT;            /* 变化量与进盆猫重量相符。 */
        return WEIGHT_CHANGED;                 /* 可能加砂、铲砂或基线漂移。 */
    }

    return WEIGHT_NO_EVENT;                    /* 没有达到已确认事件条件。 */
}

/* ========================================================================== */
/* 六、设备状态到 App DP 的完整快照映射                                    */
/* ========================================================================== */

/*
 * [确认] 0x0800FCD0 发布的 ID、数据源偏移和类型。
 * DP 的产品中文名称多数尚未通过 App 实机抓包确认，因此这里不擅自命名。
 */
static void publish_device_snapshot(DeviceStateRaw *device)
{
    publish_dp_u8(0x17u, *device_u8(device, 0x08u)); /* DP 0x17 ← device +0x08。 */
    publish_dp_u8(0x67u, *device_u8(device, 0x0Du)); /* DP 0x67 ← device +0x0D。 */
    publish_dp_u8(0x68u, *device_u8(device, 0x78u)); /* DP 0x68 ← device +0x78。 */
    publish_dp_u8(0x69u, *device_u8(device, 0x31u)); /* DP 0x69 ← device +0x31。 */
    publish_dp_string(0x6Cu, build_device_text_value()); /* DP 0x6C ← 编码字符串。 */
    publish_dp_u8(0x6Du, *device_u8(device, 0x55u)); /* DP 0x6D ← 液位 0/1/2。 */
    publish_dp_u8(0x6Fu, *device_u8(device, 0x95u)); /* DP 0x6F ← device +0x95。 */
    publish_dp_u8(0x7Cu, *device_u8(device, 0x72u)); /* DP 0x7C ← device +0x72。 */
    publish_dp_u8(0x70u, *device_u8(device, 0x10u)); /* DP 0x70 ← 猫在盆状态。 */
    publish_dp_u32(0x75u, device_u32(device, 0x34u) / 100u); /* DP 0x75 ← u32 +0x34/100。 */
    publish_dp_u8(0x7Au, *device_u8(device, 0x44u)); /* DP 0x7A ← device +0x44。 */
    publish_dp_u8(0x7Eu, *device_u8(device, 0x38u)); /* DP 0x7E ← device +0x38。 */
    publish_dp_u8(0x89u, *device_u8(device, 0x70u)); /* DP 0x89 ← device +0x70。 */
    publish_dp_u8(0x8Au, *device_u8(device, 0x6Eu)); /* DP 0x8A ← device +0x6E。 */
    publish_dp_u32(0x16u, device_u32(device, 0x74u)); /* DP 0x16 ← u32 故障位图。 */
}

/* ========================================================================== */
/* 七、两枚面板按键：短按、双击、长按和超长按                              */
/* ========================================================================== */

/*
 * [纠错说明]
 * 0x08013C64 曾因位于传感器处理调用链中而被暂称为“两路主动红外状态机”。
 * 继续追踪 0x08010C84 的两个函数指针输入、0x080111A0 的事件派发，以及
 * `button_%d short/double/long/long long` 日志后，可确认它实际是双按键识别器。
 */

typedef enum {
    BUTTON_SIGNAL_NONE = 0,                    /* 没有新事件。 */
    BUTTON_SIGNAL_SHORT = 2,                   /* 松开后形成短按候选。 */
    BUTTON_SIGNAL_PRESSED_STABLE = 4,          /* 按下约 100 ms，尚未形成对外事件。 */
    BUTTON_SIGNAL_LONG = 8,                    /* 持续约 3 秒。 */
    BUTTON_SIGNAL_LONG_LONG = 9                /* 持续约 7 秒。 */
} ButtonSignal;

typedef struct {
    uint8_t enabled;                           /* +0：该按键是否配置。 */
    uint8_t raw_active;                        /* +1：函数指针读值与有效极性比较后的结果。 */
    uint8_t active_level;                      /* +2：配置的有效极性。 */
    uint8_t state;                             /* +3：内部识别状态 0～9。 */
    uint8_t signal;                            /* +4：2/4/8/9 中间或输出信号。 */
    uint8_t unused_05;                         /* +5：当前函数未解释。 */
    uint16_t held_ticks;                       /* +6：只在 raw_active=1 时由周期函数增加。 */
    bool (*read_pin)(void);                    /* +8：按键 GPIO 读取函数指针。 */
} ButtonScanEntry;

typedef enum {
    BUTTON_EVENT_SHORT = 1,                    /* 日志：button_%d short。 */
    BUTTON_EVENT_DOUBLE = 2,                   /* 日志：button_%d double。 */
    BUTTON_EVENT_LONG = 3,                     /* 日志：button_%d long。 */
    BUTTON_EVENT_LONG_LONG = 4                 /* 日志：button_%d long long。 */
} ButtonEventType;

typedef struct {
    uint8_t type;                              /* 事件类型 1～4。 */
    uint8_t button_index;                      /* 0 或 1。 */
} ButtonEvent;

extern void dispatch_button_event(const ButtonEvent *event);

/* [确认] 0x08010C84：读取两枚按键，并按各自 active_level 归一化。 */
static void sample_two_buttons(ButtonScanEntry buttons[2])
{
    for (uint8_t index = 0u; index < 2u; ++index) { /* 固件固定循环两次。 */
        ButtonScanEntry *button = &buttons[index]; /* 取得本轮 12 字节按键结构。 */

        if (button->enabled == 0u)              /* 没有配置读取函数。 */
            continue;                           /* 跳过该通道。 */

        bool level = button->read_pin();        /* 通过结构里的函数指针读取 GPIO。 */
        button->raw_active =                    /* 不让上层关心高有效还是低有效。 */
            ((uint8_t)level == button->active_level) ? 1u : 0u;
    }
}

/* [确认] 0x080112A0：只有按住期间，held_ticks 才继续增加。 */
static void button_tick(ButtonScanEntry buttons[2], uint32_t gap_ticks[2])
{
    for (uint8_t index = 0u; index < 2u; ++index) { /* 更新两枚按键。 */
        if (buttons[index].raw_active == 1u)    /* 按键仍处于有效电平。 */
            buttons[index].held_ticks++;       /* 增加按住时间。 */
        gap_ticks[index]++;                     /* 双击窗口计数无条件增加。 */
    }
}

/*
 * [确认到主要状态] 0x08013C64：状态 2/3/5/6 由事件/双击协作逻辑使用，
 * 单看本函数不会改变它们，所以保留名称而不虚构转移。
 */
static void button_recognizer_step(ButtonScanEntry *button,
                                   uint8_t button_index,
                                   bool *long_long_global_latch)
{
    switch (button->state) {                   /* 原函数使用 10 项 TBB 跳转表。 */
    case 0u:                                   /* 等待按下。 */
        if (button->raw_active == 1u)           /* 看见有效电平。 */
            button->state = 1u;                /* 进入按下确认。 */
        else
            button->signal = BUTTON_SIGNAL_NONE; /* 没有事件。 */
        break;

    case 1u:                                   /* 确认按下已经持续。 */
        if (button->raw_active == 1u && button->held_ticks >= 10u) {
            button->state = BUTTON_SIGNAL_PRESSED_STABLE; /* 内部状态转为 4。 */
            button->signal = BUTTON_SIGNAL_PRESSED_STABLE; /* 约 100 ms 稳定按下。 */
            button->held_ticks = 0u;            /* 为长按阶段重新计时。 */
        } else {
            /* [确认] 原指令写入 button_index；两个按键初始子状态并不完全对称。 */
            button->state = button_index;
            button->signal = BUTTON_SIGNAL_NONE;
        }
        break;

    case 4u:                                   /* 已稳定按下，等待松开或长按。 */
        if (button->raw_active == 0u) {         /* 在 3 秒前松开。 */
            button->state = button_index;       /* 回到该通道的默认子状态。 */
            button->signal = BUTTON_SIGNAL_SHORT; /* 交给双击窗口继续判断。 */
        } else if (button->held_ticks >= 300u) { /* 再持续约 3 秒。 */
            button->state = BUTTON_SIGNAL_LONG; /* 内部状态转为 8。 */
            button->signal = BUTTON_SIGNAL_LONG; /* 对外形成 long 事件。 */
            button->held_ticks = 0u;            /* 开始计算超长按时间。 */
        } else {
            button->signal = BUTTON_SIGNAL_NONE; /* 仍按住但未到阈值。 */
        }
        break;

    case 7u:                                   /* [确认] 另一条外部协作的长按阶段。 */
        if (button->raw_active == 0u) {         /* 中途松开。 */
            button->state = button_index;       /* 回到默认子状态。 */
            button->signal = BUTTON_SIGNAL_NONE; /* 不生成新事件。 */
            *long_long_global_latch = false;    /* 清超长按全局闩锁。 */
        } else if (button->held_ticks >= 700u) { /* 连续约 7 秒。 */
            *long_long_global_latch = true;     /* 通知外部协作逻辑。 */
            button->signal = BUTTON_SIGNAL_LONG_LONG; /* 形成超长按信号 9。 */
            button->held_ticks = 0u;            /* 防止立即重复触发。 */
            debug_log("---> KEY_LONG_LONG");   /* 固件原始日志。 */
        }
        break;

    case 8u:                                   /* 长按事件已形成，继续观察。 */
        if (button->raw_active == 0u) {         /* 长按后松开。 */
            button->state = button_index;       /* 回到默认子状态。 */
            button->signal = BUTTON_SIGNAL_NONE; /* 清输出。 */
            *long_long_global_latch = false;    /* 清闩锁。 */
        } else if (button->held_ticks >= 700u) { /* 继续按住约 7 秒。 */
            if (!*long_long_global_latch)       /* 只在尚未上报时置 9。 */
                button->signal = BUTTON_SIGNAL_LONG_LONG;
            button->held_ticks = 0u;            /* 重置本阶段时间。 */
        } else {
            button->signal = BUTTON_SIGNAL_NONE; /* 未到超长按阈值。 */
        }
        break;

    case 9u:                                   /* 超长按后的清理状态。 */
        button->held_ticks = 0u;                /* [确认] 清按住计数。 */
        break;

    case 2u:                                   /* 双击协作中间态。 */
    case 3u:                                   /* 外部事件协作中间态。 */
    case 5u:                                   /* 外部事件协作中间态。 */
    case 6u:                                   /* 外部事件协作中间态。 */
    default:
        break;                                 /* 本函数不改变这些状态。 */
    }
}

/* [确认] 0x080111A0/0x0800E700 一带的最终事件语义。 */
static void emit_confirmed_button_event(uint8_t button_index, ButtonEventType type)
{
    ButtonEvent event;                         /* 构造两个字节的逻辑事件。 */
    event.type = (uint8_t)type;                /* 1短、2双、3长、4超长。 */
    event.button_index = button_index;          /* 0=一枚按键，1=另一枚按键。 */
    dispatch_button_event(&event);              /* 交给菜单/设备业务回调。 */
}

/* ========================================================================== */
/* 八、粪仓主动红外发射端开关                                               */
/* ========================================================================== */

#define GPIOB_BASE_ADDR             0x40010800u /* [确认] 原固件 GPIOB 基址。 */
#define INFRARED_FECES_EMITTER_PIN  0x0002u     /* [确认] GPIOB pin 1 掩码。 */
#define INFRARED_FORCE_TICKS        60u         /* [确认] 约 600 ms。 */

typedef struct {
    uint8_t unknown_00[2];                     /* +0～+1：本函数不读取。 */
    uint8_t force_request;                      /* +2：[推断] 临时开启/测试请求。 */
    uint8_t emitter_on;                         /* +3：[确认] 当前发射端开关状态。 */
    uint8_t unknown_04;                        /* +4：当前两函数未解释。 */
    uint8_t receiver_state;                    /* +5：0/1 两态接收确认。 */
    uint8_t position_state;                    /* +6：供 0x080158F4 返回的位置状态。 */
    uint8_t unknown_07[9];                     /* 保持 door_ticks 位于 +0x10。 */
    uint32_t door_ticks;                       /* +0x10：门口信号连续时间。 */
    uint32_t force_ticks;                       /* +0x14：临时开启计时。 */
} InfraredControl;

extern bool infrared_door_debounced_input(void);
extern bool infrared_door_secondary_input(void);

static void clear_device_u16(DeviceStateRaw *device, uint32_t offset)
{
    device->bytes[offset] = 0u;                /* 清半字的低 8 位。 */
    device->bytes[offset + 1u] = 0u;           /* 清半字的高 8 位。 */
}

/*
 * [确认到分支层级，物理名为推断] 0x08015874。
 * 日志巡检字段把 device +0x0D 称为 infrared_door；因此这里使用“门口红外”名称，
 * 但两个输入分别位于机器哪个位置，仍需遮挡实验确认。
 */
static void infrared_door_monitor_step(InfraredControl *control,
                                       DeviceStateRaw *device)
{
    if (control->emitter_on == 0u) {           /* 发射端未开启时不做有效检测。 */
        control->receiver_state = 0u;          /* 接收确认回到初始态。 */
        return;                                /* 等待发射端开启。 */
    }

    bool optical = infrared_door_debounced_input(); /* 第一条、已消抖的光路输入。 */
    bool secondary = infrared_door_secondary_input(); /* 第二条位置/光路输入。 */

    if (control->receiver_state == 0u) {       /* 尚未确认遮挡/触发。 */
        if (optical) {                         /* 第一次观察到有效信号。 */
            control->receiver_state = 1u;      /* 开始 2 秒确认阶段。 */
            control->door_ticks = 0u;          /* 从零计算连续时间。 */
            return;
        }

        if (secondary) {                       /* 第二输入处于固件认可的复位条件。 */
            device->bytes[0x0Du] = 0u;         /* 清 infrared_door 状态。 */
            clear_device_u16(device, 0x58u);   /* [确认] 同时清另一个 16 位计数/状态。 */
        }

        if (control->door_ticks >= 200u)       /* 200 × 10 ms = 2 秒。 */
            control->position_state = 0u;      /* 对应 0x08015900 的清零动作。 */
        return;
    }

    if (control->receiver_state == 1u) {       /* 已进入连续确认。 */
        if (optical) {                         /* 有效信号仍持续。 */
            if (control->door_ticks >= 200u)   /* 持续约 2 秒。 */
                device->bytes[0x0Du] = 1u;     /* 对外置 infrared_door 状态。 */
            return;
        }

        if (!optical && secondary) {           /* 光路恢复且第二输入允许复位。 */
            control->receiver_state = 0u;      /* 回到初始接收状态。 */
            device->bytes[0x0Du] = 0u;         /* 清门口红外状态。 */
            control->door_ticks = 0u;          /* 清连续时间。 */
            control->position_state = 0u;      /* 清供清理许可读取的位置状态。 */
        }
    }
}

static bool feces_ir_required_by_device(const DeviceStateRaw *device)
{
    /* [确认] 任一所列设备状态非零时，原函数保持红外发射端开启。 */
    return device->bytes[0x09u] != 0u ||
           device->bytes[0x1Au] != 0u ||
           device->bytes[0x0Bu] != 0u ||
           device->bytes[0x0Cu] != 0u ||
           device->bytes[0x71u] != 0u ||
           device->bytes[0x19u] != 0u ||
           device->bytes[0x0Du] != 0u;
}

/* [确认到分支层级] 0x0801590C。 */
static void infrared_feces_emitter_step(InfraredControl *control,
                                        const DeviceStateRaw *device)
{
    if (control->force_request == 1u &&         /* 临时请求正在活动。 */
        control->force_ticks >= INFRARED_FORCE_TICKS) {
        control->force_request = 0u;            /* 约 600 ms 后自动清请求。 */
        control->force_ticks = 0u;              /* 清本轮计时。 */
    }

    bool should_be_on =                         /* 两类条件都可以要求开启。 */
        control->force_request == 1u ||         /* 临时/测试请求。 */
        feces_ir_required_by_device(device);    /* 设备处于指定活动状态。 */

    if (should_be_on && control->emitter_on == 0u) {
        gpio_set_bits(GPIOB_BASE_ADDR, INFRARED_FECES_EMITTER_PIN); /* 开发射端。 */
        control->emitter_on = 1u;               /* 保存软件状态。 */
        debug_log("---> INFRARED_FECES_SWITCH_ON"); /* 固件原始日志。 */
        return;
    }

    if (!should_be_on && control->emitter_on != 0u) {
        gpio_reset_bits(GPIOB_BASE_ADDR, INFRARED_FECES_EMITTER_PIN); /* 关发射端。 */
        control->emitter_on = 0u;               /* 保存软件状态。 */
        debug_log("---> INFRARED_FECES_SWITCH_OFF"); /* 固件原始日志。 */
    }
}

/*
 * 当前恢复结论：
 * - 已从“状态机时间单位推测”推进到“TIM2 配置 + IRQ + 主循环消费”的完整证据链。
 * - 已恢复输入消抖、液位监测、电磁阀和 DP 快照的主要分支。
 * - 称重函数的进/出主条件已恢复，但异常修正、平均和所有上报分支仍未完成。
 * - 双按键短按/双击/长按/超长按链路已经恢复；两枚按键的物理名称仍需 PCB 确认。
 * - 粪仓红外发射端 PB1 的开关条件已经恢复；接收端和门口红外仍需继续命名。
 * - 显示/产测状态机、全部 DP 下行命令仍是下一批目标。
 */
