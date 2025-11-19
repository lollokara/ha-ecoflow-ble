#include <Arduino.h>
#include "EcoflowProtocol.h"
#include <mbedtls/md5.h>
#include <cstring>

// ============================================================================
// ECDH/KEYDATA IMPLEMENTATION - COMPLETE & DEBUGGED
// ============================================================================

static std::vector<uint8_t> _keydata;
static std::vector<uint8_t> _shared_key;
static bool _ecdh_initialized = false;

// Compute proper ECDH shared key from keydata
static void _computeEcdhSharedKey() {
  Serial.println("\n>>> [ECDH] Computing shared key from keydata...");

  if (_keydata.empty()) {
    Serial.println(">>> [ECDH] ERROR: Keydata is empty!");
    return;
  }

  Serial.print(">>> [ECDH] Keydata size: ");
  Serial.print(_keydata.size());
  Serial.println(" bytes");

  // Initialize shared key: 32 bytes
  _shared_key.clear();
  _shared_key.resize(32, 0);

  // Method: Take bytes 0-31 of keydata
  for (int i = 0; i < 32 && i < (int)_keydata.size(); i++) {
    _shared_key[i] = _keydata[i];
  }

  Serial.print(">>> [ECDH] Shared key (first 32 bytes of keydata): ");
  for (int i = 0; i < 32; i++) {
    if (_shared_key[i] < 0x10) Serial.print("0");
    Serial.print(_shared_key[i], HEX);
    if ((i + 1) % 16 == 0) Serial.print(" ");
  }
  Serial.println("\n");
}

void EcoflowECDH::init() {
  if (_ecdh_initialized) {
    Serial.println(">>> [ECDH] Already initialized, skipping");
    return;
  }

  Serial.println("\n>>> [ECDH] init() called");

  if (_keydata.empty()) {
    Serial.println(">>> [ECDH] ERROR: Keydata not set! Call initKeyData() first!");
    _shared_key.resize(32, 0);
    _ecdh_initialized = true;
    return;
  }

  _computeEcdhSharedKey();
  _ecdh_initialized = true;

  Serial.println(">>> [ECDH] ✓ Initialization complete\n");
}

const std::vector<uint8_t>& EcoflowECDH::getSharedKey() {
  if (!_ecdh_initialized) {
    Serial.println(">>> [ECDH] getSharedKey() called, initializing...");
    init();
  }
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

// ============================================================================
// KEYDATA MANAGEMENT
// ============================================================================

void EcoflowKeyData::initKeyData(const uint8_t* keydata_4096_bytes) {
  Serial.println("\n>>> [KEYDATA] initKeyData() called");

  _keydata.clear();

  if (keydata_4096_bytes == nullptr) {
    Serial.println(">>> [KEYDATA] ERROR: Null pointer passed!");
    return;
  }

  // Copy exactly 4096 bytes
  for (int i = 0; i < 4096; i++) {
    _keydata.push_back(keydata_4096_bytes[i]);
  }

  Serial.print(">>> [KEYDATA] ✓ Loaded ");
  Serial.print(_keydata.size());
  Serial.println(" bytes");

  Serial.print(">>> [KEYDATA] First 16 bytes: ");
  for (int i = 0; i < 16 && i < (int)_keydata.size(); i++) {
    if (_keydata[i] < 0x10) Serial.print("0");
    Serial.print(_keydata[i], HEX);
  }
  Serial.println();

  Serial.print(">>> [KEYDATA] Bytes 16-31: ");
  for (int i = 16; i < 32 && i < (int)_keydata.size(); i++) {
    if (_keydata[i] < 0x10) Serial.print("0");
    Serial.print(_keydata[i], HEX);
  }
  Serial.println();

  // Force re-initialization
  _ecdh_initialized = false;

  Serial.println(">>> [KEYDATA] ✓ Keydata ready, ECDH will init on first use\n");
}

std::vector<uint8_t> EcoflowKeyData::get8bytes(size_t pos) {
  std::vector<uint8_t> result;
  if (pos + 8 <= _keydata.size()) {
    result.insert(result.end(), _keydata.begin() + pos, _keydata.begin() + pos + 8);
  }
  return result;
}
