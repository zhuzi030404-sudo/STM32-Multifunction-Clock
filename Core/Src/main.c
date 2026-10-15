/* USER CODE BEGIN Header */
/* 新手阅读入口：先找 main()，按“初始化 → while(1)反复运行 → 处理输入 → 刷新页面”阅读。
 * 本文件负责应用流程，具体硬件操作交给各驱动；页面状态机就是用 page 记录当前页面。
 * HAL 是硬件抽象库；GPIO 是通用输入输出引脚；RTC 是实时时钟；ADC 是模数转换器。
 * HAL_GetTick() 返回开机后的毫秒计数，不能当作日期时间使用。
 * uint8_t/uint16_t/uint32_t 分别是8/16/32位无符号整数；int16_t 可表示负数；float 表示小数。
 * 文件级 static 变量只在本文件可见；函数里的 static 变量会保留上一次调用的值。
 * 带 * 的参数是指针，保存变量地址；&变量用于取地址，结构体指针用 -> 访问成员。
 * CubeMX 自动生成区域可能被重新生成覆盖，自定义内容应放在 USER CODE BEGIN/END 标记之间。 */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "led.h"
#include "buzzer.h"
#include "light_sensor.h"
#include "oled.h"
#include "key.h"
#include "encoder.h"
#include "myrtc.h"
#include "menu.h"
#include "dino.h"
#include "emoji.h"
#include "mpu6050.h"
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 这些编号代表应用页面；和OLED驱动的8像素page横带不是同一概念。 */
#define PAGE_CLOCK      0
#define PAGE_MENU       1
#define PAGE_SET_TIME   2
#define PAGE_STOPWATCH  3
#define PAGE_LIGHT      4
#define PAGE_MOTION 5
#define PAGE_ATTITUDE 6
#define PAGE_LEVEL 7
#define PAGE_DINO 8
#define PAGE_EMOJI 9
#define PAGE_COUNTDOWN 10
#define PAGE_BRIGHTNESS 11
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

RTC_HandleTypeDef hrtc;

/* USER CODE BEGIN PV */
/* 下面是需要跨多轮主循环保存的应用状态：标志通常用0/1，时间统一用毫秒。
 * 255是“不存在的旧状态”，用于强制首次刷新；亮度下标0到3对应屏幕显示的1到4级。 */
/* 0为数字主页，1为带模拟指针的主页。 */
static uint8_t clockAnalog = 0;
/* 0表示无待传指针图；1到5表示下一次需要传送的OLED横带。 */
static uint8_t clockRenderPage = 0;
/* 秒表是否继续累计：0暂停，1运行。 */
static uint8_t stopwatchRunning = 0;
/* 已经累计的毫秒数，暂停时保留。 */
static uint32_t stopwatchElapsed = 0;
/* 上次秒表更新时的开机毫秒计数，用于求时间差。 */
static uint32_t stopwatchTick = 0;
/* 设置的倒计时时长，60000毫秒就是1分钟；完成后重来时使用。 */
static uint32_t countdownDuration = 60000U;
/* 剩余毫秒数；与设置时长分开，才能暂停后继续。 */
static uint32_t countdownRemaining = 60000U;
/* 上次倒计时更新或重新开始时的毫秒计数。 */
static uint32_t countdownTick = 0;
/* 是否正在扣减剩余时间。 */
static uint8_t countdownRunning = 0;
/* 是否已经倒数到零，用于区分“暂停”和“完成”。 */
static uint8_t countdownFinished = 0;
/* 缓存上一次提示样式；255让第一次进入页面一定重绘。 */
static uint8_t countdownDrawStyle = 255;
/* 按下消音后，等待并消耗对应松手事件，避免又触发一次页面操作。 */
static uint8_t silenceKeyHeld = 0;
/* 默认自动亮度；0切换为手动。 */
static uint8_t brightnessAuto = 1;
/* 自动亮度下标0到3；下标3是第4档，不是第3档。 */
static uint8_t brightnessAutoLevel = 3;
/* 手动档位单独保存，避免被自动计算覆盖。 */
static uint8_t brightnessManualLevel = 3;
/* 缓存采样是否有效，只有状态变化才清除旧的采样提示区域。 */
static uint8_t brightnessDrawValid = 255;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 显示带正负号的原始运动计数；先扩为32位再取绝对值，避免最小16位负数溢出。 */
static void Motion_ShowValue(uint8_t row, uint8_t x, int16_t value)
{
    int32_t number = value;

    OLED_ShowString(row, x, number < 0 ? "-" : "+");

    uint32_t magnitude =
        number < 0 ? (uint32_t)(-number) : (uint32_t)number;

    OLED_ShowNum(row, x + 6U, magnitude, 5);
}

/* 更新六轴数值，固定标题留给Open函数绘制。 */
static void Motion_Draw(const MPU6050_Data *data)
{
    Motion_ShowValue(2, 18, data->ax);
    Motion_ShowValue(3, 18, data->ay);
    Motion_ShowValue(4, 18, data->az);

    Motion_ShowValue(2, 84, data->gx);
    Motion_ShowValue(3, 84, data->gy);
    Motion_ShowValue(4, 84, data->gz);
}

/* 进入运动页面时绘制轴名称或未连接提示。 */
static void Motion_Open(uint8_t ready)
{
    OLED_Fill(0x00);
    OLED_ShowChinese(0, 0, "加速度");
    OLED_ShowChinese(0, 66, "角速度");
    OLED_ShowChinese(6, 0, "重试");
    OLED_ShowString(6, 36, "1");
    OLED_ShowChinese(6, 72, "返回");
    OLED_ShowString(6, 110, "3");
    if (ready)
    {
        for (uint8_t row = 2; row <= 4; row++)
        {
            const char *axis = row == 2 ? "X:" : (row == 3 ? "Y:" : "Z:");
            OLED_ShowString(row, 0, axis);
            OLED_ShowString(row, 66, axis);
        }
    }
    else
        OLED_ShowChinese(2, 0, "未连接");
}

