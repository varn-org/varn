#if !defined(__EMSCRIPTEN__)
#error "The \"EmscriptenFetchHttpClient\" driver is only built for Emscripten (VARN_HTTP_CLIENT_DRIVER=EMSCRIPTEN_FETCH)."
#endif

#include "../../HttpClientPerform.h"

#include "../../HttpClientFailure.h"
#include "../../HttpClientTransfer.h"
#include "varn/wasm/WasmAsyncHost.h"

#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

// The browser side of a request, kept by number on the module so the callbacks of the engine reach the one they belong to.
// A body is read from `Response.body` one chunk at a time and handed over as it arrives, so no response is ever held whole, and an `AbortController` ends it.

// clang-format off
EM_JS(void, varn_fetch_open, (uint32_t id), {
    Module.varnFetches = Module.varnFetches || new Map();
    Module.varnFetches.set(id, { parts : [], controller : new AbortController(), reader : null, paused : false, reading : false, done : false });
});

EM_JS(void, varn_fetch_close, (uint32_t id), {
    Module.varnFetches.delete(id);
});

EM_JS(void, varn_fetch_body_part, (uint32_t id, const char* data, size_t length), {
    const state = Module.varnFetches.get(id);
    state.parts.push(HEAPU8.slice(data, data + length));
});

EM_JS(void, varn_fetch_pause, (uint32_t id), {
    const state = Module.varnFetches && Module.varnFetches.get(id);
    if (state)
    {
        state.paused = true;
    }
});

EM_JS(void, varn_fetch_resume, (uint32_t id), {
    const state = Module.varnFetches && Module.varnFetches.get(id);
    if (state)
    {
        state.paused = false;
        state.pump();
    }
});

EM_JS(void, varn_fetch_abort, (uint32_t id), {
    const state = Module.varnFetches && Module.varnFetches.get(id);
    if (state)
    {
        state.controller.abort();
    }
});

EM_JS(void, varn_fetch_start, (uint32_t id, const char* url, const char* method, const char* headers_json, const char* body, size_t body_length, int body_parts), {
    const state = Module.varnFetches.get(id);

    function text(value)
    {
        const length = lengthBytesUTF8(value) + 1;
        const pointer = _malloc(length);
        if (pointer)
        {
            stringToUTF8(value, pointer, length);
        }

        return pointer;
    }

    function finish(failure)
    {
        if (state.done)
        {
            return;
        }

        state.done = true;
        Module.varnFetches.delete(id);
        _varn_fetch_js_on_end(id, failure === null ? 0 : 1, failure === null ? 0 : text("[EmscriptenFetchHttpClient] " + failure));
    }

    // Reads the next chunk unless one is already being read, the engine asked to pause or the request ended.
    state.pump = function()
    {
        if (state.reading || state.paused || state.done)
        {
            return;
        }

        state.reading = true;
        state.reader.read().then((step) => {
            state.reading = false;
            if (state.done)
            {
                return;
            }

            if (step.done)
            {
                finish(null);
                return;
            }

            const length = step.value.byteLength;
            if (length > 0)
            {
                const pointer = _malloc(length);
                if (!pointer)
                {
                    state.controller.abort();
                    finish("The chunk of the response could not be allocated.");
                    return;
                }

                HEAPU8.set(step.value, pointer);
                if (!_varn_fetch_js_on_chunk(id, pointer, length))
                {
                    state.controller.abort();
                    finish(null);
                    return;
                }
            }

            state.pump();
        }).catch((failure) => {
            state.reading = false;
            finish(String(failure));
        });
    };

    let headers;
    try
    {
        headers = JSON.parse(UTF8ToString(headers_json));
    }
    catch (failure)
    {
        finish("The request headers are not valid JSON (" + String(failure) + ").");
        return;
    }

    let payload = undefined;
    if (body_parts)
    {
        payload = new Blob(state.parts);
        state.parts = [];
    }
    else if (body_length > 0)
    {
        payload = HEAPU8.slice(body, body + body_length);
    }

    const init = { method : UTF8ToString(method), headers : headers, body : payload, signal : state.controller.signal, redirect : "follow" };
    fetch(UTF8ToString(url), init).then((response) => {
        if (state.done)
        {
            return;
        }

        const encoding = response.headers.get("content-encoding");
        const declared = Number(response.headers.get("content-length"));
        const length = encoding !== null || response.headers.get("content-length") === null || !Number.isFinite(declared) ? -1 : declared;
        const wanted = _varn_fetch_js_on_head(id, response.status, text(JSON.stringify(Array.from(response.headers.entries()))), text(response.url), length);
        if (!wanted || response.body === null)
        {
            if (!wanted)
            {
                state.controller.abort();
            }

            finish(null);
            return;
        }

        state.reader = response.body.getReader();
        state.pump();
    }).catch((failure) => {
        finish(String(failure));
    });
});
// clang-format on

