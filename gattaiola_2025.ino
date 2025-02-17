#define CONFIG_ESP32_SPIRAM_SUPPORT 0
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "html_wifi_configuration.h"  // HTML content for the configuration page
#include "secrets.h"  // File containing predefined WiFi networks (SSID and password)
#include "html_set_alarm.h"  // HTML content for the alarm configuration page
#include <NTPClient.h>      // Library to get time via NTP
#include <WiFiUdp.h>        // UDP library for NTPClient

#define MAX_RETRIES 5  // Maximum number of connection retries before starting AP mode

// NTP synchronization settings
#define NTP_TIMEOUT 2000    // Timeout for each NTP attempt (2000 ms)
#define NTP_RETRIES 3       // Number of attempts for each NTP server
#define NTP_INTERVAL 3600   // Interval between syncs (3600 seconds = 1 hour)

const int SWITCH_PIN = 4;       // GPIO collegato allo switch

WebServer server(80);  // Web server running on port 80
Preferences preferences;  // Used for persistent storage of WiFi credentials

// List of predefined WiFi networks from secrets.h
const int wifiCount = sizeof(wifiList) / sizeof(wifiList[0]);

// List of NTP servers in priority order
const char* ntpServers[] = {
  "pool.ntp.org",          // Main server (global)
  "europe.pool.ntp.org",   // European server
  "time.google.com",       // Google NTP server
  "ntp1.inrim.it"          // Italian NTP server (INRIM)
};
const int numNtpServers = sizeof(ntpServers) / sizeof(ntpServers[0]);

// Enumeration representing the time synchronization status
enum SyncStatus {
  SYNC_IN_PROGRESS,  // Sync in progress
  SYNC_OK,           // Sync successful
  SYNC_FAIL          // Sync failed
};

// UDP object for NTP communication
WiFiUDP ntpUDP;

// NTP client instance using the ntpUDP object
NTPClient timeClient(ntpUDP);

// Global variable to store the sync status (initially failed)
volatile SyncStatus timeSyncStatus = SYNC_FAIL;
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

// Pin definitions for gate control
const int MOTOR_PWMA = 12;     // PWM motor
const int MOTOR_AIN1 = 14;     // Direction 1
const int MOTOR_AIN2 = 27;     // Direction 2
const int BUTTON_PIN = 5;      // Manual button
const int LIMIT_OPEN = 18;     // Limit switch open
const int LIMIT_CLOSE = 19;    // Limit switch close

// States and directions
enum GateState { CLOSED, OPEN, MOVING };
enum Direction { STOPPED, OPENING, CLOSING };
volatile GateState currentState = CLOSED;
volatile Direction currentDirection = STOPPED;
bool targetStateChanged = false;

// Debounce variables
#define DEBOUNCE_DELAY 50  // Debounce delay in milliseconds

bool buttonState = HIGH;  // Current button state
bool lastButtonState = HIGH;  // Last button state
unsigned long lastDebounceTime = 0;  // Last debounce time

// Motor parameters
const int motorSpeed = 128;    // Speed 0-255

void IRAM_ATTR limitOpenISR() {
  Serial.println("[INTERRUPT] Limit switch OPEN.");
  if (currentDirection == OPENING) {
    stopMotor();
    currentState = OPEN;
    currentDirection = STOPPED;
    Serial.println("[INTERRUPT] Limit switch OPEN reached. Gate fully open.");
  }
}

void IRAM_ATTR limitCloseISR() {
  Serial.println("[INTERRUPT] Limit switch CLOSE.");
  if (currentDirection == CLOSING) {
    stopMotor();
    currentState = CLOSED;
    currentDirection = STOPPED;
    Serial.println("[INTERRUPT] Limit switch CLOSE reached. Gate fully closed.");
  }
}
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

//////////////////////////
// TIME SYNCHRONIZATION (NTP)
//////////////////////////

