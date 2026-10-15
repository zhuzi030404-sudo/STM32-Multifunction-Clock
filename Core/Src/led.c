/* 外接LED（发光二极管）驱动：PB15低电平亮、高电平灭，这是当前接线决定的。
 * GPIO_PIN_RESET 表示输出低电平，GPIO_PIN_SET 表示输出高电平，不代表“复位/设置LED功能”。 */
#include "led.h"
#include "main.h"

/* 按原项目接法：PB15 低电平亮，高电平灭。 */

/* 点亮PB15外接灯。 */
void LED_On(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
}

/* 关闭外接灯。 */
void LED_Off(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
}

/* 翻转当前电平，实现亮灭切换。 */
void LED_Toggle(void)
{
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_15);
}
