plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.varn"
    compileSdk = 34
    ndkVersion = "27.0.12077973"

    defaultConfig {
        minSdk = (project.findProperty("varnMinSdk") as String?)?.toInt() ?: 24

        // build the native library for every supported abi (override with -PvarnAbis=arm64-v8a,...).
        // x86 (32-bit) is dropped since it is obsolete on android and libffi's i686 asm breaks under ndk clang
        val abis = (project.findProperty("varnAbis") as String?)
            ?.split(",")
            ?.map { it.trim() }
            ?: listOf("armeabi-v7a", "arm64-v8a", "x86_64")
        ndk {
            abiFilters += abis
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.31.6"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}
