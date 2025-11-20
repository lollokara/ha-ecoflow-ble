#ifndef ECOFLOW_ECDH_H
#define ECOFLOW_ECDH_H

#include <stdint.h>
#include <stddef.h>

namespace EcoflowECDH {

bool generate_public_key(uint8_t* public_key, uint8_t* private_key);
bool compute_shared_secret(const uint8_t* peer_pub_key, uint8_t* shared_secret, const uint8_t* private_key);
void generateSessionKey(const uint8_t* seed, const uint8_t* srand, uint8_t* sessionKey);

} // namespace EcoflowECDH

#endif // ECOFLOW_ECDH_H
