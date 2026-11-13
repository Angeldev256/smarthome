# Alexa, voice, buttons and the speaker

You can control the estate five ways. They all do the same thing, so mix them freely.

| Way | How |
|---|---|
| **Buttons** on the model | Lights · Gate · Fan buttons on the garage front, BOOT button = strip |
| **Alexa** | "Alexa, turn on Gate" |
| **Voice in the web app** | Tap the green mic: "fan speed two" |
| **Web app** | Switches, sliders and colour buttons |
| **Serial Monitor** | Type `gate open` and press Enter |

Every change is announced by the **speaker** on the model ("Gate opening") and appears in the app.

## 1. Alexa (no skill, no account linking)

The firmware uses **Espalexa**, which makes the ESP32 look like Philips Hue lights to Alexa.
Your Echo finds it on your Wi-Fi by itself.

1. The Echo / Alexa app and the ESP32 must be on the **same Wi-Fi** (2.4 GHz).
2. Power the estate and wait for the RGB strip to stop blinking blue.
3. Say **"Alexa, discover devices"** (or Alexa app → Devices → + → Add device → Other → Discover).
4. Nine devices appear:

| Alexa device | What you can say |
|---|---|
| Living Room Light | "turn on / off", "set Living Room Light to 30%" |
| Bedroom Light | same |
| Garage Light | same |
| Outdoor Light | same (the gate-pillar lights) |
| Fan | "turn on Fan", "set Fan to 100%" (1–33 % = speed 1, 34–66 % = speed 2, 67–100 % = speed 3) |
| Gate | "turn on Gate" = **open**, "turn off Gate" = **close** |
| Strip Light | "set Strip Light to purple", "set Strip Light to 50%", "turn off Strip Light" |
| Party Mode | "turn on Party Mode" (all lights, party strip, fan), "turn off Party Mode" |
| Good Night | "turn on Good Night" (lights off, gate closed, fan low, dim blue strip) |

### Nicer phrases with Alexa Routines
Alexa app → More → Routines → +:
* *When you say* "open the gate" → *Smart Home* → Gate → On
* *When you say* "I'm home" → Gate → On, Living Room Light → On
* *When you say* "it's hot" → Fan → 100 %

### Alexa troubleshooting
* Not found? Your router must not isolate clients ("AP isolation" off), and it must be the same network. A phone hotspot works well for demos.
* Newer Echo devices (4th gen and later) work with Espalexa 2.7 or newer. Keep the library up to date.
* Alexa doesn't work in the Wokwi simulator or when the ESP32 is in its own hotspot mode.

## 2. Voice in the web app (any phone or laptop)

Tap the green **microphone** on the dashboard and talk. It works in Chrome, Edge and Safari.
Browsers only allow the microphone on secure pages, so use the **online (GitHub Pages) version** for voice.
On the ESP32's own page (`http://…`) type the same words in the box under the mic.

| Say | What happens |
|---|---|
| "turn on the living room light" / "bedroom light off" | one room |
| "dim the bedroom to 30 percent" | brightness |
| "all lights on" / "turn off all the lights" | every light |
| "open the gate" / "close the gate" / "stop the gate" | gate motor |
| "turn on the fan" / "fan speed 3" / "fan high" / "fan faster" | fan speed |
| "fan breeze" | natural-wind mode |
| "turn off the fan in 10 minutes" | off-timer |
| "strip red" / "make the strip purple" / "rainbow" / "breathe" | RGB strip |
| "strip brightness 40 percent" / "turn off the strip" | RGB strip |
| "volume up" / "volume 15" / "mute the speaker" | speaker |
| "ring the doorbell" | ding-dong + white flash |
| "party mode" / "party off" | party scene |
| "good night" | lights off, gate closed, fan low, dim blue strip |
| "I'm home" | gate opens, living room + outdoor lights on |
| "status" | the app tells you what is on |

The app answers out loud using the browser's text-to-speech, and the model's own speaker announces the change too.

## 3. Commands (Serial Monitor, MQTT, `/api/cmd`)

Format: `device value`, e.g. `gate open`, `living 50`, `strip #ff8800`.

| Device | Values |
|---|---|
| `living`, `bedroom`, `garage_light`, `outdoor` | `on`, `off`, `toggle`, `0`–`100` (brightness) |
| `all_lights` | `on`, `off`, `toggle` |
| `gate` | `open`, `close`, `toggle`, `stop` |
| `fan` | `off`, `on`, `0`–`3`, `low`, `medium`, `high`, `next`, `faster`, `slower`, `breeze` |
| `fan_timer` | minutes until off (`0` = cancel) |
| `strip` | `on`, `off`, `toggle`, `next`, an effect, a colour, or `0`–`100` |
| `strip_color` | `red`, `green`, `blue`, `white`, `warm`, `yellow`, `orange`, `purple`, `pink`, `cyan`, … or `#rrggbb` |
| `strip_effect` | `status` (steady), `breathe`, `rainbow`, `party` |
| `strip_brightness` | `0`–`100` |
| `volume` | `0`–`30`, `up`, `down`, `mute`, `unmute` |
| `announce` | `on`, `off` (speak every change) |
| `say` | a clip name, e.g. `hello`, `welcome` (see `sdcard/README.md`) |
| `doorbell` | anything |
| `scene` | `welcome`, `goodnight`, `party`, `normal` |

Type `state` in the Serial Monitor to see everything as JSON.
