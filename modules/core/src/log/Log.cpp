#include "varn/log/Log.h"

#include <sstream>

namespace varn::log
{

void Log::line(std::string_view module, std::string message)
{
    std::ostringstream o;
    o << '[' << module << "] " << message;
    emit(Level::Info, o.str());
}

void Log::error(std::string_view module, std::string message)
{
    std::ostringstream o;
    o << '[' << module << "] " << message;
    emit(Level::Error, o.str());
}

} // namespace varn::log
