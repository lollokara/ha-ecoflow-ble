#ifndef ECOFLOW_CRYPTO_H
#define ECOFLOW_CRYPTO_H

#include <stdint.h>
#include "mbedtls/ecdh.h"
#include "mbedtls/md5.h"
#include "mbedtls/aes.h"

class EcoflowCrypto {
public:
    EcoflowCrypto();
    ~EcoflowCrypto();

    bool generate_keys();
    bool compute_shared_secret(const uint8_t* peer_pub_key, size_t peer_pub_key_len);
    void generate_session_key(const uint8_t* seed, const uint8_t* srand);
    void encrypt_session(const uint8_t* input, size_t input_len, uint8_t* output);
    void decrypt_session(const uint8_t* input, size_t input_len, uint8_t* output);

    uint8_t* get_public_key() { return public_key; }
    uint8_t* get_shared_secret() { return shared_secret; }
    uint8_t* get_session_key() { return session_key; }
    uint8_t* get_iv() { return iv; }

private:
    mbedtls_ecdh_context ecdh_ctx;
    mbedtls_aes_context aes_ctx;

    uint8_t public_key[41];
    uint8_t private_key[20];
    uint8_t shared_secret[20];
    uint8_t session_key[16];
    uint8_t iv[16];

    static int f_rng(void* p_rng, unsigned char* output, size_t output_len);
};

#endif // ECOFLOW_CRYPTO_H