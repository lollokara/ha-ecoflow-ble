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
 * @param deviceType The type of device the packet belongs to (context).
 */
void parsePacket(const Packet& pkt, EcoflowData& data, DeviceType deviceType);

} // namespace EcoflowDataParser

#endif // ECOFLOW_DATA_PARSER_H
