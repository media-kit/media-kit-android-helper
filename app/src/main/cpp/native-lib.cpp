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

#include <unistd.h>

#include <future>
#include <string>
#include <thread>

// ------------------------------------------------------------
// Globals. For sharing necessary values between Java & Dart.
// ------------------------------------------------------------

JavaVM* g_jvm = NULL;
char* g_files_dir = NULL;
int8_t g_is_emulator = -1;
AAssetManager* g_asset_manager = NULL;
jclass g_media_kit_android_helper_class = NULL;

// ------------------------------------------------------------
// Native. For access through Dart FFI.
// ------------------------------------------------------------

extern "C" __attribute__ ((visibility ("default"))) void MediaKitAndroidHelperCopyAssetToFilesDir(const char* asset_name,  char* result) {
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

    std::string directory_out = g_files_dir;
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

extern "C" __attribute__ ((visibility ("default"))) char* MediaKitAndroidHelperGetFilesDir() {
    return g_files_dir;
}

extern "C" __attribute__ ((visibility ("default"))) int8_t MediaKitAndroidHelperIsEmulator() {
    return g_is_emulator;
}

extern "C" __attribute__ ((visibility ("default"))) int32_t MediaKitAndroidHelperGetAPILevel() {
    return android_get_device_api_level();
}

extern "C" __attribute__ ((visibility ("default"))) int32_t MediaKitAndroidHelperOpenFileDescriptor(const char* uri) {
    auto file_descriptor_promise = std::promise<int32_t>{};
    std::thread([&] () {
        if (g_jvm != NULL && g_media_kit_android_helper_class != NULL) {
            __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "MediaKitAndroidHelperOpenFileDescriptor: %s", uri);
            JNIEnv* env = NULL;
            bool attached = false;
            jint get_env_result = g_jvm->GetEnv((void**)env, JNI_VERSION_1_6);

            __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "get_env_result = %d", get_env_result);

            if (get_env_result != JNI_OK) {
                if (g_jvm->AttachCurrentThread(&env, NULL) == JNI_OK) {
                    attached = true;
                    __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "JavaVM::AttachCurrentThread Success");
                } else {
                    __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "JavaVM::AttachCurrentThread Failure");
                }
            }

            if (env == NULL) {
                __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "env = NULL");
            }
            if (env != NULL) {
                jstring uri_jstring = env->NewStringUTF(uri);

                jmethodID open_file_descriptor_method_id = env->GetStaticMethodID(g_media_kit_android_helper_class, "openFileDescriptorJava", "(Ljava/lang/String;)I");
                jint file_descriptor = env->CallStaticIntMethod(g_media_kit_android_helper_class, open_file_descriptor_method_id, uri_jstring);

                __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "file_descriptor = %d", file_descriptor);

                env->DeleteLocalRef(uri_jstring);

                if (attached) {
                    g_jvm->DetachCurrentThread();
                }

                file_descriptor_promise.set_value(file_descriptor);
                return;
            }
        }
        file_descriptor_promise.set_value(-1);
    }).detach();
    return file_descriptor_promise.get_future().get();
}

