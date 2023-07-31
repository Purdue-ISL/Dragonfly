/*
 * AbrAlgorithm.h
 *
 *  Created on: Jun 6, 2021
 *      Author: eghabash
 */

#ifndef ABRALGORITHM_H_
#define ABRALGORITHM_H_

#include "BandwidthPredictor.h"
#include "ClientNetworkLayer.h"
#include "TilePredictor.h"
#include "VideoPlayer.h"

class AbrAlgorithm {
public:
  AbrAlgorithm(std::string tileChunkSizesPath,
               std::string tileChunksQaulityPath,
               std::string backgroundDisplacementPath,
               std::string fullVideoChunkSizePath,
               std::string fullVideoChunkPSNRPath, size_t window);

  static void flareAbr(AbrAlgorithm *abrAlgorithm, TilePredictor *tilePredictor,
                       BandwidthPredictor *bandwidthPredictor,
                       ClientNetworkLayer *clientNetworkLayer,
                       VideoPlayer *videoPlayer);

  static void journalAbr(AbrAlgorithm *abrAlgorithm,
                         TilePredictor *tilePredictor,
                         BandwidthPredictor *bandwidthPredictor,
                         ClientNetworkLayer *clientNetworkLayer,
                         VideoPlayer *videoPlayer);

  static void utilityAbr(AbrAlgorithm *abrAlgorithm,
                         TilePredictor *tilePredictor,
                         BandwidthPredictor *bandwidthPredictor,
                         ClientNetworkLayer *clientNetworkLayer,
                         VideoPlayer *videoPlayer);

  static void panoAbr(AbrAlgorithm *abrAlgorithm, TilePredictor *tilePredictor,
                      BandwidthPredictor *bandwidthPredictor,
                      ClientNetworkLayer *clientNetworkLayer,
                      VideoPlayer *videoPlayer, std::string panoTilesGroupsPath,
                      std::string panoVideoBitrate);

private:
  struct tileNode {
    // tileChunk_tileId
    std::pair<int, uint16_t> tile;
    // expected time to recv tile.
    float EstArrivalTime;
    // expected time to transmit tile.
    float EstDownloadTime;
    uint8_t quality;
    bool baseQualityReceived;
    tileNode *nextTile;
    tileNode *prevTile;
  };

  size_t predictionWindow_;

  const std::map<uint8_t, uint8_t> QUALITYMAP_ = {{42, 1}, {37, 2}, {32, 3},
                                                  {27, 4}, {22, 5}, {17, 6}};
  // quality --> tiles --> tile chunk sizes.
  std::map<uint8_t, std::map<uint16_t, std::vector<uint64_t>>>
      tileChunkSizePerQuality_;

  std::map<uint8_t, std::map<uint16_t, std::vector<float>>>
      tileChunkPSNRPerQuality_;

  std::vector<uint64_t> fullVideoChunksSizes_;

  std::vector<float> fullVideoChunksPSNR_;

  uint8_t numberOfQualities_;

  // yaw<left,right>,pitch<down,up>
  std::vector<std::pair<std::pair<float, float>, std::pair<float, float>>>
      backgroundDisplacement_;

  std::map<int, std::vector<uint16_t>> background_tiles_;

  // quality --> group --> chunk
  // quality range starts from 1 (being the lowest) to N (the highest)
  std::map<uint8_t, std::map<uint8_t, std::vector<uint64_t>>>
      groupChunkSizePerQuality_;

  // quality --> group --> psnr
  std::map<uint8_t, std::map<uint8_t, std::vector<float>>>
      groupChunkPSNRPerQuality_;

  // possible chunk bitrates
  std::vector<std::vector<uint8_t>> bitrateAssignments_;

  /**
   * this will all return all possible quality assignments per tile class using
   * DP. Input:
   *       - number of chunk qualities (Q)
   *       - number of classes (C)
   * Return:
   * - map<quality, all possible assignments starting with quality>
   * e.g.  for Q=3 and C=3, the map would look like this
   *       {
   *          3: ["3,3,3","3,3,2","3,3,1","3,2,2","3,2,1","3,1,1"],
   *          2: ["2,2,2","2,2,1","2,1,1"],
   *          1: ["1,1,1"]
   *       }
   *        with "3,3,3" being highest quality, and "1,1,1" the lowest.
   * */

  std::map<int, std::vector<std::string>>
  getPossibleQualityAssignment(int quality, int tileClass);

  uint8_t getNumberOfQualities();

  std::map<float, std::vector<std::pair<int, uint16_t>>> orderTilesByMaxUtility(
      std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
      uint16_t frameIdToRender);

  std::vector<std::pair<int, uint16_t>> getTilesWithMaxOverallUtility(
      std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
      std::map<float, std::vector<std::pair<int, uint16_t>>>
          sortedTilesByUtility,
      uint16_t frameIdToRender, float estimatedBw, float base1Time,
      ClientNetworkLayer *clientNetworkLayer);

