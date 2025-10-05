#include <Arduino.h>
#include "project_config.h"
#include <WiFi.h>
#include <Preferences.h>
#include "time.h"

// NTP configuration
const char *ntpServer = "pool.ntp.org";
const char *TZ_IRELAND = "GMT0BST,M3.5.0/1,M10.5.0/2";  // Ireland timezone

// ---------------------------------------------------------
// 🕓 Detect if Daylight Saving Time (DST) is active
// ---------------------------------------------------------
bool isDST(struct tm *timeinfo)
{
    int year = timeinfo->tm_year + 1900;
    int month = timeinfo->tm_mon + 1;
    int day = timeinfo->tm_mday;

    // Before March or after October → no DST
    if (month < 3 || month > 10)
        return false;

    // Between April and September → always DST
    if (month > 3 && month < 10)
        return true;

    // Compute last Sunday of March and October
    int lastSundayMarch = 31 - ((5 * year / 4 + 4) % 7);
    int lastSundayOctober = 31 - ((5 * year / 4 + 1) % 7);

    if (month == 3)
        return (day >= lastSundayMarch);
    if (month == 10)
        return (day < lastSundayOctober);

    return false;
}

// ---------------------------------------------------------
// 🌐 Try to get time from a given NTP server
// ---------------------------------------------------------
bool tryGetTime(const char *serverName, long gmtOffset, int dstOffset)
{
    configTzTime(TZ_IRELAND, serverName);
    delay(2000);

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return false;

    return true;
}

// ---------------------------------------------------------
// 💾 Save current time to NVS (persistent storage)
// ---------------------------------------------------------
void saveTimeToNVS()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    prefs.begin("time", false);
    prefs.putInt("year", timeinfo.tm_year + 1900);
    prefs.putInt("month", timeinfo.tm_mon + 1);
    prefs.putInt("day", timeinfo.tm_mday);
    prefs.putInt("hour", timeinfo.tm_hour);
    prefs.putInt("minute", timeinfo.tm_min);
    prefs.putInt("second", timeinfo.tm_sec);
    prefs.end();

    Serial.println("💾 Time saved to NVS.");
}

// ---------------------------------------------------------
// 💡 Load last saved time from NVS (if available)
// ---------------------------------------------------------
bool loadTimeFromNVS(struct tm &timeinfo)
{
    prefs.begin("time", true);
    int year = prefs.getInt("year", 0);
    int month = prefs.getInt("month", 0);
    int day = prefs.getInt("day", 0);
    int hour = prefs.getInt("hour", 0);
    int minute = prefs.getInt("minute", 0);
    int second = prefs.getInt("second", 0);
    prefs.end();

    if (year == 0)
        return false;

    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    return true;
}

// ---------------------------------------------------------
// 🧭 Print the current local time
// ---------------------------------------------------------
void printLocalTime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("❌ Failed to obtain time");
        return;
    }

    Serial.printf("🕒 Current time: %02d:%02d:%02d  %02d/%02d/%04d\n",
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                  timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
}

// ---------------------------------------------------------
// ⏳ Main time initialization sequence
// ---------------------------------------------------------
void setupTime()
{
    struct tm timeinfo;

    Serial.println("⏳ Attempting full time sync...");

    // 1️⃣ Try local router as NTP server
    IPAddress routerIP = WiFi.gatewayIP();
    String routerStr = routerIP.toString();
    Serial.printf("🌐 Trying router NTP at %s\n", routerStr.c_str());
    if (tryGetTime(routerStr.c_str(), 0, 0))
    {
        Serial.println("✅ Time synchronized from router");
        saveTimeToNVS();
        printLocalTime();
        return;
    }

    // 2️⃣ Try public NTP pool
    Serial.println("🌍 Trying public NTP pool...");
    if (tryGetTime(ntpServer, 0, 0))
    {
        Serial.println("✅ Time synchronized from internet");
        saveTimeToNVS();

        getLocalTime(&timeinfo);
        bool dst = isDST(&timeinfo);

        Serial.printf("🇮🇪 DST active: %s\n", dst ? "Yes (+1h)" : "No");
        configTime(0, dst ? 3600 : 0, ntpServer);

        delay(1000);
        printLocalTime();
        return;
    }

    // 3️⃣ Fallback to last saved time from NVS
    Serial.println("⚠️ Using last saved time from NVS (if available)...");
    if (loadTimeFromNVS(timeinfo))
    {
        Serial.printf("🕒 Recovered saved time: %02d:%02d:%02d  %02d/%02d/%04d\n",
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                      timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    }
    else
    {
        Serial.println("❌ No valid time available in NVS.");
    }
}
