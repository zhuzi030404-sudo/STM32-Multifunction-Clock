/* light_sensor.h：对外接口说明。先看本文件了解如何调用，再到同名.c查看实现。
 * 头文件声明函数的名字、参数与返回类型；#ifndef/#define/#endif避免重复包含。 */
#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include "stm32f1xx_hal.h"

/* 传入ADC句柄（描述外设的结构体地址）并校准；返回1成功、0失败，随后等待Update采样。 */
uint8_t LightSensor_Init(ADC_HandleTypeDef *adc);
/* 到50毫秒才启动一次转换，最多等2毫秒，再停止ADC；失败时将当前读数标为无效。 */
void LightSensor_Update(void);
/* 返回1表示最近一次采样有效；显示和自动亮度应先检查这个状态。 */
uint8_t LightSensor_IsValid(void);
/* 取平滑后的0到4095采样值；调用者先检查有效标志。 */
uint16_t LightSensor_GetValue(void);
/* 将缓存值缩放到0到100；inverted决定低电压是否表示更亮，不代表实际lux照度。 */
uint8_t LightSensor_GetPercent(void);
/* 切换明暗换算方向，适配模块输出电压与亮度的正向或反向关系。 */
void LightSensor_ToggleDirection(void);

#endif
