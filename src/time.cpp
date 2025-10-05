#include "project_config.h"

static tm timeinfo;        // Cached time structure
static bool timeSynced = false;  // Indicates if NTP sync was successful

// =======================================================
// 🕒 Initialize NTP time synchronization
// =======================================================
void initTime(const char *ntpServer, long gmtOffset_sec, int daylightOffset_sec) {
    LOGI("⏳ Initializing time with NTP server: %s", ntpServer);

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    if (!getLocalTime(&timeinfo, 200)) {
        LOGW("⚠️ NTP not yet available — falling back to millis() until sync...");
        timeSynced = false;
        return;
    }

    timeSynced = true;
    LOGI("✅ Time synchronized: %s", getDateTime().c_str());
}

// =======================================================
// 📅 Return current time as a tm struct
// =======================================================
tm getTimeStruct() {
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo;
}

// =======================================================
// ⏰ Return current date and time as formatted string
// =======================================================
String getDateTime() {
    time_t now;
    time(&now);
    tm localTime;
    localtime_r(&now, &localTime);

    // Fallback if NTP not yet synchronized
    if ((localTime.tm_year + 1900) < 2020) {
        return String(ts_ms());
    }

    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &localTime);
    return String(buf);
}

// =======================================================
// 🕑 Return only current time (HH:MM:SS)
// =======================================================
String getTimeOnly() {
    time_t now;
    time(&now);
    tm localTime;
    localtime_r(&now, &localTime);

    if ((localTime.tm_year + 1900) < 2020) {
        return String(ts_ms());
    }

    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", &localTime);
    return String(buf);
}

// =======================================================
// 📆 Return only current date (YYYY-MM-DD)
// =======================================================
String getDateOnly() {
    time_t now;
    time(&now);
    tm localTime;
    localtime_r(&now, &localTime);

    if ((localTime.tm_year + 1900) < 2020) {
        return "1970-fallback";
    }

    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &localTime);
    return String(buf);
}
