#pragma once

#include <cstddef>
#include <memory>

namespace varn::http::client
{

class HttpClientTransfer;

class Handle
{
public:
    Handle() = default;
    explicit Handle(std::weak_ptr<HttpClientTransfer> transfer);

    void cancel() const;
    bool active() const;
    void release(std::size_t bytes) const;

private:
    std::weak_ptr<HttpClientTransfer> transfer;
};

} // namespace varn::http::client
