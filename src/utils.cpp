#include <Arduino.h>
#include "project_config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "driver/twai.h" // CAN (TWAI) from ESP-IDF
#include "nvs_flash.h"
#include "nvs.h"
#include <AsyncTCP.h>
#include "esp_core_dump.h"
#include <HTTPClient.h>

// Returns the maximum PWM value based on resolution
int pwm_max() { return (1 << g_settings.pwm_resolution_bits) - 1; }

// --- Local helpers for human-readable output ---
static inline const __FlashStringHelper *yesno(bool v)
{
  return v ? F("Yes") : F("No");
}

static inline const __FlashStringHelper *onoff(bool v)
{
  return v ? F("ON") : F("OFF");
}

static inline const __FlashStringHelper *fanModeName(FanMode m)
{
  switch (m)
  {
  case FanMode::AUTO:
    return F("AUTO");
  case FanMode::MANUAL:
    return F("MANUAL");
  default:
    return F("UNKNOWN");
  }
}

// Format uptime into a readable string (e.g., "1d 2h 30m 5s")
static void formatUptime(char *buf, size_t n)
{
  uint32_t ms = millis();
  uint32_t s = ms / 1000;
  uint32_t d = s / 86400;
  s %= 86400;
  uint32_t h = s / 3600;
  s %= 3600;
  uint32_t m = s / 60;
  s %= 60;
  snprintf(buf, n, "%lud %luh %lum %lus",
           (unsigned long)d, (unsigned long)h, (unsigned long)m, (unsigned long)s);
}

// Registry for custom console commands
std::map<String, std::function<String(String)>> commandRegistry;

// Register a new command
void registerCommand(const String &name, std::function<String(String)> handler)
{
  commandRegistry[name] = handler;
}

// Process a command string (e.g., "set hostname mydevice")
String processCommand(const String &input)
{
  int spaceIdx = input.indexOf(' ');
  String cmd = (spaceIdx == -1) ? input : input.substring(0, spaceIdx);
  String args = (spaceIdx == -1) ? "" : input.substring(spaceIdx + 1);
  cmd.toLowerCase();

  if (cmd == "help")
  {
    String response = "=== Available Commands ===\n";
    for (auto &kv : commandRegistry)
      response += kv.first + "\n";
    response += "===========================\n";
    return response;
  }

  if (commandRegistry.count(cmd))
    return commandRegistry[cmd](args);

  return "❌ Unknown command. Type 'help' for a list.\n";
}

// Reads and executes serial commands entered via USB
void processSerialCommands()
{
  if (!Serial.available())
    return;

  String input = Serial.readStringUntil('\n');
  input.trim();
  String out = processCommand(input);
  LOGI("%s", out.c_str());
  addLog("> " + input);
  addLog(out);
}

