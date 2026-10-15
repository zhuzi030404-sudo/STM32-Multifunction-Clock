/* emoji.h：对外接口说明。先看本文件了解如何调用，再到同名.c查看实现。
 * 头文件声明函数的名字、参数与返回类型；#ifndef/#define/#endif避免重复包含。 */
#ifndef EMOJI_H
#define EMOJI_H

#include <stdint.h>

/* 进入表情页面，从第一个表情开始并显示操作提示。 */
void Emoji_Open(void);
/* 1100毫秒一轮动画，前6个阶段每100毫秒变化，随后停留；每轮主循环分批传送。 */
void Emoji_Update(void);
/* 按键1向前、2向后，六个表情首尾循环。 */
void Emoji_HandleEvent(uint8_t event);

#endif
