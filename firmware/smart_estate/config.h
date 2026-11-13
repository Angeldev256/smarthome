// =====================================================================
//  Smart Estate – hardware & feature configuration
//  Board: ESP32 DevKit (ESP32-WROOM-32)
//
//  This is the ONLY file you normally need to edit.
//  The wiring that matches these pins is in docs/BLUEPRINT.md.
// =====================================================================
#pragma once

// ---------------------------------------------------------------------
//  Credentials: copy secrets.example.h -> secrets.h and fill it in.
//  secrets.h is git-ignored so your Wi-Fi password never gets pushed.
// ---------------------------------------------------------------------
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #include "secrets.example.h"
#endif

// ---------------------------------------------------------------------
//  Feature switches (1 = on, 0 = off)
//  A part that is switched on but not plugged in does no harm.
// ---------------------------------------------------------------------
#ifndef USE_ALEXA
  #define USE_ALEXA    1   // Alexa voice control (Espalexa, no skill needed)
#endif
#ifndef USE_MQTT
  #define USE_MQTT     1   // Control from anywhere through an MQTT broker
#endif
#ifndef USE_SPEAKER
  #define USE_SPEAKER  1   // DFPlayer Mini + speaker: the model talks
#endif
#ifndef USE_RGB_STRIP
  #define USE_RGB_STRIP 1  // RGB strip around the estate walls
#endif
#ifndef USE_LCD
  #define USE_LCD      0   // optional 16x2 I2C LCD (SDA 21, SCL 22)
#endif

#ifdef WOKWI_SIM
  #undef  USE_ALEXA
  #define USE_ALEXA 0      // Alexa can't find a simulated board
#endif

// =====================================================================
//  1. BUILDING LIGHTS  (LEDs, dimmable)
//  Each pin can drive up to 3 small LEDs in parallel, each LED with its
//  own 220 Ω resistor.  For more LEDs use a transistor (see blueprint).
// =====================================================================
#define PIN_LED_LIVING    16   // left house – living room
#define PIN_LED_BEDROOM   17   // right house – bedroom
#define PIN_LED_GARAGE    18   // garage (middle back)
#define PIN_LED_OUTDOOR   19   // compound / gate pillars

// =====================================================================
//  2. GATE MOTOR
//  Choose the motor you have:
//    GATE_STEPPER : 28BYJ-48 stepper + ULN2003 board (recommended).
//                   Turns an exact number of degrees, e.g. 1080° = 3 turns.
//    GATE_DC      : DC gear motor + L298N / L9110S / L293D driver.
//                   Runs for GATE_DC_RUN_MS to open, the same time back to close.
// =====================================================================
#define GATE_STEPPER 1
#define GATE_DC      2
#ifndef GATE_MOTOR
  #define GATE_MOTOR GATE_STEPPER
#endif

// >>> How far the gate motor turns to OPEN the gate (degrees). <<<
// 1080 = three full turns.  Use 360, 720, 1440 ... or any number you like.
// To close, the motor turns the same amount back.
#define GATE_TURN_DEGREES  1080

#define GATE_REVERSE       0      // 1 = swap direction if the gate opens the wrong way
#define GATE_SPEED_RPM     12     // stepper speed: 5 (slow, strong) .. 15 (fast)
#define GATE_STEPS_PER_REV 2048   // 28BYJ-48 in full-step mode
#define GATE_AUTO_CLOSE_S  0      // close the gate by itself after N seconds (0 = never)

// Stepper: ULN2003 board IN1..IN4
#define PIN_GATE_IN1  13
#define PIN_GATE_IN2  14
#define PIN_GATE_IN3  27
#define PIN_GATE_IN4  26
// DC motor: driver IN1 / IN2 (uses the same first two wires)
#define GATE_DC_RUN_MS   4000    // time for the DC motor to fully open the gate

