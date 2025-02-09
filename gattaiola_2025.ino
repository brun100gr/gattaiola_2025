#define CONFIG_ESP32_SPIRAM_SUPPORT 0
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "html_wifi_configuration.h"  // HTML content for the configuration page
#include "secrets.h"  // File containing predefined WiFi networks (SSID and password)
#include "html_set_alarm.h"  // HTML content for the alarm configuration page

#define MAX_RETRIES 5  // Maximum number of connection retries before starting AP mode

const int SWITCH_PIN = 4;       // GPIO collegato allo switch

WebServer server(80);  // Web server running on port 80
Preferences preferences;  // Used for persistent storage of WiFi credentials

// List of predefined WiFi networks from secrets.h
const int wifiCount = sizeof(wifiList) / sizeof(wifiList[0]);

// Structure to store WiFi credentials
struct WiFiConfig {
  char ssid[32];  // SSID (max 32 characters)
  char password[64];  // Password (max 64 characters)
} wifiConfig;

int connectionRetries = 0;  // Counter for connection retries
bool apMode = false;  // Flag to indicate if the device is in Access Point mode
const char AP_SSID_PREFIX[] = "ESP32-Config-";  // Prefix for the AP SSID

// Variables for the alarm (initially set to 7:30)
int alarmHour = 7;
int alarmMinute = 30;

// Function to save WiFi credentials to persistent storage
void saveConfig() {
  Serial.println("Saving WiFi configuration...");
  preferences.begin("wifi", false);  // Open preferences namespace "wifi" in read/write mode
  preferences.putString("ssid", wifiConfig.ssid);  // Save SSID
  preferences.putString("password", wifiConfig.password);  // Save password
  preferences.end();  // Close preferences
  Serial.println("WiFi configuration saved.");
}

// Function to load WiFi credentials from persistent storage
void loadConfig() {
  Serial.println("Loading WiFi configuration...");
  preferences.begin("wifi", true);  // Open preferences namespace "wifi" in read-only mode
  String ssid = preferences.getString("ssid", "");  // Load SSID (default: empty)
  String password = preferences.getString("password", "");  // Load password (default: empty)
  preferences.end();  // Close preferences

  // Copy loaded credentials to the WiFiConfig structure
  ssid.toCharArray(wifiConfig.ssid, sizeof(wifiConfig.ssid));
  password.toCharArray(wifiConfig.password, sizeof(wifiConfig.password));
  Serial.println("WiFi configuration loaded.");
  Serial.print("Loaded SSID: ");
  Serial.println(wifiConfig.ssid);
  Serial.print("Loaded Password: ");
  Serial.println(wifiConfig.password);
}

// Function to clear all preferences
void clearAllPreferences() {
  Serial.println("Clearing all preferences...");
  preferences.begin("wifi", false);  // Open preferences namespace "wifi" in read/write mode
  preferences.clear();  // Clear all keys and values in the "wifi" namespace
  preferences.end();  // Close preferences
  Serial.println("All preferences cleared.");
}

// Function to get the current alarm time as a string
String getAlarmTime() {
  if (alarmHour < 0 || alarmHour > 23 || alarmMinute < 0 || alarmMinute > 59) {
    return "--:--";
  }
  return String(alarmHour).c_str() + String(":") + String(alarmMinute).c_str();
}

// Handler for setting the alarm
void handleSetAlarm() {
  int hour = server.arg("hour").toInt();
  int minute = server.arg("minute").toInt();

  if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
    alarmHour = hour;
    alarmMinute = minute;
    server.send(200, "text/plain", "ok");
    Serial.printf("Sveglia impostata alle: %02d:%02d\n", hour, minute);
  } else {
    server.send(400, "text/plain", "invalid");
  }
}

// Handler for getting the current alarm time
void handleGetAlarm() {
  server.send(200, "text/plain", getAlarmTime());
}

// Handler for the root URL (serves the alarm configuration page)
void handleRoot() {
  String page = HTML;
  page.replace("%ALARM_TIME%", getAlarmTime());
  server.send(200, "text/html", page);
}

