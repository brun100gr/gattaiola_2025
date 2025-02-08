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
