import re

with open("EcoflowESP32/src/mr521.pb.h", "r") as f:
    lines = f.readlines()

in_struct = False
fields = []
current_field = None

for line in lines:
    line = line.strip()
    if "typedef struct _mr521_DisplayPropertyUpload {" in line:
        in_struct = True
        continue
    if in_struct:
        if "} mr521_DisplayPropertyUpload;" in line:
            break

        # Look for has_ fields
        m_has = re.match(r"bool has_(\w+);", line)
        if m_has:
            current_field = m_has.group(1)
            continue

        # Look for value fields
        if current_field:
            # Matches: type name;
            # e.g. uint32_t errcode;
            # e.g. float pow_in_sum_w;
            m_val = re.match(r"(\w+)\s+(\w+);", line)
            if m_val:
                f_type = m_val.group(1)
                f_name = m_val.group(2)
                if f_name == current_field:
                    fields.append((f_name, f_type))
                    current_field = None

print("static void logFullDeltaPro3Data(const mr521_DisplayPropertyUpload& msg) {")
print('    ESP_LOGI(TAG, "--- Full D3P Dump ---");')
for name, ftype in fields:
    fmt = ""
    val = "msg." + name
    if ftype == "float":
        fmt = "%.2f"
    elif ftype in ["uint32_t", "int32_t", "bool", "mr521_AC_IN_CHG_MODE", "mr521_SP_CHARGER_CHG_MODE", "mr521_SERVE_MIDDLEMEN", "mr521_INSTALLMENT_PAYMENT_OVERDUE_LIMIT", "mr521_INSTALLMENT_PAYMENT_STATE"]:
        fmt = "%d"
        val = "(int)" + val
    else:
        continue # Skip structs/callbacks for now

    print(f'    if (msg.has_{name}) ESP_LOGI(TAG, "{name}: {fmt}", {val});')
print("}")
