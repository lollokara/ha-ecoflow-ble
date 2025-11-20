#include <Arduino.h>
#include "EcoflowCrypto.h"
#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"
#include "mbedtls/md.h"
#include "mbedtls/md5.h"
#include "mbedtls/aes.h"
#include "Credentials.h"
#include "esp_log.h"
#include "esp_system.h"
#include <cstring>

// secp160r1 curve parameters
#define SECP160R1_P "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF"
#define SECP160R1_A "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFC"
#define SECP160R1_B "1C97BEFC54BD7A8B65ACF89F81D4D4ADC565FA45"
#define SECP160R1_GX "4A96B5688EF573284664698968C38BB913CBFC82"
#define SECP160R1_GY "23A628553168947D59DCC912042351377AC5FB32"
#define SECP160R1_N "01000000000000000001F4C8F927AED3CA752257"

EcoflowCrypto::EcoflowCrypto() {
    mbedtls_ecdh_init(&ecdh_ctx);
    mbedtls_aes_init(&aes_ctx);
    memset(public_key, 0, sizeof(public_key));
    memset(private_key, 0, sizeof(private_key));
    memset(shared_secret, 0, sizeof(shared_secret));
    memset(session_key, 0, sizeof(session_key));
    memset(iv, 0, sizeof(iv));

    mbedtls_ecp_group_init(&ecdh_ctx.grp);
    mbedtls_mpi_init(&ecdh_ctx.d);
    mbedtls_ecp_point_init(&ecdh_ctx.Q);
    mbedtls_ecp_point_init(&ecdh_ctx.Qp);
    mbedtls_mpi_init(&ecdh_ctx.z);

    // Load custom curve
    mbedtls_mpi_read_string(&ecdh_ctx.grp.P, 16, SECP160R1_P);
    mbedtls_mpi_read_string(&ecdh_ctx.grp.A, 16, SECP160R1_A);
    mbedtls_mpi_read_string(&ecdh_ctx.grp.B, 16, SECP160R1_B);
    mbedtls_mpi_read_string(&ecdh_ctx.grp.G.X, 16, SECP160R1_GX);
    mbedtls_mpi_read_string(&ecdh_ctx.grp.G.Y, 16, SECP160R1_GY);
    mbedtls_mpi_lset(&ecdh_ctx.grp.G.Z, 1);
    mbedtls_mpi_read_string(&ecdh_ctx.grp.N, 16, SECP160R1_N);
    ecdh_ctx.grp.pbits = 160;
    ecdh_ctx.grp.nbits = 160;
    // Note: Using a placeholder ID for a custom curve is not ideal, but mbedtls does not
    // offer a standard way to register a fully custom curve. This works because the
    // underlying arithmetic functions do not rely on the ID for secp192r1.
    ecdh_ctx.grp.id = MBEDTLS_ECP_DP_SECP192R1;
}

EcoflowCrypto::~EcoflowCrypto() {
    mbedtls_ecdh_free(&ecdh_ctx);
    mbedtls_aes_free(&aes_ctx);
}

int EcoflowCrypto::f_rng(void* p_rng, unsigned char* output, size_t output_len) {
    esp_fill_random(output, output_len);
    return 0;
}

bool EcoflowCrypto::generate_keys() {
    if (mbedtls_ecdh_gen_public(&ecdh_ctx.grp, &ecdh_ctx.d, &ecdh_ctx.Q, f_rng, nullptr) != 0) {
        return false;
    }
    size_t pub_len;
    return mbedtls_ecp_point_write_binary(&ecdh_ctx.grp, &ecdh_ctx.Q, MBEDTLS_ECP_PF_UNCOMPRESSED, &pub_len, public_key, sizeof(public_key)) == 0;
}

bool EcoflowCrypto::compute_shared_secret(const uint8_t* peer_pub_key, size_t peer_pub_key_len) {
    mbedtls_ecp_point peer_Q;
    mbedtls_ecp_point_init(&peer_Q);

    if (mbedtls_ecp_point_read_binary(&ecdh_ctx.grp, &peer_Q, peer_pub_key, peer_pub_key_len) != 0) {
        mbedtls_ecp_point_free(&peer_Q);
        return false;
    }

    if (mbedtls_ecdh_compute_shared(&ecdh_ctx.grp, &ecdh_ctx.z, &peer_Q, &ecdh_ctx.d, f_rng, nullptr) != 0) {
        mbedtls_ecp_point_free(&peer_Q);
        return false;
    }

    mbedtls_mpi_write_binary(&ecdh_ctx.z, shared_secret, sizeof(shared_secret));
    mbedtls_md5(shared_secret, sizeof(shared_secret), iv);

    // The shared secret is 20 bytes, but the AES key must be 16 bytes.
    // The Python implementation truncates the secret, so we do the same here.
    memcpy(shared_secret, shared_secret, 16);

    mbedtls_ecp_point_free(&peer_Q);
    return true;
}

void EcoflowCrypto::generate_session_key(const uint8_t* seed, const uint8_t* srand) {
    uint8_t data[32];
    uint64_t data_num[4];

    int pos = seed[0] * 0x10 + ((seed[1] - 1) & 0xFF) * 0x100;

    memcpy(&data_num[0], &ECOFLOW_KEYDATA[pos], 8);
    memcpy(&data_num[1], &ECOFLOW_KEYDATA[pos + 8], 8);
    memcpy(&data_num[2], &srand[0], 8);
    memcpy(&data_num[3], &srand[8], 8);

    memcpy(data, &data_num[0], 8);
    memcpy(data + 8, &data_num[1], 8);
    memcpy(data + 16, &data_num[2], 8);
    memcpy(data + 24, &data_num[3], 8);

    mbedtls_md5(data, sizeof(data), session_key);
}

void EcoflowCrypto::encrypt_session(const uint8_t* input, size_t input_len, uint8_t* output) {
    mbedtls_aes_setkey_enc(&aes_ctx, session_key, 128);
    uint8_t temp_iv[16];
    memcpy(temp_iv, iv, 16);
    mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, input_len, temp_iv, input, output);
}

void EcoflowCrypto::decrypt_session(const uint8_t* input, size_t input_len, uint8_t* output) {
    mbedtls_aes_setkey_dec(&aes_ctx, session_key, 128);
    uint8_t temp_iv[16];
    memcpy(temp_iv, iv, 16);
    mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, input_len, temp_iv, input, output);
}