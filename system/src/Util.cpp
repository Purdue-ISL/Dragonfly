#include "Util.h"

#include <chrono>
#include <functional>
#include <iostream>

#include <gflags/gflags.h>
#include <glog/logging.h>

long Util::videoPlayTime = getTime();
std::string Util::logTimestamp = getCurrentDateTime();

const std::string Util::getCurrentDateTime() {
  time_t now = time(0);
  struct tm tstruct;
  char buf[80];
  tstruct = *localtime(&now);
  strftime(buf, sizeof(buf), "%Y-%m-%d_%H_%M_%S", &tstruct);
  return buf;
}

long Util::getTime() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void Util::sleep(long currentTime, long millisecondsToSleep) {
  while (getTime() - currentTime < millisecondsToSleep)
    ;
}

long Util::getTimePassedSinceLastFrame() {
  auto currentTime = getTime();
  auto timeDiffInMs = currentTime - Util::videoPlayTime;
  return timeDiffInMs;
}

void Util::setFramePlayTime(long FramePlayTimeInMs) {
  Util::videoPlayTime = FramePlayTimeInMs;
}

std::string Util::getLogTimestamp() { return Util::logTimestamp; }
