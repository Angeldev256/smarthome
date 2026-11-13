/*
 * =====================================================================
 *   SMART ESTATE  –  ESP32 smart home model
 * =====================================================================
 *   Controls the cardboard estate model: 2 houses, garage, main gate,
 *   clothesline with a rain cover, outdoor security light and fan.
 *
 *   Ways to control it
 *     1. Web dashboard served by the ESP32   -> http://smartestate.local
 *     2. Online web app (GitHub Pages) via MQTT -> from anywhere
 *     3. Alexa ("Alexa, open the gate")       -> Espalexa, no skill needed
 *     4. Voice commands in the web app        -> browser speech recognition
 *     5. Serial monitor (115200)              -> type e.g. "gate open"
 *
 *   Sensors : DHT11, PIR, rain sensor, 2x IR obstacle, magnetic door
 *   Outputs : 3x SG90 servo, 4x LED lights, fan, buzzer, 16x2 I2C LCD
 *
 *   Libraries (Arduino Library Manager):
 *     ESP32Servo, DHT sensor library (Adafruit) + Adafruit Unified Sensor,
 *     ArduinoJson (v7), Espalexa, PubSubClient, LiquidCrystal I2C
 * =====================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <DHT.h>

#include "config.h"
#include "webapp.h"   // generated from /webapp by tools/build_webapp.py

#if USE_LCD
  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>
#endif

#if USE_ALEXA
  #define ESPALEXA_MAXDEVICES 12
  #include <Espalexa.h>
#endif

#if USE_MQTT
  #include <PubSubClient.h>
  #if MQTT_PORT == 8883
    #include <WiFiClientSecure.h>
  #endif
#endif

// ---------------------------------------------------------------------
//  Global objects
// ---------------------------------------------------------------------
WebServer server(80);
DHT dht(PIN_DHT, DHTTYPE);
Servo servoGate, servoGarage, servoCover;

#if USE_LCD
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
#endif

#if USE_ALEXA
Espalexa espalexa;
#endif

#if USE_MQTT
  #if MQTT_PORT == 8883
    WiFiClientSecure netClient;
  #else
    WiFiClient netClient;
  #endif
PubSubClient mqtt(netClient);
String topicBase;
unsigned long lastMqttTry = 0;
unsigned long lastPublish = 0;
void publishState();
#endif

// ---------------------------------------------------------------------
//  Estate state (single source of truth, sent to every UI as JSON)
// ---------------------------------------------------------------------
struct EstateState {
  // outputs
  bool living = false, bedroom = false, garageLight = false, outdoor = false;
  bool fan = false;
  bool gateOpen = false, garageOpen = false, coverClosed = false;
  bool security = false, autoMode = true, alarm = false;
  // sensors
  float temp = NAN, hum = NAN;
  bool motion = false, carAtGate = false, carInGarage = false;
  bool doorOpen = false, raining = false;
  int rainLevel = 0;  // 0..100 %
} S;

bool stateDirty = true;          // something changed -> push to UIs
String lastEvent = "Booting";    // human readable last event

// automation helpers
unsigned long gateCloseAt = 0;      // 0 = no auto close scheduled
unsigned long outdoorOffAt = 0;     // 0 = outdoor light not on a timer
bool fanByTemp = false;             // fan was switched on by temperature
bool garageLightByCar = false;
unsigned long lastSensorRead = 0;
unsigned long lastLcd = 0;

// non-blocking buzzer beeps
int beepsLeft = 0;
unsigned long beepToggleAt = 0;
bool beepOn = false;

// ---------------------------------------------------------------------
//  Small helpers
// ---------------------------------------------------------------------
void logEvent(const String &msg) {
  lastEvent = msg;
  Serial.println("[estate] " + msg);
  stateDirty = true;
}

void beep(int times) {
  beepsLeft = times * 2;  // on + off phases
  beepToggleAt = millis();
}

// Returns 1 for on/open/true, 0 for off/close/false, -1 if unknown.
// "toggle" flips the current value.
int parseOnOff(String v, bool current) {
  v.trim();
  v.toLowerCase();
  if (v == "toggle") return current ? 0 : 1;
  if (v == "on" || v == "1" || v == "true" || v == "open" || v == "yes" ||
      v == "arm" || v == "armed" || v == "enable" || v == "start")
    return 1;
  if (v == "off" || v == "0" || v == "false" || v == "close" || v == "closed" ||
      v == "shut" || v == "no" || v == "disarm" || v == "disable" || v == "stop")
    return 0;
  return -1;
}

// ---------------------------------------------------------------------
//  Actuators
// ---------------------------------------------------------------------
void setLight(int pin, bool &field, bool on, const char *name) {
  if (field == on) return;
  field = on;
  digitalWrite(pin, on ? HIGH : LOW);
  logEvent(String(name) + (on ? " ON" : " OFF"));
}

void setFan(bool on) {
  if (S.fan == on) return;
  S.fan = on;
  digitalWrite(PIN_FAN, on ? HIGH : LOW);
  logEvent(on ? "Fan ON" : "Fan OFF");
}

void setGate(bool open) {
  if (S.gateOpen == open) return;
  S.gateOpen = open;
  servoGate.write(open ? GATE_OPEN_ANGLE : GATE_CLOSED_ANGLE);
  if (!open) gateCloseAt = 0;
  logEvent(open ? "Gate OPEN" : "Gate CLOSED");
}

void setGarage(bool open) {
  if (S.garageOpen == open) return;
  S.garageOpen = open;
  servoGarage.write(open ? GARAGE_OPEN_ANGLE : GARAGE_CLOSED_ANGLE);
  logEvent(open ? "Garage door OPEN" : "Garage door CLOSED");
}

void setCover(bool closed) {
  if (S.coverClosed == closed) return;
  S.coverClosed = closed;
  servoCover.write(closed ? COVER_CLOSED_ANGLE : COVER_OPEN_ANGLE);
  logEvent(closed ? "Clothesline cover CLOSED" : "Clothesline cover OPEN");
}

void setAlarm(bool on) {
  if (S.alarm == on) return;
  S.alarm = on;
  if (!on) digitalWrite(PIN_BUZZER, LOW);
  logEvent(on ? "ALARM! Intruder detected" : "Alarm silenced");
}

void setSecurity(bool on) {
  if (S.security == on) return;
  S.security = on;
  if (!on) setAlarm(false);
  beep(on ? 2 : 1);
  logEvent(on ? "Security ARMED" : "Security DISARMED");
}

// ---------------------------------------------------------------------
//  One command handler used by Web, MQTT, Alexa and Serial
// ---------------------------------------------------------------------
bool applyCommand(String device, const String &value, const char *source) {
  device.trim();
  device.toLowerCase();
  device.replace(" ", "_");
  Serial.printf("[cmd:%s] %s = %s\n", source, device.c_str(), value.c_str());

  if (device == "all_lights") {
    int v = parseOnOff(value, S.living);
    if (v < 0) return false;
    setLight(PIN_LED_LIVING, S.living, v, "Living room light");
    setLight(PIN_LED_BEDROOM, S.bedroom, v, "Bedroom light");
    setLight(PIN_LED_GARAGE, S.garageLight, v, "Garage light");
    setLight(PIN_LED_OUTDOOR, S.outdoor, v, "Outdoor light");
    outdoorOffAt = 0;
    return true;
  }
  if (device == "beep") { beep(value.toInt() > 0 ? value.toInt() : 1); return true; }

  bool *current = nullptr;
  if      (device == "living")       current = &S.living;
  else if (device == "bedroom")      current = &S.bedroom;
  else if (device == "garage_light") current = &S.garageLight;
  else if (device == "outdoor")      current = &S.outdoor;
  else if (device == "fan")          current = &S.fan;
  else if (device == "gate")         current = &S.gateOpen;
  else if (device == "garage_door")  current = &S.garageOpen;
  else if (device == "cover")        current = &S.coverClosed;
  else if (device == "security")     current = &S.security;
  else if (device == "auto")         current = &S.autoMode;
  else if (device == "alarm")        current = &S.alarm;
  if (!current) return false;

  // "cover": "close"/"on" = cover the clothesline, "open"/"off" = roll back
  String v = value;
  if (device == "cover") {
    String l = value; l.trim(); l.toLowerCase();
    if (l == "open") v = "off";
    else if (l == "close" || l == "closed" || l == "shut") v = "on";
  }
  int on = parseOnOff(v, *current);
  if (on < 0) return false;

  if      (device == "living")       setLight(PIN_LED_LIVING, S.living, on, "Living room light");
  else if (device == "bedroom")      setLight(PIN_LED_BEDROOM, S.bedroom, on, "Bedroom light");
  else if (device == "garage_light") { setLight(PIN_LED_GARAGE, S.garageLight, on, "Garage light"); garageLightByCar = false; }
  else if (device == "outdoor")      { setLight(PIN_LED_OUTDOOR, S.outdoor, on, "Outdoor light"); outdoorOffAt = 0; }
  else if (device == "fan")          { setFan(on); fanByTemp = false; }
  else if (device == "gate")         { setGate(on); gateCloseAt = 0; }
  else if (device == "garage_door")  setGarage(on);
  else if (device == "cover")        setCover(on);
  else if (device == "security")     setSecurity(on);
  else if (device == "alarm")        setAlarm(on);
  else if (device == "auto")         { S.autoMode = on; logEvent(on ? "Automation ON" : "Automation OFF"); }
  stateDirty = true;
  return true;
}

// ---------------------------------------------------------------------
//  JSON state
// ---------------------------------------------------------------------
String stateJson() {
  JsonDocument doc;
  doc["living"] = S.living;
  doc["bedroom"] = S.bedroom;
  doc["garage_light"] = S.garageLight;
  doc["outdoor"] = S.outdoor;
  doc["fan"] = S.fan;
  doc["gate"] = S.gateOpen;
  doc["garage_door"] = S.garageOpen;
  doc["cover"] = S.coverClosed;
  doc["security"] = S.security;
  doc["auto"] = S.autoMode;
  doc["alarm"] = S.alarm;
  if (isnan(S.temp)) doc["temp"] = nullptr; else doc["temp"] = round(S.temp * 10) / 10.0;
  if (isnan(S.hum))  doc["hum"]  = nullptr; else doc["hum"]  = round(S.hum);
  doc["motion"] = S.motion;
  doc["car_gate"] = S.carAtGate;
  doc["car_garage"] = S.carInGarage;
  doc["door_open"] = S.doorOpen;
  doc["raining"] = S.raining;
  doc["rain_level"] = S.rainLevel;
  doc["event"] = lastEvent;
  doc["ip"] = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["uptime"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();
  String out;
  serializeJson(doc, out);
  return out;
}

// ---------------------------------------------------------------------
//  Alexa (Espalexa emulates Philips Hue devices – no skill required)
//  Say: "Alexa, discover devices", then "Alexa, turn on living room light"
// ---------------------------------------------------------------------
#if USE_ALEXA
const char *ALEXA_NAMES[] = {"Living Room Light", "Bedroom Light", "Garage Light",
                             "Outdoor Light", "Fan", "Gate", "Garage Door",
                             "Rain Cover", "Security"};
const char *ALEXA_KEYS[]  = {"living", "bedroom", "garage_light", "outdoor", "fan",
                             "gate", "garage_door", "cover", "security"};
const int ALEXA_COUNT = sizeof(ALEXA_KEYS) / sizeof(ALEXA_KEYS[0]);
EspalexaDevice *alexaDevices[ALEXA_COUNT];

template <int I>
void alexaCallback(uint8_t brightness) {
  applyCommand(ALEXA_KEYS[I], brightness > 0 ? "on" : "off", "alexa");
}
typedef void (*AlexaCb)(uint8_t);
AlexaCb alexaCallbacks[] = {alexaCallback<0>, alexaCallback<1>, alexaCallback<2>,
                            alexaCallback<3>, alexaCallback<4>, alexaCallback<5>,
                            alexaCallback<6>, alexaCallback<7>, alexaCallback<8>};

bool stateFor(const char *key) {
  String k = key;
  if (k == "living") return S.living;
  if (k == "bedroom") return S.bedroom;
  if (k == "garage_light") return S.garageLight;
  if (k == "outdoor") return S.outdoor;
  if (k == "fan") return S.fan;
  if (k == "gate") return S.gateOpen;
  if (k == "garage_door") return S.garageOpen;
  if (k == "cover") return S.coverClosed;
  if (k == "security") return S.security;
  return false;
}

void syncAlexa() {  // keep the Alexa app in step with web/manual changes
  for (int i = 0; i < ALEXA_COUNT; i++) {
    if (alexaDevices[i]) alexaDevices[i]->setState(stateFor(ALEXA_KEYS[i]));
  }
}
#endif

// ---------------------------------------------------------------------
//  MQTT – online control from the hosted web app
//    smartestate/<HOME_ID>/state   (retained JSON, published by ESP32)
//    smartestate/<HOME_ID>/cmd     {"device":"gate","value":"open"}
//    smartestate/<HOME_ID>/online  "1" / "0" (last will)
// ---------------------------------------------------------------------
#if USE_MQTT
void onMqttMessage(char *topic, byte *payload, unsigned int len) {
  String msg;
  msg.reserve(len);
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];

  JsonDocument doc;
  if (deserializeJson(doc, msg) == DeserializationError::Ok && doc["device"].is<const char *>()) {
    String value = doc["value"].is<const char *>() ? String((const char *)doc["value"])
                 : doc["value"].is<bool>() ? String(doc["value"].as<bool>() ? "on" : "off")
                 : String(doc["value"].as<int>());
    if (String((const char *)doc["device"]) == "ping") { stateDirty = true; return; }
    applyCommand(doc["device"].as<const char *>(), value, "mqtt");
  } else {
    int eq = msg.indexOf('=');  // plain text form: gate=open
    if (eq > 0) applyCommand(msg.substring(0, eq), msg.substring(eq + 1), "mqtt");
  }
}

void mqttLoop() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!mqtt.connected()) {
    if (millis() - lastMqttTry < 5000) return;
    lastMqttTry = millis();
    String clientId = "smartestate-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    String willTopic = topicBase + "/online";
    const char *user = strlen(MQTT_USER) ? MQTT_USER : nullptr;
    const char *pass = strlen(MQTT_PASS) ? MQTT_PASS : nullptr;
    if (mqtt.connect(clientId.c_str(), user, pass, willTopic.c_str(), 0, true, "0")) {
      Serial.println("[mqtt] connected to " MQTT_HOST);
      mqtt.publish(willTopic.c_str(), "1", true);
      mqtt.subscribe((topicBase + "/cmd").c_str());
      stateDirty = true;
    } else {
      Serial.printf("[mqtt] connect failed, rc=%d\n", mqtt.state());
    }
    return;
  }
  mqtt.loop();
  if (millis() - lastPublish > STATE_PUBLISH_MS) publishState();  // heartbeat
}

void publishState() {
  if (!mqtt.connected()) return;  // a fresh state is sent on reconnect
  String json = stateJson();
  mqtt.publish((topicBase + "/state").c_str(), json.c_str(), true);
  lastPublish = millis();
}
#endif

// ---------------------------------------------------------------------
//  Web server (local dashboard + REST API)
// ---------------------------------------------------------------------
void sendCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleRoot() {
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", (const char *)WEBAPP_HTML_GZ, WEBAPP_HTML_GZ_LEN);
}

void handleState() {
  sendCors();
  server.send(200, "application/json", stateJson());
}

// GET/POST /api/cmd?device=gate&value=open   or JSON body {"device":..,"value":..}
void handleCmd() {
  sendCors();
  if (server.method() == HTTP_OPTIONS) { server.send(204); return; }
  String device = server.arg("device");
  String value = server.arg("value");
  if (device.isEmpty() && server.hasArg("plain")) {
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
      device = doc["device"] | "";
      value = doc["value"].is<bool>() ? String(doc["value"].as<bool>() ? "on" : "off")
                                      : String(doc["value"] | "toggle");
    }
  }
  if (value.isEmpty()) value = "toggle";
  bool ok = applyCommand(device, value, "web");
  server.send(ok ? 200 : 400, "application/json", ok ? stateJson() : "{\"error\":\"unknown device or value\"}");
}

void setupWeb() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/index.html", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/cmd", handleCmd);
  server.onNotFound([]() {
#if USE_ALEXA
    if (espalexa.handleAlexaApiCall(server.uri(), server.arg(0))) return;
#endif
    if (server.method() == HTTP_OPTIONS) { sendCors(); server.send(204); return; }
    server.send(404, "text/plain", "Not found");
  });

#if USE_ALEXA
  for (int i = 0; i < ALEXA_COUNT; i++) {
    alexaDevices[i] = new EspalexaDevice(ALEXA_NAMES[i], alexaCallbacks[i], 0);
    espalexa.addDevice(alexaDevices[i]);
  }
  espalexa.begin(&server);  // also starts the web server on port 80
#else
  server.begin();
#endif
}

// ---------------------------------------------------------------------
//  Wi-Fi (falls back to its own hotspot so the model works anywhere)
// ---------------------------------------------------------------------
void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to %s", WIFI_SSID);
#if USE_LCD
  lcd.clear(); lcd.print("Connecting WiFi");
#endif
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(400);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! Dashboard: http://");
    Serial.println(WiFi.localIP());
  } else {
    // No router? Make our own network: "SmartEstate" / smartestate
    WiFi.mode(WIFI_AP);
    WiFi.softAP("SmartEstate", "smartestate");
    Serial.print("Wi-Fi failed – hotspot 'SmartEstate' started. Open http://");
    Serial.println(WiFi.softAPIP());
  }
  if (MDNS.begin(DEVICE_HOSTNAME)) MDNS.addService("http", "tcp", 80);
}

// ---------------------------------------------------------------------
//  Sensors + automation
// ---------------------------------------------------------------------
void readSensors() {
  unsigned long now = millis();

  // --- digital sensors (read every loop) ---
  bool motion = digitalRead(PIN_PIR) == HIGH;
  bool carGate = digitalRead(PIN_IR_GATE) == LOW;
  bool carGarage = digitalRead(PIN_IR_GARAGE) == LOW;
  bool doorOpen = digitalRead(PIN_DOOR) == HIGH;  // magnet away -> pulled up

  // --- rain sensor: DO decides (set the threshold with the blue pot on
  //     the module), AO gives a 0-100 % wetness level for the dashboard ---
  int rainRaw = analogRead(PIN_RAIN_AO);                 // 0..4095, low = wet
  S.rainLevel = constrain(map(rainRaw, 4095, 0, 0, 100), 0, 100);
  bool raining = digitalRead(PIN_RAIN_DO) == LOW;

  // MOTION ------------------------------------------------------------
  if (motion != S.motion) {
    S.motion = motion;
    stateDirty = true;
    if (motion) {
      logEvent("Motion detected");
      if (S.security) setAlarm(true);
      if (S.autoMode && !S.outdoor) {
        setLight(PIN_LED_OUTDOOR, S.outdoor, true, "Outdoor light");
        outdoorOffAt = now + MOTION_LIGHT_MS;
      } else if (outdoorOffAt) {
        outdoorOffAt = now + MOTION_LIGHT_MS;
      }
    }
  }
  if (outdoorOffAt && (long)(now - outdoorOffAt) >= 0 && !S.motion) {
    outdoorOffAt = 0;
    setLight(PIN_LED_OUTDOOR, S.outdoor, false, "Outdoor light");
  }

  // CAR AT GATE -> auto open, close a few seconds after it has passed --
  if (carGate != S.carAtGate) {
    S.carAtGate = carGate;
    stateDirty = true;
    if (carGate) {
      logEvent("Car at the gate");
      if (S.autoMode && !S.security) { setGate(true); }
      else if (S.security) beep(3);
    }
  }
  if (S.carAtGate && S.gateOpen && S.autoMode) gateCloseAt = now + GATE_AUTO_CLOSE_MS;
  if (gateCloseAt && (long)(now - gateCloseAt) >= 0) {
    gateCloseAt = 0;
    setGate(false);
  }

  // CAR IN GARAGE -> garage light ---------------------------------------
  if (carGarage != S.carInGarage) {
    S.carInGarage = carGarage;
    stateDirty = true;
    logEvent(carGarage ? "Car parked in garage" : "Garage is empty");
    if (S.autoMode && carGarage && !S.garageLight) {
      setLight(PIN_LED_GARAGE, S.garageLight, true, "Garage light");
      garageLightByCar = true;
    } else if (S.autoMode && !carGarage && garageLightByCar) {
      setLight(PIN_LED_GARAGE, S.garageLight, false, "Garage light");
      garageLightByCar = false;
    }
  }

  // DOOR ----------------------------------------------------------------
  if (doorOpen != S.doorOpen) {
    S.doorOpen = doorOpen;
    logEvent(doorOpen ? "Front door opened" : "Front door closed");
    if (doorOpen && S.security) setAlarm(true);
  }

  // RAIN -> cover the clothesline ---------------------------------------
  if (raining != S.raining) {
    S.raining = raining;
    logEvent(raining ? "Rain detected!" : "Rain stopped");
    if (S.autoMode) {
      setCover(raining);
      if (raining) beep(2);
    }
  }

  // TEMPERATURE / HUMIDITY (slow sensor) --------------------------------
  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      if (isnan(S.temp) || fabs(t - S.temp) >= 0.2 || fabs(h - S.hum) >= 1) stateDirty = true;
      S.temp = t;
      S.hum = h;
      if (S.autoMode) {
        if (t >= FAN_ON_TEMP_C && !S.fan) { setFan(true); fanByTemp = true; }
        else if (t <= FAN_OFF_TEMP_C && S.fan && fanByTemp) { setFan(false); fanByTemp = false; }
      }
    }
  }
}

void updateBuzzer() {
  unsigned long now = millis();
  if (S.alarm) {  // fast siren pattern
    digitalWrite(PIN_BUZZER, (now / 200) % 2 ? HIGH : LOW);
    return;
  }
  if (beepsLeft > 0 && (long)(now - beepToggleAt) >= 0) {
    beepOn = !beepOn;
    digitalWrite(PIN_BUZZER, beepOn ? HIGH : LOW);
    beepsLeft--;
    beepToggleAt = now + 120;
    if (beepsLeft == 0) { beepOn = false; digitalWrite(PIN_BUZZER, LOW); }
  }
}

void updateLcd() {
#if USE_LCD
  if (millis() - lastLcd < 1000) return;
  lastLcd = millis();
  char line1[17], line2[17];
  if (isnan(S.temp)) snprintf(line1, sizeof(line1), "SMART ESTATE    ");
  else snprintf(line1, sizeof(line1), "T:%4.1fC H:%3d%% %c", S.temp, (int)S.hum, S.security ? 'S' : ' ');

  int page = (millis() / 3000) % 3;
  if (S.alarm)              snprintf(line2, sizeof(line2), "!!! INTRUDER !!!");
  else if (S.raining && page == 0) snprintf(line2, sizeof(line2), "Rain: cover shut");
  else if (page == 1)       snprintf(line2, sizeof(line2), "G:%-4s Gar:%-4s ", S.gateOpen ? "OPEN" : "SHUT", S.garageOpen ? "OPEN" : "SHUT");
  else {
    String ip = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    snprintf(line2, sizeof(line2), "%-16s", ip.c_str());
  }
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1); lcd.print(line2);
#endif
}

// Serial monitor control: "gate open", "living on", "all_lights off", "state"
void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.isEmpty()) return;
  if (line == "state") { Serial.println(stateJson()); return; }
  int sp = line.indexOf(' ');
  if (sp < 0) sp = line.indexOf('=');
  bool ok = sp > 0 && applyCommand(line.substring(0, sp), line.substring(sp + 1), "serial");
  if (!ok) Serial.println("Usage: <device> <on|off|open|close|toggle>  e.g. 'gate open'");
}

// ---------------------------------------------------------------------
//  setup / loop
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== SMART ESTATE starting ===");

  int outs[] = {PIN_LED_LIVING, PIN_LED_BEDROOM, PIN_LED_GARAGE, PIN_LED_OUTDOOR, PIN_FAN, PIN_BUZZER};
  for (int p : outs) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_IR_GATE, INPUT_PULLUP);
  pinMode(PIN_IR_GARAGE, INPUT);       // GPIO39 has no internal pull-up; IR module drives it
  pinMode(PIN_DOOR, INPUT_PULLUP);
  pinMode(PIN_RAIN_DO, INPUT);         // GPIO34 input-only; rain module drives it
  analogReadResolution(12);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servoGate.setPeriodHertz(50);
  servoGarage.setPeriodHertz(50);
  servoCover.setPeriodHertz(50);
  servoGate.attach(PIN_SERVO_GATE, 500, 2400);
  servoGarage.attach(PIN_SERVO_GARAGE, 500, 2400);
  servoCover.attach(PIN_SERVO_COVER, 500, 2400);
  servoGate.write(GATE_CLOSED_ANGLE);
  servoGarage.write(GARAGE_CLOSED_ANGLE);
  servoCover.write(COVER_OPEN_ANGLE);

  dht.begin();

#if USE_LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.print(" SMART  ESTATE ");
#endif

  setupWifi();
  setupWeb();

#if USE_MQTT
  topicBase = String("smartestate/") + HOME_ID;
  #if MQTT_PORT == 8883
  netClient.setInsecure();  // TLS without certificate pinning (fine for a school model)
  #endif
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(1024);
  mqtt.setCallback(onMqttMessage);
#endif

  beep(1);
  logEvent("Smart Estate ready");
}

void loop() {
#if USE_ALEXA
  espalexa.loop();      // also serves the web dashboard
#else
  server.handleClient();
#endif

  readSensors();
  updateBuzzer();
  handleSerial();

#if USE_MQTT
  mqttLoop();
#endif

  if (stateDirty) {     // push every change to Alexa + the online app
#if USE_ALEXA
    syncAlexa();
#endif
#if USE_MQTT
    publishState();
#endif
    stateDirty = false;
  }

  updateLcd();
  delay(2);
}
