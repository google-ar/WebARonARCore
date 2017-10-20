/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "TangoHandler.h"

using namespace tango_chromium;

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_TangoJniNative_cacheJavaObjects(
    JNIEnv* env, jobject, jobject jTangoUpdateCallback) {
  LOGE("Java_org_chromium_android_1webview_shell_TangoJniNative_cacheJavaObjects 1");
  TangoService_CacheJavaObjects(env, jTangoUpdateCallback);
  LOGE("Java_org_chromium_android_1webview_shell_TangoJniNative_cacheJavaObjects 2");
}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_TangoJniNative_onCreate(JNIEnv* env, jobject obj, jobject caller_activity, jint activityOrientation, jint sensorOrientation) 
{
	TangoHandler::getInstance()->onCreate(env, caller_activity, activityOrientation, sensorOrientation);
}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_TangoJniNative_onDestroy(JNIEnv* env, jobject obj) 
{
	TangoHandler::releaseInstance();
}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_TangoJniNative_onTangoServiceConnected(JNIEnv* env, jobject, jobject tango) 
{
	TangoHandler::getInstance()->onTangoServiceConnected(env, tango);
}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_TangoJniNative_onPause(JNIEnv*, jobject) 
{
	TangoHandler::getInstance()->onPause();
}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_TangoJniNative_onConfigurationChanged(JNIEnv*, jobject, int activityOrientation, int sensorOrientation) 
{
	TangoHandler::getInstance()->onDeviceRotationChanged(activityOrientation, sensorOrientation);
}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_TangoJniNative_onTextureAvailable(
    JNIEnv* env, jobject, jint cameraId) {
  TangoService_JavaCallback_OnTextureAvailable(static_cast<int>(cameraId));
}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_TangoJniNative_onImageAvailableCallback(
    JNIEnv* env, jobject, jobject image, jobject metadata, jint cameraId) {
  TangoService_JavaCallback_OnImageAvailable(env, static_cast<int>(cameraId),
                                             image, metadata);
}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_TangoJniNative_onTangoEventCallback(
    JNIEnv* env, jobject, jobject event) {
  TangoService_JavaCallback_OnTangoEvent(env, event);
}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_TangoJniNative_reset(
    JNIEnv* env, jobject, jobject event) {
  TangoHandler::getInstance()->reset();
}

#ifdef __cplusplus
}
#endif
