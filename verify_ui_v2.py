from playwright.sync_api import sync_playwright
import os

def extract_html():
    with open('EcoflowESP32/src/WebAssets.h', 'r') as f:
        content = f.read()

    start = content.find('R"rawliteral(') + len('R"rawliteral(')
    end = content.find(')rawliteral";')
    html = content[start:end]

    # Mock for Cyberpunk/Offline test
    mock_script = """
    <script>
    window.fetch = async (url, options) => {
        console.log('Mock Fetch:', url);
        if (url.includes('/api/status')) {
            return {
                json: async () => ({
                    esp_temp: 45.0,
                    d3: { connected: true, sn: 'D3-ACTIVE', name: 'Delta 3', batt: 85, in: 100, out: 200, solar: 50, cell_temp: 25, ac_on: true, dc_on: false, usb_on: true, ac_out_pow: 150, dc_out_pow: 0, usb_out_pow: 50, cfg_ac_lim: 800, cfg_max: 100, cfg_min: 0 },
                    // W2 is PAIRED but DISCONNECTED (Offline test)
                    w2: { connected: false, paired: true, sn: 'W2-OFFLINE', name: 'Wave 2', batt: 0 },
                    d3p: { connected: false },
                    ac: { connected: false }
                })
            };
        }
        if (url.includes('/api/forget')) {
            console.log('Forgot device');
            return { ok: true };
        }
        return { ok: true, json: async () => ({}) };
    };
    </script>
    """

    html = html.replace('</head>', mock_script + '</head>')

    with open('temp_ui_cyber.html', 'w') as f:
        f.write(html)

def verify_ui():
    extract_html()

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        page.goto('file://' + os.getcwd() + '/temp_ui_cyber.html')
        page.wait_for_timeout(2000) # Wait for animations

        # 1. Check if Active Device is visible and styled
        d3_card = page.locator('#card-d3')
        if d3_card.is_visible():
            print("Active D3 card visible")
            # We can't easily check backdrop-filter in headless, but we can check class presence
            if 'offline' not in d3_card.get_attribute('class'):
                print("Active D3 card is NOT offline (Correct)")

        # 2. Check if Offline Device is visible (Persistence test)
        w2_card = page.locator('#card-w2')
        if w2_card.is_visible():
            print("Offline W2 card visible (Persistence works)")
            if 'offline' in w2_card.get_attribute('class'):
                print("Offline W2 card has 'offline' class (Visuals Correct)")

        # 3. Check Forget Menu
        # Click 3-dot on W2
        w2_menu_btn = w2_card.locator('.card-menu-btn')
        w2_menu_btn.click()
        page.wait_for_timeout(500)

        forget_opt = page.locator('#menu-w2 .menu-opt', has_text='Forget')
        if forget_opt.is_visible():
            print("Forget option visible for offline device")
            forget_opt.click()
            # In real app this calls API. In mock we verify call?
            # We just check UI update if mock handled it (mock deletes from knownDevices? No, mock is stateless mostly)
            # But JS logic deletes card on success.
            page.wait_for_timeout(500)
            if not w2_card.is_visible():
                print("Offline card removed after Forget (Logic Correct)")
            else:
                # Our mock fetch returns OK, JS should remove it from DOM.
                # knownDevices.delete is in the .then() block.
                print("Offline card removal verification: Card still visible? " + str(w2_card.is_visible()))

        page.screenshot(path='screenshot_cyberpunk.png')
        browser.close()

if __name__ == "__main__":
    verify_ui()
