/* MPU6050六轴运动传感器：三轴加速度ax/ay/az、三轴角速度gx/gy/gz。
 * 软件I2C（双线通信）使用PB10时钟SCL、PB11数据SDA；开漏输出1表示释放线路，由上拉拉高。
 * ACK 是接收方应答，NACK 是不应答；本驱动检查ACK判断设备是否接收数据。
 * 姿态由四元数（描述空间旋转的四个数）保存，再换算横滚roll、俯仰pitch、偏航yaw角。
 * 偏航没有磁力计提供绝对方向，会漂移；校准时应保持静止，校准过程会暂时等待。 */
#include "main.h"
#include "mpu6050.h"
#include <math.h>

/* 沿用原项目地址：AD0 为低电平。 */
#define MPU6050_ADDRESS 0x68U

#define MPU_RAD_TO_DEG 57.29577951f
#define MPU_DEG_TO_RAD 0.01745329252f

/* 四元数q保存当前旋转；[1,0,0,0]表示初始无旋转，之后由采样持续更新。 */
static float q[4] = {1.0f, 0.0f, 0.0f, 0.0f};
/* 静止时角速度的平均偏移量，后续测量先减掉它。 */
static float gyroBias[3] = {0.0f, 0.0f, 0.0f};
static uint32_t orientationTick = 0;
static uint8_t orientationReady = 0;


/* 使用DWT（处理器调试单元的周期计数器）等待约5微秒，给通信电平留稳定时间。 */
static void MPU_Delay(void)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = (SystemCoreClock / 1000000U) * 5U;

    while ((uint32_t)(DWT->CYCCNT - start) < ticks)
    {
    }
}

/* 设置PB10时钟电平后等待片刻。 */
static void MPU_SCL(uint8_t level)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    MPU_Delay();
}

/* 设置PB11数据电平；开漏模式下1表示释放，0表示拉低。 */
static void MPU_SDA(uint8_t level)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    MPU_Delay();
}

/* 时钟为高时让数据由高变低，产生I2C起始条件。 */
static void MPU_Start(void)
{
    MPU_SDA(1);
    MPU_SCL(1);
    MPU_SDA(0);
    MPU_SCL(0);
}

/* 时钟为高时让数据由低变高，产生I2C停止条件。 */
static void MPU_Stop(void)
{
    MPU_SDA(0);
    MPU_SCL(1);
    MPU_SDA(1);
}

/* 从最高位开始发送8位，第9个时钟读取ACK；返回1表示传感器应答。 */
static uint8_t MPU_SendByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        MPU_SDA((data & 0x80U) != 0U);
        MPU_SCL(1);
        MPU_SCL(0);
        data <<= 1;
    }

    MPU_SDA(1);
    MPU_SCL(1);

    uint8_t acknowledged =
        HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_RESET;

    MPU_SCL(0);

    return acknowledged;
}

/* 接收8位数据；lastByte=1时发送NACK，通知传感器本次读完。 */
static uint8_t MPU_ReadByte(uint8_t lastByte)
{
    uint8_t data = 0;

    MPU_SDA(1);

    for (uint8_t i = 0; i < 8; i++)
    {
        MPU_SCL(1);
        data <<= 1;

        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_SET)
        {
            data |= 1U;
        }

        MPU_SCL(0);
    }

    /* 最后一个字节发送非应答，通知传感器结束数据输出。 */
    MPU_SDA(lastByte ? 1U : 0U);
    MPU_SCL(1);
    MPU_SCL(0);
    MPU_SDA(1);

    return data;
}

/* 依次发送写地址、寄存器地址和值；任一步未应答都停止并返回0。 */
static uint8_t MPU_WriteReg(uint8_t reg, uint8_t value)
{
    MPU_Start();

    if (!MPU_SendByte(MPU6050_ADDRESS << 1U) ||
        !MPU_SendByte(reg) ||
        !MPU_SendByte(value))
    {
        MPU_Stop();
        return 0;
    }

    MPU_Stop();
    return 1;
}

/* 先写寄存器起点，再重复起始并切换读方向，连续读取length个字节。 */
static uint8_t MPU_ReadRegs(uint8_t reg,
                            uint8_t *data, uint8_t length)
{
    MPU_Start();

    if (!MPU_SendByte(MPU6050_ADDRESS << 1U) ||
        !MPU_SendByte(reg))
    {
        MPU_Stop();
        return 0;
    }

    MPU_Start();

    if (!MPU_SendByte((MPU6050_ADDRESS << 1U) | 1U))
    {
        MPU_Stop();
        return 0;
    }

    for (uint8_t i = 0; i < length; i++)
    {
        data[i] = MPU_ReadByte(i == length - 1U);
    }

    MPU_Stop();
    return 1;
}

