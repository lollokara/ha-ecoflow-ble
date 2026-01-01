from playwright.sync_api import sync_playwright
import re

# Extract HTML from C++ header
with open('EcoflowESP32/src/WebAssets.h', 'r') as f:
    content = f.read()
    # Find the raw literal content
    match = re.search(r'R"rawliteral\((.*?)\)rawliteral";', content, re.DOTALL)
    if match:
        html_content = match.group(1)
        with open('verification/temp_app.html', 'w') as out:
            out.write(html_content)
    else:
        print("Failed to extract HTML")
        exit(1)

def verify_ota_ui(page):
    page.goto(f"file://{os.getcwd()}/verification/temp_app.html")

    # Mock API fetch for settings
    page.route('**/api/settings', lambda route: route.fulfill(
        status=200,
        content_type='application/json',
        body='{"min": 500, "max": 4000}'
    ))

    # Open Settings Modal
    page.click('.settings-btn')

    # Wait for animation
    page.wait_for_timeout(500)

    # Verify Firmware Section exists
    expect(page.get_by_text("📡 Firmware Update")).to_be_visible()
    expect(page.get_by_text("ESP32 Firmware")).to_be_visible()
    expect(page.get_by_text("STM32 Firmware")).to_be_visible()

    # Take screenshot of Settings Modal with Firmware section
    page.screenshot(path="verification/ota_ui.png")

import os
from playwright.sync_api import expect

if __name__ == "__main__":
    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page()
        try:
            verify_ota_ui(page)
            print("Verification script ran successfully.")
        except Exception as e:
            print(f"Verification failed: {e}")
        finally:
            browser.close()
