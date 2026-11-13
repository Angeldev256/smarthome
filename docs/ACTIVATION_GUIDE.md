# Activation guide – switch on the web app and every device, one by one

Do the steps **in this order**. Each step adds one part, tests it, and only then moves on.
If a step fails, fix it before the next one: it saves a lot of searching later.

You need: the PC with the Arduino IDE (see [INSTALL.md](INSTALL.md)), the ESP32 on USB, a phone on the same Wi-Fi.

**How to test:** every device can be tested 5 ways. Use the Serial Monitor first (simplest), then the others.

| Way | Where |
|---|---|
| Serial Monitor | Arduino IDE → *Tools → Serial Monitor*, **115200 baud**, line ending **Newline**. Type a command, press Enter. |
| Web app | the address from step 1, on your phone |
| Voice | the green mic in the web app (online version) or type in the box under it |
| Button | B1 Lights, B2 Gate, B3 Fan on the garage front, BOOT on the ESP32 |
| Alexa | after step 12 |

---

## Step 0 – Power (nothing plugged into the ESP32 yet)

1. Connect the **12 V adapter** to the **buck converter IN**.
2. Put a multimeter on **buck OUT** and turn the little brass screw until it reads **5.0 V**.
3. Connect buck OUT to the breadboard **5 V rail** and **GND rail**. Join the adapter's GND to the same GND rail.

✅ Meter shows 5.0 V. ❌ 12 V on OUT → wrong side, swap IN/OUT.

---

## Step 1 – ESP32 + web app

1. ESP32 on the breadboard: **VIN → 5 V rail**, **GND → GND rail**. Also plug in USB.
2. In PART 1 of `smarthome.ino` set your **Wi-Fi name, password and HOME_ID** (or let the installer make `secrets.h`).
   Not wired yet? Set `USE_PIR 0` for now.
3. **Upload** (*Tools → Board → ESP32 Dev Module*, your COM port, →). Hold **BOOT** if it sticks at `Connecting....`.
4. Open the **Serial Monitor**. Press the ESP32's **EN/RST** button. You should see:
   ```
   === SMART ESTATE starting ===
   Gate: DC motor, 4000 ms to open
   Connecting to MyWiFi.....
   Connected! Dashboard: http://192.168.1.50
   [estate] Smart Estate ready
   [mqtt] connected to broker.hivemq.com
   ```
5. **Activate the web app:** on your phone (same Wi-Fi) open that address, e.g. `http://192.168.1.50`,
   or **http://smartestate.local**.
6. The top-right pill turns **green "Local"**. The map, switches and the Activity list fill in.
7. Make it an app icon: Safari → Share → *Add to Home Screen* (iPhone), Chrome ⋮ → *Add to Home screen* (Android).

✅ Green "Local" pill, Activity shows "Smart Estate ready". Type `state` in the Serial Monitor → a long `{...}` line.
❌ `Wi-Fi failed – hotspot 'SmartEstate'` → wrong password or 5 GHz Wi-Fi. Either fix it, or connect your phone to
Wi-Fi **SmartEstate** (password `smartestate`) and open **http://192.168.4.1**.
❌ Pill says "ESP32 offline" → phone not on the same Wi-Fi, or use the number instead of `smartestate.local`.

---

## Step 2 – Building lights (LEDs)

Wire: **GPIO 16 / 17 / 18 / 19 → 220 Ω → LED long leg**, short leg → GND. (White/blue LEDs: 100 Ω.)

| Test | Do | Expect |
|---|---|---|
| Serial | `living on` · `bedroom 30` · `garage_light on` · `outdoor on` · `all_lights off` | LEDs on / dim / off |
| App | *Building lights* switches and sliders | same, and the rooms glow on the map |
| Button | B1 (step 3) | all on / off |

✅ Each LED follows its switch, sliders dim it. ❌ LED stays off → flip it (long leg to the resistor). Very dim → 100 Ω.

---

## Step 3 – Manual buttons

Wire: a push button from **GPIO 5** (Lights), **GPIO 15** (Gate), **GPIO 12** (Fan) to **GND**. Nothing else.

| Button | Short press | Hold 1 s |
|---|---|---|
| B1 Lights (GPIO 5) | all lights on/off | all off |
| B2 Gate (GPIO 15) | open / close | stop |
| B3 Fan (GPIO 12) | next speed | off |
| BOOT on the ESP32 | next strip effect | strip on/off |

