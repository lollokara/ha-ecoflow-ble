#include "EcoflowProtocol.h"
#include <Arduino.h>
#include <cstring>
#include <vector>
#include "esp_log.h"

static const char* TAG = "EcoflowProtocol";

// Helper to print byte arrays for debugging
static void print_hex_protocol(const uint8_t* data, size_t size, const char* label) {
    if (size == 0) return;
    char hex_str[size * 3 + 1];
    for (size_t i = 0; i < size; i++) {
        sprintf(hex_str + i * 3, "%02x ", data[i]);
    }
    hex_str[size * 3] = '\0';
    ESP_LOGV(TAG, "%s: %s", label, hex_str);
}

const uint8_t Packet::PREFIX;
const uint16_t EncPacket::PREFIX;

static uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

// Packet implementation
uint32_t Packet::g_seq = 0;

Packet::Packet(uint8_t src, uint8_t dest, uint8_t cmdSet, uint8_t cmdId, const std::vector<uint8_t>& payload, uint8_t check_type, uint8_t encrypted, uint8_t version, uint32_t seq, uint8_t product_id) :
    _src(src), _dest(dest), _cmdSet(cmdSet), _cmdId(cmdId), _payload(payload), _check_type(check_type), _encrypted(encrypted), _version(version), _seq(seq), _product_id(product_id) {
    if (_seq == 0) {
        _seq = g_seq++;
    }
}

// CRC8 implementation for packet header validation
static uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

Packet* Packet::fromBytes(const uint8_t* data, size_t len, bool is_xor) {
    if (len < 18 || data[0] != PREFIX) { // Minimum length reduced for V2
        return nullptr;
    }

    uint8_t version = data[1];
    uint16_t payload_len = data[2] | (data[3] << 8);

    if (version == 3 || version == 19) { // Allow version 19 as V3 (Delta 3 quirks?)
        if (crc16(data, len - 2) != (data[len - 2] | (data[len - 1] << 8))) {
            ESP_LOGE(TAG, "Packet CRC16 mismatch");
            return nullptr;
        }
    }

    if (crc8(data, 4) != data[4]) {
        ESP_LOGE(TAG, "Packet header CRC8 mismatch");
        return nullptr;
    }

    uint8_t product_id = data[5];
    uint32_t seq = data[6] | (data[7] << 8) | (data[8] << 16) | (data[9] << 24);
    uint8_t src = data[12];
    uint8_t dest = data[13];

    uint8_t dsrc = 0;
    uint8_t ddest = 0;
    uint8_t cmd_set = 0;
    uint8_t cmd_id = 0;
    size_t payload_offset = 0;

    if (version == 3 || version == 19) {
        dsrc = data[14];
        ddest = data[15];
        cmd_set = data[16];
        cmd_id = data[17];
        payload_offset = 18;
    } else if (version == 2) {
        // V2: No dsrc/ddst fields
        cmd_set = data[14];
        cmd_id = data[15];
        payload_offset = 16;
    } else {
        ESP_LOGE(TAG, "Unsupported packet version %d", version);
        print_hex_protocol(data, len, "Unknown Version Packet");
        return nullptr;
    }

    std::vector<uint8_t> payload;
    if (payload_len > 0) {
        payload.assign(data + payload_offset, data + payload_offset + payload_len);
        if (is_xor && data[6] != 0) {
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] ^= data[6];
            }
        }

        // Fix for Delta 3 / Protocol V3 version 19 packets
        // Python implementation: if version == 19 and payload[-2:] == b"\xbb\xbb": payload = payload[:-2]
        if (version == 19 && payload.size() >= 2) {
            if (payload[payload.size() - 2] == 0xBB && payload[payload.size() - 1] == 0xBB) {
                payload.resize(payload.size() - 2);
            }
        }
    }

    return new Packet(src, dest, cmd_set, cmd_id, payload, dsrc, ddest, version, seq, product_id);
}

