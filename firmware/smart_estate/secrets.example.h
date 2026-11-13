// =====================================================================
//  Smart Estate – credentials TEMPLATE
//  1. Copy this file to "secrets.h" in the same folder.
//  2. Fill in your own values.  secrets.h is git-ignored.
// =====================================================================
#pragma once

// ---- Wi-Fi ----------------------------------------------------------
// For the Wokwi online simulator use: SSID "Wokwi-GUEST", password ""
// and uncomment  #define WOKWI_SIM
#define WIFI_SSID      "YOUR_WIFI_NAME"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
// #define WOKWI_SIM

// ---- Online control (MQTT) -----------------------------------------
// The web app talks to the ESP32 from anywhere through this broker.
// HOME_ID must match the "Home ID" you type in the web app settings.
// Make it long and random – anyone who knows it on a public broker
// can control your model.  For real privacy create a free HiveMQ Cloud
// cluster and put its host/user/password here.
#define MQTT_HOST      "broker.hivemq.com"
#define MQTT_PORT      1883
#define MQTT_USER      ""
#define MQTT_PASS      ""
#define HOME_ID        "estate-change-me-1234"
