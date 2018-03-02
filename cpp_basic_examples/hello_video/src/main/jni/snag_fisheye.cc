#include "hello_video/snag_fisheye.h"

#include <tango_client_api.h>  // NOLINT
#include <tango-gl/util.h>

namespace hello_video {


SaveFisheyeFrame::SaveFisheyeFrame() : yuv_buffer_(nullptr) {
}

void SaveFisheyeFrame::Initialize(const TangoImageBuffer* buffer) {
   LOGI("HelloVideoApp::buffer format %d h %d w %d stride %d expo_duration %lld",
        buffer->format, buffer->height, buffer->width, buffer->stride,
        buffer->exposure_duration_ns);
   yuv_width_ = buffer->width;
   yuv_height_ = buffer->height;
   frame_count_ = 0;
   save_path_ = "/sdcard/temp"; //TODO(jhuai): the user needs to make sure this path exists
}

bool SaveFisheyeFrame::SaveFrame() {
  // Because SaveFrame is called possibly at a lower frequency than GleanFrameInfo, so expect
  // the displayed frame time from below does not agree with the interval
  //LOGI("HelloVideoApp::received frame at time %.9f with interval %.9f and exposure time %lld",
    //  timestamp_, timestamp_ - last_timestamp_, exposure_duration_ns_);
  ++frame_count_;
  if (frame_count_ % 25 == 0) { //TODO(jhuai): To collect sample frames enlarge the right value
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

    return true;
  } else {
    return false;
  }
}

void SaveFisheyeFrame::GleanFrameInfo(const std::vector<uint8_t>& yuv_buffer, const TangoImageBuffer* buffer) {
  yuv_buffer_ = &yuv_buffer;
  exposure_duration_ns_ = buffer->exposure_duration_ns;
  last_timestamp_ = timestamp_;
  timestamp_ = buffer->timestamp;
}

}