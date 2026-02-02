# Test OLED 0.42" con driver SH1106 (ESP32-C3)
# Basado en: https://electronics.stackexchange.com/questions/725871/
# Respuestas robpan / helmut doring: SH1106 128x64, rotate(1) en algunas placas
# Pines I2C: SDA=5, SCL=6

from machine import Pin, I2C
from sh1106 import SH1106_I2C

WIDTH = 128
HEIGHT = 64

i2c = I2C(0, scl=Pin(6), sda=Pin(5), freq=400000)
oled = SH1106_I2C(WIDTH, HEIGHT, i2c)

# En algunas placas la imagen se ve bien con rotate(1) (flip)
oled.rotate(1)
oled.fill(0)
oled.contrast(255)
oled.text("Hello", 40, 16)
oled.text("World!", 40, 32)
oled.show()