// =====================================================================
//  3. FAN  (small DC motor fan through a transistor or MOSFET)
//  No temperature sensor needed – you pick the speed by voice, app,
//  Alexa or the fan button.  Speeds: 0 = off, 1 = low, 2 = medium, 3 = high.
//  "Breeze" mode gently moves between low and high like natural wind.
// =====================================================================
#define PIN_FAN           23
#define FAN_DUTY_LOW      45     // % power for speed 1 (raise it if the fan stalls)
#define FAN_DUTY_MED      70     // % power for speed 2
#define FAN_DUTY_HIGH     100    // % power for speed 3
#define FAN_KICK_MS       300    // full power for a moment so a slow fan can start

// =====================================================================
//  4. RGB STRIP around the estate walls
//  It glows as soon as the estate is powered, so you can see the whole
//  setup is ON.  It also flashes to show what is happening (gate moving,
//  doorbell, Wi-Fi problem ...).
//    RGB_ANALOG : normal 12 V strip with 4 wires (+12V, R, G, B) and
//                 3 MOSFETs/transistors.
//    RGB_WS2812 : "addressable" 5 V strip with 3 wires (5V, DIN, GND).
//                 Easiest wiring – needs the Adafruit NeoPixel library.
// =====================================================================
#define RGB_ANALOG 1
#define RGB_WS2812 2
#ifndef RGB_STRIP_TYPE
  #define RGB_STRIP_TYPE RGB_ANALOG
#endif
#define PIN_STRIP_R     25     // analog strip: red   MOSFET gate
#define PIN_STRIP_G     33     // analog strip: green MOSFET gate
#define PIN_STRIP_B     32     // analog strip: blue  MOSFET gate
#define PIN_STRIP_DATA  25     // WS2812 strip: DIN (through a 330 Ω resistor)
#define STRIP_LED_COUNT 60     // WS2812 only: number of LEDs on the strip
#define STRIP_START_COLOR 0x00FF60  // colour at power-on (green = all good)
#define STRIP_START_BRIGHTNESS 60   // % at power-on

// =====================================================================
//  5. SPEAKER  (DFPlayer Mini MP3 module + 3 W / 0.5 W speaker)
//  Copy the files from /sdcard to a micro-SD card (see sdcard/README.md).
//  Only ONE data wire is needed: ESP32 TX -> 1 kΩ -> DFPlayer RX.
// =====================================================================
#define PIN_SPEAKER_TX    4      // ESP32 GPIO4 -> 1 kΩ -> DFPlayer RX
#define PIN_SPEAKER_RX    35     // optional: DFPlayer TX -> GPIO35 (can stay unconnected)
#define SPEAKER_VOLUME    22     // 0..30 at power-on
#define SPEAKER_ANNOUNCE  1      // 1 = speak every change ("Gate opening")

// =====================================================================
//  6. MANUAL BUTTONS  (push buttons between the pin and GND – nothing else)
//  Each button sends a normal command, exactly like the app or voice.
//  Change the device/value to make a button do something else.
//  Pins 34, 35, 36, 39 also work but need a 10 kΩ resistor to 3V3.
// =====================================================================
struct ButtonConfig { int pin; const char *device; const char *value; const char *longValue; };
// pin, device, short press, long press (hold 1 s)
#define BUTTONS { \
  {  5, "all_lights", "toggle", "off"     },  /* Lights: toggle all / hold = all off      */ \
  { 15, "gate",       "toggle", "stop"    },  /* Gate: open-close / hold = stop           */ \
  { 12, "fan",        "next",   "off"     },  /* Fan: next speed / hold = off             */ \
  {  0, "strip",      "next",   "toggle"  },  /* BOOT button on the ESP32: strip effect   */ \
}

// =====================================================================
//  Optional 16x2 I2C LCD  (SDA = 21, SCL = 22)
// =====================================================================
#define LCD_ADDRESS      0x27 // try 0x3F if the screen stays blank

#define STATE_PUBLISH_MS 3000           // MQTT heartbeat
#define DEVICE_HOSTNAME  "smartestate"  // http://smartestate.local
