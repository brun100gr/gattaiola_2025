#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#define MAX_RETRIES 5

WebServer server(80);
Preferences preferences;

// Struttura per salvare le credenziali
struct WiFiConfig {
  char ssid[32];
  char password[64];
} wifiConfig;

int connectionRetries = 0;
bool apMode = false;

const char AP_SSID_PREFIX[] = "ESP32-Config-";

const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }
    .container { max-width: 300px; margin: 0 auto; }
    input { margin: 10px 0; padding: 8px; width: 100%%; }
    button { padding: 10px 20px; background: #4CAF50; color: white; border: none; }
  </style>
</head>
<body>
  <div class="container">
    <h2>Configura WiFi</h2>
    <form action="/configure" method="POST">
      <input type="text" name="ssid" placeholder="SSID" required>
      <input type="password" name="password" placeholder="Password" required>
      <button type="submit">Salva e Riavvia</button>
    </form>
  </div>
</body>
</html>
)rawliteral";

void saveConfig() {
  preferences.begin("wifi", false);
  preferences.putString("ssid", wifiConfig.ssid);
  preferences.putString("password", wifiConfig.password);
  preferences.end();
}

void loadConfig() {
  preferences.begin("wifi", true);
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");
  preferences.end();

  ssid.toCharArray(wifiConfig.ssid, sizeof(wifiConfig.ssid));
  password.toCharArray(wifiConfig.password, sizeof(wifiConfig.password));
}

void startAP() {
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  
  // Genera SSID con IP predefinito (192.168.4.1)
  String apSSID = AP_SSID_PREFIX + WiFi.softAPIP().toString();
  WiFi.softAP(apSSID.c_str());
  
  server.on("/", []() {
    server.send(200, "text/html", CONFIG_HTML);
  });

  server.on("/configure", HTTP_POST, []() {
    String newSSID = server.arg("ssid");
    String newPass = server.arg("password");
    
    newSSID.toCharArray(wifiConfig.ssid, sizeof(wifiConfig.ssid));
    newPass.toCharArray(wifiConfig.password, sizeof(wifiConfig.password));
    
    saveConfig();
    server.send(200, "text/plain", "Configurazione salvata! Riavvio...");
    delay(1000);
    ESP.restart();
  });

  apMode = true;
  server.begin();
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiConfig.ssid, wifiConfig.password);
  
  Serial.print("Connessione a ");
  Serial.println(wifiConfig.ssid);

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if(++connectionRetries >= MAX_RETRIES) {
      Serial.println("\nFallimento connessione, avvio AP...");
      startAP();
      return;
    }
  }
  
  Serial.println("\nConnesso!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  loadConfig();

  if(strlen(wifiConfig.ssid) > 0) {
    connectWiFi();
  } else {
    startAP();
  }

  if(!apMode) {
    // Aggiungi qui le tue route normali
    server.on("/", []() {
      server.send(200, "text/plain", "Benvenuto nel dispositivo ESP32!");
    });
    server.begin();
  }
}

void loop() {
  if(!apMode && WiFi.status() != WL_CONNECTED) {
    if(++connectionRetries >= MAX_RETRIES) {
      startAP();
    }
  }
  server.handleClient();
}
