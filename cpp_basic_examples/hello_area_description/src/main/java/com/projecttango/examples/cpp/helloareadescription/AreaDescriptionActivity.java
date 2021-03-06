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
import android.app.FragmentManager;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.Point;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.util.Log;
import android.view.Display;
import android.view.View;
import android.view.WindowManager;
import android.widget.TextView;
import android.widget.Toast;

import com.projecttango.examples.cpp.util.TangoInitializationHelper;

import java.io.File;

/**
 * This activity is called after the user selects the area description configuration in the
 * {@code StartActivity}.
 * It will start the Tango with the selected configuration and display some debugging information
 * on the screen to illustrate the running state of the system.
 * <p/>
 * This activity triggers functions in the C++ layer to initialize and configure the Tango using
 * the area description configuration chosen by the user and handles the basic lifecycle of the
 * activity while the Tango is running.
 */
public class AreaDescriptionActivity extends Activity implements
      SetAdfNameDialog.CallbackListener, SaveAdfTask.SaveAdfListener {
  private static final String TAG = AreaDescriptionActivity.class.getSimpleName();
  private String mTangoBagOutputDir;
  private String mImuResultFile;
  private String mMetadataFile;
  private GLSurfaceView mSurfaceView;
  private static IMUManager mImuManager;

  // The interval at which we'll update our UI debug text in milliseconds.
  // This is the rate at which we query our native wrapper around the tango
  // service for pose and event information.
  private static final int UPDATE_UI_INTERVAL_MS = 1000;

  private TextView mAdfUuidTextView;
  private TextView mRelocalizationTextView;

  // Flag that controls whether user wants to run area learning mode.
  private boolean mIsAreaLearningEnabled = false;

  // Flag that controls whether user wants to load the latest area description.
  private boolean mIsLoadingAreaDescription = false;

  // Screen size for normalizing the touch input for orbiting the render camera.
  private Point mScreenSize = new Point();

  // Long-running task to save an ADF.
  private SaveAdfTask mSaveAdfTask;

  // Handles the debug text UI update loop.
  private Handler mHandler = new Handler();

  // Tango Service connection.
  ServiceConnection mTangoServiceConnection = new ServiceConnection() {
      public void onServiceConnected(ComponentName name, IBinder service) {
        TangoJniNative.onTangoServiceConnected(service, mIsAreaLearningEnabled,
                                               mIsLoadingAreaDescription);
        if (mIsAreaLearningEnabled) {
          SimpleDateLogger.logLocalTime(mMetadataFile);
          mImuManager.startRecording(mImuResultFile);
        }
        // Display loaded ADF's UUID.
        runOnUiThread(new Runnable() {
          @Override
          public void run() {
            mAdfUuidTextView.setText(TangoJniNative.getLoadedAdfUuidString());
          }
        });
      }

      public void onServiceDisconnected(ComponentName name) {
        // Handle this if you need to gracefully shutdown/retry
        // in the event that Tango itself crashes/gets upgraded while running.
      }
    };

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    queryDataFromStartActivity();
    setupUiComponents();
    TangoJniNative.onCreate(this);
    mImuManager = new IMUManager(this);
  }

  @Override
  protected void onResume() {
    super.onResume();
    Log.d(TAG, "Keeping screen on for previewing recording.");
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    mSurfaceView.onResume();
    // Start the debug text UI update loop.
    mHandler.post(mUpdateUiLoopRunnable);

    TangoInitializationHelper.bindTangoService(this, mTangoServiceConnection);
    mImuManager.register();
  }

  @Override
  protected void onPause() {
    super.onPause();
    mSurfaceView.onPause();
    TangoJniNative.onPause();
    unbindService(mTangoServiceConnection);
    // Stop the debug text UI update loop.
    mHandler.removeCallbacksAndMessages(null);

    mImuManager.unregister();
  }

  @Override
  protected void onDestroy(){
    super.onDestroy();
    TangoJniNative.onDestroy();
  }

  /**
   * The "Save ADF" button has been clicked.
   * Defined in {@code activity_area_description.xml}
   */
  public void saveAdfClicked(View view) {
    showSetAdfNameDialog();
  }

  /**
   * Implementation of callback listener interface in SetADFNameDialog.
   */
  @Override
  public void onAdfNameOk(String adfName, String adfUuid) {
    saveAdf(adfName);
  }

  /**
   * Implementation of callback listener interface in SetADFNameDialog.
   */
  @Override
  public void onAdfNameCancelled() {
    // Continue running.
  }

  /**
   * Save the current Area Description File.
   * Performs saving on a background thread and displays a progress dialog.
   */
  private void saveAdf(String adfName) {
    mSaveAdfTask = new SaveAdfTask(this, this, adfName);
    mSaveAdfTask.execute();
  }

  private void postMortemSaveAdf(String toastMessage, String adf_dataset_uuid) {
    if (mIsAreaLearningEnabled) {
      mImuManager.stopRecording();
      // TODO(jhuai): Usually, a ADF session is saved to a dir with the dataset
      // UUID as the basename. But on some lenovo Phab2 device, the dir's
      // basename may be a date_time string like 20200811_194553.
      // In this case, the below approach puts the inertial data to a dir
      // different from the session dir. Ideally, we should be able to set
      // or get the dir of the saved session, but scarce info of the
      // Tango C/Java client API prevents me from doing so.
      // As a result, the user have to associate these files by comparing
      // dir creation times.

      // move the inertial file to the export path regardless of
      // the resulting state of saveAdf
      renameInertialDataFile(
              adf_dataset_uuid.substring(
                      SaveAdfTask.uuid_len, SaveAdfTask.uuid_len * 2));
    }
    Toast.makeText(this, toastMessage, Toast.LENGTH_LONG).show();
    mSaveAdfTask = null;
    finish();
  }
  /**
   * Handles result from mSaveAdfTask.
   *   Note this happens before onPause, so moving gyro_accel.csv which creates
   *   necessary folders precedes moving W_T_B.csv.
   */
  @Override
  public void onSaveAdfFinished(String adfName, String adf_dataset_uuid) {
    String toastMessage;
    if (adf_dataset_uuid.startsWith(SaveAdfTask.uuid_mask)) {
      toastMessage =
              String.format(getResources().getString(R.string.save_adf_failed_toast_format),
                      adfName);
    } else {
      toastMessage =
              String.format(getResources().getString(R.string.save_adf_success_toast_format),
                      adfName, adf_dataset_uuid.substring(0, SaveAdfTask.uuid_len));

    }
    postMortemSaveAdf(toastMessage, adf_dataset_uuid);
  }

  /**
   * Updates the save progress dialog (called from hello_area_description_app.cc).
   */
  public void updateSavingAdfProgress(int progress) {
    // Note: this method is not called from the UI thread. We read mSaveAdfTask into
    // a local variable because the UI thread may null-out the member variable at any time.
    SaveAdfTask saveAdfTask = mSaveAdfTask;
    if (saveAdfTask != null) {
      saveAdfTask.publishProgress(progress);
    }
  }

  /**
   * Query user's input for the Tango Service configuration.
   */
  private void queryDataFromStartActivity() {
    // Get user's input from the StartActivity.
    Intent initValueIntent = getIntent();
    mTangoBagOutputDir = initValueIntent.getStringExtra(StartActivity.INTENT_TANGO_BAG_OUTPUT_DIR);
    mImuResultFile = mTangoBagOutputDir + File.separator + "gyro_accel.csv";
    mMetadataFile = mTangoBagOutputDir + File.separator + "session.yaml";
    mIsAreaLearningEnabled =
      initValueIntent.getBooleanExtra(StartActivity.USE_AREA_LEARNING, false);
    mIsLoadingAreaDescription =
      initValueIntent.getBooleanExtra(StartActivity.LOAD_ADF, false);
  }

  /**
   * Setup UI components in the current activity.
   */
  private void setupUiComponents() {
    setContentView(R.layout.activity_area_description);

    // Querying screen size, used for computing the normalized touch point.
    Display display = getWindowManager().getDefaultDisplay();
    display.getSize(mScreenSize);

    // Configure OpenGL renderer
    mSurfaceView = (GLSurfaceView) findViewById(R.id.surfaceview);
    mSurfaceView.setEGLContextClientVersion(2);
    mSurfaceView.setRenderer(new HelloVideoRenderer());

    mAdfUuidTextView = (TextView) findViewById(R.id.adf_uuid_textview);
    mRelocalizationTextView = (TextView) findViewById(R.id.relocalization_textview);

    if (mIsAreaLearningEnabled) {
      // Disable save ADF button until Tango relocalizes to the current ADF.
      findViewById(R.id.save_adf_button).setEnabled(false);
    } else {
      // Hide to save ADF button if leanring mode is off.
      findViewById(R.id.save_adf_button).setVisibility(View.GONE);
    }
  }

  private void showSetAdfNameDialog() {
    Bundle bundle = new Bundle();
    bundle.putString("name", getResources().getString(R.string.default_adf_name));
    bundle.putString("id", ""); // UUID is generated after the ADF is saved.

    FragmentManager manager = getFragmentManager();
    SetAdfNameDialog setAdfNameDialog = new SetAdfNameDialog();
    setAdfNameDialog.setArguments(bundle);
    setAdfNameDialog.show(manager, "AdfNameDialog");
  }

  /**
   * Debug text UI update loop, updating at 10Hz.
   */
  private Runnable mUpdateUiLoopRunnable = new Runnable() {
      public void run() {
        updateUi();
        mHandler.postDelayed(this, UPDATE_UI_INTERVAL_MS);
      }
    };

  /**
   * Update the debug text UI.
   */
  private void updateUi() {
    try {
      mRelocalizationTextView.setText(TangoJniNative.isRelocalized() ?
                                      "Relocalized" : "Not Relocalized");

      if (TangoJniNative.isRelocalized()) {
        findViewById(R.id.save_adf_button).setEnabled(true);
      }
    } catch (Exception e) {
      Log.e(TAG, "Exception updating UI", e);
    }
  }

  private void renameInertialDataFile(String adfUuid) {
    File file = new File(mImuResultFile);
    // renaming the file and moving it to a new location
    String destDir = mTangoBagOutputDir + File.separator + adfUuid +
        File.separator + "export";
    String dest = destDir + File.separator + "gyro_accel.csv";

    File folder = new File(destDir);
    if (!folder.exists()) {
      folder.mkdirs();
    }
    if(file.renameTo(new File(dest))) {
      // if file copied successfully then delete the original file
      file.delete();
    } else {
      Log.e(TAG, "Failed to move inertial data to " + dest);
    }
    File dateFile = new File(mMetadataFile);
    String dateDest = destDir + File.separator + "session.yaml";
    if(dateFile.renameTo(new File(dateDest))) {
      dateFile.delete();
    } else {
      Log.e(TAG, "Failed to move session metadata to " + dateDest);
    }
  }
}
