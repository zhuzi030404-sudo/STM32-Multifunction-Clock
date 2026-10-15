/* dino.h：对外接口说明。先看本文件了解如何调用，再到同名.c查看实现。
 * 头文件声明函数的名字、参数与返回类型；#ifndef/#define/#endif避免重复包含。 */
#ifndef DINO_H
#define DINO_H

#include <stdint.h>

/* 开始新一局：清分数、初始化物体位置和时间，再绘制初始画面。 */
void Dino_Open(void);
/* 处理1号键的新按下事件；在空中不再起跳，游戏结束则重开。 */
void Dino_HandleEvent(uint8_t event);
/* 主循环推进运动和碰撞，并分批传屏；跳跃用正弦曲线生成先升后降的高度。 */
void Dino_Update(void);

#endif
