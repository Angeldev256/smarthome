# Installation guide

From zero to a working Smart Estate, step by step. Takes about an hour the first time.

| Step | What | Time |
|---|---|---|
| 1 | Get the project onto your PC | 2 min |
| 2 | Install the software (installer **or** by hand) | 15 min |
| 3 | Settings in `smarthome.ino` | 5 min |
| 4 | Wire the model and test each part | 30+ min |
| 5 | Upload and find the app | 5 min |
| 6 | Speaker SD card, Alexa, online app | 10 min |

---

## 1. Get the project onto your PC

1. Open **https://github.com/Angeldev256/smarthome**.
2. Click the **branch button** (top left) and choose **`claude/wonderful-galileo-4p3xz7`**. This has the newest code.
3. Click **Code → Download ZIP**.
4. Unzip it, for example to `Documents\smarthome-project`. (Right-click the ZIP → *Extract All*.)

> With git instead: `git clone -b claude/wonderful-galileo-4p3xz7 https://github.com/Angeldev256/smarthome.git`

What's inside:

| Folder / file | What it is |
|---|---|
| `INSTALL-WINDOWS.bat` | one-click installer for Windows |
| `firmware/smarthome/smarthome.ino` | **the code** for the ESP32 (one file) |
| `sdcard/mp3/` | voice clips for the speaker |
| `webapp/index.html` | the app (double-click = demo) |
| `docs/` | this guide, blueprint, alternatives, Alexa, run the app |

## 2. Install the software

### Option A – one-click installer (Windows)

