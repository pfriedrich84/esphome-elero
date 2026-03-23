/// @file SPI.h
/// Minimal Arduino SPI shim for RadioLib compilation compatibility.
///
/// RadioLib's Module.h and ArduinoHal.h include <SPI.h> when RADIOLIB_BUILD_ARDUINO
/// is defined (i.e., when the ARDUINO macro is >= 100). On ESPHome ESP-IDF builds
/// where the Arduino SPI library isn't in the include path, this causes a fatal
/// "SPI.h: No such file or directory" error.
///
/// This shim provides just enough type definitions (SPISettings, SPIClass, etc.)
/// for RadioLib to compile. None of these stubs are ever called at runtime —
/// all SPI communication goes through EspHomeRadioLibHal, which delegates to
/// ESPHome's SPIDevice interface.
#pragma once

#include <cstdint>
#include <cstddef>

#define MSBFIRST 1
#define LSBFIRST 0
#define SPI_MODE0 0x00

class SPISettings {
 public:
  SPISettings() {}
  SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {
    (void) clock;
    (void) bitOrder;
    (void) dataMode;
  }
};

class SPIClass {
 public:
  void begin() {}
  void end() {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}
  uint8_t transfer(uint8_t data) { return data; }
  void transfer(void *buf, size_t count) {
    (void) buf;
    (void) count;
  }
};

inline SPIClass SPI;
