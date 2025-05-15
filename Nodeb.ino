#include <WiFi.h>
#include <WebServer.h>

// Wi-Fi credentials
const char* ssid = "Idk12345";
const char* password = "idk12345";

// LED pins (traffic lights)
const int redLEDs[4]   = {15, 16, 18, 22};
const int greenLEDs[4] = {4, 5, 21, 13};

// IR sensor pins
const int irSensors[4] = {14, 27, 26, 25};
int vehicleCounts[4]   = {0, 0, 0, 0};

// Sound sensors for emergency (Road 1 & 4)
const int soundSensor1 = 32;  // Road 1
const int soundSensor2 = 33;  // Road 4

// Timing and status
unsigned long lastChangeTime = 0;
int currentRoad = 0;
const unsigned long greenDuration = 5000;

bool emergencyActive = false;
bool emergencyStatusLive = false;

WebServer server(80);

// Function prototypes
void handleDashboard();
void handleSetRoad();
void checkIRSensors();
void checkEmergency();
void switchToRoad(int roadIndex);

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 4; i++) {
    pinMode(redLEDs[i], OUTPUT);
    pinMode(greenLEDs[i], OUTPUT);
    digitalWrite(redLEDs[i], HIGH);
    digitalWrite(greenLEDs[i], LOW);
    pinMode(irSensors[i], INPUT);
  }

  pinMode(soundSensor1, INPUT);
  pinMode(soundSensor2, INPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }

  Serial.println("\nWiFi connected.");
  Serial.print("IP address: "); Serial.println(WiFi.localIP());

  server.on("/", handleDashboard);
  server.on("/set", handleSetRoad);
  server.begin();
  Serial.println("HTTP server started.");

  lastChangeTime = millis();
  switchToRoad(0);
}

void loop() {
  server.handleClient();
  checkIRSensors();
  checkEmergency();

  if (emergencyActive) {
    if (digitalRead(soundSensor1) == HIGH) {
      Serial.println("Emergency on Road 1");
      switchToRoad(0);
    } else if (digitalRead(soundSensor2) == HIGH) {
      Serial.println("Emergency on Road 4");
      switchToRoad(3);
    }
    emergencyActive = false;
    emergencyStatusLive = true;
    delay(greenDuration);
    lastChangeTime = millis();
    return;
  }

  emergencyStatusLive = false;

  if (millis() - lastChangeTime > greenDuration) {
    lastChangeTime = millis();
    vehicleCounts[currentRoad] = 0;
    int nextRoad = getNextRoad();
    switchToRoad(nextRoad);
  }
}

void checkIRSensors() {
  for (int i = 0; i < 4; i++) {
    if (digitalRead(irSensors[i]) == LOW) {
      vehicleCounts[i]++;
      delay(200);  // Simple debounce
    }
  }
}

void checkEmergency() {
  if (digitalRead(soundSensor1) == HIGH || digitalRead(soundSensor2) == HIGH) {
    emergencyActive = true;
  }
}

int getNextRoad() {
  for (int offset = 1; offset < 4; offset++) {
    int road = (currentRoad + offset) % 4;
    if (vehicleCounts[road] > 0) {
      return road;
    }
  }
  return (currentRoad + 1) % 4;
}

void switchToRoad(int roadIndex) {
  Serial.print("Switching to Road "); Serial.println(roadIndex + 1);
  for (int i = 0; i < 4; i++) {
    digitalWrite(redLEDs[i], HIGH);
    digitalWrite(greenLEDs[i], LOW);
  }
  digitalWrite(redLEDs[roadIndex], LOW);
  digitalWrite(greenLEDs[roadIndex], HIGH);
  currentRoad = roadIndex;
}

void handleDashboard() {
  String html = "<html><head><meta http-equiv='refresh' content='2'/>";
  html += "<style>body{font-family:sans-serif;}table{border-collapse:collapse;}td,th{border:1px solid #888;padding:6px;} .green{color:green;} .red{color:red;}</style></head><body>";
  html += "<h2>Node B - Traffic Dashboard</h2>";
  html += "<table><tr><th>Road</th><th>Vehicles</th><th>Green?</th><th>Control</th></tr>";

  for (int i = 0; i < 4; i++) {
    html += "<tr><td>Road " + String(i + 1) + "</td>";
    html += "<td>" + String(vehicleCounts[i]) + "</td>";
    html += "<td>";
    html += (i == currentRoad) ? "<span class='green'>YES</span>" : "NO";
    html += "</td><td><a href='/set?road=" + String(i) + "'><button>Make Green</button></a></td></tr>";
  }

  html += "</table><br>";
  html += "<b>Emergency Status:</b> ";
  html += emergencyStatusLive ? "<span class='red'>Active</span>" : "<span>None</span>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSetRoad() {
  if (server.hasArg("road")) {
    int roadIndex = server.arg("road").toInt();
    if (roadIndex >= 0 && roadIndex < 4) {
      switchToRoad(roadIndex);
      lastChangeTime = millis(); // reset green duration timer
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}