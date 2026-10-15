/* myrtc.h：对外接口说明。先看本文件了解如何调用，再到同名.c查看实现。
 * 头文件声明函数的名字、参数与返回类型；#ifndef/#define/#endif避免重复包含。 */
#ifndef MYRTC_H
#define MYRTC_H

#include <stdint.h>

/* 结构体把年、月、日、时、分、秒打包成一个变量；月份1到12，日从1开始，时间使用24小时制。 */
typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} MyRTC_DateTime;

/* 允许访问备份区；只有初始化标记不存在才写默认日期，避免普通复位时每次重置时间。 */
void MyRTC_Init(void);
/* 读取累计秒数，依次拆出时分秒，再减去年数和月数得到年月日。 */
void MyRTC_Get(MyRTC_DateTime *time);

/* 支持 2000～2099 年；成功返回 1，无效日期或写入失败返回 0。 */
/* 检查2000到2099年的日期是否合法，再转换为累计秒数写入；返回1成功、0失败。 */
uint8_t MyRTC_Set(const MyRTC_DateTime *time);
/* month 的有效范围为 1～12。 */
/* 返回某年某月天数；month必须是1到12，二月按闰年规则处理。 */
uint8_t MyRTC_MonthDays(uint16_t year, uint8_t month);

#endif
