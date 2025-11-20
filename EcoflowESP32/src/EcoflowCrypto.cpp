#include <Arduino.h>
#include <cstring>
#include "EcoflowCrypto.h"
#include "Credentials.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mbedtls/md.h"

void print_hex(const uint8_t* data, size_t size, const char* label) {
    Serial.printf("%s: ", label);
    for (size_t i = 0; i < size; i++) {
        Serial.printf("%02x", data[i]);
    }
    Serial.println();
}


// secp160r1 curve parameters
#define SECP160R1_P "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF"
#define SECP160R1_A "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFC"
#define SECP160R1_B "1C97BEFC54BD7A8B65ACF89F81D4D4ADC565FA45"
#define SECP160R1_GX "4A96B5688EF573284664698968C38BB913CBFC82"
#define SECP160R1_GY "23A628553168947D59DCC912042351377AC5FB32"
#define SECP160R1_N "01000000000000000001F4C8F927AED3CA752257"

EcoflowCrypto::EcoflowCrypto() {
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Q);
    mbedtls_aes_init(&aes_ctx);

    // Load custom secp160r1 curve.
    // In this version of mbedtls, direct struct access is the intended way to load custom curves.
    if (mbedtls_mpi_read_string(&grp.P, 16, SECP160R1_P) != 0 ||
        mbedtls_mpi_read_string(&grp.A, 16, SECP160R1_A) != 0 ||
        mbedtls_mpi_read_string(&grp.B, 16, SECP160R1_B) != 0 ||
        mbedtls_ecp_point_read_string(&grp.G, 16, SECP160R1_GX, SECP160R1_GY) != 0 ||
        mbedtls_mpi_read_string(&grp.N, 16, SECP160R1_N) != 0) {
        ESP_LOGE("EcoflowCrypto", "Failed to load custom curve parameters");
        return;
    }

    grp.pbits = 160;
    grp.nbits = 160;
    grp.id = MBEDTLS_ECP_DP_NONE; // Use DP_NONE for custom curves
}

EcoflowCrypto::~EcoflowCrypto() {
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&Q);
    mbedtls_aes_free(&aes_ctx);
}

int EcoflowCrypto::f_rng(void* p_rng, unsigned char* output, size_t output_len) {
    esp_fill_random(output, output_len);
    return 0;
}

bool EcoflowCrypto::generate_keys() {
    if (mbedtls_ecp_gen_keypair(&grp, &d, &Q, f_rng, nullptr) != 0) {
        return false;
    }

    // mbedtls writes a 41-byte key (0x04 prefix + X + Y), but the device expects a 40-byte raw key (X + Y).
    uint8_t temp_pub_key[41];
    size_t pub_len;
    if (mbedtls_ecp_point_write_binary(&grp, &Q, MBEDTLS_ECP_PF_UNCOMPRESSED, &pub_len, temp_pub_key, sizeof(temp_pub_key)) != 0) {
        return false;
    }

    if (pub_len == 41) {
        memcpy(public_key, temp_pub_key + 1, 40);
        print_hex(public_key, sizeof(public_key), "Public Key");
        return true;
    }

    return false;
}

bool EcoflowCrypto::compute_shared_secret(const uint8_t* peer_pub_key, size_t peer_pub_key_len) {
    mbedtls_ecp_point peer_Q;
    mbedtls_ecp_point_init(&peer_Q);
    mbedtls_ecp_point shared_P;
    mbedtls_ecp_point_init(&shared_P);

    if (mbedtls_ecp_point_read_binary(&grp, &peer_Q, peer_pub_key, peer_pub_key_len) != 0) {
        mbedtls_ecp_point_free(&peer_Q);
        mbedtls_ecp_point_free(&shared_P);
        return false;
    }

    if (mbedtls_ecp_mul(&grp, &shared_P, &d, &peer_Q, f_rng, nullptr) != 0) {
        mbedtls_ecp_point_free(&peer_Q);
        mbedtls_ecp_point_free(&shared_P);
        return false;
    }

    unsigned char buf[41];
    size_t len;
    mbedtls_ecp_point_write_binary(&grp, &shared_P, MBEDTLS_ECP_PF_UNCOMPRESSED, &len, buf, sizeof(buf));

    // The shared secret is the 20-byte x-coordinate of the resulting point.
    uint8_t full_shared_secret[20];
    memcpy(full_shared_secret, buf + 1, sizeof(full_shared_secret));

    // The IV is the MD5 hash of the full 20-byte shared secret.
    mbedtls_md5(full_shared_secret, sizeof(full_shared_secret), iv);

    // The final AES key is the first 16 bytes of the shared secret.
    memcpy(shared_secret, full_shared_secret, 16);

    print_hex(shared_secret, 16, "Shared Secret (AES Key)");
    print_hex(iv, 16, "IV");

    mbedtls_ecp_point_free(&peer_Q);
    mbedtls_ecp_point_free(&shared_P);
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
    print_hex(session_key, 16, "Session Key");
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

void EcoflowCrypto::decrypt_shared(const uint8_t* input, size_t input_len, uint8_t* output) {
    mbedtls_aes_setkey_dec(&aes_ctx, shared_secret, 128);
    uint8_t temp_iv[16];
    memcpy(temp_iv, iv, 16);
    mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, input_len, temp_iv, input, output);
}