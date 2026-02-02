# PN532 NFC Reader - Complete Technical Specification

This document provides all necessary information to implement a PN532 NFC tag reader from scratch, without requiring the original source code. It is designed to be language and platform agnostic.

---

## 1. Hardware Configuration

### 1.1 Pin Connections

| Signal | GPIO | Description |
|--------|------|-------------|
| SDA | 5 | I2C Data Line |
| SCL | 6 | I2C Clock Line |
| VCC | - | 3.3V Power |
| GND | - | Ground |

### 1.2 I2C Parameters

| Parameter | Value |
|-----------|-------|
| I2C Address | 0x24 (7-bit) |
| Clock Speed | 100,000 Hz (100 kHz) |
| Mode | Master |
| Pull-ups | Internal pull-ups enabled on SDA and SCL |

### 1.3 PN532 Mode Selection

The PN532 module must be configured for I2C mode via hardware switches/jumpers:

| Interface | Switch 1 | Switch 2 |
|-----------|----------|----------|
| **I2C** | OFF (0) | ON (1) |
| UART | OFF (0) | OFF (0) |
| SPI | ON (1) | OFF (0) |

---

## 2. Protocol Constants

### 2.1 Frame Constants

```
PREAMBLE      = 0x00
STARTCODE1    = 0x00
STARTCODE2    = 0xFF
POSTAMBLE     = 0x00
```

### 2.2 Frame Identifiers (TFI)

```
HOST_TO_PN532 = 0xD4  (commands sent TO the PN532)
PN532_TO_HOST = 0xD5  (responses FROM the PN532)
```

### 2.3 Command Codes

| Command | Code | Description |
|---------|------|-------------|
| GetFirmwareVersion | 0x02 | Read chip version info |
| SAMConfiguration | 0x14 | Configure Security Access Module |
| InListPassiveTarget | 0x4A | Detect NFC tags |
| InRelease | 0x52 | Release activated tag |

### 2.4 Response Codes

| Response | Code | For Command |
|----------|------|-------------|
| GetFirmwareVersion | 0x03 | 0x02 |
| SAMConfiguration | 0x15 | 0x14 |
| InListPassiveTarget | 0x4B | 0x4A |
| InRelease | 0x53 | 0x52 |

### 2.5 Card Type Constants

```
BAUDRATE_106K_ISO14443A = 0x00  (for MIFARE, NTAG, etc.)
```

### 2.6 I2C Ready Indicator

```
I2C_READY = 0x01  (PN532 sends this byte when data is available)
```

---

## 3. Timing Specifications

### 3.1 Delays

| Operation | Duration | Description |
|-----------|----------|-------------|
| Post-wakeup delay | 50 ms | After sending wake-up sequence |
| Post-init delay | 100 ms | After I2C initialization |
| Retry delay | 200 ms | Between communication retries |
| Ready poll interval | 10 ms | Between I2C ready checks |

### 3.2 Timeouts

| Operation | Timeout | Description |
|-----------|---------|-------------|
| ACK timeout | 100 ms | Waiting for ACK after command |
| Response timeout | 1000 ms | Waiting for command response |
| Tag detect timeout | 150 ms | Waiting for tag detection |
| I2C write timeout | 500 ms | I2C transaction timeout |
| I2C read timeout | 100 ms | I2C transaction timeout |

### 3.3 Polling Configuration

| Parameter | Default | Min | Description |
|-----------|---------|-----|-------------|
| Poll interval | 250 ms | 50 ms | Time between tag detection attempts |
| Debounce time | 1000 ms | 0 | Ignore same tag for this duration |
| Removal threshold | 3 | - | Consecutive misses before "removed" |

---

## 4. Frame Format Specification

### 4.1 Command Frame Structure

```
┌─────┬─────┬─────┬─────┬─────┬─────┬─────────┬─────┬─────┐
│ PRE │ SC1 │ SC2 │ LEN │ LCS │ TFI │  DATA   │ DCS │POST │
├─────┼─────┼─────┼─────┼─────┼─────┼─────────┼─────┼─────┤
│0x00 │0x00 │0xFF │  n  │~n+1 │0xD4 │CMD+PARAMS│ sum │0x00 │
└─────┴─────┴─────┴─────┴─────┴─────┴─────────┴─────┴─────┘
```

