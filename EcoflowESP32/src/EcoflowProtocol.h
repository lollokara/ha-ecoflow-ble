#ifndef ECOFLOW_PROTOCOL_H
#define ECOFLOW_PROTOCOL_H

#include <vector>
#include <string>
#include <stdint.h>

class Packet {
public:
    static const uint8_t PREFIX = 0xAA;

    Packet(uint8_t src, uint8_t dest, uint8_t cmdSet, uint8_t cmdId, const std::vector<uint8_t>& payload, uint8_t check_type = 0x01, uint8_t encrypted = 0x01, uint8_t version = 0x03);
    static Packet* fromBytes(const uint8_t* data, size_t len);

    std::vector<uint8_t> toBytes() const;

    uint8_t getSrc() const { return _src; }
    uint8_t getDest() const { return _dest; }
    uint8_t getCmdSet() const { return _cmdSet; }
    uint8_t getCmdId() const { return _cmdId; }
    const std::vector<uint8_t>& getPayload() const { return _payload; }

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
    static const uint8_t FRAME_TYPE_COMMAND = 0x01;
    static const uint8_t FRAME_TYPE_PROTOCOL = 0x02;
    static const uint8_t PAYLOAD_TYPE_VX_PROTOCOL = 0x11;

    EncPacket(uint8_t frame_type, uint8_t payload_type, const std::vector<uint8_t>& payload, uint16_t seq, uint8_t device_sn, const uint8_t* key, const uint8_t* iv);
    static EncPacket* fromBytes(const uint8_t* data, size_t len, const uint8_t* key, const uint8_t* iv);

    std::vector<uint8_t> toBytes() const;
    const std::vector<uint8_t>& getPayload() const { return _payload; }

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
    std::vector<uint8_t> buildAuthStatusRequest();
    std::vector<uint8_t> buildAuthentication(const std::string& userId, const std::string& deviceSn);
    std::vector<uint8_t> buildStatusRequest();
    std::vector<uint8_t> buildAcCommand(bool on);
    std::vector<uint8_t> buildDcCommand(bool on);
    std::vector<uint8_t> buildUsbCommand(bool on);
}


#endif // ECOFLOW_PROTOCOL_H
