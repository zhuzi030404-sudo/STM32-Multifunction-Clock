/* menu.h：对外接口说明。先看本文件了解如何调用，再到同名.c查看实现。
 * 头文件声明函数的名字、参数与返回类型；#ifndef/#define/#endif避免重复包含。 */
#ifndef MENU_H
#define MENU_H

#include <stdint.h>

#define MENU_ACTION_NONE        0
#define MENU_ACTION_HOME        1
#define MENU_ACTION_SET_TIME    2
#define MENU_ACTION_STOPWATCH   3
#define MENU_ACTION_LIGHT       4
#define MENU_ACTION_MOTION 5

#define MENU_ACTION_ATTITUDE 6

#define MENU_ACTION_LEVEL 7
#define MENU_ACTION_DINO 8
#define MENU_ACTION_EMOJI 9
#define MENU_ACTION_COUNTDOWN 10
#define MENU_ACTION_BRIGHTNESS 11

/* 进入菜单时绘制固定提示，再按已有selected显示选项；不会每次把选择清零。 */
void Menu_Open(void);
/* 正数向后、负数向前，取余实现11项循环；多步变化合并后只重画一次。 */
void Menu_Move(int8_t steps);
/* 按键1/2移动，3确认，长按3回主页；返回动作编号供main切换页面。 */
uint8_t Menu_HandleEvent(uint8_t event);

#endif
