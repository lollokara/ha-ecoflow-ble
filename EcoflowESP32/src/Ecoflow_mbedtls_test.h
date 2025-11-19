#ifndef ECOFLOW_MBEDTLS_TEST_H
#define ECOFLOW_MBEDTLS_TEST_H

namespace Ecoflow_mbedtls_test {

/**
 * @brief Runs a test of the mbedtls secp160r1 implementation against known test vectors.
 *
 * @return true if the test passes, false otherwise.
 */
bool run_test();

} // namespace Ecoflow_mbedtls_test

#endif // ECOFLOW_MBEDTLS_TEST_H
