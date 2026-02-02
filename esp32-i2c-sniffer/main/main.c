/*
 * ESP32-C3 I2C Sniffer (passive monitor).
 * SDA/SCL as GPIO inputs; traces raw I2C (START, addr, R/W, ACK/NAK, data, STOP) over UART.
 * Pins configurable via menuconfig (I2C Sniffer).
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "trace_output.h"
#include "i2c_sniffer.h"

static const char *TAG = "main";

void app_main(void)
{
    int gpio_sda = CONFIG_I2C_SNIFFER_GPIO_SDA;
    int gpio_scl = CONFIG_I2C_SNIFFER_GPIO_SCL;

    trace_send_line("# I2C sniffer started SDA=%d SCL=%d", gpio_sda, gpio_scl);
    ESP_LOGI(TAG, "I2C sniffer started SDA=%d SCL=%d", gpio_sda, gpio_scl);

    esp_err_t err = i2c_sniffer_start(gpio_sda, gpio_scl);
    if (err != ESP_OK) {
        trace_send_line("# ERROR: i2c_sniffer_start failed");
        ESP_LOGE(TAG, "i2c_sniffer_start failed");
        return;
    }

    vTaskDelay(portMAX_DELAY);
}
