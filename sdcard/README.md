# Speaker SD card

Copy the **`mp3` folder** onto an empty micro-SD card (FAT32, up to 32 GB) and put the card in the DFPlayer Mini.
The card must look like this:

```
SD card
└── mp3
    ├── 0001.mp3
    ├── 0002.mp3
    └── …
```

The estate speaks these clips by number. You can replace any of them with your own recording (same file name, MP3),
or change the words in `tools/make_voice_clips.py` and run it again.

| File | Name (for `say=`) | Words |
|---|---|---|
| `0001.mp3` | `online` | Smart estate is online. |
| `0002.mp3` | `gate_opening` | Gate opening. |
| `0003.mp3` | `gate_closing` | Gate closing. |
| `0004.mp3` | `gate_stopped` | Gate stopped. |
| `0005.mp3` | `lights_on` | All lights on. |
| `0006.mp3` | `lights_off` | All lights off. |
| `0007.mp3` | `living_on` | Living room light on. |
| `0008.mp3` | `living_off` | Living room light off. |
| `0009.mp3` | `bedroom_on` | Bedroom light on. |
| `0010.mp3` | `bedroom_off` | Bedroom light off. |
| `0011.mp3` | `garage_on` | Garage light on. |
| `0012.mp3` | `garage_off` | Garage light off. |
| `0013.mp3` | `outdoor_on` | Outdoor lights on. |
| `0014.mp3` | `outdoor_off` | Outdoor lights off. |
| `0015.mp3` | `fan_off` | Fan off. |
| `0016.mp3` | `fan_1` | Fan speed one. |
| `0017.mp3` | `fan_2` | Fan speed two. |
| `0018.mp3` | `fan_3` | Fan speed three. |
| `0019.mp3` | `fan_breeze` | Fan breeze mode. |
| `0020.mp3` | `strip_on` | Strip lights on. |
| `0021.mp3` | `strip_off` | Strip lights off. |
| `0022.mp3` | `goodnight` | Good night. Sleep well. |
| `0023.mp3` | `welcome` | Welcome home! |
| `0024.mp3` | `party` | Party mode. Let's go! |
| `0025.mp3` | `doorbell` | 🔔 ding-dong, then “Someone is at the gate.” |
| `0026.mp3` | `hotspot` | Wi-Fi not found. I started my own hotspot, called Smart Estate. |
| `0027.mp3` | `hello` | Hello! I am your smart estate. Control me with your voice, the app, Alexa, or the buttons. |
| `0028.mp3` | `beep` | short beep |
| `0029.mp3` | `gate_open` | The gate is open. |
| `0030.mp3` | `gate_closed` | The gate is closed. |

Test it from the Serial Monitor: type `say hello` and press Enter.
No sound? Check the speaker is on **SPK1 / SPK2** (not GND), the 1 kΩ resistor is between GPIO 4 and the DFPlayer **RX** pin, and the card is FAT32.
