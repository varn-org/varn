#pragma once

#include <jni.h>

namespace varn::http::client
{

class AndroidHttpBridge
{
public:
    AndroidHttpBridge() = delete;

    static void publish(JavaVM* vm);
};

} // namespace varn::http::client
