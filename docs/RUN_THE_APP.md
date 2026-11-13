# Run the app live

There are three ways to open the Smart Estate app. Pick the one that fits where you are.

| Way | Needs internet? | Controls the real model? | Voice (microphone)? | Best for |
|---|---|---|---|---|
| **1. From the ESP32** | No | Yes | Type commands (no mic on http) | The exhibition / at home on the same Wi-Fi |
| **2. Online (GitHub Pages)** | Yes | Yes, from anywhere (MQTT) | Yes | Phone anywhere, voice control, showing people |
| **3. Demo on a laptop** | No | No (simulated) | Yes on Pages, typed on a file | Presenting before the model is wired |

---

## 1. From the ESP32 (the model serves the app itself)

1. Upload `smarthome.ino` with your Wi-Fi name and password in PART 1.
2. Open the **Serial Monitor** (115200). It prints `Connected! Dashboard: http://192.168.x.x`.
3. On a phone or laptop **on the same Wi-Fi**, open that address, or **http://smartestate.local**.
   (`.local` works on iPhone, Mac and Windows 10/11; on some Android phones use the number.)
4. The app opens in **Local** mode and shows the real estate. Everything you press happens on the model.

**No Wi-Fi at the venue?** If the ESP32 can't join Wi-Fi in 20 s, it makes its own hotspot:
connect your phone to **SmartEstate** (password `smartestate`) and open **http://192.168.4.1**.
The strip blinks slow red in this mode. Alexa and online control don't work there, everything else does.

**Tip:** on the phone, use *Add to Home Screen* (Safari share menu / Chrome ⋮ menu) so it opens like an app.

## 2. Online with GitHub Pages (control it from anywhere)

### a) Put the app online (once)
1. On GitHub open **Angeldev256/smarthome → Settings → General → Default branch** and switch it to
   `claude/wonderful-galileo-4p3xz7` (the newest code). GitHub only lets Pages publish from the default branch.
2. **Settings → Pages → Build and deployment → Source: GitHub Actions.**
3. **Actions** tab → **Publish web app to GitHub Pages** → **Run workflow**.
4. After about a minute the app is live at **https://angeldev256.github.io/smarthome/**.
   From now on every push that changes `webapp/` publishes it again by itself.

### b) Connect it to your model
1. In PART 1 of `smarthome.ino` set `HOME_ID` to something unique, e.g. `estate-angel-7fq2k9`, and upload.
   The Serial Monitor prints `[mqtt] connected to broker.hivemq.com`.
2. Open the GitHub Pages app → ⚙ (top right) → **Mode: Online**.
3. Broker: `wss://broker.hivemq.com:8884/mqtt` (already filled in). **Home ID:** the same one as in the sketch → **Connect**.
4. The pill at the top turns green (**Online**). Now the switches, the mic and the scenes work on the real model,
   from any phone with internet, even on mobile data.

The phone remembers these settings. Use the green **microphone** for voice: "open the gate", "arm security".

> **Privacy:** `broker.hivemq.com` is public, so anyone who guesses your Home ID could press your buttons.
> Fine for a school model. For real privacy use a free HiveMQ Cloud account (see [ONLINE_AND_SIMULATOR.md](ONLINE_AND_SIMULATOR.md#c-control-the-real-model-from-anywhere-mqtt)).

## 3. Demo (no model needed)

* Open the GitHub Pages link, or double-click `webapp/index.html` on your laptop.
* It starts in **Demo** mode: a simulated estate that follows the same rules as the firmware.
* Use the **Simulator** card: 🚶 Motion, 🚗 Car at gate, 🅿 Park in garage. Try "arm security" and then Motion.

## Something not working?

| Problem | Fix |
|---|---|
| Pill says **ESP32 offline** (Local mode) | Phone and ESP32 must be on the same Wi-Fi. Use the IP number instead of `smartestate.local`. |
| Pill says **Broker ok · waiting for ESP32** (Online) | Home ID in the app ≠ `HOME_ID` in the sketch, or the ESP32 isn't on the internet. |
| Microphone does nothing | Voice needs https: use the GitHub Pages link, in Chrome, Edge or Safari. On the ESP32's own page, type instead. |
| GitHub Pages workflow fails with "branch not allowed" | Step 2a-1: make `claude/wonderful-galileo-4p3xz7` the default branch, then run it again. |
| The app shows the old version | Pull down to refresh, or clear the page from the browser's history. |
