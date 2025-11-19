#include "EcoflowProtocol.h"
#include <mbedtls/aes.h>
#include "mbedtls/md5.h"

// ============================================================================
// CRC IMPLEMENTATION
// ============================================================================

static const uint8_t CRC8_TABLE[256] = {
    0x00, 0xD5, 0xB4, 0x61, 0x68, 0xBD, 0x9C, 0x49, 0xD0, 0x05, 0x24, 0xF1, 0xF8, 0x2D, 0x0C, 0xD9,
    0xA0, 0x75, 0x54, 0x81, 0x88, 0x5D, 0x7C, 0xA9, 0x30, 0xE5, 0xC4, 0x11, 0x18, 0xCD, 0xEC, 0x39,
    0x40, 0x95, 0xB4, 0x61, 0x68, 0xBD, 0x9C, 0x49, 0xD0, 0x05, 0x24, 0xF1, 0xF8, 0x2D, 0x0C, 0xD9,
    0xE0, 0x35, 0x14, 0xC1, 0xC8, 0x1D, 0x3C, 0xE9, 0x70, 0xA5, 0x84, 0x51, 0x58, 0x8D, 0xAC, 0x79,
    0x80, 0x55, 0x74, 0xA1, 0xA8, 0x7D, 0x5C, 0x89, 0x10, 0xC5, 0xE4, 0x31, 0x38, 0xED, 0xCC, 0x19,
    0x60, 0xB5, 0x94, 0x41, 0x48, 0x9D, 0xBC, 0x69, 0xF0, 0x25, 0x04, 0xD1, 0xD8, 0x0D, 0x2C, 0xF9,
    0xC0, 0x15, 0x34, 0xE1, 0xE8, 0x3D, 0x1C, 0xC9, 0x50, 0x85, 0xA4, 0x71, 0x78, 0xAD, 0x8C, 0x59,
    0x20, 0xF5, 0xD4, 0x01, 0x08, 0xDD, 0xFC, 0x29, 0xB0, 0x65, 0x44, 0x91, 0x98, 0x4D, 0x6C, 0xB9,
    0x00, 0xD5, 0xB4, 0x61, 0x68, 0xBD, 0x9C, 0x49, 0xD0, 0x05, 0x24, 0xF1, 0xF8, 0x2D, 0x0C, 0xD9,
    0xA0, 0x75, 0x54, 0x81, 0x88, 0x5D, 0x7C, 0xA9, 0x30, 0xE5, 0xC4, 0x11, 0x18, 0xCD, 0xEC, 0x39,
    0x40, 0x95, 0xB4, 0x61, 0x68, 0xBD, 0x9C, 0x49, 0xD0, 0x05, 0x24, 0xF1, 0xF8, 0x2D, 0x0C, 0xD9,
    0xE0, 0x35, 0x14, 0xC1, 0xC8, 0x1D, 0x3C, 0xE9, 0x70, 0xA5, 0x84, 0x51, 0x58, 0x8D, 0xAC, 0x79,
    0x80, 0x55, 0x74, 0xA1, 0xA8, 0x7D, 0x5C, 0x89, 0x10, 0xC5, 0xE4, 0x31, 0x38, 0xED, 0xCC, 0x19,
    0x60, 0xB5, 0x94, 0x41, 0x48, 0x9D, 0xBC, 0x69, 0xF0, 0x25, 0x04, 0xD1, 0xD8, 0x0D, 0x2C, 0xF9,
    0xC0, 0x15, 0x34, 0xE1, 0xE8, 0x3D, 0x1C, 0xC9, 0x50, 0x85, 0xA4, 0x71, 0x78, 0xAD, 0x8C, 0x59,
    0x20, 0xF5, 0xD4, 0x01, 0x08, 0xDD, 0xFC, 0x29, 0xB0, 0x65, 0x44, 0x91, 0x98, 0x4D, 0x6C, 0xB9,
};

uint8_t EcoflowCRC::crc8(const uint8_t* data, size_t len) {
  if (!data || len == 0) return 0;
  uint8_t crc = 0;
  for (size_t i = 0; i < len; i++) {
    crc = CRC8_TABLE[crc ^ data[i]];
  }
  return crc;
}

uint16_t EcoflowCRC::crc16(const uint8_t* data, size_t len) {
  if (!data || len == 0) return 0;
  uint16_t crc = 0x0000;
  for (size_t i = 0; i < len; i++) {
    crc ^= ((uint16_t)data[i] << 8);
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc = crc << 1;
      }
    }
    crc &= 0xFFFF;
  }
  return crc;
}

