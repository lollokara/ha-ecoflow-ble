#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

#include <Arduino.h>

const char WEB_APP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BrainTransplant</title>
    <link rel="icon" href="data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 font-size=%2290%22>🧠</text></svg>">
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

        .container { max-width: 600px; margin: 0 auto; display: flex; flex-direction: column; gap: 25px; position: relative; z-index: 1; }

        .card {
            background: var(--glass-bg);
            backdrop-filter: blur(16px) saturate(180%);
            -webkit-backdrop-filter: blur(16px) saturate(180%);
            border: 1px solid var(--glass-border);
            border-radius: 16px;
            padding: 30px;
            box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.3);
            transition: transform 0.3s cubic-bezier(0.25, 0.8, 0.25, 1), box-shadow 0.3s;
            position: relative;
            overflow: hidden;
            animation: slideUp 0.6s ease-out forwards;
        }
        @keyframes slideUp { from { opacity: 0; transform: translateY(20px); } to { opacity: 1; transform: translateY(0); } }

        .ctrl-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; font-size: 1em; }

        .btn {
            border: none; padding: 12px 20px; border-radius: 8px; font-weight: 600; cursor: pointer; color: #fff;
            transition: all 0.2s; background: linear-gradient(45deg, rgba(0, 243, 255, 0.2), rgba(0, 255, 157, 0.2));
            border: 1px solid var(--neon-cyan); color: var(--neon-cyan); width: 100%;
        }
        .btn:hover { background: linear-gradient(45deg, rgba(0, 243, 255, 0.4), rgba(0, 255, 157, 0.4)); box-shadow: 0 0 15px var(--neon-cyan); color: #fff; }
        .btn:disabled { opacity: 0.5; cursor: not-allowed; }

        input[type=file] {
            padding: 10px; background: rgba(0,0,0,0.3); border: 1px solid #444; border-radius: 6px; color: #ccc; width: 100%;
        }

        #ota-status {
            text-align: center; margin-top: 15px; font-weight: bold; min-height: 20px;
        }
        .status-ok { color: var(--neon-green); }
        .status-err { color: var(--neon-pink); }
        .status-prog { color: var(--neon-cyan); }

        .header-title {
            text-align: center; margin-bottom: 20px;
            background: linear-gradient(to right, #fff, #aaa); -webkit-background-clip: text; -webkit-text-fill-color: transparent;
        }
    </style>
</head>
<body>
    <canvas id="canvas-bg"></canvas>
    <div class="container">
        <h2 class="header-title">🧠 BrainTransplant</h2>

        <div class="card">
            <h3 style="color:#fff; margin-bottom: 20px; border-bottom: 1px solid var(--glass-border); padding-bottom: 10px;">Firmware Update</h3>

            <div style="margin-bottom: 25px;">
                <label style="display:block; margin-bottom:8px; color:var(--text-sub)">Update ESP32 (Controller)</label>
                <div class="ctrl-row" style="flex-direction:column; gap:10px;">
                    <input type="file" id="file-esp32">
                    <button class="btn" onclick="uploadFirmware('esp32')">Flash ESP32</button>
                </div>
            </div>

            <div>
                <label style="display:block; margin-bottom:8px; color:var(--text-sub)">Update STM32 (Target)</label>
                <div class="ctrl-row" style="flex-direction:column; gap:10px;">
                    <input type="file" id="file-stm32">
                    <button class="btn" onclick="uploadFirmware('stm32')">Flash STM32</button>
                </div>
            </div>

            <div id="ota-status"></div>
        </div>
    </div>

<script>
    const API = '/api';
    let isOtaRunning = false;

    function el(id) { return document.getElementById(id); }

    function updateStatus() {
        if(!isOtaRunning) return;
        fetch(API + '/update/status').then(r=>r.json()).then(s => {
            const div = el('ota-status');
            div.innerText = s.msg + (s.progress > 0 ? ` (${s.progress}%)` : '');
            div.className = s.state === 4 ? 'status-err' : (s.state === 3 ? 'status-ok' : 'status-prog');

            if(s.state === 0 || s.state === 3 || s.state === 4) isOtaRunning = false;
        });
    }

    function uploadFirmware(type) {
        const fileInput = el('file-' + type);
        if (!fileInput.files.length) { alert('Please select a file'); return; }
        const file = fileInput.files[0];

        const fd = new FormData();
        fd.append('file', file);

        el('ota-status').innerText = 'Uploading ' + type + '...';
        el('ota-status').className = 'status-prog';
        isOtaRunning = true;

        const xhr = new XMLHttpRequest();
        xhr.open('POST', API + '/update/' + type, true);

        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) {
                const percent = Math.round((e.loaded / e.total) * 100);
                el('ota-status').innerText = 'Uploading: ' + percent + '%';
            }
        };

        xhr.onload = function() {
            if (xhr.status === 200) {
                el('ota-status').innerText = 'Upload complete. Processing...';
                // Continue polling for STM32 flash progress or ESP reboot
            } else {
                el('ota-status').innerText = 'Upload failed: ' + xhr.responseText;
                el('ota-status').className = 'status-err';
                isOtaRunning = false;
            }
        };

        xhr.send(fd);
    }

    setInterval(updateStatus, 1000);

    // --- Cyberpunk Particles ---
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
        const cnt = Math.floor(w * h / 20000);
        for(let i=0; i<cnt; i++) {
            particles.push({
                x: Math.random()*w, y: Math.random()*h,
                vx: (Math.random()-0.5)*0.5, vy: (Math.random()-0.5)*0.5,
                size: Math.random() * 2 + 1,
                color: Math.random() > 0.5 ? 'rgba(0, 243, 255, ' : 'rgba(0, 255, 157, '
            });
        }
    }

    function draw() {
        ctx.clearRect(0,0,w,h);
        for(let i=0; i<particles.length; i++) {
            let p = particles[i];
            p.x += p.vx; p.y += p.vy;
            if(p.x < 0 || p.x > w) p.vx *= -1;
            if(p.y < 0 || p.y > h) p.vy *= -1;
            ctx.beginPath();
            ctx.arc(p.x, p.y, p.size, 0, Math.PI*2);
            ctx.fillStyle = p.color + '0.3)';
            ctx.fill();
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
