#include "varn/varn.h"

#include <gtest/gtest.h>

#if defined(VARN_ENABLE_TLS)

#include "drivers/poco/TlsSession.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <system_error>

namespace
{

class TlsTestHelpers
{
public:
    // Writes a self-signed certificate for "localhost" and "127.0.0.1" with its key into a fresh directory, once for the whole run, which removes it at its end.
    static const std::filesystem::path& certificateDirectory()
    {
        static const CertificateDirectory directory;
        return directory.path;
    }

    // Runs a chunk on a fresh runtime with the certificate paths and a TLS server of its own, and answers its exit code, which an assertion of the chunk turns into a failure.
    static int run(const std::string& body)
    {
        const std::filesystem::path& directory = certificateDirectory();
        const std::string chunk = "local async = require('async')\n"
                                  "local http = require('http')\n"
                                  "local socket = require('socket')\n"
                                  "local app = http.createApp()\n"
                                  "app:get('/hi', function(ctx) ctx:text('hello') end)\n"
                                  "app:get('/big', function(ctx) ctx:text(string.rep('x', 3 * 1024 * 1024)) end)\n"
                                  "local server = app:listen({ port = 0, host = '127.0.0.1', tls = true, certFile = [==[" +
                                  (directory / "cert.pem").string() + "]==], keyFile = [==[" + (directory / "key.pem").string() +
                                  "]==] })\n"
                                  "local port = server.port\n"
                                  "local function readAll(conn)\n"
                                  "  local parts = {}\n"
                                  "  while true do\n"
                                  "    local chunk = assert(conn:receive(65536):await())\n"
                                  "    if chunk == '' then return table.concat(parts) end\n"
                                  "    parts[#parts + 1] = chunk\n"
                                  "  end\n"
                                  "end\n"
                                  "async.run(function()\n" +
                                  body +
                                  "  server:close():await()\n"
                                  "end)\n";

        varn_runtime* runtime = varn_runtime_new();
        const int code = varn_runtime_run_string(runtime, chunk.c_str(), "=tls");
        varn_runtime_free(runtime);
        return code;
    }

    // A server end of TLS in memory, so a test hands bytes between it and a client session with no socket at all.
    class MemoryServer
    {
    public:
        MemoryServer()
            : context(SSL_CTX_new(TLS_server_method()), &SSL_CTX_free)
        {
            const std::filesystem::path& directory = certificateDirectory();
            SSL_CTX_use_certificate_file(context.get(), (directory / "cert.pem").string().c_str(), SSL_FILETYPE_PEM);
            SSL_CTX_use_PrivateKey_file(context.get(), (directory / "key.pem").string().c_str(), SSL_FILETYPE_PEM);
            ssl = SSL_new(context.get());
            input = BIO_new(BIO_s_mem());
            output = BIO_new(BIO_s_mem());
            SSL_set_bio(ssl, input, output);
            SSL_set_accept_state(ssl);
        }

        ~MemoryServer() { SSL_free(ssl); }

        MemoryServer(const MemoryServer&) = delete;
        MemoryServer& operator=(const MemoryServer&) = delete;

        void exchange(varn::socket::TlsSession& client)
        {
            for (int round = 0; round < 16; ++round)
            {
                const std::string toServer = client.takeCipher();
                BIO_write(input, toServer.data(), static_cast<int>(toServer.size()));
                SSL_do_handshake(ssl);

                std::string toClient(BIO_ctrl_pending(output), '\0');
                BIO_read(output, toClient.data(), static_cast<int>(toClient.size()));
                client.receiveCipher(toClient.data(), toClient.size());
                if (client.handshake() == varn::socket::TlsSession::Status::Ready && SSL_is_init_finished(ssl) == 1 && client.pendingCipher() == 0)
                {
                    return;
                }
            }
        }

        void send(varn::socket::TlsSession& client, const std::string& plaintext)
        {
            SSL_write(ssl, plaintext.data(), static_cast<int>(plaintext.size()));
            deliver(client);
        }

        void end(varn::socket::TlsSession& client)
        {
            SSL_shutdown(ssl);
            deliver(client);
        }

