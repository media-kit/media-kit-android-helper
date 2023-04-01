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
jobject g_context = NULL;

// ------------------------------------------------------------
// Native. For access through Dart FFI.
// ------------------------------------------------------------

extern "C" __attribute__ ((visibility ("default"))) void MediaKitAndroidHelperCopyAssetToExternalFilesDir(const char* asset_name,  char* result) {
    strcpy(result, "");

    if (g_jvm == NULL) {
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "Java VM is NULL.");
        return;
    }
    if (g_context == NULL) {
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "Application context is NULL.");
        return;
    }

    JNIEnv* env;
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_6;
    args.name = NULL;
    args.group = NULL;
    g_jvm->AttachCurrentThread(&env, &args);

    jclass context_class = env->GetObjectClass(g_context);
    jmethodID asset_manager_id = env->GetMethodID(context_class, "getAssets", "()Landroid/content/res/AssetManager;");
    jobject asset_manager_jobject = env->CallObjectMethod(g_context, asset_manager_id);
    AAssetManager* asset_manager = AAssetManager_fromJava(env, asset_manager_jobject);

    jstring asset_name_jstring = env->NewStringUTF(asset_name);
    const char* asset_name_chars = env->GetStringUTFChars(asset_name_jstring, NULL);
    AAsset* asset = AAssetManager_open(asset_manager, asset_name_chars, AASSET_MODE_BUFFER);

    if (asset == NULL) {
        __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "NOT FOUND: %s", asset_name_chars);
        env->DeleteLocalRef(asset_name_jstring);
        return;
    }

    off_t length = AAsset_getLength(asset);

    auto buffer = std::make_unique<uint8_t[]>(length);

    int32_t size = AAsset_read(asset, buffer.get(), length);

    __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "Asset name: %s", asset_name_chars);
    __android_log_print(ANDROID_LOG_DEBUG, "media_kit", "Asset size: %d", size);

    AAsset_close(asset);

    jclass context_obj = env->GetObjectClass(g_context);
    jmethodID method_id = env->GetMethodID(context_obj, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
    jobject external_files_dir_jobject = env->CallObjectMethod(g_context, method_id, (jstring)NULL);
    jmethodID get_absolute_path_method_id = env->GetMethodID(env->FindClass("java/io/File"), "getAbsolutePath", "()Ljava/lang/String;");
    jstring external_files_dir_jstring = (jstring)env->CallObjectMethod(external_files_dir_jobject, get_absolute_path_method_id);
    const char* external_files_dir_chars = env->GetStringUTFChars(external_files_dir_jstring, NULL);

    std::string directory_out = external_files_dir_chars;
    directory_out += "/com.alexmercerind.media_kit/";
    std::string file_name = asset_name_chars;
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

    env->ReleaseStringUTFChars(asset_name_jstring, asset_name_chars);
    env->ReleaseStringUTFChars(external_files_dir_jstring, external_files_dir_chars);
    env->DeleteLocalRef(asset_name_jstring);
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
    JavaVM* jvm;
    env->GetJavaVM(&jvm);
    g_jvm = jvm;
    g_context = env->NewGlobalRef(context);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_alexmercerind_mediakitandroidhelper_MediaKitAndroidHelper_copyAssetToExternalFilesDir(JNIEnv *env, jclass, jstring asset_name) {
    const char* asset_name_chars = env->GetStringUTFChars(asset_name, NULL);
    char result[2048];
    MediaKitAndroidHelperCopyAssetToExternalFilesDir(asset_name_chars, result);
    env->ReleaseStringUTFChars(asset_name, asset_name_chars);
    return env->NewStringUTF(result);
}
