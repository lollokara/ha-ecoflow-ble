#ifndef ECOFLOW_MBEDTLS_H
#define ECOFLOW_MBEDTLS_H

#include "mbedtls/ecp.h"

#define MBEDTLS_ECP_DP_SECP160R1 160

namespace Ecoflow_mbedtls {

int load_secp160r1_group(mbedtls_ecp_group *grp);

}

#endif
