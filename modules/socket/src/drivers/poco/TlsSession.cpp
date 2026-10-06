#include "TlsSession.h"

#include "varn/tls/SystemTrust.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <climits>
#include <stdexcept>

namespace varn::socket
{

namespace
{

class TlsContexts
{
public:
    TlsContexts() = delete;

    static SSL_CTX* client(bool verify)
    {
        if (verify)
        {
            static SSL_CTX* const strict = create(true);
            return strict;
        }

        static SSL_CTX* const insecure = create(false);
        return insecure;
    }

    // Clears the queue of the thread, so a later failure never reports an older one.
    static std::string lastError()
    {
        const unsigned long code = ERR_get_error();
        ERR_clear_error();
        if (code == 0)
        {
            return std::string();
        }

        char text[256] = {};
        ERR_error_string_n(code, text, sizeof(text));
        return text;
    }

private:
    static SSL_CTX* create(bool verify)
    {
        SSL_CTX* context = SSL_CTX_new(TLS_client_method());
        if (context == nullptr)
        {
            throw std::runtime_error("The TLS client context could not be created: " + lastError());
        }

        SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);
        SSL_CTX_set_security_level(context, 2);
        SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);

        if (!verify)
        {
            SSL_CTX_set_verify(context, SSL_VERIFY_NONE, nullptr);
            return context;
        }

        SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
        try
        {
            varn::tls::SystemTrust::install(context);
        }
        catch (...)
        {
            SSL_CTX_free(context);
            throw;
        }

        return context;
    }
};

} // namespace

// A session works on buffers in memory and never touches the socket, which the connection drives.
TlsSession::TlsSession(const std::string& host, bool verify)
    : verify(verify)
{
    SSL_CTX* context = TlsContexts::client(verify);

    ERR_clear_error();
    ssl = SSL_new(context);
    input = BIO_new(BIO_s_mem());
    output = BIO_new(BIO_s_mem());
    if (ssl == nullptr || input == nullptr || output == nullptr)
    {
        const std::string reason = TlsContexts::lastError();
        BIO_free(input);
        BIO_free(output);
        SSL_free(ssl);
        throw std::runtime_error("The TLS session could not be created: " + reason + ".");
    }

    // An empty input asks for more bytes until the peer really closed.
    BIO_set_mem_eof_return(input, -1);
    SSL_set_bio(ssl, input, output);
    SSL_set_connect_state(ssl);

    // An address sends no server name and is checked against the certificate as an address.
    ASN1_OCTET_STRING* address = a2i_IPADDRESS(host.c_str());
    const bool numeric = address != nullptr;
    ASN1_OCTET_STRING_free(address);
    ERR_clear_error();

    if (!numeric && !host.empty() && SSL_set_tlsext_host_name(ssl, host.c_str()) != 1)
    {
        const std::string reason = TlsContexts::lastError();
        SSL_free(ssl);
        throw std::runtime_error("The server name \"" + host + "\" could not be set for TLS: " + reason + ".");
    }

    if (!verify)
    {
        return;
    }

    const int checked = numeric ? X509_VERIFY_PARAM_set1_ip_asc(SSL_get0_param(ssl), host.c_str()) : SSL_set1_host(ssl, host.c_str());
    if (checked != 1)
    {
        const std::string reason = TlsContexts::lastError();
        SSL_free(ssl);
        throw std::runtime_error("The host \"" + host + "\" could not be set for certificate verification: " + reason + ".");
    }
}

TlsSession::~TlsSession()
{
    SSL_free(ssl);
}

void TlsSession::receiveCipher(const char* data, std::size_t size)
{
    while (size > 0)
    {
        const int chunk = static_cast<int>(size < static_cast<std::size_t>(INT_MAX) ? size : static_cast<std::size_t>(INT_MAX));
        const int written = BIO_write(input, data, chunk);
        if (written <= 0)
        {
            throw std::runtime_error("The TLS session could not take the bytes it received.");
        }

        data += written;
        size -= static_cast<std::size_t>(written);
    }
}

// The bytes already taken are the last, so a read past them answers the end of the stream.
void TlsSession::receiveEnd()
{
    BIO_set_mem_eof_return(input, 0);
}

TlsSession::Status TlsSession::handshake()
{
    ERR_clear_error();
    const int result = SSL_do_handshake(ssl);
    return result == 1 ? Status::Ready : fail(result);
}

TlsSession::ReadResult TlsSession::read(char* buffer, std::size_t size)
{
    ERR_clear_error();
    std::size_t bytes = 0;
    if (SSL_read_ex(ssl, buffer, size, &bytes) == 1)
    {
        return {Status::Ready, bytes};
    }

    return {fail(0), 0};
}

// A memory output never fills, so a write takes the whole plaintext or fails.
bool TlsSession::write(std::string_view plaintext)
{
    ERR_clear_error();
    std::size_t written = 0;
    if (plaintext.empty() || SSL_write_ex(ssl, plaintext.data(), plaintext.size(), &written) == 1)
    {
        return true;
    }

    fail(0);
    return false;
}

void TlsSession::closeNotify()
{
    ERR_clear_error();
    SSL_shutdown(ssl);
    ERR_clear_error();
}

std::size_t TlsSession::pendingCipher() const
{
    return BIO_ctrl_pending(output);
}

std::string TlsSession::takeCipher()
{
    std::string cipher(BIO_ctrl_pending(output), '\0');
    if (!cipher.empty())
    {
        const int taken = BIO_read(output, cipher.data(), static_cast<int>(cipher.size()));
        cipher.resize(taken > 0 ? static_cast<std::size_t>(taken) : 0);
    }

    return cipher;
}

TlsSession::Status TlsSession::fail(int result)
{
    const int reason = SSL_get_error(ssl, result);
    if (reason == SSL_ERROR_WANT_READ)
    {
        return Status::NeedsInput;
    }

    if (reason == SSL_ERROR_ZERO_RETURN)
    {
        return Status::Closed;
    }

    // A peer that closes without ending the session could cut a message short, so that end is a failure.
    const long verdict = SSL_get_verify_result(ssl);
    const unsigned long code = ERR_peek_last_error();
    const std::string queued = TlsContexts::lastError();
    if (verify && verdict != X509_V_OK)
    {
        error = X509_verify_cert_error_string(verdict);
    }
    else if (ERR_GET_REASON(code) == SSL_R_UNEXPECTED_EOF_WHILE_READING || (queued.empty() && reason == SSL_ERROR_SYSCALL))
    {
        error = "the connection ended before the TLS session was closed";
    }
    else if (!queued.empty())
    {
        error = queued;
    }
    else
    {
        error = "the TLS session failed with the OpenSSL error " + std::to_string(reason);
    }

    return Status::Failed;
}

} // namespace varn::socket