// syncTime() attempts to synchronize time using NTP servers
// 'mandatory' parameter forces sync even if already OK
void syncTime(bool mandatory) {
  // If sync is already OK and not mandatory, exit immediately
  if (!mandatory && timeSyncStatus == SYNC_OK) return;

  // Set status to sync in progress
  timeSyncStatus = SYNC_IN_PROGRESS;

  // Loop through the list of NTP servers
  for (int s = 0; s < numNtpServers; s++) {
    // Set the current NTP server
    timeClient.setPoolServerName(ntpServers[s]);
    // Set the time offset (3600 seconds = 1 hour for CET)
    timeClient.setTimeOffset(3600);

    Serial.print("\nTrying NTP server: ");
    Serial.println(ntpServers[s]);

    // Try the current server for the defined retries
    for (int i = 0; i < NTP_RETRIES; i++) {
      unsigned long startAttempt = millis();  // Record attempt start time
      bool updated = false;

      // Attempt to force time update within timeout
      while (millis() - startAttempt < NTP_TIMEOUT) {
        if (timeClient.forceUpdate()) {
          updated = true;
          break;
        }
        delay(100);  // Wait 100 ms before retrying
      }

      // If update successful and time is valid, set status to OK and return
      if (updated && validateTime()) {
        timeSyncStatus = SYNC_OK;
        Serial.println("Sync successful!");
        Serial.println("Time: " + timeClient.getFormattedTime());
        return;
      }
      Serial.println("Attempt failed (" + String(i+1) + "/" + String(NTP_RETRIES) + ")");
    }
  }

  // If no server succeeds, set sync status to fail
  timeSyncStatus = SYNC_FAIL;
  Serial.println("Error: All NTP servers failed!");
}

// validateTime() checks if the received timestamp is valid
// Here, the time is valid if after January 1, 2023
bool validateTime() {
  time_t epochTime = timeClient.getEpochTime();
  if (epochTime < 1672531200) {  // January 1, 2023
    Serial.println("Invalid timestamp");
    return false;
  }
  return true;
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

  timeClient.begin();  // Initialize the NTP client
  syncTime(true); // Force time synchronization

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

  // Motor pin configuration
  pinMode(MOTOR_PWMA, OUTPUT);
  pinMode(MOTOR_AIN1, OUTPUT);
  pinMode(MOTOR_AIN2, OUTPUT);

  // Button and limit switch configuration
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LIMIT_OPEN, INPUT_PULLUP);
  pinMode(LIMIT_CLOSE, INPUT_PULLUP);

  // Interrupts for limit switches
  attachInterrupt(digitalPinToInterrupt(LIMIT_OPEN), limitOpenISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(LIMIT_CLOSE), limitCloseISR, FALLING);

  // Initialize motor stopped
  stopMotor();
  Serial.println("System ready. Initial state: CLOSED.");

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

  static unsigned long lastSync = 0;

  if (millis() - lastSync > (NTP_INTERVAL * 1000)) {
    syncTime(false);
    lastSync = millis();
  }

  checkButton();
  executeMovement();
  server.handleClient();  // Handle incoming client requests
}

// Function to check the button state with debounce
void checkButton() {
  int reading = digitalRead(BUTTON_PIN);  // Read the current button state

  // If the state has changed, reset the debounce timer
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  // If the time elapsed exceeds the debounce delay
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // If the button state has changed
    if (reading != buttonState) {
      buttonState = reading;

      // If the button was pressed (transition HIGH -> LOW)
      if (buttonState == LOW) {
        Serial.println("[BUTTON] Pressed!");
        handleButtonPress();  // Function to handle button press
      }

      // If the button was released (transition LOW -> HIGH)
      else {
        Serial.println("[BUTTON] Released!");
      }
    }
  }

  lastButtonState = reading;  // Update the last state
}

// Function to handle button press
void handleButtonPress() {
  if (currentState == MOVING) {
    Serial.println("[MOVEMENT] Button pressed during movement. Reversing direction...");
    if (currentDirection == OPENING) {
      Serial.println("[MOVEMENT] Reversing direction: closing.");
      closeGate();
    } else if (currentDirection == CLOSING) {
      Serial.println("[MOVEMENT] Reversing direction: opening.");
      openGate();
    }
  } else {
    Serial.println("[MOVEMENT] Button pressed at rest. Changing state...");
    toggleGate();
  }
}

