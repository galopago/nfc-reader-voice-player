/*
 * OLED 0.42" with ESP32-C3 via I2C (SSD1306 compatible).
 * Text rendering with U8g2 library. Visible area: 72x40 pixels with buffer offset (28, 24).
 * See TECHNICAL_DOCUMENTATION.md in project root.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include <string.h>

#define SDA_PIN         5
#define SCL_PIN         6
#define OLED_I2C_ADDR   0x3C   /* 7-bit; U8g2 expects (addr<<1) = 0x78 */

#define OLED_OFFSET_X   28
#define OLED_OFFSET_Y   24
#define VISIBLE_W       72
#define VISIBLE_H       40

#define QUAD_W          (VISIBLE_W / 2)
#define QUAD_H          (VISIBLE_H / 2)

/* Wrappers: draw in visible coordinates [0..71] x [0..39]; add offset internally. */
#define draw_str_visible(u8g2, x, y, s)   u8g2_DrawStr(u8g2, (x) + OLED_OFFSET_X, (y) + OLED_OFFSET_Y, (s))
#define draw_frame_visible(u8g2, x, y, w, h)  u8g2_DrawFrame(u8g2, (x) + OLED_OFFSET_X, (y) + OLED_OFFSET_Y, (w), (h))

static u8g2_t u8g2;

static void oled_u8g2_init(void)
{
    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    hal.bus.i2c.sda = SDA_PIN;
    hal.bus.i2c.scl = SCL_PIN;
    u8g2_esp32_hal_init(hal);

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2, U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb);
    u8x8_SetI2CAddress(&u8g2.u8x8, (OLED_I2C_ADDR << 1));

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
}

/* Demo: multiple screens in a loop to show font and layout possibilities. */
static void demo_screen_1(u8g2_t *u)
{
    /* Small font 5x7: title + quadrants */
    u8g2_ClearBuffer(u);
    u8g2_SetFont(u, u8g2_font_5x7_tf);
    draw_str_visible(u, 0, 7, "0.42 OLED");
    draw_str_visible(u, 0, 18, "U8g2");
    draw_str_visible(u, 36, 7, "72x40");
    draw_str_visible(u, 36, 18, "fonts");
    draw_str_visible(u, 0, 29, "0-9 A-Z");
    draw_str_visible(u, 36, 29, "!?.,:");
    u8g2_SendBuffer(u);
}

static void demo_screen_2(u8g2_t *u)
{
    /* 6x10 font: different style */
    u8g2_ClearBuffer(u);
    u8g2_SetFont(u, u8g2_font_6x10_tf);
    draw_str_visible(u, 2, 10, "U8g2 6x10");
    draw_str_visible(u, 2, 24, "Hello!");
    draw_str_visible(u, 2, 36, "123 45.6");
    draw_frame_visible(u, 0, 0, 70, 38);
    u8g2_SendBuffer(u);
}

static void demo_screen_3(u8g2_t *u)
{
    /* 6x12 and numbers/symbols */
    u8g2_ClearBuffer(u);
    u8g2_SetFont(u, u8g2_font_6x12_tf);
    draw_str_visible(u, 4, 12, "Numbers:");
    draw_str_visible(u, 4, 26, "0 1 2 3 4 5");
    draw_str_visible(u, 4, 36, "6 7 8 9 + -");
    u8g2_SetFont(u, u8g2_font_5x7_tf);
    draw_str_visible(u, 40, 36, "demo");
    u8g2_SendBuffer(u);
}

static void demo_screen_4(u8g2_t *u)
{
    /* Four quadrants with different content */
    u8g2_ClearBuffer(u);
    u8g2_SetFont(u, u8g2_font_5x7_tf);
    draw_str_visible(u, 4, 7, "A");
    draw_str_visible(u, QUAD_W + 4, 7, "B");
    draw_str_visible(u, 4, QUAD_H + 7, "C");
    draw_str_visible(u, QUAD_W + 4, QUAD_H + 7, "D");
    u8g2_SetFont(u, u8g2_font_6x10_tf);
    draw_str_visible(u, 2, 18, "Q1");
    draw_str_visible(u, QUAD_W + 2, 18, "Q2");
    draw_str_visible(u, 2, QUAD_H + 18, "Q3");
    draw_str_visible(u, QUAD_W + 2, QUAD_H + 18, "Q4");
    draw_frame_visible(u, 0, 0, QUAD_W - 2, QUAD_H - 2);
    draw_frame_visible(u, QUAD_W, 0, QUAD_W - 2, QUAD_H - 2);
    draw_frame_visible(u, 0, QUAD_H, QUAD_W - 2, QUAD_H - 2);
    draw_frame_visible(u, QUAD_W, QUAD_H, QUAD_W - 2, QUAD_H - 2);
    u8g2_SendBuffer(u);
}

/* Max-size screen: one digit and one letter (2 chars), largest font that fits 72x40 without cut/overlap.
 * spleen16x32: 16px wide, 32px tall per glyph. Two chars = 32px width total, 32px height. Fits in 72x40.
 * Layout: left half (0..35) = number centered; right half (36..71) = letter centered. Baseline y=32. */
static void demo_screen_max_size(u8g2_t *u)
{
    const int font_w = 16;
    const int font_h = 32;
    int x0, x1;

    u8g2_ClearBuffer(u);
    u8g2_SetFont(u, u8g2_font_spleen16x32_mf);
    /* Left half: one digit (e.g. "8") centered in [0, QUAD_W) */
    x0 = (QUAD_W - font_w) / 2;
    draw_str_visible(u, x0, font_h, "8");
    /* Right half: one letter (e.g. "A") centered in [QUAD_W, VISIBLE_W) */
    x1 = QUAD_W + (QUAD_W - font_w) / 2;
    draw_str_visible(u, x1, font_h, "A");
    u8g2_SendBuffer(u);
}

void app_main(void)
{
    oled_u8g2_init();

    for (;;) {
        demo_screen_1(&u8g2);
        vTaskDelay(pdMS_TO_TICKS(2500));
        demo_screen_2(&u8g2);
        vTaskDelay(pdMS_TO_TICKS(2500));
        demo_screen_3(&u8g2);
        vTaskDelay(pdMS_TO_TICKS(2500));
        demo_screen_4(&u8g2);
        vTaskDelay(pdMS_TO_TICKS(2500));
        demo_screen_max_size(&u8g2);
        vTaskDelay(pdMS_TO_TICKS(2500));
    }
}
