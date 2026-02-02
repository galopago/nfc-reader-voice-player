# Technical Documentation: 0.42" OLED with ESP32-C3 (ESP-IDF)

## Summary

This document describes how to control a 0.42-inch OLED display connected to an ESP32-C3 via I2C. The panel uses an SSD1306-class controller with a 128×64 buffer, but only a 72×40 region is physically visible; that region is at a fixed offset in the buffer. **Text and graphics** are rendered using the **U8g2** library. The sections below give hardware constants, the offset rule, and a **language-neutral** procedure to integrate U8g2 and draw only in the visible area so that any agent or implementation (in any language) can replicate the behavior without reference to a specific codebase.

---

## Hardware

### Microcontroller
- **Chip**: ESP32-C3 (RISC-V single-core)
- **Tested variant**: ESP32-C3 (QFN32) revision v0.4
- **Flash**: 4MB integrated

### OLED Display
- **Size**: 0.42 inches
- **Controller**: SSD1306 compatible (actual controller may be SSD1306 or similar)
- **Controller resolution**: 128×64 pixels (internal buffer)
- **Actual visible resolution**: 72×40 pixels
- **Interface**: I2C
- **I2C address (7-bit)**: 0x3C
- **Color**: Monochrome (typically white on black)

### I2C Connections
| Signal | ESP32-C3 GPIO |
|--------|---------------|
| SDA    | 5             |
| SCL    | 6             |
| VCC    | 3.3V          |
| GND    | GND           |

---

## The Offset and Visible Area

### Problem
The controller drives a 128×64 framebuffer. The physical 0.42" panel shows only a **window** of that buffer. The window does not start at (0, 0).

### Constants (language-neutral)
- **Buffer size**: width = 128, height = 64.
- **Visible size**: width = 72, height = 40.
- **Offset of visible window in buffer**: offset_x = 28, offset_y = 24.
- **Visible window in buffer coordinates**: x in [28, 28+72-1] = [28, 99], y in [24, 24+40-1] = [24, 63].
- **User “visible” coordinates**: x in [0, 71], y in [0, 39]. Origin (0,0) = top-left of what is visible on the panel.

### Coordinate rule
For any drawing primitive that targets the buffer, the position that appears at visible coordinate (vx, vy) must be drawn at buffer coordinate (bx, by) where:

- **bx = vx + offset_x**   (i.e. bx = vx + 28)
- **by = vy + offset_y**   (i.e. by = vy + 24)

So: **buffer_x = visible_x + 28**, **buffer_y = visible_y + 24**.

---

## Text and Graphics with U8g2 (Language-Neutral)

This section describes how to integrate U8g2 and use it so that all drawing appears in the 72×40 visible area. The description is **language- and framework-neutral** so it can be implemented in any language (C, C++, Rust, Python, etc.) and any build system.

### 1. Library and HAL