std::vector<uint8_t> Packet::toBytes() const {
    std::vector<uint8_t> bytes;
    bytes.push_back(PREFIX);
    bytes.push_back(_version);
    bytes.push_back(_payload.size() & 0xFF);
    bytes.push_back((_payload.size() >> 8) & 0xFF);
    bytes.push_back(crc8(bytes.data(), bytes.size()));

    bytes.push_back(_product_id);
    bytes.push_back(_seq & 0xFF);
    bytes.push_back((_seq >> 8) & 0xFF);
    bytes.push_back((_seq >> 16) & 0xFF);
    bytes.push_back((_seq >> 24) & 0xFF);
    bytes.push_back(0); // static zeroes
    bytes.push_back(0);
    bytes.push_back(_src);
    bytes.push_back(_dest);

    if (_version == 3) {
        bytes.push_back(_check_type); // dsrc
        bytes.push_back(_encrypted); // ddest
    }

    bytes.push_back(_cmdSet);
    bytes.push_back(_cmdId);
    bytes.insert(bytes.end(), _payload.begin(), _payload.end());

    // CRC calculation must include the entire packet except the CRC bytes themselves
    uint16_t crc = crc16(bytes.data(), bytes.size());
    bytes.push_back(crc & 0xFF);
    bytes.push_back((crc >> 8) & 0xFF);

    print_hex_protocol(bytes.data(), bytes.size(), "Serialized Packet");
    return bytes;
}

// EncPacket implementation
EncPacket::EncPacket(uint8_t frame_type, uint8_t payload_type, const std::vector<uint8_t>& payload,
                     uint8_t needs_ack, uint8_t is_ack)
    : _frame_type(frame_type), _payload_type(payload_type), _payload(payload), _needs_ack(needs_ack), _is_ack(is_ack) {}

std::vector<uint8_t> EncPacket::toBytes(EcoflowCrypto* crypto, bool isBlade) const {
    std::vector<uint8_t> encrypted_payload = _payload;
    if (crypto) {
        // PKCS7 padding
        int padding = 16 - (_payload.size() % 16);
        std::vector<uint8_t> padded_payload = _payload;
        for (int i = 0; i < padding; ++i) {
            padded_payload.push_back(padding);
        }
        encrypted_payload.resize(padded_payload.size());
        crypto->encrypt_session(padded_payload.data(), padded_payload.size(), encrypted_payload.data());
        print_hex_protocol(encrypted_payload.data(), encrypted_payload.size(), "Encrypted Payload");
    }

    std::vector<uint8_t> packet_data;
    // Use correct prefix based on device type
    uint16_t prefix = isBlade ? PREFIX_BLADE : PREFIX;
    packet_data.push_back(prefix & 0xFF);
    packet_data.push_back((prefix >> 8) & 0xFF);
    packet_data.push_back((_frame_type << 4));
    packet_data.push_back(0x01); // Hardcoded based on Python implementation
    uint16_t len = encrypted_payload.size() + 2; // payload + 2 bytes for CRC
    packet_data.push_back(len & 0xFF);
    packet_data.push_back((len >> 8) & 0xFF);

    print_hex_protocol(packet_data.data(), packet_data.size(), "Packet Header");

    packet_data.insert(packet_data.end(), encrypted_payload.begin(), encrypted_payload.end());

    uint16_t crc = crc16(packet_data.data(), packet_data.size());
    packet_data.push_back(crc & 0xFF);
    packet_data.push_back((crc >> 8) & 0xFF);

    print_hex_protocol(packet_data.data(), packet_data.size(), "Full EncPacket");
    return packet_data;
}

