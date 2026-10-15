/* 按键驱动：按键1=PB1、按键2=PA6、按键3=PA4；上拉输入意味着松开为高、按下接地为低。
 * 消抖：机械触点刚接触时会快速跳变，连续稳定20毫秒后才承认新的状态。
 * 普通页面主要读取松手事件；游戏读取按下事件，避免等松手才跳跃。 */
#include "main.h"
#include "key.h"

/* 读取引脚电平：0=没有按键，1/2/3=对应按键；多个键同时按下时按代码顺序优先。 */
static uint8_t Key_ReadRaw(void)
{
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET)
    {
        return 1;
    }

    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_RESET)
    {
        return 2;
    }

    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET)
    {
        return 3;
    }

    return 0;
}

/* 返回消抖后的稳定键号；changedAt记录最后一次原始输入变化的时间。 */
uint8_t Key_GetState(void)
{
    static uint8_t lastRaw = 0;
    static uint8_t stableState = 0;
    static uint32_t changedAt = 0;

    uint8_t raw = Key_ReadRaw();
    uint32_t now = HAL_GetTick();

    if (raw != lastRaw)
    {
        lastRaw = raw;
        changedAt = now;
    }

    if ((uint32_t)(now - changedAt) >= 20U)
    {
        stableState = raw;
    }

    return stableState;
}

/* 只在稳定状态变为新按键时返回键号；持续按住不会重复触发，需在主循环持续调用。 */
uint8_t Key_GetPressEvent(void)
{
    static uint8_t lastState = 0;
    uint8_t state = Key_GetState();
    if (state == lastState)
        return KEY_EVENT_NONE;
    lastState = state;
    return state;
}

/* 松手才生成事件：1/2/3为短按；3号键按住至少1000毫秒再松手返回长按事件4。 */
uint8_t Key_GetEvent(void)
{
    static uint8_t pressedKey = 0;
    static uint32_t pressedAt = 0;

    uint8_t state = Key_GetState();
    uint32_t now = HAL_GetTick();

    if (state == pressedKey)
    {
        return KEY_EVENT_NONE;
    }

    if (state != 0)
    {
        pressedKey = state;
        pressedAt = now;
        return KEY_EVENT_NONE;
    }

    uint8_t event = pressedKey;

    if (pressedKey == 3 &&
        (uint32_t)(now - pressedAt) >= 1000U)
    {
        event = KEY_EVENT_3_LONG;
    }

    pressedKey = 0;

    return event;
}