// Initialize serial commands
void initCommands()
{
  // --- STATUS ---
  registerCommand("status", [](String args) -> String
                  {
        String out;
        out += "=== System Info ===\n";
        out += "systemC = " + String(sensorData.systemC, 2) + "\n";
        out += "engineC = " + String(sensorData.engineC, 2) + "\n";
        out += "ts      = " + String(sensorData.ts) + "\n";
        out += "manual_percent = " + String(g_settings.manual_percent) + "\n";
        out += "targetPercent  = " + String(sensorData.targetPercent, 2) + "\n";
        out += "target_pwm     = " + String(sensorData.target_pwm) + "\n";
        return out; });

  // --- GET VARIABLE ---
  registerCommand("get", [](String var) -> String
                  {
        if (var == "systemC") return "systemC = " + String(sensorData.systemC, 2);
        if (var == "engineC") return "engineC = " + String(sensorData.engineC, 2);
        if (var == "hostname") return "hostname = " + g_settings.hostname;
        return "Unknown variable!"; });

  // --- SET VARIABLE ---
  registerCommand("set", [](String args) -> String
                  {
        int sp = args.indexOf(' ');
        if (sp == -1) return "Format: set <var> <val>";
        String var = args.substring(0, sp);
        String val = args.substring(sp + 1);
        if (var == "manual_percent") {
            g_settings.manual_percent = val.toInt();
            return "✅ manual_percent=" + String(g_settings.manual_percent);
        }
        if (var == "hostname") {
            g_settings.hostname = val;
            return "✅ hostname=" + g_settings.hostname;
        }
        return "Unknown variable!"; });

  // --- FAN MODE ---
  registerCommand("fan_mode", [](String args) -> String
                  {
        if (args == "auto") { g_settings.fan_mode = FanMode::AUTO; return "Fan mode=AUTO"; }
        if (args == "manual") { g_settings.fan_mode = FanMode::MANUAL; return "Fan mode=MANUAL"; }
        return "Usage: fan_mode auto/manual"; });

  // --- MANUAL PWM SET ---
  registerCommand("set_pwm", [](String args) -> String
                  {
        int val = args.toInt();
        sensorData.target_pwm = constrain(val, 0, pwm_max());
        ledcWrite(g_settings.pwm_channel, sensorData.target_pwm);
        return "PWM=" + String(sensorData.target_pwm); });

  // --- HEAP INFO ---
  registerCommand("heap", [](String args) -> String
                  { return "Free heap=" + String(ESP.getFreeHeap()); });

  // --- CURRENT TIME ---
  registerCommand("time", [](String args) -> String
                  { return "Current time=" + getDateTime(); });

  // --- SYSTEM UPTIME ---
  registerCommand("uptime", [](String args) -> String
                  {
        char buf[32];
        formatUptime(buf, sizeof(buf));
        return "Uptime=" + String(buf); });

  // --- CLEAR LOG BUFFER ---
  registerCommand("clear_log", [](String args) -> String
                  {
        logBufferSetup = "";
        memset(logBufferRuntime, 0, sizeof(logBufferRuntime));
        logHead = 0;
        return "✅ Log cleared"; });

  // --- RESTART DEVICE ---
  registerCommand("restart", [](String args) -> String
                  {
        ESP.restart();
        return "Restarting..."; });

  // --- SEND SMS VIA CallMeBot API ---
  registerCommand("send_sms", [](String args) -> String
                  {
        sendSMS(args);
        return "📤 SMS sent: " + args; });

  // --- SET WI-FI CREDENTIALS ---
  registerCommand("wifi_set", [](String args) -> String
                  {
        String ssid, pass;
        int firstQuote = args.indexOf('"');
        int secondQuote = args.indexOf('"', firstQuote + 1);
        if (firstQuote != -1 && secondQuote != -1) {
            ssid = args.substring(firstQuote + 1, secondQuote);
            pass = args.substring(secondQuote + 1);
            pass.trim();
        } else {
            int space = args.indexOf(' ');
            if (space == -1) return "⚠️ Format: wifi_set <ssid> <password>";
            ssid = args.substring(0, space);
            pass = args.substring(space + 1);
        }

        if (ssid.isEmpty() || pass.isEmpty())
            return "⚠️ Correct format: wifi_set <ssid> <password>";

        if (tryConnect(ssid, pass)) {
            saveNetwork(ssid, pass);
            return "✅ Connection successful and saved!";
        }
        return "❌ Connection failed!"; });

  // --- CLEAR STORED WI-FI NETWORKS ---
  registerCommand("wifi_clear", [](String args) -> String
                  {
        nvs_handle_t nvs;
        if (nvs_open("wifi_networks", NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_erase_all(nvs);
            nvs_commit(nvs);
            nvs_close(nvs);
            return "🧹 All Wi-Fi networks were erased from NVS!";
        }
        return "❌ Error opening NVS!"; });
}

// -------- JSON Helpers --------
bool settingsFromJson(const String &json)
{
  JsonDocument doc;
  auto err = deserializeJson(doc, json);
  if (err)
  {
    Serial.print(F("❌ JSON parse failed: "));
    Serial.println(err.c_str());
    return false;
  }
  fillFromJson(g_settings, doc);
  return true;
}

String settingsToJson(const SystemSettings &s)
{
  JsonDocument doc;
  fillJsonFrom(s, doc);
  String out;
  serializeJson(doc, out);
  return out;
}

String settingsDefaultsJson()
{
  SystemSettings def;
  return settingsToJson(def);
}

// -------- Core Dump Handling --------
void clearCoreDumpOnce()
{
  nvs_handle_t h;
  if (nvs_open("sys", NVS_READWRITE, &h) != ESP_OK)
    return;

  uint8_t done = 0;
  nvs_get_u8(h, "cd_erased", &done);
  if (!done)
  {
    esp_core_dump_image_erase();
    nvs_set_u8(h, "cd_erased", 1);
    nvs_commit(h);
  }
  nvs_close(h);
  delay(10);
  LOGI("Reset reason: %d", esp_reset_reason());
}

// -------- URL Encode (for CallMeBot) --------
String urlencode(String str)
{
  String encoded = "";
  char c;
  char code0, code1;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (isalnum(c))
      encoded += c;
    else
    {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9)
        code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9)
        code0 = c - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}

