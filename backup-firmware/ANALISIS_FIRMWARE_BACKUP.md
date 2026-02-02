# Análisis del binario de backup (firmware original de la tarjeta)

## Objetivo

Entender cómo el firmware original controla la pantalla OLED 0.42" para reproducir la configuración, ya que con MicroPython y con el demo ESP-IDF la pantalla no muestra nada.

## Resumen del binario

- **Tamaño**: 4 MB (dump completo de flash).
- **Particiones** (offset 0x8000):
  - `nvs` @ 0x9000, 20 KB
  - `otadata` @ 0xe000, 8 KB
  - `app0` @ **0x10000**, 1.25 MB (aplicación principal)
  - `app1` @ 0x150000, 1.25 MB (OTA)
  - `spiffs` @ 0x160000

## Información extraída

### Build del firmware original

- **ESP-IDF**: v4.4.6 (3572900934), build "dirty".
- **Fecha de compilación**: 4 Oct 2023, 16:47:06.
- **Origen**: `arduino-lib-builder` → el firmware está construido con **Arduino sobre ESP-IDF**, no solo ESP-IDF puro.

### Drivers presentes

- **I2C**: strings de `i2c_driver_install`, `i2c_set_pin`, `sda gpio`, `scl gpio`, etc. (driver estándar IDF).
- **GPIO**: driver estándar.
- **WiFi**: net80211, AP, etc.

No aparecen strings literales como `"oled"`, `"ssd1306"`, `"sh1106"` ni nombres de librerías de display; las librerías Arduino suelen compilar todo y no dejan el nombre del driver en el binario.

### Posibles pistas de display (no concluyentes)

- **0xA8 0x27**: aparece 4 veces en el binario. En SSD1306, 0xA8 es "Set Multiplex Ratio" y 0x27 = 39 → 40 líneas. Coincide con una pantalla **72×40**.
- **0xA8 0x3F**: 1 vez (64 líneas, 128×64).
- **0xD3 0x00**: 10 veces (Set Display Offset = 0).
- **0x8D 0x14**: 1 vez (Charge Pump Enable, típico en init SSD1306).
- **0x81 0xCF** / **0x81 0x8F**: contrast (muchas ocurrencias).

En todos los casos revisados, estos bytes aparecen en **contexto de código máquina (RISC-V)** o datos genéricos, no en una tabla de inicialización claramente identificable. No se puede asegurar que formen parte de una secuencia de init del display.

### Lo que no se puede deducir del binario

- **Pines I2C (SDA/SCL)**: no hay constantes claras 5 y 6 en una estructura de config; podrían ser otros (p. ej. 8/9, 21/22 según placa).
- **Dirección I2C**: 0x3C o 0x3D; no hay string ni tabla que lo fije.
- **Tipo de controlador**: SSD1306 vs SH1106; ambos usan comandos muy similares.
- **Inicialización exacta**: no hay una tabla de bytes tipo `{ 0xAE, 0xD5, 0x80, ... }` localizable de forma fiable.

## Recomendaciones

1. **Comprobar hardware con el firmware original**
   - Restaurar el backup en la tarjeta y comprobar si la pantalla muestra algo (menú, demo, etc.).
   - Si **sí** funciona → el problema está en nuestra configuración (pines, dirección, init, tamaño 72×40).
   - Si **no** funciona ni con el original → revisar conexiones, alimentación y que la OLED sea la correcta.

2. **Ajustar el demo según pantalla 72×40**
   - Usar **altura 40** y **multiplex 40** (0xA8 0x27) si la librería lo permite.
   - Probar **offset vertical** (p. ej. D3) si la 0.42" lo requiere.
   - Probar **0x3C** y **0x3D** y, si hay esquemático, verificar la dirección en el PDF.

3. **Verificar pines en el esquemático**
   - Revisar **ESP32C3 OLED原理图.pdf** para ver qué GPIO están conectados a SDA/SCL de la OLED y anotarlos en `microcontroller-board-pinouts.txt`.

4. **Si tienes el código fuente del firmware original**
   - Con el proyecto Arduino/ESP-IDF se podría ver exactamente: pines, dirección I2C, librería (U8g2, Adafruit_SSD1306, etc.) y secuencia de init.

## Cómo restaurar el backup

```bash
source $IDF_PATH/export.sh
esptool.py -p /dev/ttyACM0 -b 460800 write_flash 0 backup_esp32c3_firmware.bin
```

Tras flashear, reiniciar la placa y comprobar si la OLED muestra algo con el firmware original.
