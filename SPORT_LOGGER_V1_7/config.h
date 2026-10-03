#pragma once
#include <stdint.h>
namespace Config {
constexpr char FIRMWARE_VERSION[] = "1.7-diag1";
constexpr bool SERIAL_LOG_ENABLED=true;
constexpr uint32_t SERIAL_STATUS_MS=10000; // Idle only; 0 disables summary, not events.
constexpr uint32_t I2C_HZ=400000, SD_SPI_HZ=4000000; // Try 100000 / 1000000 after inspecting wiring.
constexpr char SSID[] = "SPORT-LOGGER";
constexpr char PASSWORD[] = "SportLogger17"; // Change before use, >= 8 characters.
constexpr int SDA_PIN=21, SCL_PIN=22, SD_SCK=18, SD_MISO=19, SD_MOSI=23, SD_CS=13;
constexpr int GPS_RX=16, GPS_TX=17; // ESP32 RX <- GNSS TX. WROOM, not WROVER/PSRAM.
constexpr uint32_t GPS_BAUD=9600;
constexpr int BATTERY_PIN=34, LED_PIN=32, SWITCH_PIN=27;
constexpr uint8_t IMU_ADDR=0x68;
constexpr uint32_t SAMPLE_US=20000, FLUSH_MS=1000, SWITCH_DEBOUNCE_MS=50;
constexpr uint64_t SD_RESERVE=1024*1024; // Stop before exhausting FAT space.
constexpr bool BATTERY_ENABLED=true;
constexpr float R_TOP=22000, R_BOTTOM=10000, BATTERY_FACTOR=1.0f;
constexpr int BATTERY_CELLS=2; // Existing project used 2S. Set 1 for a single LiPo.
constexpr float WARNING_PER_CELL=3.50f;
}
