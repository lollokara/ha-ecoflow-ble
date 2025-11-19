/* Copyright 2014, Kenneth MacKay. Licensed under the BSD 2-clause license. */

#ifndef _MICRO_ECC_H_
#define _MICRO_ECC_H_

#include <stdint.h>

/* Platform selection options.
If ef_uECC_PLATFORM is not defined, the code will try to guess it based on compiler macros.
Possible values for ef_uECC_PLATFORM are defined below: */
#define ef_uECC_arch_other 0
#define ef_uECC_x86        1
#define ef_uECC_x86_64     2
#define ef_uECC_arm        3
#define ef_uECC_arm_thumb  4
#define ef_uECC_avr        5
#define ef_uECC_arm_thumb2 6

/* If desired, you can define ef_uECC_WORD_SIZE as appropriate for your platform (1, 4, or 8 bytes).
If ef_uECC_WORD_SIZE is not explicitly defined then it will be automatically set based on your
platform. */

/* Inline assembly options.
ef_uECC_asm_none  - Use standard C99 only.
ef_uECC_asm_small - Use GCC inline assembly for the target platform (if available), optimized for
                 minimum size.
ef_uECC_asm_fast  - Use GCC inline assembly optimized for maximum speed. */
#define ef_uECC_asm_none  0
#define ef_uECC_asm_small 1
#define ef_uECC_asm_fast  2
#ifndef ef_uECC_ASM
    #define ef_uECC_ASM ef_uECC_asm_fast
#endif

/* Curve selection options. */
#define ef_uECC_secp160r1 1
#define ef_uECC_secp192r1 2
#define ef_uECC_secp256r1 3
#define ef_uECC_secp256k1 4
#define ef_uECC_secp224r1 5
#ifndef ef_uECC_CURVE
    #define ef_uECC_CURVE ef_uECC_secp160r1
#endif

/* ef_uECC_SQUARE_FUNC - If enabled (defined as nonzero), this will cause a specific function to be
used for (scalar) squaring instead of the generic multiplication function. This will make things
faster by about 8% but increases the code size. */
#ifndef ef_uECC_SQUARE_FUNC
    #define ef_uECC_SQUARE_FUNC 1
#endif

#define ef_uECC_CONCAT1(a, b) a##b
#define ef_uECC_CONCAT(a, b) ef_uECC_CONCAT1(a, b)

#define ef_uECC_size_1 20 /* secp160r1 */
#define ef_uECC_size_2 24 /* secp192r1 */
#define ef_uECC_size_3 32 /* secp256r1 */
#define ef_uECC_size_4 32 /* secp256k1 */
#define ef_uECC_size_5 28 /* secp224r1 */

#define ef_uECC_BYTES ef_uECC_CONCAT(ef_uECC_size_, ef_uECC_CURVE)