// -------- Authentication --------
bool isAuthenticated(AsyncWebServerRequest *request)
{
  return request->hasHeader("Cookie") &&
         request->header("Cookie").indexOf("ESPSESSION=123456") >= 0;
}

// -------- Initialization Helpers --------
void initUart()
{
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 1000)
    ; // așteaptă până portul e gata sau 1s timeout
  Serial.println("✅ Serial initialized at 115200 baud");
}

void initDallas()
{
  dallas.begin();
  delay(10);
}

void initFs()
{
  if (!LittleFS.begin(false, "/littlefs", 10, "littlefs"))
  {
    LOGE("❌ LittleFS mount failed, attempting to format...");

    if (LittleFS.begin(true, "/littlefs", 10, "littlefs"))
      LOGI("✅ LittleFS formatted and mounted successfully.");
    else
      LOGE("❌ LittleFS format/mount failed. Partition may be corrupted.");
  }
  else
  {
    LOGI("✅ LittleFS mounted successfully.");
  }
  delay(50);
}

void initPins()
{
  pinMode(system_temp_pin, INPUT);
  pinMode(fan_control_pin, OUTPUT);
  digitalWrite(fan_control_pin, LOW);
  delay(50);
}

void initADC()
{
  analogReadResolution(12);
  analogSetPinAttenuation(system_temp_pin, ADC_11db);
  delay(10);
}

void reinitPwm(int channel, int freq_hz, int resolution_bits, bool invert)
{
  ledcDetachPin(fan_control_pin);
  ledcSetup(g_settings.pwm_channel, g_settings.pwm_freq_hz, g_settings.pwm_resolution_bits);
  ledcAttachPin(fan_control_pin, g_settings.pwm_channel);
  ledcWrite(channel, 0);
}

// -------- Send WhatsApp via CallMeBot --------
void sendSMS(const String &message)
{
  String phoneNumber = "+353858440006"; // Ireland number example
  String apikey = "9861441";            // Replace with your valid API key

  HTTPClient http;
  http.setTimeout(10000);

  String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber +
               "&text=" + urlencode(message) + "&apikey=" + apikey;

  http.begin(url);
  int httpCode = http.GET();
  if (httpCode > 0)
    LOGI("📤 SMS sent: %d", httpCode);
  else
    LOGE("❌ Failed to send SMS: %s", http.errorToString(httpCode).c_str());
  http.end();
}

// -------- Fan Control --------
void applyManualFan(bool on, uint8_t percent)
{
  int maxv = pwm_max();

  if (on && percent > 0)
  {
    sensorData.targetPercent = percent;
    sensorData.target_pwm = (int)((percent / 100.0f) * maxv);
  }
  else
  {
    sensorData.targetPercent = 0;
    sensorData.target_pwm = 0;
  }

  ledcWrite(g_settings.pwm_channel, sensorData.target_pwm);
  LOGI("[FAN] mode=%s pct=%u pwm=%d/%d",
       (g_settings.fan_mode == FanMode::AUTO ? "AUTO" : "MANUAL"),
       percent,
       sensorData.target_pwm,
       maxv);
}

// -------- CAN (TWAI) --------
bool canInit(uint32_t bitRate)
{
  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  g_config.tx_queue_len = 8;
  g_config.rx_queue_len = 16;
  g_config.clkout_divider = 0;

  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  if (bitRate == 250000)
    t_config = TWAI_TIMING_CONFIG_250KBITS();
  else if (bitRate == 1000000)
    t_config = TWAI_TIMING_CONFIG_1MBITS();

  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK)
    return false;
  if (twai_start() != ESP_OK)
    return false;

  LOGI("[CAN] TWAI started");
  return true;
}

