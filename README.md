# 🏡 Smart Estate

An **ESP32 smart home estate model** with a web dashboard, **Alexa**, **voice commands**, **buttons**,
a **talking speaker** and online remote control. It runs on the cardboard model below:
two houses, a garage, a driveway and a motor-driven gate.

![The Smart Estate model](docs/images/estate-model.jpg)

## What it does

| Part | What it does | Control it with |
|---|---|---|
| 💡 **Building lights** | LEDs in the living room, bedroom, garage and on the gate pillars. Dimmable | App, voice, Alexa, Lights button |
| 🚪 **Gate motor** | DC gear motor on an **L298N**: runs to open, back to close, stops on command. (A stepper turning exactly 1080° or a servo also work) | App, voice, Alexa, Gate button |
| 🌀 **Fan** | 3 speeds, breeze mode, off-timer. No sensor needed | App, voice, Alexa, Fan button |
| 🌈 **RGB strip** | Glows around the walls as soon as the estate has power, shows what's happening, any colour, effects | App, voice, Alexa, BOOT button |
| 🔊 **Speaker** | Says every change out loud in real time: "Gate opening", "Fan speed two", "Welcome home!" | Automatic, volume by app or voice |
| 🚗 **Ultrasonic sensors** | Car at the gate → gate opens, closes after it passes. Garage parking helper: light on, "Stop!" at 5 cm | Automatic |
| 🚶 **Motion sensor (PIR)** | Outdoor lights on for 20 s. Security armed → siren, red flashing strip, alert in the app | Automatic, security by app, voice, Alexa |
| 🎬 **Scenes** | Welcome home · Good night · Party · Doorbell | App, voice, Alexa |
| 🗣 **Alexa** | "Alexa, turn on Gate", "Alexa, set Strip Light to blue". No skill needed | |
| 🎙 **Voice in the browser** | "open the gate", "fan speed three", "dim the bedroom to 30 percent" | |
| 🌍 **Online** | Control from anywhere through MQTT, or run everything in the **Wokwi** simulator | |

📐 **Blueprint and simplified wiring: [docs/BLUEPRINT.md](docs/BLUEPRINT.md)** ·
🔀 **Other parts (DC motor, servo, big speaker, LED power, real bulb): [docs/ALTERNATIVES.md](docs/ALTERNATIVES.md)**

![Layout blueprint](docs/blueprint/blueprint-layout.png)

## Repository layout

```
firmware/smarthome/
  smarthome.ino            THE WHOLE FIRMWARE IN ONE FILE
                           PART 1 = your settings (Wi-Fi, pins, gate time, options)
                           PART 2 = the program, PART 3 = the web page (generated)
webapp/                    the web app (index.html, style.css, app.js, pictures)
sdcard/mp3/                the speaker's voice clips – copy to the DFPlayer's micro-SD card
wokwi/                     diagram.json + files to run the firmware in the Wokwi simulator
tools/build_webapp.py      packs /webapp into the end of smarthome.ino
tools/make_voice_clips.py  makes the speaker clips (only if you want other words)
INSTALL-WINDOWS.bat        one-click Windows installer (tools/windows/install-windows.ps1)
docs/INSTALL.md            installation guide + troubleshooting
docs/RUN_THE_APP.md        open the app: from the ESP32, online, demo
docs/BLUEPRINT.md          layout blueprint, simplified wiring, pin table, build order
docs/ALTERNATIVES.md       DC motor, servo, big/small speaker, LED power, real bulb, fan, strip
docs/ALEXA_AND_VOICE.md    Alexa setup, voice phrases, buttons, all commands
docs/ONLINE_AND_SIMULATOR.md   GitHub Pages, Wokwi, MQTT remote control
.github/workflows/         auto-compile check + GitHub Pages deploy
```

## Install

📦 **Full step-by-step guide: [docs/INSTALL.md](docs/INSTALL.md)**

**Windows, the quick way:** download the ZIP of branch `claude/wonderful-galileo-4p3xz7`, unzip it and double-click
**`INSTALL-WINDOWS.bat`**. It copies the sketch to `Documents\Arduino\smarthome`, asks for your Wi-Fi, installs the
ESP32 boards and libraries, uploads to the ESP32, copies the speaker clips to the SD card and opens the app.

## Quick start (Arduino IDE by hand)

