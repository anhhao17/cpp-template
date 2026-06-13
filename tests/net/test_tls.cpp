#include "etlx/net/tls.hpp"
#include "host/host_tcp.hpp"
#include "tests/net/test_certs.hpp"
#include "tests/support/tls_echo_server.hpp"

#include <cstring>
#include <string>

#include <gtest/gtest.h>

namespace {

using etlx_test_support::TlsEchoServer;
using namespace etlx_test_certs;

// Happy path: the server requires a client certificate, the client presents
// one signed by the trusted CA, and an application message round-trips.
TEST(TlsMutualAuth, ClientCertAcceptedAndEcho) {
    TlsEchoServer server(/*require_client_cert=*/true);
    const uint16_t port = server.Start();

    etlx::ports::host::PosixTcpSocket tcp;
    etlx::net::TlsSocket             tls(tcp);

    etlx::net::TlsConfig cfg;
    cfg.ca_cert_pem     = kCaCertPem;
    cfg.client_cert_pem = kClientCertPem;
    cfg.client_key_pem  = kClientKeyPem;
    cfg.server_name     = "localhost";
    ASSERT_TRUE(tls.Configure(cfg).has_value());

    ASSERT_TRUE(tls.Connect("127.0.0.1", port).has_value());
    EXPECT_TRUE(tls.handshaked());

    const char* msg = "hello-over-mtls";
    ASSERT_TRUE(tls.Send(etlx::ConstByteSpan{
        reinterpret_cast<const uint8_t*>(msg), std::strlen(msg)}).has_value());

    uint8_t buf[64];
    auto n = tls.Recv(etlx::ByteSpan{buf, sizeof(buf)});
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), n.value()), "hello-over-mtls");

    tls.Close();
    server.Wait();
    EXPECT_TRUE(server.handshake_ok());
}

// Server demands a client certificate; client presents none. The server
// rejects mutual auth. (Under TLS 1.3 the client handshake can complete before
// the rejection arrives, so the failure may surface on the first data
// exchange instead of at Connect.) Either way the exchange must not succeed and
// the server must not consider the handshake complete.
TEST(TlsMutualAuth, MissingClientCertRejected) {
    TlsEchoServer server(/*require_client_cert=*/true);
    const uint16_t port = server.Start();

    etlx::ports::host::PosixTcpSocket tcp;
    etlx::net::TlsSocket             tls(tcp);

    etlx::net::TlsConfig cfg;
    cfg.ca_cert_pem = kCaCertPem;     // verifies the server...
    cfg.server_name = "localhost";    // ...but sends no client cert
    ASSERT_TRUE(tls.Configure(cfg).has_value());

    bool succeeded = tls.Connect("127.0.0.1", port).has_value();
    if (succeeded) {
        const char* msg = "should-not-be-accepted";
        tls.Send(etlx::ConstByteSpan{reinterpret_cast<const uint8_t*>(msg), std::strlen(msg)});
        uint8_t buf[64];
        auto n = tls.Recv(etlx::ByteSpan{buf, sizeof(buf)});
        succeeded = n.has_value() && n.value() > 0;  // got a real echo back?
    }
    tls.Close();
    server.Wait();

    EXPECT_FALSE(succeeded);
    EXPECT_FALSE(server.handshake_ok());  // server never completed mutual auth
}

// Server-only TLS (no client cert required) still succeeds and verifies the
// server against the CA.
TEST(TlsMutualAuth, ServerOnlyTlsSucceeds) {
    TlsEchoServer server(/*require_client_cert=*/false);
    const uint16_t port = server.Start();

    etlx::ports::host::PosixTcpSocket tcp;
    etlx::net::TlsSocket             tls(tcp);

    etlx::net::TlsConfig cfg;
    cfg.ca_cert_pem = kCaCertPem;
    cfg.server_name = "localhost";
    ASSERT_TRUE(tls.Configure(cfg).has_value());

    EXPECT_TRUE(tls.Connect("127.0.0.1", port).has_value());
    tls.Close();
}

// Verification on, but the client trusts the wrong CA (here: the client's own
// cert as a bogus CA) -> server cert fails to verify -> handshake fails.
TEST(TlsServerVerify, UntrustedServerRejected) {
    TlsEchoServer server(/*require_client_cert=*/false);
    const uint16_t port = server.Start();

    etlx::ports::host::PosixTcpSocket tcp;
    etlx::net::TlsSocket             tls(tcp);

    etlx::net::TlsConfig cfg;
    cfg.ca_cert_pem = kClientCertPem;  // not the CA that signed the server
    cfg.server_name = "localhost";
    ASSERT_TRUE(tls.Configure(cfg).has_value());

    EXPECT_FALSE(tls.Connect("127.0.0.1", port).has_value());
}

} // namespace
