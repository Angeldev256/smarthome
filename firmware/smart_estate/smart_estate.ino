/*
 * =====================================================================
 *   SMART ESTATE  –  ESP32 smart home model
 * =====================================================================
 *   Parts
 *     - LED lights in the buildings (living room, bedroom, garage, outdoor)
 *     - Gate motor: 28BYJ-48 stepper (turns GATE_TURN_DEGREES, e.g. 1080°)
 *       or a DC gear motor
 *     - Fan (DC motor) with 3 speeds + breeze mode + off-timer
 *     - RGB strip around the walls: shows the estate is ON + effects
 *     - Speaker (DFPlayer Mini): the estate talks ("Gate opening")
 *     - Push buttons for manual control
 *
 *   Ways to control it
 *     1. Push buttons on the model                  (manual)
 *     2. Web dashboard served by the ESP32          http://smartestate.local
 *     3. Online web app (GitHub Pages) via MQTT     from anywhere
 *     4. Alexa ("Alexa, turn on Gate")             Espalexa, no skill needed
 *     5. Voice commands in the web app              browser speech recognition
 *     6. Serial Monitor (115200)                    type e.g. "gate open"
 *
 *   Settings and pins: config.h      Wiring: docs/BLUEPRINT.md
 *
 *   Libraries (Arduino Library Manager):
 *     ArduinoJson (v7), Espalexa, PubSubClient
 *     + Adafruit NeoPixel   (only for a WS2812 strip)
 *     + LiquidCrystal I2C   (only if USE_LCD is 1)
 * =====================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "esp_timer.h"

#include "config.h"
#include "webapp.h"   // generated from /webapp by tools/build_webapp.py

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

#if USE_RGB_STRIP && RGB_STRIP_TYPE == RGB_WS2812
  #include <Adafruit_NeoPixel.h>
#endif

#if USE_LCD
  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>
#endif

// ---------------------------------------------------------------------
//  PWM helpers (work with ESP32 Arduino core 2.x and 3.x)
// ---------------------------------------------------------------------
#define PWM_BITS 8
#define PWM_MAX  255
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void pwmAttach(int pin, int /*channel*/, uint32_t freq) { ledcAttach(pin, freq, PWM_BITS); }
void pwmWrite(int pin, int /*channel*/, uint32_t duty) { ledcWrite(pin, duty); }
#else
void pwmAttach(int pin, int channel, uint32_t freq) { ledcSetup(channel, freq, PWM_BITS); ledcAttachPin(pin, channel); }
void pwmWrite(int /*pin*/, int channel, uint32_t duty) { ledcWrite(channel, duty); }
#endif
// PWM channels (only used by core 2.x)
enum { CH_LIVING = 0, CH_BEDROOM, CH_GARAGE, CH_OUTDOOR, CH_FAN, CH_STRIP_R = 6, CH_STRIP_G, CH_STRIP_B };

// ---------------------------------------------------------------------
//  Global objects
// ---------------------------------------------------------------------
WebServer server(80);

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
unsigned long lastMqttTry = 0, lastPublish = 0;
void publishState();
#endif

#if USE_LCD
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);
#endif

bool stateDirty = true;          // something changed -> push to every UI
String lastEvent = "Booting";
bool quiet = false;              // true while a scene runs (one announcement only)

void logEvent(const String &msg) {
  lastEvent = msg;
  Serial.println("[estate] " + msg);
  stateDirty = true;
}

// Returns 1 for on/open/true, 0 for off/close/false, -1 if unknown.
int parseOnOff(String v, bool current) {
  v.trim();
  v.toLowerCase();
  if (v == "toggle") return current ? 0 : 1;
  if (v == "on" || v == "1" || v == "true" || v == "open" || v == "yes" || v == "start") return 1;
  if (v == "off" || v == "0" || v == "false" || v == "close" || v == "closed" || v == "shut" ||
      v == "no" || v == "stop")
    return 0;
  return -1;
}
bool isNumber(const String &v) {
  if (v.isEmpty()) return false;
  for (unsigned i = 0; i < v.length(); i++) if (!isDigit(v[i])) return false;
  return true;
}

// =====================================================================
//  SPEAKER – DFPlayer Mini, driven with its serial protocol directly
//  (no extra library).  Files: /mp3/0001.mp3, 0002.mp3 ... on the SD card.
// =====================================================================
enum Track {
  T_NONE = 0, T_ONLINE, T_GATE_OPENING, T_GATE_CLOSING, T_GATE_STOPPED,
  T_LIGHTS_ON, T_LIGHTS_OFF, T_LIVING_ON, T_LIVING_OFF, T_BEDROOM_ON, T_BEDROOM_OFF,
  T_GARAGE_ON, T_GARAGE_OFF, T_OUTDOOR_ON, T_OUTDOOR_OFF,
  T_FAN_OFF, T_FAN_1, T_FAN_2, T_FAN_3, T_FAN_BREEZE, T_STRIP_ON, T_STRIP_OFF,
  T_GOODNIGHT, T_WELCOME, T_PARTY, T_DOORBELL, T_HOTSPOT, T_HELLO, T_BEEP, T_GATE_OPEN, T_GATE_CLOSED,
  T_COUNT
};
// names usable with  say=<name>  (index = track number)
const char *TRACK_NAMES[T_COUNT] = {
  "", "online", "gate_opening", "gate_closing", "gate_stopped",
  "lights_on", "lights_off", "living_on", "living_off", "bedroom_on", "bedroom_off",
  "garage_on", "garage_off", "outdoor_on", "outdoor_off",
  "fan_off", "fan_1", "fan_2", "fan_3", "fan_breeze", "strip_on", "strip_off",
  "goodnight", "welcome", "party", "doorbell", "hotspot", "hello", "beep", "gate_open", "gate_closed"};

