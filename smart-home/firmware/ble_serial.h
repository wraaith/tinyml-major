/*
* BLE Serial Module
* Nordic UART Service (NUS) implementation using ArduinoBLE
*
* Provides a Serial-like interface over BLE for the Smart Home Controller.
* Uses standard Nordic UART UUIDs for compatibility with mobile apps.
*
* FIX: print() no longer blocks the main loop with delay(10) per 20-byte
* chunk. Bytes are queued into a TX ring buffer and drained asynchronously
* by update(), which must be called every loop() iteration.
*/

#ifndef BLE_SERIAL_H
#define BLE_SERIAL_H

#include <ArduinoBLE.h>

// ============================================================================
// NORDIC UART SERVICE UUIDs
// ============================================================================

#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHARACTERISTIC "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // Write (phone -> board)
#define NUS_TX_CHARACTERISTIC "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // Notify (board -> phone)

// ============================================================================
// BLE SERIAL CLASS
// ============================================================================

class BLESerial {
private:
  BLEService uartService;
  BLECharacteristic txCharacteristic;
  BLECharacteristic rxCharacteristic;

  // Receive buffer (unchanged)
  static const int RX_BUFFER_SIZE = 256;
  uint8_t rxBuffer[RX_BUFFER_SIZE];
  volatile int rxHead;
  volatile int rxTail;

  // TX ring buffer (non-blocking) - queueing and draining both happen in
  // loop() context, so plain int is fine here (no ISR preemption).
  static const int TX_BUFFER_SIZE = 512;
  uint8_t txBuffer[TX_BUFFER_SIZE];
  int txHead;
  int txTail;
  uint32_t droppedBytes;
  unsigned long lastTxTime;
  static const unsigned long CHUNK_INTERVAL_MS = 10;

  static BLESerial* instance;

  static void onRxWritten(BLEDevice central, BLECharacteristic characteristic) {
    if (instance) {
      const uint8_t* data = characteristic.value();
      int len = characteristic.valueLength();

      for (int i = 0; i < len; i++) {
        int nextHead = (instance->rxHead + 1) % RX_BUFFER_SIZE;
        if (nextHead != instance->rxTail) {
          instance->rxBuffer[instance->rxHead] = data[i];
          instance->rxHead = nextHead;
        }
      }
    }
  }

  // Enqueue a single byte into the TX ring buffer
  size_t enqueue(uint8_t b) {
    int nextHead = (txHead + 1) % TX_BUFFER_SIZE;
    if (nextHead != txTail) {
      txBuffer[txHead] = b;
      txHead = nextHead;
      return 1;
    }
    droppedBytes++;
    return 0;
  }

public:
  BLESerial() :
    uartService(NUS_SERVICE_UUID),
    txCharacteristic(NUS_TX_CHARACTERISTIC, BLENotify, 20),
    rxCharacteristic(NUS_RX_CHARACTERISTIC, BLEWrite, 20),
    rxHead(0), rxTail(0),
    txHead(0), txTail(0),
    droppedBytes(0), lastTxTime(0) {
    instance = this;
  }

  // Initialize the BLE UART service
  void begin() {
    uartService.addCharacteristic(txCharacteristic);
    uartService.addCharacteristic(rxCharacteristic);

    BLE.addService(uartService);

    rxCharacteristic.setEventHandler(BLEWritten, onRxWritten);

    Serial.println("BLESerial: NUS service initialized");
  }

  // FIX: renamed/expanded from poll() - call this every loop() iteration.
  // Services the BLE stack AND drains the TX ring buffer without blocking.
  void update() {
    BLE.poll();

    // Only transmit if connected and the central has subscribed to notifications
    if (!BLE.connected() || !txCharacteristic.subscribed()) return;
    if (txHead == txTail) return; // nothing queued

    unsigned long now = millis();
    if (now - lastTxTime < CHUNK_INTERVAL_MS) return;

    uint8_t chunk[20];
    size_t chunkSize = 0;
    int tempTail = txTail; // peek without committing

    while (tempTail != txHead && chunkSize < 20) {
      chunk[chunkSize++] = txBuffer[tempTail];
      tempTail = (tempTail + 1) % TX_BUFFER_SIZE;
    }

    if (chunkSize > 0) {
      // Only advance txTail if the write actually succeeded
      if (txCharacteristic.writeValue(chunk, chunkSize)) {
        txTail = tempTail;
        lastTxTime = now;
      }
    }
  }

  // Kept for backward compatibility with any existing bleSerial.poll() calls
  void poll() { update(); }

  // Number of bytes silently dropped due to a full TX buffer - check this
  // during debugging if output looks truncated.
  uint32_t getDroppedBytes() const { return droppedBytes; }

  // Check if data is available to read
  int available() {
    return (rxHead - rxTail + RX_BUFFER_SIZE) % RX_BUFFER_SIZE;
  }

  // Read a single byte
  int read() {
    if (rxHead == rxTail) return -1;

    uint8_t byte = rxBuffer[rxTail];
    rxTail = (rxTail + 1) % RX_BUFFER_SIZE;
    return byte;
  }

  // FIX: no longer blocks with delay(10). Queues all bytes into the TX ring
  // buffer; update() drains them asynchronously in the main loop.
  size_t print(const char* str) {
    if (!BLE.connected() || !str) return 0;

    size_t count = 0;
    while (*str) {
      if (enqueue((uint8_t)*str++)) {
        count++;
      } else {
        break; // buffer full - remaining bytes dropped, see getDroppedBytes()
      }
    }
    return count;
  }

  size_t print(int val) {
    char buf[12];
    itoa(val, buf, 10);
    return print(buf);
  }

  size_t print(unsigned long val) {
    char buf[12];
    sprintf(buf, "%lu", val);
    return print(buf);
  }

  size_t println(const char* str) {
    size_t n = print(str);
    n += print("\r\n");
    return n;
  }

  size_t println(int val) {
    size_t n = print(val);
    n += print("\r\n");
    return n;
  }

  size_t println() {
    return print("\r\n");
  }
};

// Static instance pointer initialization
BLESerial* BLESerial::instance = nullptr;

#endif // BLE_SERIAL_H
