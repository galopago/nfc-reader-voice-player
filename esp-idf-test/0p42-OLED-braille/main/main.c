/*
 * OLED 0.42" with ESP32-C3 via I2C (SSD1306 compatible).
 * Demo: left half = Braille cell (Spanish Grade 1), right half = character.
 * Visible area: 72x40 pixels with buffer offset (28, 24).
 * See TECHNICAL_DOCUMENTATION.md and SPANISH_BRAILLE_REFERENCE.md in project root.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include "spanish_braille.h"
#include <string.h>

#define SDA_PIN         5
#define SCL_PIN         6
#define OLED_I2C_ADDR   0x3C   /* 7-bit; U8g2 expects (addr<<1) = 0x78 */

#define OLED_OFFSET_X   28
#define OLED_OFFSET_Y   24
#define VISIBLE_W       72
#define VISIBLE_H       40

#define QUAD_W          (VISIBLE_W / 2)   /* 36: left/right half width */

/* Wrappers: draw in visible coordinates [0..71] x [0..39]; add offset internally. */
#define draw_str_visible(u8g2, x, y, s)     u8g2_DrawStr(u8g2, (x) + OLED_OFFSET_X, (y) + OLED_OFFSET_Y, (s))
#define draw_utf8_visible(u8g2, x, y, s)    u8g2_DrawUTF8(u8g2, (x) + OLED_OFFSET_X, (y) + OLED_OFFSET_Y, (s))

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

/* Braille + character demo: largest Braille cell and font that fit in 72x40.
 * Left half (0..35): one Braille cell, dot_radius=5, gap=2 -> 24x36 px, centered.
 * Right half (36..71): one character with spleen16x32 (16x32), centered, baseline y=32.
 */
static const char *demo_chars[] = {
    "a", "ñ", "5", "?", "á", ".", "!", "H"
};
static const size_t demo_chars_count = sizeof(demo_chars) / sizeof(demo_chars[0]);

static void demo_braille_and_char(u8g2_t *u, const char *utf8)
{
    uint8_t pattern = 0;
    int n = braille_es_char_to_pattern(utf8, &pattern);
    if (n == 0) {
        return;
    }

    const int dot_radius = 5;
    const int gap = 2;
    int cell_w = 0, cell_h = 0;
    braille_es_cell_size(dot_radius, gap, &cell_w, &cell_h);

    /* Left half: center Braille cell in [0, QUAD_W) x [0, VISIBLE_H) */
    int cell_x = (QUAD_W - cell_w) / 2;
    int cell_y = (VISIBLE_H - cell_h) / 2;
    int buf_x = cell_x + OLED_OFFSET_X;
    int buf_y = cell_y + OLED_OFFSET_Y;

    /* Right half: spleen16x32 is 16x32; center in [QUAD_W, VISIBLE_W), baseline at y=32 */
    const int font_w = 16;
    const int font_h = 32;
    int char_x = QUAD_W + (QUAD_W - font_w) / 2;
    int char_y = font_h;   /* baseline */

    u8g2_ClearBuffer(u);
    braille_es_draw_cell(u, buf_x, buf_y, pattern, dot_radius);
    u8g2_SetFont(u, u8g2_font_spleen16x32_mf);
    draw_utf8_visible(u, char_x, char_y, utf8);
    u8g2_SendBuffer(u);
}

void app_main(void)
{
    oled_u8g2_init();

    for (;;) {
        for (size_t i = 0; i < demo_chars_count; i++) {
            demo_braille_and_char(&u8g2, demo_chars[i]);
            vTaskDelay(pdMS_TO_TICKS(2500));
        }
    }
}
