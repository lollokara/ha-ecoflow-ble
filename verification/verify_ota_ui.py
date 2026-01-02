from playwright.sync_api import sync_playwright
import re

# Read the C++ header file containing the HTML
with open("EcoflowESP32/src/WebAssets.h", "r") as f:
    content = f.read()

# Extract the HTML content between R"rawliteral( and )rawliteral";
match = re.search(r'R"rawliteral\((.*)\)rawliteral";', content, re.DOTALL)
if match:
    html_content = match.group(1)
    # Save extracted HTML to a temporary file
    with open("verification/temp_index.html", "w") as f:
        f.write(html_content)
else:
    print("Could not extract HTML content")
    exit(1)

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the local HTML file
        import os
        page.goto("file://" + os.path.abspath("verification/temp_index.html"))

        # Wait for page load
        page.wait_for_timeout(500)

        # Mock fetch API to simulate opening settings
        page.evaluate("""
            window.fetch = async (url) => {
                if (url && url.includes && url.includes('/api/settings')) {
                    return {
                        json: async () => ({ min: 100, max: 4000 }),
                        ok: true
                    };
                }
                return { ok: true, json: async () => ({}) };
            };
        """)

        # Click the Settings button
        page.click(".settings-btn")

        # Wait for modal animation
        page.wait_for_timeout(500)

        # Take screenshot
        page.screenshot(path="verification/settings_ota.png")
        browser.close()

if __name__ == "__main__":
    run()
