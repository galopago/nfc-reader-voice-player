/**
 * PN532 NFC reader driver for ESP32-C3 (I2C).
 * Based on TECHNICAL_DOCUMENTATION.md - doc §1, §2, §3, §13.
 */

#ifndef PN532_H
#define PN532_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Hardware (doc §1) --- */
#define PN532_I2C_SDA_GPIO      5
#define PN532_I2C_SCL_GPIO      6
#define PN532_I2C_FREQ_HZ       100000
#define PN532_I2C_ADDR_7BIT     0x24
#define PN532_I2C_ADDR_WRITE   (PN532_I2C_ADDR_7BIT << 1)
#define PN532_I2C_ADDR_READ    ((PN532_I2C_ADDR_7BIT << 1) | 1)

/* --- Frame constants (doc §2) --- */
#define PN532_PREAMBLE          0x00
#define PN532_STARTCODE1        0x00
#define PN532_STARTCODE2        0xFF
#define PN532_POSTAMBLE         0x00
#define PN532_TFI_HOST_TO_PN532 0xD4
#define PN532_TFI_PN532_TO_HOST 0xD5
#define PN532_I2C_READY         0x01

/* --- Command / response codes (doc §2) --- */
#define PN532_CMD_GET_FIRMWARE_VERSION  0x02
#define PN532_RSP_GET_FIRMWARE_VERSION  0x03
#define PN532_CMD_SAM_CONFIGURATION     0x14
#define PN532_RSP_SAM_CONFIGURATION     0x15
#define PN532_CMD_IN_LIST_PASSIVE_TARGET 0x4A
#define PN532_RSP_IN_LIST_PASSIVE_TARGET 0x4B
#define PN532_CMD_IN_RELEASE            0x52
#define PN532_RSP_IN_RELEASE            0x53
#define PN532_BAUDRATE_106K_ISO14443A    0x00

/* --- Timeouts and delays in ms (doc §3) --- */
#define PN532_POST_WAKEUP_MS      50
#define PN532_POST_INIT_MS        100
#define PN532_RETRY_DELAY_MS      200
#define PN532_READY_POLL_MS       10
#define PN532_ACK_TIMEOUT_MS      100
#define PN532_RESPONSE_TIMEOUT_MS 1000
#define PN532_TAG_DETECT_TIMEOUT_MS 150
#define PN532_I2C_WRITE_TIMEOUT_MS 500
#define PN532_I2C_READ_TIMEOUT_MS  100

#define PN532_POLL_INTERVAL_MS    250
#define PN532_DEBOUNCE_MS         1000
#define PN532_REMOVAL_THRESHOLD   3

#define PN532_MAX_UID_LEN         10
#define PN532_RESPONSE_BUFFER_LEN 64

/* --- Return codes --- */
typedef enum {
    PN532_OK = 0,
    PN532_ERR_TIMEOUT,
    PN532_ERR_I2C,
    PN532_ERR_ACK,
    PN532_ERR_CHECKSUM,
    PN532_ERR_PARSE,
    PN532_ERR_RESPONSE,
    PN532_ERR_SIZE,
    PN532_ERR_NOT_FOUND,   /* No tag present (normal) */
} pn532_err_t;

/* --- Tag type from SAK (doc §11) --- */
typedef enum {
    PN532_TAG_UNKNOWN,
    PN532_TAG_MIFARE_ULTRALIGHT,
    PN532_TAG_NTAG,
    PN532_TAG_MIFARE_CLASSIC_1K,
    PN532_TAG_MIFARE_CLASSIC_4K,
    PN532_TAG_MIFARE_PLUS,
    PN532_TAG_MIFARE_DESFIRE,
} pn532_tag_type_t;

/* --- Data structures (doc §13) --- */
typedef struct {
    uint8_t ic;      /* 0x32 for PN532 */
    uint8_t version;
    uint8_t revision;
    uint8_t support;
} pn532_firmware_version_t;

typedef struct {
    uint8_t uid[PN532_MAX_UID_LEN];
    uint8_t uid_length;
    uint8_t sak;
    uint8_t atqa[2];
    pn532_tag_type_t type;
} pn532_tag_info_t;

/**
 * Initialize I2C and PN532. Does NOT send wake-up; caller does init sequence.
 * Returns PN532_OK if I2C master is installed (or already existed).
 */
pn532_err_t pn532_init(void);

/**
 * Send wake-up sequence (doc §6.1). Ignore NACK. Caller must delay 50ms after.
 */
void pn532_wakeup(void);

/**
 * Get firmware version (doc §6.2, §9.2). Fills *version on success.
 */
pn532_err_t pn532_get_firmware_version(pn532_firmware_version_t *version);

/**
 * Configure SAM (doc §6.3, §9.3). Must be called before tag detection.
 */
pn532_err_t pn532_sam_config(void);

/**
 * Detect one passive target ISO14443A (doc §6.4, §10.1).
 * timeout_ms: e.g. PN532_TAG_DETECT_TIMEOUT_MS (150).
 * Returns PN532_ERR_NOT_FOUND / PN532_ERR_TIMEOUT when no tag (normal).
 */
pn532_err_t pn532_read_passive_target(uint32_t timeout_ms, pn532_tag_info_t *tag);

/**
 * Release activated tag (doc §6.5, §10.2).
 */
pn532_err_t pn532_release_target(void);

/**
 * Derive tag type from SAK and UID length (doc §11.2).
 */
pn532_tag_type_t pn532_determine_tag_type(uint8_t sak, uint8_t uid_length);

#ifdef __cplusplus
}
#endif

#endif /* PN532_H */
