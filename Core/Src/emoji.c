/* 动态表情页面：按键1/2切换六种表情，按键3返回；phase是当前动画阶段。
 * 位图数组保存眉毛和嘴型，其他表情用线、圆等图形组合；绘图只更新中间区域以保留文字。 */
#include "emoji.h"
#include "main.h"
#include "oled.h"
#include "key.h"

static const uint8_t Eyebrow[2][32] = {
    {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0xE0,0x70,0x3C,0x1F,0x07,
        0x00,0x0C,0x0C,0x0C,0x0C,0x0E,0x06,0x07,0x03,0x03,0x01,0x00,0x00,0x00,0x00,0x00,
    },
    {
        0x07,0x1F,0x3C,0x70,0xE0,0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x01,0x03,0x03,0x07,0x06,0x0E,0x0C,0x0C,0x0C,0x0C,0x00,
    },
};

static const uint8_t Mouth[60] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x0C,0x18,0x10,0x10,0x10,0x18,0x0C,0x07,0x07,0x0C,0x18,0x10,0x10,0x10,0x18,0x0C,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

static uint32_t startedAt;
static uint8_t drawnPhase, renderPage;
static uint8_t selectedEmoji;
static const char *names[6] = {"眨眼", "开心", "难过", "生气", "惊讶", "困倦"};

/* 把位图的各个点复制到绘图草稿。 */
static void Emoji_Image(int16_t x, int16_t y, uint8_t width,
                         uint8_t height, const uint8_t *image)
{
    for (uint8_t row = 0; row < height; row++)
        for (uint8_t col = 0; col < width; col++)
            if (image[(row / 8U) * width + col] & (1U << (row % 8U)))
                OLED_DrawPoint(x + col, y + row);
}

/* 利用椭圆范围画实心眼睛，改变ry高度就能做眨眼效果。 */
static void Emoji_Eye(int16_t cx, int16_t cy, int16_t ry)
{
    for (int16_t y = -ry; y <= ry; y++)
        for (int16_t x = -6; x <= 6; x++)
            if (x * x * ry * ry + y * y * 36 <= 36 * ry * ry)
                OLED_DrawPoint(cx + x, cy + y);
}

