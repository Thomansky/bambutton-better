# Bambutton (ESP32-C3, C++)

🇬🇧 English · 🇩🇪 [Deutsch](README.de.md)

A physical button that clears the build plate in [Bambuddy](https://bambuddy.cool).
The LED shows the printer state, one press acknowledges the plate and lets the next
queued job start. **One ESP32-C3 serves two printers.** The web UI is available in
English and German (toggle in the top right corner; the board remembers the choice).

## What version 2.0 changes

Version 2.0 reworks Wi-Fi setup and connection handling. The most common problems of
the 1.x firmware and their causes:

| Problem before | Cause | Now |
|---|---|---|
| Wi-Fi setup often failed, the board stayed in the setup network | The ESP32-C3 Super Mini has a badly matched antenna; at full transmit power (19.5 dBm) the connection often never comes up. The firmware never limited the power. | Transmit power defaults to 8.5 dBm (changeable in the UI) |
| After a power cut the board stayed in the setup network for good | Exactly one 20 s connection attempt at boot, then the setup network and never another try. A router needs 1–3 minutes after a power cut. | The board keeps trying to join the home network forever. The setup network only appears while there is no connection and closes again by itself. |
| Wrong password → no feedback, the board restarted and was gone | Save and restart without a test | The connection is tested live while the phone stays on the setup network. Errors in plain words (network not found, password rejected, no IP from the router …). Only working credentials are stored. |
| The board could not be found after setup | No mDNS, hostname never applied | Reachable as `http://bambutton.local/`; IP and name are shown right after connecting |
| The setup page did not open on some phones | DNS replies were refused for modern devices (EDNS), no redirect for the OS connectivity probes | Own captive-portal DNS, redirects for Android, iOS and Windows |
| UI and buttons froze on "load printers" / "test button"; watchdog reboot every 3 minutes in setup mode | Bambuddy calls ran on the main loop; the Bambuddy task never started in setup mode, so its heartbeat stalled | All Bambuddy calls run in the background task; the page polls for the result |
| A button press fired minutes later when Wi-Fi came back | The press waited in the queue indefinitely | A press expires after 30 s; the LED flickers briefly if it could not be delivered |
| "Test button" reported an error although everything worked | Bambuddy answers HTTP 400 when no print is waiting for the acknowledgement | Shown as "connection OK, nothing to clear" |

Still included: Wi-Fi power save off, every error visible in the web UI, a single
`firmware.bin` (flashable from the browser, OTA afterwards), watchdog against hangs.

## Install

**The easiest way:** open the flash page —

### 👉 https://thomansky.github.io/bambutton-better/

Plug the board in with a data cable, click "Connect & install", done.
Needs Chrome or Edge on a desktop (Web Serial); Firefox and Safari cannot do this.

Alternatively, locally:

```bash
pio run -t upload
```

## Setup

**Directly on the flash page (USB):** right after installing, the flash page offers
**"Connect to Wi-Fi"** (Improv Wi-Fi over the serial port). Pick your network from the list,
enter the password, and the board connects and shows a link to its web UI. This also works
any time later: plug the board in, click "Connect" on the flash page and choose "Connect to
Wi-Fi". Then continue with step 4 below.

**Or with your phone:**

1. After flashing, the board opens the Wi-Fi network **`Bambutton-Setup`** (no password;
   one can be set later in the UI).
2. Connect your phone to it. The setup page opens by itself; if it does not, enter
   **http://192.168.4.1/** in the browser.
3. Pick your Wi-Fi from the list (2.4 GHz only), enter the password, optionally change
   the device name, **"Connect & save"**. The board tries the connection right away while
   your phone stays on the setup network. After a few seconds the result is on the page:
   on success the **IP address and name** (e.g. `http://bambutton.local/`), on failure
   the reason. If your phone closes the setup view during the switch, simply reopen it.
4. Reconnect phone or PC to the home network and open `http://bambutton.local/` (or the
   IP shown). The setup network closes by itself once you have left it (90 s after
   connecting at the earliest, 5 minutes at the latest), or immediately via the button
   on the page.
5. Enter the Bambuddy address (`IP:port`) and API key, **"Test connection & load
   printers"**, assign printers to the two buttons, save.

The API key is created in Bambuddy under *Settings → API Keys* and needs the permissions
**printers:read** and **printers:clear_plate**.

### When the setup network appears and when it disappears

- It is open as long as **no Wi-Fi is stored**.
- It appears **automatically** when the stored Wi-Fi is not reachable 30 s after boot,
  or later drops out for more than a minute. The board keeps trying to reach the home
  network in parallel (power cut: the router eventually comes back and the board
  connects without any action).
- It **closes by itself** once the home network is up: 90 s after connecting when nobody
  is on the setup network any more, 5 minutes after connecting at the latest.
- Without a home network it **switches off by itself after 15 minutes** (Advanced →
  "Close the setup network automatically after"; 5 to 60 minutes or never), as soon as
  nobody is connected to it. The board keeps trying to reach the home network; the setup
  network only comes back when someone opens it.
- It can be opened any time by **holding any button for 5 seconds** (the LED blinks fast
  to confirm; open for 10 minutes), in the UI (Advanced → "Open setup network for 10
  minutes"), by **holding button A while powering on** (open for 5 minutes regardless of
  the home network), or over USB from the flash page ("Connect to Wi-Fi").
- If a Wi-Fi is stored and no connection comes up for 30 minutes, the board restarts,
  provided nobody is on the setup network (self-healing in case the Wi-Fi stack got
  stuck).

## Wiring

```
Button A:  LED = GPIO3,  switch = GPIO4
Button B:  LED = GPIO5,  switch = GPIO6
```

Switch to `3V3` (internal pull-down), LED through a series resistor to `GND`. No 5 V on
the GPIOs. The pins are fixed in the firmware (`src/Settings.cpp`).

A **short press** (released within 1.5 s) clears the plate. **Holding for 5 s** opens the
setup network for 10 minutes (the LED blinks fast to confirm). Anything in between does
nothing.

## LED meaning

| Pattern | Meaning |
|---|---|
| off | no printer assigned (or idle mode "always off") |
| follows the chamber light | normal operation (switchable to "always on" / "always off") |
| slow blink (0.4 s) | plate needs clearing |
| very fast blink | request to Bambuddy in flight |
| two calm blinks after a press | Bambuddy accepted the acknowledgement |
| short flicker after a press | rejected or undeliverable (no Wi-Fi, Bambuddy unreachable, printer not waiting) |
| short flash every 2 s | no link: no Wi-Fi or Bambuddy not answering |
| fast blink on request from the UI | "identify" — shows which button is meant |

## Diagnostics

The **Status & diagnostics** section of the web UI shows live: Wi-Fi state with signal
quality, the reason of the last failed connection attempt, whether the setup network is
open, per button the printer (online/offline, state, plate, light) and its last error,
and the number of Bambuddy polls and errors. **"Test button now"** fires exactly the same
`clear-plate` call as a real press and shows HTTP status, answer and duration — no serial
console needed.

The serial console (115200 baud) additionally logs every Wi-Fi event with its reason, a
health line every 30 s (heap, RSSI, setup network, polls) and every Bambuddy error.

## Troubleshooting

**Cannot connect to Wi-Fi**
- The ESP32-C3 only does **2.4 GHz**. Routers that use one name for 2.4 and 5 GHz usually
  work anyway; otherwise create a dedicated 2.4 GHz network.
- "Password rejected" although the password is right: **WPA3-only** networks are not
  supported, use WPA2 or WPA2/WPA3 mixed mode.
- "The router dropped the connection" / "network not found" at short range: check the
  **transmit power** under *Advanced*. 8.5 dBm is almost always the best choice for the
  Super Mini; higher values frequently make the connection worse.
- A weak USB supply (thin cable, weak charger) makes the board reset while transmitting —
  try another cable or charger.
- Hidden networks: choose "other network (manual)" and type the name.

**The setup page does not open by itself**
- Enter `http://192.168.4.1/` in the browser. Switch mobile data off briefly if the phone
  otherwise reaches the internet via LTE.

**Board cannot be found after setup**
- `http://bambutton.local/` works on iPhone, Mac, Windows 10/11 and most Android devices;
  otherwise use the IP shown on the setup page or look in the router for the hostname
  (default `bambutton`; a new name shows up in the router after the next connection).

**Bambuddy errors**
- *HTTP 401/403*: API key wrong or lacking the permissions `printers:read` /
  `printers:clear_plate`.
- *HTTP 404*: the address does not point at Bambuddy or the printer ID no longer exists.
- *HTTP 400 on test*: not an error — the printer is not waiting for a plate clear.
- *HTTP 429*: Bambuddy limits requests (100/min); the board automatically switches to a
  longer interval for two minutes. Raise the interval in step 2.
- *No connection / timeout*: address as `IP:port` (e.g. `192.168.1.50:8000`), no
  `https`, check the firewall of the Bambuddy host.

## Firmware update

- **Via the UI:** upload a new `firmware.bin` (Advanced), the board restarts. Wrong files
  (bootloader.bin, partitions.bin) are rejected.
- **Via the browser:** install again from the flash page. Settings survive when "Erase"
  is not selected while flashing.

## Build it yourself

```bash
pip install platformio
pio run                 # builds .pio/build/esp32-c3-super-mini/firmware.bin
pio device monitor      # serial output, 115200 baud
```

GitHub Actions builds the firmware on every push to `main`, on pull requests and on tags
(`v*`); binaries are attached to every tag as a release and the flash page is published
from `main` to GitHub Pages.

## License

MIT
