const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }
    .container { max-width: 300px; margin: 0 auto; }
    .form-group { margin: 20px 0; }  /* Add a class for spacing */
    input { padding: 8px; width: 100%%; }
    button { padding: 10px 20px; background: #4CAF50; color: white; border: none; }
    .danger { background: #f44336; }  /* Add a class for the delete button */
  </style>
</head>
<body>
  <div class="container">
    <h2>Configura WiFi</h2>
    <form action="/configure" method="POST">
      <div class="form-group">
        <input type="text" name="ssid" placeholder="SSID" required>
      </div>
      <div class="form-group">
        <input type="password" name="password" placeholder="Password" required>
      </div>
      <div class="form-group">
        <button type="submit">Salva e Riavvia</button>
      </div>
    </form>
    <form action="/clear" method="POST">  <!-- Add a new form for clearing preferences -->
      <div class="form-group">
        <button type="submit" class="danger">Elimina Dati Salvati</button>
      </div>
    </form>
  </div>
</body>
</html>
)rawliteral";
