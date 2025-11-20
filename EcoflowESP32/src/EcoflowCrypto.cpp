#include "EcoflowCrypto.h"
#include <Arduino.h>
#include "mbedtls/md.h"
#include "Credentials.h"
#include "esp_log.h"
#include "esp_system.h" 
#include <cstring>
#include <vector> // Added for std::vector
#include "keydata.h" // Include for ECOFLOW_KEYDATA

// secp160r1 curve parameters
#define SECP160R1_P "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF"
#define SECP160R1_A "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFC"
#define SECP160R1_B "1C97BEFC54BD7A8B65ACF89F81D4D4ADC565FA45"
#define SECP160R1_GX "4A96B5688EF573284664698968C38BB913CBFC82"
#define SECP160R1_GY "23A628553168947D59DCC912042351377AC5FB32"
#define SECP160R1_N "01000000000000000001F4C8F927AED3CA752257"

#define MBEDTLS_AES_BLOCK_SIZE 16 // Define AES block size

EcoflowCrypto::EcoflowCrypto() {
    mbedtls_ecdh_init(&ecdh_ctx); // Initialize the full context
    mbedtls_aes_init(&aes_ctx);

    // Initialize individual components within ecdh_ctx that are accessed
    mbedtls_ecp_group_init(&ecdh_ctx.grp);
    mbedtls_mpi_init(&ecdh_ctx.d);
    mbedtls_ecp_point_init(&ecdh_ctx.Q);
    // Peer's public key and shared secret will be initialized as needed
    mbedtls_ecp_point_init(&ecdh_ctx.Qp);
    mbedtls_mpi_init(&ecdh_ctx.z);

    // Load custom curve parameters into ecdh_ctx.grp
    mbedtls_mpi_read_string(&ecdh_ctx.grp.P, 16, SECP160R1_P);
    mbedtls_mpi_read_string(&ecdh_ctx.grp.A, 16, SECP160R1_A);
    mbedtls_mpi_read_string(&ecdh_ctx.grp.B, 16, SECP160R1_B);
    
    // Initialize and set the generator point G
    mbedtls_mpi_read_string(&ecdh_ctx.grp.G.X, 16, SECP160R1_GX);
    mbedtls_mpi_read_string(&ecdh_ctx.grp.G.Y, 16, SECP160R1_GY);
    mbedtls_mpi_lset(&ecdh_ctx.grp.G.Z, 1); // Z-coordinate for affine coordinates is 1
    
    ecdh_ctx.grp.pbits = 160;
    ecdh_ctx.grp.nbits = 160;
    ecdh_ctx.grp.id = MBEDTLS_ECP_DP_SECP192R1; // Placeholder ID as per original comment
}

EcoflowCrypto::~EcoflowCrypto() {
    mbedtls_ecdh_free(&ecdh_ctx); // Free the full context
    mbedtls_aes_free(&aes_ctx);
}

int EcoflowCrypto::f_rng(void* p_rng, unsigned char* output, size_t output_len) {
    esp_fill_random(output, output_len);
    return 0;
}

bool EcoflowCrypto::generate_keys() {
    // mbedtls_ecdh_gen_public generates the private key (d) and public key (Q) for the context
    if (mbedtls_ecdh_gen_public(&ecdh_ctx.grp, &ecdh_ctx.d, &ecdh_ctx.Q, f_rng, nullptr) != 0) {
        return false;
    }
    size_t pub_len;
    // Write the generated public key Q to the public_key buffer
    return mbedtls_ecp_point_write_binary(&ecdh_ctx.grp, &ecdh_ctx.Q, MBEDTLS_ECP_PF_UNCOMPRESSED, &pub_len, public_key, sizeof(public_key)) == 0;
}

bool EcoflowCrypto::compute_shared_secret(const uint8_t* peer_pub_key, size_t peer_pub_key_len) {
    // Read the peer's public key into ecdh_ctx.Qp
    if (mbedtls_ecp_point_read_binary(&ecdh_ctx.grp, &ecdh_ctx.Qp, peer_pub_key, peer_pub_key_len) != 0) {
        return false;
    }

    // Compute the shared secret (z) using our private key (d) and peer's public key (Qp)
    if (mbedtls_ecdh_compute_shared(&ecdh_ctx.grp, &ecdh_ctx.z, &ecdh_ctx.Qp, &ecdh_ctx.d, f_rng, nullptr) != 0) {
        return false;
    }

    // Write the shared secret z to the shared_secret buffer
    mbedtls_mpi_write_binary(&ecdh_ctx.z, shared_secret, sizeof(shared_secret));
    // Generate IV from MD5 hash of the full shared_secret
    mbedtls_md5(shared_secret, sizeof(shared_secret), iv);

    // The shared secret is 20 bytes, but the AES key needs to be 16 bytes.
    // The Python implementation truncates the secret for the AES key.
    // Ensure this is explicitly clear and done *after* IV generation.
    // No change needed here, as it matches the Python logic.
    // The shared_secret is already 20 bytes. We use its first 16 bytes.
    // This is effectively done by the memcpy in EcoflowCrypto::encrypt_session etc.

    return true;
}

