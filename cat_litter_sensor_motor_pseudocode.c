/*
 * WooHuloo 传感器—滚筒电机联动伪代码
 *
 * 这是从 cat_litter_firmware.bin 进一步恢复的教学表达，不是原厂源码。
 * 重点地址：
 *   0x08008F88  滚筒方向 1
 *   0x08008FF0  滚筒方向 2
 *   0x08009010  滚筒停止
 *   0x08009030  电机命令分发器
 *   0x08009BAC  PIR 防护状态机
 *   0x0800ACD4  22 步清理状态机
 *   0x0800ED70  开始清理许可检查
 *   0x08010CE8 / 0x08010D04 / 0x08010D20 / 0x08010D3C  四路滚筒霍尔状态
 *
 * [确认] 由机器码、GPIO 地址、字符串或控制流直接支持。
 * [推断] 业务名称仍需 PCB、示波器或逻辑分析仪验证。
 */

#include <stdbool.h>
#include <stdint.h>

#define GPIOA_BASE                 0x40010800u
#define GPIO_SET_OFFSET            0x10u
#define GPIO_RESET_OFFSET          0x14u
#define ROLLER_PIN_1               0x0200u       /* GPIOA bit 9 */
#define ROLLER_PIN_2               0x0400u       /* GPIOA bit 10 */

#define CAT_PROTECTION_ADDR        0x2000A8C0u
#define ROLLER_CONTROL_ADDR        0x2000A8CCu
#define CLEAN_FSM_ADDR             0x2000A900u
#define DEVICE_STATE_ADDR          0x2000B500u
#define DEBOUNCED_INPUTS_ADDR      0x20000054u

typedef struct {
    uint8_t protection_bits;       /* +0x00；PIR 等模块把不同危险条件 OR 到这里 */
    uint8_t unknown_01;
    uint8_t stall_latched;         /* +0x02；0x0800E994 会置 1，并打印 cat_protection_stall */
} CatProtection;

typedef struct {
    uint8_t command;               /* +0x00：0=空闲，1=方向1，2=方向2，3=运行监控，4=停止 */
    uint8_t active_direction;      /* +0x01：保存启动时使用的方向命令 */
    uint8_t overcurrent_count;     /* +0x02：[推断] 电流/堵转检测状态 */
    uint8_t run_ticks;             /* +0x03：运行后计数，达到 150 才进入堵转检查 */
    uint8_t sample_count;          /* +0x04：[推断] 连续过阈值采样次数 */
    uint8_t unknown_05;
    uint16_t stall_count;          /* +0x06：[推断] 堵转事件计数 */
} RollerControl;

typedef struct {
    uint8_t main_step;             /* +0x00：其他业务步骤 */
    uint8_t clean_step;            /* +0x01：清理流程 0～21 */
    uint8_t litter_step;           /* +0x02：其他机械流程 */
    uint8_t deodorizer_step;       /* +0x03：其他机械流程 */
    uint8_t drawer_step;           /* +0x04：其他机械流程 */
    uint8_t unknown_05;
    uint8_t clean_count;
    uint8_t clean_retry;
    uint8_t clean_path;
    uint8_t unknown_09[9];
    uint16_t target_tick;          /* +0x12：部分步骤使用的目标计数 */
    uint8_t unknown_14[8];
    uint32_t tick;                 /* +0x1C：状态机累计计数 */
} MechanicalFsm;

typedef struct {
    uint8_t unknown_00[9];
    uint8_t clean_state;           /* +0x09：0=未清理，1=正在执行清理流程 */
    uint8_t post_clean_request;    /* +0x0A：清理完成后会置 1 */
    uint8_t operation_0b;
    uint8_t operation_0c;
    uint8_t unknown_0d[0x0B];
    uint8_t operation_18;
    uint8_t operation_19;
    uint8_t operation_1a;
    uint8_t unknown_1b[0x0B];
    uint8_t cat_or_weight_lock;    /* +0x26：[推断] 某些入口把值 1 视为禁止启动 */
    uint8_t unknown_27[0x1D];
    uint8_t cover_or_drawer_lock;  /* +0x44：[推断] 非零时某个入口拒绝清理 */
    uint8_t unknown_45[7];
    uint8_t operation_4c;
    uint8_t unknown_4d[9];
    uint8_t operation_56;
    uint8_t unknown_57[0x19];
    uint8_t operation_70;
    uint8_t fault_71;
} DeviceState;

