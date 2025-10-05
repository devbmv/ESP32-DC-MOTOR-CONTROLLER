#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <map>
#include <functional>
#include <Preferences.h>
#include <vector>
#include "time.h"

// ===================== 🌍 NTP / TIME CONFIG =====================
extern const char *ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;

void initTime(const char *ntpServer = "pool.ntp.org", long gmtOffset_sec = 0, int daylightOffset_sec = 0);
String getDateTime();
tm getTimeStruct();
String getTimeOnly();
String getDateOnly();

// ===================== ⚙️ LOG BUFFER =====================
extern String logBufferSetup;
#define LOG_BUFFER_SIZE 4096
extern char logBufferRuntime[LOG_BUFFER_SIZE];
extern size_t logHead;

extern bool WEB_DEBUG;

// ===================== 💡 LED CONFIG =====================
#define LED_PIN 35
#define NUM_LEDS 1
extern CRGB leds[NUM_LEDS];

// ===================== 🧠 SYSTEM PATHS =====================
extern const char *fileSettingsPath;

// ===================== 🧲 HARDWARE PINS =====================
extern int system_temp_pin;
extern int engine_temp_pin;
extern int fan_control_pin;

// ===================== 🌡 TEMPERATURE READ INTERVALS =====================
extern int system_temp_read_interval;
extern int engine_temp_read_interval;

// ===================== 🔌 CAN BUS PINS =====================
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_38

// ===================== 🧩 GLOBAL OBJECTS =====================
extern OneWire oneWire;
extern DallasTemperature dallas;
extern AsyncWebServer server;
extern Preferences prefs;

extern size_t currentOTAProgress;
extern size_t totalOTABytes;
extern size_t currentFSProgress;
extern size_t totalFSBytes;

// ===================== 🧮 STRUCTS =====================

struct NTCConstants {
  const float VREF = 3.30f;
  const int ADC_MAX = 4095;
  const float R_FIXED = 10000.0f;
  const float NTC_BETA = 3950.0f;
  const float NTC_R0 = 10000.0f;
  const float NTC_T0K = 298.15f;
};
extern NTCConstants ntcConstants;

struct SystemInfo {
  float systemC;
  float engineC;
  uint32_t ts;
  float targetPercent;
  int target_pwm;
};
extern SystemInfo sensorData;

struct DallasState {
  bool requested = false;
  uint32_t tReq = 0;
};
extern DallasState dState;

// ===================== 🌪 FAN CONTROL =====================
enum class FanMode : uint8_t {
  AUTO,
  MANUAL
};

// ===================== 💾 SETTINGS LOAD STATUS =====================
enum SettingsLoadStatus {
  LOAD_OK,
  LOAD_FILE_NOT_FOUND,
  LOAD_FILE_OPEN_FAIL,
  LOAD_JSON_ERROR,
  LOAD_DEFAULTS_SAVED,
};

// ===================== 🧱 RTOS OBJECTS =====================
extern TaskHandle_t hSensors;
extern TaskHandle_t hControl;
extern TaskHandle_t hCan;
extern QueueHandle_t sensorDataQueue;
extern SemaphoreHandle_t mtxState;
extern EventGroupHandle_t egFlags;
extern TimerHandle_t tHeartbeat;

constexpr EventBits_t EV_NEW_SAMPLE = (1 << 0);

// ===================== ⚙️ ROUTE STRUCTURES =====================
typedef std::function<void(AsyncWebServerRequest *request)> ViewFunc;

struct Route {
  String path;
  WebRequestMethod method;
  ViewFunc view;
  Route() : path(""), method(HTTP_GET), view(nullptr) {}
  Route(const char *p, WebRequestMethod m, void (*f)(AsyncWebServerRequest *))
      : path(p), method(m), view(f) {}
};

struct StaticRoute {
  String url;
  String file;
  String mime;
};

// ===================== 🧩 SYSTEM SETTINGS =====================
struct SystemSettings {
  // --- General ---
  String hostname = "esp-device";
  uint8_t log_level = 2;
  bool telemetry_enabled = false;
  bool fs_format_on_fail = true;

  // --- Network & OTA ---
  String wifi_ssid = "";
  String wifi_pass = "";
  bool ota_enabled = false;
  String ota_url = "";

  // --- Temperature & ADC ---
  int min_rotation_temp = 25;
  int max_rotation_temp = 50;
  int system_temp_alert = 90;
  uint16_t temp_sample_interval_ms = 1000;
  uint16_t adc_samples = 8;
  String temp_sensor_type = "DS18B20";

  // --- Fan Control ---
  volatile FanMode fan_mode;
  uint32_t fan_start_boost_ms = 300;
  uint32_t pwm_freq_hz = 12500;
  uint8_t pwm_channel = 0;
  uint8_t pwm_resolution_bits = 8;
  uint16_t fan_control_interval = 1000;
  bool invert_pwm = false;
  uint8_t manual_percent = 0;
  bool manual_on = false;

