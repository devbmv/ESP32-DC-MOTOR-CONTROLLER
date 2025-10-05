#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "project_config.h"
#include <ArduinoJson.h>

// ============================================================
// 🔹 Reset Reason
// ============================================================
String getResetReason()
{
  switch (esp_reset_reason())
  {
  case ESP_RST_POWERON:
    return F("Power-On Reset");
  case ESP_RST_EXT:
    return F("External Reset");
  case ESP_RST_SW:
    return F("Software Reset");
  case ESP_RST_PANIC:
    return F("Panic Reset");
  case ESP_RST_INT_WDT:
    return F("Interrupt Watchdog");
  case ESP_RST_TASK_WDT:
    return F("Task Watchdog");
  case ESP_RST_WDT:
    return F("Other Watchdog");
  case ESP_RST_DEEPSLEEP:
    return F("Wake from Deep Sleep");
  case ESP_RST_BROWNOUT:
    return F("Brownout");
  case ESP_RST_SDIO:
    return F("SDIO Reset");
  default:
    return F("Unknown Reset Reason");
  }
}

// ============================================================
// 🔹 Fill System JSON
// ============================================================
void fillJson(JsonDocument &json)
{
  uint64_t chipId = ESP.getEfuseMac();
  String chip_id = String((uint32_t)(chipId >> 32), HEX) + String((uint32_t)chipId, HEX);
  chip_id.toUpperCase();

  json[F("chip_id")] = chip_id;
  json[F("ip_address")] = WiFi.localIP().toString();
  json[F("mac_address")] = WiFi.macAddress();
  json[F("firmware_version")] = String(ESP.getSdkVersion());
  json[F("esp_model")] = F("ESP32");
  json[F("temperature")] = sensorData.systemC;
  json[F("free_heap")] = ESP.getFreeHeap();
  json[F("status")] = F("on");
  json[F("uptime_seconds")] = millis() / 1000;
  json[F("reset_reason")] = getResetReason();
  json[F("cpu_frequency")] = getCpuFrequencyMhz();
  json[F("flash_size")] = ESP.getFlashChipSize();
  json[F("sketch_size")] = ESP.getSketchSize();
  json[F("sketch_free_space")] = ESP.getFreeSketchSpace();
  json[F("storage_type")] = F("SPIFFS");
  json[F("board_name")] = ARDUINO_BOARD;
}

// ============================================================
// 🔹 System Info
// ============================================================
void sysInfo(AsyncWebServerRequest *req)
{
  logSystemInfo();
  req->send(200, "text/plain", F("✅ System info logged!"));
}

// ============================================================
// 🔹 Log Control
// ============================================================
void handleClearLog(AsyncWebServerRequest *request)
{
  memset(logBufferRuntime, 0, LOG_BUFFER_SIZE);
  logHead = 0;
  request->send(200, "text/plain", F("✅ Log cleared"));
}

void log(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", getLog());
}

// ============================================================
// 🔹 Authentication
// ============================================================
void handleLogout(AsyncWebServerRequest *request)
{
  Serial.println(F("📤 Logout triggered"));
  AsyncWebServerResponse *response = request->beginResponse(302);
  response->addHeader("Location", "/login");
  response->addHeader("Set-Cookie", "ESPSESSION=; Path=/; Max-Age=0");
  request->send(response);
}

void login(AsyncWebServerRequest *req)
{
  req->send(LittleFS, "/login.html", "text/html");
}

void handleLogin(AsyncWebServerRequest *request)
{
  if (request->hasParam("username", true) && request->hasParam("password", true))
  {
    String user = request->getParam("username", true)->value();
    String pass = request->getParam("password", true)->value();

    if (user == car_user_name && pass == car_user_password)
    {
      AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
      response->addHeader("Set-Cookie", "ESPSESSION=123456; Path=/");
      response->addHeader("Location", "/fan");
      request->send(response);
    }
    else
    {
      request->send(401, "text/plain", F("Invalid username or password"));
    }
  }
  else
  {
    request->send(400, "text/plain", F("Missing parameters"));
  }
}

// ============================================================
// 🔹 JSON / Status
// ============================================================
void handleJson(AsyncWebServerRequest *request)
{
  JsonDocument json;
  fillJson(json);
  String output;
  serializeJsonPretty(json, output);
  request->send(200, "application/json", output);
}

void ota_status(AsyncWebServerRequest *request)
{
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  JsonDocument doc;
  doc[F("progress")] = currentOTAProgress;
  doc[F("total")] = totalOTABytes;
  serializeJson(doc, *response);
  request->send(response);
}