**Field Definitions:**

| Field | Size | Description |
|-------|------|-------------|
| PRE (Preamble) | 1 byte | Always 0x00 |
| SC1 (Start Code 1) | 1 byte | Always 0x00 |
| SC2 (Start Code 2) | 1 byte | Always 0xFF |
| LEN | 1 byte | Length of TFI + DATA (not including checksums) |
| LCS | 1 byte | Length Checksum |
| TFI | 1 byte | Frame Identifier (0xD4 for commands) |
| DATA | n-1 bytes | Command byte + optional parameters |
| DCS | 1 byte | Data Checksum |
| POST (Postamble) | 1 byte | Always 0x00 |

### 4.2 ACK Frame (fixed)

When the PN532 successfully receives a command, it responds with this exact ACK:

```
Bytes: 00 00 FF 00 FF 00
       ├──────────────┤
       Fixed 6-byte sequence
```

When reading via I2C, the ready byte precedes the ACK:

```
Read buffer: 01 00 00 FF 00 FF 00
             │  └────────────────┘
             │         ACK frame
             └── Ready indicator
```

### 4.3 Response Frame Structure

```
┌─────┬─────┬─────┬─────┬─────┬─────┬─────────────┬─────┬─────┐
│ PRE │ SC1 │ SC2 │ LEN │ LCS │ TFI │    DATA     │ DCS │POST │
├─────┼─────┼─────┼─────┼─────┼─────┼─────────────┼─────┼─────┤
│0x00 │0x00 │0xFF │  n  │~n+1 │0xD5 │RSP_CODE+DATA│ sum │0x00 │
└─────┴─────┴─────┴─────┴─────┴─────┴─────────────┴─────┴─────┘
```

When reading via I2C, a ready byte (0x01) precedes the frame.

---

## 5. Checksum Algorithms

### 5.1 Length Checksum (LCS)

The LCS ensures LEN was received correctly.

**Formula:**
```
LCS = (0x00 - LEN) AND 0xFF
```

