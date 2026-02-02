# Copia del firmware en ESP32-C3

## Activar la herramienta (sin instalar)

Según Espressif, **no hace falta instalar esptool con pip**. Si tienes ESP-IDF instalado, solo hay que activar el entorno para que `esptool` esté disponible:

```bash
source $IDF_PATH/export.sh
```

(Si no tienes `IDF_PATH` definido, usa la ruta de tu instalación, por ejemplo: `source ~/esp/esp-idf/export.sh`.)

A partir de ahí puedes usar `esptool.py` en esa misma terminal.

## Pasos para hacer el backup

1. **Conectar la ESP32-C3 por USB** y comprobar el puerto:
   ```bash
   ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
   ```
   Anota el puerto (p. ej. `/dev/ttyACM0`).

2. **(Opcional)** Comprobar conexión y tamaño de flash:
   ```bash
   esptool.py -p /dev/ttyACM0 flash_id
   ```

3. **Leer toda la flash y guardar** (sustituye el puerto si es distinto):
   ```bash
   esptool.py -p /dev/ttyACM0 -b 460800 read_flash 0 ALL backup_esp32c3_firmware.bin
   ```
   Si `ALL` no funciona en tu versión, usa tamaño fijo (4 MB):
   ```bash
   esptool.py -p /dev/ttyACM0 -b 460800 read_flash 0 0x400000 backup_esp32c3_firmware.bin
   ```

4. **Comprobar el archivo**:
   ```bash
   ls -lh backup_esp32c3_firmware.bin
   ```

## Restaurar el backup

```bash
source $IDF_PATH/export.sh
esptool.py -p /dev/ttyACM0 write_flash 0 backup_esp32c3_firmware.bin
```
