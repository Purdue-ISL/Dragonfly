// This is adapted from:
// https://www.analyticsvidhya.com/blog/2020/04/machine-learning-using-c-linear-logistic-regression/

/*
 * LinearRegression.h
 *
 *  Created on: Sep 15, 2021
 *      Author: cbothra
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <vector>

class LinearRegression {
 private:
  float pitchA_;  // initializing pitch b0
  float pitchB_;  // initializing pitch b1
  float yawA_;
  float yawB_;

  size_t hw_;
  size_t pw_;

  std::string vpCorrPerFrameTracePath_;

  FILE *predictionLog_;

  std::vector<std::pair<float, float>> groundTruthCoordinates_;

  void estimateCoefficient(std::vector<std::pair<float, float>> &input,
                           int length);

  void initPerfect();

 public:
  LinearRegression(std::string vpCorrPerFrameTracePath, std::string model,
                   size_t window);

  /**
   * @brief It takes the histroy --half second-- of the user vp coordinates, and
   * predict where the user will be looking for the next second.
   *
   * @param @return lrPredictions: predicted 1-second of future vp coordinates
   * @param input: user coordinates as vector of pairs <yaw,pitch>.
   * @param length: this is the length of the vector (frameId)
   */
  void predict(std::vector<std::pair<float, float>> &lrPredictions,
               std::vector<std::pair<float, float>> &input, int length);

  /**
   * @brief This returns where the user will be looking without prediction
   * --ground truth--.
   *
   * @param @return ground truth future vp coordinates
   * @param length: current frame Id.
   */
  void predictPerfect(std::vector<std::pair<float, float>>, int length);
};
