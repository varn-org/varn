#include "varn/log/Log.h"

#include <exception>
#include <string>

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

// Logs the exception that the caller is handling as an error, after the sentence that says what failed, for a boundary that lets no exception through, and stays silent only when even the log cannot be written.
void Log::failure(std::string_view what) noexcept
{
    try
    {
        try
        {
            throw;
        }
        catch (const std::exception& exception)
        {
            error(std::string(what) + " " + exception.what());
        }
        catch (...)
        {
            error(what);
        }
    }
    catch (...)
    {
    }
}

} // namespace varn::log
