# Smart Estate blueprint and wiring

This page shows where every part goes on the model and how to wire it with as few wires as possible.

## 1. Layout (top view)

![Layout blueprint](blueprint/blueprint-layout.png)

*(Vector version for printing: [blueprint-layout.svg](blueprint/blueprint-layout.svg))*

**The big idea: the garage is the control hub.** The ESP32, breadboard, buck converter, ULN2003 board,
DFPlayer and the transistors all sit inside the garage box in the middle. The buttons go on the garage front,
next to the driveway. Only five small cables leave the garage:

| Cable | Goes to | Wires (peel them off the rainbow ribbon) |
|---|---|---|
| **A** | Left house | 4 wires: living-room LED signal, fan +, fan −, GND |
| **B** | Right house | 2 wires: bedroom LED signal, GND |
| **C** | Gate | 2 wires (gate-pillar LED signal, GND) + the motor's own 5-wire plug |
| **D** | RGB strip | 4 wires: +12 V, R, G, B |
| **E** | 12 V adapter | 2 wires into the hub |

Run the cables **under the base board** and push each one up through a small hole into its room.

## 2. Wiring diagram

![Wiring diagram](blueprint/blueprint-wiring.png)

*(Vector version: [blueprint-wiring.svg](blueprint/blueprint-wiring.svg))*

How to read it: each part gets **one signal wire** from the ESP32. A coloured flag
(**12V**, **5V**, **GND**) means "connect to that rail on the breadboard", so you never run long power wires.

### Pin table

| Part | ESP32 pin | Other connections |
|---|---|---|
| Living-room LEDs (L1) | **GPIO 16** → 220 Ω → LED + | LED − → GND |
| Bedroom LEDs (L2) | **GPIO 17** → 220 Ω → LED + | LED − → GND |
| Garage LED (L3) | **GPIO 18** → 220 Ω → LED + | LED − → GND |
| Gate-pillar LEDs (L4) | **GPIO 19** → 220 Ω → LED + | LED − → GND |
| Gate stepper (ULN2003) | **GPIO 13 / 14 / 27 / 26** → IN1 / IN2 / IN3 / IN4 | board + → 5V, − → GND, motor plug into the board |
| Fan | **GPIO 23** → 470 Ω → NPN base | fan between 5V and collector, emitter → GND, 1N4007 across the fan |
| RGB strip red / green / blue | **GPIO 25 / 33 / 32** → 100 Ω → MOSFET gate | drains → strip R / G / B, sources → GND, strip + → 12 V |
| Speaker (DFPlayer Mini) | **GPIO 4** → 1 kΩ → DFPlayer **RX** | VCC → 5V, GND → GND, speaker on SPK1 + SPK2 |
| Button B1 – Lights | **GPIO 5** → button → GND | nothing else |
| Button B2 – Gate | **GPIO 15** → button → GND | nothing else |
| Button B3 – Fan | **GPIO 12** → button → GND | nothing else |
| BOOT button (on the ESP32) | GPIO 0 | already on the board: next strip effect |

You can change any pin in `firmware/smart_estate/config.h`.

### Power

```
12 V adapter ─┬─────────────────────────────► RGB strip +12 V
              └─► buck converter IN
                  buck converter OUT (set to 5.0 V with a meter BEFORE connecting!)
                        └─► 5 V rail ─► ESP32 VIN, ULN2003 +, DFPlayer VCC, fan
GND: adapter −, buck −, ESP32 GND and every part's GND all joined on one rail
```

* The 12 V adapter needs **2 A or more** (the strip uses most of it).
* Never power the motor or the speaker from the ESP32's 3V3 pin. Always use the 5 V rail.
* You can leave the 12 V supply connected while you upload over USB.

## 3. Each part in detail

### Building lights (LEDs)
* Up to **3 LEDs per pin**, side by side, **each with its own resistor**.
* Yellow, red or warm LEDs: 220 Ω. White or blue LEDs: 100 Ω (they need more voltage).
* LED long leg (+) goes to the resistor, short leg (−) to GND.
* They dim smoothly: "dim the bedroom to 30 percent", or the sliders in the app.

