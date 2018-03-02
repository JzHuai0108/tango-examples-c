#ifndef CPP_BASIC_EXAMPLES_HELLO_VIDEO_SNAG_FISHEYE_H_
#define CPP_BASIC_EXAMPLES_HELLO_VIDEO_SNAG_FISHEYE_H_


#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include <tango_client_api.h>  // NOLINT

namespace hello_video {

const TangoCameraId CAMERA_OF_INTEREST = TANGO_CAMERA_FISHEYE;

class SaveFisheyeFrame {
 public:

  SaveFisheyeFrame();

  void Initialize(const TangoImageBuffer* buffer);

  bool SaveFrame();

  void GleanFrameInfo(const std::vector<uint8_t>& yuv_buffer, const TangoImageBuffer* buffer);

  const std::vector<uint8_t>* yuv_buffer_;
  int64_t exposure_duration_ns_;
  double timestamp_;
  double last_timestamp_;
  size_t yuv_width_;
  size_t yuv_height_;

  std::string save_path_;
  int frame_count_;
}; // class save fisheye frame
} // namespace hello_video

#endif