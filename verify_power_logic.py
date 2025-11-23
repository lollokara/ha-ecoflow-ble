from playwright.sync_api import sync_playwright
import os

def extract_html():
    with open('EcoflowESP32/src/WebAssets.h', 'r') as f:
        content = f.read()
    start = content.find('R"rawliteral(') + len('R"rawliteral(')
    end = content.find(')rawliteral";')
    html = content[start:end]

    # Mock API with state tracking
    mock_script = """
    <script>
    // State mock
    window.mockState = {
        w2_powerMode: 2 // Start OFF (2)
    };

    window.fetch = async (url, options) => {
        console.log('Mock Fetch:', url);

        if (url.includes('/api/status')) {
            // Return current state
            const pwr = (window.mockState.w2_powerMode === 1);
            return {
                json: async () => ({
                    esp_temp: 42.0,
                    d3: { connected: false },
                    w2: { connected: true, sn: 'W2-MOCK', name: 'Wave 2', batt: 50, mode: 0, powerMode: window.mockState.w2_powerMode, set_temp: 22, pwr: pwr },
                    d3p: { connected: false },
                    ac: { connected: false }
                })
            };
        }

        if (url.includes('/api/control')) {
             const body = JSON.parse(options.body);
             console.log('Control:', body);
             if (body.cmd === 'set_power') {
                 // Backend logic simulation: val true -> 1, val false -> 2
                 window.mockState.w2_powerMode = body.val ? 1 : 2;
             }
             return { ok: true };
        }
        return { ok: true, json: async () => ({}) };
    };
    </script>
    """

    html = html.replace('</head>', mock_script + '</head>')

    with open('temp_ui_power_logic.html', 'w') as f:
        f.write(html)

def verify_power_logic():
    extract_html()

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        page.goto('file://' + os.getcwd() + '/temp_ui_power_logic.html')

        # 1. Initial State: OFF (2)
        # Wait for poll
        page.wait_for_timeout(1500)

        btn = page.locator('#pwr-btn-w2')
        # Should NOT have class 'on'
        classes = btn.get_attribute('class')
        print(f"Initial classes: {classes}")
        if 'on' in classes:
             print("FAIL: Button has 'on' class but mock state is OFF (2)")
        else:
             print("PASS: Button is visually OFF")

        # 2. Click to Turn ON
        print("Clicking Power Button (expecting turn ON)...")
        btn.click()

        # Wait for poll cycle (logic update)
        page.wait_for_timeout(1500)

        # Check state
        classes = btn.get_attribute('class')
        print(f"Classes after click: {classes}")
        if 'on' in classes:
             print("PASS: Button is visually ON")
        else:
             print("FAIL: Button did not turn ON visually")

        # 3. Click to Turn OFF
        print("Clicking Power Button (expecting turn OFF)...")
        btn.click()
        page.wait_for_timeout(1500)

        classes = btn.get_attribute('class')
        print(f"Classes after 2nd click: {classes}")
        if 'on' not in classes:
             print("PASS: Button is visually OFF again")
        else:
             print("FAIL: Button did not turn OFF")

        browser.close()

if __name__ == "__main__":
    verify_power_logic()
