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

#include <cstdlib>
#include <sstream>
#include <dirent.h>
#include <string.h>

#include <sys/system_properties.h>
#include <tango_support.h>
#include <tango_3d_reconstruction_api.h>
#include <hello_area_description/mini_timer.h>

#include "hello_area_description/hello_area_description_app.h"

namespace {
// The minimum Tango Core version required from this application.
constexpr int kTangoCoreMinimumVersion = 9377;

int getAndroidSdkVersion() {
  char sdk_ver_str[1024];
  int sdk_ver = -1;
  if (__system_property_get("ro.build.version.sdk", sdk_ver_str)) {
    sdk_ver = atoi(sdk_ver_str);
  } // else
  // Not running on Android or SDK version is not available
  return sdk_ver;
}

const int kVersionStringLength = 128;
void OnFrameAvailableRouter(void* context, TangoCameraId,
                            const TangoImageBuffer* buffer) {
    hello_area_description::AreaLearningApp* app =
            static_cast<hello_area_description::AreaLearningApp*>(context);
    app->OnFrameAvailable(buffer);
}

// We could do this conversion in a fragment shader if all we care about is
// rendering, but we show it here as an example of how people can use RGB data
// on the CPU.
inline void Yuv2Rgb(uint8_t y_value, uint8_t u_value, uint8_t v_value,
                    uint8_t* r, uint8_t* g, uint8_t* b) {
  float float_r = y_value + (1.370705 * (v_value - 128));
  float float_g =
          y_value - (0.698001 * (v_value - 128)) - (0.337633 * (u_value - 128));
  float float_b = y_value + (1.732446 * (u_value - 128));

  float_r = float_r * !(float_r < 0);
  float_g = float_g * !(float_g < 0);
  float_b = float_b * !(float_b < 0);

  *r = float_r * (!(float_r > 255)) + 255 * (float_r > 255);
  *g = float_g * (!(float_g > 255)) + 255 * (float_g > 255);
  *b = float_b * (!(float_b > 255)) + 255 * (float_b > 255);
}


// This function routes onPoseAvailable callbacks to the application object for
// handling.
//
// @param context, context will be a pointer to a AreaLearningApp
//        instance on which to call callbacks.
// @param pose, pose data to route to onPoseAvailable function.
void onPoseAvailableRouter(void* context, const TangoPoseData* pose) {
  hello_area_description::AreaLearningApp* app =
      static_cast<hello_area_description::AreaLearningApp*>(context);
  app->onPoseAvailable(pose);
}
}  // namespace

