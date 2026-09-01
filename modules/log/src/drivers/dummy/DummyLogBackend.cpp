#include "varn/log/Log.h"

namespace varn::log
{

void Log::emit(Level /*level*/, std::string_view /*message*/)
{
}

void Log::setLevel(Level /*level*/)
{
}

void Log::addFileSink(std::string_view /*path*/, bool /*rotating*/)
{
}

} // namespace varn::log
