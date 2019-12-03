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

import android.app.Activity;
import android.content.Intent;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.widget.ToggleButton;

import java.io.File;
import java.sql.Timestamp;
import java.util.Vector;

/**
 * This starter activity allows user to setup the Area Learning configuration.
 */
public class StartActivity extends Activity {
    // The unique key string for storing user's input.
    public static final String USE_AREA_LEARNING =
            "com.projecttango.examples.cpp.helloareadescription.usearealearning";
    public static final String LOAD_ADF =
            "com.projecttango.examples.cpp.helloareadescription.loadadf";

    private static final String INTENT_CLASS_PACKAGE = "com.google.tango";
    private static final String INTENT_DEPRECATED_CLASS_PACKAGE = "com.projecttango.tango";

    public static final String INTENT_TANGO_BAG_OUTPUT_DIR="TANGO_BAG_OUTPUT_DIR";
    private final String mTangoBagOutputDir =
            Environment.getExternalStorageDirectory().getAbsolutePath()
                    + File.separator + "tango";
    // Key string for load/save Area Description Files.
    private static final String AREA_LEARNING_PERMISSION =
            "ADF_LOAD_SAVE_PERMISSION";

    private static final String DATASET_PERMISSION =
            "DATASET_PERMISSION";

    // Permission request action.
    public static final int REQUEST_CODE_TANGO_PERMISSION = 0;
    private static final String REQUEST_PERMISSION_ACTION =
            "android.intent.action.REQUEST_TANGO_PERMISSION";

    // UI elements.
    private ToggleButton mLearningModeToggleButton;
    private ToggleButton mLoadAdfToggleButton;

    private boolean mIsUseAreaLearning;
    private boolean mIsLoadAdf;

    private SensorManager mSensorManager;
    private SensorEventListener mListener;
    private long mStartTimestamp;
    private static final float NS2S = 1.0f / 1000000000.0f;

    public int accel_count = 1;
    public int gyro_count = 1;
    public long accel_interval;
    public long gyro_interval;

    public void printStats() {
        Log.i("AreaDescriptionApp", "Sample interval accel " + accel_interval/accel_count + " gyro " + gyro_interval/gyro_count);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_start);

        // Setup UI elements and listeners.
        mLearningModeToggleButton = (ToggleButton) findViewById(R.id.learningmode);
        mLoadAdfToggleButton = (ToggleButton) findViewById(R.id.loadadf);

        mIsUseAreaLearning = mLearningModeToggleButton.isChecked();
        mIsLoadAdf = mLoadAdfToggleButton.isChecked();

        if (!Util.hasPermission(getApplicationContext(), AREA_LEARNING_PERMISSION)) {
            getPermission(AREA_LEARNING_PERMISSION);
        }
        if (!Util.hasPermission(getApplicationContext(), DATASET_PERMISSION)) {
            getPermission(DATASET_PERMISSION);
        }
        mSensorManager = (SensorManager) getSystemService(getApplicationContext().SENSOR_SERVICE);

        // c.f. https://stackoverflow.com/questions/12326429/get-multiple-sensor-data-at-the-same-time-in-android
        // c.f. https://stackoverflow.com/questions/17776051/how-to-implement-gyroscope-sensor-in-android
        // c.f. http://www.41post.com/3745/programming/android-acessing-the-gyroscope-sensor-for-simple-applications
        // c.f. https://developer.android.com/guide/topics/sensors/sensors_motion.html#sensors-motion-gyro
        mListener = new SensorEventListener() {
            @Override
            public void onAccuracyChanged(Sensor arg0, int arg1) {
            }

            @Override
            public void onSensorChanged(SensorEvent event) {
                Sensor sensor = event.sensor;
                final long timestamp = event.timestamp;
                // Axis of the rotation sample, not normalized yet.
                float axisX = event.values[0];
                float axisY = event.values[1];
                float axisZ = event.values[2];

                if (sensor.getType() == Sensor.TYPE_ACCELEROMETER) {
                    ++accel_count;
                    accel_interval = timestamp - mStartTimestamp;
                    //Log.i("AreaDescriptionApp", "Accel meas " + timestamp + " " + axisX + " " + axisY + " " + axisZ);
                }
                else if (sensor.getType() == Sensor.TYPE_GYROSCOPE) {
                    ++gyro_count;
                    gyro_interval = timestamp - mStartTimestamp;
                    //Log.i("AreaDescriptionApp", "Gyro meas " + timestamp + " " + axisX + " " + axisY + " " + axisZ);
                }
                if (mStartTimestamp == 0) {
                    mStartTimestamp = timestamp;
                }
                if (accel_count % 500 == 0) {
                    printStats();
                }
            }

        };

