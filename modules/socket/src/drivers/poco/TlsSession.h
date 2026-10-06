#pragma once

#include <cstddef>
#include <string>
#include <string_view>

struct bio_st;
struct ssl_st;

namespace varn::socket
{

class TlsSession
{
public:
    enum class Status
    {
        Ready,
        NeedsInput,
        Closed,
        Failed
    };

    struct ReadResult
    {
        Status status;
        std::size_t bytes;
    };

    TlsSession(const std::string& host, bool verify);
    ~TlsSession();

    TlsSession(const TlsSession&) = delete;
    TlsSession& operator=(const TlsSession&) = delete;

    void receiveCipher(const char* data, std::size_t size);
    void receiveEnd();
    Status handshake();
    ReadResult read(char* buffer, std::size_t size);
    bool write(std::string_view plaintext);
    void closeNotify();
    std::size_t pendingCipher() const;
    std::string takeCipher();
    const std::string& failure() const { return error; }

private:
    Status fail(int result);

    bool verify;
    ssl_st* ssl = nullptr;
    bio_st* input = nullptr;
    bio_st* output = nullptr;
    std::string error;
};

} // namespace varn::socket
