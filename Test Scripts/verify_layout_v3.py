from playwright.sync_api import sync_playwright
import os

def extract_html():
    with open('EcoflowESP32/src/WebAssets.h', 'r') as f:
        content = f.read()

    start = content.find('R"rawliteral(') + len('R"rawliteral(')
    end = content.find(')rawliteral";')
    html = content[start:end]

    # Mock API
    mock_script = """
    <script>
    window.fetch = async (url, options) => {
        console.log('Mock Fetch:', url);
        if (url.includes('/api/status')) {
            return {
                json: async () => ({
                    esp_temp: 42.0,
                    d3: { connected: true, sn: 'D3-TEST', name: 'Delta 3', batt: 80, cfg_ac_lim: 600, ac_out_pow: 0, dc_out_pow: 0, usb_out_pow: 0 },
                    w2: { connected: true, sn: 'W2-TEST', name: 'Wave 2', batt: 50, mode: 0, powerMode: 2, set_temp: 22, pwr: false }, // Mocking OFF state logic (backend sends pwr: false if mode=2)
                    d3p: { connected: false },
                    ac: { connected: false }
                })
            };
        }
        return { ok: true, json: async () => ({}) };
    };
    </script>
    """

    html = html.replace('</head>', mock_script + '</head>')

    with open('temp_ui_verify_layout.html', 'w') as f:
        f.write(html)

def verify_layout():
    extract_html()

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        page.goto('file://' + os.getcwd() + '/temp_ui_verify_layout.html')
        page.wait_for_timeout(1500)

        # 1. Verify Delta 3 Limits
        # Find input with id rg-ac-d3
        rg_ac = page.locator('#rg-ac-d3')
        if rg_ac.is_visible():
            min_val = rg_ac.get_attribute('min')
            max_val = rg_ac.get_attribute('max')
            print(f"Delta 3 Range: min={min_val}, max={max_val}")
            if min_val == '400' and max_val == '1500':
                print("PASS: Delta 3 AC limits correct.")
            else:
                print("FAIL: Delta 3 AC limits incorrect.")

        # 2. Verify Wave 2 Layout
        # We expect a flex container with power btn and dial container
        # Locator: #ctrl-w2 > div (first child) should have display: flex
        # Hard to verify CSS value computed in headless easily without eval, but we can check structure.

        # We expect Power Button (pwr-btn-w2) and Dial Container (.dial-container) to be siblings in the same parent div
        pwr_btn = page.locator('#pwr-btn-w2')
        dial_cont = page.locator('#ctrl-w2 .dial-container')

        # Get parent of power button
        parent = pwr_btn.locator('..').locator('..') # pwr-btn is in a div (text-align:center), that div is in the flex container

        # Actually layout is:
        # <div style="display: flex... gap: 30px">
        #    <div style="text-align:center"> <div id="pwr-btn-w2">...</div> ... </div>  (Left)
        #    <div class="dial-container"> ... </div> (Right)
        # </div>

        # Let's check if they are visible
        if pwr_btn.is_visible() and dial_cont.is_visible():
            print("PASS: Wave 2 Power and Dial are visible.")
            # Visual check via screenshot
            page.screenshot(path='screenshot_w2_layout.png')
            print("Screenshot taken: screenshot_w2_layout.png")

        browser.close()

if __name__ == "__main__":
    verify_layout()
