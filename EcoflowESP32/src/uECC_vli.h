#ifndef _ECOFLOW_UECC_VLI_H_
#define _ECOFLOW_UECC_VLI_H_

#include "uECC.h"

#ifndef ecoflow_uECC_ENABLE_VLI_API
    #define ecoflow_uECC_ENABLE_VLI_API 0
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#if ecoflow_uECC_ENABLE_VLI_API

void ecoflow_uECC_vli_clear(ecoflow_uECC_word_t *vli, wordcount_t num_words);
ecoflow_uECC_word_t ecoflow_uECC_vli_isZero(const ecoflow_uECC_word_t *vli, wordcount_t num_words);
ecoflow_uECC_word_t ecoflow_uECC_vli_testBit(const ecoflow_uECC_word_t *vli, bitcount_t bit);
bitcount_t ecoflow_uECC_vli_numBits(const ecoflow_uECC_word_t *vli, const wordcount_t max_words);
void ecoflow_uECC_vli_set(ecoflow_uECC_word_t *dest, const ecoflow_uECC_word_t *src, wordcount_t num_words);
ecoflow_uECC_word_t ecoflow_uECC_vli_equal(const ecoflow_uECC_word_t *left,
                           const ecoflow_uECC_word_t *right,
                           wordcount_t num_words);
cmpresult_t ecoflow_uECC_vli_cmp(const ecoflow_uECC_word_t *left, const ecoflow_uECC_word_t *right, wordcount_t num_words);
void ecoflow_uECC_vli_rshift1(ecoflow_uECC_word_t *vli, wordcount_t num_words);
ecoflow_uECC_word_t ecoflow_uECC_vli_add(ecoflow_uECC_word_t *result,
                         const ecoflow_uECC_word_t *left,
                         const ecoflow_uECC_word_t *right,
                         wordcount_t num_words);
ecoflow_uECC_word_t ecoflow_uECC_vli_sub(ecoflow_uECC_word_t *result,
                         const ecoflow_uECC_word_t *left,
                         const ecoflow_uECC_word_t *right,
                         wordcount_t num_words);
void ecoflow_uECC_vli_mult(ecoflow_uECC_word_t *result,
                   const ecoflow_uECC_word_t *left,
                   const ecoflow_uECC_word_t *right,
                   wordcount_t num_words);
void ecoflow_uECC_vli_square(ecoflow_uECC_word_t *result, const ecoflow_uECC_word_t *left, wordcount_t num_words);
void ecoflow_uECC_vli_modAdd(ecoflow_uECC_word_t *result,
                     const ecoflow_uECC_word_t *left,
                     const ecoflow_uECC_word_t *right,
                     const ecoflow_uECC_word_t *mod,
                     wordcount_t num_words);
void ecoflow_uECC_vli_modSub(ecoflow_uECC_word_t *result,
                     const ecoflow_uECC_word_t *left,
                     const ecoflow_uECC_word_t *right,
                     const ecoflow_uECC_word_t *mod,
                     wordcount_t num_words);
void ecoflow_uECC_vli_mmod(ecoflow_uECC_word_t *result,
                   ecoflow_uECC_word_t *product,
                   const ecoflow_uECC_word_t *mod,
                   wordcount_t num_words);
void ecoflow_uECC_vli_mmod_fast(ecoflow_uECC_word_t *result, ecoflow_uECC_word_t *product, ecoflow_uECC_Curve curve);
void ecoflow_uECC_vli_modMult(ecoflow_uECC_word_t *result,
                      const ecoflow_uECC_word_t *left,
                      const ecoflow_uECC_word_t *right,
                      const ecoflow_uECC_word_t *mod,
                      wordcount_t num_words);
void ecoflow_uECC_vli_modMult_fast(ecoflow_uECC_word_t *result,
                           const ecoflow_uECC_word_t *left,
                           const ecoflow_uECC_word_t *right,
                           ecoflow_uECC_Curve curve);
void ecoflow_uECC_vli_modSquare(ecoflow_uECC_word_t *result,
                        const ecoflow_uECC_word_t *left,
                        const ecoflow_uECC_word_t *mod,
                        wordcount_t num_words);
void ecoflow_uECC_vli_modSquare_fast(ecoflow_uECC_word_t *result, const ecoflow_uECC_word_t *left, ecoflow_uECC_Curve curve);
void ecoflow_uECC_vli_modInv(ecoflow_uECC_word_t *result,
                     const ecoflow_uECC_word_t *input,
                     const ecoflow_uECC_word_t *mod,
                     wordcount_t num_words);

#if ecoflow_uECC_SUPPORT_COMPRESSED_POINT
void ecoflow_uECC_vli_mod_sqrt(ecoflow_uECC_word_t *a, ecoflow_uECC_Curve curve);
#endif

void ecoflow_uECC_vli_nativeToBytes(uint8_t *bytes, int num_bytes, const ecoflow_uECC_word_t *native);
void ecoflow_uECC_vli_bytesToNative(ecoflow_uECC_word_t *native, const uint8_t *bytes, int num_bytes);

unsigned ecoflow_uECC_curve_num_words(ecoflow_uECC_Curve curve);
unsigned ecoflow_uECC_curve_num_bytes(ecoflow_uECC_Curve curve);
unsigned ecoflow_uECC_curve_num_bits(ecoflow_uECC_Curve curve);
unsigned ecoflow_uECC_curve_num_n_words(ecoflow_uECC_Curve curve);
unsigned ecoflow_uECC_curve_num_n_bytes(ecoflow_uECC_Curve curve);
unsigned ecoflow_uECC_curve_num_n_bits(ecoflow_uECC_Curve curve);

const ecoflow_uECC_word_t *ecoflow_uECC_curve_p(ecoflow_uECC_Curve curve);
const ecoflow_uECC_word_t *ecoflow_uECC_curve_n(ecoflow_uECC_Curve curve);
const ecoflow_uECC_word_t *ecoflow_uECC_curve_G(ecoflow_uECC_Curve curve);
const ecoflow_uECC_word_t *ecoflow_uECC_curve_b(ecoflow_uECC_Curve curve);

int ecoflow_uECC_valid_point(const ecoflow_uECC_word_t *point, ecoflow_uECC_Curve curve);
void ecoflow_uECC_point_mult(ecoflow_uECC_word_t *result,
                     const ecoflow_uECC_word_t *point,
                     const ecoflow_uECC_word_t *scalar,
                     ecoflow_uECC_Curve curve);
int ecoflow_uECC_generate_random_int(ecoflow_uECC_word_t *random,
                             const ecoflow_uECC_word_t *top,
                             wordcount_t num_words);

#endif /* ecoflow_uECC_ENABLE_VLI_API */

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* _ECOFLOW_UECC_VLI_H_ */