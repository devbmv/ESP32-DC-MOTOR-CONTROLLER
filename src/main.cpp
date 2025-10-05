#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "project_config.h"
#include "esp_ota_ops.h"
#include "esp_core_dump.h"

/**
 * @brief Main setup routine. Initializes all modules:
 * UART, commands, time, Wi-Fi, ADC, CAN, RGB, FS, tasks, etc.
 */
void setup()
{
    initUart();
    initCommands();
    setupTime();

    LOGI("Boot start, heap=%.1f KB", heap_kb());

    // --- Hardware initialization ---
    initPins();
    LOGI("Pins init OK");

    initADC();
    LOGI("ADC init OK");

    if (canInit(500000))
    {
        setLed(0, 0, 12);
        LOGI("CAN/TWAI @500k OK");
    }
    else
    {
        LOGE("CAN init FAIL");
    }

    clearCoreDumpOnce();
    LOGI("CoreDump partition cleared once (if needed)");

    // --- OTA partitions info ---
    auto run = esp_ota_get_running_partition();
    auto boot = esp_ota_get_boot_partition();
    LOGI("Running: %s | BootSel: %s", run ? run->label : "?", boot ? boot->label : "?");

    // --- Wi-Fi setup ---
    initWiFi();
    LOGI("WiFi connected=%s, SSID=%s, IP=%s, RSSI=%d",
         WiFi.isConnected() ? "true" : "false",
         WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());

    // --- NTP time ---
    initTime("pool.ntp.org", 0, 3600);

    // --- Sensors & peripherals ---
    initDallas();
    LOGI("Dallas init OK");

    initRGB();
    LOGI("RGB init OK");

    initFs();
    LOGI("LittleFS mount OK");

    // --- Load & apply settings ---
    loadSettings();
    settingsApply();

    // --- FreeRTOS primitives ---
    sensorDataQueue = xQueueCreate(1, sizeof(SystemInfo));
    CHECK_AND_LOG(sensorDataQueue, ("Queue sensorDataQueue OK"), ("Queue sensorDataQueue FAIL"));

    mtxState = xSemaphoreCreateMutex();
    CHECK_AND_LOG(mtxState, ("mtxState OK"), ("mtxState FAIL"));

    egFlags = xEventGroupCreate();
    CHECK_AND_LOG(egFlags, ("egFlags OK"), ("egFlags FAIL"));

    // --- Heartbeat LED ---
    tHeartbeat = xTimerCreate("hb", pdMS_TO_TICKS(500), pdTRUE, nullptr, hbCb);
    if (tHeartbeat)
    {
        xTimerStart(tHeartbeat, 0);
        LOGI("Heartbeat timer started");
    }
    else
    {
        LOGE("Heartbeat timer create FAIL");
    }

    // --- Create tasks ---
    BaseType_t ok;

    ok = xTaskCreatePinnedToCore(taskSensors, "Sensors", 3072, nullptr, 2, &hSensors, 0);
    LOGI("taskSensors %s", ok == pdPASS ? "OK" : "FAIL");

    ok = xTaskCreatePinnedToCore(taskControl, "Control", 3072, nullptr, 2, &hControl, 1);
    LOGI("taskControl %s", ok == pdPASS ? "OK" : "FAIL");

    ok = xTaskCreatePinnedToCore(taskCan, "CAN", 2048, nullptr, 1, &hCan, 1);
    LOGI("taskCan %s", ok == pdPASS ? "OK" : "FAIL");

    // --- Web server ---
    initServer();
    LOGI("[HTTP] Server started");

    LOGI("Setup done, heap=%.1f KB", heap_kb());
}

/**
 * @brief Main loop. Handles serial commands and keeps system alive.
 */
void loop()
{
    processSerialCommands();
    vTaskDelay(pdMS_TO_TICKS(50)); // cooperative delay for FreeRTOS
}
