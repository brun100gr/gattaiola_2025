const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configurazione Dispositivo IoT</title>
    <style>
        body { 
            font-family: Arial, sans-serif; 
            text-align: center; 
            margin: 20px;
        }
        .container { 
            max-width: 500px; 
            margin: 0 auto;
            padding: 20px;
            border: 1px solid #ddd;
            border-radius: 8px;
        }
        .section {
            margin-bottom: 30px;
            padding: 20px;
            background-color: #f9f9f9;
            border-radius: 8px;
        }
        .form-group { 
            margin: 20px 0;
        }
        input, label { 
            padding: 8px; 
            width: 100%;
            max-width: 200px;
        }
        button { 
            padding: 10px 20px; 
            background: #4CAF50; 
            color: white; 
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }
        .danger { 
            background: #f44336; 
        }
        #alarm-time { 
            font-size: 2em; 
            margin: 20px 0;
            color: #333;
        }
        h2 {
            color: #444;
            margin-bottom: 25px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            color: #666;
        }
    </style>
</head>
<body>
    <div class="container">
        <!-- Sezione Configurazione WiFi -->
        <div class="section">
            <h2>Configurazione WiFi</h2>
            <form action="/configure" method="POST">
                <div class="form-group">
                    <input type="text" name="ssid" placeholder="SSID" value="%SSID%" required>
                </div>
                <div class="form-group">
                    <input type="password" id="password" name="password" placeholder="Password" value="%PASSWORD%" required>
                </div>
                <div class="form-group">
                    <div class="form-group">
                        <input type="checkbox" id="show-password" onclick="togglePassword()">
                        <label for="show-password">Mostra Password</label>
                    </div>
                </div>
                <div class="form-group">
                    <button type="submit">Salva e Riavvia</button>
                </div>
            </form>
            <form action="/clear" method="POST">
                <div class="form-group">
                    <button type="submit" class="danger">Elimina Dati Salvati</button>
                </div>
            </form>
        </div>

        <!-- Sezione Impostazione Sveglia -->
        <div class="section">
            <h2>Impostazione Sveglia</h2>
            <div id="alarm-time">%ALARM_TIME%</div>
            
            <div class="form-group">
                <label for="hour">Ore:</label>
                <input type="number" id="hour" min="0" max="23" value="7">
            </div>
            
            <div class="form-group">
                <label for="minute">Minuti:</label>
                <input type="number" id="minute" min="0" max="59" value="30">
            </div>
            
            <button onclick="setAlarm()">Imposta Sveglia</button>
        </div>
    </div>

    <script>
    // Funzione per aggiornare l'ora al caricamento della pagina
    window.onload = function() {
        fetch('/get_alarm')
            .then(response => response.text())
            .then(data => {
                if(data !== "--:--") {
                    const [hour, minute] = data.split(":");
                    document.getElementById("hour").value = hour;
                    document.getElementById("minute").value = minute;
                }
                document.getElementById("alarm-time").innerText = data;
            });
    };

    function setAlarm() {
        const hour = document.getElementById("hour").value.padStart(2, '0');
        const minute = document.getElementById("minute").value.padStart(2, '0');
        
        fetch(`/set_alarm?hour=${hour}&minute=${minute}`)
            .then(response => response.text())
            .then(data => {
                document.getElementById("alarm-time").innerText = 
                    data === "invalid" ? "--:--" : `${hour}:${minute}`;
            })
            .catch(error => console.error("Errore:", error));
    }

    function togglePassword() {
        const passwordField = document.getElementById("password");
        const showPasswordCheckbox = document.getElementById("show-password");
        if (showPasswordCheckbox.checked) {
            passwordField.type = "text";
        } else {
            passwordField.type = "password";
        }
    }
    </script>
</body>
</html>
)rawliteral";