namespace hello_area_description {
void AreaLearningApp::onPoseAvailable(const TangoPoseData* pose) {
  std::lock_guard<std::mutex> lock(pose_mutex_);
  pose_data_.UpdatePose(*pose);
}

AreaLearningApp::AreaLearningApp()
    : tango_core_version_string_("N/A"),
      loaded_adf_string_("Loaded ADF: N/A"),
      calling_activity_obj_(nullptr),
      on_saving_adf_progress_updated_(nullptr) {}

AreaLearningApp::~AreaLearningApp() { TangoConfig_free(tango_config_); }

void AreaLearningApp::OnCreate(JNIEnv* env, jobject caller_activity) {
  // Check the installed version of the TangoCore. If it is too old, then
  // it will not support the most up to date features.
  int version = 0;
  TangoErrorType err =
      TangoSupport_getTangoVersion(env, caller_activity, &version);
  if (err != TANGO_SUCCESS || version < kTangoCoreMinimumVersion) {
    LOGE(
        "AreaLearningApp::OnCreate, Tango Core version is out"
        " of date.");
    std::exit(EXIT_SUCCESS);
  }

  jclass cls = env->GetObjectClass(caller_activity);
  on_saving_adf_progress_updated_ =
      env->GetMethodID(cls, "updateSavingAdfProgress", "(I)V");

  calling_activity_obj_ = env->NewGlobalRef(caller_activity);

  int sdk_ver = getAndroidSdkVersion();
  LOGI("AreaLearningApp::Android SDK Version %d", sdk_ver);
  if (sdk_ver < 24) {
    camera_id_ = TANGO_CAMERA_FISHEYE;
  } else {
    LOGI("AreaLearningApp::Android API Level is 24 or more, "
         "Fisheye camera data is not available, "
         "Color camera will be used instead");
    camera_id_ = TANGO_CAMERA_COLOR;
  }

  // Initialize variables
  is_yuv_texture_available_ = false;
  swap_buffer_signal_ = false;
  is_service_connected_ = false;
  is_texture_id_set_ = false;
  video_overlay_drawable_ = NULL;
  yuv_drawable_ = NULL;
  is_video_overlay_rotation_set_ = false;
  adf_uuid_string_ = "";
  dataset_uuid_string_ = "";
}

void AreaLearningApp::OnTangoServiceConnected(
    JNIEnv* env, jobject binder, bool is_area_learning_enabled,
    bool is_loading_area_description) {
  // Associate the service binder to the Tango service.
  if (TangoService_setBinder(env, binder) != TANGO_SUCCESS) {
    LOGE(
        "AreaLearningApp::OnTangoServiceConnected,"
        "TangoService_setBinder error");
    std::exit(EXIT_SUCCESS);
  }

  if (is_area_learning_enabled || is_loading_area_description) {
    // Here, we'll configure the service to run in the way we'd want. For this
    // application, we'll start from the default configuration
    // (TANGO_CONFIG_DEFAULT). This enables basic motion tracking capabilities.
    tango_config_ = TangoService_getConfig(TANGO_CONFIG_DEFAULT);
    if (tango_config_ == nullptr) {
      LOGE("AreaLearningApp: Failed to get default config form");
      std::exit(EXIT_SUCCESS);
    }

    int ret = TangoConfig_setBool(tango_config_, "config_enable_learning_mode",
                                  is_area_learning_enabled);
    if (ret != TANGO_SUCCESS) {
      LOGE(
          "AreaLearningApp: config_enable_learning_mode failed with error"
          "code: %d",
          ret);
      std::exit(EXIT_SUCCESS);
    }

    // If load ADF, load the most recent saved ADF.
    if (is_loading_area_description) {
      std::vector<std::string> adf_list;
      GetAdfUuids(&adf_list);
      if (!adf_list.empty()) {
        std::string adf_uuid = adf_list.back();
        std::ostringstream adf_str_stream;
        adf_str_stream << "Number of ADFs:" << adf_list.size()
                       << ", Loaded ADF: " << adf_uuid;
        loaded_adf_string_ = adf_str_stream.str();
        ret = TangoConfig_setString(tango_config_,
                                    "config_load_area_description_UUID",
                                    adf_uuid.c_str());
        if (ret != TANGO_SUCCESS) {
          LOGE("AreaLearningApp: get ADF UUID failed with error code: %d", ret);
        }
      }
    }
    // Enable Dataset Recording.
    TangoErrorType err =
        TangoConfig_setBool(tango_config_, "config_enable_dataset_recording", true);

    if (err != TANGO_SUCCESS) {
      LOGE("AreaLearningApp: config_enable_dataset_recording() failed with error code: %d.", err);
      std::exit(EXIT_SUCCESS);
    }

    err = TangoConfig_setInt32(tango_config_, "config_dataset_recording_mode",
        //TANGO_RECORDING_MODE_MOTION_TRACKING);
        //TANGO_RECORDING_MODE_MOTION_TRACKING_AND_FISHEYE);
       // TANGO_RECORDING_MODE_SCENE_RECONSTRUCTION);
        TANGO_RECORDING_MODE_ALL); // all is the same as motion_tracking_and_fisheye

    if (err != TANGO_SUCCESS) {
      LOGE("AreaLearningApp: config_dataset_recording_mode() failed with error code: %d.", err);
      std::exit(EXIT_SUCCESS);
    } else {
      LOGE("AreaLearningApp: config_dataset_recording_mode() succeeded with code: %d.", err);
    }

    err = TangoConfig_setString(tango_config_, "config_datasets_path", kOutputDir.c_str());
    if (err != TANGO_SUCCESS) {
          LOGE("AreaLearningApp: config_datasets_path() failed with error code: %d.", err);
          std::exit(EXIT_SUCCESS);
    } else {
          LOGE("AreaLearningApp: config_datasets_path() succeeded with code: %d.", err);
    }

    // Setting up the frame pair for the onPoseAvailable callback.
    TangoCoordinateFramePair pairs[3] = {
        {TANGO_COORDINATE_FRAME_START_OF_SERVICE,
         TANGO_COORDINATE_FRAME_DEVICE},
        {TANGO_COORDINATE_FRAME_AREA_DESCRIPTION,
         TANGO_COORDINATE_FRAME_DEVICE},
        {TANGO_COORDINATE_FRAME_AREA_DESCRIPTION,
         TANGO_COORDINATE_FRAME_START_OF_SERVICE}};

    // Attach onPoseAvailable callback.
    // The callback will be called after the service is connected.
    ret = TangoService_connectOnPoseAvailable(3, pairs, onPoseAvailableRouter);
    if (ret != TANGO_SUCCESS) {
      LOGE(
          "AreaLearningApp: Failed to connect to pose callback with error"
          "code: %d",
          ret);
      std::exit(EXIT_SUCCESS);
    }
    ret = TangoService_connectOnFrameAvailable(camera_id_, this,
                                               OnFrameAvailableRouter);
    if (ret != TANGO_SUCCESS) {
        LOGE("AreaLearningApp::OnTangoServiceConnected, "
             "Error connecting camera %d with ret code %d",
             camera_id_, ret);
        // std::exit(EXIT_SUCCESS);
    }
    // Connect to the Tango Service, the service will start running:
    // point clouds can be queried and callbacks will be called.
    ret = TangoService_connect(this, tango_config_);
    if (ret != TANGO_SUCCESS) {
      LOGE(
          "AreaLearningApp::OnTangoServiceConnected,"
          "Failed to connect to the Tango service with error code: %d",
          ret);
      std::exit(EXIT_SUCCESS);
    }
    // Initialize TangoSupport context.
    TangoSupport_initialize(TangoService_getPoseAtTime,
                            TangoService_getCameraIntrinsics);

    is_service_connected_ = true;
    pose_data_.StartRecordingOdometry(kOutputDir + "/" + kPoseCsv);
  }
}

void AreaLearningApp::OnDestroy() {
  JNIEnv* env;
  java_vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
  env->DeleteGlobalRef(calling_activity_obj_);
  calling_activity_obj_ = nullptr;
  on_saving_adf_progress_updated_ = nullptr;
}

void AreaLearningApp::OnPause() {
  // Delete resources.
  pose_data_.ResetPoseData(
      kOutputDir + "/" + dataset_uuid_string_ +
          "/" + kExportBasename + "/" + kPoseCsv);
  // When disconnecting from the Tango Service, it is important to make sure to
  // free your configuration object. Note that disconnecting from the service,
  // resets all configuration, and disconnects all callbacks. If an application
  // resumes after disconnecting, it must re-register configuration and
  // callbacks with the service.
  TangoConfig_free(tango_config_);
  tango_config_ = nullptr;
  TangoService_disconnect();


  // Free buffer data
  is_yuv_texture_available_ = false;
  swap_buffer_signal_ = false;
  is_service_connected_ = false;
  is_video_overlay_rotation_set_ = false;
  is_texture_id_set_ = false;
  rgb_buffer_.clear();
  yuv_buffer_.clear();
  yuv_temp_buffer_.clear();
  this->DeleteDrawables();
  adf_uuid_string_ = "";
  dataset_uuid_string_ = "";
}

void DummyProgressCallback(int progress, void* callback_param) {
  LOGI("AreaLearningApp: Export dataset progress %d", progress);
}

void AreaLearningApp::ExportBagToRawFiles(std::string dataSessionPath) {
  timing::Timer saveAdfTime;
  saveAdfTime.tic();
  int callback_param = 0;
  std::string output_path = dataSessionPath + "/" + kExportBasename;
  LOGI("AreaLearningApp: %s dirToExport %s", dataSessionPath.c_str(), output_path.c_str());
  Tango3DR_Status status = Tango3DR_extractRawDataFromDataset(
          dataSessionPath.c_str(), output_path.c_str(),
          &DummyProgressCallback, &callback_param);

  if (status != TANGO_3DR_SUCCESS) {
      LOGE("AreaLearningApp: Exporting dataset failed with error code: %d", status);
      //  std::exit(EXIT_SUCCESS);
  } else {
      LOGI("AreaLearningApp: Exporting dataset succeeded to %s", output_path.c_str());
  }

//  Tango3dReconstructionAreaDescription areaDescription =
//      Tango3dReconstructionAreaDescription.createFromDataset(dataset, null, null);
  double elapsed = saveAdfTime.toc();
  LOGI("AreaLearningApp: Exporting tango raw data takes %.7f s", elapsed);
}


std::string AreaLearningApp::SaveAdf() {
  std::string adf_uuid_string;
//  if (!pose_data_.IsRelocalized()) {
//    return adf_uuid_string;
//  }
  timing::Timer saveAdfTime;
  TangoUUID uuid;
  int ret = TangoService_saveAreaDescription(&uuid);
  if (ret == TANGO_SUCCESS) {
    LOGI("AreaLearningApp: Successfully saved ADF with UUID: %s", uuid);
    adf_uuid_string = std::string(uuid);
  } else {
    // Note: uuid is set to a nullptr in this case, so don't always try to
    // construct a string from it!
    LOGE("AreaLearningApp: Failed to save ADF with error code: %d", ret);
  }
  double elapsed = saveAdfTime.toc();
  LOGI("AreaLearningApp: Saving ADF takes %.7f s", elapsed);

  TangoUUID uuid_dataset;
  TangoService_Experimental_getCurrentDatasetUUID(&uuid_dataset);

  std::string dataset_uuid_string = std::string(uuid_dataset);

  adf_uuid_string_ = adf_uuid_string;
  dataset_uuid_string_ = dataset_uuid_string;
  if (adf_uuid_string.empty()) {
    adf_uuid_string = std::string(36, '0');
  }
  // dataset_uuid_string.empty() should not happen as it is a dir for saving data
  return adf_uuid_string + dataset_uuid_string;
}

std::string AreaLearningApp::GetAdfMetadataValue(const std::string& uuid,
                                                 const std::string& key) {
  size_t size = 0;
  char* output;
  TangoAreaDescriptionMetadata metadata;

  // Get the metadata object from the Tango Service.
  int ret = TangoService_getAreaDescriptionMetadata(uuid.c_str(), &metadata);
  if (ret != TANGO_SUCCESS) {
    LOGE("AreaLearningApp: Failed to get ADF metadata with error code: %d",
         ret);
  }

  // Query specific key-value from the metadata object.
  ret = TangoAreaDescriptionMetadata_get(metadata, key.c_str(), &size, &output);
  if (ret != TANGO_SUCCESS) {
    LOGE(
        "AreaLearningApp: Failed to get ADF metadata value with error "
        "code: %d",
        ret);
  }
  return std::string(output);
}

void AreaLearningApp::SetAdfMetadataValue(const std::string& uuid,
                                          const std::string& key,
                                          const std::string& value) {
  TangoAreaDescriptionMetadata metadata;
  int ret = TangoService_getAreaDescriptionMetadata(uuid.c_str(), &metadata);
  if (ret != TANGO_SUCCESS) {
    LOGE("AreaLearningApp: Failed to get ADF metadata with error code: %d",
         ret);
  }
  ret = TangoAreaDescriptionMetadata_set(metadata, key.c_str(), value.size(),
                                         value.c_str());
  if (ret != TANGO_SUCCESS) {
    LOGE("AreaLearningApp: Failed to set ADF metadata with error code: %d",
         ret);
  }
  ret = TangoService_saveAreaDescriptionMetadata(uuid.c_str(), metadata);
  if (ret != TANGO_SUCCESS) {
    LOGE("AreaLearningApp: Failed to save ADF metadata with error code: %d",
         ret);
  }
}

std::string AreaLearningApp::GetAllAdfUuids() {
  TangoConfig tango_config_ = TangoService_getConfig(TANGO_CONFIG_DEFAULT);
  if (tango_config_ == nullptr) {
    LOGE("AreaLearningApp: Failed to get default config form");
  }

  // Get all ADF's uuids.
  char* uuid_list = nullptr;
  int ret = TangoService_getAreaDescriptionUUIDList(&uuid_list);
  // uuid_list will contain a comma separated list of UUIDs.
  if (ret != TANGO_SUCCESS) {
    LOGE("AreaLearningApp: get ADF UUID failed with error code: %d", ret);
  }
  return std::string(uuid_list);
}

void AreaLearningApp::DeleteAdf(std::string uuid) {
  TangoService_deleteAreaDescription(uuid.c_str());
}

bool AreaLearningApp::IsRelocalized() {
  std::lock_guard<std::mutex> lock(pose_mutex_);
  return pose_data_.IsRelocalized();
}

void AreaLearningApp::GetAdfUuids(std::vector<std::string>* adf_list) {
  // Get all ADF's uuids.
  char* uuid_list = nullptr;
  int ret = TangoService_getAreaDescriptionUUIDList(&uuid_list);
  // uuid_list will contain a comma separated list of UUIDs.
  if (ret != TANGO_SUCCESS) {
    LOGE("AreaLearningApp: get ADF UUID failed with error code: %d", ret);
  }

  // Parse the uuid_list to get the individual uuids.
  if (uuid_list != nullptr) {
    if (uuid_list[0] != '\0') {
      char* parsing_char;
      char* saved_ptr;
      parsing_char = strtok_r(uuid_list, ",", &saved_ptr);
      while (parsing_char != nullptr) {
        std::string str = std::string(parsing_char);
        adf_list->push_back(str);
        parsing_char = strtok_r(nullptr, ",", &saved_ptr);
      }
    }
  }
}

void AreaLearningApp::OnAdfSavingProgressChanged(int progress) {
  // Here, we notify the Java activity that we'd like it to update the saving
  // Adf progress.
  if (calling_activity_obj_ == nullptr ||
      on_saving_adf_progress_updated_ == nullptr) {
    LOGE(
        "AreaLearningApp: Cannot reference activity on ADF saving progress"
        "changed.");
    return;
  }

  JNIEnv* env;
  java_vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
  env->CallVoidMethod(calling_activity_obj_, on_saving_adf_progress_updated_,
                      progress);
}

void AreaLearningApp::OnFrameAvailable(const TangoImageBuffer *buffer) {
//  int64_t local_duration = buffer->exposure_duration_ns;
//  double dduration = local_duration * 1e-6;
//  LOGI("AreaLearningApp:: tango frame time %.9f, exposure time %.3f", buffer->timestamp, dduration);

  if (yuv_drawable_ == NULL || yuv_drawable_->GetTextureId() == 0) {
    LOGE("AreaLearningApp::yuv texture id not valid");
    return;
  }

  // The memory needs to be allocated after we get the first frame because we
  // need to know the size of the image.
  if (!is_yuv_texture_available_) {
    yuv_width_ = buffer->width;
    yuv_height_ = buffer->height;
    uv_buffer_offset_ = yuv_width_ * yuv_height_;

    yuv_size_ = yuv_width_ * yuv_height_ + yuv_width_ * yuv_height_ / 2;

    // Reserve and resize the buffer size for RGB and YUV data.
    yuv_buffer_.resize(yuv_size_);
    yuv_temp_buffer_.resize(yuv_size_);
    rgb_buffer_.resize(yuv_width_ * yuv_height_ * 3);

    AllocateTexture(yuv_drawable_->GetTextureId(), yuv_width_, yuv_height_);
    is_yuv_texture_available_ = true;
  }

  std::lock_guard<std::mutex> lock(yuv_buffer_mutex_);
  memcpy(&yuv_temp_buffer_[0], buffer->data, yuv_size_);
  swap_buffer_signal_ = true;
}

void AreaLearningApp::DeleteDrawables() {
  delete video_overlay_drawable_;
  delete yuv_drawable_;
  video_overlay_drawable_ = NULL;
  yuv_drawable_ = NULL;
}

void AreaLearningApp::OnSurfaceCreated() {
  if (video_overlay_drawable_ != NULL || yuv_drawable_ != NULL) {
    this->DeleteDrawables();
  }

  video_overlay_drawable_ =
      new tango_gl::VideoOverlay(GL_TEXTURE_EXTERNAL_OES, TANGO_SUPPORT_ROTATION_180);
  yuv_drawable_ = new tango_gl::VideoOverlay(GL_TEXTURE_2D, TANGO_SUPPORT_ROTATION_180);
}

void AreaLearningApp::OnSurfaceChanged(int width, int height) {
  glViewport(0, 0, width, height);
}

void AreaLearningApp::OnDrawFrame() {
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

  if (!is_service_connected_) {
    return;
  }

  if (!is_texture_id_set_) {
    is_texture_id_set_ = true;
    // Connect color camera texture. TangoService_connectTextureId expects a
    // valid texture id from the caller, so we will need to wait until the GL
    // content is properly allocated.
    int texture_id = static_cast<int>(video_overlay_drawable_->GetTextureId());
    TangoErrorType ret = TangoService_connectTextureId(
        camera_id_, texture_id, nullptr, nullptr);
    if (ret != TANGO_SUCCESS) {
      LOGE(
          "AreaLearningApp: Failed to connect the texture id with error"
              "code: %d",
          ret);
    }
  }

  if (!is_video_overlay_rotation_set_) {
    video_overlay_drawable_->SetDisplayRotation(TANGO_SUPPORT_ROTATION_180);
    yuv_drawable_->SetDisplayRotation(TANGO_SUPPORT_ROTATION_180);
    is_video_overlay_rotation_set_ = true;
  }
  RenderYuv();
}

void AreaLearningApp::AllocateTexture(GLuint texture_id, int width, int height) {
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
               GL_UNSIGNED_BYTE, rgb_buffer_.data());
}

