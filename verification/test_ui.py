from playwright.sync_api import sync_playwright

def test_web_assets():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the HTML content directly (simulating the web server)
        # Since I can't compile/run the C++ code to serve it, I'll extract the HTML string
        # from WebAssets.h and save it to a temp file, or just use a placeholder if I can't extract it easily.
        # But wait, I have the content of WebAssets.h. I can extract the string.

        # I will read WebAssets.h first to get the content.
        with open("EcoflowESP32/src/WebAssets.h", "r") as f:
            content = f.read()

        # Extract the raw string literal. It starts after R"rawliteral( and ends before )rawliteral";
        start_marker = 'R"rawliteral('
        end_marker = ')rawliteral";'

        start_idx = content.find(start_marker)
        end_idx = content.find(end_marker)

        if start_idx != -1 and end_idx != -1:
            html_content = content[start_idx + len(start_marker):end_idx]

            # Save to a temporary file
            with open("verification/web_ui.html", "w") as f:
                f.write(html_content)

            import os
            abs_path = os.path.abspath("verification/web_ui.html")
            page.goto(f"file://{abs_path}")

            # Verify elements exist
            # 1. Check for "Ecoflow CTRL" title
            assert "Ecoflow CTRL" in page.title()

            # 2. Check for System Logs section
            assert page.get_by_text("System Logs").is_visible()

            # 3. Check for SD Card Logs section
            assert page.get_by_text("SD Card Logs").is_visible()

            # 4. Take a screenshot
            page.screenshot(path="verification/web_ui.png", full_page=True)
            print("Screenshot saved to verification/web_ui.png")

        else:
            print("Failed to extract HTML from WebAssets.h")

if __name__ == "__main__":
    test_web_assets()
