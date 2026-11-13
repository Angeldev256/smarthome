# Running Smart Estate online

There are three "online" options. Use whichever fits your situation.

| Option | Needs hardware? | What you get |
|---|---|---|
| **A. Web app demo mode** | No | The dashboard opens with a built-in simulator. Press Rain, Motion or Car to see the automation. |
| **B. Wokwi simulator** | No | The **real ESP32 firmware** runs online in Wokwi with virtual sensors, servos, LEDs and LCD. |
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
2. Replace `sketch.ino` with the contents of `firmware/smart_estate/smart_estate.ino`.
3. Replace `diagram.json` with `wokwi/diagram.json`.
4. Click the **▾** next to the file tabs → **New file…** and create these files, pasting in their contents:
   * `config.h` ← `firmware/smart_estate/config.h`
   * `webapp.h` ← `firmware/smart_estate/webapp.h`
   * `secrets.h` ← `wokwi/secrets.h` (it already uses `Wokwi-GUEST` Wi-Fi and turns on `WOKWI_SIM`)
   * `secrets.example.h` ← `firmware/smart_estate/secrets.example.h`
   * `libraries.txt` ← `wokwi/libraries.txt`
5. Press ▶ **Start**. In the simulator:
   * the slide switches are **Rain**, **IR gate**, **IR garage** and **Door**. Flip them to trigger the sensors
   * click the **PIR** to make motion, and click the **DHT22** to change the temperature (above 30 °C turns the fan LED on)
   * type `gate open`, `living on`, `security on` or `state` in the Serial Monitor
6. **Control the simulated ESP32 from the web app:** change `HOME_ID` in `secrets.h` to something unique, then in the web app open ⚙ → **Online** and type the same Home ID. The pill turns green ("Online") and every switch moves the virtual servos and LEDs.

> Wokwi only has a DHT22, so `WOKWI_SIM` switches the code to DHT22. Alexa is turned off in the simulator because Alexa can't discover a virtual device.

## C. Control the real model from anywhere (MQTT)

How it works:

```
 Phone / laptop (GitHub Pages app) ──wss──►  MQTT broker  ◄──tcp── ESP32 in the model
            publishes  smartestate/<HOME_ID>/cmd     {"device":"gate","value":"open"}
            receives   smartestate/<HOME_ID>/state   {"gate":true,"temp":27.4,...}
                       smartestate/<HOME_ID>/online  1 / 0
```

1. In `firmware/smart_estate/secrets.h` set `HOME_ID` to a long, unique name, e.g. `estate-angel-7fq2k9`.
2. Upload the firmware. The Serial Monitor prints `[mqtt] connected`.
3. In the web app: ⚙ → **Online**, broker `wss://broker.hivemq.com:8884/mqtt`, the same Home ID → **Connect**.

**Privacy:** `broker.hivemq.com` is a free *public* test broker. Anyone who guesses your Home ID could press your buttons. That's fine for a school model. For real privacy:

1. Create a free cluster at <https://console.hivemq.cloud> and add a username and password.
2. In `secrets.h`: `MQTT_HOST` = your cluster host, `MQTT_PORT 8883`, plus `MQTT_USER` and `MQTT_PASS`. Port 8883 switches the ESP32 to TLS automatically.
3. In the web app: broker `wss://<your-cluster-host>:8884/mqtt` plus the same username and password.

## Local only (no internet at the exhibition)

* The ESP32 serves the full dashboard itself at **http://smartestate.local** or at the IP shown on the LCD.
* If it can't join Wi-Fi within 20 s, it makes its own hotspot **SmartEstate** (password `smartestate`). Connect your phone to it and open **http://192.168.4.1**.
