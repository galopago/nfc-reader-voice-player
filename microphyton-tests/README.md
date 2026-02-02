# Pruebas OLED 0.42" con MicroPython (ESP32-C3)

Pruebas para la pantalla OLED 0.42" integrada en la placa ESP32-C3. Esta OLED **no empieza en (0,0)**; la zona visible está desplazada, por lo que hay que usar **offset** al dibujar (SSD1306) o **rotación** (SH1106).

**Pines I2C en esta placa:** SDA = 5, SCL = 6.

Referencia: [How to use onboard 0.42 inch OLED for ESP32-C3 OLED development board with micropython](https://electronics.stackexchange.com/questions/725871/how-to-use-onboard-0-42-inch-oled-for-esp32-c3-oled-development-board-with-micro)

---

## Requisitos

- **MicroPython** flasheado en la ESP32-C3 (ver más abajo).
- Cable USB, puerto serie (en Linux suele ser `/dev/ttyACM0`).

---

## Flashear MicroPython (solo cuando quieras instalar MicroPython)

**Importante:** Flashear MicroPython **borra** el firmware actual. Ya tienes una copia en `backup_esp32c3_firmware.bin` en la raíz del proyecto si quieres volver al firmware anterior.

1. **Descargar firmware** para ESP32-C3 desde [MicroPython – ESP32_GENERIC_C3](https://micropython.org/download/ESP32_GENERIC_C3/) (elegir versión estable, 4 MB flash).

2. **Activar entorno ESP-IDF** (para usar `esptool.py`):
   ```bash
   source /home/libertad/esp/esp-idf/export.sh
   ```

3. **Borrar la flash** (una sola vez):
   ```bash
   esptool.py -p /dev/ttyACM0 erase_flash
   ```

4. **Grabar el firmware** (sustituir `<firmware>.bin` por la ruta al archivo descargado):
   ```bash
   esptool.py -p /dev/ttyACM0 -b 460800 write_flash 0 <firmware>.bin
   ```

Si la placa no entra en modo bootloader, mantener pulsado BOOT, pulsar RESET, soltar RESET y luego BOOT.

---

## Subir archivos a la placa

Usar **mpremote** (recomendado):

```bash
pip install mpremote
```

Subir drivers y scripts (desde la carpeta `microphyton-tests/`):

```bash
cd microphyton-tests
mpremote connect /dev/ttyACM0 cp ssd1306.py :ssd1306.py
mpremote connect /dev/ttyACM0 cp sh1106.py :sh1106.py
mpremote connect /dev/ttyACM0 cp test_ssd1306_offset.py :test_ssd1306_offset.py
mpremote connect /dev/ttyACM0 cp test_sh1106.py :test_sh1106.py
```

(Opcional) Subir `boot.py`:

```bash
mpremote connect /dev/ttyACM0 cp boot.py :boot.py
```

---

## Ejecutar las pruebas

**Prueba con SSD1306 (offset 27, 24):**

```bash
mpremote connect /dev/ttyACM0 run test_ssd1306_offset.py
```

**Prueba con SH1106 (128x64, rotate):**

```bash
mpremote connect /dev/ttyACM0 run test_sh1106.py
```

Si en tu placa una de las dos no se ve bien, prueba la otra. Algunas placas funcionan mejor con SSD1306 y offset; otras con SH1106 y `rotate(1)`.

---

## Nota sobre la OLED 0.42"

- La **región visible** está desplazada respecto al buffer 128x64.
- Con **SSD1306** se compensa dibujando con offset (ej. texto en (27, 24), (27, 34), etc.). Puedes ajustar `OLED_OFFSET_X` y `OLED_OFFSET_Y` en `test_ssd1306_offset.py` si tu placa usa otros valores (p. ej. 30, 12).
- Con **SH1106** 128x64 y `rotate(1)` a veces basta con centrar el texto (ej. 40, 16). Si la imagen sale invertida o mal orientada, prueba quitar `oled.rotate(1)` o usar `rotate=90` en el constructor del driver según la versión.

---

## Restaurar el firmware anterior

Si quieres volver al firmware que tenías antes de MicroPython:

```bash
source /home/libertad/esp/esp-idf/export.sh
esptool.py -p /dev/ttyACM0 write_flash 0 ../backup_esp32c3_firmware.bin
```