std::vector<Packet> EncPacket::parsePackets(const uint8_t* data, size_t len, EcoflowCrypto& crypto, std::vector<uint8_t>& rxBuffer, bool isAuthenticated) {
    std::vector<Packet> packets;

    ESP_LOGD(TAG, "parsePackets: Received %d bytes", len);
    // print_hex_protocol(data, len, "Received Data"); // Too verbose for high throughput

    rxBuffer.insert(rxBuffer.end(), data, data + len);

    while (rxBuffer.size() >= 2) {
        // Search for prefix 0x5A5A or 0x02AA (Blade)
        bool prefix_found = false;
        size_t prefix_idx = 0;
        uint16_t found_prefix = 0;

        for (size_t i = 0; i < rxBuffer.size() - 1; i++) {
            uint16_t current_prefix = rxBuffer[i] | (rxBuffer[i+1] << 8);
            if (current_prefix == PREFIX || current_prefix == PREFIX_BLADE) {
                prefix_idx = i;
                found_prefix = current_prefix;
                prefix_found = true;
                ESP_LOGI(TAG, "parsePackets: Found %s prefix at index %d",
                        (current_prefix == PREFIX_BLADE) ? "BLADE" : "STANDARD", i);
                break;
            }
        }

        if (!prefix_found) {
            // Check if the last bytes could be start of either prefix
            if (rxBuffer.size() >= 1) {
                uint8_t last_byte = rxBuffer.back();
                // Keep if it's the start of either PREFIX (0x5A) or PREFIX_BLADE (0xAA)
                if (last_byte == (PREFIX & 0xFF) || last_byte == (PREFIX_BLADE & 0xFF)) {
                    rxBuffer.clear();
                    rxBuffer.push_back(last_byte);
                } else {
                    rxBuffer.clear();
                }
            }
            break;
        }

        // Remove garbage before prefix
        if (prefix_idx > 0) {
            ESP_LOGW(TAG, "Discarding %d garbage bytes", prefix_idx);
            rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + prefix_idx);
        }

        // Now rxBuffer[0-1] contains the found prefix (either 0x5A5A or 0x02AA)
        if (rxBuffer.size() < 6) {
            break; // Wait for header
        }

        uint16_t frame_len = rxBuffer[4] | (rxBuffer[5] << 8);
        size_t total_len = 6 + frame_len;

        ESP_LOGI(TAG, "parsePackets: Found prefix=0x%04X, frame_len=%d, total_len=%d, buffer_size=%d",
                found_prefix, frame_len, total_len, rxBuffer.size());

        // Check for Blade V2 Packet (AA 02)
        if (found_prefix == PREFIX_BLADE) {
             // For V2 Packets, length is at offset 2 (bytes 2-3)
             // Header size is 16 bytes (incl length) + 2 CRC = 18 bytes + payload_len
             if (rxBuffer.size() < 4) {
                 break; // Need length
             }
             uint16_t payload_len = rxBuffer[2] | (rxBuffer[3] << 8);
             total_len = 18 + payload_len;

             if (rxBuffer.size() < total_len) {
                 ESP_LOGI(TAG, "parsePackets: Incomplete Blade packet - need %d bytes but have %d", total_len, rxBuffer.size());
                 break;
             }

             // No decryption needed for V2 packets on Blade
             std::vector<uint8_t> packet_data(rxBuffer.begin(), rxBuffer.begin() + total_len);

             Packet* packet = Packet::fromBytes(packet_data.data(), packet_data.size(), false); // No XOR
             if (packet) {
                packets.push_back(*packet);
                delete packet;
             } else {
                 ESP_LOGW(TAG, "parsePackets: Failed to parse Blade V2 packet");
             }

             rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + total_len);
             continue;
        }

        if (rxBuffer.size() < total_len) {
            ESP_LOGI(TAG, "parsePackets: Incomplete packet - need %d bytes but have %d", total_len, rxBuffer.size());
            break; // Wait for full packet
        }

        // Validate Header (Sanity check on frame length)
        if (frame_len < 2 || frame_len > 1024) { // 1024 is arbitrary sanity limit
             ESP_LOGW(TAG, "Invalid frame length %d, discarding prefix", frame_len);
             rxBuffer.erase(rxBuffer.begin()); // Advance by 1 to search for next 0x5A5A
             continue;
        }

        // CRC validation
        uint16_t crc_from_packet = rxBuffer[total_len - 2] | (rxBuffer[total_len - 1] << 8);
        if (crc_from_packet != crc16(rxBuffer.data(), total_len - 2)) {
            ESP_LOGW(TAG, "CRC Mismatch, discarding prefix");
            rxBuffer.erase(rxBuffer.begin()); // Advance by 1
            continue;
        }

        // Valid packet found
        size_t payload_len = frame_len - 2;
        std::vector<uint8_t> encrypted_payload(rxBuffer.begin() + 6, rxBuffer.begin() + 6 + payload_len);

        std::vector<uint8_t> decrypted_payload(encrypted_payload.size());

        // Standard EcoFlow devices use AES encryption
        crypto.decrypt_session(encrypted_payload.data(), encrypted_payload.size(), decrypted_payload.data());

        // Remove PKCS7 padding for standard devices
        if (!decrypted_payload.empty()) {
            uint8_t padding = decrypted_payload.back();
            if (padding > 0 && padding <= 16 && decrypted_payload.size() >= padding) {
                decrypted_payload.resize(decrypted_payload.size() - padding);
            }
        }

        // Standard EcoFlow packet parsing
        Packet* packet = Packet::fromBytes(decrypted_payload.data(), decrypted_payload.size(), isAuthenticated);

        if (packet) {
            ESP_LOGI(TAG, "parsePackets: Successfully parsed packet - Src:0x%02X Dest:0x%02X CmdSet:0x%02X CmdId:0x%02X PayloadLen:%d",
                    packet->getSrc(), packet->getDest(), packet->getCmdSet(), packet->getCmdId(), packet->getPayload().size());
            packets.push_back(*packet);
            delete packet;
        } else {
            ESP_LOGW(TAG, "parsePackets: Failed to parse decrypted packet");
            print_hex_protocol(decrypted_payload.data(), decrypted_payload.size(), "Failed Packet Data");
        }

        rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + total_len);
    }
    return packets;
}


