# Alexa & voice commands

## 1. Alexa (no skill, no account linking)

The firmware uses **Espalexa**, which makes the ESP32 look like Philips Hue lights to Alexa.
Your Echo finds it on your Wi-Fi by itself.

1. Make sure the Echo / Alexa phone app and the ESP32 are on the **same Wi-Fi network** (2.4 GHz).
2. Power the estate and wait until the LCD shows the IP address.
3. Say **"Alexa, discover devices"** (or Alexa app → Devices → + → Add device → Other → Discover).
4. Nine devices appear:

| Alexa device | "turn on" does | "turn off" does |
|---|---|---|
| Living Room Light | light on | light off |
| Bedroom Light | light on | light off |
| Garage Light | light on | light off |
| Outdoor Light | light on | light off |
| Fan | fan on | fan off |
| Gate | **opens** the gate | closes the gate |
| Garage Door | **opens** the garage | closes the garage |
| Rain Cover | covers the clothesline | rolls the cover back |
| Security | arms security | disarms (and silences the alarm) |

Examples:

* "Alexa, turn on Living Room Light"
* "Alexa, turn on Gate" – opens the gate
* "Alexa, turn off Garage Door" – closes the garage
* "Alexa, turn on Security"

### Nicer phrases with Alexa Routines
Alexa app → More → Routines → +:
* *When you say* "open the gate" → *Smart Home* → Gate → On
* *When you say* "good night" → All lights off, Gate off, Garage Door off, Security on
* *When you say* "I'm home" → Security off, Living Room Light on, Gate on

### Alexa troubleshooting
* Not found? Your router must not isolate clients ("AP isolation" off), and it must be the same network – a phone hotspot works well for demos.
* Newer Echo devices (4th gen and later) work with Espalexa v2.7+. Keep the library up to date.
* Alexa doesn't work in the Wokwi simulator or when the ESP32 is in its own hotspot mode.

## 2. Voice in the web app (any phone or laptop)

Tap the green **microphone** on the dashboard and talk. It works in Chrome, Edge and Safari.
Your browser only allows the microphone on secure pages, so use the **online (GitHub Pages) version** for voice.
On the ESP32's own page (`http://…`) type the same commands in the box under the mic.

| Say | What happens |
|---|---|
| "turn on the living room light" / "bedroom light off" | lights |
| "all lights on" / "turn off all the lights" | every light |
| "open the gate" / "close the gate" | gate servo |
| "open the garage" / "close the garage door" | garage servo |
| "turn on the garage light" | garage LED |
| "cover the clothes" / "open the cover" | rain cover servo |
| "turn on the fan" | fan |
| "arm security" / "disarm security" | security mode |
| "stop the alarm" | silence buzzer |
| "what's the temperature?" / "how humid is it?" | spoken answer from the DHT11 |
| "is it raining?" | spoken answer from the rain sensor |
| "status" | summary of gate, garage, lights, security |
| "good night" | lights off, gate + garage closed, security armed |
| "I'm home" | security off, living room light on, gate opens |

The app answers out loud using the browser's text-to-speech.
