# Test OLED 0.42" con driver SSD1306 y offset (ESP32-C3)
# Basado en: https://electronics.stackexchange.com/questions/725871/
# Respuesta Wolfgang Zelinka: X-offset=27, Y-offset=24, display 73x64 visible
# Pines I2C en esta placa: SDA=5, SCL=6

import machine
import ssd1306

# Offset para que el contenido quede en la zona visible de la 0.42"
OLED_OFFSET_X = 27
OLED_OFFSET_Y = 24

i2c = machine.SoftI2C(scl=machine.Pin(6), sda=machine.Pin(5))
oled = ssd1306.SSD1306_I2C(128, 64, i2c)

oled.fill(0)
oled.text("Micro", OLED_OFFSET_X, OLED_OFFSET_Y)
oled.text("   Python", OLED_OFFSET_X, OLED_OFFSET_Y + 10)
oled.text("123456789", OLED_OFFSET_X, OLED_OFFSET_Y + 20)
oled.text("M_._,_._._", OLED_OFFSET_X, OLED_OFFSET_Y + 32)
oled.show()
