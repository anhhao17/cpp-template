#ifndef ETLX_TEST_CERTS_HPP
#define ETLX_TEST_CERTS_HPP

// Test-only PEM fixtures for mTLS tests/examples. Generated with openssl
// (ECDSA P-256): a self-signed CA, a server cert for localhost/127.0.0.1,
// and a client cert, both signed by the CA. NOT for production use.
namespace etlx_test_certs {

inline constexpr char kCaCertPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBhjCCASugAwIBAgIUFm/n9SphqKOGSro7pEyZm6FY0xkwCgYIKoZIzj0EAwIw\n"
    "FzEVMBMGA1UEAwwMZXRseC10ZXN0LWNhMCAXDTI2MDYxMzAzNTkyN1oYDzIxMjYw\n"
    "NTIwMDM1OTI3WjAXMRUwEwYDVQQDDAxldGx4LXRlc3QtY2EwWTATBgcqhkjOPQIB\n"
    "BggqhkjOPQMBBwNCAAT37LqqskCjbbkYviODM1zXzcXuATtnRS6tv/j9wYNo/If3\n"
    "uymqZiFYsm3p+woyfHIDO1r5KqwoNlFeuw/RaxtEo1MwUTAdBgNVHQ4EFgQUPvYn\n"
    "EvJJlVJ1yPUlnBR8uDYsIMMwHwYDVR0jBBgwFoAUPvYnEvJJlVJ1yPUlnBR8uDYs\n"
    "IMMwDwYDVR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAgNJADBGAiEAknPei6qSVOa4\n"
    "PfCUJYC5/nLXOR0gGNyb2zLEIiDqPjACIQCDb7ayKOESzu6OZCOgQ62TgbMZ1gJI\n"
    "0xV5v7TyI2kGqw==\n"
    "-----END CERTIFICATE-----\n"
;

inline constexpr char kServerCertPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBmDCCAT6gAwIBAgIUS5voNoIqUGyXf0uP+LBz0fbDjvUwCgYIKoZIzj0EAwIw\n"
    "FzEVMBMGA1UEAwwMZXRseC10ZXN0LWNhMCAXDTI2MDYxMzAzNTkyN1oYDzIxMjYw\n"
    "NTIwMDM1OTI3WjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwWTATBgcqhkjOPQIBBggq\n"
    "hkjOPQMBBwNCAARFF3GZH25WS2ZP+347GwTCwyvOS5PkHR524qO1ZoyFoCPTymn/\n"
    "hFuVcPj2DgmSPbXyipdmFpxXkALaJd/89C0lo2kwZzAaBgNVHREEEzARgglsb2Nh\n"
    "bGhvc3SHBH8AAAEwCQYDVR0TBAIwADAdBgNVHQ4EFgQU7ASlRHcMgGOVcyaDZDXB\n"
    "ET9F+1owHwYDVR0jBBgwFoAUPvYnEvJJlVJ1yPUlnBR8uDYsIMMwCgYIKoZIzj0E\n"
    "AwIDSAAwRQIgObKqypNFnovgugWvvcQPIsOPf0A2vCOhrTZ+iPxaGEsCIQDGOdtu\n"
    "F3nQwJG8lVdwWRAUY/DepjuKC7C+fJFjnsr2MA==\n"
    "-----END CERTIFICATE-----\n"
;

inline constexpr char kServerKeyPem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MHcCAQEEILKMINWiM+t6+9W2eYKudwsgC80OZEE/f8hw5niGb+QQoAoGCCqGSM49\n"
    "AwEHoUQDQgAERRdxmR9uVktmT/t+OxsEwsMrzkuT5B0eduKjtWaMhaAj08pp/4Rb\n"
    "lXD49g4Jkj218oqXZhacV5AC2iXf/PQtJQ==\n"
    "-----END EC PRIVATE KEY-----\n"
;

inline constexpr char kClientCertPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBgDCCASegAwIBAgIUS5voNoIqUGyXf0uP+LBz0fbDjvYwCgYIKoZIzj0EAwIw\n"
    "FzEVMBMGA1UEAwwMZXRseC10ZXN0LWNhMCAXDTI2MDYxMzAzNTkyN1oYDzIxMjYw\n"
    "NTIwMDM1OTI3WjAZMRcwFQYDVQQDDA5ldGx4LWRldmljZS0wMTBZMBMGByqGSM49\n"
    "AgEGCCqGSM49AwEHA0IABDFqZPU1GWQu45CTY6CqBLAT2R6sC0YIxvavzEvi6v48\n"
    "w9pZPGBJ9P2Jsd385AzBu2WQLbAItUJKLKDU6ueN5VmjTTBLMAkGA1UdEwQCMAAw\n"
    "HQYDVR0OBBYEFDSNJqZ7yKqxEEeL+eM5TvNW49DeMB8GA1UdIwQYMBaAFD72JxLy\n"
    "SZVSdcj1JZwUfLg2LCDDMAoGCCqGSM49BAMCA0cAMEQCIFsnuz6T/ApBrabZesqo\n"
    "rjKX+b7vNl+PeBfMz9NM6IRTAiBr57RMGEEPl/NN2AQdduNquMWJgDqPSH3XPngr\n"
    "b/ZjQA==\n"
    "-----END CERTIFICATE-----\n"
;

inline constexpr char kClientKeyPem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MHcCAQEEICAELMuPMVZAXykTw9j6V2KVPDFQPqCPNYVAT7V3aIjnoAoGCCqGSM49\n"
    "AwEHoUQDQgAEMWpk9TUZZC7jkJNjoKoEsBPZHqwLRgjG9q/MS+Lq/jzD2lk8YEn0\n"
    "/Ymx3fzkDMG7ZZAtsAi1QkosoNTq543lWQ==\n"
    "-----END EC PRIVATE KEY-----\n"
;

} // namespace etlx_test_certs

#endif // ETLX_TEST_CERTS_HPP
