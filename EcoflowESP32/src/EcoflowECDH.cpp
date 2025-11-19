#include "EcoflowProtocol.h"
#include <vector>
#include <cstring>

// ============================================================================
// ECDH/KEYDATA STUB
// ============================================================================

static std::vector<uint8_t> _keydata;
static std::vector<uint8_t> _shared_key;
static bool _ecdh_initialized = false;

void EcoflowECDH::init() {
  if (_ecdh_initialized) return;
  
  // TODO: Populate _keydata from keydata.py base64 decode (4096 bytes)
  // For now: create placeholder
  _keydata.resize(4096, 0);
  
  // Perform ECDH computation - placeholder
  _shared_key.resize(256, 0);
  
  _ecdh_initialized = true;
}

const std::vector<uint8_t>& EcoflowECDH::getSharedKey() {
  if (!_ecdh_initialized) {
    init();
  }
  return _shared_key;
}

std::vector<uint8_t> EcoflowKeyData::get8bytes(size_t pos) {
  std::vector<uint8_t> result;
  
  if (pos + 8 <= _keydata.size()) {
    result.insert(result.end(), _keydata.begin() + pos, _keydata.begin() + pos + 8);
  }
  
  return result;
}
