#!/usr/bin/env python3
"""
Make the spoken MP3 clips for the DFPlayer Mini speaker.

    python3 tools/make_voice_clips.py

Writes sdcard/mp3/0001.mp3, 0002.mp3 ... The NUMBER of each clip must
match the Track list in firmware/smart_estate/smart_estate.ino, so only
change the TEXT below (or record your own voice and save it under the
same file name).

Needs ffmpeg built with the "flite" text-to-speech voice (Linux builds
usually have it). The finished clips are already in the repository, so
you only need this if you want different words.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "sdcard" / "mp3"
VOICE = "slt"  # flite voices: slt (female), rms, awb, kal (male)

# (track number, name used by  say=<name>, words)
TRACKS = [
    (1, "online", "Smart estate is online."),
    (2, "gate_opening", "Gate opening."),
    (3, "gate_closing", "Gate closing."),
    (4, "gate_stopped", "Gate stopped."),
    (5, "lights_on", "All lights on."),
    (6, "lights_off", "All lights off."),
    (7, "living_on", "Living room light on."),
    (8, "living_off", "Living room light off."),
    (9, "bedroom_on", "Bedroom light on."),
    (10, "bedroom_off", "Bedroom light off."),
    (11, "garage_on", "Garage light on."),
    (12, "garage_off", "Garage light off."),
    (13, "outdoor_on", "Outdoor lights on."),
    (14, "outdoor_off", "Outdoor lights off."),
    (15, "fan_off", "Fan off."),
    (16, "fan_1", "Fan speed one."),
    (17, "fan_2", "Fan speed two."),
    (18, "fan_3", "Fan speed three."),
    (19, "fan_breeze", "Fan breeze mode."),
    (20, "strip_on", "Strip lights on."),
    (21, "strip_off", "Strip lights off."),
    (22, "goodnight", "Good night. Sleep well."),
    (23, "welcome", "Welcome home!"),
    (24, "party", "Party mode. Let's go!"),
    (25, "doorbell", "CHIME Someone is at the gate."),
    (26, "hotspot", "Wi-Fi not found. I started my own hotspot, called Smart Estate."),
    (27, "hello", "Hello! I am your smart estate. Control me with your voice, the app, Alexa, or the buttons."),
    (28, "beep", "BEEP"),
    (29, "gate_open", "The gate is open."),
    (30, "gate_closed", "The gate is closed."),
]

ENC = ["-ar", "44100", "-ac", "1", "-c:a", "libmp3lame", "-b:a", "128k", "-map_metadata", "-1"]


def esc(text):
    # flite filter text: escape characters that the filter parser treats specially
    return text.replace("\\", "\\\\").replace("'", "\\\\\\'").replace(":", "\\:").replace(",", "\\,")


def ffmpeg(args, out):
    cmd = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", *args, *ENC, str(out)]
    subprocess.run(cmd, check=True)


def speech_input(text):
    return ["-f", "lavfi", "-i", f"flite=text='{esc(text)}':voice={VOICE}"]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    loud = "loudnorm=I=-14:TP=-1.5:LRA=11"
    for num, name, words in TRACKS:
        out = OUT / f"{num:04d}.mp3"
        if words == "BEEP":
            ffmpeg(["-f", "lavfi", "-i", "sine=frequency=1200:duration=0.18",
                    "-af", "afade=t=out:st=0.12:d=0.06,volume=0.6"], out)
        elif words.startswith("CHIME"):
            text = words[len("CHIME"):].strip()
            ffmpeg(["-f", "lavfi", "-i", "sine=frequency=659:duration=0.7",
                    "-f", "lavfi", "-i", "sine=frequency=523:duration=1.0",
                    *speech_input(text),
                    "-filter_complex",
                    "[0]afade=t=out:st=0.05:d=0.65,volume=0.5,aresample=44100[a];"
                    "[1]afade=t=out:st=0.05:d=0.95,volume=0.5,aresample=44100[b];"
                    f"[2]aresample=44100,{loud}[c];[a][b][c]concat=n=3:v=0:a=1"], out)
        else:
            ffmpeg([*speech_input(words), "-af", f"aresample=44100,{loud},apad=pad_dur=0.15"], out)
        print(f"{out.relative_to(ROOT)}  {name:13s} {words}")


if __name__ == "__main__":
    try:
        main()
    except FileNotFoundError:
        sys.exit("ffmpeg not found – install it, or use the MP3 files already in sdcard/mp3")
