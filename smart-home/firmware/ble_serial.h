/*
 * BLE Serial Module
 * Nordic UART Service (NUS) implementation using ArduinoBLE
 * 
 * Provides a Serial-like interface over BLE for the Smart Home Controller.
 * Uses standard Nordic UART UUIDs for compatibility with mobile apps.
 */

#ifndef BLE_SERIAL_H
#define BLE_SERIAL_H

#include <ArduinoBLE.h>

// ============================================================================
// NORDIC UART SERVICE UUIDs
// ============================================================================

#define NUS_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHARACTERISTIC   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // Write (phone → board)
#define NUS_TX_CHARACTERISTIC   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // Notify (board → phone)

// ============================================================================
// BLE SERIAL CLASS
// ============================================================================

class BLESerial {
private:
  BLEService uartService;
  BLECharacteristic txCharacteristic;
  BLECharacteristic rxCharacteristic;
  
  // Receive buffer
  static const int RX_BUFFER_SIZE = 256;
  uint8_t rxBuffer[RX_BUFFER_SIZE];
  volatile int rxHead;
  volatile int rxTail;
  
  // Static instance pointer for callback
  static BLESerial* instance;
  
  // Static callback for received data
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
  
public:
  BLESerial() : 
    uartService(NUS_SERVICE_UUID),
    txCharacteristic(NUS_TX_CHARACTERISTIC, BLENotify, 20),
    rxCharacteristic(NUS_RX_CHARACTERISTIC, BLEWrite, 20),
    rxHead(0), rxTail(0) {
    instance = this;
  }
  
  // Initialize the BLE UART service
  void begin() {
    // Add characteristics to service
    uartService.addCharacteristic(txCharacteristic);
    uartService.addCharacteristic(rxCharacteristic);
    
    // Add service to BLE
    BLE.addService(uartService);
    
    // Set callback for received data
    rxCharacteristic.setEventHandler(BLEWritten, onRxWritten);
    
    Serial.println("BLESerial: NUS service initialized");
  }
  
  // Poll BLE for events (call in loop)
  void poll() {
    BLE.poll();
  }
  
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
  
  // Write a string
  size_t print(const char* str) {
    if (!BLE.connected()) return 0;
    
    size_t len = strlen(str);
    size_t sent = 0;
    
    // Send in chunks of 20 bytes (BLE MTU limitation)
    while (sent < len) {
      size_t chunkSize = min((size_t)20, len - sent);
      txCharacteristic.writeValue((const uint8_t*)(str + sent), chunkSize);
      sent += chunkSize;
      delay(10); // Small delay between chunks for BLE stack
    }
    
    return sent;
  }
  
  // Print an integer
  size_t print(int val) {
    char buf[12];
    itoa(val, buf, 10);
    return print(buf);
  }
  
  // Print an unsigned long
  size_t print(unsigned long val) {
    char buf[12];
    sprintf(buf, "%lu", val);
    return print(buf);
  }
  
  // Print a string with newline
  size_t println(const char* str) {
    size_t n = print(str);
    n += print("\r\n");
    return n;
  }
  
  // Print an integer with newline
  size_t println(int val) {
    size_t n = print(val);
    n += print("\r\n");
    return n;
  }
  
  // Print just a newline
  size_t println() {
    return print("\r\n");
  }
};

// Static instance pointer initialization
BLESerial* BLESerial::instance = nullptr;

#endif // BLE_SERIAL_H
