#include "varn/http/HttpClientFailureNote.h"

#include <mutex>
#include <utility>

namespace varn::http::client
{

namespace
{
// The note outlives every runtime of the process, so a transport that fails while the process exits still finds it.
class FailureNoteState
{
public:
    FailureNoteState() = delete;

    static std::mutex& mutex()
    {
        static auto* lock = new std::mutex();
        return *lock;
    }

    static FailureNote::Writer& writer()
    {
        static auto* installed = new FailureNote::Writer();
        return *installed;
    }
};
} // namespace

// Installs the writer of the note, or removes it with an empty one. The writer runs on the thread where a request fails, the loop for a deadline and the thread of the transport for an error of the network, so it must be safe from any thread.
void FailureNote::install(Writer writer)
{
    const std::lock_guard<std::mutex> lock(FailureNoteState::mutex());
    FailureNoteState::writer() = std::move(writer);
}

// Answers the message with the note of the host after it, or the message alone when the host has no note for the address.
std::string FailureNote::append(const std::string& message, const std::string& url)
{
    const Writer writer = current();
    if (!writer)
    {
        return message;
    }

    const std::string note = writer(url);
    return note.empty() ? message : message + " " + note;
}

FailureNote::Writer FailureNote::current()
{
    const std::lock_guard<std::mutex> lock(FailureNoteState::mutex());
    return FailureNoteState::writer();
}

} // namespace varn::http::client
