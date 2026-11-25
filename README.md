# Room Control — ESP8266 Relay & Timer Automation
 
A small home automation box built around a NodeMCU (ESP8266) that controls four relays — in my case a fan, a light, my PC, and a night lamp — from a phone or laptop on the same WiFi network. There's also a little OLED screen bolted to the enclosure so I can see what's on/off and what's about to fire without opening the app at all.
 
No cloud account, no app store, no third-party server sitting in the middle. The board runs its own web server and serves a small dark-themed control panel straight from flash memory. Open the ESP8266's IP address in a browser, flip a switch, done.
 
## What it does
 
- **4 independently controlled relay channels**, each with a name you can see and toggle from the web UI (`Fan`, `Light`, `PC`, `Night` by default — change these to whatever you're actually switching).
- **Delay timers per channel** — "turn this off in 45 minutes" style scheduling. One pending timer per channel at a time.
- **Everything survives a power cut.** Relay states and any pending timers are written to EEPROM, so the board comes back the way you left it.
- **A live OLED status screen** — clock, per-channel on/off state, a little 2×2 indicator block showing which channels are active at a glance, and a WiFi status dot.
- **NTP time sync**, with a browser-pushed fallback. If the board can reach the internet it gets real time from NTP; either way, the web page also pushes the visiting browser's own clock to the board every few minutes, so timers stay accurate even if outbound NTP is blocked on your network.
- **No app to install.** The whole UI is one embedded HTML page served directly from the firmware.
## Hardware
 
This was built for a NodeMCU 1.0 / ESP-12F, an SSD1306 128×64 I2C OLED, and 4 relay modules. Adjust pins in the `#define`s at the top of the sketch if your wiring is different.
 
| Function | GPIO | NodeMCU Label |
|---|---|---|
| OLED SDA | GPIO4 | D2 |
| OLED SCL | GPIO5 | D1 |
| Relay 1 — Fan | GPIO16 | D0 |
| Relay 2 — Light | GPIO14 | D5 |
| Relay 3 — PC | GPIO12 | D6 |
| Relay 4 — Night | GPIO13 | D7 |
 
OLED I2C address is `0x3C`.
 
**A wiring detail that's easy to get backwards:** the relay boards this was built for are active-LOW at the coil, but the loads are wired through the **NC** (normally closed) contact rather than NO. That means, from the ESP8266's point of view, driving a pin **HIGH** actually leaves the relay de-energized and the load powered **through the NC contact** — so `HIGH` = load ON in this firmware (`LOAD_ON` is defined as `HIGH`). If you rewire to NO contacts, or use active-HIGH relay boards, flip the `LOAD_ON` / `LOAD_OFF` defines to match, or your switches will work backwards.
 
## Safe first-run checklist

This firmware controls physical relay outputs and exposes unauthenticated HTTP endpoints. Treat the first power-up as a hardware test, not as a ready-to-use mains controller.

1. **Configure Wi-Fi first.** Set the `ssid` and `password` constants in the sketch for a protected network. The checked-in password string is empty; do not deploy it unchanged on an open or untrusted network.
2. **Disconnect the loads during initial tests.** Upload with the relay contacts disconnected from appliances. Confirm the board boots, the OLED works, and the intended channel mapping is correct before attaching any load.
3. **Verify relay logic and contacts.** This firmware assumes active-LOW relay modules with loads wired through NC contacts. In this configuration, firmware state ON drives the pin HIGH and powers the load through NC. A different relay board or NO wiring changes that relationship. Do not rely on labels alone; verify the actual module and contact wiring with power disconnected.
4. **Account for saved state.** Relay states are restored from EEPROM after a valid stored configuration is found. A previously saved ON state can therefore energize a connected load at boot.
5. **Keep mains work safe.** Never place mains wiring on a breadboard or alongside exposed low-voltage logic. Use properly rated enclosures, protection, clearances, strain relief, and wiring practices. If you are not qualified to wire mains loads safely, test only with low-voltage loads and get competent help for mains installation.
6. **Keep it on a trusted LAN.** There is no login, no encryption, and no authorization on the relay API. Do not port-forward the device or expose it directly to the internet.

## Getting it flashed
 
1. Install the ESP8266 board package in Arduino IDE (Boards Manager → add `http://arduino.esp8266.com/stable/package_esp8266com_index.json`, then install "esp8266").
2. Install two libraries via the Library Manager: **Adafruit GFX Library** and **Adafruit SSD1306**. (`EEPROM`, `Wire`, `ESP8266WiFi`, and `ESP8266WebServer` come bundled with the board package.)
3. Board settings: **NodeMCU 1.0 (ESP-12E Module)**, default flash size, 115200 upload speed.
4. Open the sketch, update `ssid` and `password` near the top for your network, and flash it.
5. Open Serial Monitor at 115200 baud on first boot — it prints the assigned IP address once WiFi connects. That's the address you'll open in a browser.
> **Wi-Fi credentials:** the SSID and password are hardcoded in the sketch, and the checked-in password string is empty. Replace both with the correct credentials for a protected network before upload. Do not commit real credentials to a public repository; if you move them to a local `secrets.h`, add that file to `.gitignore` and include a placeholder example instead.
 
## Using it
 
Open `http://<board-ip>/` on any device on the same network. Each channel gets a card with a toggle switch and a small timer form — punch in a number of minutes, pick ON or OFF, hit **Set**, and the board will flip that channel at the target time regardless of whether the browser tab is still open. The page polls the board every second for live state and shows a countdown for any pending timer; there's a red **X** button to cancel one early.
 
The OLED on the device itself mirrors all of this: current time up top, each channel's name and state, an asterisk next to any channel with a timer pending, a filled/outline quadrant graphic for at-a-glance on/off status, and a small dot in the bottom right that's filled when connected to WiFi and hollow when it's not.
 
## REST API
 
The web page is just a client of this API — you can hit these directly too (e.g. from a shortcut, a script, or another home automation tool).
 
| Endpoint | Params | What it does |
|---|---|---|
| `GET /` | — | Serves the control panel page |
| `GET /api/status` | — | JSON with current time and, for each of the 4 channels: state, name, and pending-timer info |
| `GET /api/toggle` | `id` (0–3) | Flips that channel on/off |
| `GET /api/timer` | `id`, `m` (minutes), `a` (`1`=ON, `0`=OFF) | Schedules a delayed action on that channel |
| `GET /api/timer` | `id`, `c=1` | Cancels the pending timer on that channel |
| `GET /api/settime` | `epoch` | Pushes a Unix timestamp to the board — used by the web page as a clock fallback |
 
`/api/status` looks roughly like this:
 
```json
{
  "time": "07:42:15 PM",
  "relays": [
    { "state": true, "name": "Fan", "t_active": true, "t_action": false, "t_rem": 812 }
  ]
}
```
 
## How state is stored
 
Everything lives in one `ConfigStore` struct written to EEPROM: a magic number (`0xC0FFEE89`) used to tell "real saved data" apart from a blank/uninitialized chip, the current on/off state of all 4 channels, and up to one pending timer per channel (target Unix time, the action to take, and whether it's active). Any change — a manual toggle, a new timer, a timer firing — gets committed to EEPROM immediately.
 
## Things worth knowing before you rely on this
 
- **No login, no auth.** Anyone on your WiFi can hit the API and flip your relays. Fine for a home LAN; don't port-forward this to the open internet without putting something in front of it.
- **One timer per channel.** Setting a new timer on a channel overwrites whatever was pending on it.
- **Timers survive a reboot, but "catch up" on reconnect.** If the board loses power and misses a timer's target time entirely, it'll fire that action as soon as its clock re-syncs after boot, even if that's later than intended.
- **Plain HTTP**, no encryption.
## License
 
No license file is attached yet, which technically means default copyright applies and others can't reuse the code without asking. If you'd like this to be freely usable/forkable, add an MIT `LICENSE` file — GitHub can generate one for you right from the "Add file" menu when creating or editing the repo.


---

## Skills & technologies

`Arduino/C++` · `HTML/CSS`


## Repository structure

Key files and folders (not exhaustive):

- `README.md`
- `Room_Control_Beginner_Build_Guide.docx`
- `index.html`
- `smart-switchboard-with-watch-function.ino`

`smart-switchboard-with-watch-function.ino` is the firmware and embeds the served control page in its `INDEX_HTML` string. The separate root-level `index.html` is a standalone documentation/build-guide page; changing it alone does **not** change the page served by the ESP8266. The DOCX file is an additional repository artifact; its contents were not independently reviewed in this pass.

## How it works

The main firmware sketch is `smart-switchboard-with-watch-function.ino`. Open it in Arduino IDE, verify the board, pin mapping and library requirements, then compile and upload. Test hardware behaviour on the real circuit before relying on it.


## Maintenance & Audit

- **Audit date:** 21 November 2026
- **Documentation status:** README reviewed and audit record added on this date.
- **Scope and evidence:** This is a documentation-level maintenance pass. It does not establish that source code compiles, dependencies are current, security has been tested, or hardware/runtime behavior has been verified.
- **Code and tests:** No new functional code changes or test results are claimed by this README update. Run project-specific build, test, and hardware/runtime checks before relying on the project.
- **Maintenance baseline:** Documentation status recorded through 11 November 2026; revisit setup instructions, dependencies, safety limits, known limitations, and tests when the project changes.

## Contact

Maintained up to 25 November 2026.

If you encounter any problems, please contact [carmodbhai@gmail.com](mailto:carmodbhai@gmail.com).
