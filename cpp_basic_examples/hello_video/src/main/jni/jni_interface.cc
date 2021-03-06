/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define GLM_FORCE_RADIANS

#include <jni.h>
#include <android/bitmap.h>
#include <hello_video/hello_video_app.h>

static hello_video::HelloVideoApp app;

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_com_projecttango_examples_cpp_hellovideo_TangoJniNative_onCreate(
    JNIEnv* env, jobject, jobject activity) {
  app.OnCreate(env, activity);
}

JNIEXPORT void JNICALL
Java_com_projecttango_examples_cpp_hellovideo_TangoJniNative_onTangoServiceConnected(
    JNIEnv* env, jobject, jobject binder) {
  app.OnTangoServiceConnected(env, binder);
}

JNIEXPORT void JNICALL
Java_com_projecttango_examples_cpp_hellovideo_TangoJniNative_onPause(
    JNIEnv*, jobject) {
  app.OnPause();
}

JNIEXPORT void JNICALL
Java_com_projecttango_examples_cpp_hellovideo_TangoJniNative_onGlSurfaceCreated(
    JNIEnv*, jobject) {
  app.OnSurfaceCreated();
}

JNIEXPORT void JNICALL
Java_com_projecttango_examples_cpp_hellovideo_TangoJniNative_onGlSurfaceChanged(
    JNIEnv*, jobject, jint width, jint height) {
  app.OnSurfaceChanged(width, height);
}

JNIEXPORT void JNICALL
Java_com_projecttango_examples_cpp_hellovideo_TangoJniNative_onGlSurfaceDrawFrame(
    JNIEnv*, jobject) {
  app.OnDrawFrame();
}

JNIEXPORT void JNICALL
Java_com_projecttango_examples_cpp_hellovideo_TangoJniNative_setYuvMethod(
    JNIEnv*, jobject, jboolean use_yuv_method) {
  if (use_yuv_method) {
    app.SetTextureMethod(hello_video::HelloVideoApp::TextureMethod::kYuv);
  } else {
    app.SetTextureMethod(hello_video::HelloVideoApp::TextureMethod::kTextureId);
  }
}

JNIEXPORT void JNICALL
Java_com_projecttango_examples_cpp_hellovideo_TangoJniNative_onDisplayChanged(
    JNIEnv* /*env*/, jobject /*obj*/, jint display_rotation) {
  app.OnDisplayChanged(display_rotation);
}


JNIEXPORT bool JNICALL
Java_com_projecttango_examples_cpp_hellovideo_TangoJniNative_copyRgbaToBitmap(
        JNIEnv *env, jobject obj, jobject bitmap)
{
  AndroidBitmapInfo  info;
  void*              pixels;
  int ret = 0;

  if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
    return false;
  }
  if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
    return false;
  }

  while ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
  }

  bool is_success = app.copyRgbaToOutputBuffer(pixels);

  AndroidBitmap_unlockPixels(env, bitmap);
  return is_success;
}

#ifdef __cplusplus
}
#endif
