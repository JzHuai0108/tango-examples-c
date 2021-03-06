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

package com.projecttango.examples.cpp.helloareadescription;

import android.graphics.Bitmap;
import android.opengl.GLSurfaceView;
import android.os.Environment;
import android.util.Log;


import java.io.File;
import java.io.FileOutputStream;
import java.text.SimpleDateFormat;
import java.util.Date;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Render onGlSurfaceDrawFrame all GL content to the glSurfaceView.
 */
public class HelloVideoRenderer implements GLSurfaceView.Renderer {
    private int img_cnt = 0;
    private long lastTimeMills = 0;
    private float frameRate = 15.f;
    // Render loop of the Gl context.
    public void onDrawFrame(GL10 gl) {
        TangoJniNative.onGlSurfaceDrawFrame();
        long presentTime = System.currentTimeMillis();

        img_cnt = img_cnt + 1;
        if (lastTimeMills != 0) {
            frameRate = frameRate * 0.3f + 0.7f * 1000.f / (presentTime - lastTimeMills);
            Log.d("HelloVideoRenderer", "image rate " + frameRate);
        }
        lastTimeMills = presentTime;
    }

    // Called when the surface size changes.
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        TangoJniNative.onGlSurfaceChanged(width, height);
    }

    // Called when the surface is created or recreated.
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        TangoJniNative.onGlSurfaceCreated();
    }
}