/* 带上下限的循环加减，例如23小时再加1变为0小时。 */
static uint16_t Clock_Wrap(uint16_t value, int8_t direction,
                           uint16_t minimum, uint16_t maximum)
{
    if (direction > 0)
    {
        return (value >= maximum) ? minimum : value + 1U;
    }

    return (value <= minimum) ? maximum : value - 1U;
}

/* 修改编辑副本的年/月/日/时/分/秒；改年月后还会修正该月不存在的日期。 */
static void Clock_Adjust(MyRTC_DateTime *time,
                         uint8_t field, int8_t direction)
{
    switch (field)
    {
        case 0:
            time->year = Clock_Wrap(time->year, direction, 2000, 2099);
            break;

        case 1:
            time->month = Clock_Wrap(time->month, direction, 1, 12);
            break;

        case 2:
            time->day = Clock_Wrap(
                time->day, direction, 1,
                MyRTC_MonthDays(time->year, time->month));
            break;

        case 3:
            time->hour = Clock_Wrap(time->hour, direction, 0, 23);
            break;

        case 4:
            time->minute = Clock_Wrap(time->minute, direction, 0, 59);
            break;

        case 5:
            time->second = Clock_Wrap(time->second, direction, 0, 59);
            break;
    }

    /* 改变年份、月份后，日期不能超过该月实际天数。 */
    uint8_t maximumDay = MyRTC_MonthDays(time->year, time->month);

    if (time->day > maximumDay)
    {
        time->day = maximumDay;
    }
}

/* 显示传入时间；既可显示实时值，也可显示尚未保存的编辑值。 */
static void Clock_Draw(const MyRTC_DateTime *time)
{
    OLED_ShowNum(0, 0, time->year, 4);
    OLED_ShowNum(0, 30, time->month, 2);
    OLED_ShowNum(0, 48, time->day, 2);

    OLED_ShowNum(2, 0, time->hour, 2);
    OLED_ShowNum(2, 18, time->minute, 2);
    OLED_ShowNum(2, 36, time->second, 2);

    OLED_ShowString(0, 24, "-");
    OLED_ShowString(0, 42, "-");
    OLED_ShowString(2, 12, ":");
    OLED_ShowString(2, 30, ":");
}