        mSensorManager.registerListener(mListener, mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER), SensorManager.SENSOR_DELAY_FASTEST);
        mSensorManager.registerListener(mListener, mSensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE), SensorManager.SENSOR_DELAY_FASTEST);
    }

    /**
     * The "Load ADF" button has been clicked.
     * Defined in {@code activity_start.xml}
     */
    public void loadAdfClicked(View v) {
        mIsLoadAdf = mLoadAdfToggleButton.isChecked();
    }

    /**
     * The "Learning Mode" button has been clicked.
     * Defined in {@code activity_start.xml}
     * */
    public void learningModeClicked(View v) {
        mIsUseAreaLearning = mLearningModeToggleButton.isChecked();
    }

    /**
     * The "Start" button has been clicked.
     * Defined in {@code activity_start.xml}
     * */
    public void startClicked(View v) {
        startAreaDescriptionActivity();
    }

    /**
     * The "ADF List View" button has been clicked.
     * Defined in {@code activity_start.xml}
     * */
    public void adfListViewClicked(View v) {
        startAdfListView();
    }

    public void adfBagListViewClicked(View v) {
        startAdfBagListView();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        // The result of the permission activity.
        //
        // Note that when the permission activity is dismissed, the
        // AreaDescription's onResume() callback is called. As the
        // TangoService is connected in the onResume() function, we do not call
        // connect here.
        if (requestCode == REQUEST_CODE_TANGO_PERMISSION) {
            if (resultCode == RESULT_CANCELED) {
                finish();
            }
        }
    }

    // Call the permission intent for the Tango Service to ask for permissions.
    // All permission types can be found here:
    //   https://developers.google.com/project-tango/apis/c/c-user-permissions
    private void getPermission(String permissionType) {
        Intent intent = new Intent();
        intent.setPackage(INTENT_CLASS_PACKAGE);
        if (intent.resolveActivity(getApplicationContext().getPackageManager()) == null) {
            intent = new Intent();
            intent.setPackage(INTENT_DEPRECATED_CLASS_PACKAGE);
        }
        intent.setAction(REQUEST_PERMISSION_ACTION);
        intent.putExtra("PERMISSIONTYPE", permissionType);

        // After the permission activity is dismissed, we will receive a callback
        // function onActivityResult() with user's result.
        startActivityForResult(intent, REQUEST_CODE_TANGO_PERMISSION);
    }

    // Start the main area description activity and pass in user's configuration.
    private void startAreaDescriptionActivity() {
        Intent startADIntent = new Intent(this, AreaDescriptionActivity.class);
        startADIntent.putExtra(USE_AREA_LEARNING, mIsUseAreaLearning);
        startADIntent.putExtra(LOAD_ADF, mIsLoadAdf);
        startADIntent.putExtra(INTENT_TANGO_BAG_OUTPUT_DIR, mTangoBagOutputDir);
        startActivity(startADIntent);
    }

    // Start the ADF list activity.
    private void startAdfListView() {
        Intent startADFListViewIntent = new Intent(this, AdfUuidListViewActivity.class);
        startActivity(startADFListViewIntent);
    }

    private void startAdfBagListView() {
        Intent startBagListViewIntent = new Intent(this, AdfBagListViewActivity.class);
        startBagListViewIntent.putExtra(INTENT_TANGO_BAG_OUTPUT_DIR, mTangoBagOutputDir);
        startActivity(startBagListViewIntent);
    }

    @Override
    protected void onDestroy() {
        mSensorManager.unregisterListener(mListener);
        super.onDestroy();
    }

}
