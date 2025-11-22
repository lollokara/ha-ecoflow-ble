#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

#include <Arduino.h>

const char WEB_APP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Ecoflow Controller</title>
    <style>
        :root { --bg: #121212; --card: #1e1e1e; --primary: #00E676; --danger: #ff5252; --text: #e0e0e0; --sub: #a0a0a0; }
        body { background: var(--bg); color: var(--text); font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; margin: 0; padding: 20px; padding-bottom: 100px; overflow-x: hidden; }
        #canvas-bg { position: fixed; top: 0; left: 0; width: 100%; height: 100%; z-index: -1; opacity: 0.6; }
        h1, h2, h3 { margin: 0; }
        .container { max-width: 800px; margin: 0 auto; display: flex; flex-direction: column; gap: 20px; position: relative; z-index: 1; }
        .card { background: rgba(30, 30, 30, 0.85); backdrop-filter: blur(8px); border-radius: 12px; padding: 16px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); transition: transform 0.2s, box-shadow 0.2s; }
        .card:hover { transform: translateY(-2px); box-shadow: 0 8px 12px rgba(0,0,0,0.4); }
        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; cursor: pointer; transition: opacity 0.2s; }
        .header:hover { opacity: 0.8; }
        .status-dot { width: 10px; height: 10px; border-radius: 50%; background: #555; margin-right: 8px; display: inline-block; }
        .status-dot.on { background: var(--primary); box-shadow: 0 0 8px var(--primary); }
        .grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; margin-bottom: 12px; }
        .grid-3 { grid-template-columns: repeat(3, 1fr); }
        .stat { background: #2a2a2a; padding: 8px; border-radius: 8px; text-align: center; }
        .stat-label { font-size: 0.8em; color: var(--sub); }
        .stat-val { font-size: 1.1em; font-weight: bold; }

        /* Controls */
        .controls { border-top: 1px solid #333; padding-top: 12px; display: none; }
        .controls.open { display: block; animation: slideDown 0.3s ease-out; }
        @keyframes slideDown { from { opacity: 0; transform: translateY(-10px); } to { opacity: 1; transform: translateY(0); } }

        .ctrl-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
        .btn { border: none; padding: 8px 16px; border-radius: 6px; font-weight: bold; cursor: pointer; color: #fff; transition: opacity 0.2s; }
        .btn:active { opacity: 0.7; }
        .btn-primary { background: var(--primary); color: #000; }
        .btn-danger { background: var(--danger); }
        .btn-sub { background: #444; }
        .btn-icon { padding: 8px; font-size: 1.2em; }

        /* Switch */
        .switch { position: relative; display: inline-block; width: 40px; height: 24px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #444; transition: .4s; border-radius: 34px; }
        .slider:before { position: absolute; content: ""; height: 16px; width: 16px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }
        input:checked + .slider { background-color: var(--primary); }
        input:checked + .slider:before { transform: translateX(16px); }

        /* Range */
        input[type=range] { width: 100%; background: transparent; -webkit-appearance: none; margin: 10px 0; }
        input[type=range]::-webkit-slider-runnable-track { width: 100%; height: 6px; background: #444; border-radius: 3px; }
        input[type=range]::-webkit-slider-thumb { height: 20px; width: 20px; border-radius: 50%; background: var(--primary); -webkit-appearance: none; margin-top: -7px; }

        /* Dial for Wave 2 */
        .dial-container { display: flex; align-items: center; justify-content: center; gap: 20px; margin: 10px 0; }
        .dial-val { font-size: 2em; font-weight: bold; min-width: 60px; text-align: center; }

        /* Console */
        #console { font-family: monospace; background: #000; color: #0f0; padding: 10px; border-radius: 8px; height: 200px; overflow-y: auto; font-size: 0.8em; margin-top: 20px; }
        .log-line { border-bottom: 1px solid #111; padding: 2px 0; }
        .log-V { color: #888; } .log-D { color: #aaa; } .log-I { color: #fff; } .log-W { color: #ea0; } .log-E { color: #f44; }

        /* Intro */
        .intro { text-align: center; padding: 40px 0; }
        select { padding: 8px; background: #333; color: #fff; border: 1px solid #555; border-radius: 4px; font-size: 1em; }

        .hidden { display: none !important; }
        canvas.graph { width: 100%; height: 150px; background: #222; border-radius: 8px; margin-top: 10px; }
    </style>
</head>
<body>
    <canvas id="canvas-bg"></canvas>
    <div class="container">
        <div class="header">
            <h2>⚡ Ecoflow CTRL</h2>
            <div style="display:flex; align-items:center; gap:10px">
                <span id="esp-temp" style="font-size:0.8em; color:#aaa">--°C</span>
                <div id="global-status" class="status-dot"></div>
            </div>
        </div>

        <div id="intro-screen" class="intro hidden">
            <h3>No Devices Connected</h3>
            <p style="color:#aaa; margin-bottom: 20px;">Select a device to scan and connect.</p>
            <select id="dev-select">
                <option value="d3">Delta 3 (D3)</option>
                <option value="w2">Wave 2 (W2)</option>
                <option value="d3p">Delta Pro 3 (D3P)</option>
                <option value="ac">Alternator Charger (AC)</option>
            </select>
            <button class="btn btn-primary" onclick="connectDevice()">Scan & Connect</button>
        </div>

        <div id="device-list"></div>

        <div class="card">
            <div class="header" onclick="toggleLogs()">
                <h3>📋 Logs & Debug</h3>
                <button class="btn btn-sub" id="log-toggle-btn">Show</button>
            </div>
            <div id="log-section" class="hidden">
                <div class="ctrl-row">
                    <label class="switch"><input type="checkbox" id="log-enable" onchange="setLogConfig()"><span class="slider"></span></label>
                    <span>Enable Logs</span>
                    <select id="log-level" onchange="setLogConfig()" style="margin-left: auto;">
                        <option value="5">Verbose</option>
                        <option value="4">Debug</option>
                        <option value="3">Info</option>
                        <option value="2">Warn</option>
                        <option value="1">Error</option>
                    </select>
                </div>
                <div class="ctrl-row">
                    <input type="text" id="log-tag" placeholder="Tag (e.g. DeviceManager)" style="width: 100%; padding: 8px; background: #333; border:1px solid #444; color:#fff; border-radius:4px;">
                    <button class="btn btn-sub" onclick="setLogConfig()">Set Filter</button>
                </div>
                <div id="console"></div>
                <div class="ctrl-row">
                    <button class="btn btn-sub" onclick="clearLogs()">Clear</button>
                </div>
                <hr style="border-color:#333; margin: 15px 0;">
                <div class="ctrl-row">
                     <input type="text" id="cli-input" placeholder="Send raw command (e.g. sys_temp)..." style="width: 100%; padding: 8px; background: #333; border:1px solid #444; color:#fff; border-radius:4px; margin-right: 10px;" onkeydown="if(event.key==='Enter') sendCli()">
                     <button class="btn btn-primary" onclick="sendCli()">Send</button>
                </div>
            </div>
        </div>
    </div>

<script>
    const API = '/api';
    let state = {};
    let logPollInterval = null;

    function el(id) { return document.getElementById(id); }

    // --- Templates ---
    const renderD3 = (d) => `
        <div class="card">
            <div class="header" onclick="toggleCtrl('${d.type}')">
                <h3><span class="status-dot ${d.conn?'on':''}"></span>Delta 3 (${d.sn})</h3>
                <span class="stat-val">${d.batt}%</span>
            </div>
            <div class="grid grid-3">
                <div class="stat"><div class="stat-label">In</div><div class="stat-val">${d.in}W</div></div>
                <div class="stat"><div class="stat-label">Out</div><div class="stat-val">${d.out}W</div></div>
                <div class="stat"><div class="stat-label">Solar</div><div class="stat-val">${d.solar}W</div></div>
            </div>
            <div style="text-align:center; margin-bottom:10px; color:#aaa; font-size:0.8em;">Cell Temp: ${d.cell_temp}°C</div>
            <div class="controls" id="ctrl-${d.type}">
                <div class="ctrl-row">
                    <span>AC Output</span>
                    <label class="switch"><input type="checkbox" ${d.ac_on?'checked':''} onchange="cmd('${d.type}', 'set_ac', this.checked)"><span class="slider"></span></label>
                </div>
                <div class="ctrl-row">
                    <span>DC (12V)</span>
                    <label class="switch"><input type="checkbox" ${d.dc_on?'checked':''} onchange="cmd('${d.type}', 'set_dc', this.checked)"><span class="slider"></span></label>
                </div>
                <div class="ctrl-row">
                    <span>USB</span>
                    <label class="switch"><input type="checkbox" ${d.usb_on?'checked':''} onchange="cmd('${d.type}', 'set_usb', this.checked)"><span class="slider"></span></label>
                </div>
                <hr style="border-color:#333">
                <div class="ctrl-row">
                    <span>AC Charge Limit: <b id="val-ac-${d.type}">${d.cfg_ac_lim}</b>W</span>
                </div>
                <input type="range" min="200" max="2000" step="100" value="${d.cfg_ac_lim}" onchange="cmd('${d.type}', 'set_ac_lim', parseInt(this.value))" oninput="el('val-ac-${d.type}').innerText=this.value">

                <div class="ctrl-row">
                    <span>Max Charge: <b id="val-max-${d.type}">${d.cfg_max}</b>%</span>
                </div>
                <input type="range" min="50" max="100" step="1" value="${d.cfg_max}" onchange="cmd('${d.type}', 'set_max_soc', parseInt(this.value))" oninput="el('val-max-${d.type}').innerText=this.value">

                <div class="ctrl-row">
                    <button class="btn btn-danger" style="width:100%" onclick="disconnect('${d.type}')">Disconnect</button>
                </div>
            </div>
        </div>`;

    const renderW2 = (d) => `
        <div class="card">
            <div class="header" onclick="toggleCtrl('${d.type}')">
                <h3><span class="status-dot ${d.conn?'on':''}"></span>Wave 2 (${d.sn})</h3>
                <span class="stat-val">${d.batt}%</span>
            </div>
            <div class="grid">
                <div class="stat"><div class="stat-label">Amb Temp</div><div class="stat-val">${d.amb_temp}°C</div></div>
                <div class="stat"><div class="stat-label">Out Temp</div><div class="stat-val">${d.out_temp}°C</div></div>
            </div>
            <div class="controls" id="ctrl-${d.type}">
                <div class="ctrl-row" style="justify-content:center">
                    <span style="font-size:0.9em; color:#aaa; margin-right:10px">Set Temp</span>
                </div>
                <div class="dial-container">
                    <button class="btn btn-sub btn-icon" onclick="cmd('${d.type}', 'set_temp', ${d.set_temp - 1})">-</button>
                    <div class="dial-val">${d.set_temp}°C</div>
                    <button class="btn btn-sub btn-icon" onclick="cmd('${d.type}', 'set_temp', ${d.set_temp + 1})">+</button>
                </div>

                <div class="ctrl-row">
                    <span>Power</span>
                    <label class="switch"><input type="checkbox" ${d.pwr?'checked':''} onchange="cmd('${d.type}', 'set_power', this.checked)"><span class="slider"></span></label>
                </div>
                <div class="ctrl-row">
                    <span>Mode</span>
                    <select onchange="cmd('${d.type}', 'set_mode', parseInt(this.value))" style="width:100px">
                        <option value="0" ${d.mode==0?'selected':''}>Cooling</option>
                        <option value="1" ${d.mode==1?'selected':''}>Heating</option>
                        <option value="2" ${d.mode==2?'selected':''}>Fan</option>
                    </select>
                </div>
                <div class="ctrl-row">
                    <span>Fan Speed</span>
                    <select onchange="cmd('${d.type}', 'set_fan', parseInt(this.value))" style="width:100px">
                        <option value="0" ${d.fan==0?'selected':''}>Low</option>
                        <option value="1" ${d.fan==1?'selected':''}>Med</option>
                        <option value="2" ${d.fan==2?'selected':''}>High</option>
                    </select>
                </div>
                 <div class="ctrl-row">
                    <span>Auto Drain</span>
                    <label class="switch"><input type="checkbox" ${d.drain?'checked':''} onchange="cmd('${d.type}', 'set_drain', this.checked)"><span class="slider"></span></label>
                </div>

                <div style="margin-top:15px">
                    <span style="font-size:0.8em; color:#aaa">Temp History (1h)</span>
                    <canvas id="graph-w2" class="graph" width="300" height="150"></canvas>
                </div>

                <div class="ctrl-row" style="margin-top:15px">
                    <button class="btn btn-danger" style="width:100%" onclick="disconnect('${d.type}')">Disconnect</button>
                </div>
            </div>
        </div>`;

    const renderD3P = (d) => `
        <div class="card">
            <div class="header" onclick="toggleCtrl('${d.type}')">
                <h3><span class="status-dot ${d.conn?'on':''}"></span>Delta Pro 3 (${d.sn})</h3>
                <span class="stat-val">${d.batt}%</span>
            </div>
            <div style="text-align:center; margin-bottom:10px; color:#aaa; font-size:0.8em;">Cell Temp: ${d.cell_temp}°C</div>
            <div class="controls" id="ctrl-${d.type}">
                <div class="ctrl-row">
                    <span>AC Low Volt</span>
                    <label class="switch"><input type="checkbox" ${d.ac_lv_on?'checked':''} onchange="cmd('${d.type}', 'set_ac_lv', this.checked)"><span class="slider"></span></label>
                </div>
                <div class="ctrl-row">
                    <span>AC High Volt</span>
                    <label class="switch"><input type="checkbox" ${d.ac_hv_on?'checked':''} onchange="cmd('${d.type}', 'set_ac_hv', this.checked)"><span class="slider"></span></label>
                </div>
                <div class="ctrl-row">
                    <span>Energy Backup</span>
                    <label class="switch"><input type="checkbox" ${d.backup_en?'checked':''} onchange="cmd('${d.type}', 'set_backup_en', this.checked)"><span class="slider"></span></label>
                </div>
                <input type="range" min="0" max="100" value="${d.backup_lvl}" onchange="cmd('${d.type}', 'set_backup_level', parseInt(this.value))">
                <div class="ctrl-row">
                    <button class="btn btn-danger" style="width:100%" onclick="disconnect('${d.type}')">Disconnect</button>
                </div>
            </div>
        </div>`;

    const renderAC = (d) => `
        <div class="card">
            <div class="header" onclick="toggleCtrl('${d.type}')">
                <h3><span class="status-dot ${d.conn?'on':''}"></span>Alt Charger (${d.sn})</h3>
                <span class="stat-val">${d.batt}%</span>
            </div>
            <div class="grid">
                <div class="stat"><div class="stat-label">Car Batt</div><div class="stat-val">${d.car_volt.toFixed(1)}V</div></div>
                <div class="stat"><div class="stat-label">Limit</div><div class="stat-val">${d.pow_lim}W</div></div>
            </div>
            <div class="controls" id="ctrl-${d.type}">
                <div class="ctrl-row">
                    <span>Charger Enabled</span>
                    <label class="switch"><input type="checkbox" ${d.chg_open?'checked':''} onchange="cmd('${d.type}', 'set_open', this.checked)"><span class="slider"></span></label>
                </div>
                <div class="ctrl-row">
                    <span>Mode</span>
                    <select onchange="cmd('${d.type}', 'set_mode', parseInt(this.value))" style="width:120px">
                        <option value="0" ${d.mode==0?'selected':''}>Idle</option>
                        <option value="1" ${d.mode==1?'selected':''}>Charge</option>
                        <option value="2" ${d.mode==2?'selected':''}>Maintenance</option>
                        <option value="3" ${d.mode==3?'selected':''}>Reverse</option>
                    </select>
                </div>
                <div class="ctrl-row">
                    <span>Power Limit: <b id="val-ac-lim">${d.pow_lim}</b>W</span>
                </div>
                <input type="range" min="100" max="800" step="50" value="${d.pow_lim}" onchange="cmd('${d.type}', 'set_limit', parseInt(this.value))" oninput="el('val-ac-lim').innerText=this.value">

                <div class="ctrl-row">
                    <button class="btn btn-danger" style="width:100%" onclick="disconnect('${d.type}')">Disconnect</button>
                </div>
            </div>
        </div>`;

    const renderCommon = (d) => `
        <div class="card">
             <div class="header" onclick="toggleCtrl('${d.type}')">
                <h3><span class="status-dot ${d.conn?'on':''}"></span>${d.name} (${d.sn})</h3>
                <span class="stat-val">${d.batt}%</span>
            </div>
            <div class="controls" id="ctrl-${d.type}">
                 <div class="ctrl-row">
                    <button class="btn btn-danger" style="width:100%" onclick="disconnect('${d.type}')">Disconnect</button>
                </div>
            </div>
        </div>`;

    // --- Logic ---

    function update() {
        fetch(API + '/status').then(r => r.json()).then(data => {
            // Update ESP Temp
            if(data.esp_temp) el('esp-temp').innerText = data.esp_temp.toFixed(1) + '°C';

            const list = el('device-list');
            let html = '';
            let anyConnected = false;

            const types = ['d3', 'w2', 'd3p', 'ac'];
            types.forEach(type => {
                const d = data[type];
                if (d) { // Ensure device data exists in response
                    d.type = type;
                    if (d.connected) {
                        anyConnected = true;
                        const wasOpen = el('ctrl-'+type)?.classList.contains('open');

                        if (type === 'd3') html += renderD3(d);
                        else if (type === 'w2') html += renderW2(d);
                        else if (type === 'd3p') html += renderD3P(d);
                        else if (type === 'ac') html += renderAC(d);
                        else html += renderCommon(d);

                        setTimeout(() => {
                           const panel = el('ctrl-'+type);
                           if(wasOpen && panel) {
                               panel.classList.add('open');
                               if(type === 'w2') loadGraph();
                           }
                        }, 0);
                    }
                }
            });

            list.innerHTML = html;
            if (anyConnected) el('intro-screen').classList.add('hidden');
            else el('intro-screen').classList.remove('hidden');

            el('global-status').classList.toggle('on', anyConnected);
        });
    }

    function toggleCtrl(id) {
        const e = el('ctrl-'+id);
        e.classList.toggle('open');
        if(id === 'w2' && e.classList.contains('open')) loadGraph();
    }

    function loadGraph() {
        const canvas = el('graph-w2');
        if(!canvas) return;
        fetch(API + '/history?type=w2').then(r => r.json()).then(data => {
            const ctx = canvas.getContext('2d');
            const w = canvas.width;
            const h = canvas.height;
            ctx.clearRect(0,0,w,h);

            if(!data || data.length === 0) {
                ctx.fillStyle = '#555';
                ctx.fillText('No Data', w/2-20, h/2);
                return;
            }

            ctx.strokeStyle = '#00E676';
            ctx.lineWidth = 2;
            ctx.beginPath();

            const min = Math.min(...data) - 2;
            const max = Math.max(...data) + 2;
            const range = max - min || 1;

            data.forEach((val, i) => {
                const x = (i / (data.length - 1)) * w;
                const y = h - ((val - min) / range) * h;
                if(i===0) ctx.moveTo(x,y);
                else ctx.lineTo(x,y);
            });
            ctx.stroke();

            // Current value dot
            const lastY = h - ((data[data.length-1] - min) / range) * h;
            ctx.fillStyle = '#fff';
            ctx.beginPath();
            ctx.arc(w-4, lastY, 3, 0, Math.PI*2);
            ctx.fill();
        });
    }

    function connectDevice() {
        const type = el('dev-select').value;
        fetch(API + '/connect', { method: 'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({type}) })
        .then(()=> alert('Scanning started...'));
    }

    function disconnect(type) {
        if(!confirm('Disconnect device?')) return;
        fetch(API + '/disconnect', { method: 'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({type}) });
    }

    function cmd(type, cmdName, val) {
        fetch(API + '/control', {
            method: 'POST',
            headers:{'Content-Type':'application/json'},
            body:JSON.stringify({type, cmd: cmdName, val: val})
        });
    }

    // --- Logs ---
    function toggleLogs() {
        el('log-section').classList.toggle('hidden');
        if (!el('log-section').classList.contains('hidden')) {
            startLogPoll();
        } else {
            stopLogPoll();
        }
    }

    function startLogPoll() {
        if (logPollInterval) return;
        logPollInterval = setInterval(() => {
            fetch(API + '/logs').then(r => r.json()).then(logs => {
                const c = el('console');
                logs.forEach(l => {
                    const d = new Date(l.ts);
                    const time = d.getHours()+':'+d.getMinutes()+':'+d.getSeconds();
                    const line = document.createElement('div');
                    line.className = 'log-line log-' + (['?','E','W','I','D','V'][l.lvl] || 'I');
                    line.innerText = `[${time}] ${l.tag}: ${l.msg}`;
                    c.appendChild(line);
                });
                if(logs.length > 0) c.scrollTop = c.scrollHeight;
            });
        }, 1000);
    }

    function stopLogPoll() {
        clearInterval(logPollInterval);
        logPollInterval = null;
    }

    function setLogConfig() {
        const enabled = el('log-enable').checked;
        const level = parseInt(el('log-level').value);
        const tag = el('log-tag').value;

        fetch(API + '/log_config', {
             method: 'POST',
             headers:{'Content-Type':'application/json'},
             body:JSON.stringify({enable: enabled, level, tag})
        });
    }

    function clearLogs() {
        el('console').innerHTML = '';
    }

    function sendCli() {
        const cmd = el('cli-input').value;
        if(!cmd) return;
        fetch(API + '/raw_command', {
            method: 'POST',
            headers:{'Content-Type':'application/json'},
            body:JSON.stringify({cmd})
        }).then(r => {
            if(r.ok) {
                el('cli-input').value = '';
                const c = el('console');
                const line = document.createElement('div');
                line.className = 'log-line log-I';
                line.innerText = `> ${cmd}`;
                c.appendChild(line);
                c.scrollTop = c.scrollHeight;
            } else {
                alert('Command failed');
            }
        });
    }

    setInterval(update, 1000);
    update(); // Initial call

    // --- Particle Background ---
    const canvas = document.getElementById('canvas-bg');
    const ctx = canvas.getContext('2d');
    let particles = [];
    let w, h;

    function resize() {
        w = canvas.width = window.innerWidth;
        h = canvas.height = window.innerHeight;
        initParticles();
    }

    function initParticles() {
        particles = [];
        const cnt = Math.floor(w * h / 15000); // density
        for(let i=0; i<cnt; i++) {
            particles.push({
                x: Math.random()*w, y: Math.random()*h,
                vx: (Math.random()-0.5)*0.5, vy: (Math.random()-0.5)*0.5
            });
        }
    }

    function draw() {
        ctx.clearRect(0,0,w,h);
        ctx.fillStyle = 'rgba(255,255,255,0.5)';
        ctx.strokeStyle = 'rgba(255,255,255,0.05)';

        for(let i=0; i<particles.length; i++) {
            let p = particles[i];
            p.x += p.vx; p.y += p.vy;
            if(p.x < 0 || p.x > w) p.vx *= -1;
            if(p.y < 0 || p.y > h) p.vy *= -1;

            ctx.beginPath();
            ctx.arc(p.x, p.y, 1.5, 0, Math.PI*2);
            ctx.fill();

            // Connect
            for(let j=i+1; j<particles.length; j++) {
                let p2 = particles[j];
                let dx = p.x - p2.x, dy = p.y - p2.y;
                let dist = dx*dx + dy*dy;
                if(dist < 15000) {
                    ctx.beginPath();
                    ctx.moveTo(p.x, p.y);
                    ctx.lineTo(p2.x, p2.y);
                    ctx.stroke();
                }
            }
        }
        requestAnimationFrame(draw);
    }

    window.addEventListener('resize', resize);
    resize();
    draw();
</script>
</body>
</html>
)rawliteral";

#endif // WEB_ASSETS_H
