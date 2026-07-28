/*
 * WooHuloo 猫砂盆固件——中文逐行注释版。
 *
 * 原始分析文件：cat_litter_firmware_pseudocode.c
 * 目标处理器：[推断] ARM Cortex-M4F / Thumb-2，大概率为国民技术 N32G45x。
 * Flash 映射基址：[确认] 0x08000000。
 * 固件版本：[确认] 2.3.3，编译时间 2026-01-28 10:05:01。
 *
 * 重要说明：这不是原厂源代码，也不能直接编译。
 * 本文件是对反汇编结果的人工伪 C 表达；sub_地址 是未知函数的地址占位名。
 *
 * 注释标签：
 * [确认] 可由机器码、向量表、常量或字符串直接支持。
 * [推断] 根据调用位置和相关证据推测，仍需运行时验证。
 * [未知] 当前固件片段不足以确定。
 * [风险] 移植到 ESP32-S3 时不能照搬，必须重新设计或测量。
 */

#include <stdbool.h>  // 使用 bool、true、false，便于表达成功/失败结果。
#include <stddef.h>   // 使用 size_t，表示缓冲区或数据长度。
#include <stdint.h>   // 使用 uint8_t、uint16_t、uint32_t 等定长整数类型。

#define FLASH_BASE             0x08000000u  // [确认] 原 MCU 内部 Flash 的映射起始地址。
#define BOOT_VECTOR            0x08000000u  // [确认] Bootloader 的中断向量表位于 Flash 开头。
#define APP_VECTOR             0x08004000u  // [确认] 应用程序向量表从 0x08004000 开始，即预留 16 KiB 给 Bootloader。
#define OTA_IMAGE_BASE         0x08037000u  // [确认] 下载的新固件暂存在这个 Flash 地址。
#define OTA_METADATA_PAGE      0x08069800u  // [确认] OTA 魔数和固件大小写入这个元数据页。
#define FLASH_PAGE_SIZE        0x800u       // [确认] 0x800 字节等于 2 KiB，是该方案使用的 Flash 页大小。
#define OTA_IMAGE_PAGE_COUNT   101u         // [确认] 暂存区擦除 101 页，范围约为 0x08037000～0x080697FF。
#define OTA_MAGIC              0xABCDABCDu  // [确认] Bootloader 用这个魔数判断是否有待安装的新固件。
#define IWDG_KEY_REGISTER      (*(volatile uint32_t *)0x40003000u) // [确认] 独立看门狗喂狗寄存器；volatile 防止编译器省略访问。

typedef struct {                              // 定义 Bootloader 与应用共享的 OTA 元数据结构。
    uint32_t magic;                           // [确认] OTA 有效标记，应等于 OTA_MAGIC。
    uint32_t image_size;                      // [确认] 待安装固件的字节数。
} OtaMetadata;                                // 给这个结构命名为 OtaMetadata。

// [确认] 原程序把 OTA 元数据的 RAM 镜像放在 0x200001AC。
static OtaMetadata *const g_ota_metadata =    // 声明一个不可改指向的全局指针，但指向的数据可以修改。
    (OtaMetadata *)0x200001acu;               // [风险] ESP32-S3 不能照搬这个绝对 RAM 地址，应改成普通对象或 RTC/NVS 数据。

typedef struct {                              // 描述应用接收 OTA 报文时的运行状态。
    uint8_t  command;                         // [确认] 偏移 +0x00；可能保存当前协议命令。
    uint8_t  unknown_01;                      // [未知] 偏移 +0x01 的字段含义尚未恢复。
    uint8_t  state;                           // [确认] 偏移 +0x02；接收过程中值为 0x0A。
    uint8_t  packet_active;                   // [推断] 偏移 +0x03；可能表示当前分包是否有效。
    uint8_t  unknown_04[4];                   // [未知] 偏移 +0x04～+0x07 的四个字节。
    uint16_t max_packet_size;                 // [确认] 偏移 +0x08；OTA 开始时被设置为 256 字节。
    uint8_t  unknown_0a[2];                   // [未知] 偏移 +0x0A～+0x0B，用于对齐或其他状态。
    uint32_t rolling_crc;                     // [确认] 偏移 +0x0C；累计计算的 CRC，初值为 0xFFFFFFFF。
    uint32_t transmitted_crc;                 // [确认] 偏移 +0x10；发送端在最后一个包附带的 CRC。
    uint32_t image_size;                      // [确认] 偏移 +0x14；整份新固件的总大小。
    uint32_t bytes_received;                  // [推断] 偏移 +0x18；记录已处理位置或字节数，精确定义待抓包验证。
} OtaReceiveState;                            // OTA 接收状态结构定义结束。