struct Speaker {
  int volume = SPEAKER_VOLUME;   // 0..30
  bool announce = SPEAKER_ANNOUNCE;
  int pending = 0;               // next track to send
  int lastTrack = 0;
  unsigned long readyAt = 0;     // DFPlayer needs ~1.5 s after power-on
  unsigned long nextSendAt = 0;  // keep commands ~150 ms apart
  bool volumeSent = false;
} SPK;

#if USE_SPEAKER
void dfSend(uint8_t cmd, uint16_t param) {
  uint8_t f[10] = {0x7E, 0xFF, 0x06, cmd, 0x00, (uint8_t)(param >> 8), (uint8_t)param, 0, 0, 0xEF};
  int16_t sum = 0;
  for (int i = 1; i < 7; i++) sum -= f[i];
  f[7] = (uint8_t)(sum >> 8);
  f[8] = (uint8_t)sum;
  Serial2.write(f, 10);
}
#endif

void say(int track, bool force = false) {
  if (track <= 0 || track >= T_COUNT) return;
  if (!force && (quiet || !SPK.announce)) return;
  SPK.pending = track;
}

void speakerLoop() {
#if USE_SPEAKER
  unsigned long now = millis();
  if ((long)(now - SPK.readyAt) < 0 || (long)(now - SPK.nextSendAt) < 0) return;
  if (!SPK.volumeSent) { dfSend(0x06, SPK.volume); SPK.volumeSent = true; SPK.nextSendAt = now + 150; return; }
  if (SPK.pending) {
    dfSend(0x12, SPK.pending);  // play /mp3/000N.mp3
    SPK.lastTrack = SPK.pending;
    SPK.pending = 0;
    SPK.nextSendAt = now + 150;
  }
#endif
}

void setVolume(int v) {
  SPK.volume = constrain(v, 0, 30);
  SPK.volumeSent = false;
  logEvent("Speaker volume " + String(SPK.volume));
}

// =====================================================================
//  LIGHTS – dimmable LEDs (0..100 %)
// =====================================================================
struct Light {
  const char *key, *name;
  int pin, ch, trackOn, trackOff;
  int level, lastLevel;  // lastLevel = brightness to return to when switched on
};
Light LIGHTS[] = {
  {"living",       "Living room light", PIN_LED_LIVING,  CH_LIVING,  T_LIVING_ON,  T_LIVING_OFF,  0, 100},
  {"bedroom",      "Bedroom light",     PIN_LED_BEDROOM, CH_BEDROOM, T_BEDROOM_ON, T_BEDROOM_OFF, 0, 100},
  {"garage_light", "Garage light",      PIN_LED_GARAGE,  CH_GARAGE,  T_GARAGE_ON,  T_GARAGE_OFF,  0, 100},
  {"outdoor",      "Outdoor light",     PIN_LED_OUTDOOR, CH_OUTDOOR, T_OUTDOOR_ON, T_OUTDOOR_OFF, 0, 100},
};
const int LIGHT_COUNT = sizeof(LIGHTS) / sizeof(LIGHTS[0]);

Light *findLight(const String &key) {
  for (auto &l : LIGHTS) if (key == l.key) return &l;
  return nullptr;
}

void setLightLevel(Light &l, int level) {
  level = constrain(level, 0, 100);
  if (level == l.level) return;
  bool wasOn = l.level > 0;
  l.level = level;
  if (level > 0) l.lastLevel = level;
  pwmWrite(l.pin, l.ch, (uint32_t)level * level * PWM_MAX / 10000);  // gamma: looks even to the eye
  if (wasOn != (level > 0)) {
    logEvent(String(l.name) + (level ? " ON" : " OFF"));
    say(level ? l.trackOn : l.trackOff);
  } else {
    logEvent(String(l.name) + " " + level + "%");
  }
}

bool anyLightOn() {
  for (auto &l : LIGHTS) if (l.level) return true;
  return false;
}

void setAllLights(bool on) {
  bool q = quiet;
  quiet = true;  // one announcement for all of them
  for (auto &l : LIGHTS) setLightLevel(l, on ? l.lastLevel : 0);
  quiet = q;
  logEvent(on ? "All lights ON" : "All lights OFF");
  say(on ? T_LIGHTS_ON : T_LIGHTS_OFF);
}

// =====================================================================
//  GATE MOTOR
//  Position is counted in steps (stepper) or milliseconds (DC motor).
//  0 = closed, GATE_FULL = open.  The motor moves towards gateTarget.
// =====================================================================
#if GATE_MOTOR == GATE_STEPPER
const long GATE_FULL = (long)GATE_TURN_DEGREES * GATE_STEPS_PER_REV / 360;
const uint64_t GATE_STEP_US = 60000000ULL / ((uint64_t)GATE_SPEED_RPM * GATE_STEPS_PER_REV);
const uint8_t STEP_SEQ[4][4] = {{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 1}, {1, 0, 0, 1}};
const int GATE_PINS[4] = {PIN_GATE_IN1, PIN_GATE_IN2, PIN_GATE_IN3, PIN_GATE_IN4};
esp_timer_handle_t gateTimer = nullptr;
volatile bool coilsOn = false;
#else
const long GATE_FULL = GATE_DC_RUN_MS;
unsigned long gateLastMs = 0;
#endif
volatile long gatePos = 0;     // 0 .. GATE_FULL
volatile long gateTarget = 0;
const char *gateLastState = "closed";
unsigned long gateCloseAt = 0;

