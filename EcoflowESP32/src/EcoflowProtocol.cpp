#include "EcoflowProtocol.h"
#include "mbedtls/aes.h"
#include "mbedtls/md5.h"
#include <Arduino.h>

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

Packet::Packet(uint8_t src, uint8_t dest, uint8_t cmdSet, uint8_t cmdId, const std::vector<uint8_t>& payload, uint8_t check_type, uint8_t encrypted, uint8_t version) :
    _src(src), _dest(dest), _cmdSet(cmdSet), _cmdId(cmdId), _payload(payload), _check_type(check_type), _encrypted(encrypted), _version(version) {
    static uint16_t seq = 0;
    _seq = seq++;
}

Packet* Packet::fromBytes(const uint8_t* data, size_t len) {
    if (len < 11 || data[0] != PREFIX) {
        return nullptr;
    }
    uint16_t payload_len = data[4] | (data[5] << 8);
    if (len < 11 + payload_len) {
        return nullptr;
    }
    uint16_t crc = data[9 + payload_len] | (data[10 + payload_len] << 8);
    if (crc != crc16(data, 9 + payload_len)) {
        return nullptr;
    }

    std::vector<uint8_t> payload(data + 9, data + 9 + payload_len);
    return new Packet(data[1], data[2], data[7], data[8], payload, data[3], data[6], data[3] & 0x0F);
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
    bytes.insert(bytes.end(), _payload.begin(), _payload.end());
    uint16_t crc = crc16(bytes.data(), bytes.size());
    bytes.push_back(crc & 0xFF);
    bytes.push_back((crc >> 8) & 0xFF);
    return bytes;
}


EncPacket::EncPacket(uint8_t frame_type, uint8_t payload_type, const std::vector<uint8_t>& payload, uint16_t seq, uint8_t device_sn, const uint8_t* key, const uint8_t* iv) :
    _frame_type(frame_type), _payload_type(payload_type), _payload(payload), _seq(seq), _device_sn(device_sn) {
    if (key && iv) {
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_enc(&aes, key, 128);

        uint8_t p_payload[_payload.size()];
        memcpy(p_payload, _payload.data(), _payload.size());

        // PKCS7 padding
        int padding = 16 - (_payload.size() % 16);
        std::vector<uint8_t> padded_payload = _payload;
        for (int i = 0; i < padding; ++i) {
            padded_payload.push_back(padding);
        }

        _payload.resize(padded_payload.size());
        uint8_t temp_iv[16];
        memcpy(temp_iv, iv, 16);
        mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_payload.size(), temp_iv, padded_payload.data(), _payload.data());
        mbedtls_aes_free(&aes);
    }
}

EncPacket* EncPacket::fromBytes(const uint8_t* data, size_t len, const uint8_t* key, const uint8_t* iv) {
    if (len < 8 || (data[0] | (data[1] << 8)) != PREFIX) {
        return nullptr;
    }
    uint16_t payload_len = data[4] | (data[5] << 8);
    if (len < 8 + payload_len) {
        return nullptr;
    }
    uint16_t crc = data[6 + payload_len] | (data[7 + payload_len] << 8);
    if (crc != crc16(data, 6 + payload_len)) {
        return nullptr;
    }

    std::vector<uint8_t> payload(data + 6, data + 6 + payload_len);

    if(key && iv) {
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_dec(&aes, key, 128);
        std::vector<uint8_t> decrypted_payload(payload.size());
        uint8_t temp_iv[16];
        memcpy(temp_iv, iv, 16);
        mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, payload.size(), temp_iv, payload.data(), decrypted_payload.data());
        mbedtls_aes_free(&aes);

        // Remove PKCS7 padding
        uint8_t padding = decrypted_payload.back();
        decrypted_payload.resize(decrypted_payload.size() - padding);
        payload = decrypted_payload;
    }

    return new EncPacket(data[2], data[3], payload, 0, 0, nullptr, nullptr);
}