/* ============================ 启动流程 ============================ */

// [确认] 地址 0x080041B0：应用程序复位入口。
static void app_reset_handler(void)           // MCU 跳入应用向量表后，首先执行这个复位处理函数。
{                                             // app_reset_handler 函数体开始。
    sub_0800c160_SystemInit();                 // 初始化 CPU/FPU、时钟和向量表；函数名部分来自人工推断。
    sub_08004198_CRuntimeStart();              // 初始化 C 运行环境，然后进入应用主函数。
}                                             // app_reset_handler 函数体结束。

// [确认] 地址 0x0800C160：能看到 FPU、RCC 和 VTOR 相关写操作。
static void sub_0800c160_SystemInit(void)      // 系统级初始化函数；原始符号名未知。
{                                             // SystemInit 函数体开始。
    *(volatile uint32_t *)0xE000ED88u |=       // 访问 Cortex-M 的 CPACR 寄存器。
        0x00f00000u;                           // [确认] 开启 CP10/CP11，从而允许使用单精度浮点单元 FPU。
    sub_0800bd8c_clock_init();                 // [推断] 配置 RCC/系统时钟；具体晶振和倍频参数尚未完整恢复。
    *(volatile uint32_t *)0xE000ED08u =        // 写 Cortex-M 的向量表偏移寄存器 VTOR。
        0x08000000u;                           // [风险] 观察到写 Bootloader 基址；应用最终向量重定位过程仍需继续确认。
}                                             // SystemInit 函数体结束。

// [确认] 地址 0x08004198：结构类似 Keil 生成的 C 运行时入口。
static void sub_08004198_CRuntimeStart(void)   // 准备栈、初始化全局变量并调用应用主函数。
{                                             // CRuntimeStart 函数体开始。
    set_sp(*(uint32_t *)0x080041acu);          // [确认] 从镜像常量读取 0x2000E3D8，并把它设置为栈指针。
    sub_080049f4_scatterload();                // [推断] 将已初始化数据复制到 RAM，并清零 BSS 区域。
    app_main();                                // [确认] 调用位于 0x08011630 的应用主入口。
}                                             // CRuntimeStart 函数体结束。

/* ============================ Bootloader ============================ */

// [确认] 地址 0x08001B2C：把 OTA 暂存镜像复制到活动应用区。
static bool boot_copy_ota_image(               // 返回 true 表示整份镜像复制成功，false 表示重试仍失败。
    uint32_t image_size)                       // image_size 是需要复制的新固件字节数。
{                                             // boot_copy_ota_image 函数体开始。
    uint8_t page_buffer[512];                  // 在栈上建立 512 字节搬运缓冲区；名字不是原厂变量名。
    uint32_t src_offset = 0;                   // 暂存镜像的当前读取偏移，从 0 开始。
    uint32_t written = 0;                      // 已处理的长度计数；它与 src_offset 的差异值得继续核对。
    unsigned retry = 0;                        // 当前分块写入失败的连续重试次数。

    while (written < image_size) {             // 只要还有数据未处理，就继续分块复制。
        uint32_t len = image_size - src_offset;// 先计算从当前源偏移到镜像结尾还有多少字节。
        if (len > sizeof(page_buffer))         // 如果剩余数据超过 512 字节缓冲区，
            len = sizeof(page_buffer);         // 本轮最多只搬运 512 字节。

        memcpy(                                // 把 OTA 暂存区的数据复制进 RAM 缓冲区。
            page_buffer,                       // 目标是 512 字节 RAM 缓冲区。
            (const void *)(OTA_IMAGE_BASE + src_offset), // 来源是 OTA 暂存基址加当前偏移。
            len);                              // 本轮复制 len 字节。

        int status =                           // 保存 Flash 写入并回读验证函数的返回状态。
            sub_08002294_flash_write_and_verify( // [推断] 写活动区并验证；精确算法尚未完整恢复。
                page_buffer,                   // 要写入的 RAM 数据。
                (void *)(APP_VECTOR + src_offset), // 活动应用区的目标 Flash 地址。
                len);                          // 本轮需要写入的字节数。

        debug_printf(                          // 打印本轮复制状态，供串口诊断。
            "file %x WritePage[%x] status %x\r\n", // 日志格式：镜像大小、位置和写入结果。
            image_size, written, status);      // 把三个数值填入日志字符串。

        if (status == 0) {                     // 返回 0 被视为写入和验证成功。
            src_offset += len;                 // 成功后才推进源镜像偏移。
            retry = 0;                         // 成功后清零连续失败次数。
        } else if (++retry >= 3) {             // 失败时先增加计数，达到三次便放弃整次复制。
            return false;                      // 通知上层：本次镜像复制失败。
        }                                     // 单块成功/失败判断结束。

        written += len;                        // [风险] 伪代码显示失败时也增加 written；可能是反编译还原偏差，必须复核机器码。
    }                                         // 分块复制循环结束。
    return true;                              // 所有分块处理完毕，报告成功。
}                                             // boot_copy_ota_image 函数体结束。