✅ Serial shows `[cmd:button] all_lights = toggle` when you press B1. ❌ Nothing → the button's two legs on the same side are
connected inside: turn it 90° on the breadboard.

---

## Step 4 – Gate: DC motor + L298N

Wire: **IN1 ← GPIO 13, IN2 ← GPIO 14**, ENA jumper ON, **+12V ← 12 V adapter**, **GND ← GND rail**,
**OUT1 + OUT2 → motor**. Leave the L298N's +5V empty.

| Test | Do | Expect |
|---|---|---|
| Serial | `gate open` → wait → `gate close` · `gate stop` while moving | motor runs one way, then back, stops on `stop` |
| App | *Gate* card → Open / Stop / Close | progress bar fills, leaves swing on the map, strip pulses orange |
| Voice | "open the gate" | same + the speaker says "Gate opening" (after step 7) |
| Button | B2 | open/close; hold = stop |

**Set the time:** `gate open`, count the seconds until it's fully open, put that × 1000 in `GATE_DC_RUN_MS`, upload again.
✅ Opens and closes fully. ❌ Wrong direction → swap OUT1/OUT2 (or `GATE_REVERSE 1`).
❌ ESP32 restarts when the motor starts → GND of the L298N not joined to the ESP32 GND, or adapter too weak.

---

## Step 5 – Fan

Wire: **GPIO 23 → 470 Ω → NPN base** (2N2222/S8050), emitter → GND, fan between **5 V** and collector,
**1N4007 across the fan** (stripe to 5 V).

| Test | Do | Expect |
|---|---|---|
| Serial | `fan 1` · `fan 2` · `fan 3` · `fan breeze` · `fan off` · `fan_timer 1` | 3 speeds, slow wave, off after 1 min |
| App | *Fan* card: Off/1/2/3, Breeze, Off-timer | fan icon spins faster on the map |
| Button | B3 | next speed |

✅ Three clearly different speeds. ❌ Speed 1 doesn't start → raise `FAN_DUTY_LOW` to 55–60.

---

## Step 6 – RGB strip (4 wires, 12 V)

Wire: 3 × IRLZ44N: **gate ← 100 Ω ← GPIO 25 (R) / 33 (G) / 32 (B)**, 10 kΩ gate → GND, **drain → strip R/G/B**,
source → GND. **Strip +12V → 12 V adapter.**

| Test | Do | Expect |
|---|---|---|
| Power on | press EN/RST | blue blinks while connecting, then **steady green** = estate is ON |
| Serial | `strip red` · `strip blue` · `strip rainbow` · `strip party` · `strip_brightness 30` · `strip off` | colours and effects |
| App | *RGB strip* colour dots, effects, brightness | the wall line on the map changes colour |
| Button | BOOT | next effect |

✅ Red, green and blue each work alone. ❌ One colour missing → that MOSFET's wire. ❌ Always on → MOSFET is not
logic-level (use IRLZ44N, not IRFZ44N), or the 10 kΩ gate resistor is missing.

---

## Step 7 – Speaker (DFPlayer Mini)

1. Copy the `sdcard/mp3` folder to a FAT32 micro-SD card, put it in the DFPlayer.
2. Wire: **GPIO 4 → 1 kΩ → DFPlayer RX**, VCC → 5 V, GND → GND, speaker on **SPK1 + SPK2**.

| Test | Do | Expect |
|---|---|---|
| Power on | EN/RST | "Smart estate is online." |
| Serial | `say hello` · `volume 25` · `doorbell ring` | hello message, louder, ding-dong |
| App | *Speaker* card: volume, Say hello, Beep | |
| Any change | `living on` | "Living room light on." |

