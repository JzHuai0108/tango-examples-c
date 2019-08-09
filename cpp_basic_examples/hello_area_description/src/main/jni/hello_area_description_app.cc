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

#include <tango_support.h>
#include <tango_3d_reconstruction_api.h>

#include "hello_area_description/hello_area_description_app.h"

namespace {
// The minimum Tango Core version required from this application.
constexpr int kTangoCoreMinimumVersion = 9377;
const int kVersionStringLength = 128;

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
  pose_data_.ResetPoseData();
  // When disconnecting from the Tango Service, it is important to make sure to
  // free your configuration object. Note that disconnecting from the service,
  // resets all configuration, and disconnects all callbacks. If an application
  // resumes after disconnecting, it must re-register configuration and
  // callbacks with the service.
  TangoConfig_free(tango_config_);
  tango_config_ = nullptr;
  TangoService_disconnect();
}

void DummyProgressCallback(int progress, void* callback_param) {
  LOGI("AreaLearningApp: Export dataset progress %d", progress);
}

void ExportBagToRawFiles(std::string dirToOpen) {
    int callback_param = 0;
    std::string output_path = dirToOpen + "/" + kExportBasename;
    LOGI("AreaLearningApp: %s dirToExport %s", dirToOpen.c_str(), output_path.c_str());
    Tango3DR_Status status = Tango3DR_extractRawDataFromDataset(
            dirToOpen.c_str(), output_path.c_str(),
            &DummyProgressCallback, &callback_param);

    if (status != TANGO_3DR_SUCCESS) {
        LOGE("AreaLearningApp: extractRawDataFromDataset failed with error code: %d", status);
        //  std::exit(EXIT_SUCCESS);
    } else {
        LOGI("AreaLearningApp: Export dataset succeeded to %s", output_path.c_str());
    }

//    Tango3dReconstructionAreaDescription areaDescription =
//            Tango3dReconstructionAreaDescription.createFromDataset(dataset, null, null);
}

// dirToOpen has no trailing "/" or "\\"
// if depth <= 0, will not check the subdirs of dirToOpen
void ProcessDirectory(std::string dirToOpen, int depth) {
    auto dir = opendir(dirToOpen.c_str());
    LOGI("Processing directory: %s", dirToOpen.c_str());
    if (NULL == dir) {
        LOGI("could not open directory: %s", dirToOpen.c_str());
        return;
    }

    auto entity = readdir(dir);
    bool hasBagFile = false;
    bool hasMetadataYaml = false;
    bool hasExported = false;
    std::vector<std::string> dirsToExport;
    while (entity != NULL) {
        if (entity->d_type == DT_REG) {
            // regular file
            std::string basename = std::string(entity->d_name);
            if (basename.compare("dataset_metadata.yaml") == 0) {
                hasMetadataYaml = true;
            }
        }
        if (entity->d_type == DT_DIR) { // it's an direcotry
            std::string basename = std::string(entity->d_name);
            //don't process the  '..' and the '.' directories
            if(basename[0] == '.') {

            } else if (basename.compare("bag") == 0) {
                hasBagFile = true;
            } else if (basename.compare("export") == 0) {
                hasExported = true;
            } else if (depth > 0) {
                ProcessDirectory(dirToOpen + "/" + std::string(entity->d_name), depth - 1);
            }
        }
        if (hasBagFile && hasMetadataYaml && !hasExported) {
            ExportBagToRawFiles(dirToOpen);
        }
        entity = readdir(dir);
    }

    closedir(dir);
}

std::string AreaLearningApp::SaveAdf() {
  std::string adf_uuid_string;
//  if (!pose_data_.IsRelocalized()) {
//    return adf_uuid_string;
//  }
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

  TangoUUID uuid_dataset;
  TangoService_Experimental_getCurrentDatasetUUID(&uuid_dataset);

  std::string uuid_dataset_string = std::string(uuid_dataset);
  LOGI("AreaLearningApp: area uuid %s, dataset uuid %s", adf_uuid_string.c_str(),
          uuid_dataset_string.c_str());
  const int depthToCheck = 1;
  ProcessDirectory(kOutputDir, depthToCheck);

  return adf_uuid_string;
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

}  // namespace hello_area_description
