# 0.42" OLED Fonts Demo (U8g2)

Demo de generación de texto en una OLED 0.42" (72x40 píxeles visibles) usando la librería **U8g2** con ESP32-C3 y ESP-IDF.

## Requisitos

- ESP-IDF v5.x (target: `esp32c3`)
- OLED 0.42" compatible SSD1306 por I2C (dirección 0x3C)
- Conexiones: SDA → GPIO 5, SCL → GPIO 6, VCC 3.3V, GND

## Componentes

El proyecto incluye en `components/`:

- **u8g2**: Librería [olikraus/u8g2](https://github.com/olikraus/u8g2) (fuentes y gráficos).
- **u8g2_hal_esp_idf**: HAL para ESP32 ([mkfrey/u8g2-hal-esp-idf](https://github.com/mkfrey/u8g2-hal-esp-idf)) para I2C.

No es necesario instalar dependencias externas; los componentes están en el árbol del proyecto.

## Compilar y flashear

```bash
# Desde la raíz del proyecto (0p42-OLED-fonts)
. $IDF_PATH/export.sh   # o la ruta de tu ESP-IDF
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Ajusta el puerto (`-p`) según tu sistema.

## Qué muestra el demo

El firmware alterna cinco pantallas cada ~2.5 s:

1. **Pantalla 1**: Fuente 5x7. Textos "0.42 OLED", "U8g2", "72x40", "fonts", "0-9 A-Z", "!?.,:" en la zona visible.
2. **Pantalla 2**: Fuente 6x10. "U8g2 6x10", "Hello!", "123 45.6" y un marco alrededor del área visible.
3. **Pantalla 3**: Fuente 6x12 para números y símbolos; línea inferior con 5x7 "demo".
4. **Pantalla 4**: Cuatro cuadrantes (36x20 cada uno) con etiquetas Q1–Q4 y marcos, mostrando uso de layout y `DrawFrame`.
5. **Pantalla 5 (tamaño máximo)**: Un número y una letra (p. ej. "8" y "A") con la fuente **spleen16x32** (16×32 px por glifo), el tamaño más grande que cabe en 72×40 sin cortes ni solapamiento: uno centrado en la mitad izquierda y otro en la derecha.

Todas las coordenadas de dibujo se expresan en **área visible** (0–71 x 0–39); el offset del display (28, 24) se aplica internamente mediante macros `draw_str_visible` y `draw_frame_visible`.

## Documentación técnica

Ver [TECHNICAL_DOCUMENTATION.md](TECHNICAL_DOCUMENTATION.md) para hardware, offset, U8g2 y comandos SSD1306.
