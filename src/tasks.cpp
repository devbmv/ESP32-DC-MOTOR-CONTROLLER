#include <Arduino.h>
#include "project_config.h"
#include <TaskScheduler.h>

// =======================================================
// 🌡️ Task: Sensor Reading Loop
// Periodically reads engine and system temperatures.
// The most recent data is pushed to the sensorDataQueue.
// =======================================================
void taskSensors(void *) {
    for (;;) {
        read_engine_temp();       // Read Dallas sensor (engine temp)
        read_system_temp();       // Read analog NTC (system temp)
        xQueueOverwrite(sensorDataQueue, &sensorData); // Keep only latest values
        vTaskDelay(pdMS_TO_TICKS(10));                 // Run every 10 ms
    }
}

// =======================================================
// 🌀 Task: Fan Control Logic
// Controls the fan speed automatically or manually
// depending on the current mode and temperature readings.
// =======================================================
void taskControl(void *pvParameters) {
    SystemInfo s{};                    // Local copy of sensor data
    static uint32_t fan_timer = millis();
    static int lastCmdPct = 0;

    for (;;) {
        // Temporary variable for heap logging (if needed later)
        static uint32_t heapTimer = millis();

        // Read the latest sensor data from the queue
        if (xQueueReceive(sensorDataQueue, &s, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Flush queue to ensure we always have the newest data
            while (xQueueReceive(sensorDataQueue, &s, 0) == pdTRUE) {}
        }

        // Run control logic at configured intervals
        if (millis() - fan_timer >= g_settings.fan_control_interval) {
            fan_timer = millis();

            // --- Safety Check ---
            if (s.systemC > g_settings.system_temp_alert) {
                sensorData.targetPercent = 0;
                sensorData.target_pwm = 0;
            }

            // --- Automatic Mode ---
            else if (g_settings.fan_mode == FanMode::AUTO) {
                float ratio = (s.engineC - (float)g_settings.min_rotation_temp) /
                              ((float)g_settings.max_rotation_temp - (float)g_settings.min_rotation_temp);
                ratio = constrain(ratio, 0.0f, 1.0f);

                sensorData.targetPercent = lroundf(ratio * 100.0f);
                sensorData.target_pwm = lroundf(ratio * pwm_max()); // pwm_max() = 4095 at 12 bits
            }

            // --- Manual Mode ---
            else {
                sensorData.targetPercent = g_settings.manual_on ? g_settings.manual_percent : 0;
                sensorData.target_pwm = lroundf((sensorData.targetPercent / 100.0f) * pwm_max());
            }

            // Apply new PWM value
            ledcWrite(g_settings.pwm_channel, sensorData.target_pwm);

            // Log unified fan status
            LOGI("[FAN] mode=%s pct=%d pwm=%d/%d",
                 (g_settings.fan_mode == FanMode::AUTO ? "AUTO" : "MANUAL"),
                 sensorData.targetPercent,
                 sensorData.target_pwm,
                 pwm_max());
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // Loop every 20 ms
    }
}
