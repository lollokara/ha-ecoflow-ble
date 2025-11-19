#include "uECC.h"
#define uECC_uECC_vli_h

#ifndef ecoflow_uECC_RNG_MAX_TRIES
    #define ecoflow_uECC_RNG_MAX_TRIES 64
#endif

#if ecoflow_uECC_ENABLE_VLI_API
    #define ecoflow_uECC_VLI_API
#else
    #define ecoflow_uECC_VLI_API static
#endif

#if (ecoflow_uECC_PLATFORM == ecoflow_uECC_avr) || \
    (ecoflow_uECC_PLATFORM == ecoflow_uECC_arm) || \
    (ecoflow_uECC_PLATFORM == ecoflow_uECC_arm_thumb) || \
    (ecoflow_uECC_PLATFORM == ecoflow_uECC_arm_thumb2)
    #define CONCATX(a, ...) a ## __VA_ARGS__
    #define CONCAT(a, ...) CONCATX(a, __VA_ARGS__)

    #define STRX(a) #a
    #define STR(a) STRX(a)

    #define EVAL(...)  EVAL1(EVAL1(EVAL1(EVAL1(__VA_ARGS__))))
    #define EVAL1(...) EVAL2(EVAL2(EVAL2(EVAL2(__VA_ARGS__))))
    #define EVAL2(...) EVAL3(EVAL3(EVAL3(EVAL3(__VA_ARGS__))))
    #define EVAL3(...) EVAL4(EVAL4(EVAL4(EVAL4(__VA_ARGS__))))
    #define EVAL4(...) __VA_ARGS__

    #define DEC_1  0
    #define DEC_2  1
    #define DEC_3  2
    #define DEC_4  3
    #define DEC_5  4
    #define DEC_6  5
    #define DEC_7  6
    #define DEC_8  7
    #define DEC_9  8
    #define DEC_10 9
    #define DEC_11 10
    #define DEC_12 11
    #define DEC_13 12
    #define DEC_14 13
    #define DEC_15 14
    #define DEC_16 15
    #define DEC_17 16
    #define DEC_18 17
    #define DEC_19 18
    #define DEC_20 19
    #define DEC_21 20
    #define DEC_22 21
    #define DEC_23 22
    #define DEC_24 23
    #define DEC_25 24
    #define DEC_26 25
    #define DEC_27 26
    #define DEC_28 27
    #define DEC_29 28
    #define DEC_30 29
    #define DEC_31 30
    #define DEC_32 31

    #define DEC(N) CONCAT(DEC_, N)

    #define SECOND_ARG(_, val, ...) val
    #define SOME_CHECK_0 ~, 0
    #define GET_SECOND_ARG(...) SECOND_ARG(__VA_ARGS__, SOME,)
    #define SOME_OR_0(N) GET_SECOND_ARG(CONCAT(SOME_CHECK_, N))

    #define EMPTY(...)
    #define DEFER(...) __VA_ARGS__ EMPTY()

    #define REPEAT_NAME_0() REPEAT_0
    #define REPEAT_NAME_SOME() REPEAT_SOME
    #define REPEAT_0(...)
    #define REPEAT_SOME(N, stuff) DEFER(CONCAT(REPEAT_NAME_, SOME_OR_0(DEC(N))))()(DEC(N), stuff) stuff
    #define REPEAT(N, stuff) EVAL(REPEAT_SOME(N, stuff))

    #define REPEATM_NAME_0() REPEATM_0
    #define REPEATM_NAME_SOME() REPEATM_SOME
    #define REPEATM_0(...)
    #define REPEATM_SOME(N, macro) macro(N) \
        DEFER(CONCAT(REPEATM_NAME_, SOME_OR_0(DEC(N))))()(DEC(N), macro)
    #define REPEATM(N, macro) EVAL(REPEATM_SOME(N, macro))
#endif

#include "platform-specific.inc"

#if (ecoflow_uECC_WORD_SIZE == 1)
    #if ecoflow_uECC_SUPPORTS_secp160r1
        #define ecoflow_uECC_MAX_WORDS 21 /* Due to the size of curve_n. */
    #endif
    #if ecoflow_uECC_SUPPORTS_secp192r1
        #undef ecoflow_uECC_MAX_WORDS
        #define ecoflow_uECC_MAX_WORDS 24
    #endif
    #if ecoflow_uECC_SUPPORTS_secp224r1
        #undef ecoflow_uECC_MAX_WORDS
        #define ecoflow_uECC_MAX_WORDS 28
    #endif
    #if (ecoflow_uECC_SUPPORTS_secp256r1 || ecoflow_uECC_SUPPORTS_secp256k1)
        #undef ecoflow_uECC_MAX_WORDS
        #define ecoflow_uECC_MAX_WORDS 32
    #endif
#elif (ecoflow_uECC_WORD_SIZE == 4)
    #if ecoflow_uECC_SUPPORTS_secp160r1
        #define ecoflow_uECC_MAX_WORDS 6 /* Due to the size of curve_n. */
    #endif
    #if ecoflow_uECC_SUPPORTS_secp192r1
        #undef ecoflow_uECC_MAX_WORDS
        #define ecoflow_uECC_MAX_WORDS 6
    #endif
    #if ecoflow_uECC_SUPPORTS_secp224r1
        #undef ecoflow_uECC_MAX_WORDS
        #define ecoflow_uECC_MAX_WORDS 7
    #endif
    #if (ecoflow_uECC_SUPPORTS_secp256r1 || ecoflow_uECC_SUPPORTS_secp256k1)
        #undef ecoflow_uECC_MAX_WORDS
        #define ecoflow_uECC_MAX_WORDS 8
    #endif
#elif (ecoflow_uECC_WORD_SIZE == 8)
    #if ecoflow_uECC_SUPPORTS_secp160r1
        #define ecoflow_uECC_MAX_WORDS 3
    #endif
    #if ecoflow_uECC_SUPPORTS_secp192r1
        #undef ecoflow_uECC_MAX_WORDS
        #define ecoflow_uECC_MAX_WORDS 3
    #endif
    #if ecoflow_uECC_SUPPORTS_secp224r1
        #undef ecoflow_uECC_MAX_WORDS
        #define ecoflow_uECC_MAX_WORDS 4
    #endif
    #if (ecoflow_uECC_SUPPORTS_secp256r1 || ecoflow_uECC_SUPPORTS_secp256k1)
        #undef ecoflow_uECC_MAX_WORDS
        #define ecoflow_uECC_MAX_WORDS 4
    #endif
#endif /* ecoflow_uECC_WORD_SIZE */

#define BITS_TO_WORDS(num_bits) ((num_bits + ((ecoflow_uECC_WORD_SIZE * 8) - 1)) / (ecoflow_uECC_WORD_SIZE * 8))
#define BITS_TO_BYTES(num_bits) ((num_bits + 7) / 8)