// Function to start Access Point (AP) mode
void startAP() {
  Serial.println("Starting Access Point mode...");
  WiFi.disconnect();  // Disconnect from any existing WiFi network
  WiFi.mode(WIFI_AP);  // Set WiFi mode to Access Point

  // Generate AP SSID with the device's default IP address (192.168.4.1)
  String apSSID = AP_SSID_PREFIX + WiFi.softAPIP().toString();
  WiFi.softAP(apSSID.c_str());  // Start AP with the generated SSID
  Serial.print("AP SSID: ");
  Serial.println(apSSID);

  // Define routes for the web server in AP mode
  server.on("/", []() {
    String page = CONFIG_HTML;
    page.replace("%ALARM_TIME%", getAlarmTime());
    server.send(200, "text/html", page);  // Serve the configuration page
  });

  server.on("/configure", HTTP_POST, []() {
    // Handle form submission for new WiFi credentials
    String newSSID = server.arg("ssid");  // Get SSID from form
    String newPass = server.arg("password");  // Get password from form

    // Copy new credentials to the WiFiConfig structure
    newSSID.toCharArray(wifiConfig.ssid, sizeof(wifiConfig.ssid));
    newPass.toCharArray(wifiConfig.password, sizeof(wifiConfig.password));

    saveConfig();  // Save the new credentials
    server.send(200, "text/plain", "Configurazione salvata! Riavvio...");  // Send confirmation
    delay(1000);  // Wait for the response to be sent
    ESP.restart();  // Restart the device to apply the new configuration
  });

  server.on("/clear", HTTP_POST, []() {  // Add a new route for clearing preferences
    clearAllPreferences();  // Clear all saved preferences
    server.send(200, "text/plain", "Dati salvati eliminati! Riavvio...");  // Send confirmation
    delay(1000);  // Wait for the response to be sent
    ESP.restart();  // Restart the device
  });

  server.on("/get_alarm", handleGetAlarm);  // Handle getting the current alarm time

  server.on("/set_alarm", handleSetAlarm);  // Handle setting the alarm

  apMode = true;  // Set AP mode flag
  server.begin();  // Start the web server
  Serial.println("Access Point mode started.");
}

// Function to connect to a WiFi network
void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);  // Set WiFi mode to Station (client)

  // 1. Try connecting with saved credentials first
  if (strlen(wifiConfig.ssid) > 0) {  // Check if saved SSID is not empty
    Serial.print("Trying saved WiFi: ");
    Serial.println(wifiConfig.ssid);

    WiFi.begin(wifiConfig.ssid, wifiConfig.password);  // Connect to the saved network

    int timeout = 10;  // Timeout for connection attempt (10 seconds)
    while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
      delay(1000);  // Wait 1 second between retries
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected with saved credentials!");
      return;  // Exit if connected successfully
    }
  }

  // 2. If saved credentials fail, try predefined networks from secrets.h
  Serial.println("\nSaved credentials doesn't work!\nScanning predefined networks...");
  int attempts = 0;  // Counter for connection attempts
  for (int j = 0; j < MAX_RETRIES; j++) {
    for (int i = 0; i < wifiCount; i++) {
      Serial.printf("Trying: %s (attempt %d of %d\n)", wifiList[i][0], j + 1, MAX_RETRIES); // Print SSID being tried

      WiFi.begin(wifiList[i][0], wifiList[i][1]);  // Connect to the predefined network

      int timeout = 20;  // Timeout for connection attempt (10 seconds)
      while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
        delay(1000);  // Wait 1 second between retries
        Serial.print(".");
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to predefined network!");

        // Save the working credentials
        strncpy(wifiConfig.ssid, wifiList[i][0], sizeof(wifiConfig.ssid));
        strncpy(wifiConfig.password, wifiList[i][1], sizeof(wifiConfig.password));
        saveConfig();  // Save the new credentials

        return;  // Exit if connected successfully
      }
    }
  }

  // 3. If all attempts fail, start Access Point mode
  Serial.println("\nAll connections failed, starting AP...");
  startAP();
}

// Setup function (runs once at startup)
void setup() {
  Serial.begin(115200);  // Initialize serial communication
  Serial.println("Setup started.");

  // Start AP if button is pressed at startup
  pinMode(SWITCH_PIN, INPUT_PULLUP); // Configura il pin con pull-up interno
  if (digitalRead(SWITCH_PIN) == LOW) {
    apMode = true;
    startAP();
    return;
  }

  clearAllPreferences();  // Clear saved preferences
  loadConfig();  // Load saved WiFi credentials

  connectWiFi();  // Try connecting with saved credentials

  if (!apMode) {
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
    // Define routes for the web server in normal mode
    server.on("/", handleRoot);  // Serve the alarm configuration page
    server.on("/set_alarm", handleSetAlarm);  // Handle setting the alarm
    server.on("/get_alarm", handleGetAlarm);  // Handle getting the current alarm time
    server.on("/clear", HTTP_POST, []() {  // Add the route in normal mode as well
      clearAllPreferences();  // Clear all saved preferences
      server.send(200, "text/plain", "Dati salvati eliminati! Riavvio...");  // Send confirmation
      delay(1000);  // Wait for the response to be sent
      ESP.restart();  // Restart the device
    });
    server.begin();  // Start the web server
    Serial.println("Web server started in normal mode.");
  }
  Serial.println("Setup completed.");
}

// Loop function (runs repeatedly)
void loop() {
  if (!apMode && WiFi.status() != WL_CONNECTED) {  // Check if disconnected in normal mode
    if (++connectionRetries >= MAX_RETRIES) {  // Increment retry counter
      Serial.println("Connection lost, retrying...");
      connectWiFi();  // Retry the full connection process
      connectionRetries = 0;  // Reset the retry counter
    }
  }
  server.handleClient();  // Handle incoming client requests
}