#if GATE_MOTOR == GATE_STEPPER
// Runs every GATE_STEP_US in the esp_timer task: one step towards the target.
void gateTick(void *) {
  long pos = gatePos, target = gateTarget;
  if (pos == target) {
    if (coilsOn) {  // de-energise the coils when idle (motor stays cool)
      for (int i = 0; i < 4; i++) digitalWrite(GATE_PINS[i], LOW);
      coilsOn = false;
    }
    return;
  }
  pos += target > pos ? 1 : -1;
  gatePos = pos;
  long phase = GATE_REVERSE ? -pos : pos;
  const uint8_t *s = STEP_SEQ[((phase % 4) + 4) % 4];
  for (int i = 0; i < 4; i++) digitalWrite(GATE_PINS[i], s[i]);
  coilsOn = true;
}
#else
void gateDcDrive(int dir) {  // +1 open, -1 close, 0 stop
  if (GATE_REVERSE) dir = -dir;
  digitalWrite(PIN_GATE_IN1, dir > 0);
  digitalWrite(PIN_GATE_IN2, dir < 0);
}
#endif

bool gateMoving() { return gatePos != gateTarget; }
// true when the gate is open or on its way to open (used by "toggle")
bool gateWantsOpen() { return gateTarget == GATE_FULL || (gateMoving() && gateTarget > gatePos); }

const char *gateState() {
  if (gatePos != gateTarget) return gateTarget > gatePos ? "opening" : "closing";
  if (gatePos >= GATE_FULL) return "open";
  if (gatePos <= 0) return "closed";
  return "stopped";
}
int gatePercent() { return (int)(gatePos * 100 / (GATE_FULL > 0 ? GATE_FULL : 1)); }

void setGate(bool open) {
  long target = open ? GATE_FULL : 0;
  if (gateTarget == target) return;
  gateTarget = target;
  gateCloseAt = 0;
  logEvent(open ? "Gate opening" : "Gate closing");
  say(open ? T_GATE_OPENING : T_GATE_CLOSING);
}

void stopGate() {
  if (!gateMoving()) return;
  gateTarget = gatePos;
  logEvent("Gate stopped");
  say(T_GATE_STOPPED);
}

void gateLoop() {
#if GATE_MOTOR == GATE_DC
  unsigned long now = millis();
  long dt = (long)(now - gateLastMs);
  gateLastMs = now;
  if (gatePos != gateTarget) {
    long dir = gateTarget > gatePos ? 1 : -1;
    long pos = gatePos + dir * dt;
    if ((dir > 0 && pos >= gateTarget) || (dir < 0 && pos <= gateTarget)) pos = gateTarget;
    gatePos = pos;
    gateDcDrive(pos == gateTarget ? 0 : dir);
  } else {
    gateDcDrive(0);
  }
#endif
  // report when the gate finishes moving
  const char *st = gateState();
  if (strcmp(st, gateLastState) != 0) {
    gateLastState = st;
    stateDirty = true;
    if (!strcmp(st, "open")) {
      logEvent("Gate OPEN");
      say(T_GATE_OPEN);
      if (GATE_AUTO_CLOSE_S > 0) gateCloseAt = millis() + GATE_AUTO_CLOSE_S * 1000UL;
    } else if (!strcmp(st, "closed")) {
      logEvent("Gate CLOSED");
      say(T_GATE_CLOSED);
    }
  }
  if (gateCloseAt && (long)(millis() - gateCloseAt) >= 0) {
    gateCloseAt = 0;
    setGate(false);
  }
  // live position for the app while moving
  static unsigned long lastPosPush = 0;
  if (gateMoving() && millis() - lastPosPush > 500) { lastPosPush = millis(); stateDirty = true; }
}

// =====================================================================
//  FAN – speed 0..3, breeze mode, off-timer.  No sensor needed.
// =====================================================================
struct Fan {
  int speed = 0;                 // 0 off, 1 low, 2 medium, 3 high
  int lastSpeed = 2;             // speed used by "fan on"
  bool breeze = false;           // gently changes speed like wind
  unsigned long kickUntil = 0;   // full power for a moment when starting
  unsigned long offAt = 0;       // off-timer (0 = none)
} FAN;

void setFan(int speed, bool breeze = false) {
  speed = constrain(speed, 0, 3);
  if (speed == 0) breeze = false;
  if (speed == FAN.speed && breeze == FAN.breeze) return;
  if (FAN.speed == 0 && speed > 0) FAN.kickUntil = millis() + FAN_KICK_MS;
  FAN.speed = speed;
  FAN.breeze = breeze;
  if (speed) FAN.lastSpeed = speed;
  else FAN.offAt = 0;
  if (breeze) { logEvent("Fan breeze mode"); say(T_FAN_BREEZE); }
  else if (speed == 0) { logEvent("Fan OFF"); say(T_FAN_OFF); }
  else { logEvent("Fan speed " + String(speed)); say(T_FAN_1 + speed - 1); }
}

void fanLoop() {
  unsigned long now = millis();
  if (FAN.offAt && (long)(now - FAN.offAt) >= 0) {
    FAN.offAt = 0;
    setFan(0);
    logEvent("Fan timer finished – fan OFF");
  }
  int pct = 0;
  if (FAN.speed > 0) {
    const int duty[4] = {0, FAN_DUTY_LOW, FAN_DUTY_MED, FAN_DUTY_HIGH};
    pct = duty[FAN.speed];
    if (FAN.breeze) {  // slow wave between low and high, ~12 s per gust
      float w = 0.5f + 0.5f * sinf(now / 1900.0f) * sinf(now / 3100.0f + 1.0f);
      pct = FAN_DUTY_LOW + (int)(w * (FAN_DUTY_HIGH - FAN_DUTY_LOW));
    }
    if ((long)(now - FAN.kickUntil) < 0) pct = 100;
  }
  pwmWrite(PIN_FAN, CH_FAN, (uint32_t)pct * PWM_MAX / 100);
}

