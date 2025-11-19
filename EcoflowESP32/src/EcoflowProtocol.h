#ifndef EcoflowProtocol_h
#define EcoflowProtocol_h

#include <vector>
#include <string>
#include <cstdint>
#include "EcoflowData.h"

// ============================================================================
// Protocol Constants
// ============================================================================

const uint16_t ECOFLOW_MANUFACTURER_ID = 0xB5D5; // Placeholder, might need adjustment
const char* const SERVICE_UUID_ECOFLOW = "00000001-0000-1000-8000-00805f9b34fb";
const char* const CHAR_WRITE_UUID_ECOFLOW = "00000002-0000-1000-8000-00805f9b34fb";
const char* const CHAR_READ_UUID_ECOFLOW = "00000003-0000-1000-8000-00805f9b34fb";

// ============================================================================
// Packet (Inner, 0xAA prefix)
// ============================================================================

class Packet {
public:
    Packet(uint8_t src, uint8_t dst, uint8_t cmd_set, uint8_t cmd_id,
           const std::vector<uint8_t>& payload, uint8_t dsrc = 1, uint8_t ddst = 1,
           uint8_t version = 3, uint32_t seq = 0, uint8_t product_id = 0x0d);

    std::vector<uint8_t> toBytes();
    static Packet* fromBytes(const uint8_t* data, size_t length);

    uint8_t getCmdId() const { return _cmd_id; }
    const std::vector<uint8_t>& getPayload() const { return _payload; }

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
    uint8_t _product_id;
};

// ============================================================================
// EncPacket (Outer, 0x5A5A prefix)
// ============================================================================

class EncPacket {
public:
    enum FrameType {
        FRAME_TYPE_COMMAND = 0x00,
        FRAME_TYPE_PROTOCOL = 0x01,
    };

    enum PayloadType {
        PAYLOAD_TYPE_VX_PROTOCOL = 0x00,
    };

    EncPacket(FrameType frame_type, PayloadType payload_type,
              const std::vector<uint8_t>& payload,
              uint8_t cmd_id = 0, uint8_t version = 0,
              const uint8_t* enc_key = nullptr, const uint8_t* iv = nullptr);

    std::vector<uint8_t> toBytes();
    static EncPacket* fromBytes(const uint8_t* data, size_t length, const uint8_t* enc_key, const uint8_t* iv);

    const std::vector<uint8_t>& getPayload() const { return _payload; }


private:
    FrameType _frame_type;
    PayloadType _payload_type;
    std::vector<uint8_t> _payload;
    uint8_t _cmd_id;
    uint8_t _version;
    const uint8_t* _enc_key;
    const uint8_t* _iv;
};

// ============================================================================
// Command Builders
// ============================================================================

namespace EcoflowCommands {
    std::vector<uint8_t> buildPublicKey(const uint8_t* pubKey, size_t len);
    std::vector<uint8_t> buildSessionKeyRequest();
    std::vector<uint8_t> buildAuthStatusRequest();
    std::vector<uint8_t> buildAuthentication(const std::string& userId, const std::string& deviceSn);
    std::vector<uint8_t> buildStatusRequest();
    std::vector<uint8_t> buildAcCommand(bool on);
    std::vector<uint8_t> buildDcCommand(bool on);
    std::vector<uint8_t> buildUsbCommand(bool on);
}

// ============================================================================
// Data Parsers
// ============================================================================
namespace EcoflowDelta3 {
    struct Status {
        int batteryLevel;
        int inputPower;
        int outputPower;
        int batteryVoltage;
        int acVoltage;
        int acFrequency;
        bool acOn;
        bool dcOn;
        bool usbOn;
    };
    bool parseStatusResponse(const Packet* pkt, Status& status);
}

// ============================================================================
// Keydata helper for ECDH
// ============================================================================

namespace EcoflowKeyData {
    void initKeyData(const uint8_t* data);
    const uint8_t* get8bytes(int pos);
}

#endif // EcoflowProtocol_h
