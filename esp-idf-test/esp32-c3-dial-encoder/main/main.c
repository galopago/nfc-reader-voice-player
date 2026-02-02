/*
 * Quadrature rotary encoder test app on ESP32-C3.
 * Phase A = GPIO0, Phase B = GPIO1.
 * See QUADRATURE_ENCODER_DRIVER_SPEC.md in project root.
 */

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>

#define GPIO_PHASE_A    0
#define GPIO_PHASE_B    1
#define DEBOUNCE_US     5000
#define QUEUE_LEN       10
#define ENCODER_TASK_STACK 2048
#define ENCODER_TASK_PRIORITY (configMAX_PRIORITIES - 1)

typedef enum {
    DIRECTION_CW,
    DIRECTION_CCW
} encoder_direction_t;

typedef void (*encoder_callback_t)(encoder_direction_t direction);

/* Quadrature transition table: index = (previous_state << 2) | current_state */
static const int8_t QUADRATURE_TABLE[16] = {
     0,  /* 00→00 no change */
    -1,  /* 00→01 CCW */
    +1,  /* 00→10 CW */
     0,  /* 00→11 invalid */
    +1,  /* 01→00 CW */
     0,  /* 01→01 no change */
     0,  /* 01→10 invalid */
    -1,  /* 01→11 CCW */
    -1,  /* 10→00 CCW */
     0,  /* 10→01 invalid */
     0,  /* 10→10 no change */
    +1,  /* 10→11 CW */
     0,  /* 11→00 invalid */
    +1,  /* 11→01 CW */
    -1,  /* 11→10 CCW */
     0   /* 11→11 no change */
};

static QueueHandle_t s_event_queue = NULL;
static volatile uint8_t s_previous_state = 0;
static volatile int64_t s_last_interrupt_time = 0;
static volatile int32_t s_position_counter = 0;
static encoder_callback_t s_user_callback = NULL;
static portMUX_TYPE s_counter_spinlock = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR encoder_gpio_isr(void *arg)
{
    int64_t now = esp_timer_get_time();
    if ((now - s_last_interrupt_time) < DEBOUNCE_US) {
        return;
    }
    s_last_interrupt_time = now;

    uint8_t a = gpio_get_level(GPIO_PHASE_A);
    uint8_t b = gpio_get_level(GPIO_PHASE_B);
    uint8_t current_state = (a << 1) | b;

    uint8_t index = (s_previous_state << 2) | current_state;
    int8_t direction = QUADRATURE_TABLE[index];
    s_previous_state = current_state;

    if (direction != 0) {
        BaseType_t woken = pdFALSE;
        xQueueSendFromISR(s_event_queue, &direction, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

static void encoder_task(void *arg)
{
    int8_t direction;
    for (;;) {
        if (xQueueReceive(s_event_queue, &direction, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        taskENTER_CRITICAL(&s_counter_spinlock);
        s_position_counter += direction;
        taskEXIT_CRITICAL(&s_counter_spinlock);

        encoder_callback_t cb = s_user_callback;
        if (cb != NULL) {
            if (direction > 0) {
                cb(DIRECTION_CW);
            } else {
                cb(DIRECTION_CCW);
            }
        }
    }
}

static esp_err_t encoder_init(void)
{
    s_event_queue = xQueueCreate(QUEUE_LEN, sizeof(int8_t));
    if (s_event_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << GPIO_PHASE_A) | (1ULL << GPIO_PHASE_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io);

    uint8_t a = gpio_get_level(GPIO_PHASE_A);
    uint8_t b = gpio_get_level(GPIO_PHASE_B);
    s_previous_state = (a << 1) | b;
    s_last_interrupt_time = esp_timer_get_time();

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK) {
        vQueueDelete(s_event_queue);
        s_event_queue = NULL;
        return err;
    }
    gpio_isr_handler_add(GPIO_PHASE_A, encoder_gpio_isr, NULL);
    gpio_isr_handler_add(GPIO_PHASE_B, encoder_gpio_isr, NULL);

    BaseType_t created = xTaskCreate(encoder_task, "encoder", ENCODER_TASK_STACK,
                                    NULL, ENCODER_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        gpio_isr_handler_remove(GPIO_PHASE_A);
        gpio_isr_handler_remove(GPIO_PHASE_B);
        gpio_uninstall_isr_service();
        vQueueDelete(s_event_queue);
        s_event_queue = NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}

void encoder_register_callback(encoder_callback_t callback)
{
    taskENTER_CRITICAL(&s_counter_spinlock);
    s_user_callback = callback;
    taskEXIT_CRITICAL(&s_counter_spinlock);
}

int32_t encoder_get_count(void)
{
    taskENTER_CRITICAL(&s_counter_spinlock);
    int32_t count = s_position_counter;
    taskEXIT_CRITICAL(&s_counter_spinlock);
    return count;
}

void encoder_reset_count(void)
{
    taskENTER_CRITICAL(&s_counter_spinlock);
    s_position_counter = 0;
    taskEXIT_CRITICAL(&s_counter_spinlock);
}

static void debug_rotation_callback(encoder_direction_t direction)
{
    if (direction == DIRECTION_CW) {
        printf("U\n");
    } else {
        printf("D\n");
    }
}

void app_main(void)
{
    esp_err_t err = encoder_init();
    if (err != ESP_OK) {
        printf("encoder_init failed: %s\n", esp_err_to_name(err));
        return;
    }
    encoder_register_callback(debug_rotation_callback);
    printf("Encoder ready. Rotate the dial; U = CW, D = CCW.\n");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("count=%ld\n", (long)encoder_get_count());
    }
}
