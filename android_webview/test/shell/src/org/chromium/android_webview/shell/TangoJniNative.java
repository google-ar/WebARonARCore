/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *            http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.chromium.android_webview.shell;

import com.google.atap.tangoservice.Tango;
import com.google.atap.tangoservice.Tango.TangoUpdateCallback;
import com.google.atap.tangoservice.TangoCameraMetadata;
import com.google.atap.tangoservice.TangoConfig;
import com.google.atap.tangoservice.TangoCoordinateFramePair;
import com.google.atap.tangoservice.TangoEvent;
import com.google.atap.tangoservice.TangoImage;
import com.google.atap.tangoservice.TangoPointCloudData;
import com.google.atap.tangoservice.TangoPoseData;

import android.app.Activity;
import android.os.IBinder;
import android.util.Log;

/**
 * Interfaces between C and Java.
 *
 * Note that these are the functions that call into native code, native code is
 * responsible for the communication between the application and Tango Service.
 */
public class TangoJniNative {

    private static TangoUpdateCallback mTangoUpdateCallbackProxy = new TangoUpdateCallback() {
        @Override
        public void onPoseAvailable(TangoPoseData pose)
        {
        }
        @Override
        public void onFrameAvailable(int cameraId) 
        {
            onTextureAvailable(cameraId);
        }
        @Override
        public void onTangoEvent(TangoEvent event) 
        {
           onTangoEventCallback(event);
        }
        @Override
        public void onPointCloudAvailable(TangoPointCloudData pointCloud) 
        {
          Log.d("AugmentedReality", "Point cloud arrived");
        }
        @Override
        public void onImageAvailable(TangoImage image, TangoCameraMetadata metadata, int cameraId) 
        {
            onImageAvailableCallback(image, metadata, cameraId);
        }
    };

    // public static void initialize()
    static
    {
        System.loadLibrary("tango_chromium");
        cacheJavaObjects(mTangoUpdateCallbackProxy);
    }

    public static native void cacheJavaObjects(TangoUpdateCallback callbackProxy);

    /**
     * Check if the Tango Core version is compatible with this app.
     * If not, the application will exit.
     *
     * @param callerActivity the caller activity of this function.
     */
    public static native void onCreate(
        Activity callerActivity, int activityOrientation, int sensorOrientation);

    /*
     * Called when the Tango service is connected.
     *
     * @param binder The native binder object.
     */
    public static native void onTangoServiceConnected(Tango tango);

    /**
     * Disconnect and stop Tango service.
     */
    public static native void onPause();

    public static native void onDestroy();

    public static native void onConfigurationChanged(
        int activityOrientation, int sensorOrientation);

    public static native void onTextureAvailable(int cameraId);

    public static native void onTangoEventCallback(TangoEvent event);

    public static native void onImageAvailableCallback(
        TangoImage image, TangoCameraMetadata metadata, int cameraId);

    public static native void reset();
}
