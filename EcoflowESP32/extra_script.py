Import("env")

env.Append(
    CPPDEFINES=[
        ("CONFIG_BT_NIMBLE_CRYPTO_STACK_MBEDTLS", 1),
        ("CONFIG_MBEDTLS_CMAC_C", 1)
    ]
)