// [确认] 地址 0x08001BA8：Bootloader 的主控制流程。
static void boot_main(void)                   // 上电后判断是否安装 OTA，然后跳转应用。
{                                             // boot_main 函数体开始。
    unsigned whole_image_retries = 0;         // 记录整份镜像重新擦除和复制的次数。

    sub_080019ae_boot_hw_init();               // [推断] 初始化 Bootloader 使用的时钟、串口和 Flash 外设。
    sub_080023ea_read_ota_metadata(            // 从 Flash 元数据页读取 OTA 标记和镜像大小。
        g_ota_metadata,                        // 把结果写到共享的 RAM 元数据结构。
        OTA_METADATA_PAGE,                     // Flash 来源地址为 OTA 元数据页。
        8);                                    // 读取 8 字节，正好是两个 uint32_t。

    debug_printf(                              // 打印当前 OTA 标记和文件大小。
        " ---> otaFlag: %X, otaFileSize: %X\r\n", // 日志格式字符串。
        g_ota_metadata->magic,                 // 第一个值是 OTA 魔数。
        g_ota_metadata->image_size);           // 第二个值是镜像大小。

    if (g_ota_metadata->magic == OTA_MAGIC) {  // 只有魔数匹配时才认为存在待安装固件。
        do {                                  // 开始最多三次的整镜像安装尝试。
            sub_08001f4c_flash_erase_pages(    // 擦除旧应用所在的活动 Flash 区域。
                APP_VECTOR,                    // 从应用向量表地址开始擦除。
                100);                          // [确认] 擦除 100 个 2 KiB 页，即约 200 KiB。

            if (boot_copy_ota_image(           // 尝试把暂存镜像写入活动应用区。
                    g_ota_metadata->image_size)) { // 复制长度来自 OTA 元数据。
                sub_080023de_clear_ota_flag(); // 成功后清除 OTA 魔数，避免下次启动重复安装。
                debug_printf(                  // 输出成功日志。
                    "check new fw crc OK...\r\n"); // [推断] 日志说 CRC OK，具体验证在哪一层仍需复核。
                break;                         // 跳出整镜像重试循环。
            }                                 // 镜像复制成功分支结束。

            ++whole_image_retries;             // 整份镜像安装失败次数加一。
            debug_printf(                      // 打印升级失败和当前重试次数。
                "******upgrade error, retry_time %d******\r\n", // 错误日志格式。
                whole_image_retries);          // 把重试次数填入日志。
        } while (whole_image_retries < 3);     // 少于三次时重新擦除并复制整份镜像。

        if (whole_image_retries >= 3)          // 三次整镜像安装都失败时，
            for (;;) {                         // 进入永久循环，不再尝试启动应用。
                /* fail-stop */                // [风险] 原方案可能导致设备变砖；ESP32 应使用 A/B OTA 和回滚。
            }                                 // 永久循环体结束，正常情况下不会退出。
    }                                         // OTA 安装条件分支结束。

    debug_printf("@@@@jump to app!!!\r\n");  // 打印即将跳转到应用程序的日志。
    sub_08001e36_jump_to_image(APP_VECTOR);    // [确认] 把执行权转移到 0x08004000 的应用镜像。
    for (;;) {                                // 如果跳转函数意外返回，则留在死循环中。
    }                                         // 兜底死循环体结束。
}                                             // boot_main 函数体结束。

