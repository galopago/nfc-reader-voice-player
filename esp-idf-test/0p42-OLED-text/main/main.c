/*
 * OLED 0.42" with ESP32-C3 via I2C (SSD1306 compatible).
 * Visible area: 72x40 pixels with buffer offset (28, 24).
 * See TECHNICAL_DOCUMENTATION.md in project root.
 * If this driver does not work, try SH1106 (offset X=2, page addressing).
 */

#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

#define I2C_NUM         I2C_NUM_0
#define SDA_PIN         5
#define SCL_PIN         6
#define OLED_ADDR       0x3C
#define OLED_I2C_FREQ   400000

#define OLED_OFFSET_X   28
#define OLED_OFFSET_Y   24
#define VISIBLE_W       72
#define VISIBLE_H       40

#define FB_SIZE         (128 * 64 / 8)

/* 5x7 font: 5 columns, each byte = column (bit0 = top). Glyphs: 0-9, A-D. */
#define FONT_W          5
#define FONT_H          7

static const uint8_t font_5x7[14][5] = {
    /* 0-9 */
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, /* 0 */
    { 0x00, 0x42, 0x7F, 0x40, 0x00 }, /* 1 */
    { 0x42, 0x61, 0x51, 0x49, 0x46 }, /* 2 */
    { 0x21, 0x41, 0x45, 0x4B, 0x31 }, /* 3 */
    { 0x18, 0x14, 0x12, 0x7F, 0x10 }, /* 4 */
    { 0x27, 0x45, 0x45, 0x45, 0x39 }, /* 5 */
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, /* 6 */
    { 0x01, 0x71, 0x09, 0x05, 0x03 }, /* 7 */
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, /* 8 */
    { 0x06, 0x49, 0x49, 0x29, 0x1E }, /* 9 */
    /* A-D */
    { 0x7C, 0x12, 0x11, 0x12, 0x7C }, /* A */
    { 0x7F, 0x49, 0x49, 0x49, 0x36 }, /* B */
    { 0x3E, 0x41, 0x41, 0x41, 0x22 }, /* C */
    { 0x7F, 0x41, 0x41, 0x22, 0x1C }, /* D */
};

static uint8_t fb[FB_SIZE];

