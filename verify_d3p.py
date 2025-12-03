import os
from playwright.sync_api import sync_playwright

html_content = open("temp_ui_d3p.html").read()

# Mock the fetch function
mock_script = """
<script>
window.originalFetch = window.fetch;
window.fetch = function(url, options) {
    if (url.includes('/api/status')) {
        return Promise.resolve({
            json: () => Promise.resolve({
                "esp_temp": 45.0,
                "light_adc": 2048,
                "d3p": {
                    "connected": true,
                    "sn": "MR51XXXX",
                    "name": "D3P",
                    "paired": true,
                    "batt": 85,
                    "in": 500,
                    "out": 200,
                    "ac_in": 500,
                    "ac_lv_out": 100,
                    "ac_hv_out": 100,
                    "dc_out": 0,
                    "solar_lv": 250,
                    "solar_hv": 250,
                    "ac_hv_on": true,
                    "ac_lv_on": true,
                    "dc_on": false,
                    "backup_en": true,
                    "backup_lvl": 20,
                    "cell_temp": 30,
                    "gfi_isle": true,
                    "cfg_ac_lim": 1500,
                    "cfg_max": 90,
                    "cfg_min": 10
                },
                "d3": {"connected": false},
                "w2": {"connected": false},
                "ac": {"connected": false}
            })
        });
    }
    if (url.includes('/api/history')) {
        return Promise.resolve({
            json: () => Promise.resolve([])
        });
    }
    if (url.includes('/api/settings')) {
         return Promise.resolve({
            json: () => Promise.resolve({min:0, max:4095})
        });
    }
    return Promise.reject("Mocked fetch: " + url);
};
</script>
"""

# Inject before closing body
final_html = html_content.replace("</body>", mock_script + "</body>")
with open("temp_ui_mocked.html", "w") as f:
    f.write(final_html)

with sync_playwright() as p:
    browser = p.chromium.launch()
    page = browser.new_page()
    page.goto(f"file://{os.path.abspath('temp_ui_mocked.html')}")
    page.wait_for_timeout(2000) # Wait for update() to run
    # Expand D3P controls
    # page.click("#card-d3p .header")
    page.wait_for_timeout(1000)
    page.screenshot(path="screenshot_d3p.png", full_page=True)
    browser.close()
