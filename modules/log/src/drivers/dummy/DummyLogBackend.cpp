#include "varn/log/Log.h"

#include <stdexcept>

namespace varn::log
{

void Log::emit(Level /*level*/, std::string_view /*message*/)
{
}

void Log::output(std::string_view /*message*/)
{
}

void Log::setSink(Sink /*sink*/)
{
}

void Log::setLevel(Level /*level*/)
{
}

void Log::setName(std::string_view /*name*/)
{
}

void Log::addFileSink(std::string_view /*path*/, bool /*rotating*/)
{
    throw std::runtime_error("A log file is not available in this build.");
}

} // namespace varn::log
