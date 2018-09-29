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

import android.app.Activity;
import android.content.ComponentName;
import android.content.ServiceConnection;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.hardware.display.DisplayManager;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Environment;
import android.os.IBinder;
import android.util.Log;
import android.view.Display;
import android.view.View;
import android.widget.Button;
import android.widget.ToggleButton;

import com.projecttango.examples.cpp.util.TangoInitializationHelper;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Random;

/**
 * Main activity shows video overlay scene.
 */
public class HelloVideoActivity extends Activity {
    private GLSurfaceView mSurfaceView;
    private ToggleButton mYuvRenderSwitcher;
    private Button mSaveFisheyeButton;
    private String TAG = "HelloVideoActivity";

    private ServiceConnection mTangoServiceCoonnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder binder) {
            TangoJniNative.onTangoServiceConnected(binder);
            setDisplayRotation();
        }

        @Override
        public void onServiceDisconnected(ComponentName componentName) {
            // Handle this if you need to gracefully shutdown/retry
            // in the event that Tango itself crashes/gets upgraded while running.
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        TangoJniNative.onCreate(this);

        // Register for display orientation change updates.
        DisplayManager displayManager = (DisplayManager) getSystemService(DISPLAY_SERVICE);
        if (displayManager != null) {
            displayManager.registerDisplayListener(new DisplayManager.DisplayListener() {
                @Override
                public void onDisplayAdded(int displayId) {
                }

                @Override
                public void onDisplayChanged(int displayId) {
                    synchronized (this) {
                        setDisplayRotation();
                    }
                }

                @Override
                public void onDisplayRemoved(int displayId) {
                }
            }, null);
        }

        // Configure OpenGL renderer
        mSurfaceView = (GLSurfaceView) findViewById(R.id.surfaceview);
        mSurfaceView.setEGLContextClientVersion(2);
        mSurfaceView.setRenderer(new HelloVideoRenderer());

        mYuvRenderSwitcher = (ToggleButton) findViewById(R.id.yuv_switcher);
        mSaveFisheyeButton = (Button) findViewById(R.id.save_fisheye);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mSurfaceView.onResume();
        TangoInitializationHelper.bindTangoService(this, mTangoServiceCoonnection);
        TangoJniNative.setYuvMethod(mYuvRenderSwitcher.isChecked());
    }

    @Override
    protected void onPause() {
        super.onPause();
        mSurfaceView.onPause();
        TangoJniNative.onPause();
        unbindService(mTangoServiceCoonnection);
    }

    /**
     * The render mode toggle button was pressed.
     */
    public void renderModeClicked(View view) {
        TangoJniNative.setYuvMethod(mYuvRenderSwitcher.isChecked());
    }

    public void saveFisheye(View view) {
        Bitmap fisheyeImage;
        fisheyeImage = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
        TangoJniNative.onSaveFisheye(fisheyeImage);
        String root = Environment.getExternalStorageDirectory().toString();
        File myDir = new File(root + "/Pictures");
        myDir.mkdirs();
        Random generator = new Random();
        int n = 1000000;
        n = generator.nextInt(n);
        String fname = "Image-" + n + ".jpg";
        File file = new File(myDir, fname);
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

    /**
     * Pass device rotation to native layer. This parameter is important for Tango to render video
     * overlay in the correct device orientation.
     */
    private void setDisplayRotation() {
        Display display = getWindowManager().getDefaultDisplay();
        TangoJniNative.onDisplayChanged(display.getRotation());
    }
}