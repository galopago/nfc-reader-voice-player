/*
 * i2c_sniffer.h
 * Passive I2C monitor: SDA/SCL as GPIO inputs, decode START/address/data/STOP.
 * Events are queued and processed in a task; traces emitted via trace_output.
 */
#ifndef I2C_SNIFFER_H
#define I2C_SNIFFER_H

#include "esp_err.h"

/* Start sniffer on given SDA/SCL GPIOs (input only; do not drive bus). */
esp_err_t i2c_sniffer_start(int gpio_sda, int gpio_scl);

#endif /* I2C_SNIFFER_H */
