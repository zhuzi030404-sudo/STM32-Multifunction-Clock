/* 小恐龙游戏：按键1按下时跳跃，碰撞后再按1重开，长按3松手返回菜单。
 * 位图数组是像素图案，1表示点亮；先理解后面的游戏函数，不必逐个背十六进制图案数据。
 * 游戏运动每20毫秒推进一步，显示分横带传送，避免传屏速度直接决定游戏速度。 */
#include "dino.h"
#include "main.h"
#include "oled.h"
#include "key.h"
#include <math.h>

static const uint8_t Barrier[][48]={
    {
        0x00,0x00,0x00,0x00,0x00,0x00,0xF8,0xFC,0xF8,0x00,0x00,0xF0,0xF0,0x00,0x00,0x00,
        0xC0,0xC0,0xC7,0xCF,0xCC,0x0C,0xFF,0xFF,0xFF,0x03,0xC3,0xC3,0xC1,0xC0,0xC0,0xC0,
        0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    },
    {
        0x00,0x00,0x00,0x80,0xC0,0x00,0x00,0x00,0x78,0xE0,0xC0,0xFC,0xFC,0x00,0x00,0x00,
        0xC0,0xDC,0x18,0xFF,0xFF,0x06,0x03,0x00,0x00,0x00,0x01,0xFF,0xFF,0x0C,0xCE,0xC6,
        0x00,0x00,0x00,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x00,0x00,0x00,
    },
    {
        0x00,0x00,0x00,0x00,0x00,0xC0,0x00,0xFC,0xF0,0xC0,0x60,0x00,0x00,0x00,0x00,0x00,
        0xC0,0x08,0xFC,0xFE,0x30,0x18,0x01,0xFF,0xFF,0x01,0x00,0x18,0x70,0xFE,0x07,0xC0,
        0x00,0x00,0x01,0x01,0x00,0x00,0x00,0x01,0x01,0x00,0x00,0x00,0x00,0x01,0x00,0x00,
    },
};

static const uint8_t Cloud[]={
	0x30,0x28,0x28,0x2C,0x26,0x23,0x21,0x21,0x21,0x21,0x23,0x22,0x22,0x2E,0x38,0x30,
};

static const uint8_t Dino[][48]={
    {
        0xF0,0xC0,0x00,0x00,0x00,0x80,0x80,0xC0,0xFE,0xFF,0xFB,0xFF,0x7F,0x6F,0x6F,0x0E,
        0xC3,0x07,0x0F,0xFE,0xFF,0x7F,0x1F,0x1F,0x7F,0x7F,0x5F,0x07,0xC1,0xC3,0xC0,0xC0,
        0x00,0x00,0x00,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    },
    {
        0xF0,0xC0,0x00,0x00,0x00,0x80,0x80,0xC0,0xFE,0xFF,0xFB,0xFF,0x7F,0x6F,0x6F,0x0E,
        0xC3,0x07,0x0F,0x7E,0x7F,0x5F,0x1F,0x1F,0xFF,0xFF,0x3F,0x07,0xC1,0xC3,0xC0,0xC0,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,
    },
    {
        0xF0,0xC0,0x00,0x00,0x00,0x80,0x80,0xC0,0xFE,0xFF,0xFB,0xFF,0x7F,0x6F,0x6F,0x0E,
        0x03,0x07,0x0F,0xFE,0xFF,0x7F,0x1F,0x1F,0xFF,0xFF,0x7F,0x07,0x01,0x03,0x00,0x00,
        0x00,0x00,0x00,0x01,0x01,0x01,0x00,0x00,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,
    },
};

static uint32_t stepTick, startTick, jumpTick, frameTick;
static uint32_t score;
static int16_t obstacleX, cloudX;
static uint8_t obstacleKind, jumping, gameOver, height, renderPage;

/* 逐位展开位图并在草稿画图；每个字节存一列中连续8个竖向像素。 */
static void Dino_Sprite(int16_t x, int16_t y, uint8_t rows,
                        const uint8_t *image)
{
    for (uint8_t row = 0; row < rows; row++)
    {
        for (uint8_t col = 0; col < 16; col++)
        {
            if (image[(row / 8U) * 16U + col] & (1U << (row % 8U)))
                OLED_DrawPoint(x + col, y + row);
        }
    }
}

/* 用缩小后的有效矩形判断碰撞，避免透明图案边缘也被当作障碍。 */
static uint8_t Dino_Collides(void)
{
    /* Inset empty sprite margins so a correctly timed jump clears the obstacle. */
    return obstacleX + 3 < 14 && obstacleX + 13 > 2 && height < 18;
}

