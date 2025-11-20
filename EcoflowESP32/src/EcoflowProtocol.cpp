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

Packet* Packet::fromBytes(const uint8_t* data, size_t len) {
    if (len < 13 || data[0] != PREFIX) { // 13 bytes is the minimum length for a packet with an empty payload
        return nullptr;
    }
    uint16_t payload_len = data[4] | (data[5] << 8);
    if (len < 13 + payload_len) {
        return nullptr;
    }

    size_t crc_offset = 11 + payload_len;
    uint16_t crc_from_packet = data[crc_offset] | (data[crc_offset + 1] << 8);
    if (crc_from_packet != crc16(data, crc_offset)) {
        return nullptr;
    }

    std::vector<uint8_t> payload(data + 11, data + 11 + payload_len);
    uint16_t seq = data[9] | (data[10] << 8);
    return new Packet(data[1], data[2], data[7], data[8], payload, data[3], data[6], data[3] & 0x0F, seq);
}

std::vector<uint8_t> Packet::toBytes() const {
    std::vector<uint8_t> bytes;
    bytes.push_back(PREFIX);
    bytes.push_back(_src);
    bytes.push_back(_dest);
    bytes.push_back((_version << 4) | _check_type);
    bytes.push_back(_payload.size() & 0xFF);
    bytes.push_back((_payload.size() >> 8) & 0xFF);
    bytes.push_back(_encrypted);
    bytes.push_back(_cmdSet);
    bytes.push_back(_cmdId);
    bytes.push_back(_seq & 0xFF);
    bytes.push_back((_seq >> 8) & 0xFF);
    bytes.insert(bytes.end(), _payload.begin(), _payload.end());
    uint16_t crc = crc16(bytes.data(), bytes.size());
    bytes.push_back(crc & 0xFF);
    bytes.push_back((crc >> 8) & 0xFF);
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
    }

    std::vector<uint8_t> header;
    header.push_back(PREFIX & 0xFF);
    header.push_back((PREFIX >> 8) & 0xFF);
    header.push_back((_payload_type << 4) | _frame_type);
    header.push_back((_is_ack << 7) | (_needs_ack << 6));
    uint16_t len = encrypted_payload.size() + 2; // payload + 2 bytes for CRC
    header.push_back(len & 0xFF);
    header.push_back((len >> 8) & 0xFF);

    std::vector<uint8_t> packet_data = header;
    packet_data.insert(packet_data.end(), encrypted_payload.begin(), encrypted_payload.end());

    uint16_t crc = crc16(packet_data.data(), packet_data.size());
    packet_data.push_back(crc & 0xFF);
    packet_data.push_back((crc >> 8) & 0xFF);

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
