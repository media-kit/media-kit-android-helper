// This file is a part of media_kit (https://github.com/alexmercerind/media_kit).
//
// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the LICENSE file.

#include <jni.h>
#include <sys/stat.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <string>

// ------------------------------------------------------------
// Globals. For sharing necessary values between Java & Dart.
// ------------------------------------------------------------

JavaVM* g_jvm = NULL;
int32_t g_api_level = -1;
int8_t g_is_emulator = -1;
char* g_external_files_dir = NULL;
AAssetManager* g_asset_manager = NULL;

// ------------------------------------------------------------
// Native. For access through Dart FFI.
// ------------------------------------------------------------

extern "C" __attribute__ ((visibility ("default"))) void MediaKitAndroidHelperCopyAssetToExternalFilesDir(const char* asset_name,  char* result) {
    strcpy(result, "");

    if (g_jvm == NULL) {
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "JavaVM* is nullptr.");
        return;
    }
    if (g_asset_manager == NULL) {
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "AAssetManager* is nullptr.");
        return;
    }

    AAsset* asset = AAssetManager_open(g_asset_manager, asset_name, AASSET_MODE_BUFFER);

    if (asset == NULL) {
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "NOT FOUND: %s", asset_name);
        return;
    }

    off_t length = AAsset_getLength(asset);

    auto buffer = std::make_unique<uint8_t[]>(length);

    int32_t size = AAsset_read(asset, buffer.get(), length);

    __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "Asset name: %s", asset_name);
    __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "Asset size: %d", size);

    AAsset_close(asset);

    std::string directory_out = g_external_files_dir;
    directory_out += "/com.alexmercerind.media_kit/";
    std::string file_name = asset_name;
    std::replace(file_name.begin(), file_name.end(), '/', '_');
    std::string output_file = directory_out + file_name;

    __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "Asset directory: %s", directory_out.c_str());

    struct stat st;
    int32_t stat_result = stat(directory_out.c_str(), &st);
    if (stat_result == -1) {
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "Creating asset directory...");
        mkdir(directory_out.c_str(), 0777);
    } else {
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "Asset directory exists.");
    }

    __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "Asset file: %s", output_file.c_str());

    FILE* file = fopen(output_file.c_str(), "rb");
    if (file != NULL) {
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "Asset file exists.");
        fclose(file);
    } else {
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "Creating asset file...");
        file = fopen(output_file.c_str(), "wb");
        if (file != NULL) {
            fwrite(buffer.get(), sizeof(uint8_t), size, file);
            fclose(file);
        }
    }

    strcpy(result, output_file.c_str());
}

extern "C" __attribute__ ((visibility ("default"))) void* MediaKitAndroidHelperGetJavaVM() {
    return g_jvm;
}

extern "C" __attribute__ ((visibility ("default"))) int32_t MediaKitAndroidHelperGetAPILevel() {
    return g_api_level;
}

extern "C" __attribute__ ((visibility ("default"))) int8_t MediaKitAndroidHelperIsEmulator() {
    return g_is_emulator;
}

// ------------------------------------------------------------
// JNI. For access through platform channels.
// ------------------------------------------------------------

extern "C" JNIEXPORT jlong JNICALL
Java_com_alexmercerind_mediakitandroidhelper_MediaKitAndroidHelper_newGlobalObjectRef(JNIEnv *env, jclass, jobject obj) {
    return (jlong) (intptr_t) env->NewGlobalRef(obj);
}

extern "C" JNIEXPORT void JNICALL
Java_com_alexmercerind_mediakitandroidhelper_MediaKitAndroidHelper_deleteGlobalObjectRef(JNIEnv *env, jclass, jlong ref) {
    env->DeleteGlobalRef((jobject) (intptr_t) ref);
}

