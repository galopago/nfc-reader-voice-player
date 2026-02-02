# Quadrature Rotary Encoder Driver - Technical Specification

## Document Purpose

This document provides a complete technical specification for implementing a quadrature rotary encoder driver on any embedded system. It is designed to be:

- **Platform-agnostic**: Applicable to any microcontroller (ARM, RISC-V, AVR, x86, etc.)
- **Language-agnostic**: Implementable in C, C++, Rust, MicroPython, or any language with hardware access
- **Self-contained**: No external code references required

An LLM or developer following this specification should produce a functionally equivalent driver regardless of the target platform.

---

## 1. Quadrature Encoding Theory

### 1.1 Physical Principle

A quadrature rotary encoder contains two internal switches (or optical sensors) that produce two digital signals, conventionally named **Phase A** and **Phase B**. These signals are offset by 90 electrical degrees (one quarter of a cycle).

```
Clockwise Rotation (CW):

Phase A:  ████    ████    ████
              ████    ████    ████
Phase B:    ████    ████    ████
                ████    ████    ████
          ─────────────────────────────► Time

Counter-Clockwise Rotation (CCW):

Phase B:  ████    ████    ████
              ████    ████    ████
Phase A:    ████    ████    ████
                ████    ████    ████
          ─────────────────────────────► Time
```

### 1.2 State Encoding

The two phases create a 2-bit state value:

```
State = (Phase_A << 1) | Phase_B
```

This produces four possible states:

| State | Binary | Phase A | Phase B |
|-------|--------|---------|---------|
| 0     | 00     | LOW     | LOW     |
| 1     | 01     | LOW     | HIGH    |
| 2     | 10     | HIGH    | LOW     |
| 3     | 11     | HIGH    | HIGH    |

### 1.3 State Transition Sequences

**Clockwise (CW) rotation** produces this state sequence:
```
0 → 2 → 3 → 1 → 0 → 2 → 3 → 1 → ...
(00 → 10 → 11 → 01 → 00 → ...)
```

**Counter-Clockwise (CCW) rotation** produces the reverse:
```
0 → 1 → 3 → 2 → 0 → 1 → 3 → 2 → ...
(00 → 01 → 11 → 10 → 00 → ...)
```

### 1.4 Gray Code Property

Notice that in valid transitions, **only one bit changes at a time**. This is a Gray code property that helps distinguish valid transitions from noise or missed states.

```
Valid transitions (1 bit change):
  00 ↔ 01  (bit 0 changes)
  00 ↔ 10  (bit 1 changes)
  01 ↔ 11  (bit 1 changes)
  10 ↔ 11  (bit 0 changes)

Invalid transitions (2 bits change):
  00 ↔ 11  (both bits change - skipped state)
  01 ↔ 10  (both bits change - skipped state)
```

---

## 2. State Transition Lookup Table

### 2.1 Table Design

Instead of using conditional logic (if/else chains), use a **16-entry lookup table** for O(1) direction detection.

**Index calculation:**
```
index = (previous_state << 2) | current_state
```

This maps all 16 possible transitions (4 previous states × 4 current states) to a single array index.

### 2.2 Complete Transition Table

```
Index | Prev | Curr | Transition | Direction | Value
------|------|------|------------|-----------|------
  0   |  00  |  00  | No change  | None      |   0
  1   |  00  |  01  | Valid      | CCW       |  -1
  2   |  00  |  10  | Valid      | CW        |  +1
  3   |  00  |  11  | Invalid    | None      |   0
  4   |  01  |  00  | Valid      | CW        |  +1
  5   |  01  |  01  | No change  | None      |   0
  6   |  01  |  10  | Invalid    | None      |   0
  7   |  01  |  11  | Valid      | CCW       |  -1
  8   |  10  |  00  | Valid      | CCW       |  -1
  9   |  10  |  01  | Invalid    | None      |   0
 10   |  10  |  10  | No change  | None      |   0
 11   |  10  |  11  | Valid      | CW        |  +1
 12   |  11  |  00  | Invalid    | None      |   0
 13   |  11  |  01  | Valid      | CW        |  +1
 14   |  11  |  10  | Valid      | CCW       |  -1
 15   |  11  |  11  | No change  | None      |   0
```

### 2.3 Table as Array

