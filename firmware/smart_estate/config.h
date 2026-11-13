// =====================================================================
//  Smart Estate – hardware & feature configuration
//  Board: ESP32 DevKit (ESP32-WROOM-32)
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
// ---------------------------------------------------------------------
#ifndef USE_LCD
  #define USE_LCD      1   // 16x2 I2C LCD
#endif
#ifndef USE_ALEXA
  #define USE_ALEXA    1   // Alexa voice control (Espalexa, no skill needed)
#endif
#ifndef USE_MQTT
  #define USE_MQTT     1   // Online/remote control through an MQTT broker
#endif

// Running in the Wokwi online simulator? (set in secrets for Wokwi)
#ifdef WOKWI_SIM
  #define DHTTYPE DHT22    // Wokwi only has DHT22
  #undef  USE_ALEXA
  #define USE_ALEXA 0      // Alexa discovery can't reach a simulator
#else
  #define DHTTYPE DHT11    // the blue DHT11 module in the kit
#endif

// ---------------------------------------------------------------------
//  PIN MAP  (see docs/WIRING.md for the full wiring table)
// ---------------------------------------------------------------------
// Sensors
#define PIN_DHT          4    // DHT11 temperature & humidity (DATA)
#define PIN_PIR          27   // HC-SR501 PIR motion sensor (OUT)
#define PIN_IR_GATE      32   // IR obstacle sensor at the gate (OUT, LOW = car)
#define PIN_IR_GARAGE    39   // IR obstacle sensor inside garage (OUT, LOW = car)
#define PIN_DOOR         33   // Magnetic reed door contact (to GND, INPUT_PULLUP)
#define PIN_RAIN_DO      34   // Rain sensor (raindrop) digital out (LOW = wet)
#define PIN_RAIN_AO      35   // Rain sensor analog out (lower = wetter)

// Actuators
#define PIN_SERVO_GATE   13   // SG90 – main gate
#define PIN_SERVO_GARAGE 14   // SG90 – garage door
#define PIN_SERVO_COVER  26   // SG90 – clothesline rain cover
#define PIN_BUZZER       25   // Active buzzer (via NPN transistor for loud ones)
#define PIN_LED_LIVING   16   // Living room light (LED + 220Ω)
#define PIN_LED_BEDROOM  17   // Bedroom light
#define PIN_LED_GARAGE   18   // Garage light
#define PIN_LED_OUTDOOR  19   // Outdoor / security light
#define PIN_FAN          23   // Fan / relay / motor driver input

// I2C LCD: SDA = 21, SCL = 22 (ESP32 defaults)
#define LCD_ADDRESS      0x27 // try 0x3F if the screen stays blank
#define LCD_COLS         16
#define LCD_ROWS         2

// ---------------------------------------------------------------------
//  Servo angles – tweak to fit your cardboard model
// ---------------------------------------------------------------------
#define GATE_CLOSED_ANGLE    0
#define GATE_OPEN_ANGLE      90
#define GARAGE_CLOSED_ANGLE  0
#define GARAGE_OPEN_ANGLE    100
#define COVER_OPEN_ANGLE     0    // cover rolled back (sunny)
#define COVER_CLOSED_ANGLE   120  // cover over clothesline (raining)

// ---------------------------------------------------------------------
//  Automation settings
// ---------------------------------------------------------------------
#define FAN_ON_TEMP_C        30.0  // auto fan above this temperature
#define FAN_OFF_TEMP_C       28.0  // ...and off below this one (hysteresis)
#define GATE_AUTO_CLOSE_MS   8000  // auto-close gate after a car passes
#define MOTION_LIGHT_MS      15000 // outdoor light stays on after motion
#define SENSOR_INTERVAL_MS   2000  // DHT read interval
#define STATE_PUBLISH_MS     3000  // MQTT heartbeat publish interval

#define DEVICE_HOSTNAME      "smartestate"  // http://smartestate.local