#ifdef __cplusplus
extern "C"
{
#endif

/* ef_uECC_RNG_Function type
The RNG function should fill 'size' random bytes into 'dest'. It should return 1 if
'dest' was filled with random data, or 0 if the random data could not be generated.
The filled-in values should be either truly random, or from a cryptographically-secure PRNG.

A correctly functioning RNG function must be set (using ef_uECC_set_rng()) before calling
ef_uECC_make_key() or ef_uECC_sign().

Setting a correctly functioning RNG function improves the resistance to side-channel attacks
for ef_uECC_shared_secret() and ef_uECC_sign_deterministic().

A correct RNG function is set by default when building for Windows, Linux, or OS X.
If you are building on another POSIX-compliant system that supports /dev/random or /dev/urandom,
you can define ef_uECC_POSIX to use the predefined RNG. For embedded platforms there is no predefined
RNG function; you must provide your own.
*/
typedef int (*ef_uECC_RNG_Function)(uint8_t *dest, unsigned size);

/* ef_uECC_set_rng() function.
Set the function that will be used to generate random bytes. The RNG function should
return 1 if the random data was generated, or 0 if the random data could not be generated.

On platforms where there is no predefined RNG function (eg embedded platforms), this must
be called before ef_uECC_make_key() or ef_uECC_sign() are used.

Inputs:
    rng_function - The function that will be used to generate random bytes.
*/
void ef_uECC_set_rng(ef_uECC_RNG_Function rng_function);

/* ef_uECC_make_key() function.
Create a public/private key pair.

Outputs:
    public_key  - Will be filled in with the public key.
    private_key - Will be filled in with the private key.

Returns 1 if the key pair was generated successfully, 0 if an error occurred.
*/
int ef_uECC_make_key(uint8_t public_key[ef_uECC_BYTES*2], uint8_t private_key[ef_uECC_BYTES]);

/* ef_uECC_shared_secret() function.
Compute a shared secret given your secret key and someone else's public key.
Note: It is recommended that you hash the result of ef_uECC_shared_secret() before using it for
symmetric encryption or HMAC.

Inputs:
    public_key  - The public key of the remote party.
    private_key - Your private key.

Outputs:
    secret - Will be filled in with the shared secret value.

Returns 1 if the shared secret was generated successfully, 0 if an error occurred.
*/
int ef_uECC_shared_secret(const uint8_t public_key[ef_uECC_BYTES*2],
                       const uint8_t private_key[ef_uECC_BYTES],
                       uint8_t secret[ef_uECC_BYTES]);

/* ef_uECC_sign() function.
Generate an ECDSA signature for a given hash value.

Usage: Compute a hash of the data you wish to sign (SHA-2 is recommended) and pass it in to
this function along with your private key.

Inputs:
    private_key  - Your private key.
    message_hash - The hash of the message to sign.

Outputs:
    signature - Will be filled in with the signature value.

Returns 1 if the signature generated successfully, 0 if an error occurred.
*/
int ef_uECC_sign(const uint8_t private_key[ef_uECC_BYTES],
              const uint8_t message_hash[ef_uECC_BYTES],
              uint8_t signature[ef_uECC_BYTES*2]);

/* ef_uECC_HashContext structure.
This is used to pass in an arbitrary hash function to ef_uECC_sign_deterministic().
The structure will be used for multiple hash computations; each time a new hash
is computed, init_hash() will be called, followed by one or more calls to
update_hash(), and finally a call to finish_hash() to prudoce the resulting hash.

The intention is that you will create a structure that includes ef_uECC_HashContext
followed by any hash-specific data. For example:

typedef struct SHA256_HashContext {
    ef_uECC_HashContext uECC;
    SHA256_CTX ctx;
} SHA256_HashContext;

void init_SHA256(ef_uECC_HashContext *base) {
    SHA256_HashContext *context = (SHA256_HashContext *)base;
    SHA256_Init(&context->ctx);
}

void update_SHA256(ef_uECC_HashContext *base,
                   const uint8_t *message,
                   unsigned message_size) {
    SHA256_HashContext *context = (SHA256_HashContext *)base;
    SHA256_Update(&context->ctx, message, message_size);
}

void finish_SHA256(ef_uECC_HashContext *base, uint8_t *hash_result) {
    SHA256_HashContext *context = (SHA256_HashContext *)base;
    SHA256_Final(hash_result, &context->ctx);
}

... when signing ...
{
    uint8_t tmp[32 + 32 + 64];
    SHA256_HashContext ctx = {{&init_SHA256, &update_SHA256, &finish_SHA256, 64, 32, tmp}};
    ef_uECC_sign_deterministic(key, message_hash, &ctx.uECC, signature);
}
*/
typedef struct ef_uECC_HashContext {
    void (*init_hash)(struct ef_uECC_HashContext *context);
    void (*update_hash)(struct ef_uECC_HashContext *context,
                        const uint8_t *message,
                        unsigned message_size);
    void (*finish_hash)(struct ef_uECC_HashContext *context, uint8_t *hash_result);
    unsigned block_size; /* Hash function block size in bytes, eg 64 for SHA-256. */
    unsigned result_size; /* Hash function result size in bytes, eg 32 for SHA-256. */
    uint8_t *tmp; /* Must point to a buffer of at least (2 * result_size + block_size) bytes. */
} ef_uECC_HashContext;

/* ef_uECC_sign_deterministic() function.
Generate an ECDSA signature for a given hash value, using a deterministic algorithm
(see RFC 6979). You do not need to set the RNG using ef_uECC_set_rng() before calling
this function; however, if the RNG is defined it will improve resistance to side-channel
attacks.

Usage: Compute a hash of the data you wish to sign (SHA-2 is recommended) and pass it in to
this function along with your private key and a hash context.

Inputs:
    private_key  - Your private key.
    message_hash - The hash of the message to sign.
    hash_context - A hash context to use.

Outputs:
    signature - Will be filled in with the signature value.

Returns 1 if the signature generated successfully, 0 if an error occurred.
*/
int ef_uECC_sign_deterministic(const uint8_t private_key[ef_uECC_BYTES],
                            const uint8_t message_hash[ef_uECC_BYTES],
                            ef_uECC_HashContext *hash_context,
                            uint8_t signature[ef_uECC_BYTES*2]);

/* ef_uECC_verify() function.
Verify an ECDSA signature.

Usage: Compute the hash of the signed data using the same hash as the signer and
pass it to this function along with the signer's public key and the signature values (r and s).

Inputs:
    public_key - The signer's public key
    hash       - The hash of the signed data.
    signature  - The signature value.

Returns 1 if the signature is valid, 0 if it is invalid.
*/
int ef_uECC_verify(const uint8_t public_key[ef_uECC_BYTES*2],
                const uint8_t hash[ef_uECC_BYTES],
                const uint8_t signature[ef_uECC_BYTES*2]);

/* ef_uECC_compress() function.
Compress a public key.

Inputs:
    public_key - The public key to compress.

Outputs:
    compressed - Will be filled in with the compressed public key.
*/
void ef_uECC_compress(const uint8_t public_key[ef_uECC_BYTES*2], uint8_t compressed[ef_uECC_BYTES+1]);

/* ef_uECC_decompress() function.
Decompress a compressed public key.

Inputs:
    compressed - The compressed public key.

Outputs:
    public_key - Will be filled in with the decompressed public key.
*/
void ef_uECC_decompress(const uint8_t compressed[ef_uECC_BYTES+1], uint8_t public_key[ef_uECC_BYTES*2]);

/* ef_uECC_valid_public_key() function.
Check to see if a public key is valid.

Note that you are not required to check for a valid public key before using any other uECC
functions. However, you may wish to avoid spending CPU time computing a shared secret or
verifying a signature using an invalid public key.

Inputs:
    public_key - The public key to check.

Returns 1 if the public key is valid, 0 if it is invalid.
*/
int ef_uECC_valid_public_key(const uint8_t public_key[ef_uECC_BYTES*2]);

/* ef_uECC_compute_public_key() function.
Compute the corresponding public key for a private key.

Inputs:
    private_key - The private key to compute the public key for

Outputs:
    public_key - Will be filled in with the corresponding public key

Returns 1 if the key was computed successfully, 0 if an error occurred.
*/
int ef_uECC_compute_public_key(const uint8_t private_key[ef_uECC_BYTES],
                            uint8_t public_key[ef_uECC_BYTES * 2]);


/* ef_uECC_bytes() function.
Returns the value of ef_uECC_BYTES. Helpful for foreign-interfaces to higher-level languages.
*/
int ef_uECC_bytes(void);

/* ef_uECC_curve() function.
Returns the value of ef_uECC_CURVE. Helpful for foreign-interfaces to higher-level languages.
*/
int ef_uECC_curve(void);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* _MICRO_ECC_H_ */