```
QUADRATURE_TABLE[16] = {
     0,    // Index 0:  00→00 no change
    -1,    // Index 1:  00→01 CCW
    +1,    // Index 2:  00→10 CW
     0,    // Index 3:  00→11 invalid
    +1,    // Index 4:  01→00 CW
     0,    // Index 5:  01→01 no change
     0,    // Index 6:  01→10 invalid
    -1,    // Index 7:  01→11 CCW
    -1,    // Index 8:  10→00 CCW
     0,    // Index 9:  10→01 invalid
     0,    // Index 10: 10→10 no change
    +1,    // Index 11: 10→11 CW
     0,    // Index 12: 11→00 invalid
    +1,    // Index 13: 11→01 CW
    -1,    // Index 14: 11→10 CCW
     0     // Index 15: 11→11 no change
}
```

### 2.4 State Diagram

```
        ┌──────────────────────────────────┐
        │                                  │
        ▼            CW (+1)               │
    ┌───────┐    ─────────────►    ┌───────┐
    │       │                      │       │
    │  00   │                      │  10   │
    │       │    ◄─────────────    │       │
    └───────┘        CCW (-1)      └───────┘
        │                              │
        │                              │
   CCW  │                              │  CW
   (-1) │                              │ (+1)
        │                              │
        ▼                              ▼
    ┌───────┐        CCW (-1)      ┌───────┐
    │       │    ◄─────────────    │       │
    │  01   │                      │  11   │
    │       │    ─────────────►    │       │
    └───────┘        CW (+1)       └───────┘
        │                              ▲
        │                              │
        └──────────────────────────────┘
```

---

## 3. Software Architecture

### 3.1 Recommended Pattern: ISR + Queue + Task

For real-time embedded systems with an RTOS, use this architecture:

```
┌─────────────────┐
│   GPIO Pins     │
│   (A and B)     │
└────────┬────────┘
         │ Hardware interrupt
         ▼
┌─────────────────┐
│      ISR        │  ← Runs in interrupt context
│  (very fast)    │  ← Must complete in microseconds
│                 │  ← Cannot call blocking functions
└────────┬────────┘
         │ Send direction (+1/-1) to queue
         ▼
┌─────────────────┐
│   Event Queue   │  ← Thread-safe FIFO buffer
│  (10+ elements) │  ← Decouples ISR from processing
└────────┬────────┘
         │ Task receives from queue
         ▼
┌─────────────────┐
│  Encoder Task   │  ← Runs in normal task context
│                 │  ← Can call any function
│                 │  ← Updates counter
│                 │  ← Invokes user callback
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  User Callback  │  ← Application-specific handler
└─────────────────┘
```

### 3.2 Why This Pattern?

1. **ISR must be minimal**: Interrupt handlers block all other interrupts. Long ISRs cause system instability.

2. **Callbacks need safe context**: User callbacks may call logging functions, update displays, or perform I/O—all forbidden in ISR context.

3. **Queue absorbs bursts**: Rapid encoder rotation generates many events. The queue prevents event loss during processing delays.

### 3.3 Alternative: Polling Architecture

For systems without RTOS or without interrupt support:

```
main_loop:
    current_state = read_gpio_pins()
    if current_state != previous_state:
        index = (previous_state << 2) | current_state
        direction = QUADRATURE_TABLE[index]
        if direction != 0:
            counter += direction
            invoke_callback(direction)
        previous_state = current_state
    sleep_microseconds(100)  // Poll rate: 10 kHz
    goto main_loop
```

**Trade-offs:**
- Simpler implementation
- May miss events at high rotation speeds
- Consumes CPU cycles continuously

---

## 4. Debounce Strategy

### 4.1 Why Debounce is Necessary

Mechanical encoders contain physical contacts that "bounce" when transitioning between states. A single physical transition may generate multiple electrical transitions over a period of 1-10 milliseconds.

```
Ideal signal:      ████████████________________
Actual signal:     ████████▄▄██▄███____________
                            ↑↑↑↑↑
                         Bounce noise
```

Without debounce, each bounce triggers an interrupt, causing:
- Incorrect direction detection
- Counter oscillation
- Excessive CPU load

### 4.2 Time-Based Debounce

Implement debounce by ignoring interrupts that occur within a minimum time window after the last valid interrupt.

**Recommended values:**
- Manual rotation (slow): 5-10 ms
- Motor-driven (fast): 1-3 ms
- Optical encoders: 0 ms (no mechanical bounce)

**Algorithm:**
```
ISR_handler:
    current_time = get_system_time_microseconds()
    time_since_last = current_time - last_interrupt_time
    
    if time_since_last < DEBOUNCE_THRESHOLD:
        return  // Ignore this interrupt
    
    last_interrupt_time = current_time
    // ... proceed with state processing
```

### 4.3 Debounce Trade-offs

