/* 横向图形菜单：selected保存选中项，names是显示名称，actions是确认后交给主程序的动作编号。
 * 数组下标从0开始，两张表必须一一对应。菜单只返回动作，不直接操作时钟或运动传感器。
 * UI 指用户界面；这里通过像素绘图组合图标、选中框、中文名称和位置提示。 */
#include "menu.h"
#include "oled.h"
#include "key.h"
#include <string.h>

/* 选中项下标0到10；这是文件级状态，离开再进入菜单仍保留，但复位后回到0。 */
static uint8_t selected = 0;

static const char *names[11] =
{
    "返回",
    "时间设置",
    "秒表",
    "灯光",
    "运动数据",
    "姿态",
    "水平仪",
    "小恐龙",
    "表情",
    "倒计时",
    "亮度"
};

static const uint8_t actions[11] =
{
    MENU_ACTION_HOME,
    MENU_ACTION_SET_TIME,
    MENU_ACTION_STOPWATCH,
    MENU_ACTION_LIGHT,
    MENU_ACTION_MOTION,
    MENU_ACTION_ATTITUDE,
    MENU_ACTION_LEVEL,
    MENU_ACTION_DINO,
    MENU_ACTION_EMOJI,
    MENU_ACTION_COUNTDOWN,
    MENU_ACTION_BRIGHTNESS
};

/* 用四条直线画选中框。 */
static void Menu_Box(int16_t left, int16_t top, int16_t right, int16_t bottom)
{
    OLED_DrawLine(left, top, right, top);
    OLED_DrawLine(right, top, right, bottom);
    OLED_DrawLine(right, bottom, left, bottom);
    OLED_DrawLine(left, bottom, left, top);
}

/* 根据菜单编号，用线和圆在指定中心位置组合对应图标。 */
static void Menu_DrawIcon(uint8_t item, int16_t cx)
{
    const int16_t cy = 27;
    switch (item)
    {
        case 0:
            OLED_DrawLine(cx - 8, cy, cx + 8, cy);
            OLED_DrawLine(cx - 8, cy, cx - 2, cy - 6);
            OLED_DrawLine(cx - 8, cy, cx - 2, cy + 6);
            break;
        case 1:
            OLED_DrawCircle(cx, cy, 8, 0);
            OLED_DrawLine(cx, cy, cx, cy - 5);
            OLED_DrawLine(cx, cy, cx + 4, cy + 2);
            break;
        case 2:
            OLED_DrawCircle(cx, cy + 1, 7, 0);
            OLED_DrawLine(cx, cy + 1, cx + 3, cy - 3);
            OLED_DrawLine(cx - 3, 18, cx + 3, 18);
            OLED_DrawLine(cx, 18, cx, 21);
            break;
        case 3:
            OLED_DrawCircle(cx, cy - 3, 6, 0);
            OLED_DrawLine(cx - 3, 29, cx - 3, 32);
            OLED_DrawLine(cx + 3, 29, cx + 3, 32);
            OLED_DrawLine(cx - 3, 32, cx + 3, 32);
            OLED_DrawLine(cx - 2, 35, cx + 2, 35);
            break;
        case 4:
            OLED_DrawLine(cx - 9, 18, cx - 9, 35);
            OLED_DrawLine(cx - 9, 35, cx + 9, 35);
            OLED_DrawLine(cx - 8, 29, cx - 4, 25);
            OLED_DrawLine(cx - 4, 25, cx, 32);
            OLED_DrawLine(cx, 32, cx + 4, 21);
            OLED_DrawLine(cx + 4, 21, cx + 8, 27);
            break;
        case 5:
            OLED_DrawLine(cx, cy, cx + 8, cy);
            OLED_DrawLine(cx + 8, cy, cx + 5, cy - 2);
            OLED_DrawLine(cx, cy, cx, cy - 8);
            OLED_DrawLine(cx, cy - 8, cx - 2, cy - 5);
            OLED_DrawLine(cx, cy, cx - 6, cy + 7);
            OLED_DrawLine(cx - 6, cy + 7, cx - 6, cy + 3);
            break;
        case 6:
            Menu_Box(cx - 10, cy - 3, cx + 10, cy + 3);
            OLED_DrawCircle(cx, cy, 2, 0);
            OLED_DrawLine(cx - 7, 35, cx + 7, 35);
            break;
        case 7:
        {
            static const uint8_t dinosaur[48] = {
                0xF0,0xC0,0x00,0x00,0x00,0x80,0x80,0xC0,0xFE,0xFF,0xFB,0xFF,0x7F,0x6F,0x6F,0x0E,
                0xC3,0x07,0x0F,0xFE,0xFF,0x7F,0x1F,0x1F,0x7F,0x7F,0x5F,0x07,0xC1,0xC3,0xC0,0xC0,
                0x00,0x00,0x00,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            };
            for (uint8_t y = 0; y < 18; y++)
                for (uint8_t x = 0; x < 16; x++)
                    if (dinosaur[(y / 8U) * 16U + x] & (1U << (y % 8U)))
                        OLED_DrawPoint(cx - 8 + x, 18 + y);
            break;
        }
        case 10:
        {
            static const int8_t rays[8][2] = {{0,-9},{6,-6},{9,0},{6,6},{0,9},{-6,6},{-9,0},{-6,-6}};
            OLED_DrawCircle(cx, cy, 4, 0);
            for (uint8_t i = 0; i < 8; i++)
                OLED_DrawLine(cx + rays[i][0] / 2, cy + rays[i][1] / 2,
                              cx + rays[i][0], cy + rays[i][1]);
            break;
        }
        case 9:
            OLED_DrawLine(cx - 7, 18, cx + 7, 18);
            OLED_DrawLine(cx - 7, 35, cx + 7, 35);
            OLED_DrawLine(cx - 6, 19, cx + 6, 34);
            OLED_DrawLine(cx + 6, 19, cx - 6, 34);
            OLED_DrawLine(cx - 3, 32, cx + 3, 32);
            break;
        case 8:
            OLED_DrawCircle(cx, cy, 8, 0);
            OLED_DrawCircle(cx - 3, cy - 2, 1, 1);
            OLED_DrawCircle(cx + 3, cy - 2, 1, 1);
            OLED_DrawLine(cx - 4, cy + 3, cx - 3, cy + 5);
            OLED_DrawLine(cx - 3, cy + 5, cx + 3, cy + 5);
            OLED_DrawLine(cx + 3, cy + 5, cx + 4, cy + 3);
            break;
    }
}

