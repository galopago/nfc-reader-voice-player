/**
 * PN532 NFC reader driver implementation.
 * See TECHNICAL_DOCUMENTATION.md sections 5-10.
 */

#include "pn532.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "pn532";

#define I2C_PORT          I2C_NUM_0
#define I2C_WRITE_TICKS   pdMS_TO_TICKS(PN532_I2C_WRITE_TIMEOUT_MS)
#define I2C_READ_TICKS    pdMS_TO_TICKS(PN532_I2C_READ_TIMEOUT_MS)

/* ACK frame (6 bytes, doc §4.2); when read via I2C, ready byte 0x01 precedes */
static const uint8_t ACK_FRAME[] = { 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00 };

static bool s_i2c_installed = false;

/* Build command frame (doc §8.1). Frame buffer must hold at least 10 + param_count bytes. */
static int build_command_frame(uint8_t *frame, size_t frame_max,
                               uint8_t command, const uint8_t *params, unsigned param_count)
{
    /* LEN = TFI + command + params */
    unsigned len = 1 + 1 + param_count;
    uint8_t lcs = (uint8_t)((0x00 - len) & 0xFF);
    uint16_t dcs_sum = PN532_TFI_HOST_TO_PN532 + command;
    size_t i = 0;

    if (frame_max < (size_t)(9 + param_count)) {
        return -1;
    }

    frame[i++] = PN532_PREAMBLE;
    frame[i++] = PN532_STARTCODE1;
    frame[i++] = PN532_STARTCODE2;
    frame[i++] = (uint8_t)len;
    frame[i++] = lcs;
    frame[i++] = PN532_TFI_HOST_TO_PN532;
    frame[i++] = command;

    for (unsigned k = 0; k < param_count; k++) {
        frame[i] = params[k];
        dcs_sum += params[k];
        i++;
    }

    frame[i++] = (uint8_t)((0x00 - (dcs_sum & 0xFF)) & 0xFF);
    frame[i++] = PN532_POSTAMBLE;
    return (int)i;
}

static pn532_err_t i2c_write(const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
        return PN532_ERR_I2C;
    }
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PN532_I2C_ADDR_7BIT << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, I2C_WRITE_TICKS);
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK) ? PN532_OK : PN532_ERR_I2C;
}

static pn532_err_t i2c_read(uint8_t *buf, size_t len)
{
    if (len == 0) {
        return PN532_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
        return PN532_ERR_I2C;
    }
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PN532_I2C_ADDR_7BIT << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, buf, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, buf + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, I2C_READ_TICKS);
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK) ? PN532_OK : PN532_ERR_I2C;
}

/* Check ready: read 1 byte, true if 0x01 (doc §7.3) */
static bool is_ready(void)
{
    uint8_t b;
    if (i2c_read(&b, 1) != PN532_OK) {
        return false;
    }
    return (b == PN532_I2C_READY);
}

/* Wait for ready with timeout and 10ms poll (doc §7.4) */
static pn532_err_t wait_ready(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        if (is_ready()) {
            return PN532_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(PN532_READY_POLL_MS));
        elapsed += PN532_READY_POLL_MS;
    }
    return PN532_ERR_TIMEOUT;
}

/* Send command and receive ACK (doc §8.2) */
static pn532_err_t send_command(uint8_t command, const uint8_t *params, unsigned param_count)
{
    uint8_t frame[32];
    int frame_len = build_command_frame(frame, sizeof(frame), command, params, param_count);
    if (frame_len < 0) {
        return PN532_ERR_SIZE;
    }

    pn532_err_t err = i2c_write(frame, (size_t)frame_len);
    if (err != PN532_OK) {
        return err;
    }

    err = wait_ready(PN532_ACK_TIMEOUT_MS);
    if (err != PN532_OK) {
        return err;
    }

    /* Read 7 bytes: ready + 6-byte ACK */
    uint8_t ack_buf[7];
    err = i2c_read(ack_buf, 7);
    if (err != PN532_OK) {
        return err;
    }
    if (memcmp(ack_buf + 1, ACK_FRAME, 6) != 0) {
        return PN532_ERR_ACK;
    }
    return PN532_OK;
}

/* Read response: wait ready, read 64 bytes, find 00 00 FF, validate LCS/TFI/DCS, copy data (doc §8.3) */
static pn532_err_t read_response(uint32_t timeout_ms, uint8_t *data, size_t data_max, size_t *data_len)
{
    pn532_err_t err = wait_ready(timeout_ms);
    if (err != PN532_OK) {
        return err;
    }

    uint8_t raw[PN532_RESPONSE_BUFFER_LEN];
    err = i2c_read(raw, sizeof(raw));
    if (err != PN532_OK) {
        return err;
    }

    /* Find start sequence 0x00 0x00 0xFF in first 32 bytes */
    int start = -1;
    for (int i = 0; i <= 29; i++) {
        if (raw[i] == 0x00 && raw[i + 1] == 0x00 && raw[i + 2] == 0xFF) {
            start = i;
            break;
        }
    }
    if (start < 0) {
        return PN532_ERR_PARSE;
    }

    uint8_t len_byte = raw[start + 3];
    uint8_t lcs     = raw[start + 4];
    if (((len_byte + lcs) & 0xFF) != 0x00) {
        return PN532_ERR_CHECKSUM;
    }

    if (raw[start + 5] != PN532_TFI_PN532_TO_HOST) {
        return PN532_ERR_PARSE;
    }

    /* Data length = LEN - 1 (TFI only in LEN) */
    unsigned data_length = len_byte - 1;
    uint8_t dcs = raw[start + 5 + len_byte];

    /* Checksum: TFI + data + DCS should sum to 0 mod 256 */
    uint16_t sum = raw[start + 5];
    for (unsigned i = 0; i < data_length; i++) {
        sum += raw[start + 6 + i];
    }
    sum += dcs;
    if ((sum & 0xFF) != 0x00) {
        return PN532_ERR_CHECKSUM;
    }

    if (data_length > data_max) {
        return PN532_ERR_SIZE;
    }
    memcpy(data, raw + start + 6, data_length);
    *data_len = data_length;
    return PN532_OK;
}

