#include "EcoflowProtocol.h"

static std::vector<uint8_t> _keydata;

void EcoflowKeyData::initKeyData(const uint8_t* keydata_4096_bytes) {
  _keydata.assign(keydata_4096_bytes, keydata_4096_bytes + 4096);
}

std::vector<uint8_t> EcoflowKeyData::get8bytes(size_t pos) {
  std::vector<uint8_t> result;
  if (pos + 8 <= _keydata.size()) {
    result.insert(result.end(), _keydata.begin() + pos, _keydata.begin() + pos + 8);
  }
  return result;
}
