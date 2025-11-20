#include "EcoflowProtocol.h"
#include <Arduino.h>
#include <cstring>
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
    ESP_LOGD(TAG, "%s: %s", label, hex_str);
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
Packet::Packet(uint8_t src, uint8_t dest, uint8_t cmdSet, uint8_t cmdId, const std::vector<uint8_t>& payload, uint8_t check_type, uint8_t encrypted, uint8_t version, uint16_t seq) :
    _src(src), _dest(dest), _cmdSet(cmdSet), _cmdId(cmdId), _payload(payload), _check_type(check_type), _encrypted(encrypted), _version(version), _seq(seq) {
    if (_seq == 0) {
        static uint16_t g_seq = 0;
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

Packet* Packet::fromBytes(const uint8_t* data, size_t len) {
    if (len < 20 || data[0] != PREFIX) {
        return nullptr;
    }

    uint8_t version = data[1];
    uint16_t payload_len = data[2] | (data[3] << 8);

    if (version == 3) {
        if (crc16(data, len - 2) != (data[len - 2] | (data[len - 1] << 8))) {
            ESP_LOGE(TAG, "Packet CRC16 mismatch");
            return nullptr;
        }
    }

    if (crc8(data, 4) != data[4]) {
        ESP_LOGE(TAG, "Packet header CRC8 mismatch");
        return nullptr;
    }

    uint16_t seq = data[6] | (data[7] << 8);
    uint8_t src = data[12];
    uint8_t dest = data[13];
    uint8_t dsrc = data[14];
    uint8_t ddest = data[15];
    uint8_t cmd_set = data[16];
    uint8_t cmd_id = data[17];

    std::vector<uint8_t> payload;
    if (payload_len > 0) {
        payload.assign(data + 18, data + 18 + payload_len);
    }

    return new Packet(src, dest, cmd_set, cmd_id, payload, dsrc, ddest, version, seq);
}

std::vector<uint8_t> Packet::toBytes() const {
    std::vector<uint8_t> bytes;
    bytes.push_back(PREFIX);
    bytes.push_back(_version);
    bytes.push_back(_payload.size() & 0xFF);
    bytes.push_back((_payload.size() >> 8) & 0xFF);
    bytes.push_back(crc8(bytes.data(), bytes.size()));

    // product_id, assuming 0x0d as per python
    bytes.push_back(0x0d);
    bytes.push_back(_seq & 0xFF);
    bytes.push_back((_seq >> 8) & 0xFF);
    bytes.push_back(0); // seq upper bytes, assuming 0
    bytes.push_back(0);

    bytes.push_back(0); // static zeroes
    bytes.push_back(0);
    bytes.push_back(_src);
    bytes.push_back(_dest);
    bytes.push_back(_check_type); // dsrc
    bytes.push_back(_encrypted); // ddest
    bytes.push_back(_cmdSet);
    bytes.push_back(_cmdId);
    bytes.insert(bytes.end(), _payload.begin(), _payload.end());
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

std::vector<uint8_t> EncPacket::toBytes(EcoflowCrypto* crypto) const {
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
    packet_data.push_back(PREFIX & 0xFF);
    packet_data.push_back((PREFIX >> 8) & 0xFF);
    packet_data.push_back((_payload_type << 4) | _frame_type);
    packet_data.push_back((_is_ack << 7) | (_needs_ack << 6));
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

std::vector<Packet> EncPacket::parsePackets(const uint8_t* data, size_t len, EcoflowCrypto& crypto) {
    std::vector<Packet> packets;
    static std::vector<uint8_t> buffer;

    ESP_LOGD(TAG, "parsePackets: Received %d bytes", len);
    print_hex_protocol(data, len, "Received Data");

    buffer.insert(buffer.end(), data, data + len);

    while (buffer.size() >= 8) { // Minimum size of an EncPacket
        if (buffer[0] != (PREFIX & 0xFF) || buffer[1] != ((PREFIX >> 8) & 0xFF)) {
            ESP_LOGE(TAG, "parsePackets: Invalid prefix, discarding byte");
            buffer.erase(buffer.begin());
            continue;
        }

        uint16_t frame_len = buffer[4] | (buffer[5] << 8);
        size_t total_len = 6 + frame_len;
        if (buffer.size() < total_len) {
            break;
        }

        if (frame_len < 2) { // Must include 2 bytes for CRC
            buffer.erase(buffer.begin());
            continue;
        }

        uint16_t crc_from_packet = buffer[total_len - 2] | (buffer[total_len - 1] << 8);
        if (crc_from_packet != crc16(buffer.data(), total_len - 2)) {
            buffer.erase(buffer.begin());
            continue;
        }

        size_t payload_len = frame_len - 2;
        std::vector<uint8_t> encrypted_payload(buffer.begin() + 6, buffer.begin() + 6 + payload_len);

        std::vector<uint8_t> decrypted_payload(encrypted_payload.size());
        crypto.decrypt_session(encrypted_payload.data(), encrypted_payload.size(), decrypted_payload.data());

        // Remove PKCS7 padding
        if (!decrypted_payload.empty()) {
            uint8_t padding = decrypted_payload.back();
            if (padding > 0 && padding <= 16 && decrypted_payload.size() >= padding) {
                decrypted_payload.resize(decrypted_payload.size() - padding);
            }
        }

        Packet* packet = Packet::fromBytes(decrypted_payload.data(), decrypted_payload.size());
        if (packet) {
            packets.push_back(*packet);
            delete packet;
        }

        buffer.erase(buffer.begin(), buffer.begin() + total_len);
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
    if ((data[0] | (data[1] << 8)) != PREFIX) {
        ESP_LOGE(TAG, "parseSimple: Invalid prefix");
        return {};
    }

    uint16_t len_from_packet = data[4] | (data[5] << 8);
    size_t frame_end = 6 + len_from_packet;

    if (len < frame_end) {
        return {};
    }
    if (len_from_packet < 2) {
        return {};
    }

    size_t payload_size = len_from_packet - 2;
    std::vector<uint8_t> for_crc;
    for_crc.insert(for_crc.end(), data, data + 6 + payload_size);
    uint16_t calculated_crc = crc16(for_crc.data(), for_crc.size());
    uint16_t received_crc = data[frame_end - 2] | (data[frame_end - 1] << 8);

    if (calculated_crc != received_crc) {
        return {};
    }

    return std::vector<uint8_t>(data + 6, data + 6 + payload_size);
}