/* 重画当前选项与左右邻居、位置提示和中文名称；保留固定标题区域。 */
static void Menu_Refresh(void)
{
    OLED_FrameClear();
    Menu_DrawIcon(selected == 0U ? 10U : selected - 1U, 20);
    Menu_DrawIcon(selected, 64);
    Menu_DrawIcon((selected + 1U) % 11U, 108);
    Menu_Box(46, 16, 82, 38);
    for (uint8_t i = 0; i < 11; i++)
    {
        if (i == selected)
            OLED_DrawCircle(34 + i * 6U, 60, 2, 1);
        else
            OLED_DrawPoint(34 + i * 6U, 60);
    }
    OLED_UpdateFrame(2, 3);
    OLED_UpdateFrameArea(7, 1, 31, 67);

    /* Clear both halves of the previous centered 16-pixel Chinese label. */
    OLED_ShowString(5, 32, "           ");
    OLED_ShowString(6, 32, "           ");
    /* 当前名称由常用汉字组成：UTF-8每字3字节、字形宽16像素，因此可算居中位置。 */
    uint8_t width = (uint8_t)(strlen(names[selected]) / 3U * 16U);
    OLED_ShowChinese(5, (128U - width) / 2U, names[selected]);
}

/* 进入菜单时绘制固定提示，再按已有selected显示选项；不会每次把选择清零。 */
void Menu_Open(void)
{
    OLED_Fill(0x00);
    OLED_ShowChinese(0, 0, "菜单");
    OLED_ShowString(0, 40, "1 2");
    OLED_ShowChinese(0, 64, "切换");
    OLED_ShowString(0, 102, "3OK");
    Menu_Refresh();
}

/* 正数向后、负数向前，取余实现11项循环；多步变化合并后只重画一次。 */
void Menu_Move(int8_t steps)
{
    /* 带负号步数要先用有符号整数计算；C语言负数取余仍可能为负，下面加11修正。 */
    int16_t next = ((int16_t)selected + steps) % 11;
    if (next < 0)
        next += 11;
    if (next != selected)
    {
        selected = (uint8_t)next;
        Menu_Refresh();
    }
}

/* 按键1/2移动，3确认，长按3回主页；返回动作编号供main切换页面。 */
uint8_t Menu_HandleEvent(uint8_t event)
{
    if (event == KEY_EVENT_1)
    {
        Menu_Move(-1);
    }
    else if (event == KEY_EVENT_2)
    {
        Menu_Move(1);
    }
    else if (event == KEY_EVENT_3)
        return actions[selected];
    else if (event == KEY_EVENT_3_LONG)
        return MENU_ACTION_HOME;
    return MENU_ACTION_NONE;
}
