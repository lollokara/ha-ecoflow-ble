#include "EcoflowECDH.h"
#include <Arduino.h>
#include "mbedtls/md5.h"
#include "esp_system.h" // For esp_random

extern "C" {
#include "uECC.h"
}

extern "C" {
#include "Credentials.h"
}
#include <cstring>

namespace EcoflowECDH {

// RNG function for uECC
static int ecoflow_uECC_rng(uint8_t *dest, unsigned size) {
    esp_fill_random(dest, size);
    return 1;
}

bool generate_public_key(uint8_t* public_key, uint8_t* private_key) {
    const struct ecoflow_uECC_Curve_t * curve = ecoflow_uECC_secp160r1();
    ecoflow_uECC_set_rng(&ecoflow_uECC_rng);

    if (!ecoflow_uECC_make_key(public_key, private_key, curve)) {
        Serial.println("ecoflow_uECC_make_key() failed");
        return false;
    }
    return true;
}

bool compute_shared_secret(const uint8_t* peer_pub_key, uint8_t* shared_secret, const uint8_t* private_key) {
    const struct ecoflow_uECC_Curve_t * curve = ecoflow_uECC_secp160r1();
    if (!ecoflow_uECC_shared_secret(peer_pub_key, private_key, shared_secret, curve)) {
        Serial.println("ecoflow_uECC_shared_secret() failed");
        return false;
    }
    return true;
}

void generateSessionKey(const uint8_t* seed, const uint8_t* shared_secret, uint8_t* sessionKey) {
    uint8_t data[16 + 20]; // 16 from keydata, 20 from shared secret
    int pos = seed[0] * 16 + (seed[1] - 1) * 256;
    memcpy(data, &ECOFLOW_KEYDATA[pos], 16);
    memcpy(data + 16, shared_secret, 20);
    mbedtls_md5(data, sizeof(data), sessionKey);
}

} // namespace EcoflowECDH