extern "C" JNIEXPORT void JNICALL
Java_com_alexmercerind_mediakitandroidhelper_MediaKitAndroidHelper_setApplicationContext(JNIEnv *env, jclass, jobject context) {
    // g_jvm

    if (g_jvm == NULL) {
        env->GetJavaVM(&g_jvm);
    }

    //  g_api_level

    if (g_api_level == -1) {
        jclass build_class = env->FindClass("android/os/Build$VERSION");
        jfieldID sdk_int_field = env->GetStaticFieldID(build_class, "SDK_INT", "I");
        g_api_level = env->GetStaticIntField(build_class, sdk_int_field);
    }

    // g_is_emulator

    if (g_is_emulator == -1) {
        // https://github.com/fluttercommunity/plus_plugins/blob/ff54dc49230ee5f8b772a3326d4ff3758618df80/packages/device_info_plus/device_info_plus/android/src/main/kotlin/dev/fluttercommunity/plus/device_info/MethodCallHandlerImpl.kt#L110-L125

        jclass build_class = env->FindClass("android/os/Build");

        char brand_chars[1024];
        char device_chars[1024];
        char fingerprint_chars[1024];
        char hardware_chars[1024];
        char model_chars[1024];
        char manufacturer_chars[1024];
        char product_chars[1024];

        jfieldID brand_field_id = env->GetStaticFieldID(build_class, "BRAND", "Ljava/lang/String;");
        jstring brand_jstring = (jstring)env->GetStaticObjectField(build_class, brand_field_id);
        const char* source_brand_chars = env->GetStringUTFChars(brand_jstring, NULL);
        if (source_brand_chars != NULL) {
            strncpy(brand_chars, source_brand_chars, strlen(source_brand_chars));
            brand_chars[strlen(source_brand_chars) + 1] = '\0';
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", brand_chars);

        jfieldID device_field_id = env->GetStaticFieldID(build_class, "DEVICE", "Ljava/lang/String;");
        jstring device_jstring = (jstring)env->GetStaticObjectField(build_class, device_field_id);
        const char* source_device_chars = env->GetStringUTFChars(device_jstring, NULL);
        if (source_device_chars != NULL) {
            strncpy(device_chars, source_device_chars, strlen(source_device_chars));
            device_chars[strlen(source_device_chars) + 1] = '\0';
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", device_chars);

        jfieldID fingerprint_field_id = env->GetStaticFieldID(build_class, "FINGERPRINT", "Ljava/lang/String;");
        jstring fingerprint_jstring = (jstring)env->GetStaticObjectField(build_class, fingerprint_field_id);
        const char* source_fingerprint_chars = env->GetStringUTFChars(fingerprint_jstring, NULL);
        if (source_fingerprint_chars != NULL) {
            strncpy(fingerprint_chars, source_fingerprint_chars, strlen(source_fingerprint_chars));
            fingerprint_chars[strlen(source_fingerprint_chars) + 1] = '\0';
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", fingerprint_chars);

        jfieldID hardware_field_id = env->GetStaticFieldID(build_class, "HARDWARE", "Ljava/lang/String;");
        jstring hardware_jstring = (jstring)env->GetStaticObjectField(build_class, hardware_field_id);
        const char* source_hardware_chars = env->GetStringUTFChars(hardware_jstring, NULL);
        if (source_hardware_chars != NULL) {
            strncpy(hardware_chars, source_hardware_chars, strlen(source_hardware_chars));
            hardware_chars[strlen(source_hardware_chars) + 1] = '\0';
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", hardware_chars);

        jfieldID model_field_id = env->GetStaticFieldID(build_class, "MODEL", "Ljava/lang/String;");
        jstring model_jstring = (jstring)env->GetStaticObjectField(build_class, model_field_id);
        const char* source_model_chars = env->GetStringUTFChars(model_jstring, NULL);
        if (source_model_chars != NULL) {
            strncpy(model_chars, source_model_chars, strlen(source_model_chars));
            model_chars[strlen(source_model_chars) + 1] = '\0';
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", model_chars);

        jfieldID manufacturer_field_id = env->GetStaticFieldID(build_class, "MANUFACTURER", "Ljava/lang/String;");
        jstring manufacturer_jstring = (jstring)env->GetStaticObjectField(build_class, manufacturer_field_id);
        const char* source_manufacturer_chars = env->GetStringUTFChars(manufacturer_jstring, NULL);
        if (source_manufacturer_chars != NULL) {
            strncpy(manufacturer_chars, source_manufacturer_chars, strlen(source_manufacturer_chars));
            manufacturer_chars[strlen(source_manufacturer_chars) + 1] = '\0';
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", manufacturer_chars);

        jfieldID product_field_id = env->GetStaticFieldID(build_class, "PRODUCT", "Ljava/lang/String;");
        jstring product_jstring = (jstring)env->GetStaticObjectField(build_class, product_field_id);
        const char* source_product_chars = env->GetStringUTFChars(product_jstring, NULL);
        if (source_product_chars != NULL) {
            strncpy(product_chars, source_product_chars, strlen(source_product_chars));
            product_chars[strlen(source_product_chars) + 1] = '\0';
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", product_chars);

        if (
                (strncmp(brand_chars, "generic", sizeof("generic") - 1) == 0 && strncmp(device_chars, "generic", sizeof("generic") - 1) == 0)
                || strncmp(fingerprint_chars, "generic", sizeof("generic") - 1) == 0
                || strncmp(fingerprint_chars, "unknown", sizeof("unknown") - 1) == 0
                || strstr(hardware_chars, "goldfish") != NULL
                || strstr(hardware_chars, "ranchu") != NULL
                || strstr(model_chars, "google_sdk") != NULL
                || strstr(model_chars, "Emulator") != NULL
                || strstr(model_chars, "Android SDK built for x86") != NULL
                || strstr(manufacturer_chars, "Genymotion") != NULL
                || strstr(product_chars, "sdk_google") != NULL
                || strstr(product_chars, "google_sdk") != NULL
                || strstr(product_chars, "sdk") != NULL
                || strstr(product_chars, "sdk_x86") != NULL
                || strstr(product_chars, "vbox86p") != NULL
                || strstr(product_chars, "emulator") != NULL
                || strstr(product_chars, "simulator") != NULL) {
            g_is_emulator = 1;
        }

        env->ReleaseStringUTFChars(brand_jstring, source_brand_chars);
        env->ReleaseStringUTFChars(device_jstring, source_device_chars);
        env->ReleaseStringUTFChars(fingerprint_jstring, source_fingerprint_chars);
        env->ReleaseStringUTFChars(hardware_jstring, source_hardware_chars);
        env->ReleaseStringUTFChars(model_jstring, source_model_chars);
        env->ReleaseStringUTFChars(manufacturer_jstring, source_manufacturer_chars);
        env->ReleaseStringUTFChars(product_jstring, source_product_chars);

        env->DeleteLocalRef(brand_jstring);
        env->DeleteLocalRef(device_jstring);
        env->DeleteLocalRef(fingerprint_jstring);
        env->DeleteLocalRef(hardware_jstring);
        env->DeleteLocalRef(model_jstring);
        env->DeleteLocalRef(manufacturer_jstring);
        env->DeleteLocalRef(product_jstring);
    }

    // g_external_files_dir

    if (g_external_files_dir == NULL) {
        g_external_files_dir = new char[2048];
        jclass context_obj = env->GetObjectClass(context);
        jmethodID method_id = env->GetMethodID(context_obj, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
        jobject external_files_dir_jobject = env->CallObjectMethod(context, method_id, (jstring)NULL);
        jmethodID get_absolute_path_method_id = env->GetMethodID(env->FindClass("java/io/File"), "getAbsolutePath", "()Ljava/lang/String;");
        jstring external_files_dir_jstring = (jstring)env->CallObjectMethod(external_files_dir_jobject, get_absolute_path_method_id);
        const char* external_files_dir_chars = env->GetStringUTFChars(external_files_dir_jstring, NULL);

        strncpy(g_external_files_dir, external_files_dir_chars, strlen(external_files_dir_chars));
        g_external_files_dir[strlen(external_files_dir_chars) + 1] = '\0';

        env->ReleaseStringUTFChars(external_files_dir_jstring, external_files_dir_chars);

        env->DeleteLocalRef(external_files_dir_jobject);
        env->DeleteLocalRef(external_files_dir_jstring);
    }

    // g_asset_manager

    if (g_asset_manager == NULL) {
        jclass context_class = env->GetObjectClass(context);
        jmethodID asset_manager_id = env->GetMethodID(context_class, "getAssets", "()Landroid/content/res/AssetManager;");
        jobject asset_manager_jobject = env->CallObjectMethod(context, asset_manager_id);

        g_asset_manager = AAssetManager_fromJava(env, env->NewGlobalRef(asset_manager_jobject));

        env->DeleteLocalRef(asset_manager_jobject);
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_alexmercerind_mediakitandroidhelper_MediaKitAndroidHelper_copyAssetToExternalFilesDir(JNIEnv *env, jclass, jstring asset_name) {
    const char* asset_name_chars = env->GetStringUTFChars(asset_name, NULL);
    char result[2048];
    MediaKitAndroidHelperCopyAssetToExternalFilesDir(asset_name_chars, result);
    env->ReleaseStringUTFChars(asset_name, asset_name_chars);
    return env->NewStringUTF(result);
}
