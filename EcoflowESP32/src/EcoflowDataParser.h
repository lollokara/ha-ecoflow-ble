#ifndef ECOFLOW_DATA_PARSER_H
#define ECOFLOW_DATA_PARSER_H

#include "EcoflowData.h"
#include "EcoflowProtocol.h"
#include "types.h"

namespace EcoflowDataParser {

/**
 * @brief Parses a data packet and populates an EcoflowData struct.
 *
 * @param pkt The packet to parse.
 * @param data The EcoflowData struct to populate.
 * @param type The device type.
 */
void parsePacket(const Packet& pkt, EcoflowData& data, DeviceType type);

/**
 * @brief Triggers a debug dump of the next received packets.
 */
void triggerDebugDump();

} // namespace EcoflowDataParser

#endif // ECOFLOW_DATA_PARSER_H
