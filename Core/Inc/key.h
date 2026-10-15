/* key.h：对外接口说明。先看本文件了解如何调用，再到同名.c查看实现。
 * 头文件声明函数的名字、参数与返回类型；#ifndef/#define/#endif避免重复包含。 */
#ifndef KEY_H
#define KEY_H

#include <stdint.h>

/* 这些宏是事件编号，比较时使用名字更清楚；长按事件4只针对3号按键。 */
#define KEY_EVENT_NONE      0
#define KEY_EVENT_1         1
#define KEY_EVENT_2         2
#define KEY_EVENT_3         3
#define KEY_EVENT_3_LONG    4

/* 返回消抖后的稳定键号；changedAt记录最后一次原始输入变化的时间。 */
uint8_t Key_GetState(void);
/* 松手才生成事件：1/2/3为短按；3号键按住至少1000毫秒再松手返回长按事件4。 */
uint8_t Key_GetEvent(void);
/* Poll every loop, even outside the game, to consume held-key transitions. */
/* 只在稳定状态变为新按键时返回键号；持续按住不会重复触发，需在主循环持续调用。 */
uint8_t Key_GetPressEvent(void);

#endif