| Debounce Time | Noise Rejection | Max Rotation Speed |
|---------------|-----------------|-------------------|
| 1 ms          | Low             | ~250 pulses/sec   |
| 5 ms          | Medium          | ~50 pulses/sec    |
| 10 ms         | High            | ~25 pulses/sec    |

For a 2-pulse-per-revolution encoder rotated by hand, 5 ms debounce is appropriate.

---

## 5. Hardware Interface

### 5.1 GPIO Configuration

Each encoder phase requires one GPIO pin configured as:

| Parameter        | Setting                          |
|------------------|----------------------------------|
| Direction        | Input                            |
| Pull resistor    | Pull-up enabled (internal or external) |
| Interrupt trigger| Both edges (rising AND falling)  |

### 5.2 Pull-up Resistors

Encoders typically have open-drain outputs that connect to ground when active. Pull-up resistors are required to define the HIGH state.

**Options:**
1. **Internal pull-up** (10-50 kΩ typical): Convenient, no external components
2. **External pull-up** (4.7-10 kΩ): Better noise immunity, recommended for long wires

### 5.3 Electrical Schematic

```
    VCC (3.3V or 5V)
     │
     ├──[10kΩ]──┬── GPIO_A ◄── Encoder Phase A
     │          │
     │         ─┴─ (internal pull-up alternative)
     │
     ├──[10kΩ]──┬── GPIO_B ◄── Encoder Phase B
     │          │
     │         ─┴─
     │
    GND ◄─────────── Encoder Common
```

### 5.4 Noise Filtering (Optional)

For noisy environments or long cable runs, add hardware filtering:

```
    GPIO_A ───[100Ω]───┬─── To MCU
                       │
                    [100nF]
                       │
                      GND
```

This RC filter has a cutoff frequency of ~16 kHz, filtering high-frequency noise while passing encoder signals.

---

## 6. API Design

### 6.1 Recommended Public Interface

```
// Direction enumeration
enum Direction {
    DIRECTION_CW,      // Clockwise (up)
    DIRECTION_CCW      // Counter-clockwise (down)
}

// Callback function signature
type Callback = function(direction: Direction) -> void

// Initialize encoder driver
// Configures GPIO pins, installs interrupt handlers, starts processing task
// Returns: success or error code
function encoder_init() -> Result

// Register callback for rotation events
// Only one callback supported; subsequent calls replace previous
// Pass null/none to disable callbacks
function encoder_register_callback(callback: Callback) -> void

// Get accumulated position counter
// Increments on CW, decrements on CCW
// Thread-safe: can be called from any context
function encoder_get_count() -> int32

// Reset position counter to zero
// Thread-safe
function encoder_reset_count() -> void
```

### 6.2 Usage Example

```
function on_rotation(direction):
    if direction == DIRECTION_CW:
        print("U")  // Up
    else:
        print("D")  // Down

function main():
    encoder_init()
    encoder_register_callback(on_rotation)
    
    // Application continues...
    while true:
        sleep(1000)
```

### 6.3 Thread Safety Considerations

1. **Counter variable**: Must be declared `volatile` (or equivalent) to prevent compiler optimization issues across ISR/task boundaries.

2. **Callback pointer**: Updates should be atomic or protected by disabling interrupts briefly.

3. **Queue operations**: Use platform-provided thread-safe queue primitives.

---

## 7. Pseudocode Reference Implementation

### 7.1 Global State

```
// Lookup table (constant, read-only)
const QUADRATURE_TABLE[16] = {0, -1, +1, 0, +1, 0, 0, -1, -1, 0, 0, +1, 0, +1, -1, 0}

// Mutable state (must be volatile for ISR access)
volatile previous_state: uint8 = 0
volatile last_interrupt_time: int64 = 0
volatile position_counter: int32 = 0

// Callback storage
user_callback: Callback = null

// Inter-task communication
event_queue: Queue<int8> = new Queue(capacity=10)

// Configuration
const DEBOUNCE_US = 5000  // 5 milliseconds
const GPIO_PHASE_A = <platform-specific>
const GPIO_PHASE_B = <platform-specific>
```

### 7.2 Initialization Sequence

