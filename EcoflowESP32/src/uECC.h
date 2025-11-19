#ifndef _ECOFLOW_UECC_H_
#define _ECOFLOW_UECC_H_

#include <stdint.h>

#define ecoflow_uECC_arch_other 0
#define ecoflow_uECC_x86        1
#define ecoflow_uECC_x86_64     2
#define ecoflow_uECC_arm        3
#define ecoflow_uECC_arm_thumb  4
#define ecoflow_uECC_arm_thumb2 5
#define ecoflow_uECC_arm64      6
#define ecoflow_uECC_avr        7

#ifndef ecoflow_uECC_OPTIMIZATION_LEVEL
    #define ecoflow_uECC_OPTIMIZATION_LEVEL 2
#endif

#ifndef ecoflow_uECC_SQUARE_FUNC
    #define ecoflow_uECC_SQUARE_FUNC 0
#endif

#ifndef ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    #define ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN 0
#endif

#ifndef ecoflow_uECC_SUPPORTS_secp160r1
    #define ecoflow_uECC_SUPPORTS_secp160r1 1
#endif
#ifndef ecoflow_uECC_SUPPORTS_secp192r1
    #define ecoflow_uECC_SUPPORTS_secp192r1 1
#endif
#ifndef ecoflow_uECC_SUPPORTS_secp224r1
    #define ecoflow_uECC_SUPPORTS_secp224r1 1
#endif
#ifndef ecoflow_uECC_SUPPORTS_secp256r1
    #define ecoflow_uECC_SUPPORTS_secp256r1 1
#endif
#ifndef ecoflow_uECC_SUPPORTS_secp256k1
    #define ecoflow_uECC_SUPPORTS_secp256k1 1
#endif

#ifndef ecoflow_uECC_SUPPORT_COMPRESSED_POINT
    #define ecoflow_uECC_SUPPORT_COMPRESSED_POINT 1
#endif

struct ecoflow_uECC_Curve_t;
typedef const struct ecoflow_uECC_Curve_t * ecoflow_uECC_Curve;

#ifdef __cplusplus
extern "C"
{
#endif

#if ecoflow_uECC_SUPPORTS_secp160r1
ecoflow_uECC_Curve ecoflow_uECC_secp160r1(void);
#endif
#if ecoflow_uECC_SUPPORTS_secp192r1
ecoflow_uECC_Curve ecoflow_uECC_secp192r1(void);
#endif
#if ecoflow_uECC_SUPPORTS_secp224r1
ecoflow_uECC_Curve ecoflow_uECC_secp224r1(void);
#endif
#if ecoflow_uECC_SUPPORTS_secp256r1
ecoflow_uECC_Curve ecoflow_uECC_secp256r1(void);
#endif
#if ecoflow_uECC_SUPPORTS_secp256k1
ecoflow_uECC_Curve ecoflow_uECC_secp256k1(void);
#endif

typedef int (*ecoflow_uECC_RNG_Function)(uint8_t *dest, unsigned size);

void ecoflow_uECC_set_rng(ecoflow_uECC_RNG_Function rng_function);
ecoflow_uECC_RNG_Function ecoflow_uECC_get_rng(void);
int ecoflow_uECC_curve_private_key_size(ecoflow_uECC_Curve curve);
int ecoflow_uECC_curve_public_key_size(ecoflow_uECC_Curve curve);
int ecoflow_uECC_make_key(uint8_t *public_key, uint8_t *private_key, ecoflow_uECC_Curve curve);
int ecoflow_uECC_shared_secret(const uint8_t *public_key,
                       const uint8_t *private_key,
                       uint8_t *secret,
                       ecoflow_uECC_Curve curve);

#if ecoflow_uECC_SUPPORT_COMPRESSED_POINT
void ecoflow_uECC_compress(const uint8_t *public_key, uint8_t *compressed, ecoflow_uECC_Curve curve);
void ecoflow_uECC_decompress(const uint8_t *compressed, uint8_t *public_key, ecoflow_uECC_Curve curve);
#endif

int ecoflow_uECC_valid_public_key(const uint8_t *public_key, ecoflow_uECC_Curve curve);
int ecoflow_uECC_compute_public_key(const uint8_t *private_key, uint8_t *public_key, ecoflow_uECC_Curve curve);
int ecoflow_uECC_sign(const uint8_t *private_key,
              const uint8_t *message_hash,
              unsigned hash_size,
              uint8_t *signature,
              ecoflow_uECC_Curve curve);

typedef struct ecoflow_uECC_HashContext {
    void (*init_hash)(const struct ecoflow_uECC_HashContext *context);
    void (*update_hash)(const struct ecoflow_uECC_HashContext *context,
                        const uint8_t *message,
                        unsigned message_size);
    void (*finish_hash)(const struct ecoflow_uECC_HashContext *context, uint8_t *hash_result);
    unsigned block_size;
    unsigned result_size;
    uint8_t *tmp;
} ecoflow_uECC_HashContext;

int ecoflow_uECC_sign_deterministic(const uint8_t *private_key,
                            const uint8_t *message_hash,
                            unsigned hash_size,
                            const ecoflow_uECC_HashContext *hash_context,
                            uint8_t *signature,
                            ecoflow_uECC_Curve curve);

int ecoflow_uECC_verify(const uint8_t *public_key,
                const uint8_t *message_hash,
                unsigned hash_size,
                const uint8_t *signature,
                ecoflow_uECC_Curve curve);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* _ECOFLOW_UECC_H_ */
