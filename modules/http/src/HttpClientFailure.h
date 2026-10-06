#pragma once

#include "varn/http/HttpClientTypes.h"

#include <stdexcept>
#include <string>

namespace varn::http::client
{

/// A failure of a transport that knows what kind it is, raised on the thread of the transport and turned into the error of the transfer.
class HttpClientFailure : public std::runtime_error
{
public:
    HttpClientFailure(ErrorCode code, const std::string& message)
        : std::runtime_error(message)
        , kind(code)
    {
    }

    ErrorCode code() const { return kind; }
    Error error() const { return Error{kind, what()}; }

private:
    ErrorCode kind;
};

} // namespace varn::http::client
