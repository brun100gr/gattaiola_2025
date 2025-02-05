#include <WiFi.h>
#include <WebServer.h>

#include "secrets.h"
#include "html.h"

WebServer server(80);

// Variabili per l'allarme (inizialmente invalide)
//int alarmHour = -1;
//int alarmMinute = -1;

int alarmHour = 7;
int alarmMinute = 30;

String getAlarmTime() {
    if(alarmHour < 0 || alarmHour > 23 || alarmMinute < 0 || alarmMinute > 59) {
        return "--:--";
    }
    return String(alarmHour).c_str() + String(":") + String(alarmMinute).c_str();
}

void handleRoot() {
    String page = HTML;
    page.replace("%ALARM_TIME%", getAlarmTime());
    server.send(200, "text/html", page);
}

void handleSetAlarm() {
    int hour = server.arg("hour").toInt();
    int minute = server.arg("minute").toInt();
    
    if(hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
        alarmHour = hour;
        alarmMinute = minute;
        server.send(200, "text/plain", "ok");
        
        // Versione corretta per interi:
        Serial.printf("Sveglia impostata alle: %02d:%02d\n", hour, minute);
    } else {
        server.send(400, "text/plain", "invalid");
    }
}

void handleGetAlarm() {
    server.send(200, "text/plain", getAlarmTime());
}

void setup() {
    Serial.begin(115200);
  
    // Connessione WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    
    Serial.println("\nConnesso! IP:");
    Serial.println(WiFi.localIP());
    
    server.on("/", handleRoot);
    server.on("/set_alarm", handleSetAlarm);
    server.on("/get_alarm", handleGetAlarm);
    server.begin();
}

void loop() {
    server.handleClient();
}