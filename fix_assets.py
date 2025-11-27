import re

path = 'EcoflowESP32/src/WebAssets.h'

with open(path, 'r') as f:
    content = f.read()

# Extract HTML content (Non-greedy match!)
match = re.search(r'R"rawliteral\((.*?)\)rawliteral";', content, re.DOTALL)
if not match:
    print("Could not find rawliteral content")
    exit(1)

html = match.group(1)

# Apply fixes for undefined values
# D3
html = html.replace('${d.in}W', '${d.in || 0}W')
html = html.replace('${d.out}W', '${d.out || 0}W')
html = html.replace('${d.solar}W', '${d.solar || 0}W')
html = html.replace('${d.cell_temp}', '${d.cell_temp || 0}')
html = html.replace('${d.ac_out_pow}', '${d.ac_out_pow || 0}')
html = html.replace('${d.dc_out_pow}', '${d.dc_out_pow || 0}')
html = html.replace('${d.usb_out_pow}', '${d.usb_out_pow || 0}')
html = html.replace('${d.cfg_ac_lim}', '${d.cfg_ac_lim || 400}')
html = html.replace('value="${d.cfg_ac_lim}"', 'value="${d.cfg_ac_lim || 400}"')
html = html.replace('${d.cfg_max}', '${d.cfg_max || 100}')
html = html.replace('value="${d.cfg_max}"', 'value="${d.cfg_max || 100}"')
html = html.replace('${d.cfg_min}', '${d.cfg_min || 0}')
html = html.replace('value="${d.cfg_min}"', 'value="${d.cfg_min || 0}"')

# W2
html = html.replace('${d.amb_temp}', '${d.amb_temp || 0}')
html = html.replace('${d.out_temp}', '${d.out_temp || 0}')
html = html.replace('${d.set_temp}', '${d.set_temp || 20}')
html = html.replace('value="${d.pow_lim}"', 'value="${d.pow_lim || 100}"')
html = html.replace('${d.pow_lim}', '${d.pow_lim || 100}')

# AC
html = html.replace('${d.car_volt.toFixed(1)}', '${(d.car_volt || 0).toFixed(1)}')
html = html.replace('${d.dc_curr}', '${d.dc_curr || 0}')
html = html.replace('${(d.dc_curr) >', '${(d.dc_curr||0) >')
html = html.replace('${(d.dc_curr) <', '${(d.dc_curr||0) <')

# Reconstruct file
new_content = """#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

#include <Arduino.h>

const char WEB_APP_HTML[] PROGMEM = R"rawliteral(
""" + html + """)rawliteral";

#endif // WEB_ASSETS_H
"""

with open(path, 'w') as f:
    f.write(new_content)

print("Fixed WebAssets.h")
