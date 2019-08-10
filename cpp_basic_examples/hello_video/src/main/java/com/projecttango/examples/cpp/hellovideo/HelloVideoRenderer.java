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

package com.projecttango.examples.cpp.hellovideo;

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
    //create new java.util.Date object
    Date date = new Date();
    SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-mm-dd_hh-mm-ss");
    String strDate = dateFormat.format(date);

    private String path = Environment.getExternalStorageDirectory().getAbsolutePath()
            + File.separator + "fisheye" + File.separator + strDate;
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
//        saveFisheyeToJpg(path + File.separator + presentTime + "000000.jpg");
    }

    // Called when the surface size changes.
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        TangoJniNative.onGlSurfaceChanged(width, height);
    }

    // Called when the surface is created or recreated.
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        File file = new File(path);
        if (!file.exists()) {
            file.mkdirs();
        }
        TangoJniNative.onGlSurfaceCreated();
    }

    public void saveFisheyeToJpg(String filePath) {
        Bitmap fisheyeImage;
        fisheyeImage = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
        if(TangoJniNative.copyRgbaToBitmap(fisheyeImage)) {
            File file = new File(filePath);
            Log.i("HelloVideoActivity", "" + file);
            if (file.exists())
                file.delete();
            try {
                FileOutputStream out = new FileOutputStream(file);
                fisheyeImage.compress(Bitmap.CompressFormat.JPEG, 100, out);
                out.flush();
                out.close();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }
}