/* ============================ 应用主循环 ============================ */

// [确认] 地址 0x08011630：应用程序主入口。
static void app_main(void)                    // 完成设备初始化，然后永久运行合作式主循环。
{                                             // app_main 函数体开始。
    sub_08009110_masked_register_write(        // [未知] 对某个寄存器或 Flash 相关配置进行掩码写入。
        0x08000000u,                           // 第一个参数是 Flash 基址；精确含义尚未确认。
        0x4000u);                              // 第二个参数为 0x4000，恰好等于应用区偏移，但用途待确认。
    sub_0800be38();                            // [未知] 底层初始化函数，需要结合内部寄存器访问继续命名。
    sub_0800be88(                              // [未知] 使用一个异常常量地址和布尔开关进行初始化。
        (void *)0x08954400u,                   // [风险] 此值可能是反编译常量解释偏差，不能当成可靠外设地址。
        true);                                 // 启用该未知功能。
    sub_0800a04c(true);                        // [未知] 启用某项外设或中断功能。
    sub_0800a020(true, true);                  // [未知] 用两个真值启用某个子系统。
    sub_0800635c(                              // [未知] 对第一个对象或通道进行初始化。
        (void *)0x30000100u,                   // 对象/寄存器地址一，具体归属待确认。
        true);                                 // 打开或初始化该对象。
    sub_0800635c(                              // 复用同一函数初始化第二个对象或通道。
        (void *)0x30000200u,                   // 对象/寄存器地址二，具体归属待确认。
        true);                                 // 打开或初始化第二个对象。

    sub_0800c248_system_start();               // [推断] 完成系统外设、参数和业务模块的总体启动。
    sub_08006c18_report_reset_reason();        // 读取并打印上次复位原因。
    sub_08015868();                            // [未知] 主循环开始前的最后初始化步骤。

    for (;;) {                                // 裸机超级循环：函数按固定顺序反复执行。
        feed_iwdg();                           // 喂独立看门狗，证明主循环没有完全卡死。
        sub_0800c418();                        // [推断] 汇总并更新设备中心状态。
        sub_08008558();                        // [推断] 更新显示或产测相关状态。
        sub_08007c8c();                        // [推断] 推进清理老化/寿命测试状态。
        sub_08008354();                        // [推断] 推进生产测试状态机。
        sub_0800da04();                        // [推断] 处理 Wi-Fi 模组协议和通信事件。
        sub_08009030();                        // [确认] 把滚筒命令 1/2/4 转成方向1、方向2或停止的 GPIO 动作。
        sub_08007044();                        // [确认/推断] 运行猫防护、滚筒清理和称重等多个联动状态机。
        sub_08007818();                        // [推断] 执行另一部分产测流程。
        sub_08015330();                        // [推断] 推进执行器或阀的非阻塞控制。
        sub_08009cc8();                        // [推断] 处理 PIR、菜单或数据点逻辑。
        sub_080073d4();                        // [推断] 处理缓存的称重结果。
        sub_0800f5d8();                        // [推断] 处理除臭液余量或状态。
        sub_08015468();                        // [推断] 处理猫离开后的恢复/安全计时。
        sub_08012070();                        // [推断] 处理设备测试模式。
        sub_0800f7b8();                        // [推断] 生成巡检或状态上报信息。
        sub_080151bc();                        // [推断] 推进电磁阀或其他执行器逻辑。
    }                                         // 超级循环结束位置；运行时会立即回到循环开头。
}                                             // app_main 函数体结束，正常情况下永远不会到达这里。

// [确认] 地址 0x080074C0：独立看门狗喂狗函数。
static inline void feed_iwdg(void)             // inline 表示编译器可把这一小段直接嵌入调用位置。
{                                             // feed_iwdg 函数体开始。
    IWDG_KEY_REGISTER = 0xAAAAu;               // [确认] 向看门狗键寄存器写 0xAAAA，重新开始看门狗倒计时。
}                                             // feed_iwdg 函数体结束。

