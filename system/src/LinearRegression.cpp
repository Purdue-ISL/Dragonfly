/*
 * LinearRegression.cpp
 *
 *  Created on: Sep 15, 2021
 *      Author: cbothra
 */
#include "LinearRegression.h"

#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <random>

#include <gflags/gflags.h>

#include "Util.h"

DEFINE_int32(RandomDistributionUpperRange, 0,
             "upper bound for random distribution");

void LinearRegression::predict(
    std::vector<std::pair<float, float>> &lrPredictions,
    std::vector<std::pair<float, float>> &input, int length) {
  estimateCoefficient(std::ref(input), length);
  lrPredictions.push_back(
      std::make_pair(input[length - 1].first, input[length - 1].second));

  std::string results = "";
  for (size_t i = hw_ + 1; i < pw_ + hw_ + 1; i++) {
    float predYaw = yawA_ + yawB_ * i;
    float predPitch = pitchA_ + pitchB_ * i;

    if (predYaw > 360) {
      predYaw -= int(predYaw / 360) * 360;
    } else if (predYaw < 0) {
      predYaw += (int(-predYaw / 360) + 1) * 360;
    }

    if (predPitch > 180) {
      predPitch -= int(predPitch / 180) * 180;
    } else if (predPitch < 0) {
      predPitch += (int(-predPitch / 180) + 1) * 180;
    }
    results +=
        "(" + std::to_string(predYaw) + "," + std::to_string(predPitch) + ")--";
    lrPredictions.push_back(std::make_pair(predYaw, predPitch));
  }
  std::string frameOut = std::to_string(length) + "(" +
                         std::to_string(input[length - 1].first) + "," +
                         std::to_string(input[length - 1].second) + ")";
  fprintf(predictionLog_, "%-50s %-20s\n", frameOut.c_str(), results.c_str());
}

void LinearRegression::predictPerfect(
    std::vector<std::pair<float, float>> vpTruth, int length) {
  initPerfect();
  std::string results = "";
  for (int i = length; i < length + 25; i++) {
    vpTruth.push_back(std::make_pair(groundTruthCoordinates_[i - 1].first,
                                     groundTruthCoordinates_[i - 1].second));
    results += "(" + std::to_string(groundTruthCoordinates_[i - 1].first) +
               "," + std::to_string(groundTruthCoordinates_[i - 1].second) +
               ")--";
  }
  std::string frameOut =
      std::to_string(length) + "(" +
      std::to_string(groundTruthCoordinates_[length - 1].first) + "," +
      std::to_string(groundTruthCoordinates_[length - 1].second) + ")";
  fprintf(predictionLog_, "%-50s %-20s\n", frameOut.c_str(), results.c_str());
}

void LinearRegression::estimateCoefficient(
    std::vector<std::pair<float, float>> &input, int length) {
  std::pair<float, float> sums(0, 0);
  std::pair<float, float> sumsMul(0, 0);
  float idxMulSum = 0;
  float idxSum = 0;

  std::default_random_engine uniformGenerator;
  std::uniform_real_distribution<float> uniformDistribution(
      0, FLAGS_RandomDistributionUpperRange);
  auto getUniformRandomNumber =
      std::bind(uniformDistribution, uniformGenerator);

  int yawOverlap = 0;
  int pitchOverlap = 0;
  for (int idx = length - hw_, i = 1; idx < length; idx++, i++) {
    float yaw = input[idx].first;
    float prevYaw = input[idx - 1].first;
    if (FLAGS_RandomDistributionUpperRange > 0) {
      yaw += getUniformRandomNumber();
      prevYaw += getUniformRandomNumber();
    }

    if (idx != length - (int)hw_) {
      if (std::abs(yaw - prevYaw) >= 180) {
        if (prevYaw < yaw) {
          yawOverlap--;
        } else {
          yawOverlap++;
        }
      }
      if (std::abs(input[idx].second - input[idx - 1].second) >= 90) {
        if (input[idx - 1].second < input[idx].second) {
          pitchOverlap--;
        } else {
          pitchOverlap++;
        }
      }
    }

    auto calbYaw = yaw + yawOverlap * 360;
    sums.first += calbYaw;
    sumsMul.first += i * calbYaw;

    auto calbPitch = input[idx].second + pitchOverlap * 180;
    sums.second += calbPitch;
    sumsMul.second += i * calbPitch;

    idxSum += i;
    idxMulSum += i * i;
  }

  float idxMean_ = idxSum / hw_;
  float yawMean_ = sums.first / hw_;
  float pitchMean_ = sums.second / hw_;

  float xx = idxMulSum - (hw_ * idxMean_ * idxMean_);
  float xyYaw = sumsMul.first - (hw_ * yawMean_ * idxMean_);
  float xyPitch = sumsMul.second - (hw_ * pitchMean_ * idxMean_);

  yawB_ = xyYaw / xx;
  yawA_ = yawMean_ - (yawB_ * idxMean_);

  pitchB_ = xyPitch / xx;
  pitchA_ = pitchMean_ - (pitchB_ * idxMean_);
}

void LinearRegression::initPerfect() {
  std::string line;
  std::ifstream infile(vpCorrPerFrameTracePath_);
  while (std::getline(infile, line)) {
    auto pos = line.find(",");
    try {
      auto yaw = std::stof(line.substr(0, pos));
      auto pitch = std::stof(line.substr(pos + 1));
      groundTruthCoordinates_.push_back(std::make_pair(yaw, pitch));
    } catch (std::invalid_argument &e) {
      std::cout << "Error reading ground truth\n" << line << std::endl;
    }
  }
}

LinearRegression::LinearRegression(std::string vpCorrPerFrameTracePath,
                                   std::string model, size_t window) {
  std::string filename = "prediction_log_" + Util::getLogTimestamp() + ".txt";
  predictionLog_ = fopen(filename.c_str(), "wb");
  fprintf(predictionLog_, "%-50s %s \n", "frame id (yaw,pitch)", "predictions");

  vpCorrPerFrameTracePath_ = vpCorrPerFrameTracePath;
  pw_ = window;
  hw_ = window / 2;
}