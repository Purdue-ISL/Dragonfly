/*
 * BandwidthPredictor.cpp
 *
 *  Created on: Jun 2, 2021
 *      Author: eghabash
 */

#include "BandwidthPredictor.h"

#include <cmath>
#include <iostream>

BandwidthPredictor::BandwidthPredictor() {
  tileSizesSum_ = 0;
  totalDownloadTimeInMS_ = 0;
}

void BandwidthPredictor::addTileInfo(uint32_t tileSize, int downloadTimeInMS) {
  tileInfoMutex_.lock();
  tileSizesSum_ += tileSize;
  totalDownloadTimeInMS_ += downloadTimeInMS;
  tileInfoMutex_.unlock();
}

std::pair<uint32_t, int> BandwidthPredictor::getCurrentTilesInfo() {
  tileInfoMutex_.lock();
  std::pair<uint32_t, int> avgParameters;
  // The download time of the data needs to be > 250ms
  // for high confidence bandwidth calculations.
  if (totalDownloadTimeInMS_ < 20) {
    avgParameters = {0, 0};
  } else {
    avgParameters = {tileSizesSum_, totalDownloadTimeInMS_};
    tileSizesSum_ = 0;
    totalDownloadTimeInMS_ = 0;
  }
  tileInfoMutex_.unlock();
  return avgParameters;
}

float BandwidthPredictor::getMpcBandwidthPrediction() {
  auto trafficInfo = getCurrentTilesInfo();

  if (trafficInfo.second != 0) { // if download time is low, then skip bw calc.
    bwGroundTruth_.push_back((trafficInfo.first * 1e3) /
                             (trafficInfo.second * 1.0)); // byte/sec
  }
  if (bwPredicted_.size() > 0) {
    float currError =
        (bwPredicted_.back() - bwGroundTruth_.back()) / bwGroundTruth_.back();
    bwError_.push_back(std::abs(currError));
  }
  int timeStartIdx =
      bwGroundTruth_.size() < 50 ? 0 : bwGroundTruth_.size() - 50;
  float bwSum = 0;
  int bwCount = bwGroundTruth_.size() - timeStartIdx;
  for (; timeStartIdx < (int)bwGroundTruth_.size(); timeStartIdx++) {
    bwSum += (1 / bwGroundTruth_[timeStartIdx]);
  }
  float harmonicBw = 1 / (bwSum / bwCount);

  float maxError = 0;
  timeStartIdx = bwError_.size() < 10 ? 0 : bwError_.size() - 10;
  for (; timeStartIdx < (int)bwError_.size(); timeStartIdx++) {
    maxError = std::max(maxError, bwError_[timeStartIdx]);
  }
  auto futureBw = harmonicBw / (1 + maxError);
  futureBw = std::isnan(futureBw) ? 0 : futureBw;
  if (futureBw != 0) {
    bwPredicted_.push_back(harmonicBw);
  }
  // Bytes / Sec
  return futureBw == 0 ? 0 : futureBw;
}