// [确认] 地址 0x08006C18：读取 RCC 复位状态并打印原因。
static void sub_08006c18_report_reset_reason(void) // 诊断设备为什么发生了上一次复位。
{                                             // reset_reason 函数体开始。
    uint32_t flags =                           // 声明变量保存复位状态位图。
        *(volatile uint32_t *)0x40021024u;     // [确认] 读取 N32 RCC_CTRLSTS 寄存器；ESP32 不能使用此地址。

    if (flags & (1u << 31))                   // 检查第 31 位是否置位。
        debug_printf("---->Lower power reset\r\n"); // 若置位，报告低功耗相关复位。
    if (flags & (1u << 30))                   // 检查第 30 位是否置位。
        debug_printf("---->Window watchdog reset\r\n"); // 若置位，报告窗口看门狗复位。
    if (flags & (1u << 27))                   // 检查第 27 位是否置位。
        debug_printf("---->POR/PDR reset\r\n"); // 若置位，报告上电或掉电复位。
    if (flags & (1u << 29))                   // 检查第 29 位是否置位。
        debug_printf("---->Iwatchdog reset\r\n"); // 若置位，报告独立看门狗复位。
    if (flags & (1u << 28))                   // 检查第 28 位是否置位。
        debug_printf("---->Software reset\r\n"); // 若置位，报告软件触发复位。
    if (flags & (1u << 26))                   // 检查第 26 位是否置位。
        debug_printf("---->Pin reset\r\n"); // 若置位，报告外部复位引脚触发。
    if (flags & (1u << 25))                   // 检查第 25 位是否置位。
        debug_printf("---->MMU reset\r\n"); // 按原日志报告 MMU 相关复位；具体硬件含义需查 N32 手册。
    if (flags & (1u << 24))                   // 检查第 24 位是否置位。
        debug_printf("---->Remove reset\r\n"); // 按原字符串报告 Remove reset，翻译和来源待确认。
    if (flags & (1u << 23))                   // 检查第 23 位是否置位。
        debug_printf("---->RAM reset\r\n"); // 若置位，报告 RAM 相关复位。
    if (flags & (1u << 21))                   // 检查第 21 位是否置位。
        debug_printf("---->BKP_EMC reset\r\n"); // 报告备份域/EMC 相关复位，精确定义以芯片手册为准。
    if (flags & (1u << 20))                   // 检查第 20 位是否置位。
        debug_printf("---->RET_EMC reset\r\n"); // 报告保持域/EMC 相关复位，精确定义待查证。
    if (flags & (1u << 19))                   // 检查第 19 位是否置位。
        debug_printf("---->Bor reset\r\n"); // 若置位，报告欠压检测 BOR 复位。
}                                             // reset_reason 函数体结束。

/* ============================ CRC32 与 OTA 接收 ============================ */

// [确认] 地址 0x0800EF24：非反射 CRC32，多项式 0x04C11DB7。
static uint32_t crc32_update(                  // 在旧 CRC 基础上继续处理一段数据，并返回新 CRC。
    const uint8_t *data,                       // 指向只读输入数据的指针。
    uint32_t crc,                              // 调用者传入当前累计 CRC；首次通常为 0xFFFFFFFF。
    size_t len)                                // 本次要处理的字节数。
{                                             // crc32_update 函数体开始。
    while (len--) {                            // 每轮处理一个字节，并把剩余长度减一。
        crc ^= (uint32_t)*data++ << 24;        // 把当前字节放到 CRC 高 8 位异或，然后数据指针前进一字节。
        for (unsigned bit = 0; bit < 8; ++bit) // 一个字节有 8 位，因此逐位迭代 8 次。
            crc =                              // 根据 CRC 最高位决定本轮是否异或生成多项式。
                (crc & 0x80000000u)            // 检查当前 CRC 的最高位是否为 1。
                ? (crc << 1) ^ 0x04c11db7u    // 最高位为 1：左移后异或 CRC32 多项式。
                : (crc << 1);                 // 最高位为 0：只左移一位。
    }                                         // 所有输入字节处理完成。
    return crc;                               // 返回累计 CRC；[风险] CRC 只能检错，不能认证固件来源。
}                                             // crc32_update 函数体结束。

