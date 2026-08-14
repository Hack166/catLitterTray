/*
 * WooHuloo 滚筒清理 22 步状态机——逐行教学伪代码
 *
 * 来源：cat_litter_firmware.bin 中 0x0800ACD4～0x0800B1DA 的 Thumb 机器码。
 * 性质：这是为了理解控制逻辑而重写的伪代码，不是原厂源码，也不能直接编译。
 * 证据：状态编号、比较常量、跳转方向、霍尔组合和电机命令均来自机器码。
 * 推断：1 tick 很可能是 10 ms；依据是 1800 tick 对应日志 DELAY 18，
 *       100 tick 对应 DELAY 1。仍应使用运行日志或定时器代码最终确认。
 */

#include <stdbool.h>                                  // 使用 bool 表达传感器真假。
#include <stdint.h>                                   // 使用固定宽度整数表达 RAM 字段。

enum MotorCommand {                                   // 电机命令字节的已确认枚举。
    MOTOR_IDLE = 0,                                   // 命令 0：空闲。
    MOTOR_DIRECTION_1 = 1,                            // 命令 1：PA9=0、PA10=1。
    MOTOR_DIRECTION_2 = 2,                            // 命令 2：PA9=1、PA10=0。
    MOTOR_RUNNING = 3,                                // 命令 3：分发器正在运行和监测。
    MOTOR_STOP = 4,                                   // 命令 4：PA9=0、PA10=0。
};                                                    // 枚举结束。

typedef struct {                                      // 只列出本函数实际使用的字段。
    uint8_t unknown_00;                               // +0x00：其他机械状态。
    uint8_t clean_step;                               // +0x01：本函数的状态号 0～21。
    uint8_t unknown_02[5];                            // +0x02～+0x06：其他状态字段。
    uint8_t phase_count;                              // +0x07：阶段循环计数，不只是“错误重试”。
    uint8_t recovery_path;                            // +0x08：0=正常；2/5=不同中断恢复路径。
    uint8_t unknown_09[9];                            // +0x09～+0x11：本函数未直接解释。
    uint16_t target_tick;                             // +0x12：恢复路径使用的目标 tick。
    uint8_t unknown_14[8];                            // +0x14～+0x1B：本函数未直接解释。
    uint32_t tick;                                    // +0x1C：当前步骤运行计数。
} CleanFsm;                                           // 状态机 RAM 结构结束。

static CleanFsm *const fsm = (CleanFsm *)0x2000A900u; // [确认] 状态机 RAM 基址。
static uint8_t *const reposition_request =            // [推断] 外部中断/重新定位请求。
    (uint8_t *)0x2000A9FDu;                           // [确认] 本函数读取和清零的地址。
static uint16_t *const cat_absent_ticks =              // [推断] 连续未检测到猫的计数。
    (uint16_t *)0x2000AA08u;                          // [确认] 状态 0 使用的 16 位计数地址。

static bool hall_A(void);                             // 0x08010CE8；精确组合 1000 时日志为 A。
static bool hall_C(void);                             // 0x08010D04；精确组合 0100 时日志为 C。
static bool hall_E(void);                             // 0x08010D20；精确组合 0010 时日志为 E。
static bool hall_F(void);                             // 0x08010D3C；精确组合 0001 时日志为 F。
static bool pir_active(void);                         // 0x08006C0C；读取去抖后的 PIR。
static void motor_request(uint8_t command);           // 0x0800BFF4；写电机命令字节。
static void motor_state_mirror(uint8_t value);        // 0x0800BFE8；写电机状态镜像字节。
static void clear_cat_protection_stall(void);         // 0x0800E934；清除 protection+2。
static void set_cat_protection_stall(void);           // 0x0800E994；设置 protection+2 并打印日志。
static void finish_clean_and_report(void);            // 表示状态 21 中的清理完成和 DP 上报。

static bool at_A(void)                                // 判断滚筒是否处于 A 的精确霍尔组合。
{                                                     // 函数体开始。
    return hall_A() && !hall_C() &&                   // A=1、C=0。
           !hall_E() && !hall_F();                    // E=0、F=0。
}                                                     // 函数体结束。