extern "C" __attribute__ ((visibility ("default"))) void MediaKitAndroidHelperCloseFileDescriptor(int32_t file_descriptor) {
    close(file_descriptor);
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
Java_com_alexmercerind_mediakitandroidhelper_MediaKitAndroidHelper_setApplicationContextNative(JNIEnv *env, jclass, jobject context) {
    // g_jvm

    if (g_jvm == NULL) {
        env->GetJavaVM(&g_jvm);
    }

    // g_is_emulator

    if (g_is_emulator == -1) {

        // https://github.com/fluttercommunity/plus_plugins/blob/ff54dc49230ee5f8b772a3326d4ff3758618df80/packages/device_info_plus/device_info_plus/android/src/main/kotlin/dev/fluttercommunity/plus/device_info/MethodCallHandlerImpl.kt#L110-L125

        g_is_emulator = 0;

        jclass build_class = env->FindClass("android/os/Build");

        char brand_chars[1024];
        char device_chars[1024];
        char fingerprint_chars[1024];
        char hardware_chars[1024];
        char model_chars[1024];
        char manufacturer_chars[1024];
        char product_chars[1024];

        memset(brand_chars, '\0', 1024);
        memset(device_chars, '\0', 1024);
        memset(fingerprint_chars, '\0', 1024);
        memset(hardware_chars, '\0', 1024);
        memset(model_chars, '\0', 1024);
        memset(manufacturer_chars, '\0', 1024);
        memset(product_chars, '\0', 1024);

        jfieldID brand_field_id = env->GetStaticFieldID(build_class, "BRAND", "Ljava/lang/String;");
        jstring brand_jstring = (jstring)env->GetStaticObjectField(build_class, brand_field_id);
        const char* source_brand_chars = env->GetStringUTFChars(brand_jstring, NULL);
        if (source_brand_chars != NULL) {
            strncpy(brand_chars, source_brand_chars, 1024);
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", brand_chars);

        jfieldID device_field_id = env->GetStaticFieldID(build_class, "DEVICE", "Ljava/lang/String;");
        jstring device_jstring = (jstring)env->GetStaticObjectField(build_class, device_field_id);
        const char* source_device_chars = env->GetStringUTFChars(device_jstring, NULL);
        if (source_device_chars != NULL) {
            strncpy(device_chars, source_device_chars, 1024);
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", device_chars);

        jfieldID fingerprint_field_id = env->GetStaticFieldID(build_class, "FINGERPRINT", "Ljava/lang/String;");
        jstring fingerprint_jstring = (jstring)env->GetStaticObjectField(build_class, fingerprint_field_id);
        const char* source_fingerprint_chars = env->GetStringUTFChars(fingerprint_jstring, NULL);
        if (source_fingerprint_chars != NULL) {
            strncpy(fingerprint_chars, source_fingerprint_chars, 1024);
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", fingerprint_chars);

        jfieldID hardware_field_id = env->GetStaticFieldID(build_class, "HARDWARE", "Ljava/lang/String;");
        jstring hardware_jstring = (jstring)env->GetStaticObjectField(build_class, hardware_field_id);
        const char* source_hardware_chars = env->GetStringUTFChars(hardware_jstring, NULL);
        if (source_hardware_chars != NULL) {
            strncpy(hardware_chars, source_hardware_chars, 1024);
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", hardware_chars);

        jfieldID model_field_id = env->GetStaticFieldID(build_class, "MODEL", "Ljava/lang/String;");
        jstring model_jstring = (jstring)env->GetStaticObjectField(build_class, model_field_id);
        const char* source_model_chars = env->GetStringUTFChars(model_jstring, NULL);
        if (source_model_chars != NULL) {
            strncpy(model_chars, source_model_chars, 1024);
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", model_chars);

        jfieldID manufacturer_field_id = env->GetStaticFieldID(build_class, "MANUFACTURER", "Ljava/lang/String;");
        jstring manufacturer_jstring = (jstring)env->GetStaticObjectField(build_class, manufacturer_field_id);
        const char* source_manufacturer_chars = env->GetStringUTFChars(manufacturer_jstring, NULL);
        if (source_manufacturer_chars != NULL) {
            strncpy(manufacturer_chars, source_manufacturer_chars, 1024);
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", manufacturer_chars);

        jfieldID product_field_id = env->GetStaticFieldID(build_class, "PRODUCT", "Ljava/lang/String;");
        jstring product_jstring = (jstring)env->GetStaticObjectField(build_class, product_field_id);
        const char* source_product_chars = env->GetStringUTFChars(product_jstring, NULL);
        if (source_product_chars != NULL) {
            strncpy(product_chars, source_product_chars, 1024);
        }
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "%s", product_chars);

        if (
                (strncmp(brand_chars, "generic", strlen("generic")) == 0 && strncmp(device_chars, "generic", strlen("generic")) == 0)
                || strncmp(fingerprint_chars, "generic", strlen("generic")) == 0
                || strncmp(fingerprint_chars, "unknown", strlen("unknown")) == 0
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

    // g_files_dir

    if (g_files_dir == NULL) {

        g_files_dir = new char[2048];
        memset(g_files_dir, '\0', 2048);

        jclass context_class = env->GetObjectClass(context);
        jmethodID get_files_dir_method_id = env->GetMethodID(context_class, "getFilesDir", "()Ljava/io/File;");
        jobject files_dir_jobject = env->CallObjectMethod(context, get_files_dir_method_id);

        if (env->IsSameObject(files_dir_jobject, NULL)) {
            if (android_get_device_api_level() >= 24) {
                jmethodID get_data_dir_method_id = env->GetMethodID(context_class, "getDataDir", "()Ljava/io/File;");
                files_dir_jobject = env->CallObjectMethod(context, get_data_dir_method_id);
            } else {
                jmethodID get_application_info_method_id = env->GetMethodID(context_class, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
                jobject application_info_jobject = env->CallObjectMethod(context, get_application_info_method_id);
                jclass application_info_class = env->GetObjectClass(application_info_jobject);
                jfieldID data_dir_field = env->GetFieldID(application_info_class, "dataDir", "Ljava/lang/String;");
                jobject data_dir_jobject = env->GetObjectField(application_info_jobject, data_dir_field);

                jclass file_class = env->FindClass("java/io/File");
                jmethodID file_constructor = env->GetMethodID(file_class, "<init>", "(Ljava/lang/String;)V");

                files_dir_jobject = env->NewObject(file_class, file_constructor, data_dir_jobject);

                env->DeleteLocalRef(application_info_jobject);
                env->DeleteLocalRef(data_dir_jobject);
            }
        }

        jmethodID get_absolute_path_method_id = env->GetMethodID(env->FindClass("java/io/File"), "getAbsolutePath", "()Ljava/lang/String;");
        jstring files_dir_jstring = (jstring)env->CallObjectMethod(files_dir_jobject, get_absolute_path_method_id);

        const char* files_dir_chars = env->GetStringUTFChars(files_dir_jstring, NULL);

        strncpy(g_files_dir, files_dir_chars, 2048);

        env->ReleaseStringUTFChars(files_dir_jstring, files_dir_chars);

        env->DeleteLocalRef(files_dir_jobject);
        env->DeleteLocalRef(files_dir_jstring);
    }

    // g_asset_manager

    if (g_asset_manager == NULL) {
        jclass context_class = env->GetObjectClass(context);
        jmethodID asset_manager_id = env->GetMethodID(context_class, "getAssets", "()Landroid/content/res/AssetManager;");
        jobject asset_manager_jobject = env->CallObjectMethod(context, asset_manager_id);

        g_asset_manager = AAssetManager_fromJava(env, env->NewGlobalRef(asset_manager_jobject));

        env->DeleteLocalRef(asset_manager_jobject);
    }

    // g_media_kit_android_helper_class

    if (g_media_kit_android_helper_class == NULL) {
        g_media_kit_android_helper_class = (jclass)env->NewGlobalRef(env->FindClass("com/alexmercerind/mediakitandroidhelper/MediaKitAndroidHelper"));
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_alexmercerind_mediakitandroidhelper_MediaKitAndroidHelper_copyAssetToFilesDir(JNIEnv *env, jclass, jstring asset_name) {
    const char* asset_name_chars = env->GetStringUTFChars(asset_name, NULL);
    char result[2048];
    MediaKitAndroidHelperCopyAssetToFilesDir(asset_name_chars, result);
    env->ReleaseStringUTFChars(asset_name, asset_name_chars);
    return env->NewStringUTF(result);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_alexmercerind_mediakitandroidhelper_MediaKitAndroidHelper_openFileDescriptorNative(
        JNIEnv *env, jclass clazz, jstring uri) {
    if (g_media_kit_android_helper_class != NULL) {
        jmethodID open_file_descriptor_method_id = env->GetStaticMethodID(g_media_kit_android_helper_class, "openFileDescriptorJava", "(Ljava/lang/String;)I");
        jint file_descriptor = env->CallStaticIntMethod(g_media_kit_android_helper_class, open_file_descriptor_method_id, uri);
        return file_descriptor;
    }
    return -1;
}
