/* 旋转编码器驱动：A=PA2、B=PA3、公共端C接地。两路信号的先后顺序用于判断旋转方向。
 * EXTI 是外部中断：引脚出现上升沿或下降沿时，处理器会暂时转去执行中断处理。
 * volatile 提醒编译器该变量可能被中断修改；它本身不保证“读取并清零”不会被中断打断。
 * 本驱动只产生步数，菜单怎么移动由主程序决定；旋转正负方向需结合实物接法理解。 */
#include "encoder.h"
#include "main.h"

/* 初始化完成后才接受引脚中断。 */
static volatile uint8_t initialized;
/* 上一次A/B组合状态，用于与这次状态比较方向。 */
static volatile uint8_t previousState;
/* 不足完整一圈电气状态序列的转换累计，可正可负。 */
static volatile int8_t partialStep;
/* 已累计但尚未被主循环取走的完整步数，限制在-32到32。 */
static volatile int8_t pendingSteps;

/* 一次读取GPIOA输入寄存器IDR，把A/B电平组合成0到3的二进制状态。 */
static uint8_t Encoder_ReadState(void)
{
    /* A and B share GPIOA; sample both contacts at the same instant. */
    uint32_t pins = ENC_A_GPIO_Port->IDR;
    return (uint8_t)((((pins & ENC_A_Pin) != 0U) << 1U) |
                     ((pins & ENC_B_Pin) != 0U));
}

/* 读取初始状态并清空累计；PRIMASK保存中断屏蔽状态，结束时恢复原值。 */
void Encoder_Init(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    previousState = Encoder_ReadState();
    partialStep = 0;
    pendingSteps = 0;
    initialized = 1;
    __set_PRIMASK(mask);
}

/* 在中断回调中处理一条边沿，只更新计数，不在这里画屏幕或延时。 */
void Encoder_OnEdge(uint16_t pin)
{
    if (!initialized || (pin != ENC_A_Pin && pin != ENC_B_Pin))
        return;
    /* 查表下标=旧状态×4+新状态；+1/-1为有效方向，往返抖动相互抵消，非法双位跳变单独丢弃。 */
    static const int8_t transitions[16] =
    {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
    };
    uint8_t state = Encoder_ReadState();
    uint8_t oldState = previousState;
    previousState = state;
    /* ^为按位异或：二进制11表示A、B同时变了，跳过中间状态，不能可靠判断方向。 */
    if ((state ^ oldState) == 3U)
    {
        partialStep = 0;
        return;
    }
    /* Forward/back contact bounce cancels before a full cycle is counted. */
    partialStep += transitions[oldState * 4U + state];
    /* 四次有效转换累计一整步；这与机械卡点的关系取决于编码器实物，不能仅凭代码判断。 */
    if (partialStep >= 4)
    {
        if (pendingSteps < 32)
            pendingSteps++;
        partialStep = 0;
    }
    else if (partialStep <= -4)
    {
        if (pendingSteps > -32)
            pendingSteps--;
        partialStep = 0;
    }
}

/* 原子地取出并清零累计步数：短暂禁止中断，避免读取期间丢失新步数；范围为-32到32。 */
int8_t Encoder_GetDelta(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    /* 复制与清零是两步操作，必须在短暂屏蔽中断的同一段临界区里完成。 */
    int8_t steps = pendingSteps;
    pendingSteps = 0;
    __set_PRIMASK(mask);
    return steps;
}
