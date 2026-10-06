#pragma once

struct ssl_ctx_st;

namespace varn::tls
{

class SystemTrust
{
public:
    SystemTrust() = delete;

    static void install(ssl_ctx_st* context);
};

} // namespace varn::tls
