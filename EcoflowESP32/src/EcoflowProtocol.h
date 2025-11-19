#ifndef ECOFLOW_PROTOCOL_H
#define ECOFLOW_PROTOCOL_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <mbedtls/aes.h>
#include <mbedtls/md5.h>

/**
 * EcoflowProtocol.h - Protocol v2 Implementation (COMPLETE)
 *
 * Two-layer protocol:
 * 1. EncPacket (outer): 0x5a5a prefix, AES-256-CBC encrypted payload
 * 2. Packet (inner): 0xaa prefix, checksummed command/response
 *
 * Authentication: ECDH key exchange + MD5 session key + encrypted messages
 */

// ============================================================================
// SERVICE & CHARACTERISTICS - DELTA 3
// ============================================================================

#define SERVICE_UUID_ECOFLOW "00000001-0000-1000-8000-00805f9b34fb"
#define CHAR_WRITE_UUID_ECOFLOW "00000002-0000-1000-8000-00805f9b34fb"
#define CHAR_READ_UUID_ECOFLOW "00000003-0000-1000-8000-00805f9b34fb"

#define ECOFLOW_MANUFACTURER_ID 46517

// ============================================================================
// PROTOCOL v2 CONSTANTS
// ============================================================================

#define ENCPACKET_PREFIX_0 0x5a
#define ENCPACKET_PREFIX_1 0x5a
#define PACKET_PREFIX 0xaa

// EncPacket frame types
#define ENCPACKET_FRAME_TYPE_COMMAND 0x00
#define ENCPACKET_FRAME_TYPE_PROTOCOL 0x01
#define ENCPACKET_FRAME_TYPE_PROTOCOL_INT 0x10

// EncPacket payload types
#define ENCPACKET_PAYLOAD_TYPE_VX_PROTOCOL 0x00
#define ENCPACKET_PAYLOAD_TYPE_ODM_PROTOCOL 0x04

// Packet command sets & IDs
#define PACKET_CMD_SET_DEFAULT 0x35
#define PACKET_CMD_ID_GET_AUTH_STATUS 0x89
#define PACKET_CMD_ID_AUTO_AUTH 0x86
#define PACKET_CMD_ID_REQUEST_STATUS 0x81
#define PACKET_CMD_ID_AC_CONTROL 0x34

// Device addresses
#define DEVICE_ADDRESS_MAIN 0x32
#define DEVICE_ADDRESS_BMS 0x31

// ============================================================================
// CRC HELPERS
// ============================================================================

namespace EcoflowCRC {
uint8_t crc8(const uint8_t* data, size_t len);
uint16_t crc16(const uint8_t* data, size_t len);
}

// ============================================================================
// PACKET CLASS (Inner Protocol - always 0xaa prefix)
// ============================================================================

class Packet {
 public:
  static constexpr uint8_t PREFIX = 0xaa;
  static constexpr size_t HEADER_SIZE = 18;

  Packet(uint8_t src, uint8_t dst, uint8_t cmd_set, uint8_t cmd_id,
         const uint8_t* payload = nullptr, size_t payload_len = 0,
         uint8_t dsrc = 1, uint8_t ddst = 1, uint8_t version = 3,
         uint32_t seq = 0);

  std::vector<uint8_t> toBytes() const;
  static Packet* fromBytes(const uint8_t* data, size_t len, bool is_xor = false);

  uint8_t getSrc() const { return _src; }
  uint8_t getDst() const { return _dst; }
  uint8_t getCmdSet() const { return _cmd_set; }
  uint8_t getCmdId() const { return _cmd_id; }
  const std::vector<uint8_t>& getPayload() const { return _payload; }
  uint32_t getSeq() const { return _seq; }
  uint8_t getVersion() const { return _version; }

 private:
  uint8_t _src;
  uint8_t _dst;
  uint8_t _cmd_set;
  uint8_t _cmd_id;
  std::vector<uint8_t> _payload;
  uint8_t _dsrc;
  uint8_t _ddst;
  uint8_t _version;
  uint32_t _seq;
};

