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

#include <sstream>

#include "hello_area_description/pose_data.h"

namespace hello_area_description {

PoseData::PoseData() {}

PoseData::~PoseData() {}

void PoseData::UpdatePose(const TangoPoseData& pose_data) {
  // We check the frame pair received in the pose_data instance, and store it
  // in the proper cooresponding local member.
  //
  // The frame pairs are the one we registred in the
  // TangoService_connectOnPoseAvailable functions during the setup of the
  // Tango configuration file.
  if (pose_data.frame.base == TANGO_COORDINATE_FRAME_START_OF_SERVICE &&
      pose_data.frame.target == TANGO_COORDINATE_FRAME_DEVICE) {
    start_service_T_device_pose_ = pose_data;
    // output_stream_ will check itself is_open or not in dumping
    output_stream_ << std::setprecision(18) << pose_data.timestamp << " "
                   << std::setprecision(9) << pose_data.translation[0] << " " << pose_data.translation[1]
                   << " " << pose_data.translation[2] << std::setprecision(9)
                   << " " << pose_data.orientation[0] << " " << pose_data.orientation[1]
                   << " " << pose_data.orientation[2] << " " << pose_data.orientation[3]
                   << "\n";

  } else if (pose_data.frame.base == TANGO_COORDINATE_FRAME_AREA_DESCRIPTION &&
             pose_data.frame.target == TANGO_COORDINATE_FRAME_DEVICE) {
    adf_T_device_pose_ = pose_data;
  } else if (pose_data.frame.base == TANGO_COORDINATE_FRAME_AREA_DESCRIPTION &&
             pose_data.frame.target ==
                 TANGO_COORDINATE_FRAME_START_OF_SERVICE) {
    is_relocalized_ = (pose_data.status_code == TANGO_POSE_VALID);
  } else {
    return;
  }
}

void MoveFile(const std::string& old_name, const std::string& new_name) {
  std::ifstream ifs(old_name, std::ios::in | std::ios::binary);
  std::ofstream ofs(new_name, std::ios::out | std::ios::binary);
  ofs << ifs.rdbuf();
  std::remove(old_name.c_str());
}

void PoseData::ResetPoseData(const std::string& odometry_dest_file) {
  is_relocalized_ = false;
  output_stream_.close();
  MoveFile(output_csv_, odometry_dest_file);
  output_csv_ = "";
}

TangoPoseData PoseData::GetCurrentPoseData() {
  if (is_relocalized_) {
    return adf_T_device_pose_;
  } else {
    return start_service_T_device_pose_;
  }
}

void PoseData::StartRecordingOdometry(const std::string& output_csv) {
  output_csv_ = output_csv;
  output_stream_.open(output_csv);
}
}  // namespace hello_area_description