1. In the unzipped folder, double-click **`INSTALL-WINDOWS.bat`**.
   (If Windows says *"Windows protected your PC"*: **More info → Run anyway**. It's the script from this project.)
2. It asks before each step:
   1. copies the sketch to `Documents\Arduino\smarthome` (your old `smarthome.ino` is backed up first)
   2. asks your **Wi-Fi name, password** and makes a **Home ID**, saved in `secrets.h` next to the sketch
   3. downloads and installs the **ESP32 boards** and the **libraries** (about 500 MB)
   4. compiles and **uploads** to the ESP32 on the COM port you type, and opens the Serial Monitor
   5. copies the **speaker clips** to the SD card
   6. opens the **app** in demo mode
3. Write down the **Home ID** it shows. You type it in the app later.

The boards and libraries it installs are shared with the Arduino IDE, so you can use the IDE afterwards too.

### Option B – by hand with the Arduino IDE (Windows, Mac, Linux)

1. Install **Arduino IDE 2** from https://www.arduino.cc/en/software.
2. **USB driver:** plug in the ESP32. If no new COM port appears in *Tools → Port*, install the driver for the
   chip next to the USB socket: **CP2102** (Silicon Labs "CP210x VCP driver") or **CH340** (WCH "CH341SER").
3. **ESP32 boards:** *File → Preferences → Additional boards manager URLs*, paste
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json` → OK.
   Then *Tools → Board → Boards Manager*, search **esp32**, install **esp32 by Espressif Systems**.
4. **Libraries:** *Sketch → Include Library → Manage Libraries*, install:
   * **ArduinoJson** by Benoit Blanchon
   * **Espalexa** by Christian Schwinne
   * **PubSubClient** by Nick O'Leary
   * **Adafruit NeoPixel** (only needed for a 3-wire WS2812 strip, harmless to install)
5. **The sketch:** open `firmware/smarthome/smarthome.ino` with *File → Open*.
   Or in your own `smarthome` sketch: select all, delete, and paste in the whole of `smarthome.ino`.

## 3. Settings (PART 1 at the top of `smarthome.ino`)

Only change PART 1. The important lines:

```cpp
#define WIFI_SSID      "YOUR_WIFI_NAME"       // 2.4 GHz Wi-Fi (not 5 GHz)
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
#define HOME_ID        "estate-change-me-1234" // make it unique, type the same in the app

#define GATE_MOTOR     GATE_DC                 // your DC motor + L298N
#define GATE_DC_RUN_MS 4000                    // ms the motor needs to open the gate
#define RGB_STRIP_TYPE RGB_ANALOG              // your 4-wire 12 V strip
#define SPEAKER_TYPE   SPEAKER_DFPLAYER        // or SPEAKER_TONE for a plain speaker
#define USE_PIR        1                       // 0 until the motion sensor is plugged in!
```

If the installer made a `secrets.h`, your Wi-Fi and Home ID are in there and you can leave these lines as they are.

Using other parts (servo, stepper, big speaker, relay bulb …)? See [ALTERNATIVES.md](ALTERNATIVES.md).

## 4. Wire the model (test each part as you go)

The detailed version, one device at a time with what you should see: **[ACTIVATION_GUIDE.md](ACTIVATION_GUIDE.md)**.

Follow **[BLUEPRINT.md](BLUEPRINT.md)** for the drawings and pin table. Short version:

1. **Power first:** 12 V adapter → buck converter. Turn its screw until the meter shows **5.0 V** *before* connecting anything.
2. ESP32 on the breadboard: **VIN ← 5 V rail, GND ← GND rail**. Upload (step 5) and open the Serial Monitor.
3. Add one part at a time and test it by typing in the Serial Monitor (115200 baud, *Newline* on):

| Part | Pins | Test command |
|---|---|---|
| LEDs | 16, 17, 18, 19 (each through 220 Ω) | `living on`, `bedroom 30`, `all_lights off` |
| Buttons | 5, 15, 12 → button → GND | press them |
| Gate: L298N | IN1 ← 13, IN2 ← 14, +12V, GND, OUT1/OUT2 → motor | `gate open`, `gate stop`, `gate close` |
| Fan | 23 → 470 Ω → NPN base | `fan 1`, `fan 3`, `fan breeze` |
| RGB strip | 25 / 33 / 32 → MOSFETs, strip + → 12 V | `strip red`, `strip rainbow` |
| Speaker | 4 → 1 kΩ → DFPlayer RX | `say hello` |
| Ultrasonic gate | TRIG 21, ECHO → 1 kΩ → 34 (+2 kΩ to GND) | `state` → look at `gate_cm` |
| Ultrasonic garage | TRIG 22, ECHO → 1 kΩ → 35 (+2 kΩ to GND) | `state` → `garage_cm` |
| PIR | OUT → 36 (VP) | wave after 30 s → outdoor lights on |

4. Measure the gate: `gate open`, count the seconds until it's fully open, put that × 1000 into `GATE_DC_RUN_MS`, upload again.

## 5. Upload and find the app

1. *Tools → Board → esp32 → **ESP32 Dev Module***. *Tools → Port →* the ESP32's COM port.
2. Click **Upload** (→). If it stops at `Connecting....`, hold the **BOOT** button on the ESP32 until it starts writing.
3. *Tools → Serial Monitor*, **115200 baud**. You'll see:
   ```
   === SMART ESTATE starting ===
   Connected! Dashboard: http://192.168.1.50
   [mqtt] connected to broker.hivemq.com
   ```
4. Open that address (or **http://smartestate.local**) on a phone on the same Wi-Fi. That's your app.
   The strip glows green and the speaker says "Smart estate is online".

## 6. Finish

* **Speaker:** copy the `sdcard/mp3` folder to a FAT32 micro-SD card, put it in the DFPlayer. See [sdcard/README.md](../sdcard/README.md).
* **Alexa:** same Wi-Fi, then say *"Alexa, discover devices"*. See [ALEXA_AND_VOICE.md](ALEXA_AND_VOICE.md).
* **App online from anywhere, with voice:** see [RUN_THE_APP.md](RUN_THE_APP.md).

## Troubleshooting

| Problem | Fix |
|---|---|
| No COM port | Install the CP2102 or CH340 USB driver. Try another cable: some are charge-only. |
| `Connecting....` then fails | Hold **BOOT** while uploading. Close the Serial Monitor first. |
| `ArduinoJson.h: No such file` (or Espalexa, PubSubClient) | Install that library (step 2B-4). |
| Upload works, nothing happens | Serial Monitor at 115200. Press the **EN/RST** button to restart. |
| `Wi-Fi failed - hotspot 'SmartEstate'` | Wrong name/password, or 5 GHz Wi-Fi. The strip blinks red. Use the hotspot or fix secrets.h. |
| ESP32 keeps restarting when the motor/strip runs | Power problem: everything must run from the buck converter/adapter, all GNDs joined. |
| Gate goes the wrong way | Swap OUT1/OUT2 on the L298N, or `GATE_REVERSE 1`. |
| Motion triggers by itself | PIR needs 30 s to settle; turn its sensitivity down. Not plugged in? `USE_PIR 0`. |
| `gate_cm` always -1 | Check TRIG/ECHO aren't swapped, 5 V on VCC, and the divider on ECHO. |
| No sound | Speaker on SPK1/SPK2, SD card FAT32 with the `mp3` folder, 1 kΩ to RX. Type `volume 25`. |
| Alexa finds nothing | Same 2.4 GHz network, router "AP isolation" off. Say "discover devices" again. |
| `smartestate.local` doesn't open | Use the IP number from the Serial Monitor. |