/* 生成游戏画面草稿：地面、云、障碍和人物；只标记待传送，不在此一次发完整屏。 */
static void Dino_Draw(uint32_t now)
{
    OLED_FrameClear();
    for (int16_t x = 0; x < 128; x++)
    {
        OLED_DrawPoint(x, 46);
        if (((x + (now - startTick) / 20U) % 17U) < 3U)
            OLED_DrawPoint(x, 47);
    }
    Dino_Sprite(cloudX, 18, 8, Cloud);
    Dino_Sprite(obstacleX, 28, 18, Barrier[obstacleKind]);
    Dino_Sprite(0, 28 - height, 18,
                Dino[jumping ? 2 : ((now - startTick) / 150U) % 2U]);
    renderPage = 0;
}

/* 绘制底部按键说明，action用于显示跳跃或重开。 */
static void Dino_Footer(const char *action)
{
    OLED_ShowString(6, 0, "1");
    OLED_ShowChinese(6, 8, action);
    OLED_ShowChinese(6, 48, "长按");
    OLED_ShowString(6, 82, "3");
    OLED_ShowChinese(6, 90, "返回");
}

/* 更新顶部的分数字样和数值。 */
static void Dino_Header(void)
{
    OLED_ShowChinese(0, 48, "分数");
    OLED_ShowNum(0, 92, score, 5);
}

/* 开始新一局：清分数、初始化物体位置和时间，再绘制初始画面。 */
void Dino_Open(void)
{
    stepTick = startTick = frameTick = HAL_GetTick();
    score = 0;
    /* 屏幕最右列是127；障碍随后逐步向左移动，x变小。 */
    obstacleX = 127;
    cloudX = 96;
    obstacleKind = jumping = gameOver = height = 0;
    OLED_Fill(0x00);
    Dino_Header();
    Dino_Footer("跳跃");
    Dino_Draw(startTick);
}

/* 处理1号键的新按下事件；在空中不再起跳，游戏结束则重开。 */
void Dino_HandleEvent(uint8_t event)
{
    if (event == KEY_EVENT_1)
    {
        if (gameOver)
            Dino_Open();
        else if (!jumping)
        {
            jumping = 1;
            jumpTick = HAL_GetTick();
        }
    }
}

/* 主循环推进运动和碰撞，并分批传屏；跳跃用正弦曲线生成先升后降的高度。 */
void Dino_Update(void)
{
    uint32_t now = HAL_GetTick();
    if (gameOver)
        return;

    /* Advance every 20 ms; rendering must not determine game speed. */
    while ((uint32_t)(now - stepTick) >= 20U)
    {
        /* 推进一个固定20毫秒的模拟步；若实际循环稍慢，while可补上多个步。 */
        stepTick += 20U;
        if (jumping)
        {
            uint32_t elapsed = stepTick - jumpTick;
            /* The simulation step may precede a just-received key event. */
            if (elapsed > 0x7FFFFFFFU)
                elapsed = 0;
            if (elapsed >= 1000U)
            {
                jumping = 0;
                height = 0;
            }
            else
                /* 0到1000毫秒对应sin(0到π)：从0升至28像素再落到0，这是设计的轨迹，不是重力物理模拟。 */
                height = (uint8_t)(28.0f * sinf(3.14159265f * elapsed / 1000.0f));
        }
        obstacleX--;
        if (obstacleX <= -16)
        {
            /* 屏幕最右列是127；障碍随后逐步向左移动，x变小。 */
    obstacleX = 127;
            obstacleKind = (uint8_t)((stepTick / 20U) % 3U);
        }
        cloudX = 127 - (int16_t)(((stepTick - startTick) / 50U) % 144U);
        score = ((stepTick - startTick) / 100U) % 100000U;
        if (Dino_Collides())
        {
            gameOver = 1;
            renderPage = 6;
            OLED_Fill(0x00);
            OLED_ShowChinese(2, 32, "游戏结束");
            OLED_ShowChinese(4, 18, "分数");
            OLED_ShowNum(4, 60, score, 5);
            Dino_Footer("重开");
            return;
        }
    }

    if (renderPage == 6 && (uint32_t)(now - frameTick) >= 40U)
    {
        frameTick = now;
        Dino_Draw(now);
    }
    /* Send one 8-pixel strip per loop so keys remain responsive. */
    if (renderPage < 6U)
    {
        if (renderPage < 2U)
            OLED_UpdateFrameArea(renderPage, 1, 0, 16);
        else
            OLED_UpdateFrame(renderPage, 1);
        if (++renderPage == 6U)
            Dino_Header();
    }
}