static bool at_C(void)                                // 判断滚筒是否处于 C 的精确霍尔组合。
{                                                     // 函数体开始。
    return !hall_A() && hall_C() &&                   // A=0、C=1。
           !hall_E() && !hall_F();                    // E=0、F=0。
}                                                     // 函数体结束。

static bool at_E(void)                                // 判断滚筒是否处于 E 的精确霍尔组合。
{                                                     // 函数体开始。
    return !hall_A() && !hall_C() &&                  // A=0、C=0。
           hall_E() && !hall_F();                     // E=1、F=0。
}                                                     // 函数体结束。

static bool at_F(void)                                // 判断滚筒是否处于 F 的精确霍尔组合。
{                                                     // 函数体开始。
    return !hall_A() && !hall_C() &&                  // A=0、C=0。
           !hall_E() && hall_F();                     // E=0、F=1。
}                                                     // 函数体结束。

static bool between_halls(void)                       // 判断四路霍尔是否全部为 0。
{                                                     // 函数体开始。
    return !hall_A() && !hall_C() &&                  // A=0、C=0。
           !hall_E() && !hall_F();                    // E=0、F=0。
}                                                     // 函数体结束。

static void stop_and_mirror(void)                     // 原函数经常同时写两个 RAM 状态字节。
{                                                     // 函数体开始。
    motor_request(MOTOR_STOP);                        // 请求电机分发器执行停止。
    motor_state_mirror(MOTOR_STOP);                   // 同步保存的电机状态镜像。
}                                                     // 函数体结束。