static void send_cmd(uint8_t cmd)
{
    uint8_t buf[2] = { 0x80, cmd };
    i2c_master_write_to_device(I2C_NUM, OLED_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static void send_data(uint8_t *data, size_t len)
{
    uint8_t *buf = malloc(len + 1);
    if (!buf) return;
    buf[0] = 0x40;
    memcpy(buf + 1, data, len);
    i2c_master_write_to_device(I2C_NUM, OLED_ADDR, buf, len + 1, pdMS_TO_TICKS(100));
    free(buf);
}

static void set_pixel(int x, int y, int on)
{
    if (x < 0 || x >= VISIBLE_W || y < 0 || y >= VISIBLE_H) return;
    int bx = x + OLED_OFFSET_X;
    int by = y + OLED_OFFSET_Y;
    int idx = (by / 8) * 128 + bx;
    int bit = by % 8;
    if (on) {
        fb[idx] |= (1u << bit);
    } else {
        fb[idx] &= ~(1u << bit);
    }
}

static void oled_clear(void)
{
    memset(fb, 0, sizeof(fb));
}

/* Get font index for '0'-'9' (0-9) or 'A'-'D' (10-13). Returns 255 if unknown. */
static uint8_t font_index(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'D') return (uint8_t)(10 + (c - 'A'));
    return 255;
}

/* Draw one 5x7 character with top-left at (x, y) in visible coordinates. */
static void draw_char(int x, int y, char c)
{
    uint8_t idx = font_index(c);
    if (idx > 13) return;
    const uint8_t *glyph = font_5x7[idx];
    for (int col = 0; col < FONT_W; col++) {
        for (int row = 0; row < FONT_H; row++) {
            if ((glyph[col] >> row) & 1)
                set_pixel(x + col, y + row, 1);
        }
    }
}

/* Quadrant size (visible area 72x40 split in 4). */
#define QUAD_W          36
#define QUAD_H          20

/* Draw one character centered in quadrant with origin (qx, qy). */
static void draw_char_centered(int qx, int qy, char c)
{
    int left = qx + (QUAD_W - FONT_W) / 2;
    int top = qy + (QUAD_H - FONT_H) / 2;
    draw_char(left, top, c);
}

static void oled_update(void)
{
    send_cmd(0x21);  /* SET_COL_ADDR */
    send_cmd(0);
    send_cmd(127);
    send_cmd(0x22);  /* SET_PAGE_ADDR */
    send_cmd(0);
    send_cmd(7);
    send_data(fb, FB_SIZE);
}

static void oled_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = OLED_I2C_FREQ,
    };
    i2c_param_config(I2C_NUM, &conf);
    i2c_driver_install(I2C_NUM, conf.mode, 0, 0, 0);

    /* Display init sequence (SSD1306): Display OFF, horizontal addressing, MUX 64, etc. */
    const uint8_t init_commands[] = {
        0xAE,       /* Display OFF */
        0x20, 0x00, /* Memory Addressing Mode: Horizontal */
        0x40,       /* Display Start Line = 0 */
        0xA1,       /* Segment Re-map: column 127 mapped to SEG0 */
        0xA8, 0x3F, /* MUX Ratio = 64 (for 64 rows) */
        0xC8,       /* COM Output Scan Direction: remapped */
        0xD3, 0x00, /* Display Offset = 0 */
        0xDA, 0x12, /* COM Pins Hardware Configuration (for 128x64) */
        0xD5, 0x80, /* Display Clock Divide Ratio */
        0xD9, 0xF1, /* Pre-charge Period (for internal VCC) */
        0xDB, 0x30, /* VCOMH Deselect Level (0.83 x Vcc) */
        0x81, 0xFF, /* Contrast Control = maximum (255) */
        0xA4,       /* Entire Display ON: follows RAM content */
        0xA6,       /* Normal Display (not inverted) */
        0x8D, 0x14, /* Charge Pump: enabled (required for internal VCC) */
        0xAF,       /* Display ON */
    };
    for (size_t i = 0; i < sizeof(init_commands); i++) {
        send_cmd(init_commands[i]);
    }
}

void app_main(void)
{
    oled_init();
    oled_clear();

    /* Test pattern: 9 points (corners + midpoints + center) to verify visible area and offset */
    set_pixel(0, 0, 1);                          /* Top-left */
    set_pixel(VISIBLE_W / 2, 0, 1);               /* Top center */
    set_pixel(VISIBLE_W - 1, 0, 1);               /* Top-right */
    set_pixel(0, VISIBLE_H / 2, 1);              /* Middle left */
    set_pixel(VISIBLE_W / 2, VISIBLE_H / 2, 1);   /* Center */
    set_pixel(VISIBLE_W - 1, VISIBLE_H / 2, 1);  /* Middle right */
    set_pixel(0, VISIBLE_H - 1, 1);              /* Bottom-left */
    set_pixel(VISIBLE_W / 2, VISIBLE_H - 1, 1);  /* Bottom center */
    set_pixel(VISIBLE_W - 1, VISIBLE_H - 1, 1);  /* Bottom-right */

    /* One character in the middle of each quadrant: A, 0, 1, B */
    draw_char_centered(0, 0, 'A');               /* Upper-left: A */
    draw_char_centered(QUAD_W, 0, '0');           /* Upper-right: 0 */
    draw_char_centered(0, QUAD_H, '1');           /* Lower-left: 1 */
    draw_char_centered(QUAD_W, QUAD_H, 'B');      /* Lower-right: B */

    oled_update();
}
