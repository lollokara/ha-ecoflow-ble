import ecdsa

# 1. Generate a static private key
private_key = ecdsa.SigningKey.generate(curve=ecdsa.SECP160r1)
public_key = private_key.get_verifying_key()

# 2. Simulate a peer's key pair
peer_private_key = ecdsa.SigningKey.generate(curve=ecdsa.SECP160r1)
peer_public_key = peer_private_key.get_verifying_key()

# 3. Compute the shared secret
shared_secret = ecdsa.ECDH(ecdsa.SECP160r1, private_key, peer_public_key).generate_sharedsecret_bytes()

# 4. Print all values in a C++-friendly format
def print_hex_array(name, data):
    print(f"const uint8_t {name}[] = {{")
    print("    " + ", ".join([f"0x{byte:02x}" for byte in data]))
    print("};")

print_hex_array("PRIVATE_KEY", private_key.to_string())
print_hex_array("PUBLIC_KEY", public_key.to_string("uncompressed"))
print_hex_array("PEER_PUBLIC_KEY", peer_public_key.to_string("uncompressed"))
print_hex_array("SHARED_SECRET", shared_secret)
