#include "EcoflowProtocol.h"
#include "mbedtls/aes.h"
#include "mbedtls/md5.h"
#include <cstring>
#include "utc_sys.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

// CRC functions (implementations not shown for brevity)
uint8_t crc8(const uint8_t* data, size_t len) { /* ... */ return 0; }
uint16_t crc16(const uint8_t* data, size_t len) { /* ... */ return 0; }


// ============================================================================
// Packet Implementation
// ============================================================================

Packet::Packet(uint8_t src, uint8_t dst, uint8_t cmd_set, uint8_t cmd_id,
               const std::vector<uint8_t>& payload, uint8_t dsrc, uint8_t ddst,
               uint8_t version, uint32_t seq, uint8_t product_id)
    : _src(src), _dst(dst), _cmd_set(cmd_set), _cmd_id(cmd_id), _payload(payload),
      _dsrc(dsrc), _ddst(ddst), _version(version), _seq(seq), _product_id(product_id) {}

std::vector<uint8_t> Packet::toBytes() {
    std::vector<uint8_t> data;
    data.push_back(0xaa);
    data.push_back(_version);
    uint16_t payload_len = _payload.size();
    data.push_back(payload_len & 0xff);
    data.push_back((payload_len >> 8) & 0xff);
    data.push_back(crc8(data.data(), data.size()));
    data.push_back(_product_id);
    data.push_back(_seq & 0xff);
    data.push_back((_seq >> 8) & 0xff);
    data.push_back((_seq >> 16) & 0xff);
    data.push_back((_seq >> 24) & 0xff);
    data.push_back(0); // Unknown
    data.push_back(0); // Unknown
    data.push_back(_src);
    data.push_back(_dst);
    data.push_back(_dsrc);
    data.push_back(_ddst);
    data.push_back(_cmd_set);
    data.push_back(_cmd_id);
    data.insert(data.end(), _payload.begin(), _payload.end());
    uint16_t crc = crc16(data.data(), data.size());
    data.push_back(crc & 0xff);
    data.push_back((crc >> 8) & 0xff);
    return data;
}

Packet* Packet::fromBytes(const uint8_t* data, size_t length) {
    if (length < 20 || data[0] != 0xaa) {
        return nullptr;
    }

    uint8_t version = data[1];
    uint16_t payload_length = (data[3] << 8) | data[2];

    if (crc8(data, 4) != data[4]) {
        return nullptr;
    }

    if (crc16(data, length - 2) != ((data[length - 1] << 8) | data[length - 2])) {
        return nullptr;
    }

    uint32_t seq = (data[9] << 24) | (data[8] << 16) | (data[7] << 8) | data[6];
    uint8_t src = data[12];
    uint8_t dst = data[13];
    uint8_t dsrc = data[14];
    uint8_t ddst = data[15];
    uint8_t cmd_set = data[16];
    uint8_t cmd_id = data[17];
    std::vector<uint8_t> payload;
    if (payload_length > 0) {
        payload.assign(data + 18, data + 18 + payload_length);
    }

    return new Packet(src, dst, cmd_set, cmd_id, payload, dsrc, ddst, version, seq);
}


// ============================================================================
// EncPacket Implementation
// ============================================================================
EncPacket::EncPacket(FrameType frame_type, PayloadType payload_type,
                     const std::vector<uint8_t>& payload,
                     uint8_t cmd_id, uint8_t version,
                     const uint8_t* enc_key, const uint8_t* iv)
    : _frame_type(frame_type), _payload_type(payload_type), _payload(payload),
      _cmd_id(cmd_id), _version(version), _enc_key(enc_key), _iv(iv) {}

std::vector<uint8_t> EncPacket::toBytes() {
    std::vector<uint8_t> encrypted_payload = _payload;
    if (_enc_key && _iv) {
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_enc(&aes, _enc_key, 256);
        // PKCS7 padding
        size_t padding = 16 - (_payload.size() % 16);
        encrypted_payload.resize(_payload.size() + padding, padding);
        mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, encrypted_payload.size(), (unsigned char*)_iv, encrypted_payload.data(), encrypted_payload.data());
        mbedtls_aes_free(&aes);
    }

    std::vector<uint8_t> data;
    data.push_back(0x5a);
    data.push_back(0x5a);
    data.push_back((_frame_type << 4) | 1); //
    data.push_back(0x01);
    uint16_t payload_len = encrypted_payload.size() + 2;
    data.push_back(payload_len & 0xff);
    data.push_back((payload_len >> 8) & 0xff);
    data.insert(data.end(), encrypted_payload.begin(), encrypted_payload.end());
    uint16_t crc = crc16(data.data(), data.size());
    data.push_back(crc & 0xff);
    data.push_back((crc >> 8) & 0xff);

    return data;
}