// ============================================================================
// ENCPACKET CLASS (Outer Protocol - always 0x5a5a prefix)
// ============================================================================

class EncPacket {
 public:
  static constexpr uint8_t PREFIX_0 = 0x5a;
  static constexpr uint8_t PREFIX_1 = 0x5a;
  static constexpr size_t HEADER_SIZE = 6;

  EncPacket(uint8_t frame_type, uint8_t payload_type,
            const uint8_t* payload, size_t payload_len,
            uint8_t cmd_id = 0, uint8_t version = 0,
            const uint8_t* enc_key = nullptr,
            const uint8_t* iv = nullptr);

  std::vector<uint8_t> toBytes() const;
  static EncPacket* fromBytes(const uint8_t* data, size_t len,
                              const uint8_t* enc_key = nullptr,
                              const uint8_t* iv = nullptr);

  std::vector<uint8_t> encryptPayload() const;
  static std::vector<uint8_t> decryptPayload(const uint8_t* ciphertext, size_t len,
                                             const uint8_t* key, const uint8_t* iv);

  uint8_t getFrameType() const { return _frame_type; }
  uint8_t getPayloadType() const { return _payload_type; }
  const std::vector<uint8_t>& getPayload() const { return _payload; }

 private:
  uint8_t _frame_type;
  uint8_t _payload_type;
  std::vector<uint8_t> _payload;
  uint8_t _cmd_id;
  uint8_t _version;
  const uint8_t* _enc_key;
  const uint8_t* _iv;

  friend class EcoflowESP32;
};

// ============================================================================
// ENCRYPTION/DECRYPTION (AES-256-CBC with mbedtls)
// ============================================================================

namespace EcoflowEncryption {
std::vector<uint8_t> aesEncrypt(const uint8_t* plaintext, size_t len,
                                const uint8_t* key, const uint8_t* iv);
std::vector<uint8_t> aesDecrypt(const uint8_t* ciphertext, size_t len,
                                const uint8_t* key, const uint8_t* iv);
}

// ============================================================================
// ECDH KEY EXCHANGE
// ============================================================================

namespace EcoflowECDH {
void init();
bool generateKeys();
const uint8_t* getPublicKey();
bool computeSharedSecret(const uint8_t* device_public_key);
const std::vector<uint8_t>& getSharedKey();
void generateSessionKey(const uint8_t* sRand, const uint8_t* seed,
                        uint8_t* out_key, uint8_t* out_iv);
}

// ============================================================================
// KEYDATA (Base64-decoded ECDH constants, 4096 bytes)
// ============================================================================

namespace EcoflowKeyData {
// Initialize with base64-decoded keydata from ha-ef-ble/util/keydata.py
void initKeyData(const uint8_t* keydata_4096_bytes);
std::vector<uint8_t> get8bytes(size_t pos);
}

// ============================================================================
// COMMAND BUILDERS
// ============================================================================

namespace EcoflowCommands {
// Authentication sequence
std::vector<uint8_t> buildGetKeyInfoReq();
std::vector<uint8_t> buildGetAuthStatus();
std::vector<uint8_t> buildAutoAuthentication(const std::string& user_id,
                                             const std::string& device_sn);

// Status & control
std::vector<uint8_t> buildStatusRequest();
std::vector<uint8_t> buildAcCommand(bool on);
std::vector<uint8_t> buildDcCommand(bool on);
std::vector<uint8_t> buildUsbCommand(bool on);
}

// ============================================================================
// DELTA 3 STATUS DECODER
// ============================================================================

namespace EcoflowDelta3 {
struct Status {
  uint8_t batteryLevel;  // %
  uint16_t inputPower;   // W
  uint16_t outputPower;  // W
  uint16_t batteryVoltage; // mV
  uint16_t acVoltage;    // mV
  uint16_t acFrequency;  // Hz
  bool acOn;
  bool dcOn;
  bool usbOn;
  bool acPluggedIn;
};

// Parse decrypted Packet payload into Status
bool parseStatusResponse(const Packet* pkt, Status& out_status);
}

#endif
