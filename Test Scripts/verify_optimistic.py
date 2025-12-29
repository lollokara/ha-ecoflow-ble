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
                    d3: { connected: false },
                    w2: { connected: true, sn: 'W2-TEST', name: 'Wave 2', batt: 50, mode: 0, powerMode: 1, set_temp: 22, pwr: true },
                    d3p: { connected: false },
                    ac: { connected: false }
                })
            };
        }
        if (url.includes('/api/control')) {
             // Slow response to verify optimistic update
             await new Promise(r => setTimeout(r, 500));
             return { ok: true };
        }
        return { ok: true, json: async () => ({}) };
    };
    </script>
    """

    html = html.replace('</head>', mock_script + '</head>')

    with open('temp_ui_optimistic.html', 'w') as f:
        f.write(html)

def verify_optimistic():
    extract_html()

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        page.goto('file://' + os.getcwd() + '/temp_ui_optimistic.html')
        page.wait_for_timeout(2000)

        # 1. Check Layout
        # Vertical layout means the dial-container should have flex-direction: column
        # We can check bounding box of elements.
        # + button should be above value, value above - button.

        dial_val = page.locator('#dial-w2')
        plus_btn = page.locator('.dial-container button').nth(0) # First button is +
        minus_btn = page.locator('.dial-container button').nth(1) # Second button is -

        # Get text to confirm
        if plus_btn.inner_text() == '+':
            print("First button is +")
        else:
            print(f"FAIL: First button is {plus_btn.inner_text()}")

        box_plus = plus_btn.bounding_box()
        box_val = dial_val.bounding_box()
        box_minus = minus_btn.bounding_box()

        if box_plus['y'] < box_val['y'] and box_val['y'] < box_minus['y']:
             print("PASS: Layout is Vertical (Plus above Value above Minus)")
        else:
             print(f"FAIL: Layout positions: Plus Y={box_plus['y']}, Val Y={box_val['y']}, Minus Y={box_minus['y']}")

        # 2. Verify Optimistic Update
        # Initial value is 22 (from mock)
        # Click +
        print("Clicking + button...")
        plus_btn.click()

        # Immediately check value (before fetch would return if it was real)
        # Since mock fetch delays 500ms, and we check immediately, update must be local.
        new_val = dial_val.inner_text()
        print(f"Value after click: {new_val}")

        if new_val == '23':
            print("PASS: Optimistic update worked (22 -> 23)")
        else:
            print(f"FAIL: Value did not update immediately. Got {new_val}")

        page.screenshot(path='screenshot_optimistic.png')
        browser.close()

if __name__ == "__main__":
    verify_optimistic()