static void clean_fsm_22_steps(void)                  // 对应机器码函数 0x0800ACD4。
{                                                     // 函数体开始。
    switch (fsm->clean_step) {                        // TBB 跳转表按状态号 0～21 分发。
    case 0:                                           // 状态 0：等待 PIR 连续安全。
        if (pir_active()) {                           // 如果又检测到活动目标，
            *cat_absent_ticks = 0;                    // 就把连续安全时间清零，
            return;                                   // 本轮不启动电机。
        }                                             // PIR 活动分支结束。
        if (*cat_absent_ticks < 1000)                 // 连续安全还没有达到 1000 tick，
            return;                                   // 继续等待；推断约为 10 秒。
        clear_cat_protection_stall();                 // 清除上一阶段的保护标记。
        motor_request(MOTOR_DIRECTION_1);             // 向电机层请求方向 1。
        fsm->clean_step = 1;                          // 下一步进入状态 1。
        fsm->recovery_path = 0;                       // 标记为正常路径。
        fsm->tick = 0;                                // 新状态从 0 开始计时。
        return;                                       // 本轮处理结束。

    case 1:                                           // 状态 1：方向 1 启动后的短等待。
        if (fsm->tick < 50)                           // 未达到 50 tick，
            return;                                   // 继续保持方向 1。
        fsm->clean_step = 2;                          // 达到后进入寻找 F 的状态 2。
        fsm->tick = 0;                                // 清零本步骤计时。
        return;                                       // 本轮处理结束。

    case 2:                                           // 状态 2：沿方向 1 寻找 F。
        if (*reposition_request == 1) {               // 如果外部要求中断并重新定位，
            *reposition_request = 0;                  // 先消费这个请求，
            fsm->target_tick = (uint16_t)(fsm->tick + 50); // 保存恢复路径的目标时间，
            stop_and_mirror();                        // 立即请求停止，
            fsm->clean_step = 6;                      // 转到统一的恢复启动状态，
            fsm->recovery_path = 2;                   // 记录“从状态 2 被中断”，
            fsm->tick = 0;                            // 清零新状态计时，
            return;                                   // 本轮处理结束。
        }                                             // 外部重新定位分支结束。
        if (!at_F() && fsm->tick < 777)               // 未到 F 且没有达到超时，
            return;                                   // 继续沿方向 1 旋转。
        fsm->clean_step = 3;                          // 到 F 或超时后进入状态 3，
        stop_and_mirror();                            // 请求停止滚筒，
        /* 原固件打印 "trigger F, tick"。 */          // 这是 F 命名的直接字符串证据。
        fsm->tick = 0;                                // 清零下一状态的计时。
        return;                                       // 本轮处理结束。

    case 3:                                           // 状态 3：在 F 停留，然后反向。
        if (fsm->tick < 100)                          // 未达到 100 tick，
            return;                                   // 保持停止。
        fsm->clean_step = 4;                          // 下一步寻找 E，
        fsm->tick = 0;                                // 清零计时，
        motor_request(MOTOR_DIRECTION_2);             // 启动方向 2。
        return;                                       // 本轮处理结束。

    case 4:                                           // 状态 4：沿方向 2 寻找 E。
        if (!at_E() && fsm->tick < 330)               // 未到 E 且没有超时，
            return;                                   // 继续旋转。
        if (*reposition_request != 1) {               // 正常路径没有外部中断时，
            fsm->clean_step = 5;                      // 继续寻找 C，
            fsm->tick = 0;                            // 清零计时，
            return;                                   // 电机保持方向 2，不在 E 停止。
        }                                             // 正常路径分支结束。
        *reposition_request = 0;                      // 消费外部重新定位请求，
        fsm->clean_step = 0;                          // 回到安全等待状态，
        stop_and_mirror();                            // 请求停止，
        fsm->phase_count = 0;                         // 清除阶段循环次数，
        *cat_absent_ticks = 0;                        // 重新要求一段连续安全时间，
        fsm->tick = 0;                                // 清零状态计时，
        return;                                       // 本轮处理结束。

    case 5:                                           // 状态 5：方向 2 继续寻找 C。
        if (*reposition_request == 1) {               // 如果途中收到重新定位请求，
            *reposition_request = 0;                  // 消费请求，
            fsm->target_tick = (uint16_t)fsm->tick;   // 保存当前时间作为恢复参考，
            stop_and_mirror();                        // 请求停止，
            fsm->clean_step = 6;                      // 转入恢复状态，
            fsm->recovery_path = 5;                   // 记录“从状态 5 被中断”，
            fsm->tick = 0;                            // 清零新状态计时，
            return;                                   // 本轮处理结束。
        }                                             // 外部请求分支结束。
        if (!at_C() && fsm->tick < 1310)              // 未到 C 且没有超时，
            return;                                   // 继续方向 2。
        /* 原固件打印 "trigger C, tick"。 */          // 这是 C 命名的直接字符串证据。
        fsm->tick = 0;                                // 到 C 或超时后清零计时，
        fsm->phase_count++;                           // 记录完成一次 F→E→C 扫动。
        if (fsm->phase_count < 2) {                   // 如果还没有完成两次，
            fsm->target_tick = 918;                   // 设置返回 E 的超时参考，
            fsm->clean_step = 6;                      // 进入停顿和反向状态，
            stop_and_mirror();                        // 在 C 先停止，
            return;                                   // 本轮处理结束。
        }                                             // 第一次扫动分支结束。
        fsm->clean_step = 9;                          // 两次完成后进入离开 C 的状态 9，
        fsm->phase_count = 0;                         // 为下一机械阶段清零计数，
        return;                                       // 注意：此处不停止，方向 2 继续运行。

    case 6:                                           // 状态 6：恢复/折返前停留。
        if (fsm->tick < 100)                          // 未达到 100 tick，
            return;                                   // 保持停止。
        fsm->clean_step = 7;                          // 下一步寻找 E，
        fsm->tick = 0;                                // 清零计时。
        if (fsm->recovery_path == 2)                  // 如果从状态 2 中断，
            motor_request(MOTOR_DIRECTION_2);         // 就用方向 2 返回 E；
        else                                          // 否则是正常 C 折返或从状态 5 中断，
            motor_request(MOTOR_DIRECTION_1);         // 使用方向 1 返回 E。
        return;                                       // 本轮处理结束。

    case 7:                                           // 状态 7：寻找 E 并收束恢复路径。
        if (!at_E() &&                                // 如果还没有到 E，
            fsm->tick < (uint32_t)fsm->target_tick + 100) // 且没有达到保存的超时，
            return;                                   // 继续当前方向。
        fsm->tick = 0;                                // 到 E 或超时后清零计时，
        fsm->clean_step = 0;                          // 返回状态 0，
        stop_and_mirror();                            // 请求停止。
        if (fsm->recovery_path != 2 &&                // 正常的 C→E 折返，
            fsm->recovery_path != 5 &&                // 不是中断恢复，
            *reposition_request != 1) {               // 也没有新的外部请求时，
            *cat_absent_ticks = 1000;                 // 允许状态 0 立即开始第二次扫动，
            return;                                   // 保留 phase_count=1。
        }                                             // 正常折返分支结束。
        *reposition_request = 0;                      // 中断恢复时清除新请求，
        fsm->recovery_path = 0;                       // 清除恢复路径，
        *cat_absent_ticks = 0;                        // 重新等待猫离开安全时间，
        fsm->phase_count = 0;                         // 放弃当前阶段的循环次数，
        return;                                       // 本轮处理结束。

    case 8:                                           // 状态 8：只能确认是外部进入的短恢复状态。
        if (fsm->tick < 100)                          // 未达到 100 tick，
            return;                                   // 继续等待。
        fsm->clean_step = 0;                          // 返回状态 0，
        fsm->tick = 0;                                // 清零计时，
        return;                                       // 本函数内没有发现进入状态 8 的跳转。

    case 9:                                           // 状态 9：继续方向 2，确认已经离开 C。
        if (!between_halls() && fsm->tick < 130)      // 仍压住某个霍尔且没有超时，
            return;                                   // 继续方向 2。
        fsm->clean_step = 10;                         // 离开霍尔区后开始寻找 A，
        fsm->tick = 0;                                // 清零计时，
        return;                                       // 本轮处理结束。

    case 10: {                                        // 状态 10：沿方向 2 寻找 A。
        uint32_t base =                               // 根据循环次数选择不同超时基准，
            fsm->phase_count ? 2362u : 2863u;         // 第一次 2863，后续 2362。
        if (!at_A() && fsm->tick < base + 300u)       // 未到 A 且未达到 3163/2662 tick，
            return;                                   // 继续方向 2。
        /* 原固件打印 "trigger A, tick"。 */          // 这是 A 命名的直接字符串证据。
        fsm->clean_step = 11;                         // 到 A 或超时后进入状态 11，
        fsm->tick = 0;                                // 清零计时，
        return;                                       // 电机暂时仍保持方向 2。
    }                                                 // 状态 10 的局部作用域结束。

    case 11:                                          // 状态 11：越过 A 一小段后停止。
        if (fsm->tick < 100)                          // 未达到 100 tick，
            return;                                   // 方向 2 继续一小段。
        stop_and_mirror();                            // 到时停止，
        fsm->clean_step = 12;                         // 进入停止等待状态，
        fsm->tick = 0;                                // 清零计时，
        /* 原固件打印 "DELAY 3"。 */                  // 状态 11+12 合计 300 tick，推断约 3 秒。
        return;                                       // 本轮处理结束。

    case 12:                                          // 状态 12：在 A 附近停止等待后反向。
        if (fsm->tick < 200)                          // 未再等待 200 tick，
            return;                                   // 保持停止。
        motor_request(MOTOR_DIRECTION_1);             // 启动方向 1，
        fsm->tick = 0;                                // 清零计时，
        fsm->phase_count++;                           // 记录 A 附近反复动作次数。
        if (fsm->phase_count < 2)                     // 第一次反向时，
            fsm->clean_step = 13;                     // 进入 18 秒运行段；
        else {                                        // 第二次反向时，
            fsm->clean_step = 15;                     // 跳到继续寻找 E 的阶段，
            fsm->phase_count = 0;                     // 清除循环计数。
        }                                             // 次数判断结束。
        set_cat_protection_stall();                   // 设置原固件名为 cat_protection_stall 的标记。
        return;                                       // 本轮处理结束。

    case 13:                                          // 状态 13：方向 1 保持 1800 tick。
        if (fsm->tick < 1800)                         // 没达到 1800 tick，
            return;                                   // 继续方向 1。
        fsm->clean_step = 14;                         // 到时进入状态 14，
        fsm->tick = 0;                                // 清零计时，
        stop_and_mirror();                            // 请求停止，
        /* 原固件打印 "DELAY 18"。 */                 // 强烈提示 1800 tick 约为 18 秒。
        clear_cat_protection_stall();                 // 清除 stall 标记，
        return;                                       // 本轮处理结束。

    case 14:                                          // 状态 14：短暂停止后再次向 A。
        if (fsm->tick < 100)                          // 未达到 100 tick，
            return;                                   // 保持停止。
        motor_request(MOTOR_DIRECTION_2);             // 再次启动方向 2，
        fsm->tick = 0;                                // 清零计时，
        fsm->clean_step = 10;                         // 回到状态 10 再找一次 A，
        return;                                       // 本轮处理结束。

    case 15:                                          // 状态 15：第二次从 A 反向后的定时运行。
        if (fsm->tick < 600)                          // 未达到 600 tick，
            return;                                   // 继续方向 1。
        fsm->clean_step = 16;                         // 到时开始寻找 E，
        fsm->tick = 0;                                // 清零计时，
        return;                                       // 电机保持方向 1。

    case 16:                                          // 状态 16：沿方向 1 寻找 E。
        if (!at_E() && fsm->tick < 3603)              // 未到 E 且没有达到超时，
            return;                                   // 继续方向 1。
        fsm->clean_step = 17;                         // 到 E 或超时后进入寻找 F，
        fsm->phase_count = 0;                         // 清除循环计数，
        /* 原固件打印 "trigger E, tick"。 */          // 这是 E 命名的直接字符串证据。
        fsm->tick = 0;                                // 清零计时，
        clear_cat_protection_stall();                 // 清除 stall 标记，
        return;                                       // 电机继续方向 1。

    case 17:                                          // 状态 17：方向 1 继续寻找 F。
        if (!at_F() && fsm->tick < 924)               // 未到 F 且没有超时，
            return;                                   // 继续方向 1。
        fsm->clean_step = 18;                         // 到 F 或超时后进入状态 18，
        /* 原固件打印 "trigger F, tick"。 */          // 再次确认 F 的霍尔组合。
        fsm->tick = 0;                                // 清零计时，
        return;                                       // 此处暂时没有停止。

    case 18:                                          // 状态 18：越过 F 一小段后停止。
        if (fsm->tick < 100)                          // 未达到 100 tick，
            return;                                   // 方向 1 继续一小段。
        fsm->tick = 0;                                // 清零计时，
        fsm->clean_step = 19;                         // 进入停止等待状态，
        stop_and_mirror();                            // 请求停止，
        /* 原固件打印 "DELAY 1"。 */                  // 强烈提示 100 tick 约为 1 秒。
        return;                                       // 本轮处理结束。

    case 19:                                          // 状态 19：停止 100 tick 后反向。
        if (fsm->tick < 100)                          // 未达到 100 tick，
            return;                                   // 保持停止。
        fsm->tick = 0;                                // 清零计时，
        fsm->clean_step = 20;                         // 下一步寻找 E，
        motor_request(MOTOR_DIRECTION_2);             // 启动方向 2，
        return;                                       // 本轮处理结束。

    case 20:                                          // 状态 20：沿方向 2 返回 E。
        if (!at_E() && fsm->tick <= 520)              // 未到 E 且计时没有超过 520，
            return;                                   // 继续方向 2。
        fsm->clean_step = 21;                         // 到 E 或超时后进入结束等待，
        /* 原固件打印 "trigger E, tick"。 */          // 再次确认 E 的霍尔组合。
        fsm->tick = 0;                                // 清零计时，
        return;                                       // 电机暂时继续方向 2。

    case 21:                                          // 状态 21：最后越过 E 后完成清理。
        if (fsm->tick < 120)                          // 未达到 120 tick，
            return;                                   // 方向 2 再运行一小段。
        fsm->clean_step = 0;                          // 状态机回到初始状态，
        stop_and_mirror();                            // 最终停止滚筒，
        fsm->phase_count = 0;                         // 清除循环计数，
        *reposition_request = 0;                      // 清除外部重新定位请求，
        fsm->tick = 0;                                // 清零计时，
        clear_cat_protection_stall();                 // 清除 stall 标记，
        finish_clean_and_report();                    // 清 clean_state、上报 DP 0x65=0、置后续动作。
        return;                                       // 整个清理流程结束。
    }                                                 // switch 结束。
}                                                     // 函数体结束。

