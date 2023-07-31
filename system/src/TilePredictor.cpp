/*
 * TilePredictor.cpp
 *
 *  Created on: May 5, 2021
 *      Author: eghabash
 */

#include "TilePredictor.h"

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <set>
#include <thread>
#include <utility>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include "Util.h"

DEFINE_bool(predLR, true,
            "true: use linear regression, false: perfect predictor");

void TilePredictor::getViewportSquares(
    std::vector<SquareCoordinates> &vpSquares,
    std::pair<float, float> &viewportCenter,
    std::pair<int, int> &viewportResolution) {
  // find viewport coordinates.
  float baseX1 = viewportCenter.first - (viewportResolution.first / 2.0);
  float baseX2 = viewportCenter.first + (viewportResolution.first / 2.0);

  // 0: viewport is not wrapping.
  bool horizontalFlip = false;

  // 0: viewport is not wrapping, 1: wrapping over y = 0; 2: wrapping over y =
  // 180.
  int verticalFlip = false;

  // frame in degrees [0 - 360] in width, and [0-180] in height.

  // viewport is wrapping over x = 0.
  if (baseX1 < 0) {
    baseX1 += 360;
    horizontalFlip = true;
  }

  // viewport is wrapping over x = 360.
  if (baseX2 > 360) {
    baseX2 -= 360;
    horizontalFlip = true;
  }

  float x1 = baseX1;
  float x2 = baseX2;
  float y1 = viewportCenter.second - (viewportResolution.second / 2.0);

  // viewport is wrapping over y = 0.
  if (y1 < 0) {
    y1 = y1 + 180;
    verticalFlip = true;
  }

  float x3 = baseX1;
  float x4 = baseX2;
  float y2 = viewportCenter.second + (viewportResolution.second / 2.0);

  // viewport is wrapping over y = 180.
  if (y2 > 180) {
    y2 = y2 - 180;
    verticalFlip = true;
  }

  if (!horizontalFlip && !verticalFlip) {
    // only one square.
    SquareCoordinates vp = {std::make_pair(x3, y2), std::make_pair(x4, y2),
                            std::make_pair(x1, y1), std::make_pair(x2, y1)};
    vpSquares = {vp};
  } else if (!horizontalFlip && verticalFlip) {
    // viewport has two squares due to overlapping over y = 0.
    SquareCoordinates vpPart1 = {
        std::make_pair(x3, y2),
        std::make_pair(x4,  // @suppress("Invalid arguments")
                       y2),
        std::make_pair(x3, 0), std::make_pair(x4, 0)};

    SquareCoordinates vpPart2 = {
        std::make_pair(x1, 180),
        std::make_pair(x2,  // @suppress("Invalid arguments")
                       180),
        std::make_pair(x1, y1), std::make_pair(x2, y1)};
    vpSquares = {vpPart1, vpPart2};
  } else if (horizontalFlip && !verticalFlip) {
    // viewport has two squares due to overlapping over x = 0 or x = 360.
    SquareCoordinates vpPart1 = {std::make_pair(0, y2), std::make_pair(x4, y2),
                                 std::make_pair(0, y1), std::make_pair(x2, y1)};

    SquareCoordinates vpPart2 = {
        std::make_pair(x3, y2), std::make_pair(360, y2), std::make_pair(x1, y1),
        std::make_pair(360, y1)};
    vpSquares = {vpPart1, vpPart2};
  } else if (horizontalFlip && verticalFlip) {
    // viewport has three squares due to nested overlapping over x = 0 or x =
    // 360 and y = 0.

    SquareCoordinates vpPart1 = {std::make_pair(0, 180),
                                 std::make_pair(x2, 180), std::make_pair(0, y1),
                                 std::make_pair(x2, y1)};

    SquareCoordinates vpPart2 = {
        std::make_pair(x1, 180), std::make_pair(360, 180),
        std::make_pair(x1, y1), std::make_pair(360, y1)};

    SquareCoordinates vpPart3 = {std::make_pair(0, y2), std::make_pair(x4, y2),
                                 std::make_pair(0, 0), std::make_pair(x4, 0)};

    SquareCoordinates vpPart4 = {std::make_pair(x3, y2),
                                 std::make_pair(360, y2), std::make_pair(x3, 0),
                                 std::make_pair(360, 0)};
    vpSquares = {vpPart1, vpPart2, vpPart3, vpPart4};
  }
}