std::vector<uint8_t> EncPacket::toBytes() const {
    std::vector<uint8_t> bytes;
    bytes.push_back(PREFIX & 0xFF);
    bytes.push_back((PREFIX >> 8) & 0xFF);
    bytes.push_back(_frame_type << 4);
    bytes.push_back(0x01);

    uint16_t len_field = _payload.size() + 2;
    bytes.push_back(len_field & 0xFF);
    bytes.push_back((len_field >> 8) & 0xFF);

    bytes.insert(bytes.end(), _payload.begin(), _payload.end());
    
    uint16_t crc = crc16(bytes.data(), bytes.size());
    bytes.push_back(crc & 0xFF);
    bytes.push_back((crc >> 8) & 0xFF);

    return bytes;
}

namespace EcoflowCommands {
    std::vector<uint8_t> buildPublicKey(const uint8_t* pub_key, size_t len) {
        std::vector<uint8_t> payload;
        payload.push_back(0x01);
        payload.push_back(0x00);
        payload.insert(payload.end(), pub_key, pub_key + len);
        EncPacket pkt(EncPacket::FRAME_TYPE_COMMAND, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, payload, 0, 0, nullptr, nullptr);
        return pkt.toBytes();
    }

    std::vector<uint8_t> buildSessionKeyRequest() {
        std::vector<uint8_t> payload;
        payload.push_back(0x02);
        EncPacket pkt(EncPacket::FRAME_TYPE_COMMAND, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, payload, 0, 0, nullptr, nullptr);
        return pkt.toBytes();
    }

    std::vector<uint8_t> buildAuthStatusRequest(const uint8_t* key, const uint8_t* iv) {
        Packet inner_pkt(0x21, 0x35, 0x35, 0x89, {}, 0x01, 0x01, 0x03);
        EncPacket pkt(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, inner_pkt.toBytes(), 0, 0, key, iv);
        return pkt.toBytes();
    }

    std::vector<uint8_t> buildAuthentication(const std::string& userId, const std::string& deviceSn, const uint8_t* key, const uint8_t* iv) {
        uint8_t md5_data[16];
        mbedtls_md5((const unsigned char*)(userId + deviceSn).c_str(), userId.length() + deviceSn.length(), md5_data);

        char hex_data[33];
        for(int i=0; i<16; i++) {
            sprintf(&hex_data[i*2], "%02X", md5_data[i]);
        }
        hex_data[32] = 0;

        std::vector<uint8_t> payload(hex_data, hex_data + 32);
        Packet inner_pkt(0x21, 0x35, 0x35, 0x86, payload, 0x01, 0x01, 0x03);
        EncPacket pkt(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, inner_pkt.toBytes(), 0, 0, key, iv);
        return pkt.toBytes();
    }

    std::vector<uint8_t> buildStatusRequest(const uint8_t* key, const uint8_t* iv) {
        Packet inner_pkt(0x21, 0x35, 0x35, 0x01, {}, 0x01, 0x01, 0x03);
        EncPacket pkt(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, inner_pkt.toBytes(), 0, 0, key, iv);
        return pkt.toBytes();
    }

    std::vector<uint8_t> buildAcCommand(bool on, const uint8_t* key, const uint8_t* iv) {
        std::vector<uint8_t> payload = { (uint8_t)(on ? 1 : 0) };
        Packet inner_pkt(0x21, 0x35, 0x35, 0x91, payload, 0x01, 0x01, 0x03);
        EncPacket pkt(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, inner_pkt.toBytes(), 0, 0, key, iv);
        return pkt.toBytes();
    }

    std::vector<uint8_t> buildDcCommand(bool on, const uint8_t* key, const uint8_t* iv) {
        std::vector<uint8_t> payload = { (uint8_t)(on ? 1 : 0) };
        Packet inner_pkt(0x21, 0x35, 0x35, 0x92, payload, 0x01, 0x01, 0x03);
        EncPacket pkt(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, inner_pkt.toBytes(), 0, 0, key, iv);
        return pkt.toBytes();
    }

    std::vector<uint8_t> buildUsbCommand(bool on, const uint8_t* key, const uint8_t* iv) {
        std::vector<uint8_t> payload = { (uint8_t)(on ? 1 : 0) };
        Packet inner_pkt(0x21, 0x35, 0x35, 0x93, payload, 0x01, 0x01, 0x03);
        EncPacket pkt(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, inner_pkt.toBytes(), 0, 0, key, iv);
        return pkt.toBytes();
    }
}
