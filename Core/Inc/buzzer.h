/* buzzer.h：对外接口说明。先看本文件了解如何调用，再到同名.c查看实现。
 * 头文件声明函数的名字、参数与返回类型；#ifndef/#define/#endif避免重复包含。 */
#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

/* 初始化时先关闭蜂鸣器，避免上电误响。 */
void Buzzer_Init(void);
/* 启动一次最长10秒的提示，并立即进入鸣叫阶段；再次调用会重新计时。 */
void Buzzer_Alert(void);
/* 清除提示状态并输出高电平，让蜂鸣器停止。 */
void Buzzer_Stop(void);
/* 每轮主循环调用：每1000毫秒的前200毫秒响，其余时间停，10秒后自动结束。 */
void Buzzer_Update(void);

/* 返回1表示提示过程仍在进行，包括两声之间的安静间隔；返回0表示结束。 */
uint8_t Buzzer_IsAlerting(void);

#endif