struct ecoflow_uECC_Curve_t {
    wordcount_t num_words;
    wordcount_t num_bytes;
    bitcount_t num_n_bits;
    ecoflow_uECC_word_t p[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t n[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t G[ecoflow_uECC_MAX_WORDS * 2];
    ecoflow_uECC_word_t b[ecoflow_uECC_MAX_WORDS];
    void (*double_jacobian)(ecoflow_uECC_word_t * X1,
                            ecoflow_uECC_word_t * Y1,
                            ecoflow_uECC_word_t * Z1,
                            ecoflow_uECC_Curve curve);
#if ecoflow_uECC_SUPPORT_COMPRESSED_POINT
    void (*mod_sqrt)(ecoflow_uECC_word_t *a, ecoflow_uECC_Curve curve);
#endif
    void (*x_side)(ecoflow_uECC_word_t *result, const ecoflow_uECC_word_t *x, ecoflow_uECC_Curve curve);
#if (ecoflow_uECC_OPTIMIZATION_LEVEL > 0)
    void (*mmod_fast)(ecoflow_uECC_word_t *result, ecoflow_uECC_word_t *product);
#endif
};

#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
static void bcopy(uint8_t *dst,
                  const uint8_t *src,
                  unsigned num_bytes) {
    while (0 != num_bytes) {
        num_bytes--;
        dst[num_bytes] = src[num_bytes];
    }
}
#endif

static cmpresult_t ecoflow_uECC_vli_cmp_unsafe(const ecoflow_uECC_word_t *left,
                                       const ecoflow_uECC_word_t *right,
                                       wordcount_t num_words);

#if (ecoflow_uECC_PLATFORM == ecoflow_uECC_arm || ecoflow_uECC_PLATFORM == ecoflow_uECC_arm_thumb || \
        ecoflow_uECC_PLATFORM == ecoflow_uECC_arm_thumb2)
    #include "asm_arm.inc"
#endif

#if (ecoflow_uECC_PLATFORM == ecoflow_uECC_avr)
    #include "asm_avr.inc"
#endif

#if default_RNG_defined
static ecoflow_uECC_RNG_Function g_rng_function = &default_RNG;
#else
static ecoflow_uECC_RNG_Function g_rng_function = 0;
#endif

void ecoflow_uECC_set_rng(ecoflow_uECC_RNG_Function rng_function) {
    g_rng_function = rng_function;
}

ecoflow_uECC_RNG_Function ecoflow_uECC_get_rng(void) {
    return g_rng_function;
}

int ecoflow_uECC_curve_private_key_size(ecoflow_uECC_Curve curve) {
    return BITS_TO_BYTES(curve->num_n_bits);
}

int ecoflow_uECC_curve_public_key_size(ecoflow_uECC_Curve curve) {
    return 2 * curve->num_bytes;
}

#if !asm_clear
ecoflow_uECC_VLI_API void ecoflow_uECC_vli_clear(ecoflow_uECC_word_t *vli, wordcount_t num_words) {
    wordcount_t i;
    for (i = 0; i < num_words; ++i) {
        vli[i] = 0;
    }
}
#endif /* !asm_clear */

ecoflow_uECC_VLI_API ecoflow_uECC_word_t ecoflow_uECC_vli_isZero(const ecoflow_uECC_word_t *vli, wordcount_t num_words) {
    ecoflow_uECC_word_t bits = 0;
    wordcount_t i;
    for (i = 0; i < num_words; ++i) {
        bits |= vli[i];
    }
    return (bits == 0);
}

ecoflow_uECC_VLI_API ecoflow_uECC_word_t ecoflow_uECC_vli_testBit(const ecoflow_uECC_word_t *vli, bitcount_t bit) {
    return (vli[bit >> ecoflow_uECC_WORD_BITS_SHIFT] & ((ecoflow_uECC_word_t)1 << (bit & ecoflow_uECC_WORD_BITS_MASK)));
}

static wordcount_t vli_numDigits(const ecoflow_uECC_word_t *vli, const wordcount_t max_words) {
    wordcount_t i;
    for (i = max_words - 1; i >= 0 && vli[i] == 0; --i) {
    }

    return (i + 1);
}

ecoflow_uECC_VLI_API bitcount_t ecoflow_uECC_vli_numBits(const ecoflow_uECC_word_t *vli, const wordcount_t max_words) {
    ecoflow_uECC_word_t i;
    ecoflow_uECC_word_t digit;

    wordcount_t num_digits = vli_numDigits(vli, max_words);
    if (num_digits == 0) {
        return 0;
    }

    digit = vli[num_digits - 1];
    for (i = 0; digit; ++i) {
        digit >>= 1;
    }

    return (((bitcount_t)(num_digits - 1) << ecoflow_uECC_WORD_BITS_SHIFT) + i);
}

#if !asm_set
ecoflow_uECC_VLI_API void ecoflow_uECC_vli_set(ecoflow_uECC_word_t *dest, const ecoflow_uECC_word_t *src, wordcount_t num_words) {
    wordcount_t i;
    for (i = 0; i < num_words; ++i) {
        dest[i] = src[i];
    }
}
#endif /* !asm_set */

static cmpresult_t ecoflow_uECC_vli_cmp_unsafe(const ecoflow_uECC_word_t *left,
                                       const ecoflow_uECC_word_t *right,
                                       wordcount_t num_words) {
    wordcount_t i;
    for (i = num_words - 1; i >= 0; --i) {
        if (left[i] > right[i]) {
            return 1;
        } else if (left[i] < right[i]) {
            return -1;
        }
    }
    return 0;
}

ecoflow_uECC_VLI_API ecoflow_uECC_word_t ecoflow_uECC_vli_equal(const ecoflow_uECC_word_t *left,
                                        const ecoflow_uECC_word_t *right,
                                        wordcount_t num_words) {
    ecoflow_uECC_word_t diff = 0;
    wordcount_t i;
    for (i = num_words - 1; i >= 0; --i) {
        diff |= (left[i] ^ right[i]);
    }
    return (diff == 0);
}

ecoflow_uECC_VLI_API ecoflow_uECC_word_t ecoflow_uECC_vli_sub(ecoflow_uECC_word_t *result,
                                      const ecoflow_uECC_word_t *left,
                                      const ecoflow_uECC_word_t *right,
                                      wordcount_t num_words);

ecoflow_uECC_VLI_API cmpresult_t ecoflow_uECC_vli_cmp(const ecoflow_uECC_word_t *left,
                                      const ecoflow_uECC_word_t *right,
                                      wordcount_t num_words) {
    ecoflow_uECC_word_t tmp[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t neg = !!ecoflow_uECC_vli_sub(tmp, left, right, num_words);
    ecoflow_uECC_word_t equal = ecoflow_uECC_vli_isZero(tmp, num_words);
    return (!equal - 2 * neg);
}

#if !asm_rshift1
ecoflow_uECC_VLI_API void ecoflow_uECC_vli_rshift1(ecoflow_uECC_word_t *vli, wordcount_t num_words) {
    ecoflow_uECC_word_t *end = vli;
    ecoflow_uECC_word_t carry = 0;

    vli += num_words;
    while (vli-- > end) {
        ecoflow_uECC_word_t temp = *vli;
        *vli = (temp >> 1) | carry;
        carry = temp << (ecoflow_uECC_WORD_BITS - 1);
    }
}
#endif /* !asm_rshift1 */

#if !asm_add
ecoflow_uECC_VLI_API ecoflow_uECC_word_t ecoflow_uECC_vli_add(ecoflow_uECC_word_t *result,
                                      const ecoflow_uECC_word_t *left,
                                      const ecoflow_uECC_word_t *right,
                                      wordcount_t num_words) {
    ecoflow_uECC_word_t carry = 0;
    wordcount_t i;
    for (i = 0; i < num_words; ++i) {
        ecoflow_uECC_word_t sum = left[i] + right[i] + carry;
        if (sum != left[i]) {
            carry = (sum < left[i]);
        }
        result[i] = sum;
    }
    return carry;
}
#endif /* !asm_add */

#if !asm_sub
ecoflow_uECC_VLI_API ecoflow_uECC_word_t ecoflow_uECC_vli_sub(ecoflow_uECC_word_t *result,
                                      const ecoflow_uECC_word_t *left,
                                      const ecoflow_uECC_word_t *right,
                                      wordcount_t num_words) {
    ecoflow_uECC_word_t borrow = 0;
    wordcount_t i;
    for (i = 0; i < num_words; ++i) {
        ecoflow_uECC_word_t diff = left[i] - right[i] - borrow;
        if (diff != left[i]) {
            borrow = (diff > left[i]);
        }
        result[i] = diff;
    }
    return borrow;
}
#endif /* !asm_sub */

#if !asm_mult || (ecoflow_uECC_SQUARE_FUNC && !asm_square) || \
    (ecoflow_uECC_SUPPORTS_secp256k1 && (ecoflow_uECC_OPTIMIZATION_LEVEL > 0) && \
        ((ecoflow_uECC_WORD_SIZE == 1) || (ecoflow_uECC_WORD_SIZE == 8)))
static void muladd(ecoflow_uECC_word_t a,
                   ecoflow_uECC_word_t b,
                   ecoflow_uECC_word_t *r0,
                   ecoflow_uECC_word_t *r1,
                   ecoflow_uECC_word_t *r2) {
#if ecoflow_uECC_WORD_SIZE == 8 && !SUPPORTS_INT128
    uint64_t a0 = a & 0xffffffffull;
    uint64_t a1 = a >> 32;
    uint64_t b0 = b & 0xffffffffull;
    uint64_t b1 = b >> 32;

    uint64_t i0 = a0 * b0;
    uint64_t i1 = a0 * b1;
    uint64_t i2 = a1 * b0;
    uint64_t i3 = a1 * b1;

    uint64_t p0, p1;

    i2 += (i0 >> 32);
    i2 += i1;
    if (i2 < i1) { /* overflow */
        i3 += 0x100000000ull;
    }

    p0 = (i0 & 0xffffffffull) | (i2 << 32);
    p1 = i3 + (i2 >> 32);

    *r0 += p0;
    *r1 += (p1 + (*r0 < p0));
    *r2 += ((*r1 < p1) || (*r1 == p1 && *r0 < p0));
#else
    ecoflow_uECC_dword_t p = (ecoflow_uECC_dword_t)a * b;
    ecoflow_uECC_dword_t r01 = ((ecoflow_uECC_dword_t)(*r1) << ecoflow_uECC_WORD_BITS) | *r0;
    r01 += p;
    *r2 += (r01 < p);
    *r1 = r01 >> ecoflow_uECC_WORD_BITS;
    *r0 = (ecoflow_uECC_word_t)r01;
#endif
}
#endif /* muladd needed */

#if !asm_mult
ecoflow_uECC_VLI_API void ecoflow_uECC_vli_mult(ecoflow_uECC_word_t *result,
                                const ecoflow_uECC_word_t *left,
                                const ecoflow_uECC_word_t *right,
                                wordcount_t num_words) {
    ecoflow_uECC_word_t r0 = 0;
    ecoflow_uECC_word_t r1 = 0;
    ecoflow_uECC_word_t r2 = 0;
    wordcount_t i, k;

    for (k = 0; k < num_words; ++k) {
        for (i = 0; i <= k; ++i) {
            muladd(left[i], right[k - i], &r0, &r1, &r2);
        }
        result[k] = r0;
        r0 = r1;
        r1 = r2;
        r2 = 0;
    }
    for (k = num_words; k < num_words * 2 - 1; ++k) {
        for (i = (k + 1) - num_words; i < num_words; ++i) {
            muladd(left[i], right[k - i], &r0, &r1, &r2);
        }
        result[k] = r0;
        r0 = r1;
        r1 = r2;
        r2 = 0;
    }
    result[num_words * 2 - 1] = r0;
}
#endif /* !asm_mult */

#if ecoflow_uECC_SQUARE_FUNC

#if !asm_square
static void mul2add(ecoflow_uECC_word_t a,
                    ecoflow_uECC_word_t b,
                    ecoflow_uECC_word_t *r0,
                    ecoflow_uECC_word_t *r1,
                    ecoflow_uECC_word_t *r2) {
#if ecoflow_uECC_WORD_SIZE == 8 && !SUPPORTS_INT128
    uint64_t a0 = a & 0xffffffffull;
    uint64_t a1 = a >> 32;
    uint64_t b0 = b & 0xffffffffull;
    uint64_t b1 = b >> 32;

    uint64_t i0 = a0 * b0;
    uint64_t i1 = a0 * b1;
    uint64_t i2 = a1 * b0;
    uint64_t i3 = a1 * b1;

    uint64_t p0, p1;

    i2 += (i0 >> 32);
    i2 += i1;
    if (i2 < i1)
    { /* overflow */
        i3 += 0x100000000ull;
    }

    p0 = (i0 & 0xffffffffull) | (i2 << 32);
    p1 = i3 + (i2 >> 32);

    *r2 += (p1 >> 63);
    p1 = (p1 << 1) | (p0 >> 63);
    p0 <<= 1;

    *r0 += p0;
    *r1 += (p1 + (*r0 < p0));
    *r2 += ((*r1 < p1) || (*r1 == p1 && *r0 < p0));
#else
    ecoflow_uECC_dword_t p = (ecoflow_uECC_dword_t)a * b;
    ecoflow_uECC_dword_t r01 = ((ecoflow_uECC_dword_t)(*r1) << ecoflow_uECC_WORD_BITS) | *r0;
    *r2 += (p >> (ecoflow_uECC_WORD_BITS * 2 - 1));
    p *= 2;
    r01 += p;
    *r2 += (r01 < p);
    *r1 = r01 >> ecoflow_uECC_WORD_BITS;
    *r0 = (ecoflow_uECC_word_t)r01;
#endif
}

ecoflow_uECC_VLI_API void ecoflow_uECC_vli_square(ecoflow_uECC_word_t *result,
                                  const ecoflow_uECC_word_t *left,
                                  wordcount_t num_words) {
    ecoflow_uECC_word_t r0 = 0;
    ecoflow_uECC_word_t r1 = 0;
    ecoflow_uECC_word_t r2 = 0;

    wordcount_t i, k;

    for (k = 0; k < num_words * 2 - 1; ++k) {
        ecoflow_uECC_word_t min = (k < num_words ? 0 : (k + 1) - num_words);
        for (i = min; i <= k && i <= k - i; ++i) {
            if (i < k-i) {
                mul2add(left[i], left[k - i], &r0, &r1, &r2);
            } else {
                muladd(left[i], left[k - i], &r0, &r1, &r2);
            }
        }
        result[k] = r0;
        r0 = r1;
        r1 = r2;
        r2 = 0;
    }

    result[num_words * 2 - 1] = r0;
}
#endif /* !asm_square */

#else /* ecoflow_uECC_SQUARE_FUNC */

#if ecoflow_uECC_ENABLE_VLI_API
ecoflow_uECC_VLI_API void ecoflow_uECC_vli_square(ecoflow_uECC_word_t *result,
                                  const ecoflow_uECC_word_t *left,
                                  wordcount_t num_words) {
    ecoflow_uECC_vli_mult(result, left, left, num_words);
}
#endif /* ecoflow_uECC_ENABLE_VLI_API */

#endif /* ecoflow_uECC_SQUARE_FUNC */

/* Computes result = (left + right) % mod. 
   Assumes that left < mod and right < mod, and that result does not overlap mod. */
ecoflow_uECC_VLI_API void ecoflow_uECC_vli_modAdd(ecoflow_uECC_word_t *result,
                                  const ecoflow_uECC_word_t *left,
                                  const ecoflow_uECC_word_t *right,
                                  const ecoflow_uECC_word_t *mod, 
                                  wordcount_t num_words) {
    ecoflow_uECC_word_t carry = ecoflow_uECC_vli_add(result, left, right, num_words);
    if (carry || ecoflow_uECC_vli_cmp_unsafe(mod, result, num_words) != 1) { 
        /* result > mod (result = mod + remainder), so subtract mod to get remainder. */
        ecoflow_uECC_vli_sub(result, result, mod, num_words);
    }
}

/* Computes result = (left - right) % mod.
   Assumes that left < mod and right < mod, and that result does not overlap mod. */
ecoflow_uECC_VLI_API void ecoflow_uECC_vli_modSub(ecoflow_uECC_word_t *result,
                                  const ecoflow_uECC_word_t *left,
                                  const ecoflow_uECC_word_t *right,
                                  const ecoflow_uECC_word_t *mod,
                                  wordcount_t num_words) {
    ecoflow_uECC_word_t l_borrow = ecoflow_uECC_vli_sub(result, left, right, num_words);
    if (l_borrow) { 
        /* In this case, result == -diff == (max int) - diff. Since -x % d == d - x, 
           we can get the correct result from result + mod (with overflow). */
        ecoflow_uECC_vli_add(result, result, mod, num_words);
    }
}

/* Computes result = product % mod, where product is 2N words long. */
/* Currently only designed to work for curve_p or curve_n. */
ecoflow_uECC_VLI_API void ecoflow_uECC_vli_mmod(ecoflow_uECC_word_t *result,
                                ecoflow_uECC_word_t *product,
                                const ecoflow_uECC_word_t *mod,
                                wordcount_t num_words) {
    ecoflow_uECC_word_t mod_multiple[2 * ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t tmp[2 * ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t *v[2] = {tmp, product};
    ecoflow_uECC_word_t index;

    /* Shift mod so its highest set bit is at the maximum position. */
    bitcount_t shift = (num_words * 2 * ecoflow_uECC_WORD_BITS) - ecoflow_uECC_vli_numBits(mod, num_words);
    wordcount_t word_shift = shift / ecoflow_uECC_WORD_BITS;
    wordcount_t bit_shift = shift % ecoflow_uECC_WORD_BITS;
    ecoflow_uECC_word_t carry = 0;
    ecoflow_uECC_vli_clear(mod_multiple, word_shift);
    if (bit_shift > 0) {
        for(index = 0; index < (ecoflow_uECC_word_t)num_words; ++index) {
            mod_multiple[word_shift + index] = (mod[index] << bit_shift) | carry;
            carry = mod[index] >> (ecoflow_uECC_WORD_BITS - bit_shift);
        }
    } else {
        ecoflow_uECC_vli_set(mod_multiple + word_shift, mod, num_words);
    }

    for (index = 1; shift >= 0; --shift) {
        ecoflow_uECC_word_t borrow = 0;
        wordcount_t i;
        for (i = 0; i < num_words * 2; ++i) {
            ecoflow_uECC_word_t diff = v[index][i] - mod_multiple[i] - borrow;
            if (diff != v[index][i]) {
                borrow = (diff > v[index][i]);
            }
            v[1 - index][i] = diff;
        }
        index = !(index ^ borrow); /* Swap the index if there was no borrow */
        ecoflow_uECC_vli_rshift1(mod_multiple, num_words);
        mod_multiple[num_words - 1] |= mod_multiple[num_words] << (ecoflow_uECC_WORD_BITS - 1);
        ecoflow_uECC_vli_rshift1(mod_multiple + num_words, num_words);
    }
    ecoflow_uECC_vli_set(result, v[index], num_words);
}

/* Computes result = (left * right) % mod. */
ecoflow_uECC_VLI_API void ecoflow_uECC_vli_modMult(ecoflow_uECC_word_t *result,
                                   const ecoflow_uECC_word_t *left,
                                   const ecoflow_uECC_word_t *right,
                                   const ecoflow_uECC_word_t *mod,
                                   wordcount_t num_words) {
    ecoflow_uECC_word_t product[2 * ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_vli_mult(product, left, right, num_words);
    ecoflow_uECC_vli_mmod(result, product, mod, num_words);
}

ecoflow_uECC_VLI_API void ecoflow_uECC_vli_modMult_fast(ecoflow_uECC_word_t *result,
                                        const ecoflow_uECC_word_t *left,
                                        const ecoflow_uECC_word_t *right,
                                        ecoflow_uECC_Curve curve) {
    ecoflow_uECC_word_t product[2 * ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_vli_mult(product, left, right, curve->num_words);
#if (ecoflow_uECC_OPTIMIZATION_LEVEL > 0)
    curve->mmod_fast(result, product);
#else
    ecoflow_uECC_vli_mmod(result, product, curve->p, curve->num_words);
#endif
}

#if ecoflow_uECC_SQUARE_FUNC

#if ecoflow_uECC_ENABLE_VLI_API
/* Computes result = left^2 % mod. */
ecoflow_uECC_VLI_API void ecoflow_uECC_vli_modSquare(ecoflow_uECC_word_t *result,
                                     const ecoflow_uECC_word_t *left,
                                     const ecoflow_uECC_word_t *mod,
                                     wordcount_t num_words) {
    ecoflow_uECC_word_t product[2 * ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_vli_square(product, left, num_words);
    ecoflow_uECC_vli_mmod(result, product, mod, num_words);
}
#endif /* ecoflow_uECC_ENABLE_VLI_API */

ecoflow_uECC_VLI_API void ecoflow_uECC_vli_modSquare_fast(ecoflow_uECC_word_t *result,
                                          const ecoflow_uECC_word_t *left,
                                          ecoflow_uECC_Curve curve) {
    ecoflow_uECC_word_t product[2 * ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_vli_square(product, left, curve->num_words);
#if (ecoflow_uECC_OPTIMIZATION_LEVEL > 0)
    curve->mmod_fast(result, product);
#else
    ecoflow_uECC_vli_mmod(result, product, curve->p, curve->num_words);
#endif
}

#else /* ecoflow_uECC_SQUARE_FUNC */

#if ecoflow_uECC_ENABLE_VLI_API
ecoflow_uECC_VLI_API void ecoflow_uECC_vli_modSquare(ecoflow_uECC_word_t *result,
                                     const ecoflow_uECC_word_t *left,
                                     const ecoflow_uECC_word_t *mod,
                                     wordcount_t num_words) {
    ecoflow_uECC_vli_modMult(result, left, left, mod, num_words);
}
#endif /* ecoflow_uECC_ENABLE_VLI_API */

ecoflow_uECC_VLI_API void ecoflow_uECC_vli_modSquare_fast(ecoflow_uECC_word_t *result,
                                          const ecoflow_uECC_word_t *left,
                                          ecoflow_uECC_Curve curve) {
    ecoflow_uECC_vli_modMult_fast(result, left, left, curve);
}

#endif /* ecoflow_uECC_SQUARE_FUNC */

#define EVEN(vli) (!(vli[0] & 1))
static void vli_modInv_update(ecoflow_uECC_word_t *uv,
                              const ecoflow_uECC_word_t *mod,
                              wordcount_t num_words) {
    ecoflow_uECC_word_t carry = 0;
    if (!EVEN(uv)) {
        carry = ecoflow_uECC_vli_add(uv, uv, mod, num_words);
    }
    ecoflow_uECC_vli_rshift1(uv, num_words);
    if (carry) {
        uv[num_words - 1] |= HIGH_BIT_SET;
    }
}

/* Computes result = (1 / input) % mod. All VLIs are the same size.
   See "From Euclid's GCD to Montgomery Multiplication to the Great Divide" */
ecoflow_uECC_VLI_API void ecoflow_uECC_vli_modInv(ecoflow_uECC_word_t *result,
                                  const ecoflow_uECC_word_t *input,
                                  const ecoflow_uECC_word_t *mod,
                                  wordcount_t num_words) {
    ecoflow_uECC_word_t a[ecoflow_uECC_MAX_WORDS], b[ecoflow_uECC_MAX_WORDS], u[ecoflow_uECC_MAX_WORDS], v[ecoflow_uECC_MAX_WORDS];
    cmpresult_t cmpResult;

    if (ecoflow_uECC_vli_isZero(input, num_words)) {
        ecoflow_uECC_vli_clear(result, num_words);
        return;
    }

    ecoflow_uECC_vli_set(a, input, num_words);
    ecoflow_uECC_vli_set(b, mod, num_words);
    ecoflow_uECC_vli_clear(u, num_words);
    u[0] = 1;
    ecoflow_uECC_vli_clear(v, num_words);
    while ((cmpResult = ecoflow_uECC_vli_cmp_unsafe(a, b, num_words)) != 0) {
        if (EVEN(a)) {
            ecoflow_uECC_vli_rshift1(a, num_words);
            vli_modInv_update(u, mod, num_words);
        } else if (EVEN(b)) {
            ecoflow_uECC_vli_rshift1(b, num_words);
            vli_modInv_update(v, mod, num_words);
        } else if (cmpResult > 0) {
            ecoflow_uECC_vli_sub(a, a, b, num_words);
            ecoflow_uECC_vli_rshift1(a, num_words);
            if (ecoflow_uECC_vli_cmp_unsafe(u, v, num_words) < 0) {
                ecoflow_uECC_vli_add(u, u, mod, num_words);
            }
            ecoflow_uECC_vli_sub(u, u, v, num_words);
            vli_modInv_update(u, mod, num_words);
        } else {
            ecoflow_uECC_vli_sub(b, b, a, num_words);
            ecoflow_uECC_vli_rshift1(b, num_words);
            if (ecoflow_uECC_vli_cmp_unsafe(v, u, num_words) < 0) {
                ecoflow_uECC_vli_add(v, v, mod, num_words);
            }
            ecoflow_uECC_vli_sub(v, v, u, num_words);
            vli_modInv_update(v, mod, num_words);
        }
    }
    ecoflow_uECC_vli_set(result, u, num_words);
}

/* ------ Point operations ------ */

#include "curve-specific.inc"

/* Returns 1 if 'point' is the point at infinity, 0 otherwise. */
#define EccPoint_isZero(point, curve) ecoflow_uECC_vli_isZero((point), (curve)->num_words * 2)

/* Point multiplication algorithm using Montgomery's ladder with co-Z coordinates.
From http://eprint.iacr.org/2011/338.pdf
*/

/* Modify (x1, y1) => (x1 * z^2, y1 * z^3) */
static void apply_z(ecoflow_uECC_word_t * X1,
                    ecoflow_uECC_word_t * Y1,
                    const ecoflow_uECC_word_t * const Z,
                    ecoflow_uECC_Curve curve) {
    ecoflow_uECC_word_t t1[ecoflow_uECC_MAX_WORDS];

    ecoflow_uECC_vli_modSquare_fast(t1, Z, curve);    /* z^2 */
    ecoflow_uECC_vli_modMult_fast(X1, X1, t1, curve); /* x1 * z^2 */
    ecoflow_uECC_vli_modMult_fast(t1, t1, Z, curve);  /* z^3 */
    ecoflow_uECC_vli_modMult_fast(Y1, Y1, t1, curve); /* y1 * z^3 */
}

/* P = (x1, y1) => 2P, (x2, y2) => P' */
static void XYcZ_initial_double(ecoflow_uECC_word_t * X1,
                                ecoflow_uECC_word_t * Y1,
                                ecoflow_uECC_word_t * X2,
                                ecoflow_uECC_word_t * Y2,
                                const ecoflow_uECC_word_t * const initial_Z,
                                ecoflow_uECC_Curve curve) {
    ecoflow_uECC_word_t z[ecoflow_uECC_MAX_WORDS];
    wordcount_t num_words = curve->num_words;
    if (initial_Z) {
        ecoflow_uECC_vli_set(z, initial_Z, num_words);
    } else {
        ecoflow_uECC_vli_clear(z, num_words);
        z[0] = 1;
    }

    ecoflow_uECC_vli_set(X2, X1, num_words);
    ecoflow_uECC_vli_set(Y2, Y1, num_words);

    apply_z(X1, Y1, z, curve);
    curve->double_jacobian(X1, Y1, z, curve);
    apply_z(X2, Y2, z, curve);
}

/* Input P = (x1, y1, Z), Q = (x2, y2, Z)
   Output P' = (x1', y1', Z3), P + Q = (x3, y3, Z3)
   or P => P', Q => P + Q
   sub = x1' - x3 (used for subsequent call to XYcZ_addC()).
*/
static void XYcZ_add(ecoflow_uECC_word_t * X1,
                     ecoflow_uECC_word_t * Y1,
                     ecoflow_uECC_word_t * X2,
                     ecoflow_uECC_word_t * Y2,
                     ecoflow_uECC_word_t * sub,
                     ecoflow_uECC_Curve curve) {
    /* t1 = X1, t2 = Y1, t3 = X2, t4 = Y2 */
    ecoflow_uECC_word_t t5[ecoflow_uECC_MAX_WORDS];
    wordcount_t num_words = curve->num_words;

    ecoflow_uECC_vli_modSub(t5, X2, X1, curve->p, num_words);  /* t5 = x2 - x1 */
    ecoflow_uECC_vli_modSquare_fast(t5, t5, curve);            /* t5 = (x2 - x1)^2 = A */
    ecoflow_uECC_vli_modMult_fast(X1, X1, t5, curve);          /* x1' = x1*A = B */
    ecoflow_uECC_vli_modMult_fast(X2, X2, t5, curve);          /* t3 = x2*A = C */
    ecoflow_uECC_vli_modSub(Y2, Y2, Y1, curve->p, num_words);  /* t4 = y2 - y1 */
    ecoflow_uECC_vli_modSquare_fast(t5, Y2, curve);            /* t5 = (y2 - y1)^2 = D */

    ecoflow_uECC_vli_modSub(t5, t5, X1, curve->p, num_words);  /* t5 = D - B */
    ecoflow_uECC_vli_modSub(t5, t5, X2, curve->p, num_words);  /* t5 = D - B - C = x3 */
    ecoflow_uECC_vli_modSub(X2, X2, X1, curve->p, num_words);  /* t3 = C - B */
    ecoflow_uECC_vli_modMult_fast(Y1, Y1, X2, curve);          /* y1' = y1*(C - B) */
    ecoflow_uECC_vli_modSub(sub, X1, t5, curve->p, num_words); /* s = B - x3 */
    ecoflow_uECC_vli_modMult_fast(Y2, Y2, sub, curve);         /* t4 = (y2 - y1)*(B - x3) */
    ecoflow_uECC_vli_modSub(Y2, Y2, Y1, curve->p, num_words);  /* t4 = y3 */

    ecoflow_uECC_vli_set(X2, t5, num_words);                   /* move x3 to output */
}

/* Input P = (x1, y1, Z), Q = (x2, y2, Z), sub = x1 - x2
   Output P - Q = (x3', y3', Z3), P + Q = (x3, y3, Z3)
   or P => P - Q, Q => P + Q
*/
static void XYcZ_addC(ecoflow_uECC_word_t * X1,
                      ecoflow_uECC_word_t * Y1,
                      ecoflow_uECC_word_t * X2,
                      ecoflow_uECC_word_t * Y2,
                      ecoflow_uECC_word_t * sub,
                      ecoflow_uECC_Curve curve) {
    /* t1 = X1, t2 = Y1, t3 = X2, t4 = Y2 */
    ecoflow_uECC_word_t t5[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t t6[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t t7[ecoflow_uECC_MAX_WORDS];
    wordcount_t num_words = curve->num_words;

    ecoflow_uECC_vli_modSquare_fast(t5, sub, curve);          /* t5 = (x2 - x1)^2 = A */
    ecoflow_uECC_vli_modMult_fast(X1, X1, t5, curve);         /* t1 = x1*A = B */
    ecoflow_uECC_vli_modMult_fast(X2, X2, t5, curve);         /* t3 = x2*A = C */
    ecoflow_uECC_vli_modAdd(t5, Y2, Y1, curve->p, num_words); /* t5 = y2 + y1 */
    ecoflow_uECC_vli_modSub(Y2, Y2, Y1, curve->p, num_words); /* t4 = y2 - y1 */

    ecoflow_uECC_vli_modSub(t6, X2, X1, curve->p, num_words); /* t6 = C - B */
    ecoflow_uECC_vli_modMult_fast(Y1, Y1, t6, curve);         /* t2 = y1 * (C - B) = E */
    ecoflow_uECC_vli_modAdd(t6, X1, X2, curve->p, num_words); /* t6 = B + C */
    ecoflow_uECC_vli_modSquare_fast(X2, Y2, curve);           /* t3 = (y2 - y1)^2 = D */
    ecoflow_uECC_vli_modSub(X2, X2, t6, curve->p, num_words); /* t3 = D - (B + C) = x3 */

    ecoflow_uECC_vli_modSub(t7, X1, X2, curve->p, num_words); /* t7 = B - x3 */
    ecoflow_uECC_vli_modMult_fast(Y2, Y2, t7, curve);         /* t4 = (y2 - y1)*(B - x3) */
    ecoflow_uECC_vli_modSub(Y2, Y2, Y1, curve->p, num_words); /* t4 = (y2 - y1)*(B - x3) - E = y3 */

    ecoflow_uECC_vli_modSquare_fast(t7, t5, curve);           /* t7 = (y2 + y1)^2 = F */
    ecoflow_uECC_vli_modSub(t7, t7, t6, curve->p, num_words); /* t7 = F - (B + C) = x3' */
    ecoflow_uECC_vli_modSub(t6, t7, X1, curve->p, num_words); /* t6 = x3' - B */
    ecoflow_uECC_vli_modMult_fast(t6, t6, t5, curve);         /* t6 = (y2+y1)*(x3' - B) */
    ecoflow_uECC_vli_modSub(Y1, t6, Y1, curve->p, num_words); /* t2 = (y2+y1)*(x3' - B) - E = y3' */

    ecoflow_uECC_vli_set(X1, t7, num_words);                  /* move x3' to output */
}

/* result may overlap point. */
static void EccPoint_mult(ecoflow_uECC_word_t * result,
                          const ecoflow_uECC_word_t * point,
                          const ecoflow_uECC_word_t * scalar,
                          const ecoflow_uECC_word_t * initial_Z,
                          bitcount_t num_bits,
                          ecoflow_uECC_Curve curve) {
    /* R0 and R1 */
    ecoflow_uECC_word_t Rx[2][ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t Ry[2][ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t z[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t sub[ecoflow_uECC_MAX_WORDS];
    bitcount_t i;
    ecoflow_uECC_word_t nb;
    wordcount_t num_words = curve->num_words;

    ecoflow_uECC_vli_set(Rx[1], point, num_words);
    ecoflow_uECC_vli_set(Ry[1], point + num_words, num_words);

    XYcZ_initial_double(Rx[1], Ry[1], Rx[0], Ry[0], initial_Z, curve);
    ecoflow_uECC_vli_modSub(sub, Rx[0], Rx[1], curve->p, num_words);

    for (i = num_bits - 2; i > 0; --i) {
        nb = !ecoflow_uECC_vli_testBit(scalar, i);
        XYcZ_addC(Rx[1 - nb], Ry[1 - nb], Rx[nb], Ry[nb], sub, curve);
        XYcZ_add(Rx[nb], Ry[nb], Rx[1 - nb], Ry[1 - nb], sub, curve);
    }

    nb = !ecoflow_uECC_vli_testBit(scalar, 0);
    XYcZ_addC(Rx[1 - nb], Ry[1 - nb], Rx[nb], Ry[nb], sub, curve);

    /* Find final 1/Z value. */
    ecoflow_uECC_vli_modSub(z, Rx[1], Rx[0], curve->p, num_words); /* X1 - X0 */
    ecoflow_uECC_vli_modMult_fast(z, z, Ry[1 - nb], curve);        /* Yb * (X1 - X0) */
    ecoflow_uECC_vli_modMult_fast(z, z, point, curve);             /* xP * Yb * (X1 - X0) */
    ecoflow_uECC_vli_modInv(z, z, curve->p, num_words);            /* 1 / (xP * Yb * (X1 - X0)) */
    ecoflow_uECC_vli_modMult_fast(z, z, point + num_words, curve); /* yP / (xP * Yb * (X1 - X0)) */
    ecoflow_uECC_vli_modMult_fast(z, z, Rx[1 - nb], curve);        /* Xb * yP / (xP * Yb * (X1 - X0)) */
    /* End 1/Z calculation */

    XYcZ_add(Rx[nb], Ry[nb], Rx[1 - nb], Ry[1 - nb], sub, curve);
    apply_z(Rx[0], Ry[0], z, curve);

    ecoflow_uECC_vli_set(result, Rx[0], num_words);
    ecoflow_uECC_vli_set(result + num_words, Ry[0], num_words);
}

static ecoflow_uECC_word_t regularize_k(const ecoflow_uECC_word_t * const k,
                                ecoflow_uECC_word_t *k0,
                                ecoflow_uECC_word_t *k1,
                                ecoflow_uECC_Curve curve) {
    wordcount_t num_n_words = BITS_TO_WORDS(curve->num_n_bits);
    bitcount_t num_n_bits = curve->num_n_bits;
    ecoflow_uECC_word_t carry = ecoflow_uECC_vli_add(k0, k, curve->n, num_n_words) ||
        (num_n_bits < ((bitcount_t)num_n_words * ecoflow_uECC_WORD_SIZE * 8) &&
         ecoflow_uECC_vli_testBit(k0, num_n_bits));
    ecoflow_uECC_vli_add(k1, k0, curve->n, num_n_words);
    return carry;
}

/* Generates a random integer in the range 0 < random < top.
   Both random and top have num_words words. */
ecoflow_uECC_VLI_API int ecoflow_uECC_generate_random_int(ecoflow_uECC_word_t *random, 
                                          const ecoflow_uECC_word_t *top, 
                                          wordcount_t num_words) {
    ecoflow_uECC_word_t mask = (ecoflow_uECC_word_t)-1;
    ecoflow_uECC_word_t tries;
    bitcount_t num_bits = ecoflow_uECC_vli_numBits(top, num_words);

    if (!g_rng_function) {
        return 0;
    }

    for (tries = 0; tries < ecoflow_uECC_RNG_MAX_TRIES; ++tries) {
        if (!g_rng_function((uint8_t *)random, num_words * ecoflow_uECC_WORD_SIZE)) {
            return 0;
        }
        random[num_words - 1] &= mask >> ((bitcount_t)(num_words * ecoflow_uECC_WORD_SIZE * 8 - num_bits));
        if (!ecoflow_uECC_vli_isZero(random, num_words) && 
                ecoflow_uECC_vli_cmp(top, random, num_words) == 1) {
            return 1;
        }
    }
    return 0;
}

static ecoflow_uECC_word_t EccPoint_compute_public_key(ecoflow_uECC_word_t *result,
                                               ecoflow_uECC_word_t *private_key,
                                               ecoflow_uECC_Curve curve) {
    ecoflow_uECC_word_t tmp1[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t tmp2[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t *p2[2] = {tmp1, tmp2};
    ecoflow_uECC_word_t *initial_Z = 0;
    ecoflow_uECC_word_t carry;

    /* Regularize the bitcount for the private key so that attackers cannot use a side channel
       attack to learn the number of leading zeros. */
    carry = regularize_k(private_key, tmp1, tmp2, curve);

    /* If an RNG function was specified, try to get a random initial Z value to improve
       protection against side-channel attacks. */
    if (g_rng_function) {
        if (!ecoflow_uECC_generate_random_int(p2[carry], curve->p, curve->num_words)) {
            return 0;
        }
        initial_Z = p2[carry];
    }
    EccPoint_mult(result, curve->G, p2[!carry], initial_Z, curve->num_n_bits + 1, curve);

    if (EccPoint_isZero(result, curve)) {
        return 0;
    }
    return 1;
}

#if ecoflow_uECC_WORD_SIZE == 1

ecoflow_uECC_VLI_API void ecoflow_uECC_vli_nativeToBytes(uint8_t *bytes,
                                         int num_bytes,
                                         const uint8_t *native) {
    wordcount_t i;
    for (i = 0; i < num_bytes; ++i) {
        bytes[i] = native[(num_bytes - 1) - i];
    }
}

ecoflow_uECC_VLI_API void ecoflow_uECC_vli_bytesToNative(uint8_t *native,
                                         const uint8_t *bytes,
                                         int num_bytes) {
    ecoflow_uECC_vli_nativeToBytes(native, num_bytes, bytes);
}

#else

ecoflow_uECC_VLI_API void ecoflow_uECC_vli_nativeToBytes(uint8_t *bytes,
                                         int num_bytes,
                                         const ecoflow_uECC_word_t *native) {
    int i;
    for (i = 0; i < num_bytes; ++i) {
        unsigned b = num_bytes - 1 - i;
        bytes[i] = native[b / ecoflow_uECC_WORD_SIZE] >> (8 * (b % ecoflow_uECC_WORD_SIZE));
    }
}

ecoflow_uECC_VLI_API void ecoflow_uECC_vli_bytesToNative(ecoflow_uECC_word_t *native,
                                         const uint8_t *bytes,
                                         int num_bytes) {
    int i;
    ecoflow_uECC_vli_clear(native, (num_bytes + (ecoflow_uECC_WORD_SIZE - 1)) / ecoflow_uECC_WORD_SIZE);
    for (i = 0; i < num_bytes; ++i) {
        unsigned b = num_bytes - 1 - i;
        native[b / ecoflow_uECC_WORD_SIZE] |= 
            (ecoflow_uECC_word_t)bytes[i] << (8 * (b % ecoflow_uECC_WORD_SIZE));
    }
}

#endif /* ecoflow_uECC_WORD_SIZE */

int ecoflow_uECC_make_key(uint8_t *public_key,
                  uint8_t *private_key,
                  ecoflow_uECC_Curve curve) {
#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    ecoflow_uECC_word_t *_private = (ecoflow_uECC_word_t *)private_key;
    ecoflow_uECC_word_t *_public = (ecoflow_uECC_word_t *)public_key;
#else
    ecoflow_uECC_word_t _private[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t _public[ecoflow_uECC_MAX_WORDS * 2];
#endif
    ecoflow_uECC_word_t tries;

    for (tries = 0; tries < ecoflow_uECC_RNG_MAX_TRIES; ++tries) {
        if (!ecoflow_uECC_generate_random_int(_private, curve->n, BITS_TO_WORDS(curve->num_n_bits))) {
            return 0;
        }

        if (EccPoint_compute_public_key(_public, _private, curve)) {
#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN == 0
            ecoflow_uECC_vli_nativeToBytes(private_key, BITS_TO_BYTES(curve->num_n_bits), _private);
            ecoflow_uECC_vli_nativeToBytes(public_key, curve->num_bytes, _public);
            ecoflow_uECC_vli_nativeToBytes(
                public_key + curve->num_bytes, curve->num_bytes, _public + curve->num_words);
#endif
            return 1;
        }
    }
    return 0;
}

int ecoflow_uECC_shared_secret(const uint8_t *public_key,
                       const uint8_t *private_key,
                       uint8_t *secret,
                       ecoflow_uECC_Curve curve) {
    ecoflow_uECC_word_t _public[ecoflow_uECC_MAX_WORDS * 2];
    ecoflow_uECC_word_t _private[ecoflow_uECC_MAX_WORDS];

    ecoflow_uECC_word_t tmp[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t *p2[2] = {_private, tmp};
    ecoflow_uECC_word_t *initial_Z = 0;
    ecoflow_uECC_word_t carry;
    wordcount_t num_words = curve->num_words;
    wordcount_t num_bytes = curve->num_bytes;

#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    bcopy((uint8_t *) _private, private_key, num_bytes);
    bcopy((uint8_t *) _public, public_key, num_bytes*2);
#else
    ecoflow_uECC_vli_bytesToNative(_private, private_key, BITS_TO_BYTES(curve->num_n_bits));
    ecoflow_uECC_vli_bytesToNative(_public, public_key, num_bytes);
    ecoflow_uECC_vli_bytesToNative(_public + num_words, public_key + num_bytes, num_bytes);
#endif

    /* Regularize the bitcount for the private key so that attackers cannot use a side channel
       attack to learn the number of leading zeros. */
    carry = regularize_k(_private, _private, tmp, curve);

    /* If an RNG function was specified, try to get a random initial Z value to improve
       protection against side-channel attacks. */
    if (g_rng_function) {
        if (!ecoflow_uECC_generate_random_int(p2[carry], curve->p, num_words)) {
            return 0;
        }
        initial_Z = p2[carry];
    }

    EccPoint_mult(_public, _public, p2[!carry], initial_Z, curve->num_n_bits + 1, curve);
#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    bcopy((uint8_t *) secret, (uint8_t *) _public, num_bytes);
#else
    ecoflow_uECC_vli_nativeToBytes(secret, num_bytes, _public);
#endif
    return !EccPoint_isZero(_public, curve);
}

#if ecoflow_uECC_SUPPORT_COMPRESSED_POINT
void ecoflow_uECC_compress(const uint8_t *public_key, uint8_t *compressed, ecoflow_uECC_Curve curve) {
    wordcount_t i;
    for (i = 0; i < curve->num_bytes; ++i) {
        compressed[i+1] = public_key[i];
    }
#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    compressed[0] = 2 + (public_key[curve->num_bytes] & 0x01);
#else
    compressed[0] = 2 + (public_key[curve->num_bytes * 2 - 1] & 0x01);
#endif
}

void ecoflow_uECC_decompress(const uint8_t *compressed, uint8_t *public_key, ecoflow_uECC_Curve curve) {
#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    ecoflow_uECC_word_t *point = (ecoflow_uECC_word_t *)public_key;
#else
    ecoflow_uECC_word_t point[ecoflow_uECC_MAX_WORDS * 2];
#endif
    ecoflow_uECC_word_t *y = point + curve->num_words;
#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    bcopy(public_key, compressed+1, curve->num_bytes);
#else
    ecoflow_uECC_vli_bytesToNative(point, compressed + 1, curve->num_bytes);
#endif
    curve->x_side(y, point, curve);
    curve->mod_sqrt(y, curve);

    if ((y[0] & 0x01) != (compressed[0] & 0x01)) {
        ecoflow_uECC_vli_sub(y, curve->p, y, curve->num_words);
    }

#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN == 0
    ecoflow_uECC_vli_nativeToBytes(public_key, curve->num_bytes, point);
    ecoflow_uECC_vli_nativeToBytes(public_key + curve->num_bytes, curve->num_bytes, y);
#endif
}
#endif /* ecoflow_uECC_SUPPORT_COMPRESSED_POINT */

ecoflow_uECC_VLI_API int ecoflow_uECC_valid_point(const ecoflow_uECC_word_t *point, ecoflow_uECC_Curve curve) {
    ecoflow_uECC_word_t tmp1[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t tmp2[ecoflow_uECC_MAX_WORDS];
    wordcount_t num_words = curve->num_words;

    /* The point at infinity is invalid. */
    if (EccPoint_isZero(point, curve)) {
        return 0;
    }

    /* x and y must be smaller than p. */
    if (ecoflow_uECC_vli_cmp_unsafe(curve->p, point, num_words) != 1 || 
            ecoflow_uECC_vli_cmp_unsafe(curve->p, point + num_words, num_words) != 1) {
        return 0;
    }

    ecoflow_uECC_vli_modSquare_fast(tmp1, point + num_words, curve);
    curve->x_side(tmp2, point, curve); /* tmp2 = x^3 + ax + b */

    /* Make sure that y^2 == x^3 + ax + b */
    return (int)(ecoflow_uECC_vli_equal(tmp1, tmp2, num_words));
}

int ecoflow_uECC_valid_public_key(const uint8_t *public_key, ecoflow_uECC_Curve curve) {
#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    ecoflow_uECC_word_t *_public = (ecoflow_uECC_word_t *)public_key;
#else
    ecoflow_uECC_word_t _public[ecoflow_uECC_MAX_WORDS * 2];
#endif

#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN == 0
    ecoflow_uECC_vli_bytesToNative(_public, public_key, curve->num_bytes);
    ecoflow_uECC_vli_bytesToNative(
        _public + curve->num_words, public_key + curve->num_bytes, curve->num_bytes);
#endif
    return ecoflow_uECC_valid_point(_public, curve);
}

int ecoflow_uECC_compute_public_key(const uint8_t *private_key, uint8_t *public_key, ecoflow_uECC_Curve curve) {
#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    ecoflow_uECC_word_t *_private = (ecoflow_uECC_word_t *)private_key;
    ecoflow_uECC_word_t *_public = (ecoflow_uECC_word_t *)public_key;
#else
    ecoflow_uECC_word_t _private[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t _public[ecoflow_uECC_MAX_WORDS * 2];
#endif

#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN == 0
    ecoflow_uECC_vli_bytesToNative(_private, private_key, BITS_TO_BYTES(curve->num_n_bits));
#endif

    /* Make sure the private key is in the range [1, n-1]. */
    if (ecoflow_uECC_vli_isZero(_private, BITS_TO_WORDS(curve->num_n_bits))) {
        return 0;
    }

    if (ecoflow_uECC_vli_cmp(curve->n, _private, BITS_TO_WORDS(curve->num_n_bits)) != 1) {
        return 0;
    }

    /* Compute public key. */
    if (!EccPoint_compute_public_key(_public, _private, curve)) {
        return 0;
    }

#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN == 0
    ecoflow_uECC_vli_nativeToBytes(public_key, curve->num_bytes, _public);
    ecoflow_uECC_vli_nativeToBytes(
        public_key + curve->num_bytes, curve->num_bytes, _public + curve->num_words);
#endif
    return 1;
}


/* -------- ECDSA code -------- */

static void bits2int(ecoflow_uECC_word_t *native,
                     const uint8_t *bits,
                     unsigned bits_size,
                     ecoflow_uECC_Curve curve) {
    unsigned num_n_bytes = BITS_TO_BYTES(curve->num_n_bits);
    unsigned num_n_words = BITS_TO_WORDS(curve->num_n_bits);
    int shift;
    ecoflow_uECC_word_t carry;
    ecoflow_uECC_word_t *ptr;

    if (bits_size > num_n_bytes) {
        bits_size = num_n_bytes;
    }

    ecoflow_uECC_vli_clear(native, num_n_words);
#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    bcopy((uint8_t *) native, bits, bits_size);
#else
    ecoflow_uECC_vli_bytesToNative(native, bits, bits_size);
#endif
    if (bits_size * 8 <= (unsigned)curve->num_n_bits) {
        return;
    }
    shift = bits_size * 8 - curve->num_n_bits;
    carry = 0;
    ptr = native + num_n_words;
    while (ptr-- > native) {
        ecoflow_uECC_word_t temp = *ptr;
        *ptr = (temp >> shift) | carry;
        carry = temp << (ecoflow_uECC_WORD_BITS - shift);
    }

    /* Reduce mod curve_n */
    if (ecoflow_uECC_vli_cmp_unsafe(curve->n, native, num_n_words) != 1) {
        ecoflow_uECC_vli_sub(native, native, curve->n, num_n_words);
    }
}

static int ecoflow_uECC_sign_with_k_internal(const uint8_t *private_key,
                            const uint8_t *message_hash,
                            unsigned hash_size,
                            ecoflow_uECC_word_t *k,
                            uint8_t *signature,
                            ecoflow_uECC_Curve curve) {

    ecoflow_uECC_word_t tmp[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t s[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t *k2[2] = {tmp, s};
    ecoflow_uECC_word_t *initial_Z = 0;
#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    ecoflow_uECC_word_t *p = (ecoflow_uECC_word_t *)signature;
#else
    ecoflow_uECC_word_t p[ecoflow_uECC_MAX_WORDS * 2];
#endif
    ecoflow_uECC_word_t carry;
    wordcount_t num_words = curve->num_words;
    wordcount_t num_n_words = BITS_TO_WORDS(curve->num_n_bits);
    bitcount_t num_n_bits = curve->num_n_bits;

    /* Make sure 0 < k < curve_n */
    if (ecoflow_uECC_vli_isZero(k, num_words) || ecoflow_uECC_vli_cmp(curve->n, k, num_n_words) != 1) {
        return 0;
    }

    carry = regularize_k(k, tmp, s, curve);
    /* If an RNG function was specified, try to get a random initial Z value to improve
       protection against side-channel attacks. */
    if (g_rng_function) {
        if (!ecoflow_uECC_generate_random_int(k2[carry], curve->p, num_words)) {
            return 0;
        }
        initial_Z = k2[carry];
    }
    EccPoint_mult(p, curve->G, k2[!carry], initial_Z, num_n_bits + 1, curve);
    if (ecoflow_uECC_vli_isZero(p, num_words)) {
        return 0;
    }

    /* If an RNG function was specified, get a random number
       to prevent side channel analysis of k. */
    if (!g_rng_function) {
        ecoflow_uECC_vli_clear(tmp, num_n_words);
        tmp[0] = 1;
    } else if (!ecoflow_uECC_generate_random_int(tmp, curve->n, num_n_words)) {
        return 0;
    }

    /* Prevent side channel analysis of ecoflow_uECC_vli_modInv() to determine
       bits of k / the private key by premultiplying by a random number */
    ecoflow_uECC_vli_modMult(k, k, tmp, curve->n, num_n_words); /* k' = rand * k */
    ecoflow_uECC_vli_modInv(k, k, curve->n, num_n_words);       /* k = 1 / k' */
    ecoflow_uECC_vli_modMult(k, k, tmp, curve->n, num_n_words); /* k = 1 / k */

#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN == 0
    ecoflow_uECC_vli_nativeToBytes(signature, curve->num_bytes, p); /* store r */
#endif

#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    bcopy((uint8_t *) tmp, private_key, BITS_TO_BYTES(curve->num_n_bits));
#else
    ecoflow_uECC_vli_bytesToNative(tmp, private_key, BITS_TO_BYTES(curve->num_n_bits)); /* tmp = d */
#endif

    s[num_n_words - 1] = 0;
    ecoflow_uECC_vli_set(s, p, num_words);
    ecoflow_uECC_vli_modMult(s, tmp, s, curve->n, num_n_words); /* s = r*d */

    bits2int(tmp, message_hash, hash_size, curve);
    ecoflow_uECC_vli_modAdd(s, tmp, s, curve->n, num_n_words); /* s = e + r*d */
    ecoflow_uECC_vli_modMult(s, s, k, curve->n, num_n_words);  /* s = (e + r*d) / k */
    if (ecoflow_uECC_vli_numBits(s, num_n_words) > (bitcount_t)curve->num_bytes * 8) {
        return 0;
    }
#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    bcopy((uint8_t *) signature + curve->num_bytes, (uint8_t *) s, curve->num_bytes);
#else
    ecoflow_uECC_vli_nativeToBytes(signature + curve->num_bytes, curve->num_bytes, s);
#endif
    return 1;
}

/* For testing - sign with an explicitly specified k value */
int ecoflow_uECC_sign_with_k(const uint8_t *private_key,
                            const uint8_t *message_hash,
                            unsigned hash_size,
                            const uint8_t *k,
                            uint8_t *signature,
                            ecoflow_uECC_Curve curve) {
    ecoflow_uECC_word_t k2[ecoflow_uECC_MAX_WORDS];
    bits2int(k2, k, BITS_TO_BYTES(curve->num_n_bits), curve);
    return ecoflow_uECC_sign_with_k_internal(private_key, message_hash, hash_size, k2, signature, curve);
}

int ecoflow_uECC_sign(const uint8_t *private_key,
              const uint8_t *message_hash,
              unsigned hash_size,
              uint8_t *signature,
              ecoflow_uECC_Curve curve) {
    ecoflow_uECC_word_t k[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t tries;

    for (tries = 0; tries < ecoflow_uECC_RNG_MAX_TRIES; ++tries) {
        if (!ecoflow_uECC_generate_random_int(k, curve->n, BITS_TO_WORDS(curve->num_n_bits))) {
            return 0;
        }

        if (ecoflow_uECC_sign_with_k_internal(private_key, message_hash, hash_size, k, signature, curve)) {
            return 1;
        }
    }
    return 0;
}

/* Compute an HMAC using K as a key (as in RFC 6979). Note that K is always
   the same size as the hash result size. */
static void HMAC_init(const ecoflow_uECC_HashContext *hash_context, const uint8_t *K) {
    uint8_t *pad = hash_context->tmp + 2 * hash_context->result_size;
    unsigned i;
    for (i = 0; i < hash_context->result_size; ++i)
        pad[i] = K[i] ^ 0x36;
    for (; i < hash_context->block_size; ++i)
        pad[i] = 0x36;

    hash_context->init_hash(hash_context);
    hash_context->update_hash(hash_context, pad, hash_context->block_size);
}

static void HMAC_update(const ecoflow_uECC_HashContext *hash_context,
                        const uint8_t *message,
                        unsigned message_size) {
    hash_context->update_hash(hash_context, message, message_size);
}

static void HMAC_finish(const ecoflow_uECC_HashContext *hash_context,
                        const uint8_t *K,
                        uint8_t *result) {
    uint8_t *pad = hash_context->tmp + 2 * hash_context->result_size;
    unsigned i;
    for (i = 0; i < hash_context->result_size; ++i)
        pad[i] = K[i] ^ 0x5c;
    for (; i < hash_context->block_size; ++i)
        pad[i] = 0x5c;

    hash_context->finish_hash(hash_context, result);

    hash_context->init_hash(hash_context);
    hash_context->update_hash(hash_context, pad, hash_context->block_size);
    hash_context->update_hash(hash_context, result, hash_context->result_size);
    hash_context->finish_hash(hash_context, result);
}

/* V = HMAC_K(V) */
static void update_V(const ecoflow_uECC_HashContext *hash_context, uint8_t *K, uint8_t *V) {
    HMAC_init(hash_context, K);
    HMAC_update(hash_context, V, hash_context->result_size);
    HMAC_finish(hash_context, K, V);
}

/* Deterministic signing, similar to RFC 6979. Differences are:
    * We just use H(m) directly rather than bits2octets(H(m))
      (it is not reduced modulo curve_n).
    * We generate a value for k (aka T) directly rather than converting endianness.

   Layout of hash_context->tmp: <K> | <V> | (1 byte overlapped 0x00 or 0x01) / <HMAC pad> */
int ecoflow_uECC_sign_deterministic(const uint8_t *private_key,
                            const uint8_t *message_hash,
                            unsigned hash_size,
                            const ecoflow_uECC_HashContext *hash_context,
                            uint8_t *signature,
                            ecoflow_uECC_Curve curve) {
    uint8_t *K = hash_context->tmp;
    uint8_t *V = K + hash_context->result_size;
    wordcount_t num_bytes = curve->num_bytes;
    wordcount_t num_n_words = BITS_TO_WORDS(curve->num_n_bits);
    bitcount_t num_n_bits = curve->num_n_bits;
    ecoflow_uECC_word_t tries;
    unsigned i;
    for (i = 0; i < hash_context->result_size; ++i) {
        V[i] = 0x01;
        K[i] = 0;
    }

    /* K = HMAC_K(V || 0x00 || int2octets(x) || h(m)) */
    HMAC_init(hash_context, K);
    V[hash_context->result_size] = 0x00;
    HMAC_update(hash_context, V, hash_context->result_size + 1);
    HMAC_update(hash_context, private_key, num_bytes);
    HMAC_update(hash_context, message_hash, hash_size);
    HMAC_finish(hash_context, K, K);

    update_V(hash_context, K, V);

    /* K = HMAC_K(V || 0x01 || int2octets(x) || h(m)) */
    HMAC_init(hash_context, K);
    V[hash_context->result_size] = 0x01;
    HMAC_update(hash_context, V, hash_context->result_size + 1);
    HMAC_update(hash_context, private_key, num_bytes);
    HMAC_update(hash_context, message_hash, hash_size);
    HMAC_finish(hash_context, K, K);

    update_V(hash_context, K, V);

    for (tries = 0; tries < ecoflow_uECC_RNG_MAX_TRIES; ++tries) {
        ecoflow_uECC_word_t T[ecoflow_uECC_MAX_WORDS];
        uint8_t *T_ptr = (uint8_t *)T;
        wordcount_t T_bytes = 0;
        for (;;) {
            update_V(hash_context, K, V);
            for (i = 0; i < hash_context->result_size; ++i) {
                T_ptr[T_bytes++] = V[i];
                if (T_bytes >= num_n_words * ecoflow_uECC_WORD_SIZE) {
                    goto filled;
                }
            }
        }
    filled:
        if ((bitcount_t)num_n_words * ecoflow_uECC_WORD_SIZE * 8 > num_n_bits) {
            ecoflow_uECC_word_t mask = (ecoflow_uECC_word_t)-1;
            T[num_n_words - 1] &=
                mask >> ((bitcount_t)(num_n_words * ecoflow_uECC_WORD_SIZE * 8 - num_n_bits));
        }

        if (ecoflow_uECC_sign_with_k_internal(private_key, message_hash, hash_size, T, signature, curve)) {
            return 1;
        }

        /* K = HMAC_K(V || 0x00) */
        HMAC_init(hash_context, K);
        V[hash_context->result_size] = 0x00;
        HMAC_update(hash_context, V, hash_context->result_size + 1);
        HMAC_finish(hash_context, K, K);

        update_V(hash_context, K, V);
    }
    return 0;
}

static bitcount_t smax(bitcount_t a, bitcount_t b) {
    return (a > b ? a : b);
}

int ecoflow_uECC_verify(const uint8_t *public_key,
                const uint8_t *message_hash,
                unsigned hash_size,
                const uint8_t *signature,
                ecoflow_uECC_Curve curve) {
    ecoflow_uECC_word_t u1[ecoflow_uECC_MAX_WORDS], u2[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t z[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t sum[ecoflow_uECC_MAX_WORDS * 2];
    ecoflow_uECC_word_t rx[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t ry[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t tx[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t ty[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t tz[ecoflow_uECC_MAX_WORDS];
    const ecoflow_uECC_word_t *points[4];
    const ecoflow_uECC_word_t *point;
    bitcount_t num_bits;
    bitcount_t i;
#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    ecoflow_uECC_word_t *_public = (ecoflow_uECC_word_t *)public_key;
#else
    ecoflow_uECC_word_t _public[ecoflow_uECC_MAX_WORDS * 2];
#endif
    ecoflow_uECC_word_t r[ecoflow_uECC_MAX_WORDS], s[ecoflow_uECC_MAX_WORDS];
    wordcount_t num_words = curve->num_words;
    wordcount_t num_n_words = BITS_TO_WORDS(curve->num_n_bits);

    rx[num_n_words - 1] = 0;
    r[num_n_words - 1] = 0;
    s[num_n_words - 1] = 0;

#if ecoflow_uECC_VLI_NATIVE_LITTLE_ENDIAN
    bcopy((uint8_t *) r, signature, curve->num_bytes);
    bcopy((uint8_t *) s, signature + curve->num_bytes, curve->num_bytes);
#else
    ecoflow_uECC_vli_bytesToNative(_public, public_key, curve->num_bytes);
    ecoflow_uECC_vli_bytesToNative(
        _public + num_words, public_key + curve->num_bytes, curve->num_bytes);
    ecoflow_uECC_vli_bytesToNative(r, signature, curve->num_bytes);
    ecoflow_uECC_vli_bytesToNative(s, signature + curve->num_bytes, curve->num_bytes);
#endif

    /* r, s must not be 0. */
    if (ecoflow_uECC_vli_isZero(r, num_words) || ecoflow_uECC_vli_isZero(s, num_words)) {
        return 0;
    }

    /* r, s must be < n. */
    if (ecoflow_uECC_vli_cmp_unsafe(curve->n, r, num_n_words) != 1 || 
            ecoflow_uECC_vli_cmp_unsafe(curve->n, s, num_n_words) != 1) {
        return 0;
    }

    /* Calculate u1 and u2. */
    ecoflow_uECC_vli_modInv(z, s, curve->n, num_n_words); /* z = 1/s */
    u1[num_n_words - 1] = 0;
    bits2int(u1, message_hash, hash_size, curve);
    ecoflow_uECC_vli_modMult(u1, u1, z, curve->n, num_n_words); /* u1 = e/s */
    ecoflow_uECC_vli_modMult(u2, r, z, curve->n, num_n_words); /* u2 = r/s */

    /* Calculate sum = G + Q. */
    ecoflow_uECC_vli_set(sum, _public, num_words);
    ecoflow_uECC_vli_set(sum + num_words, _public + num_words, num_words);
    ecoflow_uECC_vli_set(tx, curve->G, num_words);
    ecoflow_uECC_vli_set(ty, curve->G + num_words, num_words);
    ecoflow_uECC_vli_modSub(z, sum, tx, curve->p, num_words); /* z = x2 - x1 */
    /* Note: safe to use tx for 'sub' param, since tx is not used after XYcZ_add. */
    XYcZ_add(tx, ty, sum, sum + num_words, tx, curve);
    ecoflow_uECC_vli_modInv(z, z, curve->p, num_words); /* z = 1/z */
    apply_z(sum, sum + num_words, z, curve);

    /* Use Shamir's trick to calculate u1*G + u2*Q */
    points[0] = 0;
    points[1] = curve->G;
    points[2] = _public;
    points[3] = sum;
    num_bits = smax(ecoflow_uECC_vli_numBits(u1, num_n_words),
                    ecoflow_uECC_vli_numBits(u2, num_n_words));

    point = points[(!!ecoflow_uECC_vli_testBit(u1, num_bits - 1)) |
                   ((!!ecoflow_uECC_vli_testBit(u2, num_bits - 1)) << 1)];
    ecoflow_uECC_vli_set(rx, point, num_words);
    ecoflow_uECC_vli_set(ry, point + num_words, num_words);
    ecoflow_uECC_vli_clear(z, num_words);
    z[0] = 1;

    for (i = num_bits - 2; i >= 0; --i) {
        ecoflow_uECC_word_t index;
        curve->double_jacobian(rx, ry, z, curve);

        index = (!!ecoflow_uECC_vli_testBit(u1, i)) | ((!!ecoflow_uECC_vli_testBit(u2, i)) << 1);
        point = points[index];
        if (point) {
            ecoflow_uECC_vli_set(tx, point, num_words);
            ecoflow_uECC_vli_set(ty, point + num_words, num_words);
            apply_z(tx, ty, z, curve);
            ecoflow_uECC_vli_modSub(tz, rx, tx, curve->p, num_words); /* Z = x2 - x1 */
            XYcZ_add(tx, ty, rx, ry, tx, curve);
            ecoflow_uECC_vli_modMult_fast(z, z, tz, curve);
        }
    }

    ecoflow_uECC_vli_modInv(z, z, curve->p, num_words); /* Z = 1/Z */
    apply_z(rx, ry, z, curve);

    /* v = x1 (mod n) */
    if (ecoflow_uECC_vli_cmp_unsafe(curve->n, rx, num_n_words) != 1) {
        ecoflow_uECC_vli_sub(rx, rx, curve->n, num_n_words);
    }

    /* Accept only if v == r. */
    return (int)(ecoflow_uECC_vli_equal(rx, r, num_words));
}

#if ecoflow_uECC_ENABLE_VLI_API

unsigned ecoflow_uECC_curve_num_words(ecoflow_uECC_Curve curve) {
    return curve->num_words;
}

unsigned ecoflow_uECC_curve_num_bytes(ecoflow_uECC_Curve curve) {
    return curve->num_bytes;
}

unsigned ecoflow_uECC_curve_num_bits(ecoflow_uECC_Curve curve) {
    return curve->num_bytes * 8;
}

unsigned ecoflow_uECC_curve_num_n_words(ecoflow_uECC_Curve curve) {
    return BITS_TO_WORDS(curve->num_n_bits);
}

unsigned ecoflow_uECC_curve_num_n_bytes(ecoflow_uECC_Curve curve) {
    return BITS_TO_BYTES(curve->num_n_bits);
}

unsigned ecoflow_uECC_curve_num_n_bits(ecoflow_uECC_Curve curve) {
    return curve->num_n_bits;
}

const ecoflow_uECC_word_t *ecoflow_uECC_curve_p(ecoflow_uECC_Curve curve) {
    return curve->p;
}

const ecoflow_uECC_word_t *ecoflow_uECC_curve_n(ecoflow_uECC_Curve curve) {
    return curve->n;
}

const ecoflow_uECC_word_t *ecoflow_uECC_curve_G(ecoflow_uECC_Curve curve) {
    return curve->G;
}

const ecoflow_uECC_word_t *ecoflow_uECC_curve_b(ecoflow_uECC_Curve curve) {
    return curve->b;
}

#if ecoflow_uECC_SUPPORT_COMPRESSED_POINT
void ecoflow_uECC_vli_mod_sqrt(ecoflow_uECC_word_t *a, ecoflow_uECC_Curve curve) {
    curve->mod_sqrt(a, curve);
}
#endif

void ecoflow_uECC_vli_mmod_fast(ecoflow_uECC_word_t *result, ecoflow_uECC_word_t *product, ecoflow_uECC_Curve curve) {
#if (ecoflow_uECC_OPTIMIZATION_LEVEL > 0)
    curve->mmod_fast(result, product);
#else
    ecoflow_uECC_vli_mmod(result, product, curve->p, curve->num_words);
#endif
}

void ecoflow_uECC_point_mult(ecoflow_uECC_word_t *result,
                     const ecoflow_uECC_word_t *point,
                     const ecoflow_uECC_word_t *scalar,
                     ecoflow_uECC_Curve curve) {
    ecoflow_uECC_word_t tmp1[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t tmp2[ecoflow_uECC_MAX_WORDS];
    ecoflow_uECC_word_t *p2[2] = {tmp1, tmp2};
    ecoflow_uECC_word_t carry = regularize_k(scalar, tmp1, tmp2, curve);

    EccPoint_mult(result, point, p2[!carry], 0, curve->num_n_bits + 1, curve);
}

#endif /* ecoflow_uECC_ENABLE_VLI_API */