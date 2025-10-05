#include <Arduino.h>
#include "project_config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// =========================================================
// 🧩 Convert JSON document → SystemSettings struct
// =========================================================
void fillFromJson(SystemSettings &s, const JsonDocument &doc)
{
    // --- General ---
    if (doc["hostname"].is<String>())
        s.hostname = (const char *)doc["hostname"];
    s.log_level = doc["log_level"] | s.log_level;
    s.telemetry_enabled = doc["telemetry_enabled"] | s.telemetry_enabled;
    s.fs_format_on_fail = doc["fs_format_on_fail"] | s.fs_format_on_fail;

    // --- Network & OTA ---
    if (doc["wifi_ssid"].is<String>())
        s.wifi_ssid = (const char *)doc["wifi_ssid"];
    if (doc["wifi_pass"].is<String>())
        s.wifi_pass = (const char *)doc["wifi_pass"];
    s.ota_enabled = doc["ota_enabled"] | s.ota_enabled;
    if (doc["ota_url"].is<String>())
        s.ota_url = (const char *)doc["ota_url"];

    // --- Temperature & ADC ---
    s.min_rotation_temp = doc["min_rotation_temp"] | s.min_rotation_temp;
    s.max_rotation_temp = doc["max_rotation_temp"] | s.max_rotation_temp;
    s.system_temp_alert = doc["system_temp_alert"] | s.system_temp_alert;
    s.temp_sample_interval_ms = doc["temp_sample_interval_ms"] | s.temp_sample_interval_ms;
    s.adc_samples = doc["adc_samples"] | s.adc_samples;
    if (doc["temp_sensor_type"].is<String>())
        s.temp_sensor_type = (const char *)doc["temp_sensor_type"];

    // --- Fan Control ---
    if (doc["fan_mode"].is<String>())
    {
        String mode = doc["fan_mode"].as<String>();
        if (mode.equalsIgnoreCase(F("AUTO")))
            s.fan_mode = FanMode::AUTO;
        else if (mode.equalsIgnoreCase(F("MANUAL")))
            s.fan_mode = FanMode::MANUAL;
        else
            Serial.printf_P(PSTR("⚠️ Invalid fan_mode: %s\n"), mode.c_str());
    }

    s.fan_control_interval = doc["fan_control_interval"] | s.fan_control_interval;
    s.manual_percent = doc["manual_percent"] | s.manual_percent;
    s.fan_start_boost_ms = doc["fan_start_boost_ms"] | s.fan_start_boost_ms;
    s.pwm_freq_hz = doc["pwm_freq_hz"] | s.pwm_freq_hz;
    s.pwm_channel = doc["pwm_channel"] | s.pwm_channel;
    s.pwm_resolution_bits = doc["pwm_resolution_bits"] | s.pwm_resolution_bits;
    s.invert_pwm = doc["invert_pwm"] | s.invert_pwm;
    s.manual_on = doc["manual_on"] | s.manual_on;

    // --- UI Mapping & Alarm ---
    s.ui_system_min = doc["ui_system_min"] | s.ui_system_min;
    s.ui_system_max = doc["ui_system_max"] | s.ui_system_max;
    s.ui_engine_min = doc["ui_engine_min"] | s.ui_engine_min;
    s.ui_engine_max = doc["ui_engine_max"] | s.ui_engine_max;
    s.deactivateAlarmTime = doc["deactivateAlarmTime"] | s.deactivateAlarmTime;
}

// =========================================================
// 📂 Load settings from LittleFS
// =========================================================
SettingsLoadStatus settingsLoadFromFS()
{
    if (!LittleFS.exists(fileSettingsPath))
    {
        LOGW("⚠️ Settings file not found — creating defaults...");
        return settingsSaveToFS() ? LOAD_DEFAULTS_SAVED : LOAD_FILE_OPEN_FAIL;
    }

    File f = LittleFS.open(fileSettingsPath, "r");
    if (!f)
    {
        LOGE("❌ Unable to open settings file for reading!");
        return LOAD_FILE_OPEN_FAIL;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err)
    {
        LOGE("❌ JSON parse error in settings: %s", err.c_str());
        LittleFS.remove(fileSettingsPath); // curăță fișier corupt
        settingsSaveToFS();                // recreează cu defaulturi
        return LOAD_JSON_ERROR;
    }

    fillFromJson(g_settings, doc);

    LOGI("✅ Settings loaded from FS successfully");
#if LOG_LEVEL >= 4
    serializeJsonPretty(doc, Serial);
    Serial.println();
#endif

    return LOAD_OK;
}

// =========================================================
// ⚙️ Load settings and print status
// =========================================================
void loadSettings()
{
    SettingsLoadStatus status = settingsLoadFromFS();

    switch (status)
    {
    case LOAD_OK:
        Serial.println(F("✔ Settings loaded successfully"));
        break;
    case LOAD_DEFAULTS_SAVED:
        Serial.println(F("⚠ Default settings created and saved"));
        break;
    case LOAD_FILE_NOT_FOUND:
        Serial.println(F("❌ File not found (unexpected branch)"));
        break;
    case LOAD_FILE_OPEN_FAIL:
        Serial.println(F("❌ Could not open settings file"));
        break;
    case LOAD_JSON_ERROR:
        Serial.println(F("❌ Corrupted settings file"));
        break;
    }
}

// =========================================================
// 🔧 Apply settings to hardware
// =========================================================
void settingsApply()
{
    // Reinitialize PWM with new configuration
    reinitPwm(
        g_settings.pwm_channel,
        g_settings.pwm_freq_hz,
        g_settings.pwm_resolution_bits,
        g_settings.invert_pwm);

    // Apply manual fan state (if active)
    applyManualFan(g_settings.manual_on, static_cast<uint8_t>(g_settings.manual_percent));

    LOGI("⚙️ Settings applied (PWM + fan updated)");
}