float TilePredictor::getFractionOfTileInVP(
    std::vector<SquareCoordinates> &partialVPs,
    std::pair<float, float> &tileCorrdinates,
    std::pair<float, float> &tileDimensions) {
  float fracOfTileInVP = 0.0;

  for (auto const &sqrCoor : partialVPs) {
    float xOverlap = std::max(
        (float)0, std::min(sqrCoor.upperRight.first,
                           tileCorrdinates.first + tileDimensions.first) -
                      std::max(sqrCoor.upperLeft.first, tileCorrdinates.first));

    float yOverlap = std::max(
        (float)0, std::min(sqrCoor.upperRight.second, tileCorrdinates.second) -
                      std::max(sqrCoor.lowerRight.second,
                               tileCorrdinates.second - tileDimensions.second));

    fracOfTileInVP += (xOverlap * yOverlap) /
                      float(tileDimensions.first * tileDimensions.second);
  }

  return fracOfTileInVP;
}

void TilePredictor::sortTileSetByArea(
    std::map<float, std::vector<uint16_t>> &tileRanksByArea,
    std::pair<float, float> &viewportCenter,
    std::pair<int, int> &viewportResolution) {
  // per tile in frame get the fraction of its area that overlaps with
  // viewport.
  std::vector<SquareCoordinates> vpSqrs;
  getViewportSquares(vpSqrs, viewportCenter, viewportResolution);
  int vpArea = viewportResolution.first * viewportResolution.second;
  float vpSize = 0;
  for (auto &tileId : tileCoordinates_) {
    float frac = getFractionOfTileInVP(vpSqrs, tileId.second,
                                       tileResolutions_[tileId.first]);
    vpSize += frac;

    // (1.0 - frac) will sort tiles in the map by area
    // (tiles with higher fraction of overlapping will come first)
    if (tileRanksByArea.find(1.0 - frac) == tileRanksByArea.end()) {
      std::vector<uint16_t> tiles;
      tileRanksByArea.insert(std::make_pair(1.0 - frac, tiles));
    }
    tileRanksByArea.find(1.0 - frac)->second.push_back(tileId.first);
  }
  // Sanity check that getFractionOfTileInVP is working properly.
  vpSize = vpSize * 30 * 15;
  if (vpSize <= vpArea - .1 || vpSize >= vpArea + .1) {
    for (auto &pair : vpSqrs) {
      std::cout << "(" << pair.upperLeft.first << "," << pair.upperLeft.second
                << ")"
                << "(" << pair.upperRight.first << "," << pair.upperRight.second
                << ")\n";
      std::cout << "(" << pair.lowerLeft.first << "," << pair.lowerLeft.second
                << ")"
                << "(" << pair.lowerRight.first << "," << pair.lowerRight.second
                << ")\n";
    }
    LOG(ERROR) << "Fraction of tiles covering viewport"
                  " does not match with viewport true size "
               << vpSize;
  }
}

