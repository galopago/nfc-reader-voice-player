/*
 * ESP32-C3 + PN532 NFC reader application.
 * Init sequence and polling task per TECHNICAL_DOCUMENTATION.md §9, §12, §14.
 */

#include "pn532.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define POLL_TASK_STACK   4096
#define POLL_TASK_PRIO    5

/* Callbacks (doc §16.1) - set before nfc_start_scanning */
static void (*s_tag_detected_cb)(const pn532_tag_info_t *tag) = NULL;
static void (*s_tag_removed_cb)(void) = NULL;

/* Polling state (doc §12.1, §13.3) */
static uint8_t s_last_uid[PN532_MAX_UID_LEN];
static uint8_t s_last_uid_length = 0;
static int64_t s_last_detection_time_ms = 0;
static bool s_tag_was_present = false;
static unsigned s_consecutive_misses = 0;

static bool compare_uid(const uint8_t *a, uint8_t len_a, const uint8_t *b, uint8_t len_b)
{
    if (len_a != len_b) {
        return false;
    }
    for (uint8_t i = 0; i < len_a; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static const char *tag_type_str(pn532_tag_type_t type)
{
    switch (type) {
        case PN532_TAG_MIFARE_ULTRALIGHT: return "MIFARE Ultralight";
        case PN532_TAG_NTAG:              return "NTAG";
        case PN532_TAG_MIFARE_CLASSIC_1K: return "MIFARE Classic 1K";
        case PN532_TAG_MIFARE_CLASSIC_4K: return "MIFARE Classic 4K";
        case PN532_TAG_MIFARE_PLUS:       return "MIFARE Plus";
        case PN532_TAG_MIFARE_DESFIRE:    return "MIFARE DESFire";
        default:                          return "Unknown";
    }
}

/* Debug: tag detected (doc §14.1) */
static void on_tag_detected(const pn532_tag_info_t *tag)
{
    printf("========================================\n");
    printf("NFC: Tag detected!\n");
    printf("  UID: ");
    for (uint8_t i = 0; i < tag->uid_length; i++) {
        printf("%02X", tag->uid[i]);
        if (i < tag->uid_length - 1) {
            printf(":");
        }
    }
    printf(" (%u bytes)\n", (unsigned)tag->uid_length);
    printf("  Type: %s\n", tag_type_str(tag->type));
    printf("  SAK: 0x%02X\n", tag->sak);
    printf("  ATQA: 0x%02X 0x%02X\n", tag->atqa[0], tag->atqa[1]);
    printf("========================================\n");

    if (s_tag_detected_cb) {
        s_tag_detected_cb(tag);
    }
}

/* Debug: tag removed (doc §14.2) */
static void on_tag_removed(void)
{
    printf("NFC: Tag removed\n");
    if (s_tag_removed_cb) {
        s_tag_removed_cb();
    }
}

void nfc_register_tag_detected_callback(void (*cb)(const pn532_tag_info_t *))
{
    s_tag_detected_cb = cb;
}

void nfc_register_tag_removed_callback(void (*cb)(void))
{
    s_tag_removed_cb = cb;
}

static void polling_task(void *arg)
{
    (void)arg;
    for (;;) {
        pn532_tag_info_t tag;
        pn532_err_t err = pn532_read_passive_target(PN532_TAG_DETECT_TIMEOUT_MS, &tag);

        if (err == PN532_OK) {
            s_consecutive_misses = 0;
            int64_t now = (int64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
            bool is_same_tag = compare_uid(tag.uid, tag.uid_length,
                                           s_last_uid, s_last_uid_length);
            bool debounce_expired = (now - s_last_detection_time_ms) > PN532_DEBOUNCE_MS;
            bool should_notify = !is_same_tag || debounce_expired || !s_tag_was_present;

            if (should_notify) {
                memcpy(s_last_uid, tag.uid, tag.uid_length);
                s_last_uid_length = tag.uid_length;
                s_last_detection_time_ms = now;
                s_tag_was_present = true;
                on_tag_detected(&tag);
            }

            pn532_release_target();
        } else if (err == PN532_ERR_NOT_FOUND || err == PN532_ERR_TIMEOUT) {
            s_consecutive_misses++;
            if (s_tag_was_present && s_consecutive_misses >= PN532_REMOVAL_THRESHOLD) {
                s_tag_was_present = false;
                on_tag_removed();
            }
        } else {
            /* Communication error - log and continue */
            printf("NFC: Communication error %d\n", (int)err);
        }

        vTaskDelay(pdMS_TO_TICKS(PN532_POLL_INTERVAL_MS));
    }
}

void nfc_start_scanning(void)
{
    xTaskCreate(polling_task, "nfc_poll", POLL_TASK_STACK, NULL, POLL_TASK_PRIO, NULL);
}

/* Full init (doc §9.1) */
static pn532_err_t nfc_init(void)
{
    pn532_err_t err = pn532_init();
    if (err != PN532_OK) {
        printf("NFC: I2C init failed %d\n", (int)err);
        return err;
    }

    pn532_wakeup();
    vTaskDelay(pdMS_TO_TICKS(PN532_POST_WAKEUP_MS));
    vTaskDelay(pdMS_TO_TICKS(PN532_POST_INIT_MS));

    pn532_firmware_version_t fw;
    int retries = 3;
    do {
        err = pn532_get_firmware_version(&fw);
        if (err == PN532_OK) {
            break;
        }
        retries--;
        if (retries > 0) {
            pn532_wakeup();
            vTaskDelay(pdMS_TO_TICKS(PN532_RETRY_DELAY_MS));
        }
    } while (retries > 0);

    if (err != PN532_OK) {
        printf("NFC: GetFirmwareVersion failed after retries %d\n", (int)err);
        return err;
    }

    if (fw.ic != 0x32) {
        printf("NFC: Warning - unexpected IC 0x%02X (expected 0x32)\n", fw.ic);
    }

    err = pn532_sam_config();
    if (err != PN532_OK) {
        printf("NFC: SAM config failed %d\n", (int)err);
        return err;
    }

    return PN532_OK;
}

void app_main(void)
{
    printf("NFC: Initializing PN532...\n");

    pn532_err_t err = nfc_init();
    if (err != PN532_OK) {
        printf("NFC: Init failed %d\n", (int)err);
        return;
    }

    printf("NFC: Init OK. Starting polling.\n");
    nfc_start_scanning();
}