void AreaLearningApp::RenderYuv() {
  if (!is_yuv_texture_available_) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(yuv_buffer_mutex_);
    if (swap_buffer_signal_) {
      std::swap(yuv_buffer_, yuv_temp_buffer_);
      swap_buffer_signal_ = false;
    }
  }
  if (camera_id_ == TANGO_CAMERA_FISHEYE) {
    for (size_t i = 0; i < yuv_height_; ++i) {
      for (size_t j = 0; j < yuv_width_; ++j) {
        size_t x_index = j;
        if (j % 2 != 0) {
          x_index = j - 1;
        }

        size_t rgb_index = (i * yuv_width_ + j) * 3;
        size_t rgba_index = (i * yuv_width_ + j) * 4;

        // The YUV texture format is NV21,
        // yuv_buffer_ buffer layout:
        //   [y0, y1, y2, ..., yn, v0, u0, v1, u1, ..., v(n/4), u(n/4)]
        Yuv2Rgb(
                yuv_buffer_[i * yuv_width_ + j],
                yuv_buffer_[uv_buffer_offset_ + (i / 2) * yuv_width_ + x_index + 1],
                yuv_buffer_[uv_buffer_offset_ + (i / 2) * yuv_width_ + x_index],
                &rgb_buffer_[rgb_index], &rgb_buffer_[rgb_index + 1],
                &rgb_buffer_[rgb_index + 2]);
      }
    }
  } else if (camera_id_ == TANGO_CAMERA_COLOR) {
    for (size_t i = 0; i < yuv_height_; ++i) {
      for (size_t j = 0; j < yuv_width_; ++j) {
        size_t x_index = j;
        if (j % 2 != 0) {
          x_index = j - 1;
        }
        size_t rgbi = yuv_height_ - 1 - i;
        size_t rgbj = yuv_width_ - 1 - j;
        size_t rgb_index = (rgbi * yuv_width_ + rgbj) * 3;
        size_t rgba_index = (rgbi * yuv_width_ + rgbj) * 4;

        // The YUV texture format is NV21,
        // yuv_buffer_ buffer layout:
        //   [y0, y1, y2, ..., yn, v0, u0, v1, u1, ..., v(n/4), u(n/4)]
        Yuv2Rgb(
                yuv_buffer_[i * yuv_width_ + j],
                yuv_buffer_[uv_buffer_offset_ + (i / 2) * yuv_width_ + x_index + 1],
                yuv_buffer_[uv_buffer_offset_ + (i / 2) * yuv_width_ + x_index],
                &rgb_buffer_[rgb_index], &rgb_buffer_[rgb_index + 1],
                &rgb_buffer_[rgb_index + 2]);
      }
    }
  }

  glBindTexture(GL_TEXTURE_2D, yuv_drawable_->GetTextureId());
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, yuv_width_, yuv_height_, 0, GL_RGB,
               GL_UNSIGNED_BYTE, rgb_buffer_.data());

  yuv_drawable_->Render(glm::mat4(1.0f), glm::mat4(1.0f));
}

void AreaLearningApp::RenderTextureId() {
  double timestamp;
  // TangoService_updateTexture() updates target camera's
  // texture and timestamp.
  int ret = TangoService_updateTexture(camera_id_, &timestamp);
  if (ret != TANGO_SUCCESS) {
    LOGE(
        "AreaLearningApp: Failed to update the texture id with error code: "
            "%d",
        ret);
  }
  video_overlay_drawable_->Render(glm::mat4(1.0f), glm::mat4(1.0f));
}
}  // namespace hello_area_description