```
function encoder_init():
    // 1. Create event queue
    event_queue = create_queue(capacity=10, element_size=1)
    if event_queue == null:
        return ERROR_NO_MEMORY
    
    // 2. Configure GPIO pins
    configure_gpio(GPIO_PHASE_A, mode=INPUT, pull=PULL_UP, interrupt=BOTH_EDGES)
    configure_gpio(GPIO_PHASE_B, mode=INPUT, pull=PULL_UP, interrupt=BOTH_EDGES)
    
    // 3. Read initial state
    a = read_gpio(GPIO_PHASE_A)
    b = read_gpio(GPIO_PHASE_B)
    previous_state = (a << 1) | b
    
    // 4. Install interrupt handlers
    install_isr(GPIO_PHASE_A, encoder_isr)
    install_isr(GPIO_PHASE_B, encoder_isr)
    
    // 5. Start processing task
    create_task(encoder_task, stack_size=2048, priority=HIGH)
    
    return SUCCESS
```

### 7.3 Interrupt Service Routine

```
function encoder_isr():  // Runs in interrupt context
    // Debounce check
    now = get_time_microseconds()
    if (now - last_interrupt_time) < DEBOUNCE_US:
        return  // Too soon, ignore
    last_interrupt_time = now
    
    // Read current state
    a = read_gpio(GPIO_PHASE_A)
    b = read_gpio(GPIO_PHASE_B)
    current_state = (a << 1) | b
    
    // Lookup direction
    index = (previous_state << 2) | current_state
    direction = QUADRATURE_TABLE[index]
    
    // Update state
    previous_state = current_state
    
    // If valid transition, send to queue
    if direction != 0:
        queue_send_from_isr(event_queue, direction)
```

### 7.4 Processing Task

```
function encoder_task():
    loop forever:
        // Block until event available
        direction = queue_receive(event_queue, timeout=INFINITE)
        
        // Update counter
        position_counter = position_counter + direction
        
        // Invoke callback if registered
        if user_callback != null:
            if direction > 0:
                user_callback(DIRECTION_CW)
            else:
                user_callback(DIRECTION_CCW)
```

### 7.5 Public API Functions

```
function encoder_register_callback(callback):
    user_callback = callback

function encoder_get_count():
    return position_counter

function encoder_reset_count():
    position_counter = 0
```

---

## 8. Testing and Validation

### 8.1 Unit Tests

| Test Case | Input Sequence | Expected Output |
|-----------|----------------|-----------------|
| CW single step | 00 → 10 | direction = +1, counter = 1 |
| CCW single step | 00 → 01 | direction = -1, counter = -1 |
| CW full cycle | 00 → 10 → 11 → 01 → 00 | 4 callbacks (all CW), counter = 4 |
| CCW full cycle | 00 → 01 → 11 → 10 → 00 | 4 callbacks (all CCW), counter = -4 |
| No change | 00 → 00 | No callback, counter unchanged |
| Invalid skip | 00 → 11 | No callback, counter unchanged |
| Direction reversal | 00 → 10 → 00 | CW then CCW, counter = 0 |

### 8.2 Integration Tests

1. **Manual rotation test**: Slowly rotate encoder one full revolution. Verify correct pulse count (should equal encoder specification, e.g., 2 pulses for 2-PPR encoder).

2. **Direction test**: Rotate CW, verify all callbacks report CW. Rotate CCW, verify all callbacks report CCW.

3. **Rapid rotation test**: Spin encoder quickly. Verify no crashes, no counter corruption, callbacks execute without blocking.

4. **Bounce stress test**: Vibrate or tap encoder without rotating. Verify no false counts.

### 8.3 Edge Cases to Verify

- System startup with encoder at any position (not just state 00)
- Callback registration/unregistration during operation
- Queue overflow under rapid rotation (should discard oldest events)
- Multiple encoders on same system (if supported)

---

## 9. Platform-Specific Notes

### 9.1 ESP32 / ESP-IDF

- Use `gpio_config()` with `GPIO_INTR_ANYEDGE`
- Use `gpio_install_isr_service()` + `gpio_isr_handler_add()`
- ISR functions must have `IRAM_ATTR` attribute
- Use `xQueueSendFromISR()` for queue operations in ISR
- Use `esp_timer_get_time()` for microsecond timestamps

### 9.2 Arduino

- Use `attachInterrupt()` with `CHANGE` mode
- Use `volatile` for shared variables
- Consider `millis()` for debounce (millisecond resolution)
- No built-in queue; use simple flag or ring buffer

### 9.3 Raspberry Pi Pico

- Use `gpio_set_irq_enabled_with_callback()`
- Use `time_us_64()` for microsecond timestamps
- Use FreeRTOS queues or Pico SDK multicore FIFO

### 9.4 STM32 / HAL