void TilePredictor::getUrgetTilesList(
    std::map<float, std::vector<uint16_t>> &urgentTiles,
    std::vector<std::pair<float, float>> &predictedCorr) {
  // this should return the high quality based on model
  // flare then use similar method to background.
  // utility return the utility matrix for the next .5 seconds.
  // to do change the function that returns the utility matrix to
  // take the number of frames and predict corr as input and vp corrdinates
  // size.
  // add function in tilePredictor to return LR corrdinates.

  while (frameId_ == 0)
    ;

  if (predictedCorr.size() == 0) {
    // low quality tiles = all tiles
    std::vector<uint16_t> allTiles;
    for (uint16_t id = 1; id <= 144; id++) {
      allTiles.push_back(id);
    }
    urgentTiles.insert(std::make_pair(0, allTiles));
  } else {
    int yawDir = 0;    // positive: right
    int pitchDir = 0;  // positive: up
    for (int idx = 1; idx < (int)predictedCorr.size(); idx++) {
      predictedCorr[idx].first - predictedCorr[idx - 1].first >= 0 ? yawDir++
                                                                   : yawDir--;
      predictedCorr[idx].second - predictedCorr[idx - 1].second >= 0
          ? pitchDir++
          : pitchDir--;
    }

    float yawMaxDisp = 0;
    float pitchMaxDisp = 0;
    // low quality tiles (high quality)
    // find the max displacement (for the first half secodns).

    // find the maximum  predicted user displacement.
    for (int frameIdx = 1; frameIdx < (int)predictedCorr.size(); frameIdx++) {
      if (yawDir >= 0) {  // right
        yawMaxDisp +=
            predictedCorr[frameIdx].first < predictedCorr[frameIdx - 1].first
                ? (predictedCorr[frameIdx].first + 360) -
                      predictedCorr[frameIdx - 1].first
                : predictedCorr[frameIdx].first -
                      predictedCorr[frameIdx - 1].first;
      } else {  // left
        yawMaxDisp +=
            predictedCorr[frameIdx].first > predictedCorr[frameIdx - 1].first
                ? ((predictedCorr[frameIdx - 1].first + 360) -
                   predictedCorr[frameIdx].first)
                : predictedCorr[frameIdx - 1].first -
                      predictedCorr[frameIdx].first;
      }

      if (pitchDir >= 0) {  // up
        pitchMaxDisp +=
            predictedCorr[frameIdx].second < predictedCorr[frameIdx - 1].second
                ? ((predictedCorr[frameIdx].second + 180) -
                   predictedCorr[frameIdx - 1].second)
                : (predictedCorr[frameIdx].second -
                   predictedCorr[frameIdx - 1].second);
      } else {  // down
        pitchMaxDisp +=
            predictedCorr[frameIdx].second > predictedCorr[frameIdx - 1].second
                ? ((predictedCorr[frameIdx - 1].second + 180) -
                   predictedCorr[frameIdx].second)
                : (predictedCorr[frameIdx - 1].second -
                   predictedCorr[frameIdx].second);
      }
    }

    yawMaxDisp = yawMaxDisp > 360 ? 360 : yawMaxDisp;
    pitchMaxDisp = pitchMaxDisp > 180 ? 180 : pitchMaxDisp;

    float yawCorrLq = -1;
    float pitchCorrLq = -1;

    if (yawDir > 0) {  // right
      yawCorrLq = predictedCorr[0].first + (yawMaxDisp / 2.0);
      yawCorrLq = yawCorrLq > 360 ? yawCorrLq - 360 : yawCorrLq;
    } else {  // left
      yawCorrLq = predictedCorr[0].first - (yawMaxDisp / 2.0);
      yawCorrLq = yawCorrLq < 0 ? yawCorrLq + 360 : yawCorrLq;
    }

    if (pitchDir > 0) {  // up
      pitchCorrLq = predictedCorr[0].second + (pitchMaxDisp / 2.0);
      pitchCorrLq = pitchCorrLq > 180 ? pitchCorrLq - 180 : pitchCorrLq;
    } else {  // down
      pitchCorrLq = predictedCorr[0].second - (pitchMaxDisp / 2.0);
      pitchCorrLq = pitchCorrLq < 0 ? pitchCorrLq + 180 : pitchCorrLq;
    }

    int vpLqDimYaw = int((100 + yawMaxDisp) * 1);
    int vpLqDimPitch = int((100 + pitchMaxDisp * 1));
    std::pair<float, float> viewportCenterLq(yawCorrLq, pitchCorrLq);
    std::pair<int, int> viewportDimentionLq(
        vpLqDimYaw > 360 ? 360 : vpLqDimYaw,
        vpLqDimPitch > 180 ? 180 : vpLqDimPitch);
    sortTileSetByArea(urgentTiles, viewportCenterLq, viewportDimentionLq);
  }
}