// ============================================================
// 🔹 OTA Update Handlers
// ============================================================
void handleUpdateFirmware(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    currentOTAProgress = 0;
    totalOTABytes = request->contentLength();
    Serial.printf_P(PSTR("[OTA] Starting firmware update (%u bytes)...\n"), totalOTABytes);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
      request->send(500, "text/plain", F("❌ Error initializing update."));
      return;
    }
  }

  currentOTAProgress = index + len;
  if (!Update.hasError() && Update.write(data, len) != len)
    Serial.printf_P(PSTR("[OTA] Incomplete write: %u bytes\n"), len);

  if (final)
  {
    if (Update.end(true))
    {
      Serial.println(F("[OTA] Firmware updated successfully!"));
      request->send(200, "text/plain", F("✅ Firmware updated successfully! Restarting in 2 seconds..."));
      delay(1000);
      ESP.restart();
    }
    else
    {
      Serial.printf_P(PSTR("[OTA] Error: %s\n"), Update.errorString());
      request->send(500, "text/plain", String(F("❌ Update failed: ")) + Update.errorString());
    }
  }
}

void handleUpdateLittleFS(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    currentFSProgress = 0;
    totalFSBytes = request->contentLength();
    Serial.printf_P(PSTR("[OTA] Starting LittleFS update (%u bytes)...\n"), totalFSBytes);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS))
    {
      request->send(500, "text/plain", F("❌ Error initializing LittleFS update."));
      return;
    }
  }

  currentFSProgress = index + len;
  if (!Update.hasError() && Update.write(data, len) != len)
    Serial.printf_P(PSTR("[OTA] Incomplete LittleFS write: %u bytes\n"), len);

  if (final)
  {
    if (Update.end(true))
    {
      Serial.println(F("[OTA] LittleFS updated successfully!"));
      request->send(200, "text/plain", F("✅ LittleFS updated successfully! Restarting in 2 seconds..."));
      delay(3000);
      ESP.restart();
    }
    else
    {
      Serial.printf_P(PSTR("[OTA] LittleFS Error: %s\n"), Update.errorString());
      request->send(500, "text/plain", String(F("❌ LittleFS update failed: ")) + Update.errorString());
    }
  }
}

// ============================================================
// 🔹 Fan & Settings
// ============================================================
void setMode(AsyncWebServerRequest *req)
{
  if (!req->hasParam("value"))
  {
    req->send(400, "application/json", F("{\"ok\":false,\"err\":\"missing value\"}"));
    return;
  }

  auto v = req->getParam("value")->value();
  FanMode newMode = v.equalsIgnoreCase("MANUAL") ? FanMode::MANUAL : FanMode::AUTO;

  if (g_settings.fan_mode != newMode)
  {
    g_settings.fan_mode = newMode;
    if (!settingsSaveToFS())
    {
      req->send(500, "application/json", F("{\"ok\":false,\"err\":\"save failed\"}"));
      return;
    }
    Serial.printf_P(PSTR("✅ Fan mode saved: %s\n"), (newMode == FanMode::MANUAL ? "MANUAL" : "AUTO"));
  }

  req->send(200, "application/json", F("{\"ok\":true}"));
}

void toggleFan(AsyncWebServerRequest *req)
{
  g_settings.manual_on = !g_settings.manual_on;
  String out = String(F("{\"ok\":true,\"manual_on\":")) + (g_settings.manual_on ? "true" : "false") + "}";
  req->send(200, "application/json", out);
}
// ============================================================
// 🔹 Wi-Fi Info
// ============================================================
void wifi_info(AsyncWebServerRequest *request)
{
  JsonDocument doc;

  int n = WiFi.scanNetworks();
  JsonArray networks = doc[F("available")].to<JsonArray>();

  for (int i = 0; i < n; ++i)
  {
    JsonObject net = networks.add<JsonObject>();
    net[F("ssid")] = WiFi.SSID(i);
    net[F("rssi")] = WiFi.RSSI(i);
    net[F("bssid")] = WiFi.BSSIDstr(i);
    net[F("channel")] = WiFi.channel(i);
    net[F("encryption")] = WiFi.encryptionType(i);
  }

  JsonObject current = doc[F("current")].to<JsonObject>();
  current[F("connected_ssid")] = WiFi.SSID();
  current[F("ip")] = WiFi.localIP().toString();
  current[F("mac")] = WiFi.macAddress();
  current[F("bssid")] = WiFi.BSSIDstr();
  current[F("rssi")] = WiFi.RSSI();
  current[F("gateway")] = WiFi.gatewayIP().toString();
  current[F("dns")] = WiFi.dnsIP().toString();
  current[F("subnet")] = WiFi.subnetMask().toString();
  current[F("status")] = WiFi.status();

  String output;
  serializeJsonPretty(doc, output);
  request->send(200, "application/json", output);
}

