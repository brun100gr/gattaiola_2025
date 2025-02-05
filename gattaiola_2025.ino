#include <WiFi.h>
#include "secrets.h"

const int wifiCount = sizeof(wifiList) / sizeof(wifiList[0]);
void connectToWiFi() {
  int attempt = 0;
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to WiFi... ");
    Serial.println(wifiList[attempt][0]);

    WiFi.config(INADDR_NONE, INADDR_NONE, IPAddress(8, 8, 8, 8)); // Usa il DNS di Google
    WiFi.begin(wifiList[attempt][0], wifiList[attempt][1]);

    int timeout = 10; // Timeout in seconds
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
      delay(1000);
      Serial.print(".");
      timeout--;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to WiFi!");
      Serial.print("SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      return;
    } else {
      Serial.println("\nConnection failed.");
      attempt = (attempt + 1) % wifiCount; // Try the next SSID
    }
  }
}

void setup() {
  // Serial comunication initialization
  Serial.begin(115200);

  // Initial connection to WiFi
  connectToWiFi();

}

void loop() {
  // Check the WiFi connection status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    connectToWiFi();
  }
}
