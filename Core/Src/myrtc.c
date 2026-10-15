/* RTC（实时时钟）驱动：硬件保存从2000年1月1日零时起的累计秒数，本文件负责换算年月日。
 * BKP 是备份寄存器，保存初始化标记；完全断电后能否继续走时取决于备用电源是否接好。
 * 日期时间请通过 MyRTC_Get/Set 读写，不要混用把计数当作一天内时间的接口。 */
#include "main.h"
#include "myrtc.h"

/* 人为选定的初始化标记，不是时间；备份寄存器与它相等表示本工程曾设置RTC。 */
#define MYRTC_MAGIC 0xA526U

/* 按公历规则判断闰年：四年一闰，百年不闰，四百年再闰。 */
static uint8_t MyRTC_IsLeapYear(uint16_t year)
{
    return ((year % 4U == 0U) && (year % 100U != 0U)) ||
           (year % 400U == 0U);
}

/* 返回某年某月天数；month必须是1到12，二月按闰年规则处理。 */
uint8_t MyRTC_MonthDays(uint16_t year, uint8_t month)
{
    static const uint8_t days[12] =
    {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if (month == 2U && MyRTC_IsLeapYear(year))
    {
        return 29;
    }

    return days[month - 1U];
}

/* 先高位、再低位、再高位读取；若高位变化则重读，避免刚好进位时拼出错误秒数。 */
static uint32_t MyRTC_ReadCounter(void)
{
    uint16_t high1;
    uint16_t high2;
    uint16_t low;

    /* 避免读取高、低部分期间发生进位，得到不一致的秒数。 */
    do
    {
        high1 = (uint16_t)RTC->CNTH;
        low = (uint16_t)RTC->CNTL;
        high2 = (uint16_t)RTC->CNTH;
    }
    while (high1 != high2);

    return ((uint32_t)high1 << 16U) | low;
}

/* 等待RTC上一次写入完成，超过1秒返回失败，防止无限等候。 */
static uint8_t MyRTC_WaitReady(void)
{
    uint32_t startedAt = HAL_GetTick();

    while ((RTC->CRL & RTC_CRL_RTOFF) == 0U)
    {
        if ((uint32_t)(HAL_GetTick() - startedAt) >= 1000U)
        {
            return 0;
        }
    }

    return 1;
}

/* 等待就绪后进入配置模式，把32位秒数拆成高低16位写入，再退出配置。 */
static uint8_t MyRTC_WriteCounter(uint32_t seconds)
{
    if (!MyRTC_WaitReady())
    {
        return 0;
    }

    SET_BIT(RTC->CRL, RTC_CRL_CNF);

    RTC->CNTH = seconds >> 16U;
    RTC->CNTL = seconds & 0xFFFFU;

    CLEAR_BIT(RTC->CRL, RTC_CRL_CNF);

    return MyRTC_WaitReady();
}

/* 检查2000到2099年的日期是否合法，再转换为累计秒数写入；返回1成功、0失败。 */
uint8_t MyRTC_Set(const MyRTC_DateTime *time)
{
    if (time == NULL ||
        time->year < 2000U || time->year > 2099U ||
        time->month < 1U || time->month > 12U ||
        time->day < 1U ||
        time->hour > 23U ||
        time->minute > 59U ||
        time->second > 59U)
    {
        return 0;
    }

    if (time->day > MyRTC_MonthDays(time->year, time->month))
    {
        return 0;
    }

    uint32_t days = 0;

    for (uint16_t year = 2000; year < time->year; year++)
    {
        days += MyRTC_IsLeapYear(year) ? 366U : 365U;
    }

    for (uint8_t month = 1; month < time->month; month++)
    {
        days += MyRTC_MonthDays(time->year, month);
    }

    days += time->day - 1U;

    /* 一天86400秒，加上当天时分秒，得到自2000年起的累计秒数。 */
    uint32_t seconds = days * 86400U
                     + time->hour * 3600U
                     + time->minute * 60U
                     + time->second;

    if (!MyRTC_WriteCounter(seconds))
    {
        return 0;
    }

    /* 写入成功后才标记已初始化；不能据此保证断电走时，仍取决于备用供电。 */
    BKP->DR1 = MYRTC_MAGIC;

    return 1;
}

/* 允许访问备份区；只有初始化标记不存在才写默认日期，避免普通复位时每次重置时间。 */
void MyRTC_Init(void)
{
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_BKP_CLK_ENABLE();

    /* 关闭 RTC 外部信号输出，避免占用 PC13。 */
    CLEAR_BIT(BKP->RTCCR,
              BKP_RTCCR_CCO | BKP_RTCCR_ASOE | BKP_RTCCR_ASOS);

    if (BKP->DR1 != MYRTC_MAGIC)
    {
        const MyRTC_DateTime initial =
        {
            2026, 10, 14, 12, 0, 0
        };

        if (!MyRTC_Set(&initial))
        {
            Error_Handler();
        }
    }
}

/* 读取累计秒数，依次拆出时分秒，再减去年数和月数得到年月日。 */
void MyRTC_Get(MyRTC_DateTime *time)
{
    uint32_t seconds = MyRTC_ReadCounter();
    uint32_t days = seconds / 86400U;
    uint32_t remaining = seconds % 86400U;

    time->hour = remaining / 3600U;
    remaining %= 3600U;

    time->minute = remaining / 60U;
    time->second = remaining % 60U;

    uint16_t year = 2000;

    while (1)
    {
        uint32_t yearDays = MyRTC_IsLeapYear(year) ? 366U : 365U;

        if (days < yearDays)
        {
            break;
        }

        days -= yearDays;
        year++;
    }

    uint8_t month = 1;

    while (days >= MyRTC_MonthDays(year, month))
    {
        days -= MyRTC_MonthDays(year, month);
        month++;
    }

    time->year = year;
    time->month = month;
    time->day = days + 1U;
}
