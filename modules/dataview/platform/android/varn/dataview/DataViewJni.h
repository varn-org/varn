#pragma once

#include <jni.h>

namespace varn::dataview
{

class DataViewJni
{
public:
    DataViewJni() = delete;

    static bool publish(JNIEnv* env);
};

} // namespace varn::dataview
