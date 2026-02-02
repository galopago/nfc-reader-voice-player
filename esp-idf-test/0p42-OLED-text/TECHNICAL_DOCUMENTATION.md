# Technical Documentation: 0.42" OLED with ESP32-C3 (ESP-IDF)

## Summary

This document describes how to control a 0.42-inch OLED display connected to an ESP32-C3 via I2C using the ESP-IDF framework. The display has an important peculiarity: it uses a controller designed for larger screens, so it requires an offset to correctly position pixels in the visible area.

---

## Hardware

### Microcontroller
- **Chip**: ESP32-C3 (RISC-V single-core)
- **Tested variant**: ESP32-C3 (QFN32) revision v0.4
- **Flash**: 4MB integrated

### OLED Display
- **Size**: 0.42 inches
- **Controller**: SSD1306 compatible (actual controller may be SSD1306 or similar)
- **Controller resolution**: 128x64 pixels (internal buffer)
- **Actual visible resolution**: ~72x40 pixels
- **Interface**: I2C
- **I2C Address**: 0x3C
- **Color**: Monochrome (typically white on black)

### I2C Connections
| Signal | ESP32-C3 GPIO |
|--------|---------------|
| SDA    | GPIO 5        |
| SCL    | GPIO 6        |
| VCC    | 3.3V          |
| GND    | GND           |

---

## The Offset Problem

### Description
The 0.42" OLED display uses a controller (SSD1306 or compatible) designed to handle 128x64 pixel screens. However, the physical 0.42" panel only shows a portion of that buffer. This means:

1. The framebuffer in memory is 128x64 pixels (1024 bytes)
2. Only a window of approximately 72x40 pixels is physically visible
3. This window does NOT start at position (0,0) of the buffer

### Solution: Calibrated Offset
For pixels to appear in the visible area, an offset must be applied to all coordinates:

```
Buffer_X_Coordinate = Visible_X_Coordinate + OFFSET_X
Buffer_Y_Coordinate = Visible_Y_Coordinate + OFFSET_Y
```

### Calibrated Offset Values (Tested and Working)
```c
#define OLED_OFFSET_X  28   // Horizontal offset
#define OLED_OFFSET_Y  24   // Vertical offset
```

### Resulting Visible Area
- **Visible width**: 72 pixels (buffer columns 28-99)
- **Visible height**: 40 pixels (buffer rows 24-63)
- **Valid user coordinates**: X=[0-71], Y=[0-39]

---

## ESP-IDF Configuration

### Framework Version
- **ESP-IDF**: v5.1.x
- **Python**: 3.9.x (for virtual environment)
- **Target**: esp32c3

### sdkconfig.defaults
```
CONFIG_IDF_TARGET="esp32c3"
```

### CMakeLists.txt (root)
```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(oled_042_example)
```

---

## I2C Configuration

### Parameters
```c
#define I2C_MASTER_NUM      I2C_NUM_0
#define OLED_I2C_SDA_PIN    5
#define OLED_I2C_SCL_PIN    6
#define OLED_I2C_ADDR       0x3C
#define OLED_I2C_FREQ_HZ    400000  // 400 kHz (Fast Mode)
```

### I2C Initialization Code
```c
#include "driver/i2c.h"

i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = OLED_I2C_SDA_PIN,
    .scl_io_num = OLED_I2C_SCL_PIN,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = OLED_I2C_FREQ_HZ,
};

i2c_param_config(I2C_MASTER_NUM, &conf);
i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
```

---

## I2C Communication Protocol with the Display

### Send Command
To send a command to the OLED controller:
```c
uint8_t data[2] = {0x80, command};  // 0x80 = Co=1, D/C#=0 (command)
i2c_master_write_to_device(I2C_MASTER_NUM, OLED_I2C_ADDR, data, 2, pdMS_TO_TICKS(100));
```

### Send Data (Pixels)
To send framebuffer data:
```c
uint8_t *buf = malloc(len + 1);
buf[0] = 0x40;  // 0x40 = Co=0, D/C#=1 (data)
memcpy(buf + 1, framebuffer_data, len);
i2c_master_write_to_device(I2C_MASTER_NUM, OLED_I2C_ADDR, buf, len + 1, pdMS_TO_TICKS(100));
```

---

## Display Initialization Sequence

This sequence configures the SSD1306 controller to work correctly:

```c
const uint8_t init_commands[] = {
    0xAE,       // Display OFF
    0x20, 0x00, // Memory Addressing Mode: Horizontal
    0x40,       // Display Start Line = 0
    0xA1,       // Segment Re-map: column 127 mapped to SEG0
    0xA8, 0x3F, // MUX Ratio = 64 (for 64 rows)
    0xC8,       // COM Output Scan Direction: remapped
    0xD3, 0x00, // Display Offset = 0
    0xDA, 0x12, // COM Pins Hardware Configuration (for 128x64)
    0xD5, 0x80, // Display Clock Divide Ratio
    0xD9, 0xF1, // Pre-charge Period (for internal VCC)
    0xDB, 0x30, // VCOMH Deselect Level (0.83 x Vcc)
    0x81, 0xFF, // Contrast Control = maximum (255)
    0xA4,       // Entire Display ON: follows RAM content
    0xA6,       // Normal Display (not inverted)
    0x8D, 0x14, // Charge Pump: enabled (required for internal VCC)
    0xAF,       // Display ON
};

// Send each command
for (int i = 0; i < sizeof(init_commands); i++) {
    oled_send_cmd(init_commands[i]);
}
```

---

## Framebuffer Structure

### Memory Organization
- **Total size**: 1024 bytes (128 columns x 8 pages)
- **Format**: MONO_VLSB (Vertical LSB first)
- **Pages**: 8 pages of 8 pixels high each

### Layout
```
Page 0: Rows 0-7    (128 bytes)
Page 1: Rows 8-15   (128 bytes)
Page 2: Rows 16-23  (128 bytes)
Page 3: Rows 24-31  (128 bytes)  <- Visible area start (with offset Y=24)
Page 4: Rows 32-39  (128 bytes)
Page 5: Rows 40-47  (128 bytes)
Page 6: Rows 48-55  (128 bytes)
Page 7: Rows 56-63  (128 bytes)  <- Visible area end
```

### Pixel Position Calculation
```c
void set_pixel(int visible_x, int visible_y, uint8_t color) {
    // Apply offset
    int buf_x = visible_x + OLED_OFFSET_X;  // +28
    int buf_y = visible_y + OLED_OFFSET_Y;  // +24
    
    // Calculate page and bit
    int page = buf_y / 8;
    int bit = buf_y % 8;
    
    // Calculate buffer index
    int index = page * 128 + buf_x;
    
    // Modify bit
    if (color) {
        framebuffer[index] |= (1 << bit);   // Turn on
    } else {
        framebuffer[index] &= ~(1 << bit);  // Turn off
    }
}
```

---

## Send Framebuffer to Display

```c
void oled_update(void) {
    // Set column address range (0-127)
    oled_send_cmd(0x21);  // SET_COL_ADDR
    oled_send_cmd(0);     // Start column
    oled_send_cmd(127);   // End column
    
    // Set page address range (0-7)
    oled_send_cmd(0x22);  // SET_PAGE_ADDR
    oled_send_cmd(0);     // Start page
    oled_send_cmd(7);     // End page
    
    // Send all 1024 bytes of framebuffer
    oled_send_data(framebuffer, 1024);
}
```

---

## Useful SSD1306 Commands

| Command | Bytes | Description |
|---------|-------|-------------|
| 0xAE | 1 | Display OFF |
| 0xAF | 1 | Display ON |
| 0x81, val | 2 | Set contrast (0-255) |
| 0xA6 | 1 | Normal display |
| 0xA7 | 1 | Inverted display |
| 0x20, mode | 2 | Addressing mode (0=horiz, 1=vert, 2=page) |
| 0x21, start, end | 3 | Column range |
| 0x22, start, end | 3 | Page range |

---

## Complete Minimal Example