// [确认] 地址 0x08013230：校验已经写入 Flash 的完整镜像。
static bool ota_image_file_check_crc(          // 比较镜像实际 CRC 与发送端给出的期望 CRC。
    uint32_t image_addr,                       // 镜像在 Flash 中的起始地址。
    uint32_t file_size,                        // 镜像总字节数。
    uint32_t expected_crc)                     // 发送端宣称的正确 CRC 值。
{                                             // ota_image_file_check_crc 函数体开始。
    uint8_t buffer[256];                       // 用 256 字节 RAM 缓冲区分块读取 Flash。
    uint32_t offset = 0;                       // 当前读取偏移，从镜像开头开始。
    uint32_t crc = 0xffffffffu;                // 按该算法要求，把累计 CRC 初始化为全 1。

    while (offset < file_size) {               // 只要还没读完整个镜像，就继续校验。
        uint32_t len = file_size - offset;     // 计算当前还剩多少字节未校验。
        if (len > sizeof(buffer))              // 如果剩余数据超过 256 字节，
            len = sizeof(buffer);              // 本轮最多读取一个缓冲区大小。

        sub_08010afc_flash_read(                // [推断] 从 Flash 复制一段数据到 RAM。
            buffer,                            // RAM 目标缓冲区。
            (const void *)(image_addr + offset), // Flash 来源地址为镜像基址加当前偏移。
            len);                              // 本轮读取的字节数。
        crc = crc32_update(buffer, crc, len);  // 把本轮数据加入累计 CRC 计算。
        offset += len;                         // 推进读取偏移，准备下一块数据。

        feed_iwdg();                           // 长时间校验期间持续喂狗，避免被看门狗复位。
        debug_printf(                          // 打印每个分块后的中间 CRC，便于调试。
            "---> check_crc: %08X\r\n", crc); // %08X 表示用 8 位十六进制输出 CRC。
    }                                         // 整个镜像读取循环结束。

    debug_printf(                              // 打印镜像地址、大小和最终偏移。
        "image addr = 0x%08x, file_size = %d, offset = %d\r\n", // 日志格式。
        image_addr, file_size, offset);        // 输出三个诊断值。
    debug_printf(                              // 同时打印实际 CRC 和期望 CRC。
        "check_image_crc = [0x%08X], image_crc = [0x%08X]\r\n", // CRC 对照日志格式。
        crc, expected_crc);                    // 第一个是本机计算值，第二个是发送端值。

    if (crc == expected_crc) {                 // 两个 CRC 完全相等时认为传输内容完整。
        debug_printf(                          // 输出 CRC 正确日志。
            "ota image file check crc correct\r\n"); // 表示镜像通过完整性检查。
        return true;                           // 向调用者报告校验成功。
    }                                         // CRC 相等分支结束。

    debug_printf(                              // 执行到这里说明 CRC 不匹配。
        "ota image file check crc error\r\n"); // 输出镜像损坏或传输错误日志。
    return false;                              // 向调用者报告校验失败。
}                                             // ota_image_file_check_crc 函数体结束。

// [确认] 地址 0x080133B0：提交 Bootloader 能看见的 OTA 标记和大小。
static void save_ota_metadata(void)            // 把 RAM 中的 OTA 元数据写入专用 Flash 页。
{                                             // save_ota_metadata 函数体开始。
    g_ota_metadata->magic = OTA_MAGIC;         // 设置有效魔数，通知下次启动安装暂存镜像。

    for (unsigned retry = 0; retry < 3; ++retry) { // 最多尝试写元数据三次。
        feed_iwdg();                           // Flash 擦写前喂狗，避免操作期间复位。
        sub_08010a60_flash_erase_pages(        // [推断] 擦除 OTA 元数据所在 Flash 页。
            OTA_METADATA_PAGE,                 // 从元数据页地址开始。
            1);                                // 只擦除一页。
        if (sub_08010b06_flash_program(        // [推断] 把 RAM 元数据写入 Flash 并返回状态。
                g_ota_metadata,                // 写入来源：RAM 中的元数据结构。
                (void *)OTA_METADATA_PAGE,     // 写入目标：OTA 元数据页。
                8) == 0)                       // 写 8 字节；返回 0 表示成功。
            break;                             // 写成功后提前结束重试循环。
    }                                         // 元数据写入重试循环结束。
}                                             // save_ota_metadata 函数体结束。

