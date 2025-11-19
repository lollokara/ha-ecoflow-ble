#ifndef ECOFLOW_DATA_PARSER_H
#define ECOFLOW_DATA_PARSER_H

#include "EcoflowData.h"
#include "EcoflowProtocol.h"

namespace EcoflowDataParser {

/**
 * @brief Parses a data packet and populates an EcoflowData struct.
 *
 * @param pkt The packet to parse.
 * @param data The EcoflowData struct to populate.
 * @return true if the packet was successfully parsed, false otherwise.
 */
bool parsePacket(const Packet& pkt, EcoflowData& data);

} // namespace EcoflowDataParser

#endif // ECOFLOW_DATA_PARSER_H
