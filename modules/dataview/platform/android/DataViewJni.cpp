#include "varn/dataview/DataViewJni.h"

#include "varn/dataview/DataView.h"
#include "varn/log/Log.h"
#include "varn/varn.h"

#include <cstdint>
#include <limits>

namespace varn::dataview
{

namespace
{

// The natives of `com.varn.VarnDataView`, which the host registers from `JNI_OnLoad` so the class resolves through the class loader of the app.
class DataViewNatives
{
public:
    static varn_dataview* asView(jlong handle)
    {
        return reinterpret_cast<varn_dataview*>(handle);
    }

    // The last reference may end on a thread Java has never seen, which is attached only for as long as the release takes.
    static void deleteGlobal(JavaVM* machine, jobject reference)
    {
        JNIEnv* env = nullptr;
        bool attached = false;
        if (machine->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
        {
            if (machine->AttachCurrentThreadAsDaemon(&env, nullptr) != JNI_OK)
            {
                return;
            }

            attached = true;
        }

        env->DeleteGlobalRef(reference);
        if (attached)
        {
            machine->DetachCurrentThread();
        }
    }

    static void throwState(JNIEnv* env, const char* message)
    {
        jclass type = env->FindClass("java/lang/IllegalStateException");
        if (type != nullptr)
        {
            env->ThrowNew(type, message);
        }
    }

    static jlong allocate(JNIEnv*, jclass, jlong size)
    {
        return reinterpret_cast<jlong>(varn_dataview_new(static_cast<size_t>(size)));
    }

    // The view keeps a global reference to the buffer, so the memory Java allocated outlives every reference of every language.
    static jlong wrap(JNIEnv* env, jclass, jobject direct)
    {
        void* address = env->GetDirectBufferAddress(direct);
        const jlong capacity = env->GetDirectBufferCapacity(direct);
        if (capacity < 0 || (address == nullptr && capacity != 0))
        {
            throwState(env, "The buffer of a data view has no native memory.");
            return 0;
        }

        JavaVM* machine = nullptr;
        env->GetJavaVM(&machine);
        jobject held = env->NewGlobalRef(direct);
        try
        {
            // clang-format off
            return reinterpret_cast<jlong>(DataView::wrap(address, static_cast<std::size_t>(capacity), [machine, held]
            {
                deleteGlobal(machine, held);
            }).detach());
            // clang-format on
        }
        catch (...)
        {
            env->DeleteGlobalRef(held);
        }

        throwState(env, "The buffer could not be wrapped in a data view.");
        return 0;
    }

    static jlong acquire(JNIEnv*, jclass, jlong id, jlong pointer, jlong size)
    {
        return reinterpret_cast<jlong>(varn_dataview_acquire(static_cast<unsigned long long>(id), reinterpret_cast<const void*>(static_cast<std::uintptr_t>(pointer)), static_cast<size_t>(size)));
    }

    static void retain(JNIEnv*, jclass, jlong handle)
    {
        varn_dataview_retain(asView(handle));
    }

    static void release(JNIEnv*, jclass, jlong handle)
    {
        varn_dataview_release(asView(handle));
    }

    static jobject buffer(JNIEnv* env, jclass, jlong handle)
    {
        const size_t size = varn_dataview_size(asView(handle));
        if (size > static_cast<size_t>(std::numeric_limits<jint>::max()))
        {
            throwState(env, "A data view larger than two gigabytes has no ByteBuffer.");
            return nullptr;
        }

        return env->NewDirectByteBuffer(varn_dataview_data(asView(handle)), static_cast<jlong>(size));
    }

    static jlong size(JNIEnv*, jclass, jlong handle)
    {
        return static_cast<jlong>(varn_dataview_size(asView(handle)));
    }

    static jlong id(JNIEnv*, jclass, jlong handle)
    {
        return static_cast<jlong>(varn_dataview_id(asView(handle)));
    }

    static jlong pointer(JNIEnv*, jclass, jlong handle)
    {
        return static_cast<jlong>(reinterpret_cast<std::uintptr_t>(varn_dataview_data(asView(handle))));
    }
};

} // namespace

// Answers false and logs why when the class is missing, such as when R8 removed it from an app.
bool DataViewJni::publish(JNIEnv* env)
{
    jclass type = env->FindClass("com/varn/VarnDataView");
    if (type == nullptr)
    {
        env->ExceptionClear();
        log::Log::error("The Java class \"com.varn.VarnDataView\" is missing, so Java cannot share data views. Keep it in the rules of R8.");
        return false;
    }

    const JNINativeMethod methods[] = {
        {const_cast<char*>("nativeAllocate"), const_cast<char*>("(J)J"), reinterpret_cast<void*>(&DataViewNatives::allocate)},
        {const_cast<char*>("nativeWrap"), const_cast<char*>("(Ljava/nio/ByteBuffer;)J"), reinterpret_cast<void*>(&DataViewNatives::wrap)},
        {const_cast<char*>("nativeAcquire"), const_cast<char*>("(JJJ)J"), reinterpret_cast<void*>(&DataViewNatives::acquire)},
        {const_cast<char*>("nativeRetain"), const_cast<char*>("(J)V"), reinterpret_cast<void*>(&DataViewNatives::retain)},
        {const_cast<char*>("nativeRelease"), const_cast<char*>("(J)V"), reinterpret_cast<void*>(&DataViewNatives::release)},
        {const_cast<char*>("nativeBuffer"), const_cast<char*>("(J)Ljava/nio/ByteBuffer;"), reinterpret_cast<void*>(&DataViewNatives::buffer)},
        {const_cast<char*>("nativeSize"), const_cast<char*>("(J)J"), reinterpret_cast<void*>(&DataViewNatives::size)},
        {const_cast<char*>("nativeId"), const_cast<char*>("(J)J"), reinterpret_cast<void*>(&DataViewNatives::id)},
        {const_cast<char*>("nativePointer"), const_cast<char*>("(J)J"), reinterpret_cast<void*>(&DataViewNatives::pointer)},
    };
    const bool registered = env->RegisterNatives(type, methods, static_cast<jint>(sizeof(methods) / sizeof(methods[0]))) == JNI_OK;
    env->DeleteLocalRef(type);
    if (!registered)
    {
        env->ExceptionClear();
        log::Log::error("The natives of the Java class \"com.varn.VarnDataView\" could not be registered.");
    }

    return registered;
}

} // namespace varn::dataview
