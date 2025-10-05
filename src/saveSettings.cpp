#include <Arduino.h>
#include "project_config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Global system settings structure
SystemSettings g_settings;

// =======================================================
// 🧩 Fill JSON Document from SystemSettings structure
// =======================================================
void fillJsonFrom(const SystemSettings &s, JsonDocument &doc)
{
    // --- General ---
    doc["hostname"] = s.hostname;
    doc["log_level"] = s.log_level;
    doc["telemetry_enabled"] = s.telemetry_enabled;
    doc["fs_format_on_fail"] = s.fs_format_on_fail;

    // --- Network & OTA ---
    doc["wifi_ssid"] = s.wifi_ssid;
    doc["wifi_pass"] = s.wifi_pass;
    doc["ota_enabled"] = s.ota_enabled;
    doc["ota_url"] = s.ota_url;

    // --- Temperature & ADC ---
    doc["min_rotation_temp"] = s.min_rotation_temp;
    doc["max_rotation_temp"] = s.max_rotation_temp;
    doc["system_temp_alert"] = s.system_temp_alert;
    doc["temp_sample_interval_ms"] = s.temp_sample_interval_ms;
    doc["adc_samples"] = s.adc_samples;
    doc["temp_sensor_type"] = s.temp_sensor_type;
    doc["alarmTriggered"] = s.alarmTriggered;
    doc["reactivateAlarmCounter"] = s.reactivateAlarmCounter;

    // --- Fan Control ---
    switch (s.fan_mode)
    {
    case FanMode::AUTO:
        doc["fan_mode"] = "AUTO";
        break;
    case FanMode::MANUAL:
        doc["fan_mode"] = "MANUAL";
        break;
    default:
        doc["fan_mode"] = "UNKNOWN";
        LOGW("⚠️ fillJsonFrom(): fan_mode UNKNOWN -> set as 'UNKNOWN'");
        break;
    }

    doc["fan_control_interval"] = s.fan_control_interval;
    doc["fan_start_boost_ms"] = s.fan_start_boost_ms;
    doc["pwm_freq_hz"] = s.pwm_freq_hz;
    doc["pwm_channel"] = s.pwm_channel;
    doc["pwm_resolution_bits"] = s.pwm_resolution_bits;
    doc["invert_pwm"] = s.invert_pwm;
    doc["manual_percent"] = s.manual_percent;
    doc["manual_on"] = s.manual_on;

    // --- UI Mapping ---
    doc["ui_system_min"] = s.ui_system_min;
    doc["ui_system_max"] = s.ui_system_max;
    doc["ui_engine_min"] = s.ui_engine_min;
    doc["ui_engine_max"] = s.ui_engine_max;
    doc["deactivateAlarmTime"] = s.deactivateAlarmTime;
}

// =======================================================
// 💾 Save current settings to LittleFS
// =======================================================
bool settingsSaveToFS()
{
    JsonDocument newDoc;
    fillJsonFrom(g_settings, newDoc);

    // --- Check if an existing settings file already exists ---
    if (LittleFS.exists(fileSettingsPath))
    {
        File fOld = LittleFS.open(fileSettingsPath, "r");
        if (fOld)
        {
            JsonDocument oldDoc;
            DeserializationError err = deserializeJson(oldDoc, fOld);
            if (!err)
            {
                // Compare old and new JSONs as strings
                String newStr, oldStr;
                serializeJson(newDoc, newStr);
                serializeJson(oldDoc, oldStr);
                if (newStr == oldStr)
                {
                    LOGI("⚠️ Settings unchanged — no write needed.");
                    fOld.close();
                    return true;
                }
            }
            fOld.close();
        }
    }

    // --- Write new settings to file ---
    File fNew = LittleFS.open(fileSettingsPath, "w");
    if (!fNew)
    {
        LOGE("❌ Unable to open settings file for writing!");
        return false;
    }

    bool ok = serializeJsonPretty(newDoc, fNew) > 0;
    fNew.close();

    if (ok)
        LOGI("✅ Settings saved successfully.");
    else
        LOGE("❌ Failed to write settings!");

    return ok;
}