/* --- Public API --- */

pn532_err_t pn532_init(void)
{
    if (s_i2c_installed) {
        return PN532_OK;
    }

    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = PN532_I2C_SDA_GPIO,
        .scl_io_num       = PN532_I2C_SCL_GPIO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = PN532_I2C_FREQ_HZ,
        .clk_flags        = 0,
    };

    esp_err_t ret = i2c_param_config(I2C_PORT, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed %d", ret);
        return PN532_ERR_I2C;
    }

    ret = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed %d", ret);
        return PN532_ERR_I2C;
    }
    s_i2c_installed = true;
    return PN532_OK;
}

void pn532_wakeup(void)
{
    const uint8_t wakeup[] = { 0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    (void)i2c_write(wakeup, sizeof(wakeup));
    /* Caller must delay 50 ms */
}

pn532_err_t pn532_get_firmware_version(pn532_firmware_version_t *version)
{
    if (!version) {
        return PN532_ERR_RESPONSE;
    }

    pn532_err_t err = send_command(PN532_CMD_GET_FIRMWARE_VERSION, NULL, 0);
    if (err != PN532_OK) {
        return err;
    }

    uint8_t data[8];
    size_t len = 0;
    err = read_response(PN532_RESPONSE_TIMEOUT_MS, data, sizeof(data), &len);
    if (err != PN532_OK) {
        return err;
    }
    if (len < 5 || data[0] != PN532_RSP_GET_FIRMWARE_VERSION) {
        return PN532_ERR_RESPONSE;
    }

    version->ic      = data[1];
    version->version = data[2];
    version->revision = data[3];
    version->support = data[4];
    return PN532_OK;
}

pn532_err_t pn532_sam_config(void)
{
    const uint8_t params[] = { 0x01, 0x14, 0x01 }; /* Mode, Timeout, IRQ */
    pn532_err_t err = send_command(PN532_CMD_SAM_CONFIGURATION, params, 3);
    if (err != PN532_OK) {
        return err;
    }

    uint8_t data[4];
    size_t len = 0;
    err = read_response(PN532_RESPONSE_TIMEOUT_MS, data, sizeof(data), &len);
    if (err != PN532_OK) {
        return err;
    }
    if (len < 1 || data[0] != PN532_RSP_SAM_CONFIGURATION) {
        return PN532_ERR_RESPONSE;
    }
    return PN532_OK;
}

pn532_err_t pn532_read_passive_target(uint32_t timeout_ms, pn532_tag_info_t *tag)
{
    if (!tag) {
        return PN532_ERR_RESPONSE;
    }
    memset(tag, 0, sizeof(*tag));

    const uint8_t params[] = { 0x01, PN532_BAUDRATE_106K_ISO14443A }; /* MaxTargets=1, BaudRate */
    pn532_err_t err = send_command(PN532_CMD_IN_LIST_PASSIVE_TARGET, params, 2);
    if (err != PN532_OK) {
        return err;
    }

    uint8_t data[24];
    size_t len = 0;
    err = read_response(timeout_ms, data, sizeof(data), &len);
    if (err == PN532_ERR_TIMEOUT) {
        return PN532_ERR_NOT_FOUND;
    }
    if (err != PN532_OK) {
        return err;
    }
    if (len < 7 || data[0] != PN532_RSP_IN_LIST_PASSIVE_TARGET) {
        return PN532_ERR_RESPONSE;
    }
    if (data[1] == 0) {
        return PN532_ERR_NOT_FOUND;
    }

    tag->atqa[0]     = data[3];
    tag->atqa[1]     = data[4];
    tag->sak        = data[5];
    tag->uid_length = data[6];

    if (tag->uid_length > PN532_MAX_UID_LEN || (7 + tag->uid_length) > len) {
        return PN532_ERR_SIZE;
    }
    memcpy(tag->uid, data + 7, tag->uid_length);
    tag->type = pn532_determine_tag_type(tag->sak, tag->uid_length);
    return PN532_OK;
}

pn532_err_t pn532_release_target(void)
{
    const uint8_t params[] = { 0x00 };
    pn532_err_t err = send_command(PN532_CMD_IN_RELEASE, params, 1);
    if (err != PN532_OK) {
        return err;
    }

    uint8_t data[4];
    size_t len = 0;
    err = read_response(PN532_RESPONSE_TIMEOUT_MS, data, sizeof(data), &len);
    if (err != PN532_OK) {
        return err;
    }
    if (len < 2 || data[0] != PN532_RSP_IN_RELEASE) {
        return PN532_ERR_RESPONSE;
    }
    return PN532_OK;
}

pn532_tag_type_t pn532_determine_tag_type(uint8_t sak, uint8_t uid_length)
{
    switch (sak) {
        case 0x00:
            return (uid_length == 7) ? PN532_TAG_NTAG : PN532_TAG_MIFARE_ULTRALIGHT;
        case 0x08:
            return PN532_TAG_MIFARE_CLASSIC_1K;
        case 0x18:
            return PN532_TAG_MIFARE_CLASSIC_4K;
        case 0x10:
        case 0x11:
            return PN532_TAG_MIFARE_PLUS;
        case 0x20:
            return PN532_TAG_MIFARE_DESFIRE;
        default:
            return PN532_TAG_UNKNOWN;
    }
}
