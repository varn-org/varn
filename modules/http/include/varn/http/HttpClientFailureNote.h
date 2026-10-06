#pragma once

#include <functional>
#include <string>

namespace varn::http::client
{

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
