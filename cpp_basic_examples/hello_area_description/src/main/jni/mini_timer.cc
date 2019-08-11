//
// Created by jhuai on 19-8-11.
//

#include "hello_area_description/mini_timer.h"

namespace timing {

Timer::Timer() {
  gettimeofday(&start_, NULL);
}
void Timer::tic() {
  gettimeofday(&start_, NULL);
}
double Timer::toc() {
  // stop timer
  gettimeofday(&end_, NULL);

  // Calculating total time
  double time_taken =
      (end_.tv_sec - start_.tv_sec) +
      (end_.tv_usec - start_.tv_usec) * 1e-6;
  return time_taken;
}

} // namespace timing