// Function to toggle the gate state
void toggleGate() {
  if (currentState == CLOSED) {
    Serial.println("[MOVEMENT] Gate closed. Command to open.");
    openGate();
  } else if (currentState == OPEN) {
    Serial.println("[MOVEMENT] Gate open. Command to close.");
    closeGate();
  }
}

// Function to execute movement logic
void executeMovement() {
  // Additional logic during movement (if needed)
}

// Function to open the gate
void openGate() {
  if (currentState != OPEN) {
    currentState = MOVING;
    currentDirection = OPENING;
    Serial.println("[MOVEMENT] Starting gate opening...");
    digitalWrite(MOTOR_AIN1, HIGH);
    digitalWrite(MOTOR_AIN2, LOW);
    analogWrite(MOTOR_PWMA, motorSpeed);
  } else {
    Serial.println("[MOVEMENT] Gate already open. No action.");
  }
}

// Function to close the gate
void closeGate() {
  if (currentState != CLOSED) {
    currentState = MOVING;
    currentDirection = CLOSING;
    Serial.println("[MOVEMENT] Starting gate closing...");
    digitalWrite(MOTOR_AIN1, LOW);
    digitalWrite(MOTOR_AIN2, HIGH);
    analogWrite(MOTOR_PWMA, motorSpeed);
  } else {
    Serial.println("[MOVEMENT] Gate already closed. No action.");
  }
}

// Function to stop the motor
void stopMotor() {
  analogWrite(MOTOR_PWMA, 0);
  digitalWrite(MOTOR_AIN1, LOW);
  digitalWrite(MOTOR_AIN2, LOW);
  Serial.println("[MOVEMENT] Motor stopped.");
}

