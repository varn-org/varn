plugins {
    alias(libs.plugins.android.application)
}

// The engine is the only place a version is defined, and the task `fetch-native` records the one that was fetched.
val engineVersionFile = rootProject.file("../varn-version.txt")
val engineVersion: String = if (engineVersionFile.exists()) {
    engineVersionFile.readText().trim()
} else {
    throw GradleException("The file \"${engineVersionFile.path}\" is missing. Run \"python3 varn.py fetch-native\" to fetch it.")
}

android {
    namespace = "com.varn.app"
    // AndroidX Core 1.19 refuses to compile against anything older, so the platform follows it.
    compileSdk {
        version = release(37)
    }

    defaultConfig {
        applicationId = "com.varn.app"
        minSdk = 24
        targetSdk = 36
        versionCode = 1
        versionName = engineVersion

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    buildTypes {
        release {
            // The release build is minified so the keep rules the engine ships with its AAR are actually exercised.
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"))
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    // The Lua samples live beside the iOS app rather than inside either one, so both ship the same set.
    sourceSets["main"].assets.srcDir("../../samples")

    // The engine already carries its own native libraries, so nothing here needs the NDK.
    packaging {
        jniLibs {
            useLegacyPackaging = false
        }
    }
}

dependencies {
    // The released AAR is fetched with the command `python3 varn.py fetch-native`.
    implementation(files("libs/varn-release.aar"))

    implementation(libs.androidx.activity.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.core.ktx)
    implementation(libs.material)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit)
}
