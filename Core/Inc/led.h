/* led.h：对外接口说明。先看本文件了解如何调用，再到同名.c查看实现。
 * 头文件声明函数的名字、参数与返回类型；#ifndef/#define/#endif避免重复包含。 */
#ifndef LED_H
#define LED_H

/* 点亮PB15外接灯。 */
void LED_On(void);
/* 关闭外接灯。 */
void LED_Off(void);
/* 翻转当前电平，实现亮灭切换。 */
void LED_Toggle(void);

#endif
