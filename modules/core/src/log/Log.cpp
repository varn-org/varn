#include "varn/log/Log.h"

namespace varn::log
{

void Log::line(std::string_view message)
{
    emit(Level::Info, message);
}

void Log::error(std::string_view message)
{
    emit(Level::Error, message);
}

} // namespace varn::log