/* 根据表情编号和动画阶段画脸，保留顶部名称及底部按键说明区域。 */
static void Emoji_Draw(uint8_t phase)
{
    static const uint8_t offsets[6] = {0, 1, 2, 2, 1, 0};
    /* phase索引动画阶段，offset控制几像素的位移或眼睛高度变化，让表情动起来。 */
    uint8_t offset = offsets[phase];
    OLED_FrameClear();
    switch (selectedEmoji)
    {
        case 0:
            Emoji_Image(30, 10 + offset, 16, 16, Eyebrow[0]);
            Emoji_Image(82, 10 + offset, 16, 16, Eyebrow[1]);
            Emoji_Eye(40, 32, 6 - offset);
            Emoji_Eye(88, 32, 6 - offset);
            Emoji_Image(54, 30, 20, 20, Mouth);
            break;
        case 1:
            OLED_DrawLine(34, 29, 40, 24 - offset);
            OLED_DrawLine(40, 24 - offset, 46, 29);
            OLED_DrawLine(82, 29, 88, 24 - offset);
            OLED_DrawLine(88, 24 - offset, 94, 29);
            OLED_DrawCircle(27, 34, 2, 1);
            OLED_DrawCircle(101, 34, 2, 1);
            OLED_DrawLine(54, 37, 59, 41 + offset);
            OLED_DrawLine(59, 41 + offset, 69, 41 + offset);
            OLED_DrawLine(69, 41 + offset, 74, 37);
            break;
        case 2:
            OLED_DrawLine(32, 23, 46, 18);
            OLED_DrawLine(82, 18, 96, 23);
            Emoji_Eye(40, 31, 4 - offset);
            Emoji_Eye(88, 31, 4 - offset);
            OLED_DrawLine(44, 37, 44, 40 + offset);
            OLED_DrawLine(84, 37, 84, 40 + offset);
            OLED_DrawLine(56, 43, 61, 40);
            OLED_DrawLine(61, 40, 67, 40);
            OLED_DrawLine(67, 40, 72, 43);
            break;
        case 3:
            OLED_DrawLine(32, 19, 47, 25);
            OLED_DrawLine(81, 25, 96, 19);
            Emoji_Eye(40, 31, 3 - offset / 2U);
            Emoji_Eye(88, 31, 3 - offset / 2U);
            OLED_DrawLine(57, 43, 63, 40);
            OLED_DrawLine(63, 40, 69, 43);
            break;
        case 4:
            OLED_DrawLine(34, 21, 40, 18);
            OLED_DrawLine(40, 18, 46, 21);
            OLED_DrawLine(82, 21, 88, 18);
            OLED_DrawLine(88, 18, 94, 21);
            OLED_DrawCircle(40, 30, 5 + offset / 2U, 0);
            OLED_DrawCircle(88, 30, 5 + offset / 2U, 0);
            OLED_DrawCircle(40, 30, 1, 1);
            OLED_DrawCircle(88, 30, 1, 1);
            OLED_DrawCircle(64, 40, 4 + offset / 2U, 0);
            break;
        case 5:
            OLED_DrawLine(33, 30, 39, 31 + offset / 2U);
            OLED_DrawLine(39, 31 + offset / 2U, 46, 30);
            OLED_DrawLine(82, 30, 88, 31 + offset / 2U);
            OLED_DrawLine(88, 31 + offset / 2U, 95, 30);
            OLED_DrawLine(60, 42, 68, 42);
            OLED_DrawLine(103, 19, 109, 19);
            OLED_DrawLine(109, 19, 103, 25);
            OLED_DrawLine(103, 25, 109, 25);
            if (offset > 0)
            {
                OLED_DrawLine(112, 28, 120, 28);
                OLED_DrawLine(120, 28, 112, 36);
                OLED_DrawLine(112, 36, 120, 36);
            }
            break;
    }
    drawnPhase = phase;
    renderPage = 1;
}

/* 选择新表情后重新计时，更新名称、编号和第一帧。 */
static void Emoji_Select(void)
{
    startedAt = HAL_GetTick();
    OLED_ShowChinese(0, 48, names[selectedEmoji]);
    OLED_ShowNum(0, 116, selectedEmoji + 1U, 1);
    Emoji_Draw(0);
}

/* 进入表情页面，从第一个表情开始并显示操作提示。 */
void Emoji_Open(void)
{
    selectedEmoji = 0;
    OLED_Fill(0x00);
    OLED_ShowChinese(6, 0, "切换");
    OLED_ShowString(6, 34, "1 2");
    OLED_ShowChinese(6, 72, "返回");
    OLED_ShowString(6, 110, "3");
    Emoji_Select();
}

/* 按键1向前、2向后，六个表情首尾循环。 */
void Emoji_HandleEvent(uint8_t event)
{
    if (event == KEY_EVENT_1)
        selectedEmoji = selectedEmoji == 0U ? 5U : selectedEmoji - 1U;
    else if (event == KEY_EVENT_2)
        selectedEmoji = (selectedEmoji + 1U) % 6U;
    else
        return;
    Emoji_Select();
}

/* 1100毫秒一轮动画，前6个阶段每100毫秒变化，随后停留；每轮主循环分批传送。 */
void Emoji_Update(void)
{
    /* 取余让动画每1100毫秒循环；不是程序在这里停等1100毫秒。 */
    uint32_t elapsed = (HAL_GetTick() - startedAt) % 1100U;
    uint8_t phase = elapsed / 100U;
    if (phase > 5U)
        phase = 5U;
    if (renderPage == 6U && phase != drawnPhase)
        Emoji_Draw(phase);
    if (renderPage < 6U)
    {
        /* Preserve the two-page name between the left and right eyebrows. */
        if (renderPage == 1U)
        {
            OLED_UpdateFrameArea(1, 1, 0, 48);
            OLED_UpdateFrameArea(1, 1, 80, 48);
        }
        else
            OLED_UpdateFrame(renderPage, 1);
        renderPage++;
    }
}
