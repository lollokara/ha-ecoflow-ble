import re
import os
from playwright.sync_api import sync_playwright

# 1. Extract HTML
header_path = "BrainTransplant/BrainTransplantESP32/src/WebAssets.h"
with open(header_path, "r") as f:
    content = f.read()

match = re.search(r'R"rawliteral\((.*)\)rawliteral";', content, re.DOTALL)
if not match:
    print("Could not find HTML in header file")
    exit(1)

html_content = match.group(1)

# 2. Inject Mock JS
mock_script = """
<script>
    // Mock Fetch
    window.originalFetch = window.fetch;
    window.fetch = async (url, options) => {
        console.log("Mock Fetch:", url);
        if (url.includes('/api/status')) {
            return {
                json: async () => ({
                    esp_temp: 42.0,
                    light_adc: 2048
                })
            };
        }
        if (url.includes('/api/update/status')) {
             return {
                json: async () => ({
                    msg: "Idle",
                    state: 0,
                    progress: 0
                })
             };
        }
        if (url.includes('/api/logs')) {
             return {
                json: async () => ([
                    {ts: Date.now(), tag: "System", msg: "BrainTransplant Ready", lvl: 3},
                    {ts: Date.now(), tag: "OTA", msg: "Waiting for firmware...", lvl: 3}
                ])
             };
        }
        return { ok: true, json: async () => ({}) };
    };
</script>
"""

# Insert mock script before </head>
html_content = html_content.replace("</head>", mock_script + "</head>")

html_path = os.path.abspath("verification/index.html")
with open(html_path, "w") as f:
    f.write(html_content)

print(f"HTML saved to {html_path}")

# 3. Playwright
with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    page = browser.new_page()
    page.goto(f"file://{html_path}")

    # Wait for title
    print(page.title())

    # Check for specific elements
    page.wait_for_selector("h2") # BrainTransplant

    # Take screenshot
    screenshot_path = os.path.abspath("verification/braintransplant_ui.png")
    page.screenshot(path=screenshot_path)
    print(f"Screenshot saved to {screenshot_path}")

    browser.close()