namespace varn::http::client
{

// The browser keeps the pool of connections itself, so a client holds nothing of its own here.
class HttpClientConnections
{
};

namespace
{

// The transfers the browser runs, by the number its side knows each one by.
class FetchRegistry
{
public:
    FetchRegistry() = delete;

    static std::uint32_t add(std::shared_ptr<HttpClientTransfer> transfer)
    {
        const std::uint32_t id = ++next();
        entries().emplace(id, std::move(transfer));
        varn::wasm::WasmAsyncHost::fetchInflight().fetch_add(1, std::memory_order_relaxed);
        return id;
    }

    static std::shared_ptr<HttpClientTransfer> find(std::uint32_t id)
    {
        const auto found = entries().find(id);
        return found == entries().end() ? nullptr : found->second;
    }

    // Takes a transfer the browser finished with, letting the pump know it waits for one less.
    static std::shared_ptr<HttpClientTransfer> remove(std::uint32_t id)
    {
        const auto found = entries().find(id);
        if (found == entries().end())
        {
            return nullptr;
        }

        std::shared_ptr<HttpClientTransfer> transfer = std::move(found->second);
        entries().erase(found);
        varn::wasm::WasmAsyncHost::fetchInflight().fetch_sub(1, std::memory_order_acq_rel);
        return transfer;
    }

private:
    static std::unordered_map<std::uint32_t, std::shared_ptr<HttpClientTransfer>>& entries()
    {
        static std::unordered_map<std::uint32_t, std::shared_ptr<HttpClientTransfer>> map;
        return map;
    }

    static std::uint32_t& next()
    {
        static std::uint32_t counter = 0;
        return counter;
    }
};

// Owns a string the browser side allocated for the engine and frees it once read.
class BrowserText
{
public:
    explicit BrowserText(char* text)
        : text(text)
    {
    }

    ~BrowserText()
    {
        std::free(text);
    }

    BrowserText(const BrowserText&) = delete;
    BrowserText& operator=(const BrowserText&) = delete;

    std::string value() const { return text != nullptr ? std::string(text) : std::string(); }

private:
    char* text;
};

class FetchCallbacks
{
public:
    FetchCallbacks() = delete;

    static int onHead(std::uint32_t id, int status, char* headersJson, char* url, double contentLength)
    {
        const BrowserText headersText(headersJson);
        const BrowserText urlText(url);
        const std::shared_ptr<HttpClientTransfer> transfer = FetchRegistry::find(id);
        if (!transfer)
        {
            return 0;
        }

        ResponseHead head;
        head.status = status;
        head.url = urlText.value();

        // The browser decodes a compressed body before handing it over, so the headers describing the encoded form would contradict what the caller receives and are dropped, and it names every header in lowercase.
        const nlohmann::json parsed = nlohmann::json::parse(headersText.value(), nullptr, false);
        if (parsed.is_array())
        {
            for (const auto& pair : parsed)
            {
                const bool named = pair.is_array() && pair.size() == 2 && pair[0].is_string() && pair[1].is_string();
                if (named && pair[0] != "content-encoding" && pair[0] != "content-length")
                {
                    head.headers.emplace_back(pair[0].get<std::string>(), pair[1].get<std::string>());
                }
            }
        }

        // The length of what the caller receives is known only when nothing was decoded.
        const bool bodiless = transfer->request().method == "HEAD" || status < 200 || status == 204 || status == 304;
        if (bodiless)
        {
            head.contentLength = 0;
        }
        else if (contentLength >= 0)
        {
            head.contentLength = static_cast<std::uint64_t>(contentLength);
        }

        return transfer->head(std::move(head)) ? 1 : 0;
    }