// =====================================================================
//  RGB STRIP
// =====================================================================
enum StripEffect { FX_STATUS = 0, FX_BREATHE, FX_RAINBOW, FX_PARTY, FX_COUNT };
const char *FX_NAMES[FX_COUNT] = {"status", "breathe", "rainbow", "party"};

struct Strip {
  bool on = true;                       // glows from power-on = "estate is ON"
  uint32_t color = STRIP_START_COLOR;   // 0xRRGGBB
  int brightness = STRIP_START_BRIGHTNESS;
  int effect = FX_STATUS;
  unsigned long flashUntil = 0;         // short white flash (doorbell, button)
  uint32_t flashColor = 0xFFFFFF;
} STRIP;

#if USE_RGB_STRIP && RGB_STRIP_TYPE == RGB_WS2812
Adafruit_NeoPixel pixels(STRIP_LED_COUNT, PIN_STRIP_DATA, NEO_GRB + NEO_KHZ800);
#endif

struct NamedColor { const char *name; uint32_t rgb; };
const NamedColor COLORS[] = {
  {"red", 0xFF0000}, {"green", 0x00FF40}, {"blue", 0x0030FF}, {"white", 0xFFFFFF},
  {"warm", 0xFF9A40}, {"warm white", 0xFF9A40}, {"yellow", 0xFFD000}, {"orange", 0xFF6000},
  {"purple", 0x9000FF}, {"violet", 0x9000FF}, {"pink", 0xFF2080}, {"cyan", 0x00FFFF},
  {"aqua", 0x00FFC0}, {"gold", 0xFFB000}, {"magenta", 0xFF00FF}};

bool parseColor(String v, uint32_t &out) {
  v.trim();
  v.toLowerCase();
  if (v.startsWith("#")) v = v.substring(1);
  if (v.length() == 6) {
    bool hex = true;
    for (unsigned i = 0; i < 6; i++) if (!isHexadecimalDigit(v[i])) hex = false;
    if (hex) { out = strtoul(v.c_str(), nullptr, 16); return true; }
  }
  for (auto &c : COLORS) if (v == c.name) { out = c.rgb; return true; }
  return false;
}

uint32_t hsv(uint16_t hue, uint8_t sat = 255) {  // hue 0..65535 -> 0xRRGGBB
  uint8_t r, g, b;
  uint32_t h = (uint32_t)hue * 6;
  uint8_t sector = h >> 16, f = (h >> 8) & 0xFF;
  uint8_t p = 255 - sat, q = 255 - (sat * f >> 8), t = 255 - (sat * (255 - f) >> 8);
  switch (sector % 6) {
    case 0: r = 255; g = t; b = p; break;
    case 1: r = q; g = 255; b = p; break;
    case 2: r = p; g = 255; b = t; break;
    case 3: r = p; g = q; b = 255; break;
    case 4: r = t; g = p; b = 255; break;
    default: r = 255; g = p; b = q; break;
  }
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint32_t scale(uint32_t c, int pct) {  // brightness 0..100 with gamma
  uint32_t k = (uint32_t)pct * pct;    // 0..10000
  return (((c >> 16 & 0xFF) * k / 10000) << 16) | (((c >> 8 & 0xFF) * k / 10000) << 8) | ((c & 0xFF) * k / 10000);
}

void stripWrite(uint32_t c) {  // whole strip one colour
#if USE_RGB_STRIP
  #if RGB_STRIP_TYPE == RGB_WS2812
  pixels.fill(pixels.Color(c >> 16 & 0xFF, c >> 8 & 0xFF, c & 0xFF));
  pixels.show();
  #else
  pwmWrite(PIN_STRIP_R, CH_STRIP_R, c >> 16 & 0xFF);
  pwmWrite(PIN_STRIP_G, CH_STRIP_G, c >> 8 & 0xFF);
  pwmWrite(PIN_STRIP_B, CH_STRIP_B, c & 0xFF);
  #endif
#endif
}

void stripLoop() {
#if USE_RGB_STRIP
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last < 25) return;  // 40 frames per second
  last = now;

  if ((long)(now - STRIP.flashUntil) < 0) {  // flash beats everything
    stripWrite(((now / 120) % 2) ? scale(STRIP.flashColor, 100) : 0);
    return;
  }
  if (!STRIP.on) { stripWrite(0); return; }

  int br = STRIP.brightness;
  switch (STRIP.effect) {
    case FX_STATUS:
      if (WiFi.getMode() == WIFI_AP) {               // no Wi-Fi: slow red blink
        stripWrite((now / 800) % 2 ? scale(0xFF0000, br) : scale(0xFF0000, br / 4));
      } else if (gateMoving()) {                     // gate moving: amber pulse
        int p = 30 + (int)(70 * (0.5f + 0.5f * sinf(now / 150.0f)));
        stripWrite(scale(0xFF7000, p * br / 100));
      } else {
        stripWrite(scale(STRIP.color, br));
      }
      break;
    case FX_BREATHE: {
      int p = 15 + (int)(85 * (0.5f + 0.5f * sinf(now / 900.0f)));
      stripWrite(scale(STRIP.color, p * br / 100));
      break;
    }
    case FX_RAINBOW:
  #if RGB_STRIP_TYPE == RGB_WS2812
      for (int i = 0; i < STRIP_LED_COUNT; i++) {
        uint32_t c = scale(hsv((uint16_t)(now * 12 + i * 65536L / STRIP_LED_COUNT)), br);
        pixels.setPixelColor(i, c >> 16 & 0xFF, c >> 8 & 0xFF, c & 0xFF);
      }
      pixels.show();
  #else
      stripWrite(scale(hsv((uint16_t)(now * 12)), br));
  #endif
      break;
    case FX_PARTY:
      stripWrite(scale(hsv((uint16_t)((now / 250) * 40503UL)), br));  // jumps colour 4x a second
      break;
  }
#endif
}

