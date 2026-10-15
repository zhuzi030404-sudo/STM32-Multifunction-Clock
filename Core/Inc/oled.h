/* oled.h：对外接口说明。先看本文件了解如何调用，再到同名.c查看实现。
 * 头文件声明函数的名字、参数与返回类型；#ifndef/#define/#endif避免重复包含。 */
#ifndef OLED_H
#define OLED_H

#include <stdint.h>

/* 初始化缓存和微秒延时，等待屏幕上电，再尝试连接并清屏。 */
void OLED_Init(void);
/* 主循环维护函数：断线每100毫秒重试，连接后每次最多恢复一条8像素横带。 */
void OLED_Service(void);
/* 记录希望亮屏并发送0xAF；若断线，恢复连接后仍按这个状态处理。 */
void OLED_DisplayOn(void);
/* 记录希望息屏并发送0xAE；这里只关显示输出，主控程序继续运行。 */
void OLED_DisplayOff(void);
/* 0x81命令调整对比度以改变亮度；相同数值不重复发送，断线时仍保存目标值。 */
void OLED_SetContrast(uint8_t contrast);
/* 用一个字节图案填满屏幕：0x00全灭、0xFF全亮，其他值可形成条纹。 */
void OLED_Fill(uint8_t pattern);
/* 早期连屏检查用的固定Hello显示函数。 */
void OLED_ShowHello(uint8_t page, uint8_t x);
/* 在指定page和x显示定长十进制数；长度决定显示位数，前导空位用0补足。 */
void OLED_ShowNum(uint8_t page, uint8_t x,
                  uint32_t number, uint8_t length);
/* 按字符依次输出字符串；page是8像素横带编号，x是像素列。 */
void OLED_ShowString(uint8_t page, uint8_t x, const char *text);
/* 从本工程已有中文字库查找16×16点阵；每个常用汉字按UTF-8的3字节匹配，未收录则留空。 */
void OLED_ShowChinese(uint8_t page, uint8_t x, const char *text);


/* 只清空绘图草稿，不会直接清除屏幕，也不会清除已经显示的文字。 */
void OLED_FrameClear(void);
/* 在草稿点亮一个像素：y/8选横带，y%8选字节中的位；越界像素忽略。 */
void OLED_DrawPoint(int16_t x, int16_t y);
/* 用Bresenham整数步进画直线，通过误差累计选择下一个像素，减少浮点计算。 */
void OLED_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
/* 按像素到圆心的距离画圆；filled=1为实心，0为圆环，并裁剪到屏幕范围。 */
void OLED_DrawCircle(int16_t centerX, int16_t centerY,
                     uint8_t radius, uint8_t filled);
/* 同步从firstPage开始的pageCount条横带，每条宽128像素。 */
void OLED_UpdateFrame(uint8_t firstPage, uint8_t pageCount);
/* 把草稿指定矩形同步到期望画面和屏幕；横向用像素，纵向用8像素横带计数。 */
void OLED_UpdateFrameArea(uint8_t firstPage, uint8_t pageCount,
                           uint8_t x, uint8_t width);

#endif
