import re
import os
import time

# Extract HTML from WebAssets.h
with open("EcoflowESP32/src/WebAssets.h", "r") as f:
    content = f.read()

match = re.search(r'R"rawliteral\((.*)\)rawliteral";', content, re.DOTALL)
if match:
    html = match.group(1)
    with open("verification/index.html", "w") as f:
        f.write(html)
else:
    print("Could not extract HTML")
    exit(1)

from playwright.sync_api import sync_playwright

with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    page = browser.new_page()
    page.goto(f"file://{os.getcwd()}/verification/index.html")

    # Force open settings modal
    print("Opening Settings Modal...")
    page.evaluate("document.getElementById('settings-modal').classList.add('show')")

    # Wait for visibility of new elements
    try:
        page.wait_for_selector("#fw-esp", state="visible", timeout=2000)
        page.wait_for_selector("#fw-stm", state="visible", timeout=2000)
        page.wait_for_selector("#btn-up-esp", state="visible", timeout=2000)
        page.wait_for_selector("#btn-up-stm", state="visible", timeout=2000)
        print("Elements found!")
    except Exception as e:
        print(f"Error finding elements: {e}")
        page.screenshot(path="verification/error.png")
        exit(1)

    page.screenshot(path="verification/verification.png")
    browser.close()
    print("Success")

if os.path.exists("verification/index.html"):
    os.remove("verification/index.html")