void TilePredictor::getUrgetTilesListsTemp(
    std::map<std::string, std::map<float, std::vector<uint16_t>>> &urgentTiles,
    std::vector<std::pair<float, float>> &predictedCorr) {
  // this should return the high quality based on model
  // flare then use similar method to background.
  // utility return the utility matrix for the next .5 seconds.
  // to do change the function that returns the utility matrix to
  // take the number of frames and predict corr as input and vp corrdinates
  // size.
  // add function in tilePredictor to return LR corrdinates.

  while (frameId_ == 0)
    ;
  float highQwindow = 13;  // half second 25FPS/2;

  if (predictedCorr.size() == 0) {
    // low quality tiles = all tiles
    std::vector<uint16_t> allTiles;
    for (uint16_t id = 1; id <= 144; id++) {
      allTiles.push_back(id);
    }
    std::map<float, std::vector<uint16_t>> lqTiles;
    lqTiles.insert(std::make_pair(0, allTiles));
    urgentTiles.insert(std::make_pair("LQ", lqTiles));

    auto &vpCorr = vpGroundTruth_[corrCount_ - 1];
    std::pair<int, int> vpResolution(100, 100);
    std::map<float, std::vector<uint16_t>> tileRanksByArea;
    sortTileSetByArea(tileRanksByArea, vpCorr, vpResolution);
    urgentTiles.insert(std::make_pair("HQ", tileRanksByArea));
  } else {
    int yawDir = 0;    // positive: right
    int pitchDir = 0;  // positive: up
    for (int idx = 1; idx < (int)predictedCorr.size(); idx++) {
      predictedCorr[idx].first - predictedCorr[idx - 1].first >= 0 ? yawDir++
                                                                   : yawDir--;
      predictedCorr[idx].second - predictedCorr[idx - 1].second >= 0
          ? pitchDir++
          : pitchDir--;
    }

    float yawMaxDisp = 0;
    float pitchMaxDisp = 0;
    float yawMaxDispHq = 0;
    float pitchMaxDispHq = 0;

    // low quality tiles (high quality)
    // 1. first find the max displacement (for the first half secodns).
    // 2. vp corrdinate will be the middle point, and size is max disp * 1.2
    // (1.1)

    // find the maximum  predicted user displacement.
    for (int frameIdx = 1; frameIdx < (int)predictedCorr.size(); frameIdx++) {
      if (yawDir >= 0) {  // right
        yawMaxDisp +=
            predictedCorr[frameIdx].first < predictedCorr[frameIdx - 1].first
                ? (predictedCorr[frameIdx].first + 360) -
                      predictedCorr[frameIdx - 1].first
                : predictedCorr[frameIdx].first -
                      predictedCorr[frameIdx - 1].first;
      } else {  // left
        yawMaxDisp +=
            predictedCorr[frameIdx].first > predictedCorr[frameIdx - 1].first
                ? ((predictedCorr[frameIdx - 1].first + 360) -
                   predictedCorr[frameIdx].first)
                : predictedCorr[frameIdx - 1].first -
                      predictedCorr[frameIdx].first;
      }

      if (pitchDir >= 0) {  // up
        pitchMaxDisp +=
            predictedCorr[frameIdx].second < predictedCorr[frameIdx - 1].second
                ? ((predictedCorr[frameIdx].second + 180) -
                   predictedCorr[frameIdx - 1].second)
                : (predictedCorr[frameIdx].second -
                   predictedCorr[frameIdx - 1].second);
      } else {  // down
        pitchMaxDisp +=
            predictedCorr[frameIdx].second > predictedCorr[frameIdx - 1].second
                ? ((predictedCorr[frameIdx - 1].second + 180) -
                   predictedCorr[frameIdx].second)
                : (predictedCorr[frameIdx - 1].second -
                   predictedCorr[frameIdx].second);
      }
      if (frameIdx == highQwindow) {
        yawMaxDispHq = yawMaxDisp;
        pitchMaxDispHq = pitchMaxDisp;
      }
    }

    yawMaxDisp = yawMaxDisp > 360 ? 360 : yawMaxDisp;
    yawMaxDispHq = yawMaxDispHq > 360 ? 360 : yawMaxDispHq;

    pitchMaxDisp = pitchMaxDisp > 180 ? 180 : pitchMaxDisp;
    pitchMaxDispHq = pitchMaxDispHq > 180 ? 180 : pitchMaxDispHq;

    float yawCorrLq = -1;
    float pitchCorrLq = -1;
    float yawCorrHq = -1;
    float pitchCorrHq = -1;

    if (yawDir > 0) {  // right
      yawCorrLq = predictedCorr[0].first + (yawMaxDisp / 2.0);
      yawCorrLq = yawCorrLq > 360 ? yawCorrLq - 360 : yawCorrLq;

      yawCorrHq = predictedCorr[0].first + (yawMaxDispHq / 2.0);
      yawCorrHq = yawCorrHq > 360 ? yawCorrHq - 360 : yawCorrHq;
    } else {  // left
      yawCorrLq = predictedCorr[0].first - (yawMaxDisp / 2.0);
      yawCorrLq = yawCorrLq < 0 ? yawCorrLq + 360 : yawCorrLq;

      yawCorrHq = predictedCorr[0].first - (yawMaxDispHq / 2.0);
      yawCorrHq = yawCorrHq < 0 ? yawCorrHq + 360 : yawCorrHq;
    }

    if (pitchDir > 0) {  // up
      pitchCorrLq = predictedCorr[0].second + (pitchMaxDisp / 2.0);
      pitchCorrLq = pitchCorrLq > 180 ? pitchCorrLq - 180 : pitchCorrLq;

      pitchCorrHq = predictedCorr[0].second + (pitchMaxDispHq / 2.0);
      pitchCorrHq = pitchCorrHq > 180 ? pitchCorrHq - 180 : pitchCorrHq;
    } else {  // down
      pitchCorrLq = predictedCorr[0].second - (pitchMaxDisp / 2.0);
      pitchCorrLq = pitchCorrLq < 0 ? pitchCorrLq + 180 : pitchCorrLq;

      pitchCorrHq = predictedCorr[0].second - (pitchMaxDispHq / 2.0);
      pitchCorrHq = pitchCorrHq < 0 ? pitchCorrHq + 180 : pitchCorrHq;
    }

    int vpLqDimYaw = int((100 + yawMaxDisp) * 1.2);
    int vpLqDimPitch = int((100 + pitchMaxDisp * 1.2));
    std::pair<float, float> viewportCenterLq(yawCorrLq, pitchCorrLq);
    std::pair<int, int> viewportDimentionLq(
        vpLqDimYaw > 360 ? 360 : vpLqDimYaw,
        vpLqDimPitch > 180 ? 180 : vpLqDimPitch);
    std::map<float, std::vector<uint16_t>> tileRanksByAreaLq;
    sortTileSetByArea(tileRanksByAreaLq, viewportCenterLq, viewportDimentionLq);
    urgentTiles.insert(std::make_pair("LQ", tileRanksByAreaLq));

    int vpHqDimYaw = int((100 + yawMaxDispHq) * 1.1);
    int vpHqDimPitch = int((100 + pitchMaxDispHq * 1.1));

    std::pair<float, float> viewportCenterHq(yawCorrHq, pitchCorrHq);
    std::pair<int, int> viewportDimentionHq(
        vpHqDimYaw > 360 ? 360 : vpHqDimYaw,
        vpHqDimPitch > 180 ? 180 : vpHqDimPitch);
    std::map<float, std::vector<uint16_t>> tileRanksByAreaHq;
    sortTileSetByArea(tileRanksByAreaHq, viewportCenterHq, viewportDimentionHq);
    urgentTiles.insert(std::make_pair("HQ", tileRanksByAreaHq));
  }
}

