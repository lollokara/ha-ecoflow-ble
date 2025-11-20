#ifndef ECOFLOW_CRYPTO_H
#define ECOFLOW_CRYPTO_H

#include <stdint.h>
#include <vector>
#include "mbedtls/ecdh.h"
#include "mbedtls/md5.h"
#include "mbedtls/aes.h"
#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"

class EcoflowCrypto {
public:
    EcoflowCrypto();
    ~EcoflowCrypto();

    bool generate_keys();
    bool compute_shared_secret(const uint8_t* peer_pub_key, size_t peer_pub_key_len);
    void generate_session_key(const uint8_t* seed, const uint8_t* srand);
    
    std::vector<uint8_t> encrypt_session(const std::vector<uint8_t>& input);
    std::vector<uint8_t> decrypt_session(const std::vector<uint8_t>& input);

    uint8_t* get_public_key() { return public_key; }
    uint8_t* get_shared_secret() { return shared_secret; }
    uint8_t* get_session_key() { return session_key; }
    uint8_t* get_iv() { return iv; }

private:
    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point Q;
    mbedtls_aes_context aes_ctx;

    uint8_t public_key[41];
    uint8_t shared_secret[20];
    uint8_t session_key[16];
    uint8_t iv[16];

    static int f_rng(void* p_rng, unsigned char* output, size_t output_len);
    
    // PKCS7 Padding functions
    std::vector<uint8_t> pad(const std::vector<uint8_t>& data);
    std::vector<uint8_t> unpad(const std::vector<uint8_t>& data);
};

#endif // ECOFLOW_CRYPTO_H