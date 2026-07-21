#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <sys/time.h>

/**
 * HOME AUTOMATION ESP12F FIRMWARE + TIMERS
 * Hardware: NodeMCU 1.0 / ESP-12F
 * Display: SSD1306 128x64 I2C (0x3C)
 * Relays: Active LOW, Wired COM+NC -> (Pin HIGH = Relay OFF physically = Load ON via NC)
 */

#define OLED_SDA 4 // D2
#define OLED_SCL 5 // D1

#define RELAY1_PIN 16 // D0
#define RELAY2_PIN 14 // D5
#define RELAY3_PIN 12 // D6
#define RELAY4_PIN 13 // D7

// Logic Setup
#define LOAD_ON  HIGH
#define LOAD_OFF LOW

// Networking & Time Configuration
const char* ssid = "Ee";
const char* password = "";
const char* TZ_INFO = "IST-5:30"; // Indian Standard Time
const char* NTP_SERVERS[] = {"pool.ntp.org", "time.google.com"};

// Data Storage (EEPROM Layout)
struct Timer {
    uint32_t targetTime; 
    bool action; // true = turn ON, false = turn OFF
    bool active;
};

struct ConfigStore {
    uint32_t magic;
    bool states[4];
    Timer timers[4];
} memory;

// Globals
ESP8266WebServer server(80);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

const uint8_t relayPins[] = {RELAY1_PIN, RELAY2_PIN, RELAY3_PIN, RELAY4_PIN};
const char* relayNames[] = {"Fan", "Light", "PC", "Night"};
bool timeSynced = false;