  // --- UI Mapping ---
  int ui_system_min = 0;
  int ui_system_max = 100;
  int ui_engine_min = 0;
  int ui_engine_max = 100;

  uint16_t deactivateAlarmTime = 0;
  bool alarmTriggered = false;
  uint32_t reactivateAlarmCounter = 0;
};
extern SystemSettings g_settings;

// ===================== ⚙️ FUNCTION DECLARATIONS =====================
void initUart();
void initWiFi();
void setLed(uint8_t r, uint8_t g, uint8_t b);
void initRGB();
void initDallas();
void initFs();
void initPins();
void initADC();
void setLed(uint8_t r, uint8_t g, uint8_t b);
void initServer();
void reinitPwm(int channel, int freq_hz, int resolution_bits, bool invert);
void clearCoreDumpOnce();

void read_system_temp();
void read_engine_temp();
void sendSMS(const String &message);
void applyManualFan(bool on, uint8_t percent);
bool canInit(uint32_t bitRate = 500000);
void taskSensors(void *);
void taskControl(void *);
void taskCan(void *);
void hbCb(TimerHandle_t);
bool setInterval(unsigned long intervalMs, void (*fn)());
int pwm_max();

void processSerialCommands();
void initCommands();
String processCommand(const String &input);

void addLog(const String &msg);
String getLog();
void logSystemInfo();
void logMessage(const char *level, const char *fmt, ...);

void settingsApply();
void loadSettings();
bool settingsSaveToFS();
SettingsLoadStatus settingsLoadFromFS();
bool settingsFromJson(const String &body);
String settingsToJson(const SystemSettings &s);
String settingsDefaultsJson();
void fillJsonFrom(const SystemSettings &s, JsonDocument &doc);
void fillFromJson(SystemSettings &s, const JsonDocument &doc);

void setupTime();
void printLocalTime();
void saveNetwork(const String &ssid, const String &password);
bool tryConnect(const String &ssid, const String &password);

// ===================== 🧠 WEB HANDLERS =====================
void sysInfo(AsyncWebServerRequest *req);
bool isAuthenticated(AsyncWebServerRequest *request);
void log(AsyncWebServerRequest *request);
void handleClearLog(AsyncWebServerRequest *request);
void handleLogin(AsyncWebServerRequest *request);
void handleLogout(AsyncWebServerRequest *request);
void handleJson(AsyncWebServerRequest *request);
void get_settings(AsyncWebServerRequest *request);
void handleUpdateFirmware(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleUpdateLittleFS(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleFSStatus(AsyncWebServerRequest *request);
void handle_log(AsyncWebServerRequest *request);
void ota_status(AsyncWebServerRequest *request);
void login(AsyncWebServerRequest *req);
void log_function_execution(AsyncWebServerRequest *request);
void wifi_info(AsyncWebServerRequest *request);
void alarma(AsyncWebServerRequest *request);
void files_list(AsyncWebServerRequest *request);
void mem_info(AsyncWebServerRequest *request);
void fan(AsyncWebServerRequest *request);
void web_restart(AsyncWebServerRequest *request);
void readFile(AsyncWebServerRequest *request);
void toggleFan(AsyncWebServerRequest *req);
void setMode(AsyncWebServerRequest *request);
void settings(AsyncWebServerRequest *req);
void apiSettings(AsyncWebServerRequest *req);
void apiSettingsDefault(AsyncWebServerRequest *req);
void apiSensors(AsyncWebServerRequest *req);
void favicon(AsyncWebServerRequest *req);
void formatFS(AsyncWebServerRequest *request);
void deleteFile(AsyncWebServerRequest *request);
void deleteFileView(AsyncWebServerRequest *request);
void sendSmsView(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void firmwareUpdateView(AsyncWebServerRequest *request);
void updateFsView(AsyncWebServerRequest *request);
void setPwmFreqView(AsyncWebServerRequest *request);
void setManualPercentView(AsyncWebServerRequest *request);

// ===================== 🧾 LOGGER MACROS =====================
#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif
// 0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE

#define LOGE(...) do { if (LOG_LEVEL >= 1) logMessage("E", __VA_ARGS__); } while (0)
#define LOGW(...) do { if (LOG_LEVEL >= 2) logMessage("W", __VA_ARGS__); } while (0)
#define LOGI(...) do { if (LOG_LEVEL >= 3) logMessage("I", __VA_ARGS__); } while (0)
#define LOGD(...) do { if (LOG_LEVEL >= 4) logMessage("D", __VA_ARGS__); } while (0)
#define LOGT(...) do { if (LOG_LEVEL >= 5) logMessage("T", __VA_ARGS__); } while (0)

#define CHECK_AND_LOG(expr, okmsg, errmsg) \
  do { if (expr) { LOGI okmsg; } else { LOGE errmsg; } } while (0)

static inline uint32_t ts_ms() { return (uint32_t)millis(); }
static inline float heap_kb() { return ESP.getFreeHeap() / 1024.0f; }