1. **Get the code:** on GitHub click **Code → Download ZIP** (or `git clone https://github.com/Angeldev256/smarthome`) and unzip it.
2. **Install the Arduino IDE 2** from arduino.cc.
3. **Add the ESP32 boards:** *File → Preferences → Additional boards manager URLs*:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   Then *Tools → Board → Boards Manager* → install **esp32 by Espressif Systems**.
4. **Install the libraries:** *Sketch → Include Library → Manage Libraries* and install:
   * `ArduinoJson` (Benoit Blanchon, v7)
   * `Espalexa` (Christian Schwinne)
   * `PubSubClient` (Nick O'Leary)
   * only for a 3-wire WS2812 strip: `Adafruit NeoPixel`
5. **Open the sketch:** open `firmware/smarthome/smarthome.ino` in the Arduino IDE.
   (Or, in your own empty `smarthome` sketch, select all, delete, and paste in the whole of `smarthome.ino`.)
6. **Add your Wi-Fi** at the top (PART 1): `WIFI_SSID`, `WIFI_PASSWORD` and a unique `HOME_ID`.
   Choose *Tools → Board → **ESP32 Dev Module***, pick the COM port, and click **Upload**.
   (If it sticks on `Connecting....`, hold the **BOOT** button on the ESP32 until upload starts.)
7. Open the *Serial Monitor* at **115200**. It prints the address, e.g. `http://192.168.1.50`. Open it on your phone, or use **http://smartestate.local**.
8. Copy `sdcard/mp3` onto the DFPlayer's micro-SD card. The estate says *"Smart estate is online"* when it starts.
9. Say **"Alexa, discover devices"**.

Wire it up first using **[docs/BLUEPRINT.md](docs/BLUEPRINT.md)**. To change how far the gate turns, edit
`GATE_DC_RUN_MS` in PART 1 of `smarthome.ino` (how many milliseconds the DC motor needs to open the gate).
**Run the app live (phone, laptop, anywhere): [docs/RUN_THE_APP.md](docs/RUN_THE_APP.md).**
Using a DC motor, a servo, a big speaker or a real bulb instead? See **[docs/ALTERNATIVES.md](docs/ALTERNATIVES.md)**.

## The web app

* **On the ESP32:** `http://smartestate.local`. It switches to local mode automatically.
* **Online:** GitHub Pages (see [docs/ONLINE_AND_SIMULATOR.md](docs/ONLINE_AND_SIMULATOR.md)). Choose ⚙ → *Online* to reach the model through MQTT from anywhere.
* **Demo:** with no hardware it runs a simulated estate with Motion, Car-at-gate and Park-in-garage buttons, so you can present the project from any laptop.

It has a live top-down map of the estate (lights glow, the gate swings as the motor turns, the fan spins at its speed,
the strip glows in its colour, cars arrive, the alarm flashes), sensor and security panel, dimmers, fan speed buttons, colour swatches, scenes, an activity log,
a microphone button for voice, and the blueprint.

If you change anything in `webapp/`, run `python3 tools/build_webapp.py` so the ESP32 copy is updated.

## API (for your own apps)

| Request | Result |
|---|---|
| `GET /api/state` | JSON with every sensor and device |
| `GET or POST /api/cmd?device=gate&value=open` | send a command, returns the new state |

Devices: `living`, `bedroom`, `garage_light`, `outdoor`, `all_lights`, `gate`, `fan`, `fan_timer`, `strip`, `strip_color`,
`strip_effect`, `strip_brightness`, `security`, `alarm`, `auto`, `volume`, `announce`, `say`, `doorbell`, `scene`.
Examples: `gate=open`, `gate=stop`, `living=40`, `fan=3`, `fan=breeze`, `strip_color=#ff8800`, `scene=party`.
The full list is in [docs/ALEXA_AND_VOICE.md](docs/ALEXA_AND_VOICE.md). The same commands work over MQTT
(`smartestate/<HOME_ID>/cmd`) and in the Serial Monitor (`gate open`).

## The parts

![Components](docs/images/components-layout.jpg)

ESP32 · breadboard + rainbow jumper ribbon · LEDs + resistors · DC gear motor + L298N driver (gate) ·
small DC fan + NPN transistor + 1N4007 · 4-wire 12 V RGB strip + 3 MOSFETs · DFPlayer Mini + speaker + micro-SD ·
2 × HC-SR04 ultrasonic + 1 kΩ/2 kΩ dividers · PIR HC-SR501 · 3 push buttons · buck converter · 12 V 2 A adapter.
