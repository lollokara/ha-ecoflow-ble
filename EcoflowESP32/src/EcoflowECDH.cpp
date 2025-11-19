#include "EcoflowECDH.h"
#include "Ecoflow_mbedtls.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/md5.h"
extern "C" {
#include "Credentials.h"
}
#include <cstring>

namespace EcoflowECDH {

// Helper to initialize the RNG
static int init_rng(mbedtls_ctr_drbg_context *ctr_drbg, mbedtls_entropy_context *entropy) {
    const char *pers = "ecflow_esp32";
    mbedtls_entropy_init(entropy);
    mbedtls_ctr_drbg_init(ctr_drbg);
    return mbedtls_ctr_drbg_seed(ctr_drbg, mbedtls_entropy_func, entropy,
                               (const unsigned char *)pers, strlen(pers));
}

void generate_public_key(uint8_t* buf, size_t* len, uint8_t* private_key_out) {
    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point q;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    int ret = 0;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&q);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    if (init_rng(&ctr_drbg, &entropy) != 0) {
        goto cleanup;
    }

    ret = Ecoflow_mbedtls::load_secp160r1_group(&grp);
    if(ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_ecdh_gen_public(&grp, &d, &q, mbedtls_ctr_drbg_random, &ctr_drbg);
    if(ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_mpi_write_binary(&d, private_key_out, 21);
    if(ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_ecp_point_write_binary(&grp, &q,
                                         MBEDTLS_ECP_PF_UNCOMPRESSED, len,
                                         buf, 41);

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&q);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

void compute_shared_secret(const uint8_t* peer_pub_key, size_t peer_len, uint8_t* shared_secret_out, const uint8_t* private_key) {
    mbedtls_ecp_group grp;
    mbedtls_mpi d, z;
    mbedtls_ecp_point qp;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    int ret = 0;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&z);
    mbedtls_ecp_point_init(&qp);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    if (init_rng(&ctr_drbg, &entropy) != 0) {
        goto cleanup;
    }

    ret = Ecoflow_mbedtls::load_secp160r1_group(&grp);
    if(ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_mpi_read_binary(&d, private_key, 21);
    if(ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_ecp_point_read_binary(&grp, &qp, peer_pub_key, peer_len);
    if(ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_ecdh_compute_shared(&grp, &z, &qp, &d,
                                      mbedtls_ctr_drbg_random, &ctr_drbg);
    if(ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_mpi_write_binary(&z, shared_secret_out, 20);

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_mpi_free(&z);
    mbedtls_ecp_point_free(&qp);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

void generateSessionKey(const uint8_t* sRand, const uint8_t* seed, uint8_t* sessionKey) {
    uint8_t data[32];

    int pos = seed[0] * 16 + (seed[1] - 1) * 256;

    memcpy(data, &ECOFLOW_KEYDATA[pos], 8);
    memcpy(data + 8, &ECOFLOW_KEYDATA[pos + 8], 8);
    memcpy(data + 16, sRand, 16);

    mbedtls_md5(data, 32, sessionKey);
}

} // namespace EcoflowECDH
