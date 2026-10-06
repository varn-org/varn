#pragma once

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

private:
    std::weak_ptr<HttpClientTransfer> transfer;
};

} // namespace varn::http::client
