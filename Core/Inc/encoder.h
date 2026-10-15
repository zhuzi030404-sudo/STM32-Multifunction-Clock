/* encoder.h：对外接口说明。先看本文件了解如何调用，再到同名.c查看实现。
 * 头文件声明函数的名字、参数与返回类型；#ifndef/#define/#endif避免重复包含。 */
#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

/* 读取初始状态并清空累计；PRIMASK保存中断屏蔽状态，结束时恢复原值。 */
void Encoder_Init(void);
/* 在中断回调中处理一条边沿，只更新计数，不在这里画屏幕或延时。 */
void Encoder_OnEdge(uint16_t pin);
/* 原子地取出并清零累计步数：短暂禁止中断，避免读取期间丢失新步数；范围为-32到32。 */
int8_t Encoder_GetDelta(void);

#endif
