# Running Smart Estate online

There are three "online" options. Use whichever fits your situation.

| Option | Needs hardware? | What you get |
|---|---|---|
| **A. Web app demo mode** | No | The dashboard opens with a built-in simulated estate: the gate turns, the fan spins, the strip glows. |
| **B. Wokwi simulator** | No | The **real ESP32 firmware** runs online in Wokwi with a virtual stepper gate, LEDs, RGB LED, buttons, 2 ultrasonic sensors and a PIR. |
| **C. Real model + online web app** | Yes | Control the real estate from any phone, anywhere, through MQTT. |

---

## A. Publish the web app (GitHub Pages)

1. On GitHub: **Settings → Pages → Build and deployment → Source: GitHub Actions**.
2. Merge this work into `main` (or run the *Publish web app to GitHub Pages* workflow by hand from the **Actions** tab).
3. Your app is at `https://<your-github-username>.github.io/smarthome/`.
4. It opens in **Demo** mode. Use the gear icon to switch to **Online** mode.

You can also just open `webapp/index.html` on your PC by double-clicking it.

## B. Run the firmware in the Wokwi online simulator

1. Go to <https://wokwi.com> → **New project → ESP32 (Arduino)**.
2. Replace `sketch.ino` with the contents of `firmware/smarthome/smarthome.ino`.
3. Replace `diagram.json` with `wokwi/diagram.json`.
4. Click the **▾** next to the file tabs → **New file…** and create these files, pasting in their contents:
   * `secrets.h` ← `wokwi/secrets.h` (it uses `Wokwi-GUEST` Wi-Fi and turns on `WOKWI_SIM`)
   * `libraries.txt` ← `wokwi/libraries.txt`
5. Press ▶ **Start**. In the simulator:
   * press the **Lights**, **Gate** and **Fan** buttons (hold one for 1 s for its second job)
   * watch the stepper turn 1080° when the gate opens, and the RGB LED show the strip colour
   * the blue **Fan** LED gets brighter with each fan speed
   * click an **HC-SR04** and drag its distance below 12 cm: a "car" at the gate opens it; below 15 cm in the garage = parked
   * click the **PIR** to make motion (wait 30 s after start). Type `security on` first and the RGB LED flashes red: that's the alarm
   * type `gate open`, `living 50`, `fan 3`, `strip red`, `scene party` or `state` in the Serial Monitor
6. **Control the simulated ESP32 from the web app:** change `HOME_ID` in `secrets.h` to something unique, then in the web app open ⚙ → **Online** and type the same Home ID. The pill turns green ("Online") and every switch moves the virtual motor and LEDs.

> Alexa is turned off in the simulator because Alexa can't discover a virtual device. Wokwi has no DFPlayer, so the speaker stays quiet there.

## C. Control the real model from anywhere (MQTT)

How it works:

```
 Phone / laptop (GitHub Pages app) ──wss──►  MQTT broker  ◄──tcp── ESP32 in the model
            publishes  smartestate/<HOME_ID>/cmd     {"device":"gate","value":"open"}
            receives   smartestate/<HOME_ID>/state   {"gate":true,"gate_state":"opening","fan_speed":2,...}
                       smartestate/<HOME_ID>/online  1 / 0
```

1. In PART 1 of `firmware/smarthome/smarthome.ino` (or in a `secrets.h` next to it) set `HOME_ID` to a long, unique name, e.g. `estate-angel-7fq2k9`.
2. Upload the firmware. The Serial Monitor prints `[mqtt] connected`.
3. In the web app: ⚙ → **Online**, broker `wss://broker.hivemq.com:8884/mqtt`, the same Home ID → **Connect**.

**Privacy:** `broker.hivemq.com` is a free *public* test broker. Anyone who guesses your Home ID could press your buttons. That's fine for a school model. For real privacy:

1. Create a free cluster at <https://console.hivemq.cloud> and add a username and password.
2. In `secrets.h`: `MQTT_HOST` = your cluster host, `MQTT_PORT 8883`, plus `MQTT_USER` and `MQTT_PASS`. Port 8883 switches the ESP32 to TLS automatically.
3. In the web app: broker `wss://<your-cluster-host>:8884/mqtt` plus the same username and password.

## Local only (no internet at the exhibition)

* The ESP32 serves the full dashboard itself at **http://smartestate.local** or at the IP printed in the Serial Monitor.
* If it can't join Wi-Fi within 20 s, it makes its own hotspot **SmartEstate** (password `smartestate`). Connect your phone to it and open **http://192.168.4.1**.