  std::vector<std::pair<std::pair<int, uint16_t>, uint8_t>> qualityABR(
      std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
      uint16_t frameIdToRender, float estimatedBw, float baseTime,
      ClientNetworkLayer *clientNetworkLayer);

  void removeNodeAndUpdateUtility(
      std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
      tileNode *&headTile, tileNode *&tailTile, int frameIdSt, float currTime,
      tileNode *&tileN, float &overallValue);

  tileNode *returnBestPosition(
      std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
      tileNode *&tailTile, tileNode *&tileN, int frameIdSt,
      float downloadTimeUpdated, float tileNewPsnr, float overallValueUpdated,
      float &overallValue);

  void updateArrivalTimeOfSuccessorNodes(tileNode *&tailTile, tileNode *&tileN,
                                         tileNode *&potentionalPos,
                                         float downloadTimeUpdated,
                                         bool placeAtTail);

  void moveAndUpdateTile(tileNode *&headTile, tileNode *&tailTile,
                         tileNode *&tileN, tileNode *&potentionalPos,
                         float downloadTimeUpdated, float currTime,
                         uint8_t qualityIdx, bool placeAtTail);

  void checkTilesUtility(
      std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
      std::map<std::pair<int, uint16_t>, tileNode *> &tilesNodeMap,
      tileNode *&headTile, tileNode *&tailTile, int frameIdSt,
      float estimatedBw, float currTime);

  /**
   * @brief This function will take all urgent tiles that user may view in
   * the
   *        future (next two seconds). Then, it will check if the client
   *        received the tile already or not. If not, then add to request.
   *
   * @param frameIdToRender: this the frame the client player will play
   * next.
   * @param clientNetworkLayer: this checks whether tile has been received
   * in
   *        the buffer or not.
   * @param urgetTiles: list of critical tiles to the user for the next
   * 2-sec
   * @return std::pair<std::string, int>, it returns all tiles in one string
   *         along with their total size
   */
  std::pair<std::string, int> buildBackgroundUrgentTilesRequest(
      uint32_t frameIdToRender, ClientNetworkLayer *clientNetworkLayer,
      std::map<float, std::vector<uint16_t>> &urgetTiles);

  std::vector<std::pair<int, uint16_t>> getBackgroundLessUrgentTilesInfo(
      uint32_t frameIdToRender, ClientNetworkLayer *clientNetworkLayer,
      std::map<float, std::vector<uint16_t>> &urgetTiles);

  /**
   * @brief This will finds the total size for all tiles in each class (i.e.
   *        edgeport or viewport) for each quality possible.
   *        (tile-class,quality) --> size
   *
   *
   * @param frameIdSetQualitySizeSumToReturn: return the size of each class set
   * per each quality possible.
   * @param tilesRequestToReturn: tiles to request from each class.
   * @param tilePredictor: This returns all tiles belong to each class.
   * @param clientNetworkLayer: This excludes tiles that have already been
   * received.
   * @param tileChunkSizePerQuality: This has the size for each tile per
   * quality.
   * @param frameIdToRender: The frame that video player will play next.
   * @param numOfQualities: the number of possible qualities.
   */
  void getTileSetSizePerQuality(
      std::map<int, std::map<uint8_t, std::vector<uint64_t>>>
          &frameIdSetQualitySizeSumToReturn,
      std::map<int, std::map<uint8_t, std::vector<uint16_t>>>
          &tilesRequestToReturn,
      TilePredictor *tilePredictor, ClientNetworkLayer *clientNetworkLayer,
      uint32_t frameIdToRender, uint8_t numOfQualities, uint8_t &numOfClasses);

  void getTileSetSizePerQualityJournal(
      std::map<int, std::map<uint8_t, std::vector<uint64_t>>>
          &frameIdSetQualitySizeSumToReturn,
      std::map<uint8_t, std::vector<std::pair<int, uint16_t>>>
          &tilesRequestToReturn,
      TilePredictor *tilePredictor, ClientNetworkLayer *clientNetworkLayer,
      uint32_t frameIdToRender, uint8_t numOfQualities, uint8_t &numOfClasses,
      int stChunk);

  /**
   * @brief finds the highest quality assignment per tile class where deadline
   * is met.
   *
   * @param frameIdSetQualitySizeSum
   * @param qualitiesAssignments: possible qualities allowed per tile class.
   * @param frameIdToRender: what frame video player will render next.
   * @param numOfQualities: number of qualities per tile.
   * @param predictedBw: predicted bandwidth (avg of past 5 seconds.)
   * @param baseTime: download time to get all critical tiles.
   * @return int: highest quality assignment that meets deadline
   */
  std::map<int, int>
  getQualityIdx(std::map<int, std::map<uint8_t, std::vector<uint64_t>>>
                    &frameIdSetQualitySizeSum,
                std::vector<std::string> qualitiesAssignments,
                uint32_t frameIdToRender, uint8_t numOfQualities,
                float predictedBw, float baseTime, std::string model);

  /**
   * @brief Given tiles, and quality return their total size
   */
  long getTilesSizes(
      std::vector<std::pair<std::pair<int, uint16_t>, uint8_t>> &fgTiles);

