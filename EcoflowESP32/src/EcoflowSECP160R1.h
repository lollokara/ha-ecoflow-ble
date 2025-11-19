#ifndef EcoflowSECP160R1_h
#define EcoflowSECP160R1_h

#include <cstdint>
#include <cstddef>

namespace EcoflowSECP160R1 {

int make_key(uint8_t* public_key, uint8_t* private_key);
int shared_secret(const uint8_t* public_key, const uint8_t* private_key, uint8_t* secret);

} // namespace EcoflowSECP160R1

#endif // EcoflowSECP160R1_h
