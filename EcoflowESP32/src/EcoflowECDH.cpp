#include "EcoflowProtocol.h"
#include <mbedtls/ecdh.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/md5.h>
#include <cstring>

namespace EcoflowECDH {

static mbedtls_ecdh_context ecdh_ctx;
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;

void init() {
    mbedtls_ecdh_init(&ecdh_ctx);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    const char* pers = "ecoflow_esp32";
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char*)pers, strlen(pers));

    mbedtls_ecp_group_load(&ecdh_ctx.grp, MBEDTLS_ECP_DP_SECP192R1);
}

void generate_public_key(uint8_t* buf, size_t* len) {
    mbedtls_ecdh_gen_public(&ecdh_ctx.grp, &ecdh_ctx.d, &ecdh_ctx.Q, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ecp_point_write_binary(&ecdh_ctx.grp, &ecdh_ctx.Q, MBEDTLS_ECP_PF_UNCOMPRESSED, len, buf, 100);
}

void compute_shared_secret(const uint8_t* peer_pub_key, size_t peer_len, uint8_t* shared_secret) {
    mbedtls_ecp_point peer_Q;
    mbedtls_ecp_point_init(&peer_Q);
    mbedtls_ecp_point_read_binary(&ecdh_ctx.grp, &peer_Q, peer_pub_key, peer_len);

    size_t shared_secret_len;
    unsigned char shared_secret_buf[24]; // SECP192R1 uses 24 bytes for x-coord
    mbedtls_ecdh_calc_secret(&ecdh_ctx, &shared_secret_len, shared_secret_buf, sizeof(shared_secret_buf), mbedtls_ctr_drbg_random, &ctr_drbg);

    memcpy(shared_secret, shared_secret_buf, 24);

    uint8_t iv[16];
    mbedtls_md5(shared_secret, 24, iv);
    memcpy(shared_secret + 24, iv, 16);

    mbedtls_ecp_point_free(&peer_Q);
}

void generateSessionKey(const uint8_t* sRand, const uint8_t* seed, uint8_t* sessionKey, uint8_t* iv) {
    uint8_t data[32];
    int pos = seed[0] * 0x10 + ((seed[1] - 1) & 0xFF) * 0x100;
    memcpy(data, EcoflowKeyData::get8bytes(pos), 8);
    memcpy(data + 8, EcoflowKeyData::get8bytes(pos + 8), 8);
    memcpy(data + 16, sRand, 16);

    mbedtls_md5(data, 32, sessionKey);

    uint8_t iv_input[4] = {seed[0], seed[1], seed[0], seed[1]};
    mbedtls_md5(iv_input, 4, iv);
}

} // namespace EcoflowECDH
