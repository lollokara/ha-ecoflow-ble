from playwright.sync_api import sync_playwright
import os

# We need to read WebAssets.h and extract the HTML to a file for Playwright to load.
# Since we can't run the ESP32 server, we will load the HTML directly and mock the API.

def extract_html():
    with open('EcoflowESP32/src/WebAssets.h', 'r') as f:
        content = f.read()

    # Extract string between R"rawliteral( and )rawliteral";
    start = content.find('R"rawliteral(') + len('R"rawliteral(')
    end = content.find(')rawliteral";')
    html = content[start:end]

    # We need to mock the JS fetch behavior since we are running locally.
    # We'll inject a script to override window.fetch

    mock_script = """
    <script>
    window.fetch = async (url, options) => {
        console.log('Mock Fetch:', url);
        if (url.includes('/api/status')) {
            return {
                json: async () => ({
                    esp_temp: 42.5,
                    d3: { connected: true, sn: 'D3-123', name: 'Delta 3', batt: 85, in: 100, out: 200, solar: 50, cell_temp: 25, ac_on: true, dc_on: false, usb_on: true, ac_out_pow: 150, dc_out_pow: 0, usb_out_pow: 50, cfg_ac_lim: 800, cfg_max: 100, cfg_min: 0 },
                    w2: { connected: true, sn: 'W2-456', name: 'Wave 2', batt: 60, amb_temp: 24, out_temp: 18, set_temp: 20, mode: 0, sub_mode: 2, fan: 1, pwr: true, drain: false, light: true, beep: false, pwr_bat: -100, pwr_mppt: 0, pwr_psdr: 0 },
                    d3p: { connected: false },
                    ac: { connected: false }
                })
            };
        }
        if (url.includes('/api/history')) {
            return { json: async () => [20, 21, 20, 19, 18, 18, 18] };
        }
        if (url.includes('/api/control')) {
            const body = JSON.parse(options.body);
            console.log('Control:', body);
            // For testing, we can't easily update state without a real backend loop,
            // but we can verify the click happened.
            return { ok: true };
        }
        return { ok: true, json: async () => ({}) };
    };
    </script>
    """

    # Insert mock script before </head>
    html = html.replace('</head>', mock_script + '</head>')

    with open('temp_ui.html', 'w') as f:
        f.write(html)

def verify_ui():
    extract_html()

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        page.goto('file://' + os.getcwd() + '/temp_ui.html')

        # Wait for updates to render
        page.wait_for_timeout(2000)

        # 1. Verify "Add Device" button exists
        add_btn = page.locator('#add-device-btn')
        if add_btn.is_visible():
            print("Add Device button visible")
            add_btn.click()
            page.wait_for_timeout(500) # Wait for animation
            page.screenshot(path='screenshot_add_menu.png')
        else:
            print("Add Device button NOT visible")

        # 2. Verify Wave 2 UI elements
        # Check Power Button
        pwr_btn = page.locator('#pwr-btn-w2')
        if pwr_btn.is_visible():
            print("Wave 2 Power Button visible")

        # Check Sub Mode (Should be visible because Mode is 0/Cool)
        sub_mode = page.locator('#sub-mode-row-w2')
        if sub_mode.is_visible():
            print("Wave 2 Sub Mode visible (Correct)")
        else:
            print("Wave 2 Sub Mode HIDDEN (Incorrect)")

        # Check Icons in Sub Mode
        # We expect 4 options: Max, Norm, Eco, Night
        # IDs: sub-0-w2, sub-3-w2, sub-2-w2, sub-1-w2
        if page.locator('#sub-0-w2').is_visible():
            print("Sub Mode 'Max' option visible")

        # 3. Verify Delta 3 Power Splits
        ac_pow = page.locator('#ac-pow-d3')
        if ac_pow.is_visible() and ac_pow.inner_text() == '150':
            print("Delta 3 AC Power Split visible and correct")

        # Take final screenshot
        page.screenshot(path='screenshot_final.png')

        browser.close()

if __name__ == "__main__":
    verify_ui()