    private:
        void deliver(varn::socket::TlsSession& client)
        {
            std::string toClient(BIO_ctrl_pending(output), '\0');
            BIO_read(output, toClient.data(), static_cast<int>(toClient.size()));
            client.receiveCipher(toClient.data(), toClient.size());
        }

        std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> context;
        SSL* ssl = nullptr;
        BIO* input = nullptr;
        BIO* output = nullptr;
    };

private:
    struct CertificateDirectory
    {
        CertificateDirectory()
            : path(writeCertificate())
        {
        }

        ~CertificateDirectory()
        {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }

        CertificateDirectory(const CertificateDirectory&) = delete;
        CertificateDirectory& operator=(const CertificateDirectory&) = delete;

        std::filesystem::path path;
    };

    static std::filesystem::path writeCertificate()
    {
        std::random_device seed;
        const auto directory = std::filesystem::temp_directory_path() / ("varn-tls-test-" + std::to_string(seed()));
        std::filesystem::create_directories(directory);

        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_EC_gen("P-256"), &EVP_PKEY_free);
        std::unique_ptr<X509, decltype(&X509_free)> certificate(X509_new(), &X509_free);
        EXPECT_TRUE(key && certificate);

        X509_set_version(certificate.get(), 2);
        ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), 1);
        X509_gmtime_adj(X509_getm_notBefore(certificate.get()), -3600);
        X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 7 * 24 * 3600);
        X509_set_pubkey(certificate.get(), key.get());

        X509_NAME* name = X509_get_subject_name(certificate.get());
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("localhost"), -1, -1, 0);
        X509_set_issuer_name(certificate.get(), name);

        X509V3_CTX context;
        X509V3_set_ctx_nodb(&context);
        X509V3_set_ctx(&context, certificate.get(), certificate.get(), nullptr, nullptr, 0);
        X509_EXTENSION* names = X509V3_EXT_conf_nid(nullptr, &context, NID_subject_alt_name, "DNS:localhost,IP:127.0.0.1");
        X509_add_ext(certificate.get(), names, -1);
        X509_EXTENSION_free(names);
        EXPECT_GT(X509_sign(certificate.get(), key.get(), EVP_sha256()), 0);

        // clang-format off
        write(directory / "cert.pem", [&](BIO* out)
        {
            return PEM_write_bio_X509(out, certificate.get());
        });
        write(directory / "key.pem", [&](BIO* out)
        {
            return PEM_write_bio_PrivateKey(out, key.get(), nullptr, nullptr, 0, nullptr, nullptr);
        });
        // clang-format on
        return directory;
    }

    template <typename Writer>
    static void write(const std::filesystem::path& path, Writer writer)
    {
        std::unique_ptr<BIO, decltype(&BIO_free)> out(BIO_new_file(path.string().c_str(), "w"), &BIO_free);
        ASSERT_TRUE(out);
        ASSERT_EQ(writer(out.get()), 1);
    }
};

} // namespace

TEST(TlsSession, APeerThatEndsTheSessionAnswersTheEndOfTheStream)
{
    TlsTestHelpers::MemoryServer server;
    varn::socket::TlsSession client("localhost", false);
    EXPECT_EQ(client.handshake(), varn::socket::TlsSession::Status::NeedsInput);
    server.exchange(client);

    server.send(client, "hello");
    char buffer[64];
    const auto read = client.read(buffer, sizeof(buffer));
    ASSERT_EQ(read.status, varn::socket::TlsSession::Status::Ready);
    EXPECT_EQ(std::string(buffer, read.bytes), "hello");

    server.end(client);
    client.receiveEnd();
    EXPECT_EQ(client.read(buffer, sizeof(buffer)).status, varn::socket::TlsSession::Status::Closed);
}

TEST(TlsSession, APeerThatClosesWithoutEndingTheSessionFails)
{
    TlsTestHelpers::MemoryServer server;
    varn::socket::TlsSession client("localhost", false);
    EXPECT_EQ(client.handshake(), varn::socket::TlsSession::Status::NeedsInput);
    server.exchange(client);

    server.send(client, "partial");
    char buffer[64];
    EXPECT_EQ(client.read(buffer, sizeof(buffer)).status, varn::socket::TlsSession::Status::Ready);

    client.receiveEnd();
    EXPECT_EQ(client.read(buffer, sizeof(buffer)).status, varn::socket::TlsSession::Status::Failed);
    EXPECT_EQ(client.failure(), "the connection ended before the TLS session was closed");
}