/* 将小数四舍五入到整数，正负数分别加减0.5再截断。 */
static int16_t Clock_Round(float value)
{
    return (int16_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

/* turns表示转过几圈，乘2π变弧度；sin/cos求指针端点，屏幕y向下所以余弦前取负号。 */
static void Clock_Hand(float turns, uint8_t length)
{
    float angle = turns * 6.28318531f;
    OLED_DrawLine(100, 29,
                  100 + Clock_Round(sinf(angle) * length),
                  29 - Clock_Round(cosf(angle) * length));
}

/* 按当前模式更新时钟；模拟指针先画入草稿，交给后面主循环逐条横带传送。 */
static void Clock_HomeDraw(const MyRTC_DateTime *time)
{
    if (clockAnalog)
    {
        OLED_FrameClear();
        OLED_DrawCircle(100, 29, 18, 0);
        for (uint8_t i = 0; i < 12; i++)
        {
            float angle = i * 6.28318531f / 12.0f;
            uint8_t inner = (i % 3U == 0U) ? 13U : 15U;
            OLED_DrawLine(100 + Clock_Round(sinf(angle) * inner),
                          29 - Clock_Round(cosf(angle) * inner),
                          100 + Clock_Round(sinf(angle) * 17),
                          29 - Clock_Round(cosf(angle) * 17));
        }
        Clock_Hand((time->hour % 12U + time->minute / 60.0f +
                    time->second / 3600.0f) / 12.0f, 8);
        Clock_Hand((time->minute + time->second / 60.0f) / 60.0f, 12);
        Clock_Hand(time->second / 60.0f, 16);
        OLED_DrawCircle(100, 29, 1, 1);
        clockRenderPage = 1;
    }
    Clock_Draw(time);
}

/* 绘制时钟主页固定文字和电池占位图；BAT --表示未接电量测量电路。 */
static void Clock_Open(void)
{
    OLED_Fill(0x00);
    clockRenderPage = 0;
    OLED_ShowChinese(4, 0, clockAnalog ? "图形时钟" : "数字时钟");
    OLED_ShowChinese(6, 0, "切换");
    OLED_ShowString(6, 34, "1 2");
    OLED_ShowChinese(6, 72, "菜单");
    OLED_ShowString(6, 110, "3");

    /* No battery circuit is connected; leave the charge level unknown. */
    OLED_FrameClear();
    for (int16_t x = 108; x <= 121; x++)
    {
        OLED_DrawPoint(x, 1);
        OLED_DrawPoint(x, 6);
    }
    for (int16_t y = 1; y <= 6; y++)
    {
        OLED_DrawPoint(108, y);
        OLED_DrawPoint(121, y);
    }
    for (int16_t x = 122; x <= 124; x++)
    {
        OLED_DrawPoint(x, 3);
        OLED_DrawPoint(x, 4);
    }
    OLED_UpdateFrame(0, 1);
    OLED_ShowString(0, 70, "BAT --");
}

/* 显示当前编辑字段与保存/取消提示；field为0到5。 */
static void Clock_ShowField(uint8_t field, uint8_t saveFailed)
{
    static const char *names[6] = {"年份", "月份", "日期", "小时", "分钟", "秒"};
    for (uint8_t row = 4; row <= 7; row++)
        OLED_ShowString(row, 0, "                     ");
    OLED_ShowChinese(4, 0, names[field]);
    if (saveFailed)
        OLED_ShowChinese(4, 48, "保存失败");
    else
    {
        OLED_ShowChinese(4, 40, "长按");
        OLED_ShowString(4, 74, "3");
        OLED_ShowChinese(4, 84, "取消");
    }
    OLED_ShowString(6, 0, "1+ 2-");
    OLED_ShowChinese(6, 34, field == 5U ? "保存" : "下一项");
    OLED_ShowString(6, 88, "3");
}

/* 按毫秒差累加秒表，不依赖当前页面；100小时取余回零，无符号减法可处理计数回绕。 */
static void Stopwatch_Update(void)
{
    uint32_t now = HAL_GetTick();

    if (stopwatchRunning)
    {
        stopwatchElapsed =
            (stopwatchElapsed + (uint32_t)(now - stopwatchTick))
            % 360000000U;
    }

    stopwatchTick = now;
}

/* 把累计毫秒拆成小时、分钟、秒和百分之一秒，只负责显示。 */
static void Stopwatch_Draw(void)
{
    /* 整数除法舍掉小数；/1000把毫秒变为完整秒，%60再取当前分钟内的秒。 */
    uint32_t totalSeconds = stopwatchElapsed / 1000U;

    uint8_t hours = totalSeconds / 3600U;
    uint8_t minutes = (totalSeconds / 60U) % 60U;
    uint8_t seconds = totalSeconds % 60U;
    uint8_t hundredths = (stopwatchElapsed / 10U) % 100U;

    OLED_ShowNum(2, 0, hours, 2);
    OLED_ShowString(2, 12, ":");

    OLED_ShowNum(2, 18, minutes, 2);
    OLED_ShowString(2, 30, ":");

    OLED_ShowNum(2, 36, seconds, 2);
    OLED_ShowString(2, 48, ":");

    OLED_ShowNum(2, 54, hundredths, 2);

    OLED_ShowChinese(0, 80, stopwatchRunning ? "运行" : "停止");
}

/* 绘制秒表标题与操作提示，不重置已经累计的时间。 */
static void Stopwatch_Open(void)
{
    OLED_Fill(0x00);

    OLED_ShowChinese(0, 0, "秒表");
    OLED_ShowChinese(4, 0, "开始");
    OLED_ShowString(4, 34, "1");
    OLED_ShowChinese(4, 72, "停止");
    OLED_ShowString(4, 110, "2");
    OLED_ShowString(6, 0, "3");
    OLED_ShowChinese(6, 8, "清零");
    OLED_ShowChinese(6, 48, "长按");
    OLED_ShowString(6, 82, "3");
    OLED_ShowChinese(6, 90, "返回");
}

/* 扣除实际经过的毫秒，归零时启动蜂鸣；离开倒计时页面后仍需调用。 */
static void Countdown_Update(void)
{
    if (!countdownRunning)
        return;
    uint32_t now = HAL_GetTick();
    /* 例如上次1000、本次1025，则经过25毫秒；不能假设每轮循环恰好1毫秒。 */
    uint32_t elapsed = now - countdownTick;
    countdownTick = now;
    if (elapsed >= countdownRemaining)
    {
        countdownRemaining = 0;
        countdownRunning = 0;
        countdownFinished = 1;
        Buzzer_Alert();
    }
    else
        countdownRemaining -= elapsed;
}

/* 每次加减10秒，范围10秒到60分钟；同时记录作为下一次重来的设置时长。 */
static void Countdown_Adjust(int8_t direction)
{
    /* 加999再整除1000实现向上取整：剩余1到1000毫秒都按1秒处理。 */
    uint32_t seconds = (countdownRemaining + 999U) / 1000U;
    if (direction > 0)
        seconds = seconds >= 3590U ? 3600U : seconds + 10U;
    else
        seconds = seconds <= 20U ? 10U : seconds - 10U;
    countdownDuration = countdownRemaining = seconds * 1000U;
    countdownFinished = 0;
}

/* 剩余毫秒向上取整显示秒，避免还有不足1秒时提前显示00；按运行状态改变提示。 */
static void Countdown_Draw(void)
{
    /* 加999再整除1000实现向上取整：剩余1到1000毫秒都按1秒处理。 */
    uint32_t seconds = (countdownRemaining + 999U) / 1000U;
    uint8_t sounding = Buzzer_IsAlerting();
    OLED_ShowChinese(0, 80, sounding ? "响铃" :
                     (countdownRunning ? "运行" :
                      (countdownFinished ? "完成" :
                       (countdownRemaining == countdownDuration ? "设置" : "暂停"))));
    OLED_ShowNum(2, 40, seconds / 3600U, 2);
    OLED_ShowString(2, 52, ":");
    OLED_ShowNum(2, 58, (seconds / 60U) % 60U, 2);
    OLED_ShowString(2, 70, ":");
    OLED_ShowNum(2, 76, seconds % 60U, 2);
    OLED_ShowChinese(6, 0, sounding ? "消音" :
                     (countdownRunning ? "暂停" :
                      (countdownFinished ? "重来" : "开始")));
    uint8_t style = sounding ? 2U : (countdownRunning ? 1U : 0U);
    if (style != countdownDrawStyle)
    {
        OLED_ShowString(4, 0, "                     ");
        OLED_ShowString(5, 0, "                     ");
        if (sounding)
            OLED_ShowChinese(4, 24, "任意键消音");
        else if (countdownRunning)
            OLED_ShowChinese(4, 40, "计时中");
        else
        {
            OLED_ShowChinese(4, 0, "加");
            OLED_ShowString(4, 20, "1");
            OLED_ShowChinese(4, 48, "减");
            OLED_ShowString(4, 68, "2");
            OLED_ShowNum(4, 88, 10, 2);
            OLED_ShowChinese(4, 104, "秒");
        }
        countdownDrawStyle = style;
    }
}

/* 进入倒计时页只重绘界面，不清空后台计时状态。 */
static void Countdown_Open(void)
{
    OLED_Fill(0x00);
    OLED_ShowChinese(0, 0, "倒计时");
    OLED_ShowString(6, 36, "3");
    OLED_ShowChinese(6, 48, "长按");
    OLED_ShowString(6, 82, "3");
    OLED_ShowChinese(6, 90, "返回");
    countdownDrawStyle = 255;
    Countdown_Draw();
}

/* 按应用保存的灯状态更新文字。 */
static void Light_Draw(uint8_t on)
{
    OLED_ShowChinese(2, 0, on ? "已开启" : "已关闭");
}

/* 显示灯光控制界面，进入页面不会改变灯原来的状态。 */
static void Light_Open(uint8_t on)
{
    OLED_Fill(0x00);

    OLED_ShowChinese(0, 0, "灯光");
    OLED_ShowChinese(4, 0, "开启");
    OLED_ShowString(4, 34, "1");
    OLED_ShowChinese(4, 72, "关闭");
    OLED_ShowString(4, 110, "2");
    OLED_ShowChinese(6, 0, "返回");
    OLED_ShowString(6, 36, "3");

    Light_Draw(on);
}

/* 把光照百分比映射到四档亮度；升档和降档阈值相差8个百分点以减少跳档。 */
static void Brightness_Update(void)
{
    /* 四档屏幕对比度参数，不是百分比；通过屏幕控制命令改变发光亮度。 */
    static const uint8_t contrast[4] = {0x20, 0x60, 0xA0, 0xCF};
    /* 相对光照的三个分档边界；上下再加减4个百分点，形成回差。 */
    static const uint8_t edges[3] = {25, 50, 75};
    if (brightnessAuto)
    {
        if (!LightSensor_IsValid())
            return;
        uint8_t percent = LightSensor_GetPercent();
        /* Different up/down thresholds prevent flickering near a boundary. */
        while (brightnessAutoLevel < 3U && percent >= edges[brightnessAutoLevel] + 4U)
            brightnessAutoLevel++;
        while (brightnessAutoLevel > 0U && percent <= edges[brightnessAutoLevel - 1U] - 4U)
            brightnessAutoLevel--;
    }
    OLED_SetContrast(contrast[brightnessAuto ? brightnessAutoLevel : brightnessManualLevel]);
}

/* 显示相对光照、采样值、亮度级和模式；无有效采样时用占位符提示。 */
static void Brightness_Draw(void)
{
    uint8_t valid = LightSensor_IsValid();
    OLED_ShowChinese(2, 0, "光照");
    if (valid)
        OLED_ShowNum(2, 40, LightSensor_GetPercent(), 3);
    else
        OLED_ShowString(2, 40, "---");
    OLED_ShowString(2, 60, "%");
    OLED_ShowChinese(2, 88, "级");
    OLED_ShowNum(2, 110, (brightnessAuto ? brightnessAutoLevel : brightnessManualLevel) + 1U, 1);
    if (valid != brightnessDrawValid)
    {
        OLED_ShowString(4, 0, "                     ");
        OLED_ShowString(5, 0, "                     ");
        brightnessDrawValid = valid;
    }
    if (valid)
    {
        OLED_ShowChinese(4, 0, "采样");
        OLED_ShowNum(4, 40, LightSensor_GetValue(), 4);
    }
    else
        OLED_ShowChinese(4, 0, "采样失败");
    OLED_ShowChinese(6, 0, brightnessAuto ? "自动" : "手动");
    OLED_ShowChinese(6, 72, !brightnessAuto ? "调亮" : (valid ? "反向" : "重试"));
}

/* 绘制亮度页面固定内容，再显示当前读数。 */
static void Brightness_Open(void)
{
    OLED_Fill(0x00);
    OLED_ShowChinese(0, 0, "亮度");
    OLED_ShowChinese(0, 72, "返回");
    OLED_ShowString(0, 110, "3");
    OLED_ShowString(6, 36, "1");
    OLED_ShowString(6, 110, "2");
    brightnessDrawValid = 255;
    Brightness_Draw();
}

/* 姿态角取整并显示正负号，单位为度。 */
static void Attitude_ShowValue(uint8_t row, float value)
{
    int32_t rounded =
        (int32_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
    OLED_ShowString(row, 42, rounded < 0 ? "-" : "+");
    uint32_t magnitude =
        rounded < 0 ? (uint32_t)(-rounded) : (uint32_t)rounded;
    OLED_ShowNum(row, 48, magnitude, 3);
}

/* 更新横滚、俯仰、偏航三个角度，偏航只是相对方向。 */
static void Attitude_Draw(const MPU6050_Angles *angles)
{
    Attitude_ShowValue(0, angles->roll);
    Attitude_ShowValue(2, angles->pitch);
    Attitude_ShowValue(4, angles->yaw);
}

/* 绘制姿态页，ready决定显示角度标签还是未就绪提示。 */
static void Attitude_Open(uint8_t ready)
{
    OLED_Fill(0x00);
    OLED_ShowChinese(6, 0, ready ? "校准" : "重试");
    OLED_ShowString(6, 36, "1");
    OLED_ShowChinese(6, 72, "返回");
    OLED_ShowString(6, 110, "3");
    if (ready)
    {
        OLED_ShowChinese(0, 0, "横滚");
        OLED_ShowChinese(2, 0, "俯仰");
        OLED_ShowChinese(4, 0, "偏航");
        OLED_ShowChinese(0, 80, "姿态");
        OLED_ShowChinese(2, 80, "度");
    }
    else
    {
        OLED_ShowChinese(0, 0, "姿态");
        OLED_ShowChinese(2, 0, "未就绪");
    }
}

/* 先提示保持静止，再初始化并校准；此过程有等待，完成后才回到主循环。 */
static uint8_t Attitude_Calibrate(void)
{
    OLED_Fill(0x00);
    OLED_ShowChinese(0, 0, "保持静止");
    OLED_ShowChinese(2, 0, "正在校准");
    uint8_t connected = MPU6050_Init();
    uint8_t ready = connected;

    if (ready)
    {
        ready = MPU6050_Calibrate();
    }

    Attitude_Open(ready);
    if (!ready)
        OLED_ShowChinese(2, 0, connected ? "校准失败" : "未连接");
    return ready;
}

/* 把角度差折回-180到180度，避免跨越正负180度时气泡突然跳到另一边。 */
static float Level_Difference(float angle, float reference)
{
    float difference = angle - reference;
    if (difference > 180.0f) difference -= 360.0f;
    if (difference < -180.0f) difference += 360.0f;
    return difference;
}

/* 把相对零点的倾角映射为气泡偏移，并把偏移半径限制到11像素。 */
static void Level_Draw(const MPU6050_Angles *angles,
                        float zeroRoll, float zeroPitch)
{
    float dx = -0.6f * Level_Difference(angles->roll, zeroRoll);
    float dy = 0.6f * Level_Difference(angles->pitch, zeroPitch);
    float distance = sqrtf(dx * dx + dy * dy);

    if (distance > 11.0f)
    {
        dx *= 11.0f / distance;
        dy *= 11.0f / distance;
    }

    int16_t bubbleX = 64 + (int16_t)(dx >= 0.0f ? dx + 0.5f : dx - 0.5f);
    int16_t bubbleY = 32 + (int16_t)(dy >= 0.0f ? dy + 0.5f : dy - 0.5f);

    OLED_FrameClear();
    OLED_DrawCircle(64, 32, 14, 0);

    for (int16_t i = -2; i <= 2; i++)
    {
        OLED_DrawPoint(64 + i, 32);
        OLED_DrawPoint(64, 32 + i);
    }

    OLED_DrawCircle(bubbleX, bubbleY, 3, 1);
}

/* 清除旧气泡区域并提示读取失败，避免残留画面被误当成有效值。 */
static void Level_ReadError(void)
{
    OLED_FrameClear();
    OLED_UpdateFrameArea(2, 4, 49, 31);
    OLED_ShowChinese(2, 0, "读取失败");
    OLED_ShowChinese(6, 0, "重试");
    OLED_ShowString(6, 36, "1");
}

/* 进入水平仪页先初始化和校准，成功返回1；失败返回0并提示原因。 */
static uint8_t Level_Open(void)
{
    OLED_Fill(0x00);
    OLED_ShowChinese(0, 0, "保持静止");
    OLED_ShowChinese(2, 0, "正在校准");
    uint8_t connected = MPU6050_Init();
    uint8_t ready = connected;
    if (ready)
        ready = MPU6050_Calibrate();
    OLED_Fill(0x00);
    OLED_ShowChinese(0, 0, "水平仪");
    OLED_ShowChinese(6, 72, "返回");
    OLED_ShowString(6, 110, "3");
    OLED_ShowChinese(6, 0, ready ? "归零" : "重试");
    OLED_ShowString(6, 36, "1");
    if (!ready)
        OLED_ShowChinese(2, 0, connected ? "校准失败" : "未连接");
    return ready;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* 系统时钟已配置：HSE（高速外部晶振）8MHz，经PLL（锁相环倍频器）乘9得到72MHz。
   * AHB系统总线72MHz，APB1外设总线36MHz，APB2外设总线72MHz；ADC时钟再除6为12MHz。
   * RCC是复位与时钟控制模块；上述频率由SystemClock_Config()设置。 */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_RTC_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  /* 硬件基础配置完成后，再初始化自定义驱动；先设蜂鸣器关闭，再准备时钟、显示和光照。 */
  Encoder_Init();
  Buzzer_Init();
  MyRTC_Init();
  OLED_Init();
  LightSensor_Init(&hadc1);
  LightSensor_Update();
  Brightness_Update();
  LED_Off();

  /* dateTime保存实时读数，editTime保存设置页面的编辑副本；确认保存前不会修改RTC。 */
  MyRTC_DateTime dateTime;
  MyRTC_DateTime editTime;

  /* page决定当前页面；screenOff只表示显示关闭，主循环和后台计时仍然运行。 */
  uint8_t page = PAGE_CLOCK;
  /* 编辑字段0到5依次是年、月、日、时、分、秒。 */
  uint8_t field = 0;
  /* 秒正常范围0到59，故255一定不同，促使第一次就显示时间。 */
  uint8_t lastSecond = 255;
  uint8_t lightOn = 0;
  uint8_t screenOff = 0;

  /* 以下Refresh变量控制“多久重画一次”；计时本身每轮更新，与画屏周期不同。 */
  uint32_t stopwatchRefresh = 0;
  uint32_t countdownRefresh = 0;
  uint32_t brightnessRefresh = 0;

  Clock_Open();

  MPU6050_Data motionData;
  uint8_t motionReady = 0;
  uint32_t motionRefresh = 0;

  MPU6050_Angles attitudeData;
  uint8_t attitudeReady = 0;
  uint32_t attitudeSample = 0;
  uint32_t attitudeRefresh = 0;

  MPU6050_Angles levelData;
  uint8_t levelReady = 0;
  uint8_t levelStatus = 0;
  uint8_t levelRenderPage = 0;
  uint32_t levelSample = 0;
  uint32_t levelRefresh = 0;
  float levelZeroRoll = 0.0f;
  float levelZeroPitch = 0.0f;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
    {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  /* 第一部分：每轮都更新后台任务，因此离开对应页面或息屏也不会停掉倒计时。 */
	  Stopwatch_Update();
      Countdown_Update();
      Buzzer_Update();
      LightSensor_Update();
      Brightness_Update();
      OLED_Service();
      /* 第二部分：读取输入。event为松手事件，pressEvent为按下事件，encoderSteps为累计旋转步数。 */
      uint8_t event = Key_GetEvent();
      uint8_t pressEvent = Key_GetPressEvent();
      int8_t encoderSteps = Encoder_GetDelta();

      /* 消音优先：这次按键只负责停止提示，避免同时触发当前页面的其他动作。 */
      if (Buzzer_IsAlerting() &&
          (event != KEY_EVENT_NONE || pressEvent != KEY_EVENT_NONE))
      {
          Buzzer_Stop();
          if (event == KEY_EVENT_NONE)
              silenceKeyHeld = 1;
          pressEvent = KEY_EVENT_NONE;
          if (!screenOff)
              event = KEY_EVENT_NONE;
      }
      if (silenceKeyHeld)
      {
          pressEvent = KEY_EVENT_NONE;
          if (event != KEY_EVENT_NONE)
          {
              silenceKeyHeld = 0;
              if (!screenOff)
                  event = KEY_EVENT_NONE;
          }
      }

      /* 息屏时仍已完成上面的后台更新；有松手事件就恢复主页，然后continue开始下一轮。 */
      if (screenOff)
      {
          if (event != KEY_EVENT_NONE)
          {
              MyRTC_Get(&dateTime);
              Clock_Open();
              Clock_HomeDraw(&dateTime);
              if (clockAnalog)
              {
                  OLED_UpdateFrameArea(1, 5, 80, 40);
                  clockRenderPage = 0;
              }
              lastSecond = dateTime.second;
              OLED_DisplayOn();
              screenOff = 0;
          }
          /* Consume the wake event instead of opening the menu as well. */
          continue;
      }

      /* 第三部分：页面状态机。相同按键在不同页面含义不同，只执行当前case的操作。 */
      switch (page)
      {
          case PAGE_CLOCK:
          {
              /* 主页操作：键1或键2切换显示，短按3进菜单，长按3松手息屏。 */
              if (event == KEY_EVENT_1 || event == KEY_EVENT_2)
              {
                  clockAnalog = !clockAnalog;
                  lastSecond = 255;
                  Clock_Open();
              }
              else if (event == KEY_EVENT_3)
              {
                  page = PAGE_MENU;
                  Menu_Open();
              }
              else if (event == KEY_EVENT_3_LONG)
              {
                  OLED_DisplayOff();
                  screenOff = 1;
                  continue;
              }
              break;
          }

          case PAGE_MENU:
          {
              /* 旋钮只在菜单页使用；其他页面不会把旋转累计成日后的菜单操作。 */
              Menu_Move(encoderSteps);
              uint8_t action = Menu_HandleEvent(event);

              if (action == MENU_ACTION_HOME)
              {
                  page = PAGE_CLOCK;
                  lastSecond = 255;
                  Clock_Open();
              }
              else if (action == MENU_ACTION_SET_TIME)
              {
                  /* 把当前日期读入编辑副本；只有最后一项确认保存才调用MyRTC_Set写回硬件。 */
                  MyRTC_Get(&editTime);

                  page = PAGE_SET_TIME;
                  field = 0;

                  OLED_Fill(0x00);
                  Clock_Draw(&editTime);
                  Clock_ShowField(field, 0);
              }
              else if (action == MENU_ACTION_STOPWATCH)
              {
                  page = PAGE_STOPWATCH;
                  Stopwatch_Open();
              }
              else if (action == MENU_ACTION_LIGHT)
              {
                  page = PAGE_LIGHT;
                  Light_Open(lightOn);
              }
              else if (action == MENU_ACTION_MOTION)
              {
                  motionReady = MPU6050_Init();

                  page = PAGE_MOTION;
                  Motion_Open(motionReady);
              }
              else if (action == MENU_ACTION_ATTITUDE)
              {
                  page = PAGE_ATTITUDE;
                  attitudeReady = Attitude_Calibrate();
                  attitudeSample = 0;
                  attitudeRefresh = 0;
              }
              else if (action == MENU_ACTION_BRIGHTNESS)
              {
                  page = PAGE_BRIGHTNESS;
                  Brightness_Open();
              }
              else if (action == MENU_ACTION_COUNTDOWN)
              {
                  page = PAGE_COUNTDOWN;
                  Countdown_Open();
              }
              else if (action == MENU_ACTION_EMOJI)
              {
                  page = PAGE_EMOJI;
                  Emoji_Open();
              }
              else if (action == MENU_ACTION_DINO)
              {
                  page = PAGE_DINO;
                  Dino_Open();
              }
              else if (action == MENU_ACTION_LEVEL)
              {
                  page = PAGE_LEVEL;
                  levelZeroRoll = 0.0f;
                  levelZeroPitch = 0.0f;
                  levelStatus = 0;
                  levelRenderPage = 0;
                  levelSample = 0;
                  levelRefresh = 0;
                  levelReady = Level_Open();
              }
              break;
          }

          case PAGE_BRIGHTNESS:
          {
              if (event == KEY_EVENT_1)
              {
                  /* 由自动切到手动时先继承当前亮度，避免模式切换造成突然明暗变化。 */
                  if (brightnessAuto)
                      brightnessManualLevel = brightnessAutoLevel;
                  brightnessAuto = !brightnessAuto;
                  Brightness_Update();
              }
              else if (event == KEY_EVENT_2)
              {
                  if (!brightnessAuto)
                      brightnessManualLevel = (brightnessManualLevel + 1U) % 4U;
                  else if (!LightSensor_IsValid())
                  {
                      LightSensor_Init(&hadc1);
                      LightSensor_Update();
                  }
                  else
                      LightSensor_ToggleDirection();
                  Brightness_Update();
              }
              else if (event == KEY_EVENT_3 || event == KEY_EVENT_3_LONG)
              {
                  page = PAGE_MENU;
                  Menu_Open();
              }
              break;
          }

          case PAGE_COUNTDOWN:
          {
              if (event == KEY_EVENT_1 && !countdownRunning)
                  Countdown_Adjust(1);
              else if (event == KEY_EVENT_2 && !countdownRunning)
                  Countdown_Adjust(-1);
              else if (event == KEY_EVENT_3)
              {
                  if (countdownRunning)
                      countdownRunning = 0;
                  else
                  {
                      if (countdownRemaining == 0U)
                          countdownRemaining = countdownDuration;
                      countdownFinished = 0;
                      /* 恢复计时先更新起点，否则暂停期间的时间也会被错误扣掉。 */
                      countdownTick = HAL_GetTick();
                      countdownRunning = 1;
                  }
              }
              else if (event == KEY_EVENT_3_LONG)
              {
                  page = PAGE_MENU;
                  Menu_Open();
              }
              break;
          }

          case PAGE_STOPWATCH:
          {
              if (event == KEY_EVENT_1)
              {
                  stopwatchRunning = 1;
              }
              else if (event == KEY_EVENT_2)
              {
                  stopwatchRunning = 0;
              }
              else if (event == KEY_EVENT_3)
              {
                  stopwatchRunning = 0;
                  stopwatchElapsed = 0;
              }
              else if (event == KEY_EVENT_3_LONG)
              {
                  page = PAGE_MENU;
                  Menu_Open();
              }
              break;
          }

          case PAGE_LIGHT:
          {
              if (event == KEY_EVENT_1)
              {
                  lightOn = 1;
                  LED_On();
                  Light_Draw(lightOn);
              }
              else if (event == KEY_EVENT_2)
              {
                  lightOn = 0;
                  LED_Off();
                  Light_Draw(lightOn);
              }
              else if (event == KEY_EVENT_3 ||
                       event == KEY_EVENT_3_LONG)
              {
                  page = PAGE_MENU;
                  Menu_Open();
              }
              break;
          }

          case PAGE_MOTION:
          {
              if (event == KEY_EVENT_1)
              {
                  motionReady = MPU6050_Init();
                  motionRefresh = 0;
                  Motion_Open(motionReady);
              }
              else if (event == KEY_EVENT_3 ||
                  event == KEY_EVENT_3_LONG)
              {
                  page = PAGE_MENU;
                  Menu_Open();
              }
              break;
          }


          case PAGE_ATTITUDE:
          {
              if (event == KEY_EVENT_1)
              {
                  attitudeReady = Attitude_Calibrate();
              }
              else if (event == KEY_EVENT_3 ||
                       event == KEY_EVENT_3_LONG)
              {
                  page = PAGE_MENU;
                  Menu_Open();
              }
              break;
          }


          case PAGE_LEVEL:
          {
              if (event == KEY_EVENT_1)
              {
                  if (!levelReady)
                  {
                      levelRenderPage = 0;
                      levelStatus = 0;
                      levelZeroRoll = 0.0f;
                      levelZeroPitch = 0.0f;
                      levelSample = 0;
                      levelRefresh = 0;
                      levelReady = Level_Open();
                  }
                  else if (MPU6050_UpdateAngles(&levelData))
                  {
                      /* 只更改界面参考零点，不是在重新校准传感器的角速度零偏。 */
                      levelZeroRoll = levelData.roll;
                      levelZeroPitch = levelData.pitch;
                      levelRenderPage = 0;
                      levelRefresh = 0;
                  }
                  else
                  {
                      levelReady = 0;
                      levelStatus = 2;
                      levelRenderPage = 0;
                      Level_ReadError();
                  }
              }
              else if (event == KEY_EVENT_3 ||
                       event == KEY_EVENT_3_LONG)
              {
                  page = PAGE_MENU;
                  Menu_Open();
              }
              break;
          }

          case PAGE_EMOJI:
          {
              if (event == KEY_EVENT_3 || event == KEY_EVENT_3_LONG)
              {
                  page = PAGE_MENU;
                  Menu_Open();
              }
              else
                  Emoji_HandleEvent(event);
              break;
          }

          case PAGE_DINO:
          {
              if (event == KEY_EVENT_3_LONG)
              {
                  page = PAGE_MENU;
                  Menu_Open();
              }
              /* 游戏需要按下立即响应；普通菜单用松手事件，能识别长短按。 */
              else if (pressEvent == KEY_EVENT_1)
                  Dino_HandleEvent(pressEvent);
              break;
          }

          case PAGE_SET_TIME:
          {
              if (event != KEY_EVENT_NONE)
              {
                  uint8_t saveFailed = 0;

                  if (event == KEY_EVENT_1)
                  {
                      Clock_Adjust(&editTime, field, 1);
                  }
                  else if (event == KEY_EVENT_2)
                  {
                      Clock_Adjust(&editTime, field, -1);
                  }
                  else if (event == KEY_EVENT_3)
                  {
                      if (field < 5U)
                      {
                          field++;
                      }
                      else if (MyRTC_Set(&editTime))
                      {
                          page = PAGE_MENU;
                          Menu_Open();
                      }
                      else
                      {
                          saveFailed = 1;
                      }
                  }
                  else if (event == KEY_EVENT_3_LONG)
                  {
                      page = PAGE_MENU;
                      Menu_Open();
                  }

                  if (page == PAGE_SET_TIME)
                  {
                      Clock_Draw(&editTime);
                      Clock_ShowField(field, saveFailed);
                  }
              }
              break;
          }
      }

      /* 第四部分：按需刷新。时钟每秒更新一次；模拟指针分条传送，让按键能及时被轮询。 */
      if (page == PAGE_CLOCK)
      {
          MyRTC_Get(&dateTime);

          if (dateTime.second != lastSecond)
          {
              lastSecond = dateTime.second;
              Clock_HomeDraw(&dateTime);
          }
          /* Transfer one strip per loop so drawing does not block key polling. */
          if (clockRenderPage != 0)
          {
              OLED_UpdateFrameArea(clockRenderPage, 1, 80, 40);
              if (++clockRenderPage > 5U)
              {
                  clockRenderPage = 0;
              }
          }
      }

      /* 下面集中按时间条件刷新当前页面。其他页面的绘图不执行，但前面的后台计时一直执行。 */
      uint32_t now = HAL_GetTick();

      /* 采样与画屏分开：约10毫秒更新姿态，约100毫秒准备气泡画面，再分条传送。 */
      if (page == PAGE_LEVEL && levelReady)
      {
          if ((uint32_t)(now - levelSample) >= 10U)
          {
              levelSample = now;

              if (MPU6050_UpdateAngles(&levelData))
              {
                  if (levelStatus == 2U)
                  {
                      OLED_ShowString(0, 66, "      ");
                      OLED_ShowString(1, 66, "      ");
                  }
                  levelStatus = 1;
              }
              else
              {
                  levelReady = 0;
                  levelStatus = 2;
                  levelRenderPage = 0;
                  Level_ReadError();
              }
          }

          if (levelStatus == 1U && levelRenderPage == 0U &&
              (uint32_t)(now - levelRefresh) >= 100U)
          {
              Level_Draw(&levelData, levelZeroRoll, levelZeroPitch);
              levelRefresh = now;
              levelRenderPage = 2;
          }

          /* Transfer one 8-pixel strip per loop to keep key polling responsive. */
          if (levelRenderPage != 0U)
          {
              OLED_UpdateFrameArea(levelRenderPage, 1, 49, 31);
              levelRenderPage++;
              if (levelRenderPage > 5U)
              {
                  levelRenderPage = 0;
              }
          }
      }


      if (page == PAGE_ATTITUDE && attitudeReady &&
          (uint32_t)(now - attitudeSample) >= 10U)
      {
          attitudeSample = now;

          if (MPU6050_UpdateAngles(&attitudeData))
          {
              if ((uint32_t)(now - attitudeRefresh) >= 100U)
              {
                  attitudeRefresh = now;
                  Attitude_Draw(&attitudeData);
              }
          }
          else
          {
              attitudeReady = 0;
              Attitude_Open(0);
              OLED_ShowChinese(2, 0, "读取失败");
          }
      }


      if (page == PAGE_MOTION && motionReady &&
          ((uint32_t)(now - motionRefresh) >= 100U ||
           event != KEY_EVENT_NONE))
      {
          motionRefresh = now;

          if (MPU6050_Read(&motionData))
          {
              Motion_Draw(&motionData);
          }
          else
          {
              for (uint8_t row = 2; row <= 5; row++)
                  OLED_ShowString(row, 0, "                     ");
              OLED_ShowChinese(2, 0, "读取失败");
              motionReady = 0;
          }
      }

      if (page == PAGE_EMOJI)
          Emoji_Update();

      if (page == PAGE_DINO)
          Dino_Update();

      if (page == PAGE_BRIGHTNESS &&
          ((uint32_t)(now - brightnessRefresh) >= 100U || event != KEY_EVENT_NONE))
      {
          brightnessRefresh = now;
          Brightness_Draw();
      }

      if (page == PAGE_COUNTDOWN &&
          ((uint32_t)(now - countdownRefresh) >= 100U || event != KEY_EVENT_NONE))
      {
          countdownRefresh = now;
          Countdown_Draw();
      }

      if (page == PAGE_STOPWATCH &&
          ((uint32_t)(now - stopwatchRefresh) >= 100U ||
           event != KEY_EVENT_NONE))
      {
          stopwatchRefresh = now;
          Stopwatch_Draw();
      }

    }

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_ADC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */
  /* ADC1配置由CubeMX生成：PA1通道1，软件触发、单次转换、右对齐，采样时间239.5周期。 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */
  /* RTC使用32.768kHz低速外部晶振，自动分频为每秒一次计数；日期换算在myrtc.c。 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_ALARM;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* GPIO=通用输入输出：先使能端口时钟再配置引脚。
   * PB8/PB9和PB10/PB11是开漏软件总线；PB15灯与PA8蜂鸣器低电平有效。
   * PA2/PA3双边沿触发EXTI外部中断；NVIC是管理中断优先级和开关的控制器。 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_15|GPIO_PIN_8
                          |GPIO_PIN_9, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : ENC_A_Pin ENC_B_Pin */
  GPIO_InitStruct.Pin = ENC_A_Pin|ENC_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA4 PA6 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB10 PB11 PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BUZZER_Pin */
  GPIO_InitStruct.Pin = BUZZER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI2_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* HAL外部中断回调：引脚边沿由中断入口交给这里，再转给编码器驱动。 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    Encoder_OnEdge(GPIO_Pin);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* 初始化失败会到这里关中断并停住；它是故障处理，不是正常退出程序的入口。 */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
