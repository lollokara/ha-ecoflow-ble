#include "EcoflowProtocol.h"
#include <mbedtls/md5.h>
#include <cstring>

// ============================================================================
// ECDH/KEYDATA IMPLEMENTATION
// ============================================================================

static std::vector<uint8_t> _keydata;
static std::vector<uint8_t> _shared_key;
static bool _ecdh_initialized = false;

void EcoflowECDH::init() {
  if (_ecdh_initialized) return;

  if (_keydata.empty()) {
    // Placeholder: needs to be populated by initKeyData() with actual 4096-byte keydata
    _keydata.resize(4096, 0);
  }

  // Placeholder ECDH computation
  _shared_key.resize(32, 0);
  _ecdh_initialized = true;
}

const std::vector<uint8_t>& EcoflowECDH::getSharedKey() {
  if (!_ecdh_initialized) init();
  return _shared_key;
}

void EcoflowECDH::generateSessionKey(const uint8_t* sRand, const uint8_t* seed,
                                     uint8_t* out_key, uint8_t* out_iv) {
  const auto& shared_key = getSharedKey();

  // Session key: MD5(sRand || seed || shared_key[0:16])
  std::vector<uint8_t> session_input;
  session_input.insert(session_input.end(), sRand, sRand + 16);
  session_input.insert(session_input.end(), seed, seed + 2);
  if (shared_key.size() >= 16) {
    session_input.insert(session_input.end(), shared_key.begin(), shared_key.begin() + 16);
  }

  mbedtls_md5_context md5_session;
  mbedtls_md5_init(&md5_session);
  mbedtls_md5_starts(&md5_session);
  mbedtls_md5_update(&md5_session, session_input.data(), session_input.size());
  mbedtls_md5_finish(&md5_session, out_key);
  mbedtls_md5_free(&md5_session);

  // IV: MD5(seed || seed)
  std::vector<uint8_t> iv_input(4);
  memcpy(iv_input.data(), seed, 2);
  memcpy(iv_input.data() + 2, seed, 2);

  mbedtls_md5_context md5_iv;
  mbedtls_md5_init(&md5_iv);
  mbedtls_md5_starts(&md5_iv);
  mbedtls_md5_update(&md5_iv, iv_input.data(), iv_input.size());
  mbedtls_md5_finish(&md5_iv, out_iv);
  mbedtls_md5_free(&md5_iv);
}

// ============================================================================
// KEYDATA MANAGEMENT
// ============================================================================

void EcoflowKeyData::initKeyData(const uint8_t* keydata_4096_bytes) {
  _keydata.clear();
  if (keydata_4096_bytes) {
    _keydata.insert(_keydata.end(), keydata_4096_bytes, keydata_4096_bytes + 4096);
  }
  _ecdh_initialized = false;  // Force re-init with new keydata
}

std::vector<uint8_t> EcoflowKeyData::get8bytes(size_t pos) {
  std::vector<uint8_t> result;
  if (pos + 8 <= _keydata.size()) {
    result.insert(result.end(), _keydata.begin() + pos, _keydata.begin() + pos + 8);
  }
  return result;
}
