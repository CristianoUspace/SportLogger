// SPORT LOGGER V1.7-diag1 - Aprire QUESTO file nell'IDE Arduino.
// Tenere firmware.cpp, config.h, core.h e ui.h nella stessa cartella.
// setup() e loop() sono definiti in firmware.cpp e compilati automaticamente.
// Scheda: ESP32 Dev Module; pacchetto esp32 by Espressif Systems 2.0.17.
// Le impostazioni dei pin, batteria e Wi-Fi si trovano in config.h.
#include <Arduino.h>
// Esplicita le dipendenze anche nello sketch principale per il Gestore librerie.
#include <ArduinoJson.h>
#include <TinyGPSPlus.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