/* 高字节左移8位后合并低字节，再解释为有符号16位数，以保留负方向测量。 */
static int16_t MPU_Join(const uint8_t *bytes)
{
    return (int16_t)(((uint16_t)bytes[0] << 8U) | bytes[1]);
}

/* 开启延时计数器、尝试释放总线、检查芯片身份，再配置量程；返回1成功、0失败。 */
uint8_t MPU6050_Init(void)
{
    orientationReady = 0;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    MPU_SDA(1);
    MPU_SCL(1);
    HAL_Delay(100);

    /* Clear an interrupted byte before probing the reconnected sensor. */
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_RESET)
        return 0;
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_RESET)
    {
        for (uint8_t i = 0; i < 9; i++)
        {
            MPU_SCL(0);
            MPU_SCL(1);
        }
    }
    MPU_Stop();
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_RESET)
        return 0;

    uint8_t id = 0;

    if (!MPU_ReadRegs(0x75, &id, 1) || id != 0x68U)
    {
        return 0;
    }

    /* 沿用原项目的采样及量程设置。 */
    /* 每行是“寄存器地址、设置值”：唤醒、启用轴、采样分频、低通滤波、角速度±2000度/秒、加速度±16g。
     * g在这里表示重力加速度单位，约9.8米/秒²；原始计数还需要按量程换算。 */
    static const uint8_t settings[6][2] =
    {
        {0x6B, 0x01},
        {0x6C, 0x00},
        {0x19, 0x04},
        {0x1A, 0x06},
        {0x1B, 0x18},
        {0x1C, 0x18}
    };

    for (uint8_t i = 0; i < 6; i++)
    {
        if (!MPU_WriteReg(settings[i][0], settings[i][1]))
        {
            return 0;
        }
    }

    HAL_Delay(100);
    return 1;
}

/* 从0x3B连续读取14字节，取出六轴原始计数；不是已经换算好的g或度每秒。 */
uint8_t MPU6050_Read(MPU6050_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    uint8_t bytes[14];

    if (!MPU_ReadRegs(0x3B, bytes, 14))
    {
        return 0;
    }

    data->ax = MPU_Join(&bytes[0]);
    data->ay = MPU_Join(&bytes[2]);
    data->az = MPU_Join(&bytes[4]);

    /* 第 6、7 字节为温度数据，本阶段不使用。 */
    data->gx = MPU_Join(&bytes[8]);
    data->gy = MPU_Join(&bytes[10]);
    data->gz = MPU_Join(&bytes[12]);

    return 1;
}

/* 根据重力方向估计初始倾斜，写入四元数；初始偏航设为参考零方向。 */
static void MPU_SetInitialTilt(float ax, float ay, float az)
{
    float halfRoll = atan2f(ay, az) * 0.5f;
    float halfPitch =
        atan2f(-ax, sqrtf(ay * ay + az * az)) * 0.5f;

    float cr = cosf(halfRoll);
    float sr = sinf(halfRoll);
    float cp = cosf(halfPitch);
    float sp = sinf(halfPitch);

    q[0] = cr * cp;
    q[1] = sr * cp;
    q[2] = cr * sp;
    q[3] = -sr * sp;
}

/* 静止采100组数据求角速度零偏（静止仍存在的偏移），检查重力幅值并设置初始姿态。 */
uint8_t MPU6050_Calibrate(void)
{
    orientationReady = 0;

    float sums[6] = {0};
    MPU6050_Data data;

    for (uint16_t i = 0; i < 100; i++)
    {
        if (!MPU6050_Read(&data))
        {
            return 0;
        }

        sums[0] += data.ax;
        sums[1] += data.ay;
        sums[2] += data.az;
        sums[3] += data.gx;
        sums[4] += data.gy;
        sums[5] += data.gz;

        HAL_Delay(5);
    }

    float ax = sums[0] / 100.0f;
    float ay = sums[1] / 100.0f;
    float az = sums[2] / 100.0f;
    float norm = sqrtf(ax * ax + ay * ay + az * az);

    /* At the configured +/-16g range, gravity is about 2048 counts. */
    if (norm < 1536.0f || norm > 2560.0f)
    {
        return 0;
    }

    gyroBias[0] = sums[3] / 100.0f;
    gyroBias[1] = sums[4] / 100.0f;
    gyroBias[2] = sums[5] / 100.0f;

    MPU_SetInitialTilt(ax, ay, az);
    orientationTick = HAL_GetTick();
    orientationReady = 1;

    return 1;
}