void EcoflowCrypto::generate_session_key(const uint8_t* seed, const uint8_t* srand) {
    uint8_t data[32];
    uint64_t data_num[4];

    int pos = seed[0] * 0x10 + ((seed[1] - 1) & 0xFF) * 0x100;
    
    // ECOFLOW_KEYDATA is expected to be declared in keydata.h and defined in keydata.cpp
    memcpy(&data_num[0], &ECOFLOW_KEYDATA[pos], 8);
    memcpy(&data_num[1], &ECOFLOW_KEYDATA[pos + 8], 8);
    memcpy(&data_num[2], srand, 8); // No +16 needed, srand is base pointer
    memcpy(&data_num[3], srand + 8, 8); // Offset by 8

    memcpy(data, &data_num[0], 8);
    memcpy(data + 8, &data_num[1], 8);
    memcpy(data + 16, &data_num[2], 8);
    memcpy(data + 24, &data_num[3], 8);

    mbedtls_md5(data, sizeof(data), session_key);
}

std::vector<uint8_t> EcoflowCrypto::pad(const std::vector<uint8_t>& data) {
    size_t padding_len = MBEDTLS_AES_BLOCK_SIZE - (data.size() % MBEDTLS_AES_BLOCK_SIZE);
    if (padding_len == 0) {
        padding_len = MBEDTLS_AES_BLOCK_SIZE; // Always add a full block of padding if already block aligned
    }
    std::vector<uint8_t> padded_data = data;
    padded_data.reserve(data.size() + padding_len); // Pre-allocate memory
    for (size_t i = 0; i < padding_len; ++i) {
        padded_data.push_back((uint8_t)padding_len);
    }
    return padded_data;
}

std::vector<uint8_t> EcoflowCrypto::unpad(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return data;
    }
    uint8_t padding_len = data.back();
    if (padding_len == 0 || padding_len > MBEDTLS_AES_BLOCK_SIZE || padding_len > data.size()) {
        // Invalid padding, or padding indicates size larger than data itself
        // This could indicate corrupted data or incorrect padding scheme.
        // For now, return original data or throw an error.
        // In a real scenario, you might want to log this or throw a specific exception.
        return data; 
    }
    return std::vector<uint8_t>(data.begin(), data.end() - padding_len);
}

std::vector<uint8_t> EcoflowCrypto::encrypt_session(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> padded_input = pad(input);
    std::vector<uint8_t> output(padded_input.size());

    // Use the first 16 bytes of shared_secret as the AES key
    mbedtls_aes_setkey_enc(&aes_ctx, session_key, 128); // 128-bit key = 16 bytes
    
    uint8_t temp_iv[16];
    memcpy(temp_iv, iv, 16); // Use a temporary IV as mbedtls_aes_crypt_cbc modifies it

    mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, padded_input.size(), temp_iv, padded_input.data(), output.data());
    
    return output;
}

std::vector<uint8_t> EcoflowCrypto::decrypt_session(const std::vector<uint8_t>& input) {
    // Input must be a multiple of block size for CBC decryption
    if (input.empty() || input.size() % MBEDTLS_AES_BLOCK_SIZE != 0) {
        // Invalid input size for CBC decryption, return empty or throw error
        return {}; 
    }

    std::vector<uint8_t> decrypted_output(input.size());

    // Use the first 16 bytes of shared_secret as the AES key
    mbedtls_aes_setkey_dec(&aes_ctx, session_key, 128); // 128-bit key = 16 bytes
    
    uint8_t temp_iv[16];
    memcpy(temp_iv, iv, 16); // Use a temporary IV as mbedtls_aes_crypt_cbc modifies it

    mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, input.size(), temp_iv, input.data(), decrypted_output.data());
    
    return unpad(decrypted_output);
}