### Gate motor: turns 1080°
* Motor: **28BYJ-48** stepper with its **ULN2003** board. It turns an exact angle.
* Open = turn `GATE_TURN_DEGREES` (default **1080° = 3 full turns**). Close = turn the same amount back.
* Choose your own angle in `config.h`:
  ```cpp
  #define GATE_TURN_DEGREES  1080   // 360 = 1 turn, 720 = 2 turns, 1440 = 4 turns ...
  #define GATE_SPEED_RPM     12     // 5 = slow and strong, 15 = fast
  #define GATE_REVERSE       0      // 1 if it opens the wrong way
  #define GATE_AUTO_CLOSE_S  0      // e.g. 20 = close by itself after 20 s
  ```
* Fixing it to the gate: wind a thread on a small **spool** on the shaft (3 turns pull the gate open),
  or glue the gate leaf straight to the shaft and set the angle to 90.
* Pressing Stop (app, voice, or holding the Gate button) stops it exactly where it is.
* **DC gear motor instead?** Use an L298N or L9110S driver on GPIO 13 + 14 and set
  `#define GATE_MOTOR GATE_DC` and `GATE_DC_RUN_MS` (how many milliseconds it takes to open).

### Fan (no sensor needed)
* Small DC motor fan, switched by an **NPN transistor** (2N2222 or S8050, base through 470 Ω).
  For a bigger fan use a **TIP120** with 1 kΩ.
* Always put a **1N4007 diode** across the fan (stripe towards 5V). It protects the transistor.
* Speeds: **off, 1, 2, 3**, plus **breeze** (changes speed slowly, like wind) and an **off-timer**.
* If speed 1 doesn't start the fan, raise `FAN_DUTY_LOW` in `config.h`.

### RGB strip: shows the estate is ON
* Glows **green** as soon as the estate has power, so you can see the whole setup is on.
* It also shows what's happening: **blue blinks** while connecting to Wi-Fi, **slow red blink** = no Wi-Fi (hotspot mode),
  **orange pulse** while the gate moves, **white flash** for the doorbell or a button press.
* Effects: steady, breathe, rainbow, party. Any colour by app, voice ("strip purple") or Alexa ("set Strip Light to blue").
* **12 V strip with 4 wires (+12V, R, G, B):** 3 logic-level MOSFETs (IRLZ44N), each gate through 100 Ω, plus 10 kΩ gate → GND.
* **5 V WS2812 strip with 3 wires (5V, DIN, GND):** even simpler. DIN ← 330 Ω ← GPIO 25, then in `config.h` set
  `#define RGB_STRIP_TYPE RGB_WS2812` and `STRIP_LED_COUNT`, and install the **Adafruit NeoPixel** library.

### Speaker: the estate talks
* **DFPlayer Mini** MP3 module + a small speaker (3 W or less, 4–8 Ω).
* Only **one data wire**: ESP32 GPIO 4 → 1 kΩ → DFPlayer RX. Speaker on **SPK1 + SPK2** (never on GND).
* Copy the `sdcard/mp3` folder to a micro-SD card. See [sdcard/README.md](../sdcard/README.md).
* It announces every change in real time: "Gate opening", "Fan speed two", "Living room light on", "Welcome home!"
  Turn announcements off in the app or with `announce off`.

### Manual buttons
* A push button between the pin and GND. That's all. The ESP32's own pull-ups do the rest.
* **B1 Lights:** press = all lights on/off, hold 1 s = all off.
* **B2 Gate:** press = open / close, hold 1 s = stop.
* **B3 Fan:** press = next speed, hold 1 s = off.
* **BOOT** button on the ESP32: next strip effect, hold = strip on/off.
* Want a different job for a button? Edit the `BUTTONS` list in `config.h`, e.g. `{ 5, "living", "toggle", "off" }`.

## 4. Build order (test as you go)

1. Power only: adapter → buck converter, set it to **5.0 V** with a meter.
2. ESP32 on the breadboard, upload the code, open the Serial Monitor (115200).
3. Add the LEDs → type `living on`, `bedroom 30`.
4. Add the buttons → press them.
5. Add the ULN2003 + motor → type `gate open`. Change `GATE_TURN_DEGREES` if needed.
6. Add the fan → `fan 1`, `fan 3`, `fan breeze`.
7. Add the RGB strip → `strip red`, `strip rainbow`.
8. Add the DFPlayer + SD card → `say hello`.
9. Fit everything into the garage and run cables A–E under the base.

## 5. Not in this version

To keep the wiring simple, this version has **no sensors** (PIR, rain, IR, door, temperature),
**no servos** and **no buzzer** (the speaker makes the sounds). The fan and lights are controlled by you:
app, voice, Alexa or the buttons. If you add sensors later, the previous version of the code is in the
git history of the `claude/youthful-lamport-8r8fsm` branch.
