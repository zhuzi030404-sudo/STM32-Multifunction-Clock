/* mpu6050.h：对外接口说明。先看本文件了解如何调用，再到同名.c查看实现。
 * 头文件声明函数的名字、参数与返回类型；#ifndef/#define/#endif避免重复包含。 */
#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

/* 下方结构体把相关读数放在一起；读取函数通过指针填入调用者提供的变量。 */
typedef struct
{
    /* a开头是加速度原始计数，g开头是陀螺仪角速度原始计数，x/y/z代表三个轴。 */
    int16_t ax;
    int16_t ay;
    int16_t az;

    int16_t gx;
    int16_t gy;
    int16_t gz;
} MPU6050_Data;

/* 成功返回 1，失败返回 0。 */
/* 开启延时计数器、尝试释放总线、检查芯片身份，再配置量程；返回1成功、0失败。 */
uint8_t MPU6050_Init(void);
/* 从0x3B连续读取14字节，取出六轴原始计数；不是已经换算好的g或度每秒。 */
uint8_t MPU6050_Read(MPU6050_Data *data);

/* 下方结构体把相关读数放在一起；读取函数通过指针填入调用者提供的变量。 */
typedef struct
{
    /* roll=横滚，pitch=俯仰，yaw=偏航，单位都是度；yaw是相对方向并会漂移。 */
    float roll;
    float pitch;
    float yaw;
} MPU6050_Angles;

/* 静止采100组数据求角速度零偏（静止仍存在的偏移），检查重力幅值并设置初始姿态。 */
uint8_t MPU6050_Calibrate(void);
/* 读取六轴数据，积分角速度并用重力方向修正，输出单位为度的姿态；返回0表示不可用。 */
uint8_t MPU6050_UpdateAngles(MPU6050_Angles *angles);

#endif