// ---------------------------------------------------------
// WEB INTERFACE (EMBEDDED HTML)
// ---------------------------------------------------------
const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Room Control</title>
<style>
:root { --bg: #121212; --card: #1e1e1e; --text: #e0e0e0; --acc: #03dac6; --err: #cf6679; }
body { font-family: -apple-system, system-ui, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 20px; text-align: center; }
.clock { font-size: 2.5rem; font-weight: bold; color: var(--acc); margin-bottom: 20px; font-variant-numeric: tabular-nums; text-shadow: 0 0 10px rgba(3,218,198,0.3); }
.grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(290px, 1fr)); gap: 18px; max-width: 900px; margin: auto; text-align: left; }
.card { background: var(--card); border-radius: 16px; padding: 20px; box-shadow: 0 6px 16px rgba(0,0,0,0.4); border-top: 4px solid #bb86fc; }
.card-top { display: flex; justify-content: space-between; align-items: center; padding-bottom: 12px; border-bottom: 1px solid #333; margin-bottom: 15px;}
.name { font-size: 1.5rem; font-weight: 600; color: #fff; }
.switch { position: relative; width: 66px; height: 36px; display: inline-block; }
.switch input { opacity: 0; width: 0; height: 0; }
.slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background: #444; transition: .3s; border-radius: 36px; }
.slider:before { position: absolute; content: ""; height: 26px; width: 26px; left: 5px; bottom: 5px; background: white; transition: .3s; border-radius: 50%; box-shadow: 0 2px 4px rgba(0,0,0,0.2); }
input:checked + .slider { background: var(--acc); }
input:checked + .slider:before { transform: translateX(30px); }
.tmr { background: #262626; padding: 12px; border-radius: 8px; display: flex; gap: 6px; align-items: center; flex-wrap: wrap; }
input[type=number], select, button { background: #333; color: white; border: 1px solid #555; padding: 7px; border-radius: 6px; outline: none; font-size:0.9rem;}
input[type=number] { width: 55px; text-align: center; }
button { background: var(--acc); color: black; font-weight: bold; cursor: pointer; padding: 7px 14px; transition: transform 0.1s;}
button:active { transform: scale(0.93); }
button.btn-cancel { background: var(--err); color: white; }
.info { font-size: 0.85rem; color: #999; margin-top: 10px; font-style: italic; min-height: 18px; }
</style></head>
<body>
<div class="clock" id="clock">Connecting...</div>
<div class="grid" id="grid"></div>
<script>
const audio = new (window.AudioContext || window.webkitAudioContext)();
function clickSound() {
  try {
    const o = audio.createOscillator(); const g = audio.createGain();
    o.connect(g); g.connect(audio.destination); o.type = 'sine'; o.frequency.setValueAtTime(800, audio.currentTime);
    g.gain.setValueAtTime(0.05, audio.currentTime); g.gain.exponentialRampToValueAtTime(0.001, audio.currentTime + 0.1);
    o.start(); o.stop(audio.currentTime + 0.1);
  } catch(e){}
}

// Generate UI dynamically so inputs keep focus on refresh
const names = ["Fan", "Light", "PC", "Night"];
let gridHtml = '';
for(let i=0; i<4; i++) {
  gridHtml += `
  <div class="card">
    <div class="card-top">
      <span class="name">${names[i]}</span>
      <label class="switch"><input type="checkbox" id="sw${i}" onchange="act(${i})"><span class="slider"></span></label>
    </div>
    <div class="tmr">
      Timer: <input type="number" id="tm${i}" placeholder="Min" min="1">
      <select id="ta${i}"><option value="1">ON</option><option value="0">OFF</option></select>
      <button onclick="setTmr(${i})">Set</button>
      <button id="tc${i}" class="btn-cancel" onclick="canTmr(${i})" style="display:none">X</button>
    </div>
    <div class="info" id="ti${i}">No timer active</div>
  </div>`;
}
document.getElementById('grid').innerHTML = gridHtml;

async function act(id) {
  clickSound(); 
  await fetch('/api/toggle?id=' + id);
  refresh();
}

async function setTmr(id) {
  clickSound();
  let m = document.getElementById('tm'+id).value;
  let a = document.getElementById('ta'+id).value;
  if(m > 0) { await fetch(`/api/timer?id=${id}&m=${m}&a=${a}`); refresh(); }
}

async function canTmr(id) {
  clickSound();
  await fetch('/api/timer?id='+id+'&c=1');
  refresh();
}

async function refresh() {
  try {
    const res = await fetch('/api/status');
    const d = await res.json();
    document.getElementById('clock').innerText = d.time;
    for (let i = 0; i < 4; i++) {
      let r = d.relays[i];
      let cb = document.getElementById('sw' + i);
      if (cb.checked !== r.state) cb.checked = r.state;
      
      let info = document.getElementById('ti' + i);
      let cx = document.getElementById('tc' + i);
      if (r.t_active) {
          cx.style.display = 'inline-block';
          info.innerText = `Pending: Turn ${r.t_action ? 'ON' : 'OFF'} in ${Math.floor(r.t_rem / 60)}m ${r.t_rem % 60}s`;
          info.style.color = '#03dac6';
      } else {
          cx.style.display = 'none';
          info.innerText = 'No timer active';
          info.style.color = '#999';
      }
    }
  } catch(e){}
}

async function pushTime() {
  try { await fetch('/api/settime?epoch=' + Math.floor(Date.now()/1000)); } catch(e){}
}
pushTime();
setInterval(pushTime, 300000); // Correct MCU clock skew if internet offline
setInterval(refresh, 1000);    // Update displays & countdowns 
</script>
</body></html>
)=====";

// ---------------------------------------------------------
// CORE SYSTEM HELPERS
// ---------------------------------------------------------
void commitState() {
    EEPROM.put(0, memory);
    EEPROM.commit();
}

void applyHardwareStates() {
    for (int i = 0; i < 4; i++) {
        digitalWrite(relayPins[i], memory.states[i] ? LOAD_ON : LOAD_OFF);
    }
}

// Ensure the local time epoch represents actual reality, avoiding false timer fires at boot 
void evaluateTimeSync() {
    if (!timeSynced && time(nullptr) > 1600000000) {
        timeSynced = true;
        Serial.println(F("[SYSTEM] Network/Browser Clock Sync Confirmed"));
    }
}

// Evaluates scheduled tasks dynamically
void processTimers() {
    evaluateTimeSync();
    if (!timeSynced) return;
    
    time_t now = time(nullptr);
    bool stateChanged = false;

    for (int i = 0; i < 4; i++) {
        if (memory.timers[i].active) {
            if ((uint32_t)now >= memory.timers[i].targetTime) {
                // Timer threshold hit! Activate the rule
                memory.states[i] = memory.timers[i].action;
                memory.timers[i].active = false; // Mark complete
                stateChanged = true;
                Serial.printf("[TIMER] Channel %d auto-switched to %s\n", i, memory.states[i] ? "ON" : "OFF");
            }
        }
    }

    // Persist and apply changes if timer successfully fired
    if (stateChanged) {
        applyHardwareStates();
        commitState();
    }
}

// ---------------------------------------------------------
// HTTP ROUTING CONTROLLERS
// ---------------------------------------------------------
void handleStatus() {
    evaluateTimeSync();
    char tBuffer[30] = "Syncing Time...";
    time_t now = time(nullptr);
    
    if (timeSynced) {
        struct tm* p = localtime(&now);
        int h = p->tm_hour % 12;
        if (h == 0) h = 12;
        snprintf(tBuffer, sizeof(tBuffer), "%02d:%02d:%02d %s", h, p->tm_min, p->tm_sec, (p->tm_hour >= 12 ? "PM" : "AM"));
    }

    char json[700]; // Generate output JSON avoiding dynamic strings fragmenting heap
    int pos = snprintf(json, sizeof(json), "{\"time\":\"%s\",\"relays\":[", tBuffer);
    
    for (int i = 0; i < 4; i++) {
        long remaining = 0;
        if (memory.timers[i].active) {
            remaining = (long)memory.timers[i].targetTime - (long)now;
            if (remaining < 0) remaining = 0;
        }

        pos += snprintf(json + pos, sizeof(json) - pos,
            "{\"state\":%s,\"name\":\"%s\",\"t_active\":%s,\"t_action\":%s,\"t_rem\":%ld}%s",
            memory.states[i] ? "true" : "false",
            relayNames[i],
            memory.timers[i].active ? "true" : "false",
            memory.timers[i].action ? "true" : "false",
            remaining,
            (i < 3) ? "," : ""
        );
    }
    snprintf(json + pos, sizeof(json) - pos, "]}");
    
    server.send(200, "application/json", json);
}

void handleToggle() {
    if (server.hasArg("id")) {
        int id = server.arg("id").toInt();
        if (id >= 0 && id < 4) {
            memory.states[id] = !memory.states[id];
            applyHardwareStates();
            commitState();
            Serial.printf("[USER] Channel %d toggled. Now %s\n", id, memory.states[id] ? "ON" : "OFF");
        }
    }
    server.send(200, "text/plain", "OK");
}

void handleTimer() {
    if (server.hasArg("id")) {
        int id = server.arg("id").toInt();
        if (id >= 0 && id < 4) {
            // Cancel specific Channel's timer setup
            if (server.hasArg("c")) { 
                memory.timers[id].active = false;
                Serial.printf("[USER] Cleared Timer for Channel %d\n", id);
            } 
            // Set newly configured Delay Action
            else if (server.hasArg("m") && server.hasArg("a")) {
                int minutes = server.arg("m").toInt();
                bool actionReq = (server.arg("a").toInt() == 1);
                
                memory.timers[id].targetTime = time(nullptr) + (minutes * 60);
                memory.timers[id].action = actionReq;
                memory.timers[id].active = true;
                
                Serial.printf("[USER] Set Timer %d mins on Channel %d to turn %s\n", 
                                minutes, id, actionReq ? "ON" : "OFF");
            }
            commitState();
        }
    }
    server.send(200, "text/plain", "OK");
}

void handleSetTime() {
    // Allows Javascript web clients to forcefully pass timestamp on missing/broken Wifi link
    if (server.hasArg("epoch")) {
        time_t ep = (time_t)server.arg("epoch").toInt();
        if (ep > 1600000000) {
            struct timeval tv = { ep, 0 };
            settimeofday(&tv, nullptr);
            timeSynced = true;
        }
    }
    server.send(200, "text/plain", "OK");
}

// ---------------------------------------------------------
// OLED RENDERING ENGINE
// ---------------------------------------------------------
void drawDisplay() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 150) return; // Throttled updates ~6.6 FPS
    lastUpdate = millis();

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    // Header Line - System Digital Clock rendering
    display.setTextSize(1);
    display.setCursor(0, 0);
    evaluateTimeSync();
    
    if (timeSynced) {
        time_t now = time(nullptr);
        struct tm* p = localtime(&now);
        int h = p->tm_hour % 12;
        if (h == 0) h = 12;
        
        char blink = (millis() % 1000 < 500) ? ':' : ' ';
        char clk[25];
        snprintf(clk, sizeof(clk), "%02d%c%02d%c%02d %s", h, blink, p->tm_min, blink, p->tm_sec, (p->tm_hour >= 12 ? "PM" : "AM"));
        display.print(clk);
    } else {
        display.print(F("Syncing Time..."));
    }

    // Body Grid Generation - Relay Text Left side
    for (int i = 0; i < 4; i++) {
        int y = 18 + (i * 11);
        display.setCursor(0, y);
        display.print(relayNames[i]);
        
        display.setCursor(45, y);
        // Additional graphical info: show a trailing `*` marker when active timers apply 
        display.print(memory.states[i] ? F("ON") : F("OFF"));
        if(memory.timers[i].active) {
           display.print(F(" *"));
        }
    }

    // Visual Indicator Component - 40x40 Graphic representation Right Side
    int baseX = 82;
    int baseY = 16;
    int sz = 40;
    
    display.drawRect(baseX, baseY, sz, sz, SSD1306_WHITE); // Outer bounds Shell 
    display.drawLine(baseX + sz/2, baseY, baseX + sz/2, baseY + sz, SSD1306_WHITE); // Split vert
    display.drawLine(baseX, baseY + sz/2, baseX + sz, baseY + sz/2, SSD1306_WHITE); // Split hori

    // Fill quadrant boxes if specific load is triggered ON 
    int fillPadding = 2;
    int fillW = (sz / 2) - (fillPadding * 2) + 1;
    if (memory.states[0]) display.fillRect(baseX + fillPadding, baseY + fillPadding, fillW, fillW, SSD1306_WHITE);
    if (memory.states[1]) display.fillRect(baseX + (sz/2) + fillPadding, baseY + fillPadding, fillW, fillW, SSD1306_WHITE);
    if (memory.states[2]) display.fillRect(baseX + fillPadding, baseY + (sz/2) + fillPadding, fillW, fillW, SSD1306_WHITE);
    if (memory.states[3]) display.fillRect(baseX + (sz/2) + fillPadding, baseY + (sz/2) + fillPadding, fillW, fillW, SSD1306_WHITE);

    // Wireless connection stability element symbolization
    if (WiFi.status() == WL_CONNECTED) {
        display.fillCircle(124, 60, 3, SSD1306_WHITE); // Filled bubble online
    } else {
        display.drawCircle(124, 60, 3, SSD1306_WHITE); // Outline bubble local net offline
    }

    display.display();
}

// ---------------------------------------------------------
// HARDWARE INITIALIZATIONS & BACKGROUND THREAD
// ---------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println(F("\n[BOOT] Timer Control System initializing."));

    // Secure load and format layout struct bounds inside EEPROM Flash
    EEPROM.begin(sizeof(ConfigStore));
    EEPROM.get(0, memory);
    
    if (memory.magic != 0xC0FFEE89) { // Fresh allocation format token match
        memory.magic = 0xC0FFEE89;
        for (int i = 0; i < 4; i++) {
            memory.states[i] = false;
            memory.timers[i].active = false; 
        }
        commitState();
        Serial.println(F("[EEPROM] New storage structure built cleanly."));
    } else {
        Serial.println(F("[EEPROM] Retained state profiles injected successfully."));
    }

    // Attach GPIO channels, force setup immediately minimizing relay false chatter at boot.
    for (int i = 0; i < 4; i++) {
        pinMode(relayPins[i], OUTPUT);
    }
    applyHardwareStates();

    // Graphic Setup Pipeline
    Wire.begin(OLED_SDA, OLED_SCL);
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        display.clearDisplay();
        display.display();
    }

    // Initiate WiFi Station configuration context 
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    // Establish external clock linkage
    configTime(TZ_INFO, NTP_SERVERS[0], NTP_SERVERS[1]);

    // Dispatch REST callbacks mappings 
    server.on("/", []() { server.send_P(200, "text/html", INDEX_HTML); });
    server.on("/api/status", handleStatus);
    server.on("/api/toggle", handleToggle);
    server.on("/api/timer", handleTimer);
    server.on("/api/settime", handleSetTime);
    server.begin();

    Serial.println(F("[HTTP] Background interfaces engaged."));
}

void loop() {
    server.handleClient();
    processTimers(); // Interceptor testing timeline thresholds limits for events
    drawDisplay();   // Visual representations refreshed continuously

    // Gracefully handling network connectivity detach events in background bounds
    static unsigned long wifiFailSafeCheck = 0;
    if (millis() - wifiFailSafeCheck > 10000) {
        wifiFailSafeCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.begin(ssid, password); 
        }
    }
}