- **U8g2**: Monochrome graphics library that knows how to drive many displays (including SSD1306 128×64) and provides text (multiple built-in fonts) and primitives (lines, rectangles, frames, etc.). Source: [olikraus/u8g2](https://github.com/olikraus/u8g2).
- **Integration**: The application must compile and link the U8g2 core (the “csrc” sources or an equivalent build). No modification of the library is required.
- **HAL (Hardware Abstraction Layer)**: U8g2 does not know the MCU or bus. A small HAL must provide:
  - A **byte callback**: “send these bytes to the display over I2C (or SPI)”. The HAL opens the I2C (or SPI) session, sends the bytes, and closes the session.
  - A **GPIO/delay callback**: optional reset/DC pins and “delay N ms”.
- **Platform**: For ESP32 with ESP-IDF, a ready-made HAL exists: [mkfrey/u8g2-hal-esp-idf](https://github.com/mkfrey/u8g2-hal-esp-idf). It implements the byte callback using the ESP-IDF I2C driver and the GPIO/delay callback. In other environments, implement the same two callbacks using the local I2C/SPI and time APIs.

**Steps (conceptual):**
1. Add or build the U8g2 core so it is available to the application.
2. Add or implement the HAL that connects U8g2 to I2C (and optionally GPIO/delay).
3. In the application, ensure the HAL is initialized (e.g. set I2C pins and, if used, I2C address) before any U8g2 call.

### 2. Display Setup (SSD1306 128×64 I2C)

- **Display type**: SSD1306, 128×64, I2C (not SPI).
- **Setup function**: Use the U8g2 setup that matches “SSD1306 I2C 128×64” with **full buffer** (so the library manages a full 128×64 buffer). The exact name depends on the language binding; in C it is analogous to `u8g2_Setup_ssd1306_i2c_128x64_noname_f`.
- **Callbacks**: Pass the HAL byte callback and the HAL GPIO/delay callback into that setup.
- **I2C address**: The SSD1306 I2C address is 0x3C (7-bit). U8g2 often expects the address in “write” form (e.g. 0x3C << 1 = 0x78). After setup, set the display I2C address in the U8g2 handle to that value (e.g. 0x78).
- **Init**: Call the library’s “init display” and then “set power save off” so the panel is on.

**Steps (conceptual):**
1. Create the display handle (e.g. one instance of the U8g2 context).
2. Initialize the HAL (I2C pins, and optionally I2C bus speed).
3. Call the SSD1306 I2C 128×64 setup with the two callbacks.
4. Set the I2C address (0x3C << 1) on the handle.
5. Call “init display” and “power save off”.

### 3. Coordinate Wrapper (Visible-Only Coordinates)

U8g2 draws into a 128×64 buffer. The 0.42" panel shows only the region at offset (28, 24) and size 72×40. So **every** call that specifies a position (draw string, draw line, draw frame, etc.) must use **buffer** coordinates. To keep the rest of the application simple and correct, use a **wrapper** so the application always thinks in **visible** coordinates [0..71] × [0..39].

- **Rule**: For each drawing function that takes (x, y) or (x, y, width, height), the wrapper must add the offset before calling the underlying U8g2 function:
  - **buffer_x = visible_x + 28**
  - **buffer_y = visible_y + 24**
  - If the call has width/height, pass them unchanged (they are pixel counts).

**Wrapper examples (conceptual):**
- “Draw string at visible (vx, vy) with text S” → call library “draw string” at (vx + 28, vy + 24) with S.
- “Draw frame (rectangle) at visible (vx, vy) with size (w, h)” → call library “draw frame” at (vx + 28, vy + 24) with size (w, h).

Implement one wrapper per drawing primitive used (string, line, frame, box, etc.). The application then always passes visible coordinates and never uses 28 or 24 directly.

**Constants to define (language-neutral):**
- OFFSET_X = 28
- OFFSET_Y = 24
- VISIBLE_WIDTH = 72
- VISIBLE_HEIGHT = 40

### 4. Typical Draw Loop

1. Clear the buffer (library call).
2. Set font if drawing text (library call).
3. Call **only** the wrappers that add the offset (passing visible x, y).
4. Send the buffer to the display (library call, e.g. “send buffer”).

No clipping to 72×40 is strictly necessary if the application never draws outside [0, 71] × [0, 39]; the wrapper ensures that buffer coordinates stay in the visible window [28..99] × [24..63].

### 5. Summary Checklist for Any Implementation

- Obtain/build U8g2 and the HAL (or equivalent) for the target platform.
- Initialize the HAL (I2C pins, address 0x3C).
- Setup display: SSD1306 I2C 128×64, full buffer, with HAL callbacks; set I2C address (0x78); init display; power save off.
- For every drawing call that takes position (x, y): use a wrapper that calls the library with (x + 28, y + 24).
- Application logic uses only visible coordinates in [0, 71] × [0, 39].

---

## ESP-IDF–Specific Notes (This Project)

- **Target**: esp32c3.
- **Components**: U8g2 core is in `components/u8g2` (all csrc built). HAL is in `components/u8g2_hal_esp_idf`. Main application depends on the HAL component, which depends on `u8g2`, `driver`, and `freertos`.
- **HAL init**: Set `bus.i2c.sda` and `bus.i2c.scl` to the GPIO numbers (e.g. 5 and 6), then call the HAL init. The HAL uses the ESP-IDF I2C driver internally.
- **Demo**: The firmware runs a loop of several screens (multiple fonts, strings, numbers, quadrants, and one “max size” screen with two large characters). See the project README.

---

## I2C and Display Reference (Low-Level)

The following is reference only. When using U8g2, the HAL and U8g2 perform these steps internally.

### I2C parameters
- **Port**: e.g. I2C_NUM_0 (ESP32).
- **SDA pin**: 5, **SCL pin**: 6.
- **Address**: 0x3C (7-bit).
- **Speed**: e.g. 400000 Hz (fast mode); some HALs use 50000 Hz.

### SSD1306 I2C protocol (conceptual)
- **Command**: Send control byte 0x80, then one command byte.
- **Data**: Send control byte 0x40, then pixel data bytes.
- **Framebuffer**: 128×64 pixels, 1024 bytes, vertical LSB first (8 rows per “page”).

### Display init sequence (command bytes)
Display OFF; set addressing mode horizontal; set start line 0; segment remap; MUX 64; COM scan direction; display offset 0; COM pins config; clock divide; precharge; VCOMH; contrast 255; display follows RAM; normal (non-inverted); charge pump on; display ON. Exact byte sequences are in the SSD1306 datasheet; U8g2 sends them via the HAL.

### Useful SSD1306 commands
| Command   | Description        |
|-----------|--------------------|
| 0xAE      | Display OFF        |
| 0xAF      | Display ON         |
| 0x81, val | Contrast 0–255     |
| 0xA6 / 0xA7 | Normal / Inverted |
| 0x20, mode | Addressing mode   |
| 0x21, c0, c1 | Column range   |
| 0x22, p0, p1 | Page range     |

---

## Framebuffer Layout (Reference)

- **Size**: 1024 bytes (128 columns × 8 pages; each page = 8 rows).
- **Visible rows** in buffer: rows 24–63 (pages 3–7). Visible columns: 28–99.
- **Pixel at buffer (bx, by)**: byte index = (by / 8) * 128 + bx; bit = by % 8 (LSB = top).

---

## Additional Notes

### SH1106
Some 0.42" panels use SH1106. It may need different init and a different column offset (e.g. 2). Use the SH1106 driver in U8g2 if applicable.

### Offset calibration
If the image is shifted, adjust offset_x and offset_y. Increasing offset moves content toward the center of the 128×64 buffer.

### References
- SSD1306 datasheet (commands and registers)
- SH1106 datasheet (alternative controller)
- [olikraus/u8g2](https://github.com/olikraus/u8g2)
- [mkfrey/u8g2-hal-esp-idf](https://github.com/mkfrey/u8g2-hal-esp-idf) (ESP-IDF HAL)

---

## Change History

| Date       | Change |
|------------|--------|
| 2026-02-01 | Initial document; calibrated offset X=28, Y=24 |
| 2026-02-01 | U8g2 integration; language-neutral U8g2 and wrapper section; removed manual font content |
| 2026-02-01 | Document focused on U8g2 path; low-level I2C/framebuffer kept as reference only |