// ============================================================================
// PACKET CLASS IMPLEMENTATION
// ============================================================================

constexpr uint8_t Packet::PREFIX;
constexpr size_t Packet::HEADER_SIZE;

Packet::Packet(uint8_t src, uint8_t dst, uint8_t cmd_set, uint8_t cmd_id,
               const uint8_t* payload, size_t payload_len,
               uint8_t dsrc, uint8_t ddst, uint8_t version, uint32_t seq)
    : _src(src), _dst(dst), _cmd_set(cmd_set), _cmd_id(cmd_id),
      _dsrc(dsrc), _ddst(ddst), _version(version), _seq(seq) {
  if (payload && payload_len > 0) {
    _payload.insert(_payload.end(), payload, payload + payload_len);
  }
}

std::vector<uint8_t> Packet::toBytes() const {
  std::vector<uint8_t> data;
  data.push_back(PREFIX);
  data.push_back(_version);
  uint16_t payload_len = _payload.size();
  data.push_back((payload_len >> 8) & 0xFF);
  data.push_back(payload_len & 0xFF);
  data.push_back(_cmd_set);
  data.push_back(_cmd_id);
  data.push_back(_src);
  data.push_back(_dst);
  data.push_back(_dsrc);
  data.push_back(_ddst);
  data.push_back((_seq >> 24) & 0xFF);
  data.push_back((_seq >> 16) & 0xFF);
  data.push_back((_seq >> 8) & 0xFF);
  data.push_back(_seq & 0xFF);

  uint8_t header_crc = EcoflowCRC::crc8(data.data(), data.size());
  data.push_back(header_crc);

  if (!_payload.empty()) {
    data.insert(data.end(), _payload.begin(), _payload.end());
  }

  uint16_t payload_crc = 0;
  if (!_payload.empty()) {
    payload_crc = EcoflowCRC::crc16(_payload.data(), _payload.size());
  }

  data.push_back((payload_crc >> 8) & 0xFF);
  data.push_back(payload_crc & 0xFF);

  return data;
}

Packet* Packet::fromBytes(const uint8_t* data, size_t len, bool is_xor) {
  if (!data || len < HEADER_SIZE + 2) return nullptr;
  if (data[0] != PREFIX) return nullptr;

  uint8_t version = data[1];
  uint16_t payload_len = ((uint16_t)data[2] << 8) | data[3];
  uint8_t cmd_set = data[4];
  uint8_t cmd_id = data[5];
  uint8_t src = data[6];
  uint8_t dst = data[7];
  uint8_t dsrc = data[8];
  uint8_t ddst = data[9];
  uint32_t seq = ((uint32_t)data[10] << 24) | ((uint32_t)data[11] << 16) |
                 ((uint32_t)data[12] << 8) | data[13];

  uint8_t header_crc_calc = EcoflowCRC::crc8(data, HEADER_SIZE);
  if (header_crc_calc != data[HEADER_SIZE]) return nullptr;

  std::vector<uint8_t> payload;
  if (payload_len > 0) {
    if (len < HEADER_SIZE + 1 + payload_len + 2) return nullptr;
    const uint8_t* payload_data = data + HEADER_SIZE + 1;
    if (is_xor && data[HEADER_SIZE] != 0x00) {
      for (uint16_t i = 0; i < payload_len; i++) {
        payload.push_back(payload_data[i] ^ data[HEADER_SIZE]);
      }
    } else {
      payload.insert(payload.end(), payload_data, payload_data + payload_len);
    }

    uint16_t payload_crc_received = ((uint16_t)payload_data[payload_len] << 8) |
                                    payload_data[payload_len + 1];
    uint16_t payload_crc_calc = EcoflowCRC::crc16(payload_data, payload_len);
    if (payload_crc_calc != payload_crc_received) return nullptr;
  }

  return new Packet(src, dst, cmd_set, cmd_id, payload.data(), payload.size(),
                    dsrc, ddst, version, seq);
}

// ============================================================================
// ENCPACKET CLASS IMPLEMENTATION
// ============================================================================

constexpr uint8_t EncPacket::PREFIX_0;
constexpr uint8_t EncPacket::PREFIX_1;
constexpr size_t EncPacket::HEADER_SIZE;

