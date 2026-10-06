#include "varn/http/HttpTypes.h"

namespace varn::http
{

// Answers whether a close frame may carry the code, which leaves out the codes the protocol reserves for a local report or never assigned.
bool WebSocketConnection::validCloseCode(int code)
{
    return (code >= 1000 && code <= 1003) || (code >= 1007 && code <= 1014) || (code >= 3000 && code <= 4999);
}

} // namespace varn::http