void TilePredictor::getBackgroundTiles(
    std::map<float, std::vector<uint16_t>> &bgTiles,
    std::pair<std::pair<float, float>, std::pair<float, float>>
        displacement /* <left,right> <down,up> */,
    uint16_t frameGroundTruth) {
  while (frameId_ == 0)
    ;
  if (frameGroundTruth != 0) {
    while (frameId_ < frameGroundTruth)
      ;
    LOG(INFO) << "Ground Truth predictions:";
    LOG(INFO) << vpGroundTruth_[frameGroundTruth - 1].first << ","
              << vpGroundTruth_[frameGroundTruth - 1].second;
    LOG(INFO) << displacement.first.first << " : " << displacement.first.second;
    LOG(INFO) << displacement.second.first << " : "
              << displacement.second.second;
    LOG(INFO) << "=======\n";
  }
  auto vpCorr = frameGroundTruth == 0 ? vpGroundTruth_[corrCount_ - 1]
                                      : vpGroundTruth_[frameGroundTruth - 1];
  float leftCorr = vpCorr.first - (displacement.first.first + 50);
  float rightCorr = vpCorr.first + (displacement.first.second + 50);
  float downCorr = vpCorr.second - (displacement.second.first + 50);
  float upCorr = vpCorr.second + (displacement.second.second + 50);

  // calibrate corrdinates
  leftCorr = leftCorr < 0 ? leftCorr + 360 : leftCorr;
  rightCorr = rightCorr > 360 ? rightCorr - 360 : rightCorr;
  downCorr = downCorr < 0 ? downCorr + 180 : downCorr;
  upCorr = upCorr > 180 ? upCorr - 180 : upCorr;

  // find the distance between the two exterme; divide by half to find the
  // center.
  float yawMidDis = leftCorr > rightCorr ? (((360 - leftCorr) + rightCorr) / 2)
                                         : ((rightCorr - leftCorr) / 2);
  float pitchMidDis = downCorr > upCorr ? (((180 - downCorr) + upCorr) / 2)
                                        : ((upCorr - downCorr) / 2);

  float yawCenter = leftCorr + yawMidDis;
  float pitchCenter = downCorr + pitchMidDis;

  yawCenter = yawCenter < 0 ? yawCenter + 360
                            : (yawCenter > 360 ? yawCenter - 360 : yawCenter);
  pitchCenter = pitchCenter < 0
                    ? pitchCenter + 180
                    : (pitchCenter > 180 ? pitchCenter - 180 : pitchCenter);
  // 100x100 is the viewport size + displacment in each direction will be the
  // background dimension required.
  float yawDimension =
      displacement.first.first + displacement.first.second + 100;
  float pitchDimension =
      displacement.second.first + displacement.second.second + 100;

  yawDimension = yawDimension > 360 ? 360 : yawDimension;
  pitchDimension = pitchDimension > 180 ? 180 : pitchDimension;

  std::pair<float, float> viewportCenter(yawCenter, pitchCenter);
  std::pair<int, int> viewportDimention(yawDimension, pitchDimension);
  sortTileSetByArea(bgTiles, viewportCenter, viewportDimention);
}

