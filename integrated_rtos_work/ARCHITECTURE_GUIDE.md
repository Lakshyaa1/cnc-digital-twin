# CNC Predictive Maintenance RTOS Architecture - Explanation & Integration Guide

## Overview

This is a **FreeRTOS-based firmware** for ESP32 (IDF v5.5) that integrates 7 sensors into a cohesive real-time system. The architecture is built on these core concepts:

- **Tasks**: Periodic or event-driven workers that run at specific priorities
- **Queues**: FIFO buffers for passing sensor data safely between tasks
- **Semaphores**: Signals that wake event-driven tasks (barcode, RFID)
- **Mutex**: Lock that protects the shared sensor data structure
- **ISRs**: Fast interrupt handlers that post to semaphores (don't do heavy work)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    HIGH PRIORITY (Vibration-Critical)           │
├─────────────────────────────────────────────────────────────────┤
│ accel_task (1kHz)           → accel_queue                       │
│ pressure_task (10Hz)        → pressure_queue                    │
│ ultrasonic_task (10Hz)      → ultrasonic_queue                  │
├─────────────────────────────────────────────────────────────────┤
│                    MEDIUM PRIORITY (Event-Driven)               │
├─────────────────────────────────────────────────────────────────┤
│ barcode_handler (ISR→sem)   → barcode_queue                     │
│ rfid_handler (ISR→sem)      → rfid_queue                        │
├─────────────────────────────────────────────────────────────────┤
│                    LOW PRIORITY (Slow Sensors)                  │
├─────────────────────────────────────────────────────────────────┤
│ housekeeping_task (1Hz)     → housekeep_queue                   │
├─────────────────────────────────────────────────────────────────┤
│                    AGGREGATOR (Data Fusion)                     │
├─────────────────────────────────────────────────────────────────┤
│ All queues ──→ aggregator_task ──→ [shared_sensor_data]        │
│                   (mutex protected)                             │
├─────────────────────────────────────────────────────────────────┤
│                    ANALYTICS (Predictive Maintenance)           │
├─────────────────────────────────────────────────────────────────┤
│ shared_sensor_data ──→ analytics_task ──→ anomaly detection    │
└─────────────────────────────────────────────────────────────────┘
```

---

## Pin Layout (ESP32)

### SPI Devices
| Device      | MOSI | MISO | CLK  | CS  | SPI Host |
|-------------|------|------|------|-----|----------|
| KX134 Accel | 23   | 19   | 18   | 5   | SPI2     |
| RC522 RFID  | 11   | 9    | 10   | 8   | SPI3     |

### I2C Devices
| Device      | SDA | SCL | Port   |
|-------------|-----|-----|--------|
| BMP280      | 21  | 22  | I2C_0  |

### ADC / GPIO
| Device              | Pin  | Type    |
|---------------------|------|---------|
| Analog Pressure     | 34   | ADC1_CH6|
| Water Sensor        | 4    | GPIO_IN |
| Ultrasonic Trigger  | 12   | GPIO_OUT|
| Ultrasonic Echo     | 13   | RMT_RX  |

### UART
| Device         | TX  | RX  | Port     | Baud |
|----------------|-----|-----|----------|------|
| Barcode Reader | 17  | 16  | UART_NUM_2 | 9600 |

**Note**: Ensure no pin conflicts. If you need different pins, update the `#define` statements in each task file.

---

## Task Priorities & Rates

| Task                | Priority | Rate      | Reason |
|---------------------|----------|-----------|--------|
| `accel_task`        | Highest  | 1 kHz     | Vibration FFT needs high sample rate |
| `pressure_task`     | High     | 10 Hz     | Safety-critical, triggers anomalies |
| `ultrasonic_task`   | High     | 10 Hz     | Collision detection |
| `barcode_handler`   | Medium   | Event     | User-facing, low latency needed |
| `rfid_handler`      | Medium   | Event     | User-facing, low latency needed |
| `housekeeping_task` | Low      | 1 Hz      | Slow sensors, housekeeping |
| `aggregator_task`   | Low      | 10 Hz     | Data fusion, not time-critical |
| `analytics_task`    | Low      | 2 Hz      | Anomaly detection, runs offline |

### Priority Numbering

```c
configMAX_PRIORITIES = typically 25 on ESP32

ACCEL_TASK_PRIORITY         = 24  (configMAX_PRIORITIES - 1)
PRESSURE_TASK_PRIORITY      = 23  (configMAX_PRIORITIES - 2)
ULTRASONIC_TASK_PRIORITY    = 23  (configMAX_PRIORITIES - 2)
BARCODE_HANDLER_PRIORITY    = 22  (configMAX_PRIORITIES - 3)
RFID_HANDLER_PRIORITY       = 22  (configMAX_PRIORITIES - 3)
HOUSEKEEPING_TASK_PRIORITY  = 21  (configMAX_PRIORITIES - 4)
AGGREGATOR_TASK_PRIORITY    = 21  (configMAX_PRIORITIES - 4)
ANALYTICS_TASK_PRIORITY     = 21  (configMAX_PRIORITIES - 4)
```

Higher number = higher priority. Accel always preempts everything except currently-running higher-priority task.

---

## Data Flow

### Periodic Sensors (accel, pressure, ultrasonic, housekeeping)

```
Sensor Hardware
    ↓
Read in Task (periodic vTaskDelayUntil)
    ↓
Create sample struct (timestamp, values)
    ↓
xQueueSend() to queue (non-blocking)
    ↓
Queue buffers samples (up to N samples)
    ↓
aggregator_task drains queue every 100ms
    ↓
Update shared_sensor_data (protected by mutex)
```

**Example: Accel Task**
```c
void accel_task(void *pvParameters) {
    while (1) {
        // Wake every 1ms (1kHz)
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1));
        
        // Read sensor
        accel_sample_t sample;
        kx134_read_accel(&sample.x, &sample.y, &sample.z);
        sample.timestamp_ms = get_timestamp_ms();
        
        // Post to queue (non-blocking)
        xQueueSend(accel_queue, &sample, 0);
    }
}
```

### Event-Driven Sensors (barcode, RFID)

```
Hardware Event (UART byte, card detected)
    ↓
ISR fires (minimal work)
    ↓
ISR posts to semaphore (xSemaphoreGiveFromISR)
    ↓
Handler task wakes from xSemaphoreTake()
    ↓
Handler does heavy work (lookup, auth, FFT, etc.)
    ↓
Update shared_sensor_data (mutex protected)
    ↓
Loop back to sleep on semaphore
```

**Example: Barcode Flow**
```
User scans barcode → UART byte arrives
    ↓
UART ISR reads byte, stores in buffer
    ↓
Last byte is '\n' → ISR gives barcode_sem
    ↓
barcode_handler_task wakes from xSemaphoreTake()
    ↓
Handler calls barcode_lookup_stl_file() (slow, no ISR!)
    ↓
Updates shared_sensor_data.last_barcode (mutex)
    ↓
Loop back to sleep on semaphore
```

---

## Shared Data Structure

All tasks can **read** this safely (with mutex):

```c
typedef struct {
    // Latest readings
    accel_sample_t last_accel;
    bmp280_sample_t last_bmp280;
    analog_pressure_sample_t last_analog_pressure;
    ultrasonic_sample_t last_ultrasonic;
    water_level_sample_t last_water_level;
    barcode_sample_t last_barcode;
    rfid_sample_t last_rfid;
    
    // Anomaly flags
    uint8_t accel_anomaly;        // High vibration
    uint8_t pressure_anomaly;     // Out of range
    uint8_t temp_anomaly;         // Out of range
    uint8_t user_authenticated;   // RFID verified
    
    uint64_t last_update_ms;
} sensor_data_t;
```

**Reading** (in analytics_task or future WiFi layer):
```c
sensor_data_lock();
{
    float pressure = shared_sensor_data.last_bmp280.pressure_pa;
    float temp = shared_sensor_data.last_bmp280.temperature_c;
    uint8_t anomaly = shared_sensor_data.accel_anomaly;
}
sensor_data_unlock();
```

**Writing** (only aggregator or event handlers):
```c
sensor_data_lock();
{
    shared_sensor_data.last_accel = accel_copy;
    shared_sensor_data.accel_anomaly = (magnitude > 5000) ? 1 : 0;
}
sensor_data_unlock();
```

---

## How to Integrate Your Sensor Code

Each sensor task has the same structure:

```c
void sensor_task(void *pvParameters) {
    // 1. Initialize hardware (I2C, SPI, ADC, GPIO, etc.)
    sensor_init();
    
    // 2. Setup timing
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(10);  // Your sampling rate
    
    // 3. Main loop
    while (1) {
        // Wait for next period
        vTaskDelayUntil(&last_wake_time, period);
        
        // Read sensor
        sample_t sample;
        sensor_read(&sample);
        sample.timestamp_ms = get_timestamp_ms();
        
        // Post to queue (non-blocking)
        xQueueSend(sensor_queue, &sample, 0);
    }
}
```

### Steps to Integrate Your Code

1. **Copy your sensor driver functions** into the task file (e.g., `accel_task.c`)
2. **Replace `sensor_init()`, `sensor_read()` calls** with your function names
3. **Update pin definitions** at the top of the file
4. **Adjust sampling rate** (`pdMS_TO_TICKS(1)` for 1kHz, `pdMS_TO_TICKS(100)` for 10Hz, etc.)
5. **Populate the sample struct** with your sensor's data
6. **Adjust queue sizes** in `cnc_firmware_rtos.h` if needed

### Example: Integrating Your KX134 Code

**Your original code** (polling mode):
```c
void read_kx134() {
    int16_t x, y, z;
    // ... your SPI read code ...
    printf("X: %d, Y: %d, Z: %d\n", x, y, z);
}
```

**Integrated into accel_task.c**:
```c
// Replace the stub functions with your code:
static void kx134_init(void) {
    // Copy your initialization code here
    // (bus init, device config, etc.)
}

static void kx134_read_accel(int16_t *x, int16_t *y, int16_t *z) {
    // Copy your read logic here
}

// Task remains the same:
void accel_task(void *pvParameters) {
    kx134_init();  // Calls your init
    while (1) {
        vTaskDelayUntil(&last_wake_time, period);
        kx134_read_accel(&sample.x, &sample.y, &sample.z);
        // ... rest of task ...
    }
}
```

---

## Mutex (Mutual Exclusion) - Simple Explanation

**Problem**: If task A and task B both try to read/write `shared_sensor_data` at the same time, data corruption happens.

**Solution**: Lock it with a mutex.

```c
// Before accessing shared_sensor_data:
sensor_data_lock();    // Acquire lock (xMutexTake)
{
    // Safe zone: only this task can access shared_sensor_data
    float temp = shared_sensor_data.last_bmp280.temperature_c;
}
sensor_data_unlock();  // Release lock (xMutexGive)
```

If another task tries to lock while you hold it, it **blocks** until you release.

---

## Queue (FIFO Buffer) - Simple Explanation

**Purpose**: Pass data safely between tasks without locks.

```c
// Task A (producer): Post sample
accel_sample_t sample = {100, 200, 300, 12345};
xQueueSend(accel_queue, &sample, 0);  // 0 = non-blocking

// Task B (consumer): Retrieve sample
accel_sample_t received;
if (xQueueReceive(accel_queue, &received, 100)) {
    printf("Got sample: x=%d, y=%d, z=%d\n", received.x, received.y, received.z);
}
```

Queue handles all thread-safety automatically.

---

## Semaphore (Event Signal) - Simple Explanation

**Purpose**: Wake a sleeping task when an event happens.

```c
// Task: Sleep until event
void barcode_handler_task(void *pvParameters) {
    while (1) {
        xSemaphoreTake(barcode_sem, portMAX_DELAY);  // Sleep here
        // Wakes up when ISR gives the semaphore
        process_barcode();
    }
}

// ISR: Signal the event
void UART_ISR() {
    char byte = read_uart();
    // ... buffer it ...
    if (barcode_complete) {
        xSemaphoreGiveFromISR(barcode_sem, NULL);  // Wake the task
    }
}
```

No busy-polling, no wasted CPU. The task sleeps until needed.

---

## ISR (Interrupt Service Routine) - Keep It Fast

**Golden Rule**: ISR should do **minimal work**, only **signal** events, then return immediately.

### Bad ISR (Slow, blocks everything):
```c
void UART_ISR() {
    char byte = read_uart();
    authenticate_user(byte);      // ❌ Heavy work in ISR
    lookup_database(byte);        // ❌ Locks up CPU for 50ms
    // Everything else is blocked!
}
```

### Good ISR (Fast, event-driven):
```c
void UART_ISR() {
    char byte = read_uart();      // Quick read (1 µs)
    buffer[idx++] = byte;         // Quick store (1 µs)
    
    if (byte == '\n') {
        xSemaphoreGiveFromISR(barcode_sem, NULL);  // Signal task (1 µs)
    }
    // Total: ~3 µs, other tasks can run
}
```

The handler task does the heavy work **outside** the ISR.

---

## Testing & Debugging

### 1. Check Log Output
```
I (1234) MAIN: ===== CNC PREDICTIVE MAINTENANCE FIRMWARE =====
I (1245) RTOS: Initializing RTOS architecture...
I (1256) RTOS: ✓ Queues created
I (1267) RTOS: ✓ Semaphores created
I (1278) RTOS: ✓ Mutex created
I (1289) RTOS: ✓ accel_task created (priority 24)
...
```

### 2. Monitor Individual Sensor Logs
```
D (1500) ACCEL: accel: x=120 y=45 z=9800 (sample #1000)
D (1600) PRESSURE: pressure: bmp280_p=101325.00 Pa, temp=25.50 C, analog_p=5.20 bar
D (1700) ULTRASONIC: ultrasonic: distance=45.23 cm
D (1800) HOUSEKEEPING: housekeeping: water=EMPTY
```

### 3. Test Event-Driven Tasks
```
// Scan a barcode
I (5000) BARCODE: Barcode scan #1: PART_001 → STL file ID: 1

// Place RFID card
I (6000) RFID: RFID scan #1: UID=04 12 AB CD Auth=PASS
```

### 4. Monitor Anomalies
```
W (10000) ANALYTICS: HIGH VIBRATION ALERT: 5.23 g (threshold: 5.00 g)
W (10500) ANALYTICS: HIGH PRESSURE ALERT: 125.3 kPa (max: 120.0 kPa)
E (11000) ANALYTICS: COLLISION WARNING: 15.2 cm (threshold: 20.0 cm)
```

---

## Compilation (ESP-IDF v5.5)

```bash
# Build
idf.py build

# Flash
idf.py flash

# Monitor
idf.py monitor
```

---

## Next Steps (Future Phases)

1. **WiFi Integration**: Add WiFi task to push aggregated data to server
2. **Storage**: Log anomalies to SPIFFS filesystem
3. **Machine Learning**: Train model on historical vibration data to predict bearing failure
4. **Web Dashboard**: Real-time analytics dashboard
5. **Emergency Stop**: Trigger hardware relay on collision/anomaly

---

## Summary

- **8 tasks**, **6 queues**, **2 semaphores**, **1 mutex**
- **Data flows** from sensors → queues → aggregator → shared struct → analytics
- **Priorities** ensure critical sensors (accel) never miss samples
- **ISRs** signal events without blocking CPU
- **Mutex** protects shared data from corruption
- **Easy to extend**: Add new sensor = add new task + new queue

Good luck! 🚀