// [确认] 地址 0x08011E1C：通过 2 KiB RAM 页缓冲区写入 OTA 数据。
static bool ota_write_chunk(                   // 接收一段 OTA 数据，必要时把完整页写入 Flash。
    const uint8_t *data,                       // 本次 OTA 有效负载数据。
    uint32_t offset,                           // 这段数据在整个新固件中的偏移。
    uint32_t len)                              // 本次有效负载长度。
{                                             // ota_write_chunk 函数体开始。
    if (len == 0) {                            // 长度为零被伪代码解释为传输结束/提交动作。
        save_ota_metadata();                   // 保存魔数和镜像大小，让 Bootloader 下次安装。
        reset_ota_page_buffer();               // 清空 2 KiB RAM 页缓冲区状态。
        return true;                           // 报告结束处理成功。
    }                                         // 零长度结束分支完成。

    append_to_2k_page_buffer(data, len);       // [推断] 把本包数据追加进 2 KiB RAM 页缓存。
    if (page_buffer_is_full() ||               // 如果缓存已经装满一个 Flash 页，
        offset + len == g_ota_metadata->image_size) { // 或者当前包正好到达整份镜像末尾，
        uint32_t destination =                 // 计算当前缓存页要写入的 Flash 地址。
            OTA_IMAGE_BASE + current_page_offset(); // 暂存区基址加已经写过的页偏移。
        if (sub_08010b06_flash_program(        // 尝试把缓存页写入 OTA 暂存 Flash。
                page_buffer(),                 // 返回当前 2 KiB RAM 缓冲区地址。
                (void *)destination,           // 写入目标是刚计算的暂存区地址。
                FLASH_PAGE_SIZE) != 0) {       // 固定写一个 2 KiB Flash 页；非零表示失败。
            debug_printf("OTA FAIL\r\n");    // 输出本页写入失败日志。
        } else {                              // Flash 页写入成功时进入此分支。
            debug_printf(                      // 打印刚写入的目标地址。
                "upgradeAddr = %lx\r\n", destination); // 用十六进制/长整数形式输出地址。
        }                                     // Flash 写入成功/失败分支结束。
        advance_page_and_clear_buffer();       // [风险] 即使写失败似乎也推进下一页；需复核机器码和重试逻辑。
    }                                         // 满页或最后一页处理结束。
    return true;                              // [风险] 当前伪代码无论 Flash 是否失败都返回 true，不能直接照搬。
}                                             // ota_write_chunk 函数体结束。