std::map<uint16_t, std::map<uint8_t, std::vector<uint16_t>>>
TilePredictor::getPredictedTilesFlareLR(
    std::vector<std::pair<int, int>> vpResolutions) {
  std::map<uint16_t, std::map<uint8_t, std::vector<uint16_t>>>
      tileClassAllFrames;

  // video join time as it only happens at the start of video sessions.
  while (frameId_ == 0)
    ;

  std::vector<std::pair<float, float>> predictedCorr;

  if (FLAGS_predLR) {
    LOG(INFO) << "Linear Regression";
    if (corrCount_ >= (predictionWindow_ / 2)) {
      linearRegressor_->predict(predictedCorr, std::ref(vpGroundTruth_),
                                corrCount_);
    }
  } else {
    LOG(INFO) << "Perfect predictor";
    linearRegressor_->predictPerfect(predictedCorr, corrCount_);
  }
  uint16_t frameId = frameId_;

  for (uint16_t idx = 0; idx < predictionWindow_; idx++) {  // per frame
    if (frameId >= 1475) {
      break;
    }

    std::pair<float, float> viewportCenter;
    // use static predictor.
    if (predictedCorr.size() == 0) {
      viewportCenter = vpGroundTruth_[corrCount_ - 1];
    } else {
      viewportCenter = predictedCorr[idx];
    }
    // Key: tile-class (i.e. rank).
    // Value: all tiles with that class.
    std::map<uint8_t, std::vector<uint16_t>> fillingMap;

    // Key: frame id
    // Value: all predicted tiles sorted based on rank into sets.
    //        and, tiles within the set are sorted based on area overlapped with
    //        viewport.
    tileClassAllFrames.insert(std::make_pair(frameId + idx, fillingMap));
    auto &tileClassMap = tileClassAllFrames.find(frameId + idx)->second;

    // Two classes: 0 (VP--> highest_class), and 1 (Edge)
    uint8_t tileClass = 0;

    for (auto &vpResolution : vpResolutions) {  // per vp-class
      // find viewport squares.
      // std::vector<SquareCoordinates> vpSqrs;
      // getViewportSquares(vpSqrs, viewportCenter, vpResolution);

      // key: fraction area of tile that overlaps with viewport.
      // value: list of all tiles.

      std::map<float, std::vector<uint16_t>> tilesSortedByArea;
      // sortTileSetByArea(tilesSortedByArea, vpSqrs,
      //                vpResolution.first * vpResolution.second);
      sortTileSetByArea(std::ref(tilesSortedByArea), viewportCenter,
                        vpResolution);

      for (auto const &tileSet : tilesSortedByArea) {
        // if the tiles do not overlap with viewport then skip.
        if ((1 - tileSet.first) == 0) {
          continue;
        }

        // go over all tiles in the set.
        for (auto const &tile : tileSet.second) {
          if (tileClassMap.find(tileClass) == tileClassMap.end()) {
            std::vector<uint16_t> tileSet;
            tileClassMap.insert(std::make_pair(tileClass, tileSet));
          }
          tileClassMap.find(tileClass)->second.push_back(tile);
        }
        if (VLOG_IS_ON(1)) {
          VLOG(1) << "Fraction of area in viewport:" << (1 - tileSet.first);
          std::string vLogTiles;
          for (auto const &tile : tileSet.second) {
            vLogTiles += std::to_string(tile) + ",";
          }
          VLOG(1) << vLogTiles;
        }
      }
      tileClass++;
    }
    if (tileClassAllFrames.find(frameId + idx)->second.size() == 0) {
      tileClassAllFrames.erase(frameId + idx);
    }
  }
  // print tiles in sets
  if (VLOG_IS_ON(1)) {
    for (auto const &chunkSet : tileClassAllFrames) {
      for (auto const &setTiles : chunkSet.second) {
        LOG(INFO) << static_cast<int>(chunkSet.first) << ":"
                  << static_cast<int>(setTiles.first);
        std::string tiles;
        for (auto const &tile : setTiles.second) {
          tiles += std::to_string(tile) + ",";
        }
        LOG(INFO) << tiles;
      }
    }
    LOG(INFO) << "==================";
  }
  return tileClassAllFrames;
}

std::map<uint16_t, std::map<float, std::vector<uint16_t>>>
TilePredictor::getOverlappingAreaSizePerTile(std::pair<int, int> vpResolution) {
  // video join time as it only happens at the start of video sessions.
  while (frameId_ == 0)
    ;

  std::vector<std::pair<float, float>> predictedCorr;

  if (FLAGS_predLR) {
    LOG(INFO) << "Linear Regression";
    if (corrCount_ >= (predictionWindow_ / 2)) {
      linearRegressor_->predict(predictedCorr, std::ref(vpGroundTruth_),
                                corrCount_);
    }
  } else {
    LOG(INFO) << "Perfect predictor";
    linearRegressor_->predictPerfect(predictedCorr, corrCount_);
  }

  std::map<uint16_t, std::map<float, std::vector<uint16_t>>> tileAreaPerFrame;

  uint16_t frameId = frameId_;
  for (uint16_t idx = 0; idx < predictionWindow_; idx++) {  // per frame
    if (frameId >= 1475) {
      break;
    }
    std::pair<float, float> viewportCenter;
    // use static predictor.
    if (predictedCorr.size() == 0) {
      viewportCenter = vpGroundTruth_[corrCount_ - 1];
    } else {
      viewportCenter = predictedCorr[idx];
    }
    // Key: tile-class (i.e. rank).
    // Value: all tiles with that class.
    std::map<uint8_t, std::vector<uint16_t>> fillingMap;

    std::map<float, std::vector<uint16_t>> tilesSortedByArea;
    // sortTileSetByArea(tilesSortedByArea, vpSqrs,
    //                vpResolution.first * vpResolution.second);
    sortTileSetByArea(std::ref(tilesSortedByArea), viewportCenter,
                      vpResolution);
    tileAreaPerFrame.insert({(frameId + idx) - 1, tilesSortedByArea});
  }
  return tileAreaPerFrame;
}