  /**
   * @brief Given tiles and chunkId, update tiles (remove recieved tiles) and
   * return size of tiles.
   */
  void updateTilesAndgetTotalSize(
      long &totalSize, std::vector<std::pair<int, uint16_t>> &updatedTiles,
      std::map<float, std::vector<uint16_t>> &tilesMap, int chunkId,
      ClientNetworkLayer *clientNetworkLayer);

  void updateTilesAndgetTotalSize(
      long &totalSize, std::vector<std::pair<int, uint16_t>> &tilesToUpdate,
      ClientNetworkLayer *clientNetworkLayer);

  float getUtilityDiff(tileNode *&tileN, std::vector<float> &locationVec,
                       float currentArrivalTime, float potentialArrivalTime,
                       uint8_t potentialQuality, int frameIdSt);
  /**
   * @brief This will return list of interleaving fg/bg tiles to request
   *
   * @param backgroundTiles
   * @param chunkIdxs
   * @param bgBw
   * @param fgTiles
   * @param fgBw
   * @return std::string
   */
  std::string
  scheduler(std::vector<std::vector<std::pair<int, uint16_t>>> &backgroundTiles,
            std::vector<int> chunkIdxs, float bgBw,
            std::vector<std::pair<std::pair<int, uint16_t>, uint8_t>> &fgTiles,
            float fgBw);

  /**
   * @brief Return the number of tiles received per chunk
   *        for the next three chunks (i.e. 3-seconds)
   *
   * @param clientNetworkLayer
   * @param frameId
   * @return std::vector<uint16_t>
   */
  std::vector<uint16_t>
  getBufferChunkStatus(ClientNetworkLayer *clientNetworkLayer,
                       uint32_t frameId);

  /**
   * @brief This returns the bitrate to assign for the next 3 chunks,
   *        if chunk is received, it returns 0 as its bitrate.
   *
   * @param bandwidthPredictor
   * @param clientNetworkLayer
   * @param frameId
   * @return std::vector<uint64_t>
   */
  int mpcBitratePerChunk(BandwidthPredictor *bandwidthPredictor,
                         ClientNetworkLayer *clientNetworkLayer,
                         std::map<uint8_t, std::vector<float>> &chunksBitrates,
                         uint32_t frameId, int maxQuality);

  /**
   * @brief this select the quality of the groups per chunk
   *        for the next 3 chunks.
   * @param bitrateAssignmentIdx This contains the target bitrate idx for each
   *                             chunk.
   * @param chunksBitrates       The bitrate for all chunks in all qualities.
   * @param groupsAreaPerChunk   The overlapping area per group of tiles per
   *                             chunk.
   * @return std::vector<std::vector<uint8_t>>
   *         chunk --> group = quality.
   *
   */
  std::map<int, std::vector<uint8_t>> selectIntraGroupQuality(
      int bitrateAssignmentIdx,
      std::map<uint8_t, std::vector<float>> chunksBitrates,
      std::map<int, std::map<uint8_t, float>> groupsAreaPerChunk);

  /**
   * @brief sorted groups by diff in psnr * overlapping area with vp
   *
   * @param groupsArea
   * @param groupCurQualityArray
   * @param chunkId
   * @param qualityTarget
   * @return std::map<float, std::vector<uint8_t>> --> <gain, list of groups>
   *         groups with highest impact first since gain is flipped.
   */
  std::map<float, std::vector<uint8_t>>
  sortedGroupsByMaxPsnrImpact(std::map<uint8_t, float> groupsArea,
                              std::vector<uint8_t> groupCurQualityVec,
                              int chunkId, uint8_t qualityTarget);
  /**
   * @brief calculates the overlapping area per group in each chunk,
   *        for the next 3-sec chunks.
   *
   * @param tilePredictor
   * @param tilesGroups     the group id per tile   [tileId] = groupId
   * @return std::map<int, std::map<uint8_t, float>>
   *         chunk --> group --> overlapping area.
   */
  std::map<int, std::map<uint8_t, float>> areaPerGroup(
      std::map<uint16_t, std::map<float, std::vector<uint16_t>>> areaPerTile,
      uint8_t tilesGroups[]);

  void buildBitrateAssigment(std::vector<uint8_t> assignment,
                             int numOfChunkInHorizon,
                             int numOfPossibleBitrates);

  void readTilesGroups(uint8_t tilesGroups[],
                       std::map<uint8_t, std::vector<uint16_t>> &groupsTiles,
                       uint8_t groupCount[], int numTilesW, int numbTilesH,
                       std::string tilesGroupsFilePath);

  void readChunksBitrates(std::map<uint8_t, std::vector<float>> &chunksBitrates,
                          std::string chunkBitratesFilePath);

  void fillGroupQualityInfo(uint8_t tilesGroups[], int numOfTiles);

  std::vector<std::pair<int, uint16_t>> sortTilesByUtilityAndQuality(
      ClientNetworkLayer *clientNetworkLayer, uint8_t quality,
      std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
      tileNode *headRequest);
};
#endif /* ABRALGORITHM_H_ */