❌ Silent → speaker on GND by mistake, card not FAT32, no `mp3` folder, or RX/TX swapped (it's **RX** on the DFPlayer).
Too chatty? App → Announcements off (or `announce off`).

---

## Step 8 – Ultrasonic sensor at the gate (S1)

Wire: VCC → 5 V, GND → GND, **TRIG → GPIO 21**, **ECHO → 1 kΩ → GPIO 34**, plus **2 kΩ from GPIO 34 to GND**.
Mount it on the gate pillar looking out at the road.

| Test | Do | Expect |
|---|---|---|
| Serial | `state` and look at `"gate_cm"` | your hand at 10 cm → about 10 |
| Car | put a toy car < 12 cm in front | "Car at the gate. Opening.", gate opens; 5 s after you take the car away it closes |
| App | *Sensors & security* → Car at the gate | shows the distance, a red car appears on the map |

❌ Always `-1` → TRIG/ECHO swapped, no 5 V, or the divider wrong. ❌ Opens by itself → something closer than 12 cm:
lower `CAR_AT_GATE_CM`.

---

## Step 9 – Ultrasonic sensor in the garage (S2)

Wire like S1 but **TRIG → GPIO 22**, **ECHO → 1 kΩ → GPIO 35** (+2 kΩ to GND). Mount it on the garage back wall.

| Test | Do | Expect |
|---|---|---|
| Car | push a toy car in slowly | < 15 cm: "Car parked", garage light on. < 5 cm: "Stop! Stop!" + red flash |
| Car out | pull it out | garage light off |
| App | Garage parking tile | distance and a bar that fills as the car gets closer |

---

## Step 10 – Motion sensor (PIR)

1. Wire: VCC → 5 V, GND → GND, **OUT → GPIO 36 (VP)**. Set `USE_PIR 1` in PART 1 and upload again.
2. On the PIR: **time** knob fully left (shortest), **sensitivity** about half. Wait **30 s** after power-on.

| Test | Do | Expect |
|---|---|---|
| Motion | wave at it | "Motion detected", outdoor lights on for 20 s, ripple on the map |
| Security | `security on` (or the app switch, or "arm security"), then wave | **alarm**: siren, strip flashing red, red alert + *Silence alarm* button in the app |
| Stop it | `alarm off` / *Silence alarm* / "stop the alarm" / `security off` | quiet |

❌ Triggers by itself → sensitivity down, keep it away from the fan and the strip's warmth.

---

## Step 11 – Scenes (everything together)

| Command | What happens |
|---|---|
| `scene welcome` / "I'm home" | security off, gate opens, living room + outdoor lights on, strip green |
| `scene goodnight` / "good night" | all lights off, gate closes, fan low, strip dim blue, security armed |
| `scene party` / "party mode" | all lights on, fan 2, strip party colours |
| `scene normal` | strip back to steady green |
| `doorbell ring` | ding-dong + white flash |

---

## Step 12 – Alexa

1. Echo and ESP32 on the **same 2.4 GHz Wi-Fi**.
2. Say **"Alexa, discover devices"**. 10 devices appear: 4 lights, Fan, Gate, Strip Light, Party Mode, Good Night, Security.
3. Test: "Alexa, turn on Living Room Light", "Alexa, turn on Gate", "Alexa, set Fan to 100%",
   "Alexa, set Strip Light to purple", "Alexa, turn on Security".

❌ Nothing found → router "AP isolation" off, same network, say it again. A phone hotspot works well for demos.
More: [ALEXA_AND_VOICE.md](ALEXA_AND_VOICE.md).

---

## Step 13 – Web app online (from anywhere, with voice)

1. GitHub → **Settings → General → Default branch** → `claude/wonderful-galileo-4p3xz7`.
2. **Settings → Pages → Source: GitHub Actions**. **Actions → Publish web app to GitHub Pages → Run workflow**.
3. Open **https://angeldev256.github.io/smarthome/** → ⚙ → **Online** → Home ID = your `HOME_ID` → **Connect**.
4. Pill turns green **"Online"**. Tap the **mic** and say "open the gate".

More: [RUN_THE_APP.md](RUN_THE_APP.md).

---

## Final checklist

- [ ] Strip glows green after power-on, speaker says "Smart estate is online"
- [ ] App opens at `http://smartestate.local` (green "Local")
- [ ] 4 light zones switch and dim
- [ ] Gate opens, stops, closes (time set in `GATE_DC_RUN_MS`)
- [ ] Fan: 3 speeds + breeze
- [ ] Strip: colours + effects
- [ ] Speaker talks
- [ ] Car at the gate opens it, garage parking says "Stop!"
- [ ] Motion turns on the outdoor lights; armed → alarm
- [ ] 3 buttons + BOOT work
- [ ] Alexa finds 10 devices
- [ ] Online app works from mobile data
