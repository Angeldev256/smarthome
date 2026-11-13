# 🏡 Smart Estate

An **ESP32 smart home estate model** with a web dashboard, **Alexa**, **voice commands**,
online remote control and an online simulator. It runs on the cardboard model below:
two houses, a garage, a driveway, a servo gate and clotheslines that a rain sensor covers automatically.

![The Smart Estate model](docs/images/estate-model.jpg)

## What it does

| Feature | Sensor → Action |
|---|---|
| 🌧 **Rain protection** | Rain sensor detects rain → servo swings a cover over the clothesline, buzzer beeps |
| 🚗 **Automatic gate** | IR sensor sees a car → gate servo opens, then closes 8 s after the car passes |
| 🅿 **Smart garage** | IR sensor in the garage → garage light turns on. Garage door servo opens or closes on command |
| 🚶 **Security light** | PIR motion → outdoor light on for 15 s |
| 🚨 **Alarm** | Security armed + motion or front door opened (reed switch) → buzzer siren, red alert on the app |
| 🌡 **Climate** | DHT11 temperature/humidity → fan on above 30 °C, off below 28 °C |
| 💡 **Lights** | Living room, bedroom, garage, outdoor LEDs |
| 📟 **LCD panel** | 16×2 I2C shows temperature, humidity, gate/garage state and the IP address |
| 🗣 **Alexa** | "Alexa, turn on Gate", "Alexa, turn on Living Room Light". No skill needed |
| 🎙 **Voice in the browser** | "open the garage", "is it raining?", "good night" |
| 🌍 **Online** | Control from anywhere through MQTT, or run everything in the **Wokwi** simulator |

## Repository layout

```
firmware/smart_estate/     ESP32 Arduino sketch
  smart_estate.ino         main program (sensors, automation, web, Alexa, MQTT)
  config.h                 pin map, servo angles, thresholds  ← edit to match your wiring
  secrets.example.h        copy to secrets.h and add Wi-Fi + Home ID
  webapp.h                 the dashboard, packed for the ESP32 (generated)
webapp/                    the web app (index.html, style.css, app.js, photos)
wokwi/                     diagram.json + files to run the firmware in the Wokwi simulator
tools/build_webapp.py      packs /webapp into firmware/smart_estate/webapp.h
docs/WIRING.md             parts list, pin table, power wiring, where parts go in the model
docs/ALEXA_AND_VOICE.md    Alexa setup + all voice commands
docs/ONLINE_AND_SIMULATOR.md   GitHub Pages, Wokwi, MQTT remote control
.github/workflows/         auto-compile check + GitHub Pages deploy
```

## Quick start (Arduino IDE on your PC)

1. **Get the code:** on GitHub click **Code → Download ZIP** (or `git clone https://github.com/Angeldev256/smarthome`) and unzip it.
2. **Install the Arduino IDE 2** from arduino.cc.
3. **Add the ESP32 boards:** *File → Preferences → Additional boards manager URLs*:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   Then *Tools → Board → Boards Manager* → install **esp32 by Espressif Systems**.
4. **Install the libraries:** *Sketch → Include Library → Manage Libraries* and install:
   * `ESP32Servo` (Kevin Harrington)
   * `DHT sensor library` (Adafruit) + `Adafruit Unified Sensor`
   * `ArduinoJson` (Benoit Blanchon, v7)
   * `Espalexa` (Christian Schwinne)
   * `PubSubClient` (Nick O'Leary)
   * `LiquidCrystal I2C` (Frank de Brabander)
5. **Add your Wi-Fi:** in `firmware/smart_estate/` copy `secrets.example.h` to **`secrets.h`** and fill in `WIFI_SSID`, `WIFI_PASSWORD` and a unique `HOME_ID`.
6. Open `firmware/smart_estate/smart_estate.ino`, choose *Tools → Board → **ESP32 Dev Module***, pick the COM port, and click **Upload**.
   (If it sticks on `Connecting....`, hold the **BOOT** button on the ESP32 until upload starts.)
7. Open the *Serial Monitor* at **115200**. It prints the address, e.g. `http://192.168.1.50`. Open it on your phone, or use **http://smartestate.local**.
8. Say **"Alexa, discover devices"**.

Wire it up first using **[docs/WIRING.md](docs/WIRING.md)**.

## The web app

* **On the ESP32:** `http://smartestate.local`. It switches to local mode automatically.
* **Online:** GitHub Pages (see [docs/ONLINE_AND_SIMULATOR.md](docs/ONLINE_AND_SIMULATOR.md)). Choose ⚙ → *Online* to reach the model through MQTT from anywhere.
* **Demo:** with no hardware it runs a simulated estate with Rain, Motion, Car and Door buttons, so you can present the project from any laptop.

It has a live top-down map of the estate (lights glow, gate swings, rain falls, cover closes),
sensor tiles, switches for every device, an activity log, a microphone button for voice, and the parts gallery.

If you change anything in `webapp/`, run `python3 tools/build_webapp.py` so the ESP32 copy is updated.

## API (for your own apps)

| Request | Result |
|---|---|
| `GET /api/state` | JSON with every sensor and device |
| `GET or POST /api/cmd?device=gate&value=open` | send a command, returns the new state |

Devices: `living`, `bedroom`, `garage_light`, `outdoor`, `all_lights`, `fan`, `gate`, `garage_door`, `cover`, `security`, `alarm`, `auto`, `beep`.
Values: `on/off`, `open/close`, `toggle`. The same commands work over MQTT (`smartestate/<HOME_ID>/cmd`) and in the Serial Monitor (`gate open`).

## The parts

![Components](docs/images/components-layout.jpg)

ESP32 · breadboard + jumpers · 3× SG90 servo · rain sensor · DHT11 · PIR HC-SR501 · 2× IR obstacle sensor ·
magnetic door switch · buzzer · 16×2 I2C LCD · LEDs · fan · buck converter · 12 V adapter.
