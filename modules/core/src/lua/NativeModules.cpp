#include "varn/lua/NativeModules.h"

#include "varn/async/AsyncModule.h"
#include "varn/async/Promise.h"
#include "varn/crypto/CryptoModule.h"
#include "varn/datetime/DatetimeModule.h"
#include "varn/ffi/FfiModule.h"
#include "varn/fs/FsModule.h"
#include "varn/http/HttpServerModule.h"
#include "varn/json/JsonModule.h"
#include "varn/log/LogModule.h"
#include "varn/platform/PlatformModule.h"
#include "varn/process/ProcessModule.h"
#include "varn/socket/SocketModule.h"
#include "varn/xml/XmlModule.h"
#include "varn/zip/ZipModule.h"

namespace varn::lua
{

void NativeModuleRegistry::installAll(lua_State* L)
{
    async::Promise::installMetatable(L);
    async::AsyncModule::install(L);
    fs::FsModule::install(L);
    ffi::FfiModule::install(L);
    log::LogModule::install(L);
    crypto::CryptoModule::install(L);
    datetime::DatetimeModule::install(L);
    json::JsonModule::install(L);
    xml::XmlModule::install(L);
    platform::PlatformModule::install(L);
    process::ProcessModule::install(L);
    zip::ZipModule::install(L);
    http::HttpServerModule::install(L);
    socket::SocketModule::install(L);
}

} // namespace varn::lua
