#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "project_config.h"
#include "driver/twai.h"     // ESP-IDF CAN (TWAI)
#include <FastLED.h>
#include "settings.h"
#include <ArduinoJson.h>

// ======================================================
// 🧩 Global Configuration Flags
// ======================================================
bool WEB_DEBUG = true;        // Enables web-based debug logs
String logBuffer;             // Temporary log storage

// ======================================================
// 📡 GPIO Pins
// ======================================================
int system_temp_pin  = 6;
int engine_temp_pin  = 7;
int fan_control_pin  = 8;

// ======================================================
// 💡 RGB LED Configuration
// ======================================================
CRGB leds[NUM_LEDS];          // RGB LED array for FastLED control

// ======================================================
// ⚙️ File System Paths
// ======================================================
const char *fileSettingsPath = "/config.json";  // Settings file in LittleFS

// ======================================================
// 🕒 Read Intervals (ms)
// ======================================================
int system_temp_read_interval = 1000;
int engine_temp_read_interval = 1000;

// ======================================================
// 🔄 OTA Update Tracking
// ======================================================
size_t currentOTAProgress = 0;
size_t totalOTABytes      = 0;

size_t currentFSProgress  = 0;
size_t totalFSBytes       = 0;

// ======================================================
// 🌡️ Temperature Sensors
// ======================================================
OneWire oneWire(engine_temp_pin);   // OneWire bus for Dallas sensors
DallasTemperature dallas(&oneWire);
DallasState dState = {};            // Internal Dallas state structure
SystemInfo sensorData = {};         // Shared system data
NTCConstants ntcConstants;          // NTC calibration constants

// ======================================================
// ⚙️ RTOS primitives
// ======================================================
TaskHandle_t hSensors  = nullptr;   // Core 0: Sensor acquisition task
TaskHandle_t hControl  = nullptr;   // Core 1: Fan control logic task
TaskHandle_t hCan      = nullptr;   // Core 1: CAN bus handler
QueueHandle_t sensorDataQueue = nullptr;  // Queue for sensor updates
SemaphoreHandle_t mtxState     = nullptr; // Mutex for shared state protection
EventGroupHandle_t egFlags     = nullptr; // Event flags (e.g., heartbeat)
TimerHandle_t tHeartbeat       = nullptr; // Heartbeat timer

// ======================================================
// 🌐 Web Server
// ======================================================
AsyncWebServer server(80);   // HTTP server on port 80
