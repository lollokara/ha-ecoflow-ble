#ifndef ECOFLOW_MBEDTLS_H
#define ECOFLOW_MBEDTLS_H

#include "mbedtls/ecp.h"

namespace Ecoflow_mbedtls {

/**
 * @brief Loads the secp160r1 curve parameters into an mbedtls_ecp_group.
 *
 * @param grp The mbedtls_ecp_group to initialize.
 * @return 0 on success, or a negative mbedtls error code on failure.
 */
int load_secp160r1_group(mbedtls_ecp_group *grp);

} // namespace Ecoflow_mbedtls

#endif // ECOFLOW_MBEDTLS_H
