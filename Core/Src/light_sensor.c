/* 光敏采样驱动：模块AO（模拟输出）接PA1，使用ADC1通道1，12位结果为0到4095。
 * 采样值代表电压大小；换算的百分比只是相对明暗，没有标定成实际照度lux（勒克斯）。
 * Update()每50毫秒最多采样一次；Get函数读取缓存值，不会每次都重新启动转换。 */
#include "light_sensor.h"

/* 保存传进来的ADC句柄地址，后续采样都使用这个外设。 */
static ADC_HandleTypeDef *sensorAdc;
/* 上一次采样时刻，单位毫秒。 */
static uint32_t sampledAt;
/* 平滑值放大8倍保存，右移3位即可取回实际值。 */
static uint32_t filteredTimes8;
/* ADC硬件校准是否成功；不是对光照lux进行物理标定。 */
static uint8_t calibrated;
/* 最近一次采样能否使用。 */
static uint8_t valid;
/* 默认电压越低视为越亮，界面允许切换这个关系。 */
static uint8_t inverted = 1;

/* 传入ADC句柄（描述外设的结构体地址）并校准；返回1成功、0失败，随后等待Update采样。 */
uint8_t LightSensor_Init(ADC_HandleTypeDef *adc)
{
    sensorAdc = adc;
    valid = 0;
    calibrated = 0;
    /* 把上次采样时刻提前50毫秒，让第一次Update可以立即采样。 */
    sampledAt = HAL_GetTick() - 50U;
    if (adc != 0)
        calibrated = HAL_ADCEx_Calibration_Start(adc) == HAL_OK;
    return calibrated;
}

/* 到50毫秒才启动一次转换，最多等2毫秒，再停止ADC；失败时将当前读数标为无效。 */
void LightSensor_Update(void)
{
    uint32_t now = HAL_GetTick();
    if (!calibrated || (uint32_t)(now - sampledAt) < 50U)
        return;
    sampledAt = now;
    if (HAL_ADC_Start(sensorAdc) != HAL_OK)
    {
        HAL_ADC_Stop(sensorAdc);
        valid = 0;
        return;
    }
    HAL_StatusTypeDef status = HAL_ADC_PollForConversion(sensorAdc, 2);
    /* 条件?结果A:结果B是三目表达式；4096超出12位合法范围，作为失败哨兵值。 */
    uint32_t value = status == HAL_OK ? HAL_ADC_GetValue(sensorAdc) : 4096U;
    HAL_StatusTypeDef stopped = HAL_ADC_Stop(sensorAdc);
    if (status != HAL_OK || stopped != HAL_OK || value > 4095U)
    {
        valid = 0;
        return;
    }
    if (!valid)
        filteredTimes8 = value * 8U;
    else
        /* 指数平滑：新值约为旧值的7/8加本次采样的1/8；内部乘8保存，减少整数截断损失。 */
        filteredTimes8 = filteredTimes8 - (filteredTimes8 >> 3U) + value;
    valid = 1;
}

/* 返回1表示最近一次采样有效；显示和自动亮度应先检查这个状态。 */
uint8_t LightSensor_IsValid(void)
{
    return valid;
}

/* 取平滑后的0到4095采样值；调用者先检查有效标志。 */
uint16_t LightSensor_GetValue(void)
{
    return (uint16_t)(filteredTimes8 >> 3U);
}

/* 将缓存值缩放到0到100；inverted决定低电压是否表示更亮，不代表实际lux照度。 */
uint8_t LightSensor_GetPercent(void)
{
    uint32_t value = LightSensor_GetValue();
    if (inverted)
        value = 4095U - value;
    return (uint8_t)((value * 100U + 2047U) / 4095U);
}

/* 切换明暗换算方向，适配模块输出电压与亮度的正向或反向关系。 */
void LightSensor_ToggleDirection(void)
{
    inverted = !inverted;
}