// [推断] 这是对 0x0800F0F6 附近 OTA 报文处理逻辑的高层重建。
static void ota_frame_handler(                 // 处理一帧 OTA 协议数据并更新接收状态。
    OtaReceiveState *s,                        // 指向本次 OTA 会话状态结构。
    const uint8_t *frame,                      // 指向收到的原始协议帧。
    size_t frame_len)                          // 原始协议帧的总长度。
{                                             // ota_frame_handler 函数体开始。
    if (is_ota_start_frame(frame, frame_len)) { // 判断当前帧是不是 OTA 开始帧。
        s->max_packet_size = 256;              // 告诉协议层后续每个数据包最多 256 字节。
        s->image_size = read_be32(frame + 6);  // 从帧偏移 6 处读取大端 32 位固件总大小。
        g_ota_metadata->image_size =           // 同步设置 Bootloader 元数据中的镜像大小。
            s->image_size;                     // 值来自刚解析的 OTA 开始帧。
        s->rolling_crc = 0xffffffffu;          // 把本机累计 CRC 重置为算法初值。
        s->transmitted_crc = 0xffffffffu;      // 把发送端 CRC 先初始化为占位值。
        s->bytes_received = 0;                 // 清零已接收位置/长度计数。
        s->state = 0x0a;                       // 把接收会话切换到 0x0A 状态，表示正在 OTA。

        feed_iwdg();                           // 擦除元数据页前喂狗。
        sub_08010a60_flash_erase_pages(        // 擦除旧 OTA 元数据。
            OTA_METADATA_PAGE,                 // 从元数据页开始。
            1);                                // 擦除一页。
        feed_iwdg();                           // 大范围擦除 OTA 暂存区前再次喂狗。
        sub_08010a60_flash_erase_pages(        // 擦除接收新镜像所需的全部暂存页。
            OTA_IMAGE_BASE,                    // 从 OTA 暂存区基址开始。
            OTA_IMAGE_PAGE_COUNT);             // 擦除 101 个 2 KiB 页。
        reset_ota_page_buffer();               // 清零 RAM 页缓存和当前页偏移。
        return;                                // 开始帧处理完成，不再按数据帧解析。
    }                                         // OTA 开始帧分支结束。

    if (!is_ota_data_frame(frame, frame_len) || // 如果当前帧不是 OTA 数据帧，
        s->state != 0x0a)                      // 或者接收状态并非正在 OTA，
        return;                                // 直接忽略当前帧，避免错误写 Flash。

    uint32_t packet_len =                      // 声明当前 OTA 包的有效负载长度。
        ota_packet_length(frame);              // [推断] 从协议帧头解析该长度。
    uint32_t packet_offset =                   // 声明当前包在整份镜像中的偏移。
        ota_packet_offset(frame);              // [推断] 从协议帧头解析偏移。
    const uint8_t *payload = frame + 10;       // [确认/推断] 有效负载被解释为从帧偏移 10 开始。

    if (is_last_ota_packet(                    // 判断当前数据包是不是整份镜像的最后一包。
            s, packet_offset, packet_len)) {   // 判断依据包括会话状态、包偏移和包长度。
        s->transmitted_crc =                   // 从最后一包末尾取出发送端给出的 CRC。
            read_be32(payload + packet_len - 4); // 最后四字节按大端 32 位读取。
        packet_len -= 4;                       // CRC 不属于固件正文，因此有效负载长度减四。
    }                                         // 最后一包特殊处理结束。

    ota_write_chunk(                           // 把本包固件正文加入页缓存并在需要时写 Flash。
        payload, packet_offset, packet_len);   // 传入正文指针、整镜像偏移和正文长度。
    s->rolling_crc =                           // 更新本机计算的累计 CRC。
        crc32_update(                          // 使用前面恢复出的 CRC32 算法。
            payload,                          // 本次参与计算的是固件正文。
            s->rolling_crc,                    // 从上一包累计 CRC 继续。
            packet_len);                       // 处理当前正文长度，不包括最后的 CRC 四字节。
    s->bytes_received = packet_offset;         // [风险] 字段被写为包偏移而非 offset+len，精确定义必须抓包复核。

    if (s->rolling_crc == s->transmitted_crc) // 本机累计 CRC 与发送端 CRC 相等时，
        debug_printf(                          // 输出已经收到完整镜像、准备后验校验的日志。
            "Whole firmware image received. Postvalidating.\r\n"); // [风险] CRC 相等不代表固件来源可信。
}                                             // ota_frame_handler 函数体结束。

/* ============================ Wi-Fi 模组产品信息 ============================ */

// [确认] 地址 0x08013774：在 RAM 中拼接产品标识 JSON。
static void send_wifi_product_info(void)       // 回应原涂鸦 Wi-Fi 模组的产品信息查询。
{                                             // send_wifi_product_info 函数体开始。
    const char *product_id =                   // 声明只读产品 ID 字符串指针。
        "iz8nbemamfyiau0l";                   // [确认] 原竞品涂鸦 PID；[风险] 不能用于自有产品身份。
    const char *version = "2.3.3";            // [确认] 原固件上报给模组的 MCU 软件版本。

    char *out = (char *)0x2000c210u;           // [确认] 原程序把输出 JSON 拼接到这个固定 RAM 地址。
    size_t n = 0;                              // n 记录当前已经写入输出缓冲区的字符数。
    n += append(out + n, "{\"p\":\"");   // 追加 JSON 开头和产品 ID 字段名：{"p":"。
    n += append(out + n, product_id);          // 追加原竞品 PID 字符串。
    n += append(out + n, "\",\"v\":\""); // 结束 PID，并开始版本字段：","v":"。
    n += append(out + n, version);             // 追加固件版本 2.3.3。
    n += append(out + n, "\",\"m\":");  // 结束版本字符串，并开始数值模式字段 m。
    n += append(out + n, "0");                // 追加模式值 0；该值的具体平台语义见涂鸦协议。
    n += append(out + n, "}");                // 追加 JSON 右花括号，结束产品信息。

    sub_08015af0_protocol_send(                // [推断] 通过涂鸦 MCU UART 协议发送拼好的数据。
        1,                                     // [确认] 第一个协议参数为 1，可能是产品信息命令。
        3,                                     // [未知] 第二个参数为 3，可能表示协议版本或帧类型。
        (uint16_t)n);                          // 发送长度是刚拼好的 JSON 字节数，并转换为 16 位整数。
}                                             // send_wifi_product_info 函数体结束。

/* 当前已经恢复并完成逐行注释的代码到此结束。 */