- Use `HAL_GPIO_Init()` with `GPIO_MODE_IT_RISING_FALLING`
- Implement callback in `HAL_GPIO_EXTI_Callback()`
- Use `HAL_GetTick()` or DWT cycle counter for timing
- Use CMSIS-RTOS queues if using RTOS

### 9.5 Linux / Embedded Linux

- Use GPIO sysfs or libgpiod for interrupt-driven GPIO
- Use `poll()` or `epoll()` on GPIO value files
- Use `clock_gettime(CLOCK_MONOTONIC)` for timing
- Use pthreads or eventfd for producer-consumer pattern

---

## 10. Debug Output via UART

### 10.1 Purpose

During development and hardware validation, it is essential to verify that the encoder driver is working correctly before integrating it into the larger application. A simple debug mechanism using UART serial output provides immediate visual feedback.

### 10.2 Debug Strategy

Implement a minimal debug callback that outputs single characters to the serial console:

- **"U"** (Up) - Printed when clockwise rotation is detected
- **"D"** (Down) - Printed when counter-clockwise rotation is detected

Each character is followed by a newline for easy reading in a serial terminal.

### 10.3 Implementation Pattern

```
// Debug callback - outputs single character per rotation event
function debug_rotation_callback(direction):
    if direction == DIRECTION_CW:
        serial_print("U\n")
    else:
        serial_print("D\n")

// Register debug callback during development
function main():
    encoder_init()
    encoder_register_callback(debug_rotation_callback)
    // ...
```

### 10.4 Serial Configuration

| Parameter | Recommended Value |
|-----------|-------------------|
| Baud rate | 115200 bps |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |

### 10.5 Expected Output

When rotating the encoder, the serial terminal should display:

```
U
U
U
D
D
U
```

This output confirms:
1. Interrupts are firing correctly
2. Direction detection is working
3. The callback mechanism is functional
4. The task/queue pipeline is operating

### 10.6 Disabling Debug Output for Production

The debug callback should be **removed or commented out** in production code to:

- Reduce CPU overhead (serial I/O is slow)
- Eliminate unnecessary serial traffic
- Allow the UART peripheral to be used for other purposes

**Option A: Conditional compilation**
```
#ifdef DEBUG_ENCODER
    encoder_register_callback(debug_rotation_callback)
#else
    encoder_register_callback(production_callback)  // Or null for no callback
#endif
```

**Option B: Comment out registration**
```
function main():
    encoder_init()
    // DEBUG: Uncomment the following line to enable serial debug output
    // encoder_register_callback(debug_rotation_callback)
    
    // Production callback
    encoder_register_callback(menu_navigation_callback)
```

**Option C: Null callback**
```
// To disable all callbacks:
encoder_register_callback(null)
```

### 10.7 Alternative Debug Methods

| Method | Pros | Cons |
|--------|------|------|
| UART character output | Simple, real-time | Requires serial connection |
| LED toggle | No external tools needed | Limited information |
| Counter display on LCD/OLED | Visual feedback | Requires display driver |
| GPIO pin toggle + oscilloscope | Precise timing analysis | Requires test equipment |
| Log to flash memory | Post-mortem analysis | Delayed feedback |

### 10.8 Debug Callback Best Practices

1. **Keep it minimal**: Only print essential information in the callback
2. **Avoid blocking**: Use non-blocking print functions if available
3. **Buffer overflow**: Be aware of serial buffer limits during rapid rotation
4. **Timestamps optional**: Add timestamps only if debugging timing issues

```
// Extended debug with timestamp (optional)
function verbose_debug_callback(direction):
    timestamp = get_time_milliseconds()
    dir_char = (direction == DIRECTION_CW) ? "U" : "D"
    serial_printf("[%lu] %s count=%d\n", timestamp, dir_char, encoder_get_count())
```

---

## 11. Glossary

| Term | Definition |
|------|------------|
| CW | Clockwise rotation direction |
| CCW | Counter-clockwise rotation direction |
| ISR | Interrupt Service Routine - code executed in response to hardware interrupt |
| PPR | Pulses Per Revolution - encoder resolution specification |
| Quadrature | Encoding scheme using two 90°-offset signals |
| Gray code | Binary encoding where adjacent values differ by one bit |
| Debounce | Filtering technique to ignore electrical noise from mechanical contacts |
| Pull-up | Resistor connecting signal line to positive voltage |
| RTOS | Real-Time Operating System |
| UART | Universal Asynchronous Receiver/Transmitter - serial communication interface |
| Baud rate | Data transmission speed in symbols per second |

---

## Document Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-02-02 | Initial specification |
| 1.1 | 2026-02-02 | Added Section 10: Debug Output via UART |