std::vector<uint8_t> EncPacket::parseSimple(const uint8_t* data, size_t len) {
    ESP_LOGD(TAG, "parseSimple: Received %d bytes", len);
    print_hex_protocol(data, len, "Simple Data");
    if (len < 8) {
        ESP_LOGE(TAG, "parseSimple: Data too short");
        return {};
    }

    // Show first 16 bytes for analysis
    ESP_LOGI(TAG, "parseSimple: First 16 bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
             len > 8 ? data[8] : 0, len > 9 ? data[9] : 0, len > 10 ? data[10] : 0, len > 11 ? data[11] : 0,
             len > 12 ? data[12] : 0, len > 13 ? data[13] : 0, len > 14 ? data[14] : 0, len > 15 ? data[15] : 0);

    uint16_t received_prefix = data[0] | (data[1] << 8);
    ESP_LOGI(TAG, "parseSimple: Received prefix: 0x%04X, Expected: 0x%04X or 0x%04X", received_prefix, PREFIX, PREFIX_BLADE);

     if (received_prefix != PREFIX && received_prefix != PREFIX_BLADE) {
        ESP_LOGE(TAG, "parseSimple: Invalid prefix - received 0x%04X, expected 0x%04X or 0x%04X",
                 received_prefix, PREFIX, PREFIX_BLADE);
        ESP_LOGI(TAG, "parseSimple: First 16 bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                 data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
                 len > 8 ? data[8] : 0, len > 9 ? data[9] : 0, len > 10 ? data[10] : 0, len > 11 ? data[11] : 0,
                 len > 12 ? data[12] : 0, len > 13 ? data[13] : 0, len > 14 ? data[14] : 0, len > 15 ? data[15] : 0);
        return {};
    }

    uint16_t len_from_packet = data[4] | (data[5] << 8);
    size_t frame_end = 6 + len_from_packet;

    ESP_LOGI(TAG, "parseSimple: Frame length from packet: %d, calculated frame end: %d, received data length: %d",
             len_from_packet, frame_end, len);

    if (len < frame_end) {
        ESP_LOGW(TAG, "parseSimple: Incomplete packet - need %d bytes but have %d", frame_end, len);
        return {};
    }
    if (len_from_packet < 2) {
        ESP_LOGW(TAG, "parseSimple: Invalid frame length %d (too short)", len_from_packet);
        return {};
    }

    size_t payload_size = len_from_packet - 2;
    std::vector<uint8_t> for_crc;
    for_crc.insert(for_crc.end(), data, data + 6 + payload_size);
    uint16_t calculated_crc = crc16(for_crc.data(), for_crc.size());
    uint16_t received_crc = data[frame_end - 2] | (data[frame_end - 1] << 8);

    ESP_LOGI(TAG, "parseSimple: Payload size: %d, calculated CRC: 0x%04X, received CRC: 0x%04X",
             payload_size, calculated_crc, received_crc);

    if (calculated_crc != received_crc) {
        ESP_LOGW(TAG, "parseSimple: CRC mismatch - calculated:0x%04X received:0x%04X", calculated_crc, received_crc);
        return {};
    }

    ESP_LOGI(TAG, "parseSimple: Successfully parsed %s packet with %d payload bytes",
            (received_prefix == PREFIX_BLADE) ? "BLADE" : "STANDARD", payload_size);
    return std::vector<uint8_t>(data + 6, data + 6 + payload_size);
}