void taskCan(void *pvParameters)
{
  twai_message_t msg;

  for (;;)
  {
    if (twai_receive(&msg, pdMS_TO_TICKS(50)) == ESP_OK)
    {
      LOGI("=== CAN FRAME RECEIVED ===");
      LOGI("ID: 0x%08X (%u)", msg.identifier, msg.identifier);
      LOGI("DLC: %d", msg.data_length_code);

      for (int i = 0; i < msg.data_length_code; i++)
      {
        uint8_t val = msg.data[i];
        char binStr[9];
        for (int b = 7; b >= 0; b--)
          binStr[7 - b] = ((val >> b) & 1) ? '1' : '0';
        binStr[8] = '\0';
        LOGI("Byte[%d] HEX: 0x%02X DEC: %3u BIN: %s", i, val, val, binStr);
      }

      LOGI("===========================");
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// -------- LED Control --------
void setLed(uint8_t r, uint8_t g, uint8_t b)
{
  leds[0].setRGB(r, g, b);
  FastLED.show();
}

// -------- Timed Function Execution --------
bool setInterval(unsigned long intervalMs, void (*fn)())
{
  static unsigned long lastCall[20];
  static int counter = 0;

  int id = counter++;
  unsigned long now = millis();

  if (now - lastCall[id] >= intervalMs)
  {
    lastCall[id] = now;
    fn();
    return true;
  }
  return false;
}

// -------- Temperature Readings --------
void read_system_temp()
{
  static uint32_t system_temp_timer = 0;
  if (millis() - system_temp_timer >= system_temp_read_interval)
  {
    uint16_t raw = analogRead(system_temp_pin);
    float voltage = (raw * ntcConstants.VREF) / ntcConstants.ADC_MAX;
    const float eps = 0.0001;
    float denom = max(ntcConstants.VREF - voltage, eps);
    float r_ntc = (voltage * ntcConstants.R_FIXED) / denom;
    float invT = (1.0 / ntcConstants.NTC_T0K) +
                 (1.0 / ntcConstants.NTC_BETA) * log(r_ntc / ntcConstants.NTC_R0);
    sensorData.systemC = (1.0 / invT) - 273.15;
    system_temp_timer = millis();
  }
}

void read_engine_temp()
{
  static unsigned long engine_temp_read_timer = 0;
  static bool tempRequested = false;

  if (!tempRequested && (millis() - engine_temp_read_timer >= engine_temp_read_interval))
  {
    dallas.requestTemperatures();
    tempRequested = true;
    engine_temp_read_timer = millis();
  }

  if (tempRequested && (millis() - engine_temp_read_timer >= 750))
  {
    float temp = dallas.getTempCByIndex(0);
    if (temp != DEVICE_DISCONNECTED_C)
    {
      sensorData.engineC = temp;
    }
    else
    {
      sensorData.engineC = NAN;
      LOGW("Dallas sensor disconnected!");
    }
    tempRequested = false;
  }
}

// -------- System Info Logging --------
void logSystemInfo()
{
  LOGI("");
  LOGI("Free heap: %.2f MB", ESP.getFreeHeap() / (1024.0 * 1024));
  LOGI("Min free heap: %.2f MB", ESP.getMinFreeHeap() / (1024.0 * 1024));
  LOGI("Largest free block: %.2f MB",
       heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / (1024.0 * 1024));
  LOGI("Free DRAM: %.2f MB",
       heap_caps_get_free_size(MALLOC_CAP_8BIT) / (1024.0 * 1024));
  LOGI("Free IRAM: %.2f MB",
       heap_caps_get_free_size(MALLOC_CAP_32BIT) / (1024.0 * 1024));

  LOGI("Stack high water mark (control): %.2f KB",
       uxTaskGetStackHighWaterMark(hControl) * sizeof(StackType_t) / 1024.0);
  LOGI("Stack high water mark (sensors): %.2f KB",
       uxTaskGetStackHighWaterMark(hSensors) * sizeof(StackType_t) / 1024.0);
  LOGI("Stack high water mark (CAN): %.2f KB",
       uxTaskGetStackHighWaterMark(hCan) * sizeof(StackType_t) / 1024.0);

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  LOGI("Chip model: %d, cores: %d, features: %d",
       chip_info.model, chip_info.cores, chip_info.features);
  LOGI("CPU freq: %d MHz", getCpuFrequencyMhz());
  LOGI("Flash size: %.2f MB", ESP.getFlashChipSize() / (1024.0 * 1024));
  LOGI("");
}