static CatProtection *const protection = (CatProtection *)CAT_PROTECTION_ADDR;
static RollerControl *const roller = (RollerControl *)ROLLER_CONTROL_ADDR;
static MechanicalFsm *const fsm = (MechanicalFsm *)CLEAN_FSM_ADDR;
static DeviceState *const device = (DeviceState *)DEVICE_STATE_ADDR;

static void gpio_set_bits(uint32_t port, uint32_t bits)
{
    *(volatile uint32_t *)(port + GPIO_SET_OFFSET) = bits;
}

static void gpio_reset_bits(uint32_t port, uint32_t bits)
{
    *(volatile uint32_t *)(port + GPIO_RESET_OFFSET) = bits;
}

/* 0x08008F88：[确认] PA9=0、PA10=1。物理顺/逆时针取决于接线。 */
static void roller_direction_1(void)
{
    gpio_reset_bits(GPIOA_BASE, ROLLER_PIN_1);
    gpio_set_bits(GPIOA_BASE, ROLLER_PIN_2);
}

/* 0x08008FF0：[确认] PA10=0、PA9=1，与方向 1 相反。 */
static void roller_direction_2(void)
{
    gpio_reset_bits(GPIOA_BASE, ROLLER_PIN_2);
    gpio_set_bits(GPIOA_BASE, ROLLER_PIN_1);
}

/* 0x08009010：[确认] PA9=0、PA10=0。究竟是滑行还是制动取决于电机驱动芯片。 */
static void roller_stop(void)
{
    gpio_reset_bits(GPIOA_BASE, ROLLER_PIN_2);
    gpio_reset_bits(GPIOA_BASE, ROLLER_PIN_1);
}

/* 0x0800BFF4：业务代码不直接写 GPIO，只写一个命令字节。 */
static void roller_request(uint8_t command)
{
    roller->command = command;
}

/*
 * 0x08009030：主循环每轮调用的电机命令分发器。
 * 只有 protection_bits==0 才允许从停止状态启动一个方向。
 */
static void roller_process(void)
{
    switch (roller->command) {
    case 0:
        return;

    case 1:
        if (protection->protection_bits != 0)
            return;
        roller->overcurrent_count = 0;
        roller->run_ticks = 0;
        roller_direction_1();
        roller->active_direction = 1;
        roller->command = 3;
        return;

    case 2:
        if (protection->protection_bits != 0)
            return;
        roller->overcurrent_count = 0;
        roller->run_ticks = 0;
        roller_direction_2();
        roller->active_direction = 2;
        roller->command = 3;
        return;

    case 3:
        if (roller->run_ticks < 150)
            return;
        roller->run_ticks = 150;
        sub_0800BAF4_probable_stall_monitor(); /* ADC 阈值、连续采样、停机与故障记录 */
        sub_0801238C_publish_stall_state();
        return;

    case 4:
        roller_stop();
        roller->command = 0;
        return;
    }
}

/*
 * 四路去抖后的滚筒霍尔输入。
 * 固件日志已经把组合确认成 A=1000、C=0100、E=0010、F=0001；
 * 这里的 0/1/2/3 按日志输出顺序对应 A/C/E/F。物理安装位置仍需实机确认。
 */
static uint8_t roller_hall_A(void) { return *(uint8_t *)(DEBOUNCED_INPUTS_ADDR + 0x01); }
static uint8_t roller_hall_C(void) { return *(uint8_t *)(DEBOUNCED_INPUTS_ADDR + 0x09); }
static uint8_t roller_hall_E(void) { return *(uint8_t *)(DEBOUNCED_INPUTS_ADDR + 0x11); }
static uint8_t roller_hall_F(void) { return *(uint8_t *)(DEBOUNCED_INPUTS_ADDR + 0x19); }

