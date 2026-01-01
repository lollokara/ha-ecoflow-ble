from playwright.sync_api import sync_playwright
import re

html_template = r"""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Ecoflow CTRL</title>
    <style>
        :root {
            --bg-dark: #050505;
            --glass-bg: rgba(20, 20, 25, 0.6);
            --glass-border: rgba(255, 255, 255, 0.08);
            --neon-cyan: #00f3ff;
            --neon-pink: #ff00ff;
            --neon-green: #00ff9d;
            --text-main: #e0e0e0;
            --text-sub: #909090;
        }
        body {
            background: var(--bg-dark); color: var(--text-main);
            font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
            margin: 0; padding: 20px; padding-bottom: 100px; overflow-x: hidden;
        }
        #canvas-bg { position: fixed; top: 0; left: 0; width: 100%; height: 100%; z-index: -1; opacity: 0.8; pointer-events: none; }
        h1, h2, h3 { margin: 0; font-weight: 300; letter-spacing: 1px; }

        .container { max-width: 850px; margin: 0 auto; display: flex; flex-direction: column; gap: 25px; position: relative; z-index: 1; }

        /* Glassmorphism Card */
        .card {
            background: var(--glass-bg);
            backdrop-filter: blur(16px) saturate(180%);
            -webkit-backdrop-filter: blur(16px) saturate(180%);
            border: 1px solid var(--glass-border);
            border-radius: 16px;
            padding: 20px;
            box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.3);
            transition: transform 0.3s cubic-bezier(0.25, 0.8, 0.25, 1), box-shadow 0.3s;
            position: relative;
            overflow: hidden;
            animation: slideUp 0.6s ease-out forwards;
            opacity: 0;
            transform: translateY(20px);
        }

        /* Modal */
        .modal-overlay {
            position: fixed; top: 0; left: 0; width: 100%; height: 100%;
            background: rgba(0,0,0,0.8); z-index: 1000;
            display: flex; align-items: center; justify-content: center;
            opacity: 0; pointer-events: none; transition: opacity 0.3s;
        }
        .modal-overlay.show { opacity: 1; pointer-events: auto; }
        .modal {
            background: rgba(20, 20, 25, 0.95);
            border: 1px solid var(--neon-cyan); border-radius: 16px;
            padding: 25px; max-width: 400px; width: 90%;
            box-shadow: 0 0 30px rgba(0, 243, 255, 0.2);
            transform: scale(0.9); transition: transform 0.3s;
        }
        .modal-overlay.show .modal { transform: scale(1); }
        .card::before {
            content: ''; position: absolute; top: 0; left: -100%; width: 50%; height: 100%;
            background: linear-gradient(to right, transparent, rgba(255,255,255,0.05), transparent);
            transform: skewX(-25deg); transition: 0.5s; pointer-events: none;
        }
        .card:hover::before { left: 150%; transition: 0.7s; }
        .card:hover { transform: translateY(-5px) scale(1.01); box-shadow: 0 12px 40px 0 rgba(0, 0, 0, 0.5), 0 0 15px rgba(0, 243, 255, 0.1); }

        /* Offline State */
        .card.offline { filter: grayscale(1) opacity(0.6); }
        .card.offline:hover { filter: grayscale(0.8) opacity(0.8); }

        @keyframes slideUp { to { opacity: 1; transform: translateY(0); } }

        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; cursor: pointer; user-select: none; }
        .status-dot { width: 8px; height: 8px; border-radius: 50%; background: #444; margin-right: 10px; display: inline-block; box-shadow: 0 0 5px rgba(0,0,0,0.5); transition: all 0.3s; }
        .status-dot.on { background: var(--neon-green); box-shadow: 0 0 12px var(--neon-green); }
        .settings-btn { cursor: pointer; font-size: 1.2em; color: var(--text-sub); transition: 0.2s; margin-left: 15px; }
        .settings-btn:hover { color: #fff; text-shadow: 0 0 10px #fff; }

        .grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 15px; margin-bottom: 15px; }
        .grid-3 { grid-template-columns: repeat(3, 1fr); }

        .stat {
            background: rgba(255,255,255,0.03); border-radius: 10px; padding: 10px; text-align: center;
            border: 1px solid rgba(255,255,255,0.02); transition: background 0.2s;
        }
        .stat:hover { background: rgba(255,255,255,0.06); }
        .stat-label { font-size: 0.75em; color: var(--text-sub); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 4px; }
        .stat-val { font-size: 1.2em; font-weight: 600; color: var(--neon-cyan); text-shadow: 0 0 5px rgba(0, 243, 255, 0.3); }

        /* Controls */
        .controls { border-top: 1px solid var(--glass-border); padding-top: 15px; display: none; }
        .controls.open { display: block; animation: expand 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275); }
        @keyframes expand { from { opacity: 0; transform: translateY(-10px); } to { opacity: 1; transform: translateY(0); } }

        .ctrl-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; font-size: 0.9em; }

        /* Buttons */
        .btn {
            border: none; padding: 8px 16px; border-radius: 8px; font-weight: 600; cursor: pointer; color: #fff;
            transition: all 0.2s; background: rgba(255,255,255,0.1); border: 1px solid rgba(255,255,255,0.1);
        }
        .btn:hover { background: rgba(255,255,255,0.2); box-shadow: 0 0 10px rgba(255,255,255,0.2); }
        .btn:active { transform: scale(0.95); }
        .btn-primary { background: linear-gradient(45deg, rgba(0, 243, 255, 0.2), rgba(0, 255, 157, 0.2)); border-color: var(--neon-cyan); color: var(--neon-cyan); }
        .btn-primary:hover { background: linear-gradient(45deg, rgba(0, 243, 255, 0.4), rgba(0, 255, 157, 0.4)); box-shadow: 0 0 15px var(--neon-cyan); color: #fff; }

        /* Add Device Button */
        #add-device-btn {
            background: rgba(0, 255, 157, 0.15); border: 1px solid var(--neon-green); color: var(--neon-green);
            border-radius: 20px; padding: 6px 14px; display: flex; align-items: center; gap: 6px; font-size: 0.85em;
            backdrop-filter: blur(4px);
        }
        #add-device-btn:hover { background: rgba(0, 255, 157, 0.3); box-shadow: 0 0 15px var(--neon-green); color: #fff; }

        /* Add Menu */
        #add-device-menu {
            position: absolute; top: 50px; right: 0;
            background: rgba(15, 15, 20, 0.9); backdrop-filter: blur(20px);
            border: 1px solid var(--glass-border); border-radius: 12px; padding: 8px;
            display: flex; flex-direction: column; gap: 5px; min-width: 160px;
            transform-origin: top right; transition: all 0.3s cubic-bezier(0.68, -0.55, 0.27, 1.55);
            opacity: 0; transform: scale(0.8) translateY(-20px); pointer-events: none; z-index: 100;
            box-shadow: 0 10px 30px rgba(0,0,0,0.5);
        }
        #add-device-menu.show { opacity: 1; transform: scale(1) translateY(0); pointer-events: auto; }
        .menu-item {
            padding: 10px; border-radius: 8px; cursor: pointer; transition: 0.2s; font-size: 0.9em; color: #ccc;
            border: 1px solid transparent;
        }
        .menu-item:hover { background: rgba(0, 243, 255, 0.1); color: #fff; border-color: rgba(0, 243, 255, 0.3); }

        /* 3-Dot Menu */
        .card-menu-btn { cursor: pointer; padding: 0 8px; font-weight: bold; font-size: 1.4em; color: var(--text-sub); transition: color 0.2s; }
        .card-menu-btn:hover { color: #fff; }
        .card-menu {
            position: absolute; top: 45px; right: 15px;
            background: rgba(10, 10, 15, 0.95); border: 1px solid #333; border-radius: 8px;
            padding: 5px; display: none; z-index: 20; box-shadow: 0 5px 15px rgba(0,0,0,0.5);
        }
        .card-menu.show { display: block; animation: fadeIn 0.2s; }
        .menu-opt { padding: 8px 15px; cursor: pointer; font-size: 0.9em; color: #aaa; transition: 0.2s; border-radius: 4px; }
        .menu-opt:hover { background: rgba(255, 50, 50, 0.2); color: #ff5252; }

        /* Switches & Inputs */
        .switch { position: relative; display: inline-block; width: 44px; height: 24px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #333; transition: .3s; border-radius: 34px; border: 1px solid #444; }
        .slider:before { position: absolute; content: ""; height: 16px; width: 16px; left: 3px; bottom: 3px; background-color: #888; transition: .3s; border-radius: 50%; }
        input:checked + .slider { background-color: rgba(0, 255, 157, 0.2); border-color: var(--neon-green); }
        input:checked + .slider:before { transform: translateX(20px); background-color: var(--neon-green); box-shadow: 0 0 10px var(--neon-green); }

        input[type=range] { width: 100%; background: transparent; -webkit-appearance: none; margin: 10px 0; }
        input[type=range]::-webkit-slider-runnable-track { width: 100%; height: 4px; background: #333; border-radius: 2px; }
        input[type=range]::-webkit-slider-thumb {
            height: 16px; width: 16px; border-radius: 50%; background: var(--neon-cyan);
            -webkit-appearance: none; margin-top: -6px; box-shadow: 0 0 10px var(--neon-cyan);
            cursor: pointer; transition: transform 0.1s;
        }
        input[type=range]::-webkit-slider-thumb:hover { transform: scale(1.2); }

        /* Segmented Control */
        .seg-ctrl { display: flex; background: rgba(0,0,0,0.3); border-radius: 10px; padding: 3px; gap: 3px; border: 1px solid rgba(255,255,255,0.05); }
        .seg-opt {
            flex: 1; padding: 8px; text-align: center; cursor: pointer; border-radius: 8px;
            font-size: 0.85em; transition: all 0.3s ease; color: #777; position: relative; overflow: hidden;
            display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 2px;
        }
        .seg-opt:hover { color: #aaa; background: rgba(255,255,255,0.05); }
        .seg-opt.active {
            background: rgba(255,255,255,0.1); color: #fff; font-weight: 600;
            text-shadow: 0 0 8px rgba(255,255,255,0.5); box-shadow: 0 2px 10px rgba(0,0,0,0.2);
        }
        .seg-opt.active div { transform: scale(1.1); }

        /* Power Button */
        .pwr-btn-large {
            width: 70px; height: 70px; border-radius: 50%;
            background: radial-gradient(circle at 30% 30%, #2a2a2a, #1a1a1a);
            border: 2px solid #333;
            display: flex; align-items: center; justify-content: center;
            font-size: 1.8em; color: #444; cursor: pointer;
            transition: all 0.3s; margin: 0 auto;
            box-shadow: 0 5px 15px rgba(0,0,0,0.4), inset 0 2px 5px rgba(255,255,255,0.05);
        }
        .pwr-btn-large:hover { border-color: #555; color: #666; }
        .pwr-btn-large.on {
            border-color: var(--neon-cyan); color: #fff;
            background: radial-gradient(circle at 30% 30%, rgba(0, 243, 255, 0.2), rgba(0,0,0,0.8));
            box-shadow: 0 0 20px rgba(0, 243, 255, 0.4), inset 0 0 10px rgba(0, 243, 255, 0.2);
            text-shadow: 0 0 10px var(--neon-cyan);
        }
        .pwr-btn-large:active { transform: scale(0.95); }

        /* Misc */
        select {
            padding: 6px 10px; background: #222; color: #fff; border: 1px solid #444;
            border-radius: 6px; outline: none; font-family: inherit;
        }
        #console { font-family: 'Fira Code', monospace; background: rgba(0,0,0,0.5); border: 1px solid #333; padding: 10px; border-radius: 8px; height: 200px; overflow-y: auto; font-size: 0.8em; margin-top: 15px; }
        .log-line { border-bottom: 1px solid rgba(255,255,255,0.05); padding: 3px 0; }
        .log-I { color: #fff; } .log-W { color: #ffd700; } .log-E { color: #ff5252; } .log-D { color: #aaa; }

        .hidden { display: none !important; }
        .intro { text-align: center; padding: 60px 20px; animation: fadeIn 1s; }
        @keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
    </style>
</head>
<body>
    <canvas id="canvas-bg"></canvas>
    <div class="container">
        <!-- Main Header -->
        <div style="display: flex; justify-content: space-between; align-items: center; padding: 0 10px;">
            <div style="display:flex; align-items:center; gap:10px">
                <h2 style="background: linear-gradient(to right, #fff, #aaa); -webkit-background-clip: text; -webkit-text-fill-color: transparent;">⚡ Ecoflow CTRL</h2>
                <div id="global-status" class="status-dot"></div>
            </div>
            <div style="display:flex; align-items:center; gap:15px; position: relative;">
                <button id="add-device-btn" onclick="toggleAddMenu()">
                    <span>+ DEVICE</span>
                </button>
                <div id="add-device-menu">
                    <div class="menu-item" onclick="connectDevice('d3')">Delta 3</div>
                    <div class="menu-item" onclick="connectDevice('w2')">Wave 2</div>
                    <div class="menu-item" onclick="connectDevice('d3p')">Delta Pro 3</div>
                    <div class="menu-item" onclick="connectDevice('ac')">Alternator Charger</div>
                </div>
                <span id="esp-temp" style="font-size:0.85em; color:var(--text-sub)">--°C</span>
                <div class="settings-btn" onclick="openSettings()">⚙️</div>
            </div>
        </div>

        <!-- Settings Modal -->
        <div id="settings-modal" class="modal-overlay">
            <div class="modal">
                <h3 style="margin-bottom: 20px; color:#fff;">⚙️ Settings</h3>

                <div class="ctrl-row">
                    <span>Current Light Level (ADC)</span>
                    <span id="set-curr-adc" style="font-family:monospace; color:var(--neon-cyan)">...</span>
                </div>

                <hr style="border-color:var(--glass-border); margin:15px 0">

                <div class="ctrl-row"><span>Min Light (ADC): <b id="val-min-adc" style="color:var(--neon-green)">0</b></span></div>
                <input type="range" id="rg-min-adc" min="0" max="4095" step="1" oninput="el('val-min-adc').innerText=this.value">

                <div class="ctrl-row"><span>Max Light (ADC): <b id="val-max-adc" style="color:var(--neon-green)">4095</b></span></div>
                <input type="range" id="rg-max-adc" min="0" max="4095" step="1" oninput="el('val-max-adc').innerText=this.value">

                <hr style="border-color:var(--glass-border); margin:15px 0">
                <h4 style="color:#fff; margin-bottom: 10px;">Firmware Update</h4>

                <div class="ctrl-row">
                    <span>Update ESP32</span>
                    <input type="file" id="file-esp32" style="max-width: 200px;">
                </div>
                <button class="btn btn-primary" onclick="uploadFirmware('esp32')" style="width:100%">Flash ESP32</button>

                <div class="ctrl-row" style="margin-top: 15px;">
                    <span>Update STM32</span>
                    <input type="file" id="file-stm32" style="max-width: 200px;">
                </div>
                <button class="btn btn-primary" onclick="uploadFirmware('stm32')" style="width:100%">Flash STM32</button>

                <div id="ota-status" style="margin-top: 10px; font-size: 0.8em; color: var(--neon-cyan);"></div>

                <div style="display:flex; justify-content:flex-end; gap:10px; margin-top:20px;">
                    <button class="btn" onclick="closeSettings()">Cancel</button>
                    <button class="btn btn-primary" onclick="saveSettings()">Save</button>
                </div>
            </div>
        </div>

        <div id="intro-screen" class="intro hidden">
            <h3 style="color:#fff; margin-bottom: 10px;">Ready to Connect</h3>
            <p style="color:var(--text-sub);">Select a device type from the menu to begin scanning.</p>
        </div>

        <div id="device-list"></div>

    </div>

<script>
    const API = '/api';
    // ... Mock functions for testing ...
    function el(id) { return document.getElementById(id); }
    function openSettings() { el('settings-modal').classList.add('show'); }
    function closeSettings() { el('settings-modal').classList.remove('show'); }
    function uploadFirmware(type) { el('ota-status').innerText = 'Uploading ' + type + '... (Mock)'; }
    function connectDevice(type) {}
    function toggleAddMenu() {}
    function setLogConfig() {}
    function clearLogs() {}
    function sendCli() {}
    function saveSettings() {}
    function toggleLogs() {}
</script>
</body>
</html>
"""

def verify_firmware_ui(page):
    # 1. Load HTML
    page.set_content(html_template)

    # 2. Open Settings
    page.click(".settings-btn")

    # 3. Check for Firmware Update Section
    page.wait_for_selector("text=Firmware Update")

    # 4. Check for Input Files
    page.wait_for_selector("#file-esp32")
    page.wait_for_selector("#file-stm32")

    # 5. Check Buttons
    page.wait_for_selector("button:has-text('Flash ESP32')")
    page.wait_for_selector("button:has-text('Flash STM32')")

    # 6. Take Screenshot
    page.screenshot(path="verification/verification.png")

with sync_playwright() as p:
    browser = p.chromium.launch()
    page = browser.new_page()
    verify_firmware_ui(page)
    browser.close()