std::map<std::pair<int, uint16_t>, std::vector<float>>
TilePredictor::buildUtilityMatrix(
    std::vector<std::pair<float, float>> &predictedCorr,
    std::vector<std::pair<int, int>> &vpResolutions, int chunkToCal) {
  // video join time as it only happens at the start of video sessions.
  while (frameId_ == 0)
    ;
  uint16_t frameId = frameId_;

  // chunkId_TileId: cumlative utility
  std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix;

  // keep track of what is the frameId for utility at zero (base frame id)
  utilityMatrix.insert({{-1, -1}, {static_cast<float>(frameId)}});

  for (uint8_t idx = 0; idx < predictionWindow_; idx++) {  // per frame
    if (frameId >= 1475) {
      break;
    }
    int chunkId = ((frameId + idx) - 1) / 25;
    if (chunkToCal != -1 && chunkToCal != chunkId) {
      continue;
    }
    std::pair<float, float> viewportCenter;
    // use static predictor.
    if (predictedCorr.size() == 0) {
      viewportCenter = vpGroundTruth_[corrCount_ - 1];
    } else {
      viewportCenter = predictedCorr[idx];
    }

    for (auto &vpResolution : vpResolutions) {  // per vp-class
      // find viewport squares.

      // key: fraction area of tile that overlaps with viewport.
      // value: list of all tiles.
      std::map<float, std::vector<uint16_t>> tilesSortedByArea;
      sortTileSetByArea(tilesSortedByArea, viewportCenter, vpResolution);

      for (auto const &tileSet : tilesSortedByArea) {
        // if the tiles do not overlap with viewport then skip.
        if ((1 - tileSet.first) == 0) {
          continue;
        }
        // go over all tiles in the set.
        for (auto const &tile : tileSet.second) {
          std::pair<int, uint16_t> key{chunkId, tile};
          if (utilityMatrix.find(key) == utilityMatrix.end()) {
            std::vector<float> cumlativeTileUtility(
                predictionWindow_,
                0);  // lookahead frames is predictionWindow_.
            utilityMatrix.insert(std::make_pair(key, cumlativeTileUtility));
          }
          utilityMatrix.find(key)->second[idx] += (1 - tileSet.first);
        }

        // sanity check
        if (VLOG_IS_ON(1)) {
          std::pair<int, uint16_t> tileToPrint{1, 30};
          bool found = false;

          std::string vLogTiles;
          for (auto const &tile : tileSet.second) {
            std::pair<int, uint16_t> key{chunkId, tile};

            if (key == tileToPrint) {
              found = true;
            }
            vLogTiles += std::to_string(tile) + ",";
          }
          if (found) {
            VLOG(0) << "Fraction of area in viewport:" << (1 - tileSet.first)
                    << " [ " << std::to_string(frameId + idx) << " ] ";

            VLOG(0) << vLogTiles;
          }
        }
      }
    }
  }

  for (auto &tileUtility : utilityMatrix) {
    for (uint8_t idx = 1; idx < tileUtility.second.size(); idx++) {
      tileUtility.second[idx] += tileUtility.second[idx - 1];
    }
  }

  // print all tiles along with their utility.
  if (VLOG_IS_ON(1)) {
    std::pair<int, uint16_t> tileToPrint{1, 30};
    for (auto &tileUtility : utilityMatrix) {
      if (tileUtility.first != tileToPrint) {
        continue;
      }
      std::string tile = std::to_string(tileUtility.first.first) + "_" +
                         std::to_string(tileUtility.first.second) + ":";
      for (auto const utility : tileUtility.second) {
        tile += std::to_string(utility) + ",  ";
      }
      tile.pop_back();
      tile.pop_back();
      tile.pop_back();
      LOG(INFO) << tile;
    }
  }
  return utilityMatrix;
}