```c
#include "driver/i2c.h"
#include <string.h>

#define I2C_NUM         I2C_NUM_0
#define SDA_PIN         5
#define SCL_PIN         6
#define OLED_ADDR       0x3C
#define OFFSET_X        28
#define OFFSET_Y        24
#define VISIBLE_W       72
#define VISIBLE_H       40

static uint8_t fb[1024];  // Framebuffer 128x64

void send_cmd(uint8_t cmd) {
    uint8_t buf[2] = {0x80, cmd};
    i2c_master_write_to_device(I2C_NUM, OLED_ADDR, buf, 2, 100);
}

void send_data(uint8_t *data, size_t len) {
    uint8_t *buf = malloc(len + 1);
    buf[0] = 0x40;
    memcpy(buf + 1, data, len);
    i2c_master_write_to_device(I2C_NUM, OLED_ADDR, buf, len + 1, 100);
    free(buf);
}

void oled_init(void) {
    // I2C init
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM, &conf);
    i2c_driver_install(I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);
    
    // Display init sequence
    uint8_t cmds[] = {0xAE, 0x20, 0x00, 0x40, 0xA1, 0xA8, 0x3F, 0xC8,
                      0xD3, 0x00, 0xDA, 0x12, 0xD5, 0x80, 0xD9, 0xF1,
                      0xDB, 0x30, 0x81, 0xFF, 0xA4, 0xA6, 0x8D, 0x14, 0xAF};
    for (int i = 0; i < sizeof(cmds); i++) send_cmd(cmds[i]);
}

void set_pixel(int x, int y, int on) {
    if (x < 0 || x >= VISIBLE_W || y < 0 || y >= VISIBLE_H) return;
    int bx = x + OFFSET_X, by = y + OFFSET_Y;
    int idx = (by / 8) * 128 + bx;
    if (on) fb[idx] |= (1 << (by % 8));
    else    fb[idx] &= ~(1 << (by % 8));
}

void oled_update(void) {
    send_cmd(0x21); send_cmd(0); send_cmd(127);
    send_cmd(0x22); send_cmd(0); send_cmd(7);
    send_data(fb, 1024);
}

void oled_clear(void) { memset(fb, 0, 1024); }

void app_main(void) {
    oled_init();
    oled_clear();
    
    // Draw 9 pixels: corners + center + midpoints
    set_pixel(0, 0, 1);              // Top-left corner
    set_pixel(VISIBLE_W/2, 0, 1);    // Top center
    set_pixel(VISIBLE_W-1, 0, 1);    // Top-right corner
    set_pixel(0, VISIBLE_H/2, 1);    // Middle left
    set_pixel(VISIBLE_W/2, VISIBLE_H/2, 1);  // Center
    set_pixel(VISIBLE_W-1, VISIBLE_H/2, 1);  // Middle right
    set_pixel(0, VISIBLE_H-1, 1);    // Bottom-left corner
    set_pixel(VISIBLE_W/2, VISIBLE_H-1, 1);  // Bottom center
    set_pixel(VISIBLE_W-1, VISIBLE_H-1, 1);  // Bottom-right corner
    
    oled_update();
}
```

---

## Text Rendering and Placement (Code-Neutral)

This section describes how to render bitmap text and center it in regions of the visible area. The description is independent of programming language and microcontroller; it relies only on a “set pixel at (x, y)” primitive in visible coordinates (0 to VISIBLE_W−1, 0 to VISIBLE_H−1).

### Bitmap font

- **Glyph size**: Fixed width GW and height GH (e.g. 5×7).
- **Glyph representation**: One glyph = GW columns. Each column is one byte: bit 0 = top row, bit (GH−1) = bottom row. So each glyph is a sequence of GW bytes.
- **Character set**: Store a table of glyphs indexed by character code (e.g. 0–9 for digits, then A–D for letters). Map the desired character code to the corresponding glyph in the table.

### Drawing one character

- **Input**: Top-left corner (x, y) in visible coordinates, and the glyph (array of GW bytes).
- **Algorithm**: For column c from 0 to GW−1 and row r from 0 to GH−1: if bit r of glyph[c] is set, set pixel at (x + c, y + r). (Bit r = (glyph[c] >> r) & 1 in zero-based indexing.)

### Quadrant layout (72×40 visible)

- **Quadrant size**: Q_W = visible_width / 2, Q_H = visible_height / 2 (e.g. 36×20).
- **Quadrant origins** (top-left of each quadrant): (0, 0), (Q_W, 0), (0, Q_H), (Q_W, Q_H) — corresponding to upper-left, upper-right, lower-left, lower-right.

### Centering one character in a quadrant

- **Input**: Quadrant origin (qx, qy), quadrant size (Q_W, Q_H), glyph size (GW, GH).
- **Output**: Top-left (left, top) so the glyph is centered in the quadrant.
- **Formula**: left = qx + (Q_W − GW) / 2, top = qy + (Q_H − GH) / 2 (use integer division). Then draw the character with top-left at (left, top).

With these rules, any agent or implementation can reproduce text placement using only a pixel buffer and a set-pixel function, regardless of language or platform.

---

## Additional Notes

### Alternative: SH1106 Driver
Some 0.42" displays use the SH1106 controller instead of SSD1306. The main differences are:
- SH1106 uses page addressing mode (not horizontal)
- SH1106 typically requires a column offset of 2 pixels

If SSD1306 code doesn't work, try with offset X=2, Y=0 and use page addressing.

### Offset Calibration
If pixels don't appear correctly, the offset may vary depending on the display manufacturer. Calibration method:
1. Draw pixels at the 4 corners of the assumed "visible" area
2. If pixels are missing on one side, adjust the corresponding offset
3. Increasing the offset moves pixels toward the center of the buffer

### References
- SSD1306 Datasheet: Controller commands and registers
- SH1106 Datasheet: Compatible alternative controller
- ESP-IDF I2C Driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/i2c.html

---

## Change History

| Date | Change |
|------|--------|
| 2026-02-01 | Initial document. Calibrated offset: X=28, Y=24 |
| 2026-02-01 | Added "Text Rendering and Placement (Code-Neutral)" section for bitmap font and quadrant centering |
