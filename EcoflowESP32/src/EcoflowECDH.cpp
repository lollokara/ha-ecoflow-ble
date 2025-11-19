#include <Arduino.h>
#include "EcoflowProtocol.h"
#include "uECC.h"
#include <mbedtls/md5.h>
#include <cstring>
#include <esp_system.h>

// ============================================================================
// ECDH IMPLEMENTATION with micro-ecc
// ============================================================================

static uint8_t _private_key[uECC_BYTES];
static uint8_t _public_key[uECC_BYTES * 2];
static std::vector<uint8_t> _shared_key;
static bool _keys_generated = false;

// Custom RNG for micro-ecc using ESP32's hardware RNG
static int esp32_rng(uint8_t *dest, unsigned size) {
    esp_fill_random(dest, size);
    return 1;
}

void EcoflowECDH::init() {
    uECC_set_rng(&esp32_rng);
}

bool EcoflowECDH::generateKeys() {
    if (uECC_make_key(_public_key, _private_key)) {
        _keys_generated = true;
        return true;
    }
    return false;
}

const uint8_t* EcoflowECDH::getPublicKey() {
    return _public_key;
}

bool EcoflowECDH::computeSharedSecret(const uint8_t* device_public_key) {
    if (!_keys_generated) {
        return false;
    }
    uint8_t secret[uECC_BYTES];
    if (uECC_shared_secret(device_public_key, _private_key, secret)) {
        _shared_key.assign(secret, secret + uECC_BYTES);
        return true;
    }
    return false;
}

const std::vector<uint8_t>& EcoflowECDH::getSharedKey() {
  return _shared_key;
}

void EcoflowECDH::generateSessionKey(const uint8_t* sRand, const uint8_t* seed,
                                     uint8_t* out_key, uint8_t* out_iv) {
  Serial.println(">>> [SESSION] Generating session key...");

  const auto& shared_key = getSharedKey();

  // Debug: Show what we're using
  Serial.print(">>> [SESSION] Shared key size: ");
  Serial.println(shared_key.size());

  Serial.print(">>> [SESSION] Shared key[0:16]: ");
  for (int i = 0; i < 16 && i < (int)shared_key.size(); i++) {
    if (shared_key[i] < 0x10) Serial.print("0");
    Serial.print(shared_key[i], HEX);
  }
  Serial.println();

  // Session key: MD5(sRand || seed || shared_key[0:16])
  std::vector<uint8_t> session_input;
  session_input.insert(session_input.end(), sRand, sRand + 16);
  session_input.insert(session_input.end(), seed, seed + 2);
  if (shared_key.size() >= 16) {
    session_input.insert(session_input.end(), shared_key.begin(), shared_key.begin() + 16);
  }

  Serial.print(">>> [SESSION] Input (sRand||seed||shared[0:16]): ");
  for (size_t i = 0; i < session_input.size(); i++) {
    if (session_input[i] < 0x10) Serial.print("0");
    Serial.print(session_input[i], HEX);
    if ((i + 1) % 16 == 0) Serial.print(" ");
  }
  Serial.println();

  mbedtls_md5_context md5_session;
  mbedtls_md5_init(&md5_session);
  mbedtls_md5_starts(&md5_session);
  mbedtls_md5_update(&md5_session, session_input.data(), session_input.size());
  mbedtls_md5_finish(&md5_session, out_key);
  mbedtls_md5_free(&md5_session);

  Serial.print(">>> [SESSION] MD5 session key: ");
  for (int i = 0; i < 16; i++) {
    if (out_key[i] < 0x10) Serial.print("0");
    Serial.print(out_key[i], HEX);
  }
  Serial.println();

  // IV: MD5(seed || seed)
  uint8_t iv_input[4] = {seed[0], seed[1], seed[0], seed[1]};

  Serial.print(">>> [SESSION] IV input (seed||seed): ");
  for (int i = 0; i < 4; i++) {
    if (iv_input[i] < 0x10) Serial.print("0");
    Serial.print(iv_input[i], HEX);
  }
  Serial.println();

  mbedtls_md5_context md5_iv;
  mbedtls_md5_init(&md5_iv);
  mbedtls_md5_starts(&md5_iv);
  mbedtls_md5_update(&md5_iv, (const uint8_t*)iv_input, 4);
  mbedtls_md5_finish(&md5_iv, out_iv);
  mbedtls_md5_free(&md5_iv);

  Serial.print(">>> [SESSION] MD5 IV: ");
  for (int i = 0; i < 16; i++) {
    if (out_iv[i] < 0x10) Serial.print("0");
    Serial.print(out_iv[i], HEX);
  }
  Serial.println();

  Serial.println(">>> [SESSION] ✓ Session key & IV generated successfully\n");
}
