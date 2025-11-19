#include "Ecoflow_mbedtls.h"
#include "mbedtls/bignum.h"

namespace Ecoflow_mbedtls {

// All values are big-endian hex strings

const char* secp160r1_p_hex = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF";
const char* secp160r1_a_hex = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFC";
const char* secp160r1_b_hex = "1C97BEFC54BD7A8B65ACF89F81D4D4ADC565FA45";

const char* secp160r1_g_x_hex = "4A96B5688EF573284664698968C38BB913CBFC82";
const char* secp160r1_g_y_hex = "23A628553168947D59DCC912042351377AC5FB32";
const char* secp160r1_n_hex = "0100000000000000000001F4C8F927AED3CA752257";

int load_secp160r1_group(mbedtls_ecp_group *grp) {
    int ret;

    mbedtls_ecp_group_init(grp);

    // Load prime and coefficients from hex strings
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&grp->P, 16, secp160r1_p_hex));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&grp->A, 16, secp160r1_a_hex));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&grp->B, 16, secp160r1_b_hex));

    // Load base point from hex string (X and Y coordinates separately)
    MBEDTLS_MPI_CHK(mbedtls_ecp_point_read_string(&grp->G, 16, secp160r1_g_x_hex, secp160r1_g_y_hex));

    // Load order from hex string
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&grp->N, 16, secp160r1_n_hex));

    grp->pbits = 160;
    grp->nbits = 161;

cleanup:
    if (ret != 0) {
        // If something failed, free the group to avoid memory leaks
        mbedtls_ecp_group_free(grp);
    }
    return ret;
}

} // namespace Ecoflow_mbedtls