std::map<uint16_t, std::map<uint8_t, std::vector<uint16_t>>>
TilePredictor::getPredictedTilesStatic() {
  std::map<uint16_t, std::map<uint8_t, std::vector<uint16_t>>>
      tileClassAllFrames;

  // video join time as it only happens at the start of video sessions.
  while (frameId_ == 0)
    ;
  std::vector<std::pair<int, int>> vpResolutions = {{100, 100}, {120, 120}};

  // Todo fix
  uint16_t frameId = frameId_;

  int lastFrame = frameId + 25 * 3;
  // TODO: think how the prediction algorithm would fit.
  for (; frameId < lastFrame; frameId += 25) {
    if (frameId >= 1475) {
      break;
    }
    std::map<uint8_t, std::vector<uint16_t>> fillingMap;
    tileClassAllFrames.insert(std::make_pair(frameId, fillingMap));
    auto &viewportCenter = vpGroundTruth_[corrCount_ - 1];
    auto &tileClassMap = tileClassAllFrames.find(frameId)->second;

    /*
     * key is rank where 0(highest rank), and so on.
     * value is list of tiles with the same rank (key),
     * ordered by fraction of area overlapping with viewport.
     * For instance, first tile (index 0) with key = 0,
     * will be the highly ranked tile.
     */
    uint8_t tileClass = 0;

    // This set will contain all tiles in prev set. all tiles in set are
    // unique.
    std::set<uint16_t> tilesInPrevSets;
    for (auto &vpResolution : vpResolutions) {
      // find viewport squares.

      // key: fraction area of tile that overlaps with viewport.
      // value: all tiles with fraction of area overlapping with viewport
      // equals to key.
      std::map<float, std::vector<uint16_t>> tilesSortedByArea;
      sortTileSetByArea(tilesSortedByArea, viewportCenter, vpResolution);

      for (auto const &tiles : tilesSortedByArea) {
        // if the tiles do not overlap with viewport then skip.
        if ((1 - tiles.first) == 0) {
          continue;
        }

        for (auto const &tile : tiles.second) {
          // if the tile already included in previous higher rank sets, no
          // need to include it in the lower sets
          if (tilesInPrevSets.find(tile) != tilesInPrevSets.end()) {
            continue;
          }
          tilesInPrevSets.insert(tile);

          if (tileClassMap.find(tileClass) == tileClassMap.end()) {
            std::vector<uint16_t> tileSet;
            tileClassMap.insert(std::make_pair(tileClass, tileSet));
          }

          tileClassMap.find(tileClass)->second.push_back(tile);
        }

        if (VLOG_IS_ON(0)) {
          VLOG(1) << "Fraction of area in viewport:" << (1 - tiles.first);
          std::string vLogTiles;
          for (auto const &tile : tiles.second) {
            vLogTiles += std::to_string(tile) + ",";
          }
          VLOG(1) << vLogTiles;
        }
      }
      tileClass++;
    }
  }
  // print tiles in sets
  if (VLOG_IS_ON(1)) {
    for (auto const &tileClassesSingleFrame : tileClassAllFrames) {
      for (auto const &tileClass : tileClassesSingleFrame.second) {
        LOG(INFO) << static_cast<int>(tileClassesSingleFrame.first) << ":"
                  << static_cast<int>(tileClass.first);
        std::string tiles;
        for (auto const &tile : tileClass.second) {
          tiles += std::to_string(tile) + ",";
        }
        LOG(INFO) << tiles;
      }
    }
    LOG(INFO) << "==================";
  }

  return tileClassAllFrames;
}

void TilePredictor::addVpCoordinate(std::pair<float, float> coordinate,
                                    bool playNextFrame) {
  // vpGroundTruth_[frameId_] = coordinate;
  vpGroundTruth_.push_back(coordinate);
  corrCount_++;
  if (playNextFrame) {
    frameId_++;
  }
}

TilePredictor::TilePredictor(std::string vpCorrPerFrameTracePath,
                             std::string model, size_t window) {
  // vpGroundTruth_.reserve(2000);
  vpPredictions_.reserve(2000);
  frameId_ = 0;
  corrCount_ = 0;
  predictionWindow_ = window * 25;
  linearRegressor_ =
      new LinearRegression(vpCorrPerFrameTracePath, model, predictionWindow_);

  // fill
  uint16_t c = 1;
  for (float y = 180; y > 0; y -= 15) {
    for (float x = 0; x < 360; x += 30) {
      tileCoordinates_.insert(std::make_pair(c, std::make_pair(x, y)));
      tileResolutions_.insert(std::make_pair(c, std::make_pair(30, 15)));
      c++;
    }
  }
}

uint16_t TilePredictor::getFrameId() { return frameId_; }

void TilePredictor::getPredictedCorr(
    std::vector<std::pair<float, float>> &predictedCorr) {
  if (FLAGS_predLR) {
    LOG(INFO) << "Linear Regression";
    if (corrCount_ >= (predictionWindow_ / 2)) {
      linearRegressor_->predict(predictedCorr, vpGroundTruth_, corrCount_);
    }
  } else {
    LOG(INFO) << "Perfect predictor";
    linearRegressor_->predictPerfect(predictedCorr, frameId_);
  }
}