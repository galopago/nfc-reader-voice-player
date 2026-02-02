/*
 * i2c_sniffer.c
 * Passive I2C capture: GPIO ISRs push START/STOP/BIT into queue; task runs FSM and emits traces.
 */
#include "i2c_sniffer.h"
#include "trace_output.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

#define SNIFF_EV_START  0
#define SNIFF_EV_STOP   1
#define SNIFF_EV_BIT    2

#define EV_QUEUE_LEN    128
#define MAX_DATA_BYTES  64

typedef struct {
    uint8_t type;  /* SNIFF_EV_* */
    uint8_t value; /* for BIT: 0 or 1 */
} sniff_ev_t;

static QueueHandle_t s_ev_queue;
static int s_gpio_sda = -1;
static int s_gpio_scl = -1;

static void IRAM_ATTR gpio_scl_isr(void *arg)
{
    int level = gpio_get_level(s_gpio_sda);
    sniff_ev_t ev = { .type = SNIFF_EV_BIT, .value = (uint8_t)(level & 1) };
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_ev_queue, &ev, &woken);
    portYIELD_FROM_ISR(woken);
}

static void IRAM_ATTR gpio_sda_isr(void *arg)
{
    int scl = gpio_get_level(s_gpio_scl);
    if (scl == 0) return; /* Only care about SDA change when SCL is high (START/STOP) */
    int sda = gpio_get_level(s_gpio_sda);
    sniff_ev_t ev;
    if (sda == 0) {
        ev.type = SNIFF_EV_START;
        ev.value = 0;
    } else {
        ev.type = SNIFF_EV_STOP;
        ev.value = 0;
    }
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_ev_queue, &ev, &woken);
    portYIELD_FROM_ISR(woken);
}

typedef enum {
    ST_IDLE,
    ST_ADDR,
    ST_ACK_ADDR,
    ST_DATA,
    ST_ACK_DATA,
} sniffer_state_t;

static void sniffer_task(void *arg)
{
    sniff_ev_t ev;
    sniffer_state_t state = ST_IDLE;
    uint8_t bit_count = 0;
    uint8_t addr_byte = 0;
    uint8_t data_byte = 0;
    uint8_t addr = 0;
    uint8_t rw = 0;      /* 0=W, 1=R */
    uint8_t ack = 0;
    uint8_t data_buf[MAX_DATA_BYTES];
    unsigned data_len = 0;
    char line_buf[320];
    unsigned line_len = 0;

    (void)arg;

    while (1) {
        if (xQueueReceive(s_ev_queue, &ev, portMAX_DELAY) != pdTRUE) continue;

        if (ev.type == SNIFF_EV_START) {
            if (state != ST_IDLE && line_len > 0) {
                /* Emit previous transaction (incomplete) then start new */
                trace_send_buf(line_buf, line_len);
                trace_send_buf("\n", 1);
            }
            state = ST_ADDR;
            bit_count = 0;
            addr_byte = 0;
            data_len = 0;
            line_len = (unsigned)snprintf(line_buf, sizeof(line_buf), "S");
        }
        else if (ev.type == SNIFF_EV_STOP) {
            if (state != ST_IDLE && line_len > 0) {
                if (line_len + 3 <= sizeof(line_buf)) {
                    line_len += (unsigned)snprintf(line_buf + line_len, sizeof(line_buf) - line_len, ",P");
                }
                trace_send_line("%s", line_buf);
            }
            state = ST_IDLE;
            line_len = 0;
        }
        else if (ev.type == SNIFF_EV_BIT) {
            if (state == ST_ADDR) {
                addr_byte = (addr_byte << 1) | (ev.value & 1);
                bit_count++;
                if (bit_count == 8) {
                    addr = (uint8_t)(addr_byte >> 1);
                    rw = addr_byte & 1;
                    line_len += (unsigned)snprintf(line_buf + line_len, sizeof(line_buf) - line_len,
                        ",0x%02X,%s", (unsigned)addr, rw ? "R" : "W");
                    state = ST_ACK_ADDR;
                    bit_count = 0;
                }
            }
            else if (state == ST_ACK_ADDR) {
                ack = ev.value & 1;
                line_len += (unsigned)snprintf(line_buf + line_len, sizeof(line_buf) - line_len,
                    ",%s", ack ? "N" : "A");
                state = ST_DATA;
                bit_count = 0;
                data_byte = 0;
            }
            else if (state == ST_DATA) {
                data_byte = (data_byte << 1) | (ev.value & 1);
                bit_count++;
                if (bit_count == 8) {
                    if (data_len < MAX_DATA_BYTES) {
                        data_buf[data_len++] = data_byte;
                        line_len += (unsigned)snprintf(line_buf + line_len, sizeof(line_buf) - line_len,
                            ",0x%02X", (unsigned)data_byte);
                    }
                    state = ST_ACK_DATA;
                    bit_count = 0;
                }
            }
            else if (state == ST_ACK_DATA) {
                ack = ev.value & 1;
                /* Optionally append ,K or ,N; plan uses A/N for addr ACK only; data ACK we can skip or add */
                state = ST_DATA;
                bit_count = 0;
                data_byte = 0;
            }
        }
    }
}

esp_err_t i2c_sniffer_start(int gpio_sda, int gpio_scl)
{
    s_gpio_sda = gpio_sda;
    s_gpio_scl = gpio_scl;

    s_ev_queue = xQueueCreate(EV_QUEUE_LEN, sizeof(sniff_ev_t));
    if (s_ev_queue == NULL) return ESP_ERR_NO_MEM;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << gpio_sda) | (1ULL << gpio_scl),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) return err;

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    if (err == ESP_ERR_INVALID_STATE) { /* already installed */ }

    err = gpio_isr_handler_add(gpio_scl, gpio_scl_isr, NULL);
    if (err != ESP_OK) return err;
    err = gpio_isr_handler_add(gpio_sda, gpio_sda_isr, NULL);
    if (err != ESP_OK) {
        gpio_isr_handler_remove(gpio_scl);
        return err;
    }

    /* SCL: trigger only on rising edge (data valid). Reconfigure after generic anyedge. */
    gpio_set_intr_type(gpio_scl, GPIO_INTR_POSEDGE);
    gpio_set_intr_type(gpio_sda, GPIO_INTR_ANYEDGE);

    xTaskCreate(sniffer_task, "sniffer", 3072, NULL, 5, NULL);
    return ESP_OK;
}