TEST(SocketTls, AnInsecureConnectRoundTripsWithAReceiveAndASendInFlightTogether)
{
    EXPECT_EQ(TlsTestHelpers::run("  local conn = assert(socket.tls.connect('127.0.0.1', port, { insecure = true, timeoutMs = 5000 }):await())\n"
                                  "  local pending = conn:receive(65536)\n"
                                  "  assert(conn:send('GET /hi HTTP/1.1\\r\\nHost: x\\r\\nConnection: close\\r\\n\\r\\n'):await())\n"
                                  "  local first = assert(pending:await())\n"
                                  "  assert(first:match('^HTTP/1.1 200'), first)\n"
                                  "  conn:close():await()\n"),
              0);
}

TEST(SocketTls, AVerifiedConnectRefusesASelfSignedCertificate)
{
    EXPECT_EQ(TlsTestHelpers::run("  local conn, err = socket.tls.connect('127.0.0.1', port, { timeoutMs = 5000 }):await()\n"
                                  "  assert(not conn, 'A self-signed certificate must not verify')\n"
                                  "  assert(err == 'The TLS handshake with 127.0.0.1 failed: self-signed certificate.', err)\n"),
              0);
}

TEST(SocketTls, StartTlsUpgradesAPlainConnection)
{
    EXPECT_EQ(TlsTestHelpers::run("  local conn = assert(socket.tcp.connect('127.0.0.1', port):await())\n"
                                  "  assert(conn:startTls('localhost', { insecure = true }):await())\n"
                                  "  assert(conn:send('GET /hi HTTP/1.1\\r\\nHost: x\\r\\nConnection: close\\r\\n\\r\\n'):await())\n"
                                  "  local reply = readAll(conn)\n"
                                  "  assert(reply:sub(-5) == 'hello', reply)\n"
                                  "  conn:close():await()\n"),
              0);
}

TEST(SocketTls, ALargeBodyArrivesWhole)
{
    EXPECT_EQ(TlsTestHelpers::run("  local conn = assert(socket.tls.connect('127.0.0.1', port, { insecure = true }):await())\n"
                                  "  assert(conn:send('GET /big HTTP/1.1\\r\\nHost: x\\r\\nConnection: close\\r\\n\\r\\n'):await())\n"
                                  "  local reply = readAll(conn)\n"
                                  "  local body = reply:match('\\r\\n\\r\\n(.*)$')\n"
                                  "  assert(body and #body == 3 * 1024 * 1024, #reply)\n"
                                  "  conn:close():await()\n"),
              0);
}

TEST(SocketTls, ACloseSettlesAReceiveThatWaits)
{
    EXPECT_EQ(TlsTestHelpers::run("  local conn = assert(socket.tls.connect('127.0.0.1', port, { insecure = true }):await())\n"
                                  "  local waiting = conn:receive(10)\n"
                                  "  conn:close():await()\n"
                                  "  local data, err = waiting:await()\n"
                                  "  assert(data == nil and err == 'The socket was closed.', tostring(err))\n"),
              0);
}

TEST(SocketTls, AHandshakeThatNeverAnswersEndsAtTheDeadline)
{
    EXPECT_EQ(TlsTestHelpers::run("  local listener = assert(socket.tcp.listen('127.0.0.1', 0):await())\n"
                                  "  local accepted = listener:accept()\n"
                                  "  local conn, err = socket.tls.connect('127.0.0.1', listener.port, { insecure = true, timeoutMs = 200 }):await()\n"
                                  "  assert(not conn)\n"
                                  "  assert(err == 'The TLS handshake with 127.0.0.1 did not finish within 200 ms.', err)\n"
                                  "  accepted:await():close():await()\n"
                                  "  listener:close():await()\n"),
              0);
}

#endif
