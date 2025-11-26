#ifndef ECOFLOW_CONSTANTS_H
#define ECOFLOW_CONSTANTS_H

namespace Ecoflow {
    namespace D2 {
        // Command Sets
        constexpr uint8_t CMDSET_PD = 0x20;

        // Command IDs
        constexpr uint8_t CMDID_PD_HEARTBEAT = 0x02;
        constexpr uint8_t CMDID_EMS_HEARTBEAT = 0x02;
        constexpr uint8_t CMDID_BMS_HEARTBEAT = 0x32;
        constexpr uint8_t CMDID_MPPT_HEARTBEAT = 0x02;
        constexpr uint8_t CMDID_KIT_INFO = 0x0E;

        // Source Addresses
        constexpr uint8_t SRC_PD = 0x02;
        constexpr uint8_t SRC_EMS = 0x03;
        constexpr uint8_t SRC_BMS = 0x03;
        constexpr uint8_t SRC_MPPT = 0x05;
        constexpr uint8_t SRC_KIT_INFO = 0x03;
    }
}

#endif // ECOFLOW_CONSTANTS_H
