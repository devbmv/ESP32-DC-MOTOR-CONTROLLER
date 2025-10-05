#include <Arduino.h>
#include "project_config.h"
#include <Preferences.h>

// Global NVS handler
Preferences prefs;

// ======================================================
// 💾 Save a generic value into NVS (templated)
// ------------------------------------------------------
// Supports: int, float, bool, String, or any custom struct
// ======================================================
template <typename T>
void saveNVS(const char *ns, const char *key, const T &value)
{
    prefs.begin(ns, false); // open namespace for writing

    if constexpr (std::is_same<T, int>::value)
        prefs.putInt(key, value);

    else if constexpr (std::is_same<T, float>::value)
        prefs.putFloat(key, value);

    else if constexpr (std::is_same<T, bool>::value)
        prefs.putBool(key, value);

    else if constexpr (std::is_same<T, String>::value)
        prefs.putString(key, value);

    else
        prefs.putBytes(key, &value, sizeof(T));

    prefs.end();
    Serial.printf("💾 [NVS] Saved key='%s' (size=%u bytes)\n", key, sizeof(T));
}

// ======================================================
// 📤 Load a generic value from NVS (templated)
// ------------------------------------------------------
// Returns the stored value or the default if not found
// ======================================================
template <typename T>
T loadNVS(const char *ns, const char *key, const T &defValue)
{
    prefs.begin(ns, true); // open namespace read-only

    T value = defValue;

    if constexpr (std::is_same<T, int>::value)
        value = prefs.getInt(key, defValue);

    else if constexpr (std::is_same<T, float>::value)
        value = prefs.getFloat(key, defValue);

    else if constexpr (std::is_same<T, bool>::value)
        value = prefs.getBool(key, defValue);

    else if constexpr (std::is_same<T, String>::value)
        value = prefs.getString(key, defValue);

    else
    {
        size_t size = prefs.getBytesLength(key);
        if (size == sizeof(T))
            prefs.getBytes(key, &value, sizeof(T));
        else
            Serial.printf("⚠️ [NVS] Key='%s' not found or size mismatch\n", key);
    }

    prefs.end();
    return value;
}