/*
 * 0x0800ED70：一个“开始清理”入口的许可检查。
 * 这些偏移的业务名尚未全部确认，因此宁可保留 operation_xx，也不伪造名称。
 */
static bool try_start_clean(void)
{
    if (device->operation_0b != 0 || device->clean_state != 0 ||
        device->operation_1a != 0 || device->operation_19 != 0 ||
        device->operation_0c != 0 || device->fault_71 != 0 ||
        device->operation_4c != 0 || device->operation_56 != 0) {
        return false;
    }

    sub_080107F8_reset_entry_timer();

    /* 主动点亮一侧红外并重复读取另一侧；具体传感器用途待 PCB 确认。 */
    uint8_t optical_result = sub_08010C44_active_optical_check();
    uint8_t position_or_mode = sub_080158F4_get_state();
    debug_printf("entry check: %d, %d\r\n", optical_result, position_or_mode);

    if (optical_result > 1)
        return false;
    if (optical_result == 1 && position_or_mode < 3)
        return false;

    fsm_related_start_flag_reset();
    device->clean_state = 1;
    device->post_clean_request = 0;
    fsm->tick = 0;
    tuya_or_local_state_report(0x65, 1); /* 原产品 DP 0x65；自有产品应换成自有 API 字段 */
    return true;
}

/*
 * 0x0800ACD4：完整函数有 22 个 clean_step。
 * 下方保留总体结构；逐状态的精确教学还原见：
 * cat_litter_clean_fsm_22_states_annotated.c
 */
static void clean_fsm_step(void)
{
    if (device->clean_state != 1)
        return;

    switch (fsm->clean_step) {
    case 0:
        /* 等待初始安全/位置条件与约 1000 个 tick。 */
        if (initial_condition_ok()) {
            roller_request(1);
            fsm->clean_step = 1;
            fsm->tick = 0;
        }
        break;

    case 1:
        if (fsm->tick >= 50) {
            fsm->clean_step = 2;
            fsm->tick = 0;
        }
        break;

    case 2:
        /* 结合四路霍尔组合和超时决定停止或继续寻找位置。 */
        if (roller_hall_A() || roller_hall_C() ||
            roller_hall_E() || roller_hall_F()) {
            roller_request(4);
            fsm->clean_step = next_step_from_halls();
            fsm->tick = 0;
        }
        break;

    /*
     * 中间步骤反复执行：
     *   方向1/方向2 -> 等待 -> 读取霍尔组合 -> 停止 -> 再反向。
     * 固件日志确认的机械位置顺序包含：C -> A -> E -> F -> E。
     * 每一步同时有超时出口，避免某个霍尔损坏后电机永久旋转。
     */
    case 3:  case 4:  case 5:  case 6:  case 7:
    case 8:  case 9:  case 10: case 11: case 12:
    case 13: case 14: case 15: case 16: case 17:
    case 18: case 19: case 20:
        advance_cleaning_position_sequence();
        break;

    case 21:
        if (fsm->tick >= 120) {
            roller_request(4);
            device->clean_state = 0;
            fsm->clean_step = 0;
            fsm->clean_retry = 0;
            fsm->tick = 0;
            tuya_or_local_state_report(0x65, 0);
            device->post_clean_request = 1;
        }
        break;
    }
}

/*
 * 0x08009BAC：PIR 防护的关键联动片段。
 * PIR 进入活动态后设置保护位；若滚筒 command==3（正在运行），马上请求停止。
 */
static void pir_protection_step(void)
{
    bool pir_active = sub_08006C0C_get_debounced_pir();

    if (pir_active) {
        protection->protection_bits |= 0x40;

        if (roller->command == 3) {
            roller_request(4);
            set_cat_protection_status(1);
        }
    } else {
        protection->protection_bits &= (uint8_t)~0x40;
        set_cat_protection_status(0);
    }
}

/*
 * 最终控制链：
 *   原始 GPIO/称重 ADC
 *     -> 去抖、滤波、猫存在判断
 *     -> protection_bits / 四路 roller_hall
 *     -> clean_fsm_step() 只产生 1/2/4 命令
 *     -> roller_process()
 *     -> PA9/PA10
 *     -> 外部电机驱动芯片
 */
