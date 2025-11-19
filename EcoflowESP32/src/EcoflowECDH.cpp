#include "EcoflowProtocol.h"
#include "uECC.h"
#include <mbedtls/md5.h>
#include <cstring>
#include "EcoflowSECP160R1.h"

namespace EcoflowECDH {

void generate_public_key(uint8_t* buf, size_t* len, uint8_t* private_key) {
    EcoflowSECP160R1::make_key(buf, private_key);
    *len = 42; // SECP160R1 public key size
}

void compute_shared_secret(const uint8_t* peer_pub_key, size_t peer_len, uint8_t* shared_secret, const uint8_t* private_key) {
    EcoflowSECP160R1::shared_secret(peer_pub_key, private_key, shared_secret);

    uint8_t iv[16];
    mbedtls_md5(shared_secret, 20, iv);
    memcpy(shared_secret + 20, iv, 16);
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