// ============================================================
// 🔹 Files List
// ============================================================
void files_list(AsyncWebServerRequest *request)
{
  String json = "[";
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file)
  {
    if (json.length() > 1)
      json += ",";
    json += "\"" + String(file.name()) + "\"";
    file = root.openNextFile();
  }
  json += "]";
  request->send(200, "application/json", json);
}

// ============================================================
// 🔹 Memory Info
// ============================================================
void mem_info(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  const char *revLabel;
  switch (chip_info.revision)
  {
  case 0:
    revLabel = PSTR("ECO0");
    break;
  case 1:
    revLabel = PSTR("ECO1");
    break;
  case 2:
    revLabel = PSTR("ECO2");
    break;
  case 3:
    revLabel = PSTR("ECO3");
    break;
  default:
    revLabel = PSTR("Unknown");
    break;
  }

  doc[F("heap_free")] = ESP.getFreeHeap();
  doc[F("heap_total")] = ESP.getHeapSize();
  doc[F("heap_min")] = ESP.getMinFreeHeap();
  doc[F("psram_free")] = ESP.getFreePsram();
  doc[F("psram_total")] = ESP.getPsramSize();
  doc[F("flash_size")] = ESP.getFlashChipSize();
  doc[F("flash_speed")] = ESP.getFlashChipSpeed();
  doc[F("flash_mode")] = ESP.getFlashChipMode();
  doc[F("LittleFS_total")] = LittleFS.totalBytes();
  doc[F("LittleFS_used")] = LittleFS.usedBytes();
  doc[F("chip_rev")] = revLabel;
  doc[F("sdk_version")] = String(esp_get_idf_version());
  doc[F("cpu_freq_mhz")] = ESP.getCpuFreqMHz();

  String output;
  serializeJsonPretty(doc, output);
  request->send(200, "application/json", output);
}

// ============================================================
// 🔹 Alarm Status
// ============================================================
void alarma(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  doc[F("alarmTriggered")] = g_settings.alarmTriggered;
  doc[F("active")] = (millis() - g_settings.reactivateAlarmCounter) < g_settings.deactivateAlarmTime;
  String output;
  serializeJson(doc, output);
  request->send(200, "application/json", output);
}

// ============================================================
// 🔹 Restart ESP
// ============================================================
void web_restart(AsyncWebServerRequest *request)
{
  Serial.println(F("ℹ️ Saving settings before restart..."));
  settingsSaveToFS();
  delay(200);
  request->send(200, "text/plain", F("ESP32 restarting..."));
  delay(3000);
  ESP.restart();
}

// ============================================================
// 🔹 LittleFS Upload Progress
// ============================================================
void handleFSStatus(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  float progress = 0;
  if (totalFSBytes > 0)
    progress = (float)currentFSProgress / totalFSBytes * 100.0;
  doc[F("progress")] = progress;
  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json);
}

// ============================================================
// 🔹 File Read / Delete / Format
// ============================================================
void readFile(AsyncWebServerRequest *request)
{
  if (!request->hasParam("name"))
  {
    request->send(400, "text/plain", F("❌ Missing parameter 'name'"));
    return;
  }
  String filename = request->getParam("name")->value();
  if (!filename.startsWith("/"))
    filename = "/" + filename;
  if (!LittleFS.exists(filename))
  {
    request->send(404, "text/plain", String(F("❌ File not found: ")) + filename);
    return;
  }
  File f = LittleFS.open(filename, "r");
  if (!f)
  {
    request->send(500, "text/plain", String(F("❌ Error opening file: ")) + filename);
    return;
  }
  String content;
  while (f.available())
    content += char(f.read());
  f.close();
  request->send(200, "text/plain", content);
}

void deleteFile(AsyncWebServerRequest *request)
{
  String filename;
  if (request->hasParam("name"))
    filename = request->getParam("name")->value();
  else if (request->hasParam("name", true))
    filename = request->getParam("name", true)->value();
  else
  {
    request->send(400, "text/plain", F("❌ Missing parameter 'name'"));
    return;
  }
  if (!LittleFS.exists(filename))
  {
    request->send(404, "text/plain", F("❌ File not found"));
    return;
  }
  if (LittleFS.remove(filename))
    request->send(200, "text/plain", String(F("✅ File deleted: ")) + filename);
  else
    request->send(500, "text/plain", F("❌ File deletion failed"));
}