Alternatively (two's complement):
```
LCS = (~LEN + 1) AND 0xFF
```

**Validation:**
```
(LEN + LCS) AND 0xFF == 0x00
```

**Example:**
```
LEN = 0x02
LCS = (0x00 - 0x02) AND 0xFF = 0xFE
Verification: (0x02 + 0xFE) AND 0xFF = 0x00 ✓
```

### 5.2 Data Checksum (DCS)

The DCS ensures TFI + DATA were received correctly.

**Formula:**
```
sum = TFI + DATA[0] + DATA[1] + ... + DATA[n-1]
DCS = (0x00 - sum) AND 0xFF
```

**Validation:**
```
(TFI + DATA[0] + ... + DATA[n-1] + DCS) AND 0xFF == 0x00
```

**Example (GetFirmwareVersion command):**
```
TFI = 0xD4
CMD = 0x02
sum = 0xD4 + 0x02 = 0xD6
DCS = (0x00 - 0xD6) AND 0xFF = 0x2A
Verification: (0xD4 + 0x02 + 0x2A) AND 0xFF = 0x00 ✓
```

---

## 6. Command Specifications

### 6.1 Wake-up Sequence

Some PN532 modules enter low-power mode and require a wake-up sequence.

**Procedure:**
1. Send these bytes via I2C (ignore NACK errors):
   ```
   55 55 00 00 00 00 00 00 00 00
   ```
2. Wait 50 ms
3. Proceed with normal communication

**Implementation Note:** The wake-up may fail (NACK). This is expected. Ignore the error and proceed.

### 6.2 GetFirmwareVersion (0x02)

Verifies communication and identifies the PN532.

**Command Frame:**
```
00 00 FF 02 FE D4 02 2A 00
│        │  │  │  │  │  └── Postamble
│        │  │  │  │  └───── DCS = ~(0xD4+0x02)+1 = 0x2A
│        │  │  │  └──────── Command = 0x02
│        │  │  └─────────── TFI = 0xD4
│        │  └────────────── LCS = ~0x02+1 = 0xFE
│        └───────────────── LEN = 0x02 (TFI + CMD)
└────────────────────────── Preamble + Start Code
```

**Expected Response:**
```
Frame: 00 00 FF 06 FA D5 03 32 01 06 07 E8 00
                     │  │  │  │  │  │
                     │  │  │  │  │  └── Support flags
                     │  │  │  │  └───── Revision
                     │  │  │  └──────── Version
                     │  │  └─────────── IC (0x32 = PN532)
                     │  └────────────── Response code (0x03)
                     └───────────────── TFI (0xD5)
```

**Response Data Fields:**

| Offset | Field | Expected Value |
|--------|-------|----------------|
| 0 | Response Code | 0x03 |
| 1 | IC | 0x32 (PN532) |
| 2 | Firmware Version | varies (e.g., 1) |
| 3 | Firmware Revision | varies (e.g., 6) |
| 4 | Support | varies (e.g., 0x07) |

### 6.3 SAMConfiguration (0x14)

Configures the Security Access Module for normal operation. **Must be called before reading tags.**

**Parameters:**
```
Mode    = 0x01  (Normal mode)
Timeout = 0x14  (50ms × 20 = 1 second)
IRQ     = 0x01  (Use IRQ pin)
```

**Command Frame:**
```
00 00 FF 05 FB D4 14 01 14 01 02 00
│        │  │  │  │  │  │  │  │  └── Postamble
│        │  │  │  │  │  │  │  └───── DCS
│        │  │  │  │  │  │  └──────── IRQ = 0x01
│        │  │  │  │  │  └─────────── Timeout = 0x14
│        │  │  │  │  └────────────── Mode = 0x01
│        │  │  │  └───────────────── Command = 0x14
│        │  │  └──────────────────── TFI = 0xD4
│        │  └─────────────────────── LCS = 0xFB
│        └────────────────────────── LEN = 0x05
└─────────────────────────────────── Preamble + Start Code
```

**DCS Calculation:**
```
sum = 0xD4 + 0x14 + 0x01 + 0x14 + 0x01 = 0xFE
DCS = (0x00 - 0xFE) AND 0xFF = 0x02
```

**Expected Response:**
```
Frame: 00 00 FF 02 FE D5 15 16 00
                     │  │
                     │  └── Response code (0x15)
                     └───── TFI (0xD5)
```

**Validation:** Response code must be 0x15.

### 6.4 InListPassiveTarget (0x4A)

Detects and activates ISO14443A NFC tags (MIFARE, NTAG, etc.).

**Parameters:**
```
MaxTargets = 0x01  (detect 1 tag)
BaudRate   = 0x00  (106 kbps ISO14443A)
```

**Command Frame:**
```
00 00 FF 04 FC D4 4A 01 00 E1 00
│        │  │  │  │  │  │  │  └── Postamble
│        │  │  │  │  │  │  └───── DCS = 0xE1
│        │  │  │  │  │  └──────── BaudRate = 0x00
│        │  │  │  │  └─────────── MaxTargets = 0x01
│        │  │  │  └────────────── Command = 0x4A
│        │  │  └───────────────── TFI = 0xD4
│        │  └──────────────────── LCS = 0xFC
│        └─────────────────────── LEN = 0x04
└──────────────────────────────── Preamble + Start Code
```

**DCS Calculation:**
```
sum = 0xD4 + 0x4A + 0x01 + 0x00 = 0x1F
DCS = (0x00 - 0x1F) AND 0xFF = 0xE1
```

**Response When Tag Found (example with 7-byte UID):**
```
Frame: 00 00 FF 0F F1 D5 4B 01 01 00 44 00 07 04 7F A9 9A 1F 66 80 C8 00
                     │  │  │  │  │  │  │  │  └──────────────────────┘
                     │  │  │  │  │  │  │  │           UID (7 bytes)
                     │  │  │  │  │  │  │  └── UID Length = 7
                     │  │  │  │  │  │  └───── SAK = 0x00
                     │  │  │  │  │  └──────── ATQA[1] = 0x44
                     │  │  │  │  └─────────── ATQA[0] = 0x00
                     │  │  │  └────────────── Target Number = 0x01
                     │  │  └───────────────── Targets Found = 0x01
                     │  └──────────────────── Response Code = 0x4B
                     └─────────────────────── TFI = 0xD5
```

**Response Data Field Map:**

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | Response Code | 1 | Must be 0x4B |
| 1 | Targets Found | 1 | Number of tags detected (0 or 1) |
| 2 | Target Number | 1 | Always 0x01 for first target |
| 3 | ATQA[0] | 1 | Answer To Request byte 0 |
| 4 | ATQA[1] | 1 | Answer To Request byte 1 |
| 5 | SAK | 1 | Select Acknowledge (indicates tag type) |
| 6 | UID Length | 1 | 4 or 7 bytes |
| 7+ | UID | n | Unique Identifier bytes |

**Response When No Tag (timeout):**

The PN532 will not send the ready byte (0x01) within the timeout period. Treat timeout as "no tag present."

### 6.5 InRelease (0x52)

Releases the currently activated tag. Call after reading to allow re-detection.

**Parameters:**
```
Target = 0x00  (release all targets)
```

**Command Frame:**
```
00 00 FF 03 FD D4 52 00 DA 00
```

**DCS Calculation:**
```
sum = 0xD4 + 0x52 + 0x00 = 0x126 → 0x26 (truncated)
DCS = (0x00 - 0x26) AND 0xFF = 0xDA
```

**Expected Response:**
```
Frame: 00 00 FF 03 FD D5 53 00 D8 00
                     │  │  │
                     │  │  └── Status (0x00 = success)
                     │  └───── Response Code = 0x53
                     └──────── TFI = 0xD5
```

---

## 7. I2C Communication Procedures

### 7.1 Writing to PN532

```
PROCEDURE i2c_write(data[], length):
    1. Send I2C START condition
    2. Send address byte: (0x24 << 1) | 0x00 = 0x48 (write)
    3. Wait for ACK from PN532
    4. FOR each byte in data:
        a. Send byte
        b. Wait for ACK
    5. Send I2C STOP condition
    6. RETURN success/failure
```

### 7.2 Reading from PN532

```
PROCEDURE i2c_read(buffer[], length):
    1. Send I2C START condition
    2. Send address byte: (0x24 << 1) | 0x01 = 0x49 (read)
    3. Wait for ACK from PN532
    4. FOR i = 0 TO length-2:
        a. Read byte into buffer[i]
        b. Send ACK
    5. Read final byte into buffer[length-1]
    6. Send NACK (to signal end of read)
    7. Send I2C STOP condition
    8. RETURN success/failure
```

### 7.3 Checking Ready Status

```
PROCEDURE is_ready():
    1. Read 1 byte from PN532
    2. IF byte == 0x01:
        RETURN true
    3. ELSE:
        RETURN false
```

### 7.4 Waiting for Ready

```
PROCEDURE wait_ready(timeout_ms):
    start_time = current_time()
    WHILE (current_time() - start_time) < timeout_ms:
        IF is_ready():
            RETURN success
        delay(10ms)
    RETURN timeout_error
```

---

## 8. Frame Processing Algorithms

### 8.1 Building a Command Frame

```
PROCEDURE build_command_frame(command, params[], param_count):
    frame = empty array
    
    // Calculate LEN (TFI + command + params)
    len = 1 + 1 + param_count  // TFI + CMD + params
    
    // Calculate LCS (two's complement of LEN)
    lcs = (~len + 1) AND 0xFF
    
    // Build header
    frame.append(0x00)  // Preamble
    frame.append(0x00)  // Start Code 1
    frame.append(0xFF)  // Start Code 2
    frame.append(len)   // LEN
    frame.append(lcs)   // LCS
    
    // Build data section
    frame.append(0xD4)  // TFI (Host to PN532)
    frame.append(command)
    
    // Calculate DCS while adding params
    dcs_sum = 0xD4 + command
    FOR each param in params:
        frame.append(param)
        dcs_sum = dcs_sum + param
    
    // Calculate and append DCS
    dcs = (~dcs_sum + 1) AND 0xFF
    frame.append(dcs)
    
    // Append postamble
    frame.append(0x00)
    
    RETURN frame
```

### 8.2 Sending Command and Receiving ACK

```
PROCEDURE send_command(command, params[], param_count):
    // Build and send frame
    frame = build_command_frame(command, params, param_count)
    result = i2c_write(frame, frame.length)
    IF result != success:
        RETURN write_error
    
    // Wait for ACK
    result = wait_ready(100ms)  // ACK timeout
    IF result != success:
        RETURN timeout_error
    
    // Read ACK frame (7 bytes: ready + 6-byte ACK)
    buffer[7]
    i2c_read(buffer, 7)
    
    // Verify ACK (skip ready byte at index 0)
    expected_ack = [0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00]
    IF buffer[1..6] != expected_ack:
        RETURN invalid_ack_error
    
    RETURN success
```

### 8.3 Reading and Parsing Response

```
PROCEDURE read_response(timeout_ms):
    // Wait for response ready
    result = wait_ready(timeout_ms)
    IF result != success:
        RETURN timeout_error, null
    
    // Read raw frame (64 bytes is enough for any response)
    buffer[64]
    i2c_read(buffer, 64)
    
    // Find start sequence (0x00 0x00 0xFF) in first 32 bytes
    start_index = -1
    FOR i = 0 TO 29:
        IF buffer[i] == 0x00 AND buffer[i+1] == 0x00 AND buffer[i+2] == 0xFF:
            start_index = i
            BREAK
    
    IF start_index == -1:
        RETURN parse_error, null
    
    // Parse frame from start_index
    frame = buffer starting at start_index
    len = frame[3]
    lcs = frame[4]
    
    // Validate LCS
    IF (len + lcs) AND 0xFF != 0x00:
        RETURN checksum_error, null
    
    // Validate TFI
    tfi = frame[5]
    IF tfi != 0xD5:
        RETURN invalid_tfi_error, null
    
    // Extract data (starts at offset 6, length is len-1 because len includes TFI)
    data_length = len - 1
    data = frame[6 .. 6 + data_length - 1]
    
    // Validate DCS
    dcs = frame[5 + len]
    checksum = tfi
    FOR each byte in data:
        checksum = checksum + byte
    checksum = checksum + dcs
    IF checksum AND 0xFF != 0x00:
        RETURN checksum_error, null
    
    RETURN success, data
```

---

## 9. Initialization Sequence

### 9.1 Complete Initialization Algorithm

```
PROCEDURE initialize_nfc_reader():
    // Step 1: Initialize I2C bus
    configure_i2c(
        sda_pin = 5,
        scl_pin = 6,
        frequency = 100000,
        pull_ups = enabled
    )
    
    // Step 2: Send wake-up sequence
    wakeup_bytes = [0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    i2c_write(wakeup_bytes, 10)  // Ignore errors
    delay(50ms)
    
    // Step 3: Wait for PN532 to fully wake up
    delay(100ms)
    
    // Step 4: Verify communication with retries
    retries = 3
    WHILE retries > 0:
        result = get_firmware_version()
        IF result == success:
            BREAK
        retries = retries - 1
        IF retries > 0:
            // Retry with wake-up
            i2c_write(wakeup_bytes, 10)
            delay(200ms)
    
    IF result != success:
        RETURN initialization_failed
    
    // Step 5: Verify IC version (optional but recommended)
    IF firmware.ic != 0x32:
        log_warning("Unexpected IC version")
    
    // Step 6: Configure SAM
    result = sam_configuration()
    IF result != success:
        RETURN sam_config_failed
    
    RETURN success
```

### 9.2 GetFirmwareVersion Implementation

```
PROCEDURE get_firmware_version():
    // Send command (no parameters)
    result = send_command(0x02, [], 0)
    IF result != success:
        RETURN result, null
    
    // Read response
    result, data = read_response(1000ms)
    IF result != success:
        RETURN result, null
    
    // Validate response
    IF data.length < 5 OR data[0] != 0x03:
        RETURN invalid_response, null
    
    // Extract version info
    version.ic = data[1]       // Should be 0x32
    version.ver = data[2]      // Firmware version
    version.rev = data[3]      // Firmware revision
    version.support = data[4]  // Supported features
    
    RETURN success, version
```

### 9.3 SAMConfiguration Implementation

```
PROCEDURE sam_configuration():
    params = [0x01, 0x14, 0x01]  // Mode, Timeout, IRQ
    
    result = send_command(0x14, params, 3)
    IF result != success:
        RETURN result
    
    result, data = read_response(1000ms)
    IF result != success:
        RETURN result
    
    IF data.length < 1 OR data[0] != 0x15:
        RETURN invalid_response
    
    RETURN success
```

---

## 10. Tag Detection and Reading

### 10.1 Read Passive Target Implementation

```
PROCEDURE read_passive_target(timeout_ms):
    params = [0x01, 0x00]  // MaxTargets=1, BaudRate=ISO14443A
    
    result = send_command(0x4A, params, 2)
    IF result != success:
        RETURN result, null
    
    result, data = read_response(timeout_ms)
    IF result == timeout_error:
        RETURN not_found, null  // No tag present
    IF result != success:
        RETURN result, null
    
    // Validate response
    IF data.length < 7 OR data[0] != 0x4B:
        RETURN invalid_response, null
    
    // Check if any targets found
    targets_found = data[1]
    IF targets_found == 0:
        RETURN not_found, null
    
    // Parse tag information
    tag.atqa[0] = data[3]
    tag.atqa[1] = data[4]
    tag.sak = data[5]
    tag.uid_length = data[6]
    
    // Validate UID length
    IF tag.uid_length > 10 OR (7 + tag.uid_length) > data.length:
        RETURN invalid_size, null
    
    // Copy UID
    FOR i = 0 TO tag.uid_length - 1:
        tag.uid[i] = data[7 + i]
    
    RETURN success, tag
```

### 10.2 Release Target Implementation

```
PROCEDURE release_target():
    params = [0x00]  // Release all
    
    result = send_command(0x52, params, 1)
    IF result != success:
        RETURN result
    
    result, data = read_response(1000ms)
    IF result != success:
        RETURN result
    
    IF data.length < 2 OR data[0] != 0x53:
        RETURN invalid_response
    
    RETURN success
```

---

## 11. Tag Type Identification

### 11.1 SAK to Tag Type Mapping

The SAK (Select Acknowledge) byte identifies the tag type:

| SAK Value | Tag Type | UID Length |
|-----------|----------|------------|
| 0x00 | MIFARE Ultralight / NTAG | 4 or 7 bytes |
| 0x08 | MIFARE Classic 1K | 4 bytes |
| 0x18 | MIFARE Classic 4K | 4 bytes |
| 0x10 | MIFARE Plus (2K) | 4 or 7 bytes |
| 0x11 | MIFARE Plus (4K) | 4 or 7 bytes |
| 0x20 | MIFARE DESFire | 7 bytes |

### 11.2 NTAG Detection

NTAG tags have SAK = 0x00 and typically 7-byte UIDs:

```
PROCEDURE determine_tag_type(sak, uid_length):
    SWITCH sak:
        CASE 0x00:
            IF uid_length == 7:
                RETURN NTAG
            ELSE:
                RETURN MIFARE_ULTRALIGHT
        CASE 0x08:
            RETURN MIFARE_CLASSIC_1K
        CASE 0x18:
            RETURN MIFARE_CLASSIC_4K
        CASE 0x10, 0x11:
            RETURN MIFARE_PLUS
        CASE 0x20:
            RETURN MIFARE_DESFIRE
        DEFAULT:
            RETURN UNKNOWN
```

---

## 12. Continuous Polling State Machine

### 12.1 State Definitions

```
STATES:
    IDLE        - Not scanning
    POLLING     - Actively looking for tags
    TAG_PRESENT - Tag detected and being tracked
    RELEASING   - Releasing tag for re-detection
```

### 12.2 Polling Task Algorithm

```
PROCEDURE polling_task():
    consecutive_misses = 0
    REMOVAL_THRESHOLD = 3
    
    WHILE scanning_enabled:
        // Attempt to detect tag
        result, tag = read_passive_target(150ms)
        
        IF result == success:
            consecutive_misses = 0
            tag.type = determine_tag_type(tag.sak, tag.uid_length)
            
            // Debounce logic
            current_time = get_current_time_ms()
            is_same_tag = compare_uid(tag.uid, last_uid)
            debounce_expired = (current_time - last_detection_time) > debounce_ms
            
            // Notify callback if appropriate
            IF NOT is_same_tag OR debounce_expired OR NOT tag_was_present:
                last_uid = tag.uid
                last_uid_length = tag.uid_length
                last_detection_time = current_time
                tag_was_present = true
                
                call_tag_detected_callback(tag)
            
            // Release for re-detection
            release_target()
        
        ELSE IF result == not_found OR result == timeout:
            consecutive_misses = consecutive_misses + 1
            
            // Detect tag removal
            IF tag_was_present AND consecutive_misses >= REMOVAL_THRESHOLD:
                tag_was_present = false
                call_tag_removed_callback()
        
        ELSE:
            // Communication error - log and continue
            log_warning("Communication error")
        
        // Wait before next poll
        delay(poll_interval_ms)
```

### 12.3 Debounce Logic

```
PROCEDURE should_notify_callback(tag):
    current_time = get_current_time_ms()
    
    // Check if this is a different tag
    is_different = NOT compare_uid(tag.uid, tag.uid_length, 
                                   last_uid, last_uid_length)
    
    // Check if debounce period expired
    time_since_last = current_time - last_detection_time
    debounce_expired = time_since_last > debounce_time_ms
    
    // Check if tag was removed and came back
    tag_returned = NOT tag_was_present
    
    RETURN is_different OR debounce_expired OR tag_returned
```

---

## 13. Data Structures

### 13.1 Tag Information Structure

```
STRUCTURE tag_info:
    uid[10]      : byte array   // Unique identifier (max 10 bytes)
    uid_length   : byte         // Actual UID length (4 or 7)
    sak          : byte         // Select Acknowledge
    atqa[2]      : byte array   // Answer To Request Type A
    type         : enum         // Derived tag type
```

### 13.2 Firmware Version Structure

```
STRUCTURE firmware_version:
    ic       : byte   // IC version (0x32 for PN532)
    version  : byte   // Firmware major version
    revision : byte   // Firmware minor version
    support  : byte   // Feature flags
```

### 13.3 Reader State Structure

```
STRUCTURE reader_state:
    initialized        : boolean
    scanning           : boolean
    tag_present        : boolean
    last_uid[10]       : byte array
    last_uid_length    : byte
    last_detection_time: integer (ms)
    debounce_ms        : integer
    poll_interval_ms   : integer
```

---

## 14. Debug Output (UART Serial)

### 14.1 Debug Messages

The implementation includes UART serial output for debugging:

**On Tag Detection:**
```
========================================
NFC: Tag detected!
  UID: 04:7F:A9:9A:1F:66:80 (7 bytes)
  Type: NTAG
  SAK: 0x00
  ATQA: 0x00 0x44
========================================
```

**On Tag Removal:**
```
NFC: Tag removed
```

### 14.2 Disabling Debug Output

To disable debug output for production integration:

1. Remove or comment out all `printf()` statements in callback functions
2. Remove or comment out debug logging calls
3. The NFC reader will continue to work via callbacks without serial output

The debug output is isolated in the application layer (callback functions) and does not affect driver functionality.

---

## 15. Error Handling

### 15.1 Error Types

| Error | Cause | Recovery |
|-------|-------|----------|
| I2C Timeout | No response from PN532 | Check wiring, retry with wake-up |
| Invalid ACK | Communication error | Retry command |
| Checksum Error | Data corruption | Retry command |
| No Tag Found | Normal (no tag present) | Continue polling |
| Invalid Response | Unexpected data format | Log error, retry |

### 15.2 Retry Strategy

```
PROCEDURE execute_with_retry(operation, max_retries):
    FOR attempt = 1 TO max_retries:
        result = operation()
        IF result == success:
            RETURN success
        
        IF attempt < max_retries:
            send_wakeup_sequence()
            delay(200ms)
    
    RETURN failure
```

---

## 16. Integration Guidelines

### 16.1 Minimal Integration

```
// 1. Initialize
nfc_init()

// 2. Register callback
nfc_register_callback(my_tag_handler)

// 3. Start scanning
nfc_start_scanning()

// 4. Handle events in callback
FUNCTION my_tag_handler(tag):
    process_tag_uid(tag.uid, tag.uid_length)
```

### 16.2 Shared I2C Bus

When sharing I2C bus with other devices (e.g., OLED at address 0x3C):

1. Check if I2C driver is already installed before initializing
2. If already installed, reuse the existing bus
3. Do not uninstall the I2C driver on shutdown if not installed by NFC module

### 16.3 Thread Safety

- Use mutex when accessing shared state (callbacks, configuration)
- Callback is invoked from polling task context (not interrupt)
- Safe to call most system functions from callback
- Keep callback execution short to avoid blocking detection

---

## 17. Complete Byte Sequence Examples

### 17.1 Full Initialization Sequence

```
Step 1 - Wake-up:
  TX: 55 55 00 00 00 00 00 00 00 00
  (wait 50ms)

Step 2 - GetFirmwareVersion:
  TX: 00 00 FF 02 FE D4 02 2A 00
  RX: 01 00 00 FF 00 FF 00                    (ACK)
  RX: 01 00 00 FF 06 FA D5 03 32 01 06 07 E8 00  (Response)

Step 3 - SAMConfiguration:
  TX: 00 00 FF 05 FB D4 14 01 14 01 02 00
  RX: 01 00 00 FF 00 FF 00                    (ACK)
  RX: 01 00 00 FF 02 FE D5 15 16 00           (Response)
```

### 17.2 Tag Detection Sequence

```
TX: 00 00 FF 04 FC D4 4A 01 00 E1 00          (InListPassiveTarget)
RX: 01 00 00 FF 00 FF 00                      (ACK)
RX: 01 00 00 FF 0F F1 D5 4B 01 01 00 44 00 07 
    04 7F A9 9A 1F 66 80 C8 00                (Response with 7-byte UID)

Parsed:
  Response Code: 0x4B
  Targets: 1
  ATQA: 00 44
  SAK: 0x00
  UID Length: 7
  UID: 04:7F:A9:9A:1F:66:80
  Tag Type: NTAG (SAK=0x00, 7-byte UID)
```

### 17.3 Release Sequence

```
TX: 00 00 FF 03 FD D4 52 00 DA 00             (InRelease)
RX: 01 00 00 FF 00 FF 00                      (ACK)
RX: 01 00 00 FF 03 FD D5 53 00 D8 00          (Response)
```

---

## 18. Verification Checklist

Use this checklist to verify implementation correctness:

- [ ] I2C address 0x24 configured correctly
- [ ] I2C speed is 100 kHz
- [ ] Wake-up sequence sent before first command
- [ ] LCS checksum validates for all sent frames
- [ ] DCS checksum validates for all sent frames
- [ ] ACK frame verified after each command
- [ ] Response start sequence (00 00 FF) found correctly
- [ ] Response TFI is 0xD5
- [ ] Response checksums validated
- [ ] GetFirmwareVersion returns IC = 0x32
- [ ] SAMConfiguration called before tag detection
- [ ] Tag detection timeout handled as "no tag"
- [ ] UID correctly extracted from response
- [ ] Tag type determined from SAK byte
- [ ] Release called after each successful detection
- [ ] Debounce prevents duplicate callbacks
- [ ] Tag removal detected after 3 consecutive misses
