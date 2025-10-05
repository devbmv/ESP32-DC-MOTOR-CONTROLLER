#include <Arduino.h>
#include "project_config.h"
#include <WiFi.h>
#include <Preferences.h>
#include <vector>

struct WifiCred {
  String ssid;
  String password;
};

std::vector<WifiCred> savedNetworks;

// =======================================================
// 🔹 Load saved Wi-Fi networks from NVS
// =======================================================
void loadSavedNetworks()
{
  prefs.begin("wifi_networks", true);
  int count = prefs.getInt("count", 0);
  savedNetworks.clear();

  for (int i = 0; i < count; i++)
  {
    String keySsid = "ssid" + String(i);
    String keyPass = "pass" + String(i);
    String ssid = prefs.getString(keySsid.c_str(), "");
    String pass = prefs.getString(keyPass.c_str(), "");
    if (ssid.length() > 0)
      savedNetworks.push_back({ssid, pass});
  }
  prefs.end();

  Serial.printf("📋 Loaded %d saved Wi-Fi networks.\n", savedNetworks.size());
}

// =======================================================
// 🔹 Save or update a Wi-Fi network in NVS
// =======================================================
void saveNetwork(const String &ssid, const String &password)
{
  prefs.begin("wifi_networks", false);
  int count = prefs.getInt("count", 0);

  // If it already exists → update password
  for (int i = 0; i < count; i++)
  {
    String keySsid = "ssid" + String(i);
    if (prefs.getString(keySsid.c_str(), "") == ssid)
    {
      String keyPass = "pass" + String(i);
      prefs.putString(keyPass.c_str(), password.c_str());
      prefs.end();
      Serial.println("💾 Updated existing Wi-Fi network.");
      return;
    }
  }

  // Add new network
  String keySsid = "ssid" + String(count);
  String keyPass = "pass" + String(count);
  prefs.putString(keySsid.c_str(), ssid.c_str());
  prefs.putString(keyPass.c_str(), password.c_str());
  prefs.putInt("count", count + 1);
  prefs.end();

  Serial.printf("💾 Saved new Wi-Fi network: %s\n", ssid.c_str());
}

// =======================================================
// 🔹 Try connecting to a specific network
// =======================================================
bool tryConnect(const String &ssid, const String &password)
{
  Serial.printf("🔌 Trying Wi-Fi: %s ...\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.printf("✅ Connected to %s\n", ssid.c_str());
    Serial.print("📶 IP: ");
    Serial.println(WiFi.localIP());

    // Save this network as last used
    prefs.begin("wifi_networks", false);
    prefs.putString("last_ssid", ssid.c_str());
    prefs.putString("last_pass", password.c_str());
    prefs.end();

    return true;
  }

  Serial.println("❌ Failed to connect.");
  return false;
}

// =======================================================
// 🔹 Try connecting to all saved networks
// =======================================================
bool connectSavedNetworks()
{
  loadSavedNetworks();

  // First, try the last successful network
  prefs.begin("wifi_networks", true);
  String lastSsid = prefs.getString("last_ssid", "");
  String lastPass = prefs.getString("last_pass", "");
  prefs.end();

  if (lastSsid.length() > 0)
  {
    Serial.printf("⭐ Trying last successful network: %s\n", lastSsid.c_str());
    if (tryConnect(lastSsid, lastPass))
      return true;
  }

  // Try all saved networks
  for (auto &net : savedNetworks)
  {
    if (tryConnect(net.ssid, net.password))
      return true;
  }

  Serial.println("❌ No saved Wi-Fi networks available or reachable.");
  return false;
}

// =======================================================
// 🔹 Automatic Wi-Fi initialization
// =======================================================
void initWiFi()
{
  Serial.println("📡 Initializing Wi-Fi...");
  if (connectSavedNetworks())
    return;

  Serial.println("📶 Trying default credentials...");
  if (tryConnect(WIFI_SSID, WIFI_PASSWORD))
  {
    saveNetwork(WIFI_SSID, WIFI_PASSWORD);
    return;
  }

  Serial.println("⚠️ Could not connect to any network!");
}