EncPacket* EncPacket::fromBytes(const uint8_t* data, size_t length, const uint8_t* enc_key, const uint8_t* iv) {
    if (length < 8 || data[0] != 0x5a || data[1] != 0x5a) {
        return nullptr;
    }

    uint16_t payload_len = (data[5] << 8) | data[4];
    if (payload_len + 6 > length) {
        return nullptr;
    }

    if (crc16(data, payload_len + 4) != ((data[payload_len + 5] << 8) | data[payload_len + 4])) {
        return nullptr;
    }

    std::vector<uint8_t> payload;
    payload.assign(data + 6, data + 6 + payload_len - 2);

    if (enc_key && iv) {
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_dec(&aes, enc_key, 256);
        mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, payload.size(), (unsigned char*)iv, payload.data(), payload.data());
        mbedtls_aes_free(&aes);

        // Remove PKCS7 padding
        uint8_t padding = payload.back();
        if (padding > 0 && padding <= 16) {
            payload.resize(payload.size() - padding);
        }
    }

    return new EncPacket((FrameType)((data[2] >> 4) & 0x0f), (PayloadType)(data[2] & 0x0f), payload);
}

// ============================================================================
// Command Builders Implementation
// ============================================================================
namespace EcoflowCommands {
    std::vector<uint8_t> buildPublicKey(const uint8_t* pubKey, size_t len) {
        std::vector<uint8_t> payload = {0x01, 0x00};
        payload.insert(payload.end(), pubKey, pubKey + len);
        EncPacket pkt(EncPacket::FRAME_TYPE_COMMAND, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, payload);
        return pkt.toBytes();
    }

    std::vector<uint8_t> buildSessionKeyRequest() {
        EncPacket pkt(EncPacket::FRAME_TYPE_COMMAND, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, {0x02});
        return pkt.toBytes();
    }

    std::vector<uint8_t> buildAuthStatusRequest() {
        Packet inner(0x21, 0x35, 0x35, 0x89, {});
        EncPacket outer(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, inner.toBytes());
        return outer.toBytes();
    }

    std::vector<uint8_t> buildAuthentication(const std::string& userId, const std::string& deviceSn) {
        std::string combined = userId + deviceSn;
        unsigned char md5_result[16];
        mbedtls_md5( (const unsigned char*)combined.c_str(), combined.length(), md5_result);

        char hex_payload[33];
        for(int i=0; i<16; i++) {
            sprintf(hex_payload + i*2, "%02X", md5_result[i]);
        }
        hex_payload[32] = 0;

        std::vector<uint8_t> payload(hex_payload, hex_payload + 32);
        Packet inner(0x21, 0x35, 0x35, 0x86, payload);
        EncPacket outer(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, inner.toBytes());
        return outer.toBytes();
    }

    std::vector<uint8_t> buildStatusRequest() {
        Packet inner(0x21, 0x35, 0x01, 0x51, {});
        EncPacket outer(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, inner.toBytes());
        return outer.toBytes();
    }

    std::vector<uint8_t> buildAcCommand(bool on) {
        std::vector<uint8_t> payload = {0x01, (uint8_t)(on ? 1 : 0)};
        Packet inner(0x21, 0x35, 0x02, 0x01, payload);
        EncPacket outer(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, inner.toBytes());
        return outer.toBytes();
    }

    std::vector<uint8_t> buildDcCommand(bool on) {
        std::vector<uint8_t> payload = {0x01, (uint8_t)(on ? 1 : 0)};
        Packet inner(0x21, 0x35, 0x02, 0x02, payload);
        EncPacket outer(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, inner.toBytes());
        return outer.toBytes();
    }

    std::vector<uint8_t> buildUsbCommand(bool on) {
        std::vector<uint8_t> payload = {0x01, (uint8_t)(on ? 1 : 0)};
        Packet inner(0x21, 0x35, 0x02, 0x03, payload);
        EncPacket outer(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, inner.toBytes());
        return outer.toBytes();
    }
}

// ============================================================================
// Data Parsers Implementation
// ============================================================================
namespace EcoflowDelta3 {
    bool parseStatusResponse(const Packet* pkt, Status& status) {
        if (!pkt || pkt->getCmdId() != 0x51) return false;

        const auto& payload = pkt->getPayload();
        if (payload.size() < 20) return false; // Basic safety check

        // This is a simplified parser based on reverse-engineering.
        // Offsets may need adjustment for different firmware versions.
        status.outputPower = (payload[15] << 8) | payload[14];
        status.inputPower = (payload[17] << 8) | payload[16];
        status.batteryLevel = payload[18];

        uint8_t flags = payload[19];
        status.acOn = (flags & 0x01);
        status.usbOn = (flags & 0x02);
        status.dcOn = (flags & 0x04);

        if (payload.size() >= 25) {
            status.batteryVoltage = (payload[24] << 8) | payload[23];
        }
        if (payload.size() >= 29) {
            status.acVoltage = (payload[28] << 8) | payload[27];
        }
        return true;
    }
}


// ============================================================================
// Keydata helper Implementation
// ============================================================================
namespace EcoflowKeyData {
    static const uint8_t* _keydata = nullptr;
    void initKeyData(const uint8_t* data) {
        _keydata = data;
    }
    const uint8_t* get8bytes(int pos) {
        if (!_keydata) return nullptr;
        return &_keydata[pos];
    }
}
