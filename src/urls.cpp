#include <Arduino.h>
#include "project_config.h"
#include <LittleFS.h>

// =======================================================
// 🌐 Initialize Asynchronous Web Server and Routes
// =======================================================
void initServer()
{
  // --- Dynamic API routes ---
  std::vector<Route> urlpatterns = {
      {"/deleteFile", HTTP_POST, deleteFile},
      {"/format_fs", HTTP_POST, formatFS},
      {"/meminfo", HTTP_GET, mem_info},
      {"/sysinfo", HTTP_GET, sysInfo},
      {"/get_settings", HTTP_GET, get_settings},
      {"/files_list", HTTP_GET, files_list},
      {"/wifi_info", HTTP_GET, wifi_info},
      {"/restart", HTTP_GET, web_restart},
      {"/alarma", HTTP_GET, alarma},
      {"/fs_status", HTTP_GET, handleFSStatus},
      {"/ota_status", HTTP_GET, ota_status},
      {"/log", HTTP_GET, log},
      {"/clear_log", HTTP_GET, handleClearLog},
      {"/log_function_execution", HTTP_GET, log_function_execution},
      {"/login", HTTP_GET, login},
      {"/logout", HTTP_GET, handleLogout},
      {"/readFile", HTTP_GET, readFile},
      {"/toggle_fan", HTTP_GET, toggleFan},
      {"/settings", HTTP_GET, settings},
      {"/api/settings/defaults", HTTP_GET, apiSettingsDefault},
      {"/set_mode", HTTP_GET, setMode},
      {"/fan", HTTP_GET, fan},
      {"/favicon.ico", HTTP_GET, favicon},
      {"/status", HTTP_GET, handleJson}};

  // --- GET: /api/settings ---
  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", settingsToJson(g_settings));
  });

  // --- GET: /api/sensors ---
  server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *req) {
    LOGI("📡 GET /api/sensors called");
    JsonDocument doc;
    doc["systemC"] = sensorData.systemC;
    doc["engineC"] = sensorData.engineC;
    doc["ts"] = sensorData.ts;
    doc["manual_percent"] = g_settings.manual_percent;
    doc["targetPercent"] = sensorData.targetPercent;
    doc["target_pwm"] = sensorData.target_pwm;
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
  });

  // --- POST: /api/settings ---
  server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
              static String body;
              if (index == 0)
                body = "";
              body.concat((const char *)data, len);

              if (index + len == total)
              {
                bool ok = settingsFromJson(body);
                if (ok)
                {
                  settingsApply();
                  ok = settingsSaveToFS();
                }
                req->send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
              }
            });

  // --- POST: /send_sms ---
  server.on("/send_sms", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, sendSmsView);

  // --- POST: /set_pwm_freq ---
  server.on("/set_pwm_freq", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("freq"))
    {
      request->send(400, "text/plain", "❌ Missing parameter 'freq'");
      return;
    }
    int newFreq = request->getParam("freq")->value().toInt();
    if (newFreq <= 0)
    {
      request->send(400, "text/plain", "⚠️ Invalid frequency value");
      return;
    }

    uint32_t realFreq = ledcChangeFrequency(g_settings.pwm_channel, newFreq, g_settings.pwm_resolution_bits);
    if (realFreq == 0)
    {
      request->send(500, "text/plain", "❌ Error changing frequency");
      return;
    }

    g_settings.pwm_freq_hz = realFreq;
    settingsSaveToFS();

    static char buf[256];
    snprintf(buf, sizeof(buf), "✅ Frequency set: %lu Hz, resolution: %u bits",
             (unsigned long)realFreq, g_settings.pwm_resolution_bits);
    request->send(200, "text/plain", buf);
  });

  // --- POST: /set_manual_percent ---
  server.on("/set_manual_percent", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("value"))
    {
      request->send(400, "text/plain", "❌ Missing parameter 'value'");
      return;
    }

    int val = request->getParam("value")->value().toInt();
    val = constrain(val, 0, 100);

    g_settings.manual_percent = val;
    g_settings.manual_on = (val > 0);
    settingsSaveToFS();

    static char buf[256];
    snprintf(buf, sizeof(buf), "✅ Manual speed set to %d%% (PWM=%d)", val, sensorData.target_pwm);
    request->send(200, "text/plain", buf);
  });

  // --- POST: /cmd (serial command passthrough) ---
  server.on("/cmd", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
              String input((char *)data, len);
              input.trim();

              String out = processCommand(input);
              LOGI("> %s", input.c_str());
              LOGI("%s", out.c_str());

              req->send(200, "text/plain", out);
            });

  // --- POST: /firmware_update (OTA firmware upload) ---
  server.on("/firmware_update", HTTP_POST, [](AsyncWebServerRequest *request) {
    LOGI("-> OTA update route /firmware_update");
    request->send(200, "text/plain", "Firmware updated successfully!");
  },
            handleUpdateFirmware);

  // --- POST: /updatefs (LittleFS OTA upload) ---
  server.on("/updatefs", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "✅ LittleFS updated successfully!");
    LOGI("LittleFS upload finished");
  },
            handleUpdateLittleFS);

  // --- Dynamic routes ---
  for (auto &route : urlpatterns)
  {
    LOGI("✅ Added route: %s (method=%d)", route.path.c_str(), route.method);
    server.on(route.path.c_str(), route.method, route.view);
  }

  // --- Static routes ---
  std::vector<StaticRoute> staticroutes = {
      {"/alert.mp3", "/alert.mp3", "audio/mpeg"},
      {"/log_page", "/log.html", "text/html"},
      {"/about", "/about.html", "text/html"},
      {"/despre", "/despre.html", "text/html"},
      {"/servicii", "/servicii.html", "text/html"},
      {"/contact", "/contact.html", "text/html"},
      {"/bootstrap.min.css", "/bootstrap.min.css", "text/css"},
      {"/bootstrap.bundle.min.js", "/bootstrap.bundle.min.js", "application/javascript"},
      {"/script.js", "/script.js", "application/javascript"},
      {"/navbar", "/navbar.html", "text/html"},
      {"/wifi", "/wifi_info.html", "text/html"},
      {"/style.css", "/style.css", "text/css"},
      {"/dashboard", "/dashboard.html", "text/html"},
      {"/firmware_update", "/firmware_update.html", "text/html"},
      {"/alarm.wav", "/alarm.wav", "audio/wav"}};

  for (auto &route : staticroutes)
  {
    server.serveStatic(route.url.c_str(), LittleFS, route.file.c_str())
        .setCacheControl("max-age=86400");
  }

  // --- Root (default page) ---
  server.serveStatic("/", LittleFS, "/")
      .setDefaultFile("index.html")
      .setCacheControl("max-age=86400");

  // --- 404 handler ---
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "404 - Not found");
  });

  // --- Start server ---
  server.begin();
  LOGI("[HTTP] Web server started successfully");
}
