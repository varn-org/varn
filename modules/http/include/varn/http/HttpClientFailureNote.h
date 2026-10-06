#pragma once

#include <functional>
#include <string>

namespace varn::http::client
{

/// A sentence the host adds to the failures of requests that did not reach their host, by their deadline or by an error of the network, such as a permission of the system that the app does not hold.
class FailureNote
{
public:
    using Writer = std::function<std::string(const std::string& url)>;

    FailureNote() = delete;

    static void install(Writer writer);
    static std::string append(const std::string& message, const std::string& url);

private:
    static Writer current();
};

} // namespace varn::http::client
