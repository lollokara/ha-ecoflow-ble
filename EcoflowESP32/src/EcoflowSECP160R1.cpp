#include "EcoflowSECP160R1.h"
#include <uECC.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <cstring>

namespace EcoflowSECP160R1 {

static int rng_function(uint8_t *dest, unsigned size) {
    static mbedtls_entropy_context entropy;
    static mbedtls_ctr_drbg_context ctr_drbg;
    static bool initialized = false;
    if (!initialized) {
        mbedtls_ctr_drbg_init(&ctr_drbg);
        mbedtls_entropy_init(&entropy);
        const char* pers = "ecoflow_esp32";
        mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char*)pers, strlen(pers));
        initialized = true;
    }
    return mbedtls_ctr_drbg_random(&ctr_drbg, dest, size) == 0;
}

int make_key(uint8_t* public_key, uint8_t* private_key) {
    const struct uECC_Curve_t* curve = uECC_secp160r1();
    uECC_set_rng(&rng_function);
    return uECC_make_key(public_key, private_key, curve);
}

int shared_secret(const uint8_t* public_key, const uint8_t* private_key, uint8_t* secret) {
    const struct uECC_Curve_t* curve = uECC_secp160r1();
    return uECC_shared_secret(public_key, private_key, secret, curve);
}

} // namespace EcoflowSECP160R1