/* 读取六轴数据，积分角速度并用重力方向修正，输出单位为度的姿态；返回0表示不可用。 */
uint8_t MPU6050_UpdateAngles(MPU6050_Angles *angles)
{
    if (angles == NULL || !orientationReady)
    {
        return 0;
    }

    MPU6050_Data data;
    if (!MPU6050_Read(&data))
    {
        return 0;
    }

    uint32_t now = HAL_GetTick();
    /* 毫秒乘0.001变为秒；角速度乘时间才能累计为角度变化。 */
    float dt = (uint32_t)(now - orientationTick) * 0.001f;
    orientationTick = now;

    /* No gyro history exists across a long gap; skip that integration step. */
    /* dt是两次采样间隔，单位秒；长时间未采样时不凭当前角速度补算整段未知运动。 */
    if (dt > 0.0f && dt <= 0.25f)
    {
        /* ±2000度/秒量程下16.4原始计数约等于1度/秒，再乘π/180转换为算法使用的弧度/秒。 */
        float gx = (data.gx - gyroBias[0]) / 16.4f * MPU_DEG_TO_RAD;
        float gy = (data.gy - gyroBias[1]) / 16.4f * MPU_DEG_TO_RAD;
        float gz = (data.gz - gyroBias[2]) / 16.4f * MPU_DEG_TO_RAD;

        float ax = data.ax;
        float ay = data.ay;
        float az = data.az;
        float norm = sqrtf(ax * ax + ay * ay + az * az);

        float q0 = q[0];
        float q1 = q[1];
        float q2 = q[2];
        float q3 = q[3];

        /* Use gravity feedback only when acceleration magnitude is plausible. */
        if (norm >= 1536.0f && norm <= 2560.0f)
        {
            ax /= norm;
            ay /= norm;
            az /= norm;

            float vx = 2.0f * (q1 * q3 - q0 * q2);
            float vy = 2.0f * (q0 * q1 + q2 * q3);
            float vz = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;

            gx += 2.0f * (ay * vz - az * vy);
            gy += 2.0f * (az * vx - ax * vz);
            gz += 2.0f * (ax * vy - ay * vx);
        }

        /* 按四元数微分关系积分角速度，再归一化，让四元数长度保持为1，减小数值累计误差。 */
        float halfDt = dt * 0.5f;
        q[0] = q0 + (-q1 * gx - q2 * gy - q3 * gz) * halfDt;
        q[1] = q1 + ( q0 * gx + q2 * gz - q3 * gy) * halfDt;
        q[2] = q2 + ( q0 * gy - q1 * gz + q3 * gx) * halfDt;
        q[3] = q3 + ( q0 * gz + q1 * gy - q2 * gx) * halfDt;

        float inverseNorm = 1.0f / sqrtf(
            q[0] * q[0] + q[1] * q[1] +
            q[2] * q[2] + q[3] * q[3]);

        for (uint8_t i = 0; i < 4; i++)
        {
            q[i] *= inverseNorm;
        }
    }

    /* 浮点误差可能使结果略超出-1到1，先限幅再调用反正弦asinf，避免得到无效数。 */
    float sinPitch = 2.0f * (q[0] * q[2] - q[3] * q[1]);
    if (sinPitch > 1.0f)
    {
        sinPitch = 1.0f;
    }
    else if (sinPitch < -1.0f)
    {
        sinPitch = -1.0f;
    }

    angles->roll = atan2f(
        2.0f * (q[0] * q[1] + q[2] * q[3]),
        1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]))
        * MPU_RAD_TO_DEG;
    angles->pitch = asinf(sinPitch) * MPU_RAD_TO_DEG;
    angles->yaw = atan2f(
        2.0f * (q[0] * q[3] + q[1] * q[2]),
        1.0f - 2.0f * (q[2] * q[2] + q[3] * q[3]))
        * MPU_RAD_TO_DEG;

    return 1;
}
