# The engine reaches these classes from C++ by name, so R8 must not rename or remove them. They are merged
# into the consuming application's configuration, which is what keeps a minified release from failing at load.

-keep class com.varn.VarnRuntime { *; }
-keep class com.varn.VarnRuntime$HostFunction { *; }
-keep class com.varn.VarnRuntime$ConsoleSink { *; }

-keep class com.varn.VarnHttp { *; }
-keep class com.varn.VarnHttp$Response { *; }
-keep class com.varn.VarnHttp$ChunkSink { *; }
-keep class com.varn.NativeChunkSink { *; }

-keepclasseswithmembernames class com.varn.** {
    native <methods>;
}
