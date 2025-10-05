#include <cstdarg>
#include "project_config.h"

// ==========================================================
// 🧠 Circular runtime log buffer
// ==========================================================
String logBufferSetup;                         // Boot logs (first seconds)
char logBufferRuntime[LOG_BUFFER_SIZE];        // Circular buffer for runtime logs
size_t logHead = 0;                            // Current write index

// ==========================================================
// 📜 Add message to setup log (before main runtime)
// ==========================================================
void addLogSetup(const String &msg)
{
    logBufferSetup += msg + "\n";
}

// ==========================================================
// 📜 Add message to circular runtime log
// ==========================================================
void addLog(const char *msg)
{
    size_t len = strlen(msg);
    if (len + 2 >= LOG_BUFFER_SIZE)
        return; // Ignore too-long messages

    for (size_t i = 0; i < len; i++)
    {
        logBufferRuntime[logHead] = msg[i];
        logHead = (logHead + 1) % LOG_BUFFER_SIZE;
    }

    // Append newline and move head
    logBufferRuntime[logHead] = '\n';
    logHead = (logHead + 1) % LOG_BUFFER_SIZE;
}

// Overload for String
void addLog(const String &msg)
{
    addLog(msg.c_str());
}

// ==========================================================
// 🪵 Main logging function (printf-style)
// ==========================================================
void logMessage(const char *level, const char *fmt, ...)
{
    static char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Get timestamp (NTP or millis)
    String ts = getDateTime();
    if (ts == "N/A" || ts.length() < 5)
    {
        ts = String(ts_ms());
    }

    // Compose final log line
    String msg = "[" + String(level) + "][" + ts + "] " + buf;

    // Output to serial
    Serial.println(msg);

    // Store in appropriate buffer
    if (millis() < 10000) // Boot logs (first 10s)
        addLogSetup(msg);
    else
        addLog(msg.c_str());
}

// ==========================================================
// 📋 Retrieve entire log buffer as String
// ==========================================================
String getLog()
{
    String out = logBufferSetup + "\n--- Live Runtime ---\n";

    size_t idx = logHead;
    for (size_t i = 0; i < LOG_BUFFER_SIZE; i++)
    {
        char c = logBufferRuntime[idx];
        if (c != 0)
            out += c;
        idx = (idx + 1) % LOG_BUFFER_SIZE;
    }

    return out;
}
