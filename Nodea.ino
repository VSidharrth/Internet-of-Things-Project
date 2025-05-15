#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>  // Required for JSON response

const char* ssid = "Idk12345";
const char* password = "idk12345";

// Pins
const int redLEDs[4]   = {15, 16, 18, 22};
const int greenLEDs[4] = {4, 5, 21, 13};
const int irPins[4]    = {14, 27, 26, 25};
const int soundSensor1 = 33;
const int soundSensor2 = 32;

// Traffic states
int vehicleCounts[4] = {0, 0, 0, 0};
bool irLastState[4]  = {0, 0, 0, 0};
int currentGreen     = 0;
bool emergencyDetected = false;
bool manualOverride = false;
int manualGreen = 0;
unsigned long manualStartTime = 0;
unsigned long greenStartTime = 0;
const unsigned long greenDuration = 5000;

WebServer server(80);

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 4; i++) {
    pinMode(irPins[i], INPUT);
    pinMode(redLEDs[i], OUTPUT);
    pinMode(greenLEDs[i], OUTPUT);
  }
  pinMode(soundSensor1, INPUT);
  pinMode(soundSensor2, INPUT);
  connectToWiFi();

  server.on("/", handleDashboard);
  server.on("/manual", handleManualSwitch);
  server.on("/status", handleStatusUpdate);  // New endpoint
  server.begin();
}

void loop() {
  server.handleClient();
  readIRSensors();
  detectEmergency();
  controlTraffic();
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
}

void readIRSensors() {
  for (int i = 0; i < 4; i++) {
    bool state = digitalRead(irPins[i]);
    if (state == LOW && irLastState[i] == HIGH) {
      vehicleCounts[i]++;
    }
    irLastState[i] = state;
  }
}

void detectEmergency() {
  emergencyDetected = (digitalRead(soundSensor1) == HIGH || digitalRead(soundSensor2) == HIGH);
}

void controlTraffic() {
  unsigned long now = millis();

  // Manual override for 5 seconds max
  if (manualOverride && now - manualStartTime >= 5000) {
    manualOverride = false;
  }

  if (manualOverride) {
    setLights(manualGreen);
    return;
  }

  if (now - greenStartTime >= greenDuration || emergencyDetected) {
    int next = chooseNextRoad();
    setLights(next);
    greenStartTime = now;
  }
}

int chooseNextRoad() {
  if (emergencyDetected) {
    if (digitalRead(soundSensor1) == HIGH) return 0;
    if (digitalRead(soundSensor2) == HIGH) return 2;
  }

  int best = currentGreen;
  int maxCount = 0;
  for (int i = 1; i <= 4; i++) {
    int idx = (currentGreen + i) % 4;
    if (vehicleCounts[idx] > maxCount) {
      maxCount = vehicleCounts[idx];
      best = idx;
    }
  }
  return maxCount == 0 ? currentGreen : best;
}

void setLights(int newGreen) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(redLEDs[i], i != newGreen ? HIGH : LOW);
    digitalWrite(greenLEDs[i], i == newGreen ? HIGH : LOW);
  }
  vehicleCounts[newGreen] = 0;
  currentGreen = newGreen;
}

void handleManualSwitch() {
  if (server.hasArg("road")) {
    int req = server.arg("road").toInt();
    if (req >= 0 && req < 4) {
      manualGreen = req;
      manualOverride = true;
      manualStartTime = millis();
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// JSON response for AJAX polling
void handleStatusUpdate() {
  StaticJsonDocument<256> json;
  for (int i = 0; i < 4; i++) {
    json["vehicles"][i] = vehicleCounts[i];
  }
  json["currentGreen"] = currentGreen;
  json["emergency"] = emergencyDetected;
  serializeJson(json, server.client());
}

void handleDashboard() {
  String html = R"rawliteral(
    <html>
    <head>
      <title>Traffic Dashboard</title>
      <meta http-equiv='refresh' content='1000'>
      <script>
        function fetchData() {
          fetch("/status")
            .then(res => res.json())
            .then(data => {
              for (let i = 0; i < 4; i++) {
                document.getElementById("count" + i).innerText = data.vehicles[i];
                document.getElementById("green" + i).innerText = (i == data.currentGreen ? "YES" : "NO");
              }
              document.getElementById("emergency").innerText = data.emergency ? "YES" : "No";
            });
        }
        setInterval(fetchData, 1000);
        window.onload = fetchData;
      </script>
    </head>
    <body>
      <h2>Traffic Dashboard</h2>
      <table border='1' cellpadding='6'>
        <tr><th>Road</th><th>Vehicle Count</th><th>Green</th></tr>
        <tr><td>Road 1</td><td id='count0'>-</td><td id='green0'>-</td></tr>
        <tr><td>Road 2</td><td id='count1'>-</td><td id='green1'>-</td></tr>
        <tr><td>Road 3</td><td id='count2'>-</td><td id='green2'>-</td></tr>
        <tr><td>Road 4</td><td id='count3'>-</td><td id='green3'>-</td></tr>
      </table>
      <p><b>Emergency:</b> <span id="emergency">Checking...</span></p>
      <hr>
      <form action='/manual' method='GET'>
        <label>Manual Switch to:</label>
        <select name='road'>
          <option value='0'>Road 1</option>
          <option value='1'>Road 2</option>
          <option value='2'>Road 3</option>
          <option value='3'>Road 4</option>
        </select>
        <input type='submit' value='Set Green (5s)'>
      </form>
    </body></html>
  )rawliteral";
  server.send(200, "text/html", html);
}