void formatFS(AsyncWebServerRequest *request)
{
  if (LittleFS.format())
    request->send(200, "text/plain", F("✅ LittleFS formatted successfully!"));
  else
    request->send(500, "text/plain", F("❌ LittleFS format failed!"));
}

// ============================================================
// 🔹 Manual Fan Control
// ============================================================
void setManualPercentView(AsyncWebServerRequest *request)
{
  if (!request->hasParam("value"))
  {
    request->send(400, "text/plain", F("❌ Missing parameter 'value'"));
    return;
  }

  int val = constrain(request->getParam("value")->value().toInt(), 0, 100);
  if (g_settings.manual_percent != val || g_settings.manual_on != (val > 0))
  {
    g_settings.manual_percent = val;
    g_settings.manual_on = (val > 0);

    applyManualFan(g_settings.manual_on, g_settings.manual_percent);
    if (!settingsSaveToFS())
    {
      request->send(500, "text/plain", F("❌ Error saving settings!"));
      return;
    }

    char buf[128];
    snprintf_P(buf, sizeof(buf), PSTR("✅ Manual speed set to %d%% (PWM=%d)"), val, (int)sensorData.target_pwm);
    request->send(200, "text/plain", buf);
  }
  else
  {
    request->send(200, "text/plain", F("⚠️ Value unchanged"));
  }
}

// ============================================================
// 🔹 Favicon & Utilities
// ============================================================
void favicon(AsyncWebServerRequest *req)
{
  AsyncWebServerResponse *res = req->beginResponse(204);
  res->addHeader("Cache-Control", "public, max-age=86400");
  req->send(res);
}

// ============================================================
// 🔹 Get Example Settings
// ============================================================
void get_settings(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  doc[F("demoSetting")] = F("Example setting for testing JSON response");
  String output;
  serializeJson(doc, output);
  request->send(200, "application/json", output);
}

// ============================================================
// 🔹 Fan Page
// ============================================================
void fan(AsyncWebServerRequest *request)
{
  if (!isAuthenticated(request))
  {
    request->redirect("/login.html");
    return;
  }
  if (LittleFS.exists("/fan.html"))
  {
    request->send(LittleFS, "/fan.html", "text/html");
  }
  else
  {
    request->send(404, "text/plain", F("fan.html not found"));
  }
}

// ============================================================
// 🔹 Log Function Execution Page
// ============================================================
void log_function_execution(AsyncWebServerRequest *request)
{
  if (LittleFS.exists("/log_function_execution.html"))
  {
    request->send(LittleFS, "/log_function_execution.html", "text/html");
  }
  else
  {
    request->send(404, "text/plain", F("log_function_execution.html not found"));
  }
}

// ============================================================
// 🔹 Settings Page
// ============================================================
void settings(AsyncWebServerRequest *req)
{
  if (LittleFS.exists("/settings.html.gz"))
  {
    auto *res = req->beginResponse(LittleFS, "/settings.html.gz", "text/html");
    res->addHeader("Content-Encoding", "gzip");
    req->send(res);
  }
  else if (LittleFS.exists("/settings.html"))
  {
    req->send(LittleFS, "/settings.html", "text/html");
  }
  else
  {
    req->send(404, "text/plain", F("settings.html not found"));
  }
}

// ============================================================
// 🔹 API Default Settings
// ============================================================
void apiSettingsDefault(AsyncWebServerRequest *req)
{
  req->send(200, "application/json", settingsDefaultsJson());
}

// ============================================================
// 🔹 Send SMS (Async Task)
// ============================================================
void sendSmsView(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  JsonDocument doc;
  if (deserializeJson(doc, data, len))
  {
    request->send(400, "application/json", F("{\"error\":\"Invalid message\"}"));
    return;
  }
  if (!doc.containsKey("text"))
  {
    request->send(400, "application/json", F("{\"error\":\"Missing field 'text'\"}"));
    return;
  }

  String message = doc[F("text")].as<String>();
  Serial.println("📤 Sending SMS: " + message);

  // Async task pinned to core 1
  xTaskCreatePinnedToCore([](void *param)
                          {
    String msg = *((String *)param);
    sendSMS(msg);
    delete (String *)param;
    vTaskDelete(NULL); }, "sendSMS", 8192, new String(message), 1, NULL, 1);

  request->send(200, "application/json", F("{\"status\":\"OK\"}"));
}
