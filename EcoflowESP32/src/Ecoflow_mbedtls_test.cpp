#include "Ecoflow_mbedtls_test.h"
#include "Ecoflow_mbedtls.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include <Arduino.h>

namespace Ecoflow_mbedtls_test {

const uint8_t PRIVATE_KEY[] = {
    0x00, 0x95, 0xa8, 0x98, 0x4a, 0xb8, 0x20, 0x80, 0x58, 0xa2, 0xc4, 0xd5, 0x98, 0xca, 0x42, 0xc8, 0x88, 0x25, 0xaa, 0xe9, 0x1e
};
const uint8_t PUBLIC_KEY[] = {
    0x04, 0x55, 0xd0, 0xa3, 0x55, 0xd6, 0x61, 0x5e, 0x86, 0x74, 0x0c, 0xf3, 0x48, 0x11, 0x52, 0x77, 0xf1, 0x2f, 0xae, 0xf2, 0xd4, 0x35, 0x51, 0xae, 0x29, 0x91, 0x7c, 0xa5, 0x61, 0xe7, 0x63, 0xad, 0x0e, 0x2b, 0xa5, 0xca, 0xe4, 0x0a, 0x7f, 0xd5, 0x5a
};
const uint8_t PEER_PUBLIC_KEY[] = {
    0x04, 0xef, 0x56, 0x52, 0xcd, 0xda, 0x87, 0x3c, 0xdf, 0x6d, 0xbf, 0x76, 0x3b, 0x4f, 0x97, 0xdc, 0xf1, 0x75, 0x55, 0xbc, 0x91, 0xec, 0x35, 0x11, 0x5c, 0xe6, 0x68, 0xf2, 0x5c, 0x67, 0x54, 0xb8, 0xc9, 0x26, 0xce, 0xb5, 0xd3, 0x1d, 0x02, 0xdf, 0x4f
};
const uint8_t SHARED_SECRET[] = {
    0xb4, 0xe6, 0x6a, 0xc9, 0x58, 0x6d, 0xff, 0xd2, 0x10, 0xd9, 0xd9, 0x36, 0x57, 0x99, 0xec, 0xfe, 0xcf, 0x71, 0xaf, 0xaa
};

bool run_test() {
    mbedtls_ecp_group grp;
    mbedtls_mpi d, z;
    mbedtls_ecp_point Q, Qp;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "ecdh_test";
    int ret;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&z);
    mbedtls_ecp_point_init(&Q);
    mbedtls_ecp_point_init(&Qp);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    Serial.println("Running mbedtls secp160r1 test...");

    // 1. Load the custom curve
    ret = Ecoflow_mbedtls::load_secp160r1_group(&grp);
    if (ret != 0) {
        Serial.printf("Failed to load secp160r1 group: %d\n", ret);
        return false;
    }

    // 2. Load the private key
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&d, PRIVATE_KEY, sizeof(PRIVATE_KEY)));

    // 3. Generate the public key
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, mbedtls_ctr_drbg_random, &ctr_drbg));

    // 4. Verify the public key
    uint8_t public_key_buf[41];
    size_t public_key_len;
    MBEDTLS_MPI_CHK(mbedtls_ecp_point_write_binary(&grp, &Q, MBEDTLS_ECP_PF_UNCOMPRESSED, &public_key_len, public_key_buf, sizeof(public_key_buf)));
    if (memcmp(public_key_buf, PUBLIC_KEY, sizeof(PUBLIC_KEY)) != 0) {
        Serial.println("Public key mismatch!");
        return false;
    }

    // 5. Load the peer's public key
    MBEDTLS_MPI_CHK(mbedtls_ecp_point_read_binary(&grp, &Qp, PEER_PUBLIC_KEY, sizeof(PEER_PUBLIC_KEY)));

    // 6. Compute the shared secret
    MBEDTLS_MPI_CHK(mbedtls_ecdh_compute_shared(&grp, &z, &Qp, &d, mbedtls_ctr_drbg_random, &ctr_drbg));

    // 7. Verify the shared secret
    uint8_t shared_secret_buf[20];
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&z, shared_secret_buf, sizeof(shared_secret_buf)));
    if (memcmp(shared_secret_buf, SHARED_SECRET, sizeof(SHARED_SECRET)) != 0) {
        Serial.println("Shared secret mismatch!");
        return false;
    }

    Serial.println("mbedtls secp160r1 test passed!");
    return true;

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_mpi_free(&z);
    mbedtls_ecp_point_free(&Q);
    mbedtls_ecp_point_free(&Qp);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    return false;
}

} // namespace Ecoflow_mbedtls_test
