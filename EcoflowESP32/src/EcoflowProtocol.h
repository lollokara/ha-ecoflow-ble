#ifndef ECOFLOW_PROTOCOL_H
#define ECOFLOW_PROTOCOL_H

#include <Arduino.h>
#include <vector>
#include <string>
#include "EcoflowCrypto.h"

class Packet {
public:
    static const uint8_t PREFIX = 0xAA;
    static const uint16_t PREFIX_BLADE = 0x02AA;

    Packet(uint8_t src, uint8_t dest, uint8_t cmdSet, uint8_t cmdId, const std::vector<uint8_t>& payload, uint8_t check_type = 0x01, uint8_t encrypted = 0x01, uint8_t version = 0x03, uint32_t seq = 0, uint8_t product_id = 0x0d);
    Packet() : _src(0), _dest(0), _cmdSet(0), _cmdId(0), _check_type(0), _encrypted(0), _version(0), _seq(0), _product_id(0) {}

    static Packet* fromBytes(const uint8_t* data, size_t len, bool is_xor = false);
    std::vector<uint8_t> toBytes() const;
    
    uint8_t getCmdId() const { return _cmdId; }
    const std::vector<uint8_t>& getPayload() const { return _payload; }
    uint8_t getSrc() const { return _src; }
    uint8_t getDest() const { return _dest; }
    uint8_t getCmdSet() const { return _cmdSet; }
    uint32_t getSeq() const { return _seq; }
    uint8_t getVersion() const { return _version; }

    void setSrc(uint8_t src) { _src = src; }
    void setDest(uint8_t dest) { _dest = dest; }
    void setCmdSet(uint8_t cmdSet) { _cmdSet = cmdSet; }
    void setCmdId(uint8_t cmdId) { _cmdId = cmdId; }
    void setPayload(const std::vector<uint8_t>& payload) { _payload = payload; }

    static void reset_sequence() { g_seq = 0; }


private:
    uint8_t _src;
    uint8_t _dest;
    uint8_t _cmdSet;
    uint8_t _cmdId;
    std::vector<uint8_t> _payload;
    uint8_t _check_type;
    uint8_t _encrypted;
    uint8_t _version;
    uint32_t _seq;
    uint8_t _product_id;
    static uint32_t g_seq;
};

class EncPacket {
public:
    static const uint16_t PREFIX = 0x5A5A;
    static const uint16_t PREFIX_BLADE = 0x02AA;
    static const uint8_t FRAME_TYPE_COMMAND = 0x00;
    static const uint8_t FRAME_TYPE_PROTOCOL = 0x01;
    static const uint8_t PAYLOAD_TYPE_VX_PROTOCOL = 0x00;

    EncPacket(uint8_t frame_type, uint8_t payload_type, const std::vector<uint8_t>& payload,
              uint8_t needs_ack = 0, uint8_t is_ack = 0);

    const std::vector<uint8_t>& getPayload() const { return _payload; }
    std::vector<uint8_t> toBytes(EcoflowCrypto* crypto = nullptr, bool isBlade = false) const;

    static std::vector<Packet> parsePackets(const uint8_t* data, size_t len, EcoflowCrypto& crypto, std::vector<uint8_t>& rxBuffer, bool isAuthenticated = false);
    static std::vector<uint8_t> parseSimple(const uint8_t* data, size_t len);

private:
    uint8_t _frame_type;
    uint8_t _payload_type;
    std::vector<uint8_t> _payload;
    uint8_t _needs_ack;
    uint8_t _is_ack;
};

#endif // ECOFLOW_PROTOCOL_H