/*
# Include necessary libraries
#include <WiFi.h>           // Library for WiFi connection
#include <NTPClient.h>      // Library to get time via NTP
#include <WiFiUdp.h>        // UDP library for NTPClient
#include <Arduino.h>        // Base Arduino definitions

// WiFi credentials: change these to your network
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// List of NTP servers in priority order
const char* ntpServers[] = {
  "pool.ntp.org",          // Main server (global)
  "europe.pool.ntp.org",   // European server
  "time.google.com",       // Google NTP server
  "ntp1.inrim.it"          // Italian NTP server (INRIM)
};
const int numNtpServers = sizeof(ntpServers) / sizeof(ntpServers[0]);

// NTP synchronization settings
#define NTP_TIMEOUT 2000    // Timeout for each NTP attempt (2000 ms)
#define NTP_RETRIES 3       // Number of attempts for each NTP server
#define NTP_INTERVAL 3600   // Interval between syncs (3600 seconds = 1 hour)

// UDP object for NTP communication
WiFiUDP ntpUDP;

// NTP client instance using the ntpUDP object
NTPClient timeClient(ntpUDP);

// Status LED pin: this LED indicates the sync status
const int SYNC_LED = 2;

// Enumeration representing the time synchronization status
enum SyncStatus {
  SYNC_IN_PROGRESS,  // Sync in progress
  SYNC_OK,           // Sync successful
  SYNC_FAIL          // Sync failed
};

// Global variable to store the sync status (initially failed)
volatile SyncStatus timeSyncStatus = SYNC_FAIL;

//////////////////////////
// WIFI FUNCTIONS
//////////////////////////

// The initWiFi() function initializes the WiFi connection in Station mode
void initWiFi() {
  WiFi.mode(WIFI_STA);             // Set WiFi to Station mode (client)
  WiFi.setAutoReconnect(true);     // Enable automatic reconnection
  WiFi.begin(ssid, password);      // Start the connection with provided credentials
  Serial.println("Connecting to WiFi...");
}

//////////////////////////
// WIFI EVENT HANDLER
//////////////////////////

// WiFiEventHandler function handles WiFi events (e.g., disconnection, reconnection)
void WiFiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch(event) {
    // Case: WiFi disconnected
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("\nWiFi lost! Attempting to reconnect...");
      WiFi.reconnect();             // Attempt to reconnect automatically
      break;

    // Case: WiFi connected and IP address assigned
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("\nWiFi connected! IP: ");
      Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
      // Force sync if not yet synchronized
      if(timeSyncStatus != SYNC_OK) {
        syncTime(true);
      }
      break;

    default:
      break;
  }
}

//////////////////////////
// TIME SYNCHRONIZATION (NTP)
//////////////////////////

// syncTime() attempts to synchronize time using NTP servers
// 'mandatory' parameter forces sync even if already OK
void syncTime(bool mandatory) {
  // If sync is already OK and not mandatory, exit immediately
  if (!mandatory && timeSyncStatus == SYNC_OK) return;

  // Set status to sync in progress
  timeSyncStatus = SYNC_IN_PROGRESS;

  // Loop through the list of NTP servers
  for (int s = 0; s < numNtpServers; s++) {
    // Set the current NTP server
    timeClient.setPoolServerName(ntpServers[s]);
    // Set the time offset (3600 seconds = 1 hour for CET)
    timeClient.setTimeOffset(3600);

    Serial.print("\nTrying NTP server: ");
    Serial.println(ntpServers[s]);

    // Try the current server for the defined retries
    for (int i = 0; i < NTP_RETRIES; i++) {
      unsigned long startAttempt = millis();  // Record attempt start time
      bool updated = false;

      // Attempt to force time update within timeout
      while (millis() - startAttempt < NTP_TIMEOUT) {
        if (timeClient.forceUpdate()) {
          updated = true;
          break;
        }
        delay(100);  // Wait 100 ms before retrying
      }

      // If update successful and time is valid, set status to OK and return
      if (updated && validateTime()) {
        timeSyncStatus = SYNC_OK;
        Serial.println("Sync successful!");
        Serial.println("Time: " + timeClient.getFormattedTime());
        return;
      }
      Serial.println("Attempt failed (" + String(i+1) + "/" + String(NTP_RETRIES) + ")");
    }
  }

  // If no server succeeds, set sync status to fail
  timeSyncStatus = SYNC_FAIL;
  Serial.println("Error: All NTP servers failed!");
}

// validateTime() checks if the received timestamp is valid
// Here, the time is valid if after January 1, 2023
bool validateTime() {
  time_t epochTime = timeClient.getEpochTime();
  if (epochTime < 1672531200) {  // January 1, 2023
    Serial.println("Invalid timestamp");
    return false;
  }
  return true;
}

//////////////////////////
// STATUS LED UPDATE
//////////////////////////

// updateStatusLED() updates the LED based on sync status
// SYNC_OK: LED solid on
// SYNC_IN_PROGRESS: LED blinks fast (200 ms)
// SYNC_FAIL: LED blinks slow (1000 ms)
void updateStatusLED() {
  static unsigned long lastBlink = 0;  // Last LED update time
  static bool ledState = false;        // Current LED state

  switch (timeSyncStatus) {
    case SYNC_OK:
      digitalWrite(SYNC_LED, HIGH);  // LED on (sync successful)
      break;

    case SYNC_IN_PROGRESS:
      if (millis() - lastBlink > 200) {
        ledState = !ledState;
        digitalWrite(SYNC_LED, ledState);
        lastBlink = millis();
      }
      break;

    case SYNC_FAIL:
      if (millis() - lastBlink > 1000) {
        ledState = !ledState;
        digitalWrite(SYNC_LED, ledState);
        lastBlink = millis();
      }
      break;
  }
}

//////////////////////////
// MAIN PROGRAM: setup() and loop()
//////////////////////////

void setup() {
  Serial.begin(115200);

  pinMode(SYNC_LED, OUTPUT);

  WiFi.onEvent(WiFiEventHandler);

  initWiFi();

  timeClient.begin();

  syncTime(true);
}

void loop() {
  static unsigned long lastSync = 0;

  if (millis() - lastSync > (NTP_INTERVAL * 1000)) {
    syncTime(false);
    lastSync = millis();
  }

  updateStatusLED();

  delay(100);
}

*/