EncPacket::EncPacket(uint8_t frame_type, uint8_t payload_type,
                     const uint8_t* payload, size_t payload_len,
                     uint8_t cmd_id, uint8_t version,
                     const uint8_t* enc_key, const uint8_t* iv)
    : _frame_type(frame_type), _payload_type(payload_type),
      _cmd_id(cmd_id), _version(version), _enc_key(enc_key), _iv(iv) {
  if (payload && payload_len > 0) {
    _payload.insert(_payload.end(), payload, payload + payload_len);
  }
}

std::vector<uint8_t> EncPacket::encryptPayload() const {
  if (!_enc_key || !_iv) return _payload;
  return EcoflowEncryption::aesEncrypt(_payload.data(), _payload.size(),
                                       _enc_key, _iv);
}

std::vector<uint8_t> EncPacket::toBytes() const {
  std::vector<uint8_t> data;
  data.push_back(PREFIX_0);
  data.push_back(PREFIX_1);

  std::vector<uint8_t> encrypted = encryptPayload();
  data.push_back(_frame_type);
  data.push_back(_payload_type);

  uint16_t payload_len = encrypted.size();
  data.push_back((payload_len >> 8) & 0xFF);
  data.push_back(payload_len & 0xFF);

  data.insert(data.end(), encrypted.begin(), encrypted.end());

  uint16_t crc = EcoflowCRC::crc16(data.data(), data.size());
  data.push_back((crc >> 8) & 0xFF);
  data.push_back(crc & 0xFF);

  return data;
}

EncPacket* EncPacket::fromBytes(const uint8_t* data, size_t len,
                                const uint8_t* enc_key,
                                const uint8_t* iv) {
  if (!data || len < HEADER_SIZE + 2) return nullptr;
  if (data[0] != PREFIX_0 || data[1] != PREFIX_1) return nullptr;

  uint8_t frame_type = data[2];
  uint8_t payload_type = data[3];
  uint16_t payload_len = ((uint16_t)data[4] << 8) | data[5];

  if (len < HEADER_SIZE + payload_len + 2) return nullptr;

  const uint8_t* payload_data = data + HEADER_SIZE;
  uint16_t crc_received = ((uint16_t)data[HEADER_SIZE + payload_len] << 8) |
                          data[HEADER_SIZE + payload_len + 1];
  uint16_t crc_calc = EcoflowCRC::crc16(data, HEADER_SIZE + payload_len);

  if (crc_calc != crc_received) return nullptr;

  std::vector<uint8_t> decrypted;
  if (enc_key && iv) {
    decrypted = decryptPayload(payload_data, payload_len, enc_key, iv);
  } else {
    decrypted.insert(decrypted.end(), payload_data, payload_data + payload_len);
  }

  return new EncPacket(frame_type, payload_type, decrypted.data(), decrypted.size(),
                       0, 0, nullptr, nullptr);
}

std::vector<uint8_t> EncPacket::decryptPayload(const uint8_t* ciphertext, size_t len,
                                               const uint8_t* key, const uint8_t* iv) {
  return EcoflowEncryption::aesDecrypt(ciphertext, len, key, iv);
}

// ============================================================================
// AES ENCRYPTION/DECRYPTION (mbedtls)
// ============================================================================

std::vector<uint8_t> EcoflowEncryption::aesEncrypt(const uint8_t* plaintext, size_t len,
                                                   const uint8_t* key, const uint8_t* iv) {
  size_t pad_len = 16 - (len % 16);
  size_t padded_len = len + pad_len;
  std::vector<uint8_t> padded(padded_len);
  memcpy(padded.data(), plaintext, len);

  uint8_t pad_byte = pad_len;
  for (size_t i = len; i < padded_len; i++) {
    padded[i] = pad_byte;
  }

  mbedtls_aes_context aes_ctx;
  mbedtls_aes_init(&aes_ctx);

  uint8_t iv_copy[16];
  memcpy(iv_copy, iv, 16);

  mbedtls_aes_setkey_enc(&aes_ctx, key, 128);

  std::vector<uint8_t> ciphertext(padded_len);
  mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, padded_len,
                        iv_copy, padded.data(), ciphertext.data());

  mbedtls_aes_free(&aes_ctx);

  return ciphertext;
}

