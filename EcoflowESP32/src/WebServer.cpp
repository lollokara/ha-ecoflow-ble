#include "WebServer.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_log.h>

static const char* TAG = "WebServer";
AsyncWebServer WebServer::server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Ecoflow Controller</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: sans-serif; background: #1a1a1a; color: #eee; margin: 0; padding: 20px; }
        .container { max-width: 800px; margin: auto; display: grid; gap: 20px; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
        .card { background: #2d2d2d; padding: 15px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.3); }
        h2 { margin-top: 0; border-bottom: 1px solid #444; padding-bottom: 10px; }
        .status-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
        .label { color: #aaa; font-size: 0.9em; }
        .value { font-weight: bold; font-size: 1.1em; }
        .btn-group { margin-top: 15px; display: flex; gap: 10px; }
        button { padding: 8px 12px; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; color: white; }
        .btn-con { background: #2196F3; }
        .btn-dis { background: #F44336; }
        .btn-cmd { background: #4CAF50; }
        input { padding: 8px; border-radius: 4px; border: 1px solid #555; background: #333; color: white; width: 100%; box-sizing: border-box; margin-bottom: 10px; }
        .led { display: inline-block; width: 10px; height: 10px; border-radius: 50%; background: #555; margin-right: 5px; }
        .led.on { background: #0f0; box-shadow: 0 0 5px #0f0; }
    </style>
</head>
<body>
    <h1>Ecoflow Controller</h1>
    <div class="container" id="app">
        <!-- Cards will be injected here -->
    </div>

    <div class="card" style="margin-top: 20px; max-width: 800px; margin-left: auto; margin-right: auto;">
        <h2>Command Console</h2>
        <input type="text" id="cmdInput" placeholder="Enter command (e.g. sys_temp, d3_set_ac 1)..." onkeydown="if(event.key==='Enter') sendCmd()">
        <button class="btn-cmd" onclick="sendCmd()">Send</button>
        <pre id="cmdOutput" style="background: #000; padding: 10px; margin-top: 10px; border-radius: 4px; min-height: 50px;"></pre>
    </div>

<script>
    const devices = {
        d3: "Delta 3",
        w2: "Wave 2",
        d3p: "Delta Pro 3",
        ac: "Alt Charger"
    };

    function updateStatus() {
        fetch('/api/status')
            .then(response => response.json())
            .then(data => {
                const app = document.getElementById('app');
                app.innerHTML = ''; // Rebuild (simple approach)

                for (const [key, info] of Object.entries(data)) {
                    const name = devices[key] || key;
                    const html = `
                        <div class="card">
                            <h2>
                                <span class="led ${info.connected ? 'on' : ''}"></span>
                                ${name}
                            </h2>
                            <div class="status-grid">
                                <div><span class="label">Status:</span> <span class="value">${info.connected ? 'Connected' : 'Disconnected'}</span></div>
                                <div><span class="label">SN:</span> <span class="value">${info.sn || '-'}</span></div>
                                <div><span class="label">Battery:</span> <span class="value">${info.batt}%</span></div>
                            </div>
                            <div class="btn-group">
                                <button class="btn-con" onclick="connect('${key}')">Connect</button>
                                <button class="btn-dis" onclick="disconnect('${key}')">Disconnect</button>
                            </div>
                        </div>
                    `;
                    app.innerHTML += html;
                }
            });
    }

    function connect(type) {
        fetch('/api/connect', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({type: type}) });
    }

    function disconnect(type) {
        fetch('/api/disconnect', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({type: type}) });
    }

    function sendCmd() {
        const cmd = document.getElementById('cmdInput').value;
        if(!cmd) return;
        fetch('/api/command', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({cmd: cmd}) })
            .then(r => r.text()).then(t => {
                document.getElementById('cmdOutput').innerText = "Command sent.";
                document.getElementById('cmdInput').value = '';
            });
    }

    setInterval(updateStatus, 2000);
    updateStatus();
</script>
</body>
</html>
)rawliteral";

void WebServer::begin() {
    Preferences prefs;
    prefs.begin("ecoflow", true);
    String ssid = prefs.getString("wifi_ssid", "");
    String pass = prefs.getString("wifi_pass", "");
    prefs.end();

    if (ssid.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());
        ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid.c_str());
        // We don't block here, let it connect in background
    } else {
        ESP_LOGW(TAG, "No WiFi credentials saved. Web UI disabled.");
        return;
    }

    setupRoutes();
    server.begin();
}

void WebServer::setupRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    server.on("/api/status", HTTP_GET, handleStatus);

    server.on("/api/command", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handleCommand);
    server.on("/api/connect", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handleScan);
    server.on("/api/disconnect", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handleDisconnect);
}

void WebServer::handleStatus(AsyncWebServerRequest *request) {
    String json = DeviceManager::getInstance().getDeviceStatusJson();
    request->send(200, "application/json", json);
}

void WebServer::handleCommand(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, data, len);
    String cmd = doc["cmd"];
    if (cmd.length() > 0) {
        CmdUtils::processInput(cmd);
        request->send(200, "text/plain", "OK");
    } else {
        request->send(400, "text/plain", "Bad Request");
    }
}

void WebServer::handleScan(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, data, len);
    String typeStr = doc["type"];

    DeviceType type = DeviceType::DELTA_3;
    if (typeStr == "w2") type = DeviceType::WAVE_2;
    else if (typeStr == "d3p") type = DeviceType::DELTA_PRO_3;
    else if (typeStr == "ac") type = DeviceType::ALTERNATOR_CHARGER;

    DeviceManager::getInstance().scanAndConnect(type);
    request->send(200, "text/plain", "Scanning...");
}

void WebServer::handleDisconnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, data, len);
    String typeStr = doc["type"];

    DeviceType type = DeviceType::DELTA_3;
    if (typeStr == "w2") type = DeviceType::WAVE_2;
    else if (typeStr == "d3p") type = DeviceType::DELTA_PRO_3;
    else if (typeStr == "ac") type = DeviceType::ALTERNATOR_CHARGER;

    DeviceManager::getInstance().disconnect(type);
    request->send(200, "text/plain", "Disconnected");
}
