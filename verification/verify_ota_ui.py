from playwright.sync_api import sync_playwright
import os

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load local HTML file
        page.goto(f"file://{os.getcwd()}/verification/index.html")

        # Click settings button to open modal
        page.click(".settings-btn")

        # Wait for modal to appear
        page.wait_for_selector("#settings-modal.show")

        # Take screenshot of the modal with Firmware Update section
        page.screenshot(path="verification/ota_ui.png")

        browser.close()

if __name__ == "__main__":
    run()
