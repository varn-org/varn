#include "HttpClientDownload.h"

#include "HttpClientFailure.h"

#include <charconv>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace varn::http::client
{

HttpClientDownload::HttpClientDownload(std::string url, std::filesystem::path path, bool resume)
    : url(std::move(url))
    , target(std::move(path))
    , partial(target.string() + ".part")
    , validatorFile(target.string() + ".part-validator")
    , resume(resume)
{
}

// Answers the headers that ask for the rest of a partial file an earlier attempt kept, which only a download that resumes reads.
// The partial file continues only with the validator of the response it came from, so a file that changed on the server is sent whole again.
Headers HttpClientDownload::prepare()
{
    if (!resume)
    {
        return {};
    }

    std::error_code code;
    const std::uintmax_t size = std::filesystem::file_size(partial, code);
    if (code || size == 0)
    {
        return {};
    }

    std::ifstream stored(validatorFile, std::ios::binary);
    std::string kept((std::istreambuf_iterator<char>(stored)), std::istreambuf_iterator<char>());
    if (kept.empty())
    {
        return {};
    }

    held = size;
    validator = std::move(kept);
    return {{"Range", "bytes=" + std::to_string(held) + "-"}, {"If-Range", validator}};
}

// Answers the sink that takes the body on the thread of the transport, which keeps the download alive for as long as the transport may write.
BodySink HttpClientDownload::sink()
{
    auto self = shared_from_this();

    // clang-format off
    return BodySink{
        [self](const ResponseHead& head) { self->begin(head); },
        [self](std::string_view chunk) { self->write(chunk); },
    };
    // clang-format on
}

// Closes the partial file of a body that arrived whole and makes it the file, answering its size.
std::uint64_t HttpClientDownload::finish()
{
    std::lock_guard<std::mutex> lock(mutex);
    closed = true;

    if (file.is_open())
    {
        file.close();
        if (file.fail())
        {
            throw std::runtime_error("The download of \"" + url + "\" could not finish the file \"" + partial.string() + "\".");
        }
    }

    std::error_code code;
    std::filesystem::rename(partial, target, code);
    if (code)
    {
        throw std::runtime_error("The download of \"" + url + "\" could not move its file to \"" + target.string() + "\" (" + code.message() + ").");
    }

    std::filesystem::remove(validatorFile, code);
    return std::filesystem::file_size(target);
}

// Closes the partial file of a download that failed or was cancelled, and removes it unless a later download can resume it.
void HttpClientDownload::discard()
{
    std::lock_guard<std::mutex> lock(mutex);
    closed = true;
    file.close();

    if (!resume || !std::filesystem::exists(validatorFile))
    {
        removeParts();
    }
}

// Decides on the thread of the transport, from the head of the final response, whether the body continues the partial file, replaces it or is refused.
void HttpClientDownload::begin(const ResponseHead& head)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (closed)
    {
        return;
    }

    if (head.status == 206)
    {
        const std::optional<ContentRange> range = head.contentRange();
        if (held == 0 || !range || range->first != held)
        {
            refuse("The server answered the download of \"" + url + "\" with a range that does not continue its partial file.");
        }

        continued = true;
        open(std::ios::app);
        return;
    }

    if (head.status == 416 && wholeAlready(head))
    {
        continued = true;
        whole = true;
        return;
    }

    if (head.status < 200 || head.status >= 300)
    {
        refuse("The server answered the download of \"" + url + "\" with the status " + std::to_string(head.status) + ".");
    }

    held = 0;
    open(std::ios::trunc);
    if (resume)
    {
        keepValidator(head);
    }
}

void HttpClientDownload::write(std::string_view chunk)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (closed || whole)
    {
        return;
    }

    file.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    if (file.fail())
    {
        refuse("The download of \"" + url + "\" could not write to the file \"" + partial.string() + "\".");
    }
}

void HttpClientDownload::open(std::ios::openmode mode)
{
    std::error_code code;
    if (target.has_parent_path())
    {
        std::filesystem::create_directories(target.parent_path(), code);
    }

    file.open(partial, std::ios::binary | std::ios::out | mode);
    if (code || !file.is_open())
    {
        refuse("The download of \"" + url + "\" could not open the file \"" + partial.string() + "\".");
    }
}

// Keeps the validator a server gave the body, a strong entity tag or else the date it was last modified, which is what lets a later download continue it.
void HttpClientDownload::keepValidator(const ResponseHead& head)
{
    std::optional<std::string_view> found = head.header("etag");
    if (found && found->substr(0, 2) == "W/")
    {
        found.reset();
    }

    if (!found)
    {
        found = head.header("last-modified");
    }

    if (!found)
    {
        std::error_code code;
        std::filesystem::remove(validatorFile, code);
        return;
    }

    std::ofstream stored(validatorFile, std::ios::binary | std::ios::trunc);
    stored.write(found->data(), static_cast<std::streamsize>(found->size()));
    if (stored.fail())
    {
        refuse("The download of \"" + url + "\" could not write the file \"" + validatorFile.string() + "\".");
    }
}

void HttpClientDownload::removeParts()
{
    std::error_code code;
    std::filesystem::remove(partial, code);
    std::filesystem::remove(validatorFile, code);
}

// A server answers a range that starts at the end of the file with `416` and the length of the file, which means the partial file is already whole.
bool HttpClientDownload::wholeAlready(const ResponseHead& head) const
{
    const std::optional<std::string_view> found = head.header("content-range");
    constexpr std::string_view prefix = "bytes */";
    if (held == 0 || !found || found->substr(0, prefix.size()) != prefix)
    {
        return false;
    }

    const std::string_view digits = found->substr(prefix.size());
    std::uint64_t length = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), length);
    return error == std::errc() && end == digits.data() + digits.size() && length == held;
}

void HttpClientDownload::refuse(const std::string& message) const
{
    throw HttpClientFailure(ErrorCode::Callback, message);
}

} // namespace varn::http::client
