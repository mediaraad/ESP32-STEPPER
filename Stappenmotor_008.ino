#include "secrets.h"  // WIFI_TP_SSID en WIFI_TP_PASSWORD
#include <WiFi.h>
#include <WebServer.h>

// WiFi-instellingen uit secrets.h
const char* ssid     = WIFI_TP_SSID;
const char* password = WIFI_TP_PASSWORD;

// Motorpinnen ULN2003
const int motorPins[] = {14, 12, 13, 15};

// Timing
float stepDelay = 1.0;        // μs per stap
bool direction = true;         // true = vooruit, false = achteruit
int sliderValue = 10;          // huidig sliderwaarde
WebServer server(80);

// 8-step half-step sequence voor 28BYJ-48
const int steps[8][4] = {
  {1,0,0,0},
  {1,1,0,0},
  {0,1,0,0},
  {0,1,1,0},
  {0,0,1,0},
  {0,0,1,1},
  {0,0,0,1},
  {1,0,0,1}
};

int stepIndex = 0;
unsigned long previousMicros = 0;

// Steps per seconde limieten
#define MAX_STEPS_PER_SEC 1500.0
#define MIN_STEPS_PER_SEC 10.0

// HTML webinterface
const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Stappenmotor Control</title>
<script src="https://code.jquery.com/jquery-3.6.0.min.js"></script>
<style>
  body { background-color: #121212; color: #eeeeee; font-family: Arial, sans-serif; text-align: center; }
  h2 { color: #00bfff; }
  input[type=range] { width: 60%; }
  label { display: block; margin-top: 20px; }
  span { font-weight: bold; }
  .container { margin: 50px auto; width: 80%; }
  input[type=checkbox] { transform: scale(1.5); margin-left: 10px; }
  .status-container {
    margin-top: 30px;
    padding: 15px;
    border: 1px solid #444;
    border-radius: 10px;
    background-color: #1e1e1e;
    width: 60%;
    margin-left: auto;
    margin-right: auto;
    text-align: left;
  }
  .status-container h3 {
    color: #00bfff;
    text-align: center;
  }
  .status-container p { margin: 5px 0; }
</style>
</head>

<body>
<div class="container">
  <h2>Stappenmotor</h2>

  <label>
    Snelheid (1 = langzaam, 60 = extreem snel):
    <input type="range" min="1" max="60" value="10" id="speedSlider">
    <span id="speedVal">10</span>
  </label>

  <label>
    Draairichting:
    <input type="checkbox" id="dirSwitch" checked> Vooruit / Achteruit
  </label>

  <div class="status-container">
    <h3>Status</h3>
    <p>Sliderwaarde: <span id="statusSlider">0</span></p>
    <p>Stappen/sec: <span id="statusSteps">0</span></p>
    <p>StepDelay (μs): <span id="statusDelay">0</span></p>
    <p>Draairichting: <span id="statusDir">Vooruit</span></p>
  </div>
</div>

<script>
$(function(){
  $('#speedSlider').on('input', function(){
    $('#speedVal').text($(this).val());
    $.get(`/speed?value=${$(this).val()}`);
  });

  $('#dirSwitch').on('change', function(){
    $.get(`/direction?value=${$(this).prop('checked')}`);
  });

  setInterval(function(){
    $.get("/status", function(data){
      $('#statusSlider').text(data.sliderValue);
      $('#statusSteps').text(data.stepsPerSec);
      $('#statusDelay').text(data.stepDelay_us);
      $('#statusDir').text(data.direction);
    }, "json");
  }, 200);
});
</script>
</body>
</html>
)rawliteral";

// ---------- Web handlers ----------
void handleRoot() {
  server.send(200, "text/html", html);
}

void handleSpeed() {
  if (server.hasArg("value")) {
    sliderValue = server.arg("value").toInt();
    sliderValue = constrain(sliderValue, 1, 60);

    // Intuïtieve mapping: 1 = langzaam, 60 = snel
    float stepsPerSec = MIN_STEPS_PER_SEC + (sliderValue - 1) * (MAX_STEPS_PER_SEC - MIN_STEPS_PER_SEC) / 59.0;
    stepDelay = 1000000.0 / stepsPerSec; // μs per stap
  }
  server.send(200,"text/plain","OK");
}

void handleDirection() {
  if (server.hasArg("value")) {
    direction = (server.arg("value") == "true");
  }
  server.send(200,"text/plain","OK");
}

// Status endpoint
void handleStatus() {
  float stepsPerSec = 1000000.0 / stepDelay;

  String json = "{";
  json += "\"sliderValue\":" + String(sliderValue) + ",";
  json += "\"stepsPerSec\":" + String(stepsPerSec,1) + ",";
  json += "\"stepDelay_us\":" + String(stepDelay,1) + ",";
  json += "\"direction\":\"" + String(direction ? "Vooruit" : "Achteruit") + "\"";
  json += "}";

  server.send(200,"application/json",json);
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  for(int i=0;i<4;i++) pinMode(motorPins[i], OUTPUT);

  WiFi.begin(ssid,password);
  Serial.print("Verbinden met WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi verbonden!");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/speed", handleSpeed);
  server.on("/direction", handleDirection);
  server.on("/status", handleStatus);
  server.begin();
}

// ---------- Loop ----------
void loop() {
  server.handleClient();

  unsigned long now = micros();
  if(now - previousMicros >= stepDelay){
    previousMicros = now;

    for(int i=0;i<4;i++){
      digitalWrite(motorPins[i], steps[stepIndex][i]);
    }

    stepIndex += direction ? 1 : -1;
    if(stepIndex > 7) stepIndex = 0;
    if(stepIndex < 0) stepIndex = 7;
  }
}
