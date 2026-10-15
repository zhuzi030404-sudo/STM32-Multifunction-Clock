/* 有源蜂鸣器驱动：PA8低电平响、高电平停。有源蜂鸣器内部能自行振荡，只需控制开关。
 * 主循环反复调用 Update()，根据经过的时间切换输出，不用长时间等待来制造鸣叫节奏。 */
#include "buzzer.h"
#include "main.h"

/* 整段提醒是否进行中；即使当前两声之间安静，这个标志仍可能为1。 */
static uint8_t alertRunning = 0;
/* 实际输出是否正在发声，用于避免重复写入相同电平。 */
static uint8_t outputOn = 0;
/* 本次提醒的起始毫秒时刻。 */
static uint32_t startedAt = 0;

/* 清除提示状态并输出高电平，让蜂鸣器停止。 */
void Buzzer_Stop(void)
{
    alertRunning = 0;
    outputOn = 0;
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
}

/* 初始化时先关闭蜂鸣器，避免上电误响。 */
void Buzzer_Init(void)
{
    Buzzer_Stop();
}

/* 启动一次最长10秒的提示，并立即进入鸣叫阶段；再次调用会重新计时。 */
void Buzzer_Alert(void)
{
    startedAt = HAL_GetTick();
    alertRunning = 1;
    outputOn = 1;
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

/* 每轮主循环调用：每1000毫秒的前200毫秒响，其余时间停，10秒后自动结束。 */
void Buzzer_Update(void)
{
    if (!alertRunning)
        return;

    uint32_t elapsed = HAL_GetTick() - startedAt;
    if (elapsed >= 10000U)
    {
        Buzzer_Stop();
        return;
    }

    /* %是取余：1200毫秒余200，不响；2000毫秒余0，开始下一声。 */
    uint8_t shouldOn = (elapsed % 1000U) < 200U;
    if (shouldOn != outputOn)
    {
        outputOn = shouldOn;
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin,
                          outputOn ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
}

/* 返回1表示提示过程仍在进行，包括两声之间的安静间隔；返回0表示结束。 */
uint8_t Buzzer_IsAlerting(void)
{
    return alertRunning;
}
