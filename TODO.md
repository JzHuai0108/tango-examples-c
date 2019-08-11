Problem: record the raw fisheye frames at > 25Hz with synchronized timestamps to the inertial measurement unit. It is beneficial if the fisheye frames can be visualized while being recorded.

Possible solutions:
1. Hello area description + IMU sensor manager
save ADF effectively saves a bag which can export to a list of pnm fisheye images and accelerometer data.
Unfortunately, the gyro data is not available. 
The good news though is that the raw fisheye images and the accelerometer data all are 
timestamped by the SENSOR_EVENT clock which can be used by the IMU sensor manager.
Also the exported fisheye frames are at 30Hz regularly.

is there an option to enable export gyro data? Not yet seen.

Is there an option to only save bag while saving ADF as the procedure is so slow? No, adf has to be saved before exporting.

Does the area description app show anything in the original master branch?

2. Hello video + MediaEncoder
Hello video can somehow visualize the fisheye frame, and save them into jpg frames. 
But the frame rate looks around 20Hz if no saving is done, otherwise it drops to less than 10Hz. 
It would be time consuming to add a heavy mediaencoder though.

关闭         >  开启
kTextureId  >  kYuv

Is the saved fisheye frame original? 
Better follow the coding practice in Tango Ros streamer to 
ensure the quality of the saved fisheye frames.

3. Hello Video + Hello area description + IMU sensor event listener
each for visualization, bag recording and exporting, and accelerometer and gyro recording
The major job is to piece the three together. And it is expected not to take long.

4. Hello video + opencv imwrite
The saving frame rate will not be high, say 20Hz. 
It is so difficult to use opencv in jni c++ code.
Even we may use opencv function to convert the byte buffer to a gray scale image, 
it is still a pain in saving the gray image because of limited support by android.


* How tango ros streamer set message timestamps?
IMU messages: ConnectedNode mConnectedNode.getCurrentTime()
where getCurrentTime is defined [here](http://docs.ros.org/hydro/api/rosjava_core/html/interfaceorg_1_1ros_1_1node_1_1ConnectedNode.html#ad16bd9227059e0acda36998065c3ee27).
...
time_offset_ = ros::Time::now().toSec() - GetBootTimeInSecond();
...

fisheye_image_ = cv::Mat(buffer->height + buffer->height / 2, buffer->width,
                             CV_8UC1, buffer->data, buffer->stride); // No deep copy.
fisheye_image_header_.stamp.fromSec(buffer->timestamp + time_offset_);
color_image_header_.stamp.fromSec(buffer->timestamp + time_offset_);
...

* How does tango ros streamer obtain the raw fisheye frame?
see https://github.com/Intermodalics/tango_ros/blob/c0904da44b691670c8ed5e0916bf346ae16edd73/tango_ros_common/tango_ros_native/src/tango_ros_nodelet.cpp
The basic idea is similar to hello_video


Out of three plans, 1 < 3 < 2 < 4 in order of complexity.