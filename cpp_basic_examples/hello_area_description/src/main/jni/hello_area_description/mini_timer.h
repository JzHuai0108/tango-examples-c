//
// Created by jhuai on 19-8-11.
//

#ifndef CPP_BASIC_EXAMPLES_MINITIMER_H
#define CPP_BASIC_EXAMPLES_MINITIMER_H

#include <sys/time.h>

namespace timing {
class Timer {
  struct timeval start_, end_;
 public:
  // start timer
  Timer();

  // reset timer.
  void tic();

  // stop timer
  // return elapsed time
  double toc();
};
} // namespace timing

#endif //CPP_BASIC_EXAMPLES_MINITIMER_H