void stripFlash(uint32_t color, int ms) {
  STRIP.flashColor = color;
  STRIP.flashUntil = millis() + ms;
}

void setStripOn(bool on) {
  if (STRIP.on == on) return;
  STRIP.on = on;
  logEvent(on ? "Strip lights ON" : "Strip lights OFF");
  say(on ? T_STRIP_ON : T_STRIP_OFF);
}

void setStripEffect(int fx) {
  STRIP.effect = ((fx % FX_COUNT) + FX_COUNT) % FX_COUNT;
  STRIP.on = true;
  logEvent(String("Strip effect: ") + FX_NAMES[STRIP.effect]);
}

void setStripColor(uint32_t c) {
  STRIP.color = c;
  STRIP.on = true;
  if (STRIP.effect != FX_BREATHE) STRIP.effect = FX_STATUS;
  char buf[24];
  snprintf(buf, sizeof(buf), "Strip colour #%06lX", (unsigned long)c);
  logEvent(buf);
}

// =====================================================================
//  SCENES
// =====================================================================
void runScene(String name) {
  name.toLowerCase();
  name.replace(" ", "");
  name.replace("_", "");
  quiet = true;
  if (name == "goodnight" || name == "night") {
    setAllLights(false);
    setGate(false);
    setFan(1);
    STRIP.on = true; STRIP.effect = FX_STATUS; STRIP.color = 0x0030FF; STRIP.brightness = 10;
    quiet = false; logEvent("Scene: good night"); say(T_GOODNIGHT);
  } else if (name == "welcome" || name == "welcomehome" || name == "home") {
    setGate(true);
    setLightLevel(LIGHTS[0], 100);  // living room
    setLightLevel(LIGHTS[3], 100);  // outdoor
    STRIP.on = true; STRIP.effect = FX_STATUS; STRIP.color = STRIP_START_COLOR; STRIP.brightness = STRIP_START_BRIGHTNESS;
    quiet = false; logEvent("Scene: welcome home"); say(T_WELCOME);
  } else if (name == "party") {
    setAllLights(true);
    setFan(2);
    STRIP.on = true; STRIP.effect = FX_PARTY; STRIP.brightness = 100;
    quiet = false; logEvent("Scene: party"); say(T_PARTY);
  } else if (name == "normal" || name == "partyoff") {
    STRIP.on = true; STRIP.effect = FX_STATUS; STRIP.color = STRIP_START_COLOR; STRIP.brightness = STRIP_START_BRIGHTNESS;
    quiet = false; logEvent("Scene: normal");
  }
  quiet = false;
  stateDirty = true;
}