std::vector<uint8_t> EcoflowEncryption::aesDecrypt(const uint8_t* ciphertext, size_t len,
                                                   const uint8_t* key, const uint8_t* iv) {
  mbedtls_aes_context aes_ctx;
  mbedtls_aes_init(&aes_ctx);

  uint8_t iv_copy[16];
  memcpy(iv_copy, iv, 16);

  mbedtls_aes_setkey_dec(&aes_ctx, key, 128);

  std::vector<uint8_t> plaintext(len);
  mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, len,
                        iv_copy, (uint8_t*)ciphertext, plaintext.data());

  mbedtls_aes_free(&aes_ctx);

  if (len > 0) {
    uint8_t pad_len = plaintext[len - 1];
    if (pad_len <= 16 && pad_len <= len) {
      plaintext.resize(len - pad_len);
    }
  }

  return plaintext;
}

// ============================================================================
// COMMAND BUILDERS
// ============================================================================

std::vector<uint8_t> EcoflowCommands::buildGetKeyInfoReq() {
  Packet pkt(0x21, DEVICE_ADDRESS_MAIN, PACKET_CMD_SET_DEFAULT, 0x00);
  return pkt.toBytes();
}

std::vector<uint8_t> EcoflowCommands::buildGetAuthStatus() {
  Packet pkt(0x21, DEVICE_ADDRESS_MAIN, PACKET_CMD_SET_DEFAULT,
             PACKET_CMD_ID_GET_AUTH_STATUS);
  return pkt.toBytes();
}

std::vector<uint8_t> EcoflowCommands::buildAutoAuthentication(
    const std::string& user_id, const std::string& device_sn) {
  std::string combined = user_id + device_sn;
  uint8_t digest[16];

  mbedtls_md5_context md5;
  mbedtls_md5_init(&md5);
  mbedtls_md5_starts_ret(&md5);
  mbedtls_md5_update(&md5, (const uint8_t*)combined.c_str(), combined.length());
  mbedtls_md5_finish(&md5, digest);
  mbedtls_md5_free(&md5);

  Packet pkt(0x21, DEVICE_ADDRESS_MAIN, PACKET_CMD_SET_DEFAULT,
             PACKET_CMD_ID_AUTO_AUTH, digest, 16);
  return pkt.toBytes();
}

std::vector<uint8_t> EcoflowCommands::buildStatusRequest() {
  Packet pkt(0x21, DEVICE_ADDRESS_MAIN, PACKET_CMD_SET_DEFAULT,
             PACKET_CMD_ID_REQUEST_STATUS);
  return pkt.toBytes();
}

std::vector<uint8_t> EcoflowCommands::buildAcCommand(bool on) {
  uint8_t payload[1] = {on ? 1 : 0};
  Packet pkt(0x21, DEVICE_ADDRESS_MAIN, PACKET_CMD_SET_DEFAULT,
             PACKET_CMD_ID_AC_CONTROL, payload, 1);
  return pkt.toBytes();
}

std::vector<uint8_t> EcoflowCommands::buildDcCommand(bool on) {
  uint8_t payload[1] = {on ? 1 : 0};
  Packet pkt(0x21, DEVICE_ADDRESS_MAIN, 0x36, 0x35, payload, 1);
  return pkt.toBytes();
}

std::vector<uint8_t> EcoflowCommands::buildUsbCommand(bool on) {
  uint8_t payload[1] = {on ? 1 : 0};
  Packet pkt(0x21, DEVICE_ADDRESS_MAIN, 0x37, 0x36, payload, 1);
  return pkt.toBytes();
}

// ============================================================================
// DELTA 3 STATUS DECODER
// ============================================================================

bool EcoflowDelta3::parseStatusResponse(const Packet* pkt, Status& out_status) {
  if (!pkt || pkt->getPayload().size() < 50) return false;

  const auto& payload = pkt->getPayload();
  
  out_status.batteryLevel = payload[0];
  out_status.inputPower = (payload[1] << 8) | payload[2];
  out_status.outputPower = (payload[3] << 8) | payload[4];
  out_status.batteryVoltage = (payload[5] << 8) | payload[6];
  out_status.acVoltage = (payload[7] << 8) | payload[8];
  out_status.acFrequency = (payload[9] << 8) | payload[10];

  uint16_t flags = (payload[11] << 8) | payload[12];
  out_status.acOn = (flags & 0x01) != 0;
  out_status.dcOn = (flags & 0x02) != 0;
  out_status.usbOn = (flags & 0x04) != 0;
  out_status.acPluggedIn = (flags & 0x08) != 0;

  return true;
}
