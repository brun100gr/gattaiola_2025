const char HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Impostazione Sveglia</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }
        #alarm-time { font-size: 2em; margin: 20px 0; }
        input, button { font-size: 1.2em; padding: 10px; margin: 5px; }
    </style>
</head>
<body>
    <h2>Ora della Sveglia</h2>
    <div id="alarm-time">%ALARM_TIME%</div>
    
    <label for="hour">Ore:</label>
    <input type="number" id="hour" min="0" max="23" value="7">
    
    <label for="minute">Minuti:</label>
    <input type="number" id="minute" min="0" max="59" value="30">
    
    <button onclick="setAlarm()">Imposta Sveglia</button>

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
    </script>
</body>
</html>
)rawliteral";