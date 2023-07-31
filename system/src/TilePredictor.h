/*
 * ABR.h
 *
 *  Created on: May 5, 2021
 *      Author: eghabash
 */

#ifndef TILEPREDICTOR_H_
#define TILEPREDICTOR_H_

#include <map>
#include <mutex>
#include <utility>
#include <vector>

#include "LinearRegression.h"

class TilePredictor {
public:
  void addVpCoordinate(std::pair<float, float> coordinate, bool playNextFrame);

  // returns frameId --> tile class --> set of tiles.
  std::map<uint16_t, std::map<uint8_t, std::vector<uint16_t>>>
  getPredictedTilesStatic();

  std::map<uint16_t, std::map<uint8_t, std::vector<uint16_t>>>
  getPredictedTilesFlareLR(std::vector<std::pair<int, int>> vpResolutions);

  /**
   * @brief This function calculates the utility for all to-be-recevied tiles in
   *        the next 2 seconds.
   *
   * @param predictedCorr: the predicted user corrdinates using LR.
   * @param vpResolutions: different viewport corrdinates (classes)
   * @param numberOfFutureFrames (number of future frames to lookahead (2
   * seconds unless otherwise.))
   * @return std::map<std::pair<int, uint16_t>, std::vector<float>>
   *         <chunkId, tileId>: vecotr<utility if received by frame (vec index)>
   */
  std::map<std::pair<int, uint16_t>, std::vector<float>>
  buildUtilityMatrix(std::vector<std::pair<float, float>> &predictedCorr,
                     std::vector<std::pair<int, int>> &vpResolutions,
                     int chunkToCal);

  TilePredictor(std::string vpCorrPerFrameTracePath, std::string model,
                size_t window);
  uint16_t getFrameId();

  // key1: High/low quality tiles
  // key2: (1 - fraction of overlapping with VP per tile)
  // value: list of tiles.
  void getUrgetTilesListsTemp(
      std::map<std::string, std::map<float, std::vector<uint16_t>>>
          &urgentTiles,
      std::vector<std::pair<float, float>> &predictedCorr);

  void getUrgetTilesList(std::map<float, std::vector<uint16_t>> &urgentTiles,
                         std::vector<std::pair<float, float>> &predictedCorr);

  void getPredictedCorr(std::vector<std::pair<float, float>> &predictedCorr);

  void
  getBackgroundTiles(std::map<float, std::vector<uint16_t>> &bgTiles,
                     std::pair<std::pair<float, float>, std::pair<float, float>>
                         displacement /* <left,right> <down,up> */,
                     uint16_t frameGroundTruth);

  void
  sortTileSetByArea(std::map<float, std::vector<uint16_t>> &tileRanksByArea,
                    std::pair<float, float> &viewportCenter,
                    std::pair<int, int> &viewportResolution);

  /**
   * @brief Get the Overlapping Area Size Per Tile object;
   *        used by Pano to determine the importance per tile-group
   *
   * @param vpResolution viewport resolution in degrees.
   * @param predictionWindow number of frames in horizon.
   * @return std::map<uint16_t, std::map<float, std::vector<uint16_t>>>
   *         <frame, <area_normalized,<tiles>>
   */
  std::map<uint16_t, std::map<float, std::vector<uint16_t>>>
  getOverlappingAreaSizePerTile(std::pair<int, int> vpResolution);

private:
  struct SquareCoordinates {
    std::pair<float, float> upperLeft;
    std::pair<float, float> upperRight;
    std::pair<float, float> lowerLeft;
    std::pair<float, float> lowerRight;
  };

  LinearRegression *linearRegressor_;

  // Assuming the video is only 2000 frames. otherwise,
  // increase the size of vectors.
  // ToDo use manifest to determine, based on video size and FPS.
  std::vector<std::pair<float, float>> vpPredictions_;
  std::vector<std::pair<float, float>> vpGroundTruth_;
  uint16_t frameId_;
  uint16_t corrCount_;

  size_t predictionWindow_;

  // TODO: build manifest to fill from.
  std::map<uint16_t, std::pair<float, float>> tileCoordinates_;
  std::map<uint16_t, std::pair<float, float>> tileResolutions_;

  /*
   * This return the viewport as input,
   * and returns it as multiple squares in case of overlapping.
   */
  void getViewportSquares(std::vector<SquareCoordinates> &vpSquares,
                          std::pair<float, float> &viewportCenter,
                          std::pair<int, int> &viewportResolution);

  /*
   *  This takes tile coordinates, and viewport squares as input,
   *  and returns the fraction of tile that overlap with viewport as output.
   */
  float getFractionOfTileInVP(std::vector<SquareCoordinates> &partialVPs,
                              std::pair<float, float> &tileCorrdinates,
                              std::pair<float, float> &tileDimensions);

  std::pair<float, float> getVpCoordinate();
};

#endif /* TILEPREDICTOR_H_ */