// =====================================================================
//  ONE command handler used by buttons, Web, MQTT, Alexa and Serial
// =====================================================================
bool applyCommand(String device, String value, const char *source) {
  device.trim(); device.toLowerCase(); device.replace(" ", "_");
  value.trim(); value.toLowerCase();
  Serial.printf("[cmd:%s] %s = %s\n", source, device.c_str(), value.c_str());
  stateDirty = true;

  // --- lights -------------------------------------------------------
  if (Light *l = findLight(device)) {
    if (isNumber(value)) { setLightLevel(*l, value.toInt()); return true; }
    int on = parseOnOff(value, l->level > 0);
    if (on < 0) return false;
    setLightLevel(*l, on ? l->lastLevel : 0);
    return true;
  }
  if (device == "all_lights" || device == "lights") {
    int on = parseOnOff(value, anyLightOn());
    if (on < 0) return false;
    setAllLights(on);
    return true;
  }

  // --- gate ---------------------------------------------------------
  if (device == "gate") {
    if (value == "stop") { stopGate(); return true; }
    int open = parseOnOff(value, gateWantsOpen());
    if (open < 0) return false;
    setGate(open);
    return true;
  }

  // --- fan ----------------------------------------------------------
  if (device == "fan") {
    if (value == "next" || value == "cycle") { setFan((FAN.speed + 1) % 4); return true; }
    if (value == "breeze" || value == "natural" || value == "wind") { setFan(FAN.speed ? FAN.speed : 2, true); return true; }
    if (value == "low" || value == "slow") { setFan(1); return true; }
    if (value == "medium" || value == "mid") { setFan(2); return true; }
    if (value == "high" || value == "fast" || value == "max") { setFan(3); return true; }
    if (value == "faster" || value == "up") { setFan(min(FAN.speed + 1, 3)); return true; }
    if (value == "slower" || value == "down") { setFan(max(FAN.speed - 1, 0)); return true; }
    if (isNumber(value)) { setFan(value.toInt()); return true; }
    int on = parseOnOff(value, FAN.speed > 0);
    if (on < 0) return false;
    setFan(on ? FAN.lastSpeed : 0);
    return true;
  }
  if (device == "fan_timer") {  // minutes, 0 = cancel
    int m = value.toInt();
    if (m <= 0) { FAN.offAt = 0; logEvent("Fan timer cancelled"); return true; }
    if (!FAN.speed) setFan(FAN.lastSpeed);
    FAN.offAt = millis() + (unsigned long)m * 60000UL;
    logEvent("Fan switches off in " + String(m) + " min");
    return true;
  }

  // --- RGB strip ----------------------------------------------------
  if (device == "strip" || device == "strip_effect" || device == "strip_color" || device == "strip_brightness") {
    uint32_t c;
    if (device == "strip_brightness" || (device == "strip" && isNumber(value))) {
      STRIP.brightness = constrain(value.toInt(), 0, 100);
      STRIP.on = STRIP.brightness > 0;
      if (!STRIP.brightness) STRIP.brightness = STRIP_START_BRIGHTNESS;
      logEvent("Strip brightness " + String(STRIP.brightness) + "%");
      return true;
    }
    if (value == "next") { setStripEffect(STRIP.on ? STRIP.effect + 1 : STRIP.effect); return true; }
    for (int i = 0; i < FX_COUNT; i++) if (value == FX_NAMES[i]) { setStripEffect(i); return true; }
    if (value == "solid") { setStripEffect(FX_STATUS); return true; }
    if (parseColor(value, c)) { setStripColor(c); return true; }
    int on = parseOnOff(value, STRIP.on);
    if (on < 0) return false;
    setStripOn(on);
    return true;
  }

  // --- speaker ------------------------------------------------------
  if (device == "volume") {
    if (value == "up" || value == "louder") setVolume(SPK.volume + 4);
    else if (value == "down" || value == "quieter" || value == "softer") setVolume(SPK.volume - 4);
    else if (value == "mute" || value == "off") setVolume(0);
    else if (value == "unmute" || value == "on") setVolume(SPEAKER_VOLUME);
    else if (isNumber(value)) setVolume(value.toInt());
    else return false;
    say(T_BEEP, true);
    return true;
  }
  if (device == "announce") {
    int on = parseOnOff(value, SPK.announce);
    if (on < 0) return false;
    SPK.announce = on;
    logEvent(on ? "Announcements ON" : "Announcements OFF");
    return true;
  }
  if (device == "say") {
    int t = isNumber(value) ? value.toInt() : -1;
    for (int i = 1; i < T_COUNT && t < 0; i++) if (value == TRACK_NAMES[i]) t = i;
    if (t <= 0) return false;
    say(t, true);
    return true;
  }
  if (device == "beep") { say(T_BEEP, true); return true; }
  if (device == "doorbell") {
    say(T_DOORBELL, true);
    stripFlash(0xFFFFFF, 1500);
    logEvent("Ding dong! Someone is at the gate");
    return true;
  }

  // --- scenes -------------------------------------------------------
  if (device == "scene") { runScene(value); return true; }
  if (device == "goodnight" || device == "welcome" || device == "party") {
    int on = parseOnOff(value, false);
    if (device == "party" && on == 0) { runScene("normal"); return true; }
    if (on == 1 || value.isEmpty()) { runScene(device); return true; }
    return on == 0;
  }
  return false;
}

