# ESP12F Relay and OLED Controller
 
A small home automation controller built around a bare ESP12F module. It
drives five relays, shows the time and each relay's state on a 0.96 inch
OLED screen, and lets you switch the relays from a web page on your phone
or computer over WiFi.
 
Human presence detection was discussed but left out of this build by
request. If you want it later, the cleanest option is a PIR sensor
(HC-SR501) for simple motion detection, or an mmWave sensor (LD2410) if
you also want a rough occupant count. Either would need its own free
GPIO pin, and the project currently uses all the safe ones, so wiring
would need to be revisited.
 
## Hardware
 
- ESP12F module
- 0.96 inch OLED display, SSD1306 driver, I2C interface
- 5 channel relay module
- 5V supply for the relay module, 3.3V supply for the ESP12F and OLED
- Common ground between the ESP12F, the OLED, and the relay module
See `docs/wiring_diagram.svg` for the connection overview.
 
## Pin assignment
 
| Signal        | ESP12F pin | Notes                                   |
|---------------|------------|------------------------------------------|
| OLED SDA      | D2         |                                            |
| OLED SCL      | D1         |                                            |
| Relay 1       | D5         |                                            |
| Relay 2       | D6         |                                            |
| Relay 3       | D7         |                                            |
| Relay 4       | D0         | No interrupt support, fine as plain output |
| Relay 5       | D3         | Boot strapping pin, must read high at boot |
 
D8 was left unused on purpose. It is a boot strapping pin (GPIO15) that
needs to read low at boot, and most relay boards default to pulling their
input high, which can prevent the board from starting. D4 was also left
free as a spare pin.
 
## Libraries needed in the Arduino IDE
 
Install these through the Library Manager before compiling:
 
- ESP8266 board package (esp8266 by ESP8266 Community)
- Adafruit GFX Library
- Adafruit SSD1306
- LittleFS (bundled with the ESP8266 core)
## Project layout
 
```
esp12f-relay-oled/
  firmware/
    firmware.ino    Main sketch: WiFi, NTP time, relay control, OLED, web server
    config.h        WiFi credentials, pin numbers, relay logic, time zone
  data/
    index.html      Web control page, uploaded to the ESP12F's LittleFS storage
  docs/
    wiring_diagram.svg
  README.md
```
 
Code and documentation are kept apart on purpose: `firmware.ino` and
`config.h` are the only files you need to open in the Arduino IDE, and
`index.html` never needs to be touched by the compiler.
 
## Setup
 
1. Open `firmware/firmware.ino` in the Arduino IDE. Make sure
   `firmware/config.h` sits in the same folder, since the sketch
   includes it directly.
2. Select an ESP8266 board profile that matches your ESP12F breakout
   (a "Generic ESP8266 Module" profile works for most bare modules).
3. Install the ESP8266 LittleFS upload tool for the Arduino IDE if you
   have not already, then use it to upload the `data` folder. This
   copies `index.html` onto the module's flash storage, so the web page
   can be served without also being baked into the sketch.
4. Wire the OLED and relays as listed in the pin table above.
5. Upload `firmware.ino` to the ESP12F.
6. Open the Serial Monitor at 115200 baud to see the WiFi connection
   progress and the IP address the module receives.
7. Visit that IP address in a browser on the same network to reach the
   relay control page.
The WiFi network name and password are already set in `config.h` to
match the network named "Ee". If you move the module to a different
network later, that is the only file you need to edit.
 
## How it behaves
 
- On startup, the OLED shows a connecting message, then the IP address
  it was given, then switches to the normal display: date and time on
  the top line, followed by each relay's name and ON/OFF state.
- The time is fetched once at startup from an NTP server and kept
  running from the module's internal clock afterward, set to Indian
  Standard Time.
- The web page shows a toggle switch for each relay and a live clock,
  both of which refresh once a second.
- Relay wiring assumes an active low relay board, meaning a low signal
  turns the relay on. If your board behaves the opposite way, change
  `RELAY_ACTIVE_LOW` to `false` in `config.h`.
