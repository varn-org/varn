#if !defined(__EMSCRIPTEN__)
#error "EmscriptenFetchHttpClient is only built for Emscripten (VARN_HTTP_CLIENT_DRIVER=EMSCRIPTEN_FETCH)."
#endif

#include "../../HttpClientResponseLua.h"
#include "varn/async/Promise.h"
#include "varn/http/HttpClientPerform.h"
#include "varn/wasm/WasmAsyncHost.h"

#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>

#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace varn::http::client
{

using varn::async::Promise;

struct FetchContext
{
    std::shared_ptr<Promise> promise;
    std::string method;
    std::string url;
    std::string body;
    std::string headersJson;
};

class FetchResult
{
public:
    static void succeed(FetchContext* ctx, int status, const void* data, size_t numBytes)
    {
        std::string respBody;
        if (data != nullptr && numBytes > 0)
        {
            respBody.assign(static_cast<const char*>(data), numBytes);
        }

        try
        {
            // clang-format off
            ctx->promise->resolveCustom([status, respBody](lua_State* L)
            {
                HttpClientResponseLua::pushResponse(L, status, {}, respBody);
            });
            // clang-format on
        }
        catch (...)
        {
        }

        delete ctx;
    }

    static void fail(FetchContext* ctx, const std::string& message)
    {
        try
        {
            ctx->promise->reject(message);
        }
        catch (...)
        {
        }

        delete ctx;
    }
};

extern "C"
{

    EMSCRIPTEN_KEEPALIVE void varn_fetch_js_on_success(uintptr_t ctxHandle, int httpStatus, uintptr_t bodyPtr, size_t bodyLen)
    {
        auto* ctx = reinterpret_cast<FetchContext*>(ctxHandle);
        const void* data = (bodyLen > 0 && bodyPtr != 0) ? reinterpret_cast<const void*>(bodyPtr) : nullptr;
        if (ctx != nullptr)
        {
            try
            {
                FetchResult::succeed(ctx, httpStatus, data, bodyLen);
            }
            catch (...)
            {
            }
        }

        if (bodyPtr != 0)
        {
            std::free(reinterpret_cast<void*>(bodyPtr));
        }

        varn::wasm::WasmAsyncHost::fetchInflight().fetch_sub(1, std::memory_order_acq_rel);
    }

    EMSCRIPTEN_KEEPALIVE void varn_fetch_js_on_error(uintptr_t ctxHandle, uintptr_t msgPtr)
    {
        auto* ctx = reinterpret_cast<FetchContext*>(ctxHandle);
        std::string copy;
        if (msgPtr != 0)
        {
            copy.assign(reinterpret_cast<const char*>(msgPtr));
            std::free(reinterpret_cast<void*>(msgPtr));
        }
        else
        {
            copy = "[EmscriptenFetchHttpClient] The request failed.";
        }

        if (ctx != nullptr)
        {
            try
            {
                FetchResult::fail(ctx, copy);
            }
            catch (...)
            {
            }
        }

        varn::wasm::WasmAsyncHost::fetchInflight().fetch_sub(1, std::memory_order_acq_rel);
    }

} // extern "C"

// clang-format off
EM_JS(void, varn_browser_fetch_start, (uintptr_t ctx, const char* url, const char* method, const char* headers_json, const char* body_ptr, int body_len, int timeout_sec, double max_bytes), {
    function reportError(message)
    {
        const len = lengthBytesUTF8(message) + 1;
        const eptr = _malloc(len);
        if (!eptr)
        {
            _varn_fetch_js_on_error(ctx, 0);
            return;
        }

        stringToUTF8(message, eptr, len);
        _varn_fetch_js_on_error(ctx, eptr);
    }

    const urlStr = UTF8ToString(url);
    const methodStr = UTF8ToString(method);
    const headersStr = UTF8ToString(headers_json);

    let parsed;
    try
    {
        parsed = JSON.parse(headersStr);
    }
    catch (e)
    {
        reportError("[EmscriptenFetchHttpClient] Invalid headers json (" + String(e) + ")");
        return;
    }

    if (parsed === null || typeof parsed !== "object" || Array.isArray(parsed))
    {
        reportError("[EmscriptenFetchHttpClient] Headers json must be a JSON object.");
        return;
    }

    const headersInit = {};
    for (const key of Object.keys(parsed))
    {
        const value = parsed[key];
        if (typeof value !== "string")
        {
            reportError("[EmscriptenFetchHttpClient] Header value must be a string (key: " + key + ")");
            return;
        }

        headersInit[key] = value;
    }

    let body = undefined;
    if (body_len > 0 && body_ptr)
    {
        body = HEAPU8.subarray(body_ptr, body_ptr + body_len);
    }

    const ctrl = new AbortController();
    let tid = 0;
    if (timeout_sec > 0)
    {
        tid = setTimeout(() => ctrl.abort(), timeout_sec * 1000);
    }

    fetch(urlStr, {method : methodStr, headers : headersInit, body : body, signal : ctrl.signal})
        .then((res) => {
            const st = res.status;
            return res.arrayBuffer().then((ab) => ({status : st, ab : ab}));
        })
        .then((x) => {
            if (tid)
            {
                clearTimeout(tid);
            }

            const u8 = new Uint8Array(x.ab);
            const len = u8.byteLength;
            if (max_bytes > 0 && len > max_bytes)
            {
                reportError("[EmscriptenFetchHttpClient] The response body exceeds the maximum allowed size.");
                return;
            }

            let ptr = 0;
            if (len > 0)
            {
                ptr = _malloc(len);
                if (!ptr)
                {
                    throw new Error("[EmscriptenFetchHttpClient] The response body could not be allocated.");
                }

                HEAPU8.set(u8, ptr);
            }

            _varn_fetch_js_on_success(ctx, x.status, ptr, len);
        })
        .catch((e) => {
            if (tid)
            {
                clearTimeout(tid);
            }

            reportError(String(e));
        });
});
// clang-format on

void HttpClientPerform::performAsync(
    const std::shared_ptr<Promise>& promise,
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body,
    int timeoutSeconds,
    std::size_t maxResponseBytes)
{
    auto* ctx = new FetchContext{promise, method, url, body, {}};
    nlohmann::json hdr = nlohmann::json::object();
    for (const auto& kv : headers)
    {
        hdr[kv.first] = kv.second;
    }

    ctx->headersJson = hdr.dump();

    varn::wasm::WasmAsyncHost::fetchInflight().fetch_add(1, std::memory_order_relaxed);

    varn_browser_fetch_start(reinterpret_cast<uintptr_t>(ctx), ctx->url.c_str(), ctx->method.c_str(),
                             ctx->headersJson.c_str(), ctx->body.empty() ? nullptr : ctx->body.data(),
                             static_cast<int>(ctx->body.size()), timeoutSeconds,
                             static_cast<double>(maxResponseBytes));
}

} // namespace varn::http::client