// =====================================================================
//  STATE as JSON (for the app, MQTT and /api/state)
// =====================================================================
String stateJson() {
  JsonDocument doc;
  for (auto &l : LIGHTS) {
    doc[l.key] = l.level > 0;
    doc[String(l.key) + "_level"] = l.level;
  }
  doc["fan"] = FAN.speed > 0;
  doc["fan_speed"] = FAN.speed;
  doc["fan_breeze"] = FAN.breeze;
  long fanLeft = FAN.offAt ? (long)(FAN.offAt - millis()) : 0;
  doc["fan_timer"] = fanLeft > 0 ? fanLeft / 60000L + 1 : 0;  // minutes left
  doc["gate"] = gateWantsOpen();
  doc["gate_state"] = gateState();
  doc["gate_pos"] = gatePercent();
  doc["strip"] = STRIP.on;
  char hex[8];
  snprintf(hex, sizeof(hex), "#%06lX", (unsigned long)(STRIP.color & 0xFFFFFF));
  doc["strip_color"] = hex;
  doc["strip_effect"] = FX_NAMES[STRIP.effect];
  doc["strip_brightness"] = STRIP.brightness;
  doc["volume"] = SPK.volume;
  doc["announce"] = SPK.announce;
  doc["event"] = lastEvent;
  doc["ip"] = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["uptime"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();
  JsonObject f = doc["features"].to<JsonObject>();
  f["speaker"] = (bool)USE_SPEAKER;
  f["strip"] = (bool)USE_RGB_STRIP;
  f["strip_type"] = RGB_STRIP_TYPE == RGB_WS2812 ? "ws2812" : "analog";
  f["gate_motor"] = GATE_MOTOR == GATE_STEPPER ? "stepper" : "dc";
  f["gate_degrees"] = GATE_TURN_DEGREES;
  f["alexa"] = (bool)USE_ALEXA;
  String out;
  serializeJson(doc, out);
  return out;
}

// =====================================================================
//  ALEXA (Espalexa makes the ESP32 look like Philips Hue lights)
//  "Alexa, discover devices" then "Alexa, turn on Living Room Light"
// =====================================================================
#if USE_ALEXA
EspalexaDevice *alexaLights[LIGHT_COUNT];
EspalexaDevice *alexaFan, *alexaGate, *alexaStrip, *alexaParty, *alexaNight;

uint8_t pctTo255(int pct) { return (uint8_t)((pct * 255 + 50) / 100); }
int b255ToPct(uint8_t b) { return b ? max(1, (b * 100 + 127) / 255) : 0; }

template <int I>
void alexaLight(uint8_t b) { applyCommand(LIGHTS[I].key, String(b255ToPct(b)), "alexa"); }
void (*const ALEXA_LIGHT_CB[])(uint8_t) = {alexaLight<0>, alexaLight<1>, alexaLight<2>, alexaLight<3>};
const char *const ALEXA_LIGHT_NAMES[] = {"Living Room Light", "Bedroom Light", "Garage Light", "Outdoor Light"};

void alexaFanCb(uint8_t b) {  // 1-33 % = low, 34-66 % = medium, 67-100 % = high
  int p = b255ToPct(b);
  applyCommand("fan", String(p == 0 ? 0 : p <= 33 ? 1 : p <= 66 ? 2 : 3), "alexa");
}
void alexaGateCb(EspalexaDevice *d) { applyCommand("gate", d->getValue() ? "open" : "close", "alexa"); }
void alexaPartyCb(EspalexaDevice *d) { applyCommand("party", d->getValue() ? "on" : "off", "alexa"); }
void alexaNightCb(EspalexaDevice *d) { if (d->getValue()) applyCommand("scene", "goodnight", "alexa"); }
void alexaStripCb(uint8_t b, uint32_t rgb) {
  if (!b) { applyCommand("strip", "off", "alexa"); return; }
  STRIP.brightness = b255ToPct(b);
  if (rgb) setStripColor(rgb);
  else setStripOn(true);
  stateDirty = true;
}

void setupAlexa() {
  for (int i = 0; i < LIGHT_COUNT; i++) {
    alexaLights[i] = new EspalexaDevice(ALEXA_LIGHT_NAMES[i], ALEXA_LIGHT_CB[i], 0);
    espalexa.addDevice(alexaLights[i]);
  }
  alexaFan = new EspalexaDevice("Fan", alexaFanCb, 0);
  alexaGate = new EspalexaDevice("Gate", alexaGateCb, EspalexaDeviceType::onoff, 0);
  alexaStrip = new EspalexaDevice("Strip Light", alexaStripCb, pctTo255(STRIP_START_BRIGHTNESS));
  alexaParty = new EspalexaDevice("Party Mode", alexaPartyCb, EspalexaDeviceType::onoff, 0);
  alexaNight = new EspalexaDevice("Good Night", alexaNightCb, EspalexaDeviceType::onoff, 0);
  espalexa.addDevice(alexaFan);
  espalexa.addDevice(alexaGate);
  espalexa.addDevice(alexaStrip);
  espalexa.addDevice(alexaParty);
  espalexa.addDevice(alexaNight);
}

void syncAlexa() {  // keep the Alexa app in step with buttons / app / voice
  for (int i = 0; i < LIGHT_COUNT; i++) {
    if (LIGHTS[i].level) alexaLights[i]->setPercent(LIGHTS[i].level);
    else alexaLights[i]->setState(false);
  }
  const int fanPct[4] = {0, 33, 66, 100};
  if (FAN.speed) alexaFan->setPercent(fanPct[FAN.speed]); else alexaFan->setState(false);
  alexaGate->setState(gateWantsOpen());
  alexaStrip->setState(STRIP.on);
  alexaParty->setState(STRIP.on && STRIP.effect == FX_PARTY);
  alexaNight->setState(false);
}
#endif

// =====================================================================
//  MQTT – online control from the hosted web app
//    smartestate/<HOME_ID>/state   (retained JSON, published by ESP32)
//    smartestate/<HOME_ID>/cmd     {"device":"gate","value":"open"}
//    smartestate/<HOME_ID>/online  "1" / "0" (last will)
// =====================================================================
#if USE_MQTT
void onMqttMessage(char *topic, byte *payload, unsigned int len) {
  String msg;
  msg.reserve(len);
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  JsonDocument doc;
  if (deserializeJson(doc, msg) == DeserializationError::Ok && doc["device"].is<const char *>()) {
    String device = doc["device"].as<const char *>();
    if (device == "ping") { stateDirty = true; return; }
    String value = doc["value"].is<const char *>() ? String(doc["value"].as<const char *>())
                 : doc["value"].is<bool>() ? String(doc["value"].as<bool>() ? "on" : "off")
                 : String(doc["value"].as<int>());
    applyCommand(device, value, "mqtt");
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
  if (millis() - lastPublish > STATE_PUBLISH_MS) publishState();
}

void publishState() {
  if (!mqtt.connected()) return;
  String json = stateJson();
  mqtt.publish((topicBase + "/state").c_str(), json.c_str(), true);
  lastPublish = millis();
}
#endif

// =====================================================================
//  WEB SERVER (dashboard + REST API)
// =====================================================================
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
  setupAlexa();
  espalexa.begin(&server);  // also starts the web server on port 80
#else
  server.begin();
#endif
}

// =====================================================================
//  WI-FI (falls back to its own hotspot so the model works anywhere)
// =====================================================================
void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to %s", WIFI_SSID);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    stripWrite((millis() / 300) % 2 ? scale(0x0050FF, 60) : 0);  // blue blink = connecting
    delay(100);
    if (millis() % 500 < 100) Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! Dashboard: http://");
    Serial.println(WiFi.localIP());
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("SmartEstate", "smartestate");
    Serial.print("Wi-Fi failed – hotspot 'SmartEstate' (password smartestate). Open http://");
    Serial.println(WiFi.softAPIP());
    say(T_HOTSPOT, true);
  }
  if (MDNS.begin(DEVICE_HOSTNAME)) MDNS.addService("http", "tcp", 80);
}

// =====================================================================
//  MANUAL BUTTONS (short press / long press, debounced)
// =====================================================================
const ButtonConfig BUTTON_CFG[] = BUTTONS;
const int BUTTON_COUNT = sizeof(BUTTON_CFG) / sizeof(BUTTON_CFG[0]);
struct ButtonState { bool down = false; bool longDone = false; unsigned long since = 0; unsigned long changedAt = 0; };
ButtonState btn[BUTTON_COUNT];

