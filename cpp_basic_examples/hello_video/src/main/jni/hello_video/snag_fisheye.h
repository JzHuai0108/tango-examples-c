#ifndef CPP_BASIC_EXAMPLES_HELLO_VIDEO_SNAG_FISHEYE_H_
#define CPP_BASIC_EXAMPLES_HELLO_VIDEO_SNAG_FISHEYE_H_


#include <fstream>
#include <iomanip>
#include <sstream>

#include <tango_client_api.h>  // NOLINT

namespace hello_video {
class SaveFisheyeFrame {
 public:

  SaveFisheyeFrame() : yuv_buffer_(nullptr) {
  }

  void Initialize(const TangoImageBuffer* buffer) {
     LOGI("HelloVideoApp::buffer format %d h %d w %d stride %d expo_duration %lld",
          buffer->format, buffer->height, buffer->width, buffer->stride,
          buffer->exposure_duration_ns);
     yuv_width_ = buffer->width;
     yuv_height_ = buffer->height;
     frame_count_ = 0;
     save_path_ = "/sdcard/temp"; //TODO(jhuai): the user needs to make sure this path exists
  }

  bool SaveFrame() {
    LOGI("HelloVideoApp::received frame at time %.9f with interval %.9f",
        timestamp_, timestamp_ - last_timestamp_);
    if (frame_count_ < 0) { //TODO(jhuai): To collect sample frames enlarge the right value
      std::ostringstream o;
      o << save_path_ << "/" << std::setprecision(16) << timestamp_ << "_" <<
        exposure_duration_ns_ << ".txt";
      std::string filename = o.str();
      std::ofstream ofs(filename, std::ofstream::out);

      LOGI("HelloVideoApp::creating image file %s", filename.c_str());
      for (int row = 0; row < yuv_height_; ++row) {
        for (int col = 0; col < yuv_width_; ++col) {
          int index = row*yuv_width_ + col;
          ofs << (int)(yuv_buffer_->at(index)) << " ";
        }
        ofs << std::endl;
      }

      ofs.close();
      ++frame_count_;
      return true;
    } else {
      return false;
    }
  }

  void GleanFrameInfo(const std::vector<uint8_t>& yuv_buffer, const TangoImageBuffer* buffer) {
    yuv_buffer_ = &yuv_buffer;
    exposure_duration_ns_ = buffer->exposure_duration_ns;
    last_timestamp_ = timestamp_;
    timestamp_ = buffer->timestamp;
  }

  const std::vector<uint8_t>* yuv_buffer_;
  int64_t exposure_duration_ns_;
  double timestamp_;
  double last_timestamp_;
  size_t yuv_width_;
  size_t yuv_height_;

  std::string save_path_;
  size_t frame_count_;
}; // class save fisheye frame
} // namespace hello_video

#endif