    static int onChunk(std::uint32_t id, char* data, std::size_t length)
    {
        const std::shared_ptr<HttpClientTransfer> transfer = FetchRegistry::find(id);
        const bool keep = transfer && transfer->deliver(data, length);
        std::free(data);
        return keep ? 1 : 0;
    }

    static void onEnd(std::uint32_t id, int failed, char* message)
    {
        const BrowserText messageText(message);
        const std::shared_ptr<HttpClientTransfer> transfer = FetchRegistry::remove(id);
        if (!transfer)
        {
            return;
        }

        transfer->clearInterrupt();
        transfer->clearFlow();

        if (failed != 0)
        {
            transfer->fail(Error{ErrorCode::Network, messageText.value()});
            return;
        }

        transfer->succeed();
    }
};

// The pieces a body that streams from the caller is gathered in before the browser sends it.
constexpr std::size_t kPieceBytes = 65536;

} // namespace

std::shared_ptr<HttpClientConnections> HttpClientPerform::connections()
{
    return std::make_shared<HttpClientConnections>();
}

// Starts a request in the browser and answers at once, since every callback arrives later from the event loop of the page.
void HttpClientPerform::start(HttpClientConnections& /*connections*/, const std::shared_ptr<HttpClientTransfer>& transfer)
{
    const Request& request = transfer->request();
    const std::uint32_t id = FetchRegistry::add(transfer);
    varn_fetch_open(id);

    // clang-format off
    transfer->setFlow([id] { varn_fetch_pause(id); }, [id] { varn_fetch_resume(id); });
    const bool interruptible = transfer->setInterrupt([id] { varn_fetch_abort(id); });
    // clang-format on

    // A browser streams a request body only over HTTP/2 and only in some engines, so a body from the caller is gathered into a blob of parts, which the browser may keep out of the memory of the module.
    std::optional<Error> refused;
    if (!interruptible)
    {
        refused = Error{ErrorCode::Cancelled, "[HttpClient] The request was cancelled."};
    }
    else if (request.bodySource)
    {
        try
        {
            char buffer[kPieceBytes];
            for (std::size_t length = transfer->readBody(buffer, sizeof(buffer)); length > 0; length = transfer->readBody(buffer, sizeof(buffer)))
            {
                varn_fetch_body_part(id, buffer, length);
            }
        }
        catch (const HttpClientFailure& failure)
        {
            refused = failure.error();
        }
    }

    if (refused)
    {
        varn_fetch_close(id);
        FetchRegistry::remove(id);
        transfer->clearInterrupt();
        transfer->clearFlow();
        transfer->fail(*refused);
        return;
    }

    nlohmann::json headers = nlohmann::json::array();
    for (const auto& [name, value] : request.headers)
    {
        headers.push_back({name, value});
    }

    const std::string headersJson = headers.dump();
    varn_fetch_start(id, request.url.c_str(), request.method.c_str(), headersJson.c_str(), request.body.data(), request.body.size(), request.bodySource ? 1 : 0);
}

} // namespace varn::http::client

extern "C"
{

    EMSCRIPTEN_KEEPALIVE int varn_fetch_js_on_head(std::uint32_t id, int status, char* headersJson, char* url, double contentLength)
    {
        return varn::http::client::FetchCallbacks::onHead(id, status, headersJson, url, contentLength);
    }

    EMSCRIPTEN_KEEPALIVE int varn_fetch_js_on_chunk(std::uint32_t id, char* data, std::size_t length)
    {
        return varn::http::client::FetchCallbacks::onChunk(id, data, length);
    }

    EMSCRIPTEN_KEEPALIVE void varn_fetch_js_on_end(std::uint32_t id, int failed, char* message)
    {
        varn::http::client::FetchCallbacks::onEnd(id, failed, message);
    }

} // extern "C"