void setupButtons() {
  for (int i = 0; i < BUTTON_COUNT; i++) {
    int p = BUTTON_CFG[i].pin;
    if (p < 0) continue;
    pinMode(p, p >= 34 ? INPUT : INPUT_PULLUP);  // 34-39 have no internal pull-up
  }
}

void buttonsLoop() {
  unsigned long now = millis();
  for (int i = 0; i < BUTTON_COUNT; i++) {
    const ButtonConfig &c = BUTTON_CFG[i];
    if (c.pin < 0) continue;
    bool pressed = digitalRead(c.pin) == LOW;
    ButtonState &b = btn[i];
    if (pressed != b.down && now - b.changedAt > 30) {  // 30 ms debounce
      b.changedAt = now;
      b.down = pressed;
      if (pressed) { b.since = now; b.longDone = false; }
      else if (!b.longDone) {                          // released: short press
        stripFlash(0xFFFFFF, 120);
        applyCommand(c.device, c.value, "button");
      }
    }
    if (b.down && !b.longDone && now - b.since > 1000 && c.longValue) {  // held 1 s
      b.longDone = true;
      stripFlash(0xFF3000, 240);
      applyCommand(c.device, c.longValue, "button");
    }
  }
}

// =====================================================================
//  Optional LCD
// =====================================================================
void updateLcd() {
#if USE_LCD
  static unsigned long last = 0;
  if (millis() - last < 1000) return;
  last = millis();
  char l1[17], l2[17];
  snprintf(l1, sizeof(l1), "Gate:%-7s F:%d ", gateState(), FAN.speed);
  if ((millis() / 4000) % 2) {
    String ip = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    snprintf(l2, sizeof(l2), "%-16s", ip.c_str());
  } else {
    snprintf(l2, sizeof(l2), "L:%c%c%c%c Strip:%-4s ", LIGHTS[0].level ? '1' : '-', LIGHTS[1].level ? '2' : '-',
             LIGHTS[2].level ? '3' : '-', LIGHTS[3].level ? '4' : '-', STRIP.on ? "ON" : "OFF");
  }
  lcd.setCursor(0, 0); lcd.print(l1);
  lcd.setCursor(0, 1); lcd.print(l2);
#endif
}

// Serial monitor control: "gate open", "living 50", "fan 3", "strip red", "state"
void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.isEmpty()) return;
  if (line == "state") { Serial.println(stateJson()); return; }
  int sp = line.indexOf(' ');
  if (sp < 0) sp = line.indexOf('=');
  bool ok = sp > 0 && applyCommand(line.substring(0, sp), line.substring(sp + 1), "serial");
  if (!ok) Serial.println("Usage: <device> <value>   e.g.  gate open | living 50 | fan 3 | strip red | scene party");
}

// =====================================================================
//  setup / loop
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== SMART ESTATE starting ===");

  // lights + fan (PWM)
  for (auto &l : LIGHTS) { pwmAttach(l.pin, l.ch, 5000); pwmWrite(l.pin, l.ch, 0); }
  pwmAttach(PIN_FAN, CH_FAN, 20000);  // 20 kHz: no motor whine
  pwmWrite(PIN_FAN, CH_FAN, 0);

  // gate motor
#if GATE_MOTOR == GATE_STEPPER
  for (int p : GATE_PINS) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  esp_timer_create_args_t args = {};
  args.callback = gateTick;
  args.name = "gate";
  esp_timer_create(&args, &gateTimer);
  esp_timer_start_periodic(gateTimer, GATE_STEP_US);
  Serial.printf("Gate: stepper, %d degrees = %ld steps\n", GATE_TURN_DEGREES, GATE_FULL);
#else
  pinMode(PIN_GATE_IN1, OUTPUT);
  pinMode(PIN_GATE_IN2, OUTPUT);
  gateDcDrive(0);
  gateLastMs = millis();
#endif

  // RGB strip – on straight away so you can see the estate has power
#if USE_RGB_STRIP
  #if RGB_STRIP_TYPE == RGB_WS2812
  pixels.begin();
  #else
  pwmAttach(PIN_STRIP_R, CH_STRIP_R, 5000);
  pwmAttach(PIN_STRIP_G, CH_STRIP_G, 5000);
  pwmAttach(PIN_STRIP_B, CH_STRIP_B, 5000);
  #endif
  stripWrite(scale(STRIP_START_COLOR, STRIP_START_BRIGHTNESS));
#endif

  // speaker
#if USE_SPEAKER
  Serial2.begin(9600, SERIAL_8N1, PIN_SPEAKER_RX, PIN_SPEAKER_TX);
  SPK.readyAt = millis() + 1500;
#endif

  setupButtons();

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
  netClient.setInsecure();
  #endif
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(1024);
  mqtt.setCallback(onMqttMessage);
#endif

  if (WiFi.getMode() != WIFI_AP) say(T_ONLINE, true);
  logEvent("Smart Estate ready");
}

void loop() {
#if USE_ALEXA
  espalexa.loop();  // also serves the web dashboard
#else
  server.handleClient();
#endif

  buttonsLoop();
  gateLoop();
  fanLoop();
  stripLoop();
  speakerLoop();
  handleSerial();

#if USE_MQTT
  mqttLoop();
#endif

  if (stateDirty) {
#if USE_ALEXA
    syncAlexa();
#endif
#if USE_MQTT
    publishState();
#endif
    stateDirty = false;
  }

  updateLcd();
  delay(1);
}
