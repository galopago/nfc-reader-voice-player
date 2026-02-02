# ESP32 I2C Sniffer

Passive I2C sniffer for ESP32-C3 with ESP-IDF. Monitors the I2C bus without acting as master or slave (only listens to SDA/SCL via GPIO) and outputs raw traces over UART to analyse register mapping on poorly documented devices.

## Wiring

- Connect the **SDA** and **SCL** lines of the I2C bus you want to monitor to the configured GPIOs (default SDA=6, SCL=7).
- **Listen only**: do not power the bus from the ESP32; the bus must have its own pull-ups and be powered by the system generating the traffic.
- Use a common GND between the ESP32 and the system under test.

## Build and flash

Requirement: [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/) installed.

Activate the ESP-IDF environment (as in [Espressif’s instructions](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html)):

```bash
. $IDF_PATH/export.sh
```

(If `IDF_PATH` is not set, use your installation path, e.g. `source ~/esp/esp-idf/export.sh`.)

Then:

```bash
cd esp32-i2c-sniffer
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

To change SDA/SCL pins:

```bash
idf.py menuconfig
# Menu "I2C Sniffer" -> GPIO SDA / GPIO SCL
```

## Output format (text)

The sniffer output is **text**, not binary. It is sent over UART as character lines, one per I2C transaction, with comma-separated fields. This allows reading in a serial monitor, parsing in scripts, or from a future MCP without defining a binary protocol.

### Line structure

| Field    | Meaning                              | Example |
|----------|--------------------------------------|--------|
| **S**    | START                                | `S`    |
| **0xNN** | Slave address (7 bits)               | `0x3C` |
| **W** / **R** | Write / Read                     | `W`    |
| **A** / **N** | ACK / NAK after address          | `A`    |
| **0xNN** …   | Data bytes (hex)                  | `0x00`, `0x0F` |
| **P**    | STOP                                 | `P`    |

### Example

```
S,0x3C,W,A,0x00,0x0F,0x01,P
```

Meaning: START → address **0x3C** → **Write** → **ACK** → data **0x00 0x0F 0x01** → STOP.

### Lines starting with `#`

These are sniffer status messages (not I2C traffic), for example:

```
# I2C sniffer started SDA=6 SCL=7
```

### Default pins (GPIO)

| Function | Default GPIO |
|----------|--------------|
| SDA      | 6            |
| SCL      | 7            |

Change them with `idf.py menuconfig` → **I2C Sniffer** menu.

## Use with MCP (future)

The firmware is designed so an AI agent can access traces via an MCP on the host: the MCP connects to the ESP32 over the serial port (UART), reads the trace lines and exposes them as a resource or tool. All trace output goes through the `trace_output` module; in the future the UART output can be replaced with WiFi (socket) or USB CDC without changing the I2C capture logic.
