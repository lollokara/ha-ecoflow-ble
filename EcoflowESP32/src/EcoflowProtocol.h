#ifndef ECOFLOW_PROTOCOL_H
#define ECOFLOW_PROTOCOL_H

#include <Arduino.h>
#include <vector>
#include <string>

class Packet {
public:
    static const uint8_t PREFIX = 0xAA;

    Packet(uint8_t src, uint8_t dest, uint8_t cmdSet, uint8_t cmdId, const std::vector<uint8_t>& payload, uint8_t check_type, uint8_t encrypted, uint8_t version);

    static Packet* fromBytes(const uint8_t* data, size_t len);
    std::vector<uint8_t> toBytes() const;
    
    uint8_t getCmdId() const { return _cmdId; }
    const std::vector<uint8_t>& getPayload() const { return _payload; }
    uint8_t getSrc() const { return _src; }
    uint8_t getCmdSet() const { return _cmdSet; }

private:
    uint8_t _src;
    uint8_t _dest;
    uint8_t _cmdSet;
    uint8_t _cmdId;
    std::vector<uint8_t> _payload;
    uint8_t _check_type;
    uint8_t _encrypted;
    uint8_t _version;
    uint16_t _seq;
};

class EncPacket {
public:
    static const uint16_t PREFIX = 0x5A5A;
    static const uint8_t FRAME_TYPE_COMMAND = 0x00;
    static const uint8_t FRAME_TYPE_PROTOCOL = 0x01;
    static const uint8_t FRAME_TYPE_PROTOCOL_INT = 0x10;
    static const uint8_t PAYLOAD_TYPE_VX_PROTOCOL = 0x00;
    static const uint8_t PAYLOAD_TYPE_ODM_PROTOCOL = 0x04;

    EncPacket(uint8_t frame_type, uint8_t payload_type, const std::vector<uint8_t>& payload, uint16_t seq, uint8_t device_sn, const uint8_t* key, const uint8_t* iv);

    const std::vector<uint8_t>& getPayload() const { return _payload; }
    std::vector<uint8_t> toBytes() const;

    static EncPacket* fromBytes(const uint8_t* data, size_t len, const uint8_t* key, const uint8_t* iv);
    static std::vector<uint8_t> parseSimple(const uint8_t* data, size_t len);

private:
    uint8_t _frame_type;
    uint8_t _payload_type;
    std::vector<uint8_t> _payload;
    uint16_t _seq;
    uint8_t _device_sn;
};

namespace EcoflowCommands {
    std::vector<uint8_t> buildPublicKey(const uint8_t* pub_key, size_t len);
    std::vector<uint8_t> buildSessionKeyRequest();
    std::vector<uint8_t> buildAuthStatusRequest(const uint8_t* key, const uint8_t* iv);
    std::vector<uint8_t> buildAuthentication(const std::string& userId, const std::string& deviceSn, const uint8_t* key, const uint8_t* iv);
    std::vector<uint8_t> buildStatusRequest(const uint8_t* key, const uint8_t* iv);
    std::vector<uint8_t> buildAcCommand(bool on, const uint8_t* key, const uint8_t* iv);
    std::vector<uint8_t> buildDcCommand(bool on, const uint8_t* key, const uint8_t* iv);
    std::vector<uint8_t> buildUsbCommand(bool on, const uint8_t* key, const uint8_t* iv);
}

#endif // ECOFLOW_PROTOCOL_H
