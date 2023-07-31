/*
 * AbrAlgorithm.cpp
 *
 *  Created on: Jun 6, 2021
 *      Author: eghabash
 */

#include "AbrAlgorithm.h"

#include <float.h>
#include <unistd.h>

#include <chrono>
#include <limits>
#include <thread>

#include "Util.h"
#include "folly/String.h"
#include "glog/logging.h"

#define ABR_FREQ 100

DEFINE_bool(JournalCoraseABR, true, "corase or fine");
DEFINE_string(UtilityCoraseBackgroundStream, "fine",
              "background stream full 360 chunks (coarse),"
              "wide range of small tiles(fine),"
              "or not background stream (off).");
DEFINE_bool(UtilityCoraseForegroundStream, false,
            "true (Dragonfly-coarse),"
            "false (Dragonfly)");

AbrAlgorithm::AbrAlgorithm(std::string tileChunkSizesPath,
                           std::string tileChunksQaulityPath,
                           std::string backgroundDisplacementPath,
                           std::string fullVideoChunkSizePath,
                           std::string fullVideoChunkPSNRPath, size_t window) {
  predictionWindow_ = window * 25;
  std::ifstream infile1(tileChunkSizesPath);
  std::string line;
  while (std::getline(infile1, line)) {
    line.pop_back();
    auto pos = line.find(":");
    std::vector<int> TileIdQualityPair;
    std::vector<uint64_t> tileChunkSizes;
    try {
      folly::split("-", line.substr(0, pos), TileIdQualityPair);
      folly::split(",", line.substr(pos + 2), tileChunkSizes);
      uint8_t qualityIdx = QUALITYMAP_.find(TileIdQualityPair[1])->second;
      if (tileChunkSizePerQuality_.find(qualityIdx) ==
          tileChunkSizePerQuality_.end()) {
        std::map<uint16_t, std::vector<uint64_t>> tileChunksSizesMap;
        tileChunksSizesMap.insert({TileIdQualityPair[0], tileChunkSizes});
        tileChunkSizePerQuality_.insert({qualityIdx, tileChunksSizesMap});
      } else {
        tileChunkSizePerQuality_[qualityIdx].insert(
            {TileIdQualityPair[0], tileChunkSizes});
      }
    } catch (std::invalid_argument &e) {
      LOG(ERROR) << "AbrAlgorithm::AbrAlgorithm(): cannot read line :" << line;
    }
  }

  std::ifstream infile2(tileChunksQaulityPath);
  while (std::getline(infile2, line)) {
    line.pop_back();
    auto pos = line.find(":");
    std::vector<int> TileIdQualityPair;
    std::vector<float> tileChunkQuality;
    try {
      folly::split("-", line.substr(0, pos), TileIdQualityPair);
      folly::split(",", line.substr(pos + 2), tileChunkQuality);
      uint8_t qualityIdx = QUALITYMAP_.find(TileIdQualityPair[1])->second;
      if (TileIdQualityPair[1] == 50 || TileIdQualityPair[1] == 17) {
        continue;
      }
      if (tileChunkPSNRPerQuality_.find(qualityIdx) ==
          tileChunkPSNRPerQuality_.end()) {
        std::map<uint16_t, std::vector<float>> tileChunksQualityMap;
        tileChunksQualityMap.insert({TileIdQualityPair[0], tileChunkQuality});
        tileChunkPSNRPerQuality_.insert({qualityIdx, tileChunksQualityMap});
      } else {
        tileChunkPSNRPerQuality_[qualityIdx].insert(
            {TileIdQualityPair[0], tileChunkQuality});
      }
    } catch (std::invalid_argument &e) {
      LOG(ERROR) << "AbrAlgorithm::AbrAlgorithm(): cannot read line :" << line;
    }
  }

  numberOfQualities_ = tileChunkPSNRPerQuality_.size();

  std::ifstream infile3(backgroundDisplacementPath);
  while (std::getline(infile3, line)) {
    std::vector<float> displacements;
    folly::split(",", line, displacements);
    backgroundDisplacement_.push_back({{displacements[0], displacements[1]},
                                       {displacements[2], displacements[3]}});
  }

  std::ifstream infile4(fullVideoChunkSizePath);
  while (std::getline(infile4, line)) {
    line.pop_back();
    folly::split(",", line.substr(1), fullVideoChunksSizes_);
  }

  std::ifstream infile5(fullVideoChunkPSNRPath);
  while (std::getline(infile5, line)) {
    line.pop_back();
    folly::split(",", line.substr(1), fullVideoChunksPSNR_);
  }
}

void AbrAlgorithm::getTileSetSizePerQuality(
    std::map<int, std::map<uint8_t, std::vector<uint64_t>>>
        &frameIdSetQualitySizeSumToReturn,
    std::map<int, std::map<uint8_t, std::vector<uint16_t>>>
        &tilesRequestToReturn,
    TilePredictor *tilePredictor, ClientNetworkLayer *clientNetworkLayer,
    uint32_t frameIdToRender, uint8_t numOfQualities, uint8_t &numOfClasses) {
  std::set<std::string> tilesInPrevSets;

  // get all tiles need per frame in each class.
  // frame[i] needs M tiles for Class C1, and N tiles for Class C2
  // note that M ∩ N = Φ, tiles in class M cannot be in N (not duplicate tiles)
  auto tileClassesOfFutureFrames =
      tilePredictor->getPredictedTilesFlareLR({{100, 100}, {120, 120}});
  if (tileClassesOfFutureFrames.size() == 0) {
    return;
  }

  for (auto const &tileClassesSingleFrame :
       tileClassesOfFutureFrames) {  // A- per frame
    auto frameId = tileClassesSingleFrame.first;
    if (frameId < frameIdToRender) {
      continue;
    }
    auto chunkId = ((frameId - 1) / 25);

    if (frameIdSetQualitySizeSumToReturn.find(frameId) ==
        frameIdSetQualitySizeSumToReturn.end()) {
      std::map<uint8_t, std::vector<uint64_t>> setQualitySizeSum;
      frameIdSetQualitySizeSumToReturn.insert(
          std::make_pair(frameId, setQualitySizeSum));
    }
    if (tilesRequestToReturn.find(chunkId) == tilesRequestToReturn.end()) {
      std::map<uint8_t, std::vector<uint16_t>> tilesPerClassRank;
      tilesRequestToReturn.insert({chunkId, tilesPerClassRank});
    }
    auto &tilesPerClassRank = tilesRequestToReturn.find(chunkId)->second;
    bool frameHasTiles = false;

    auto &classQualitySizeSum =
        frameIdSetQualitySizeSumToReturn.find(frameId)->second;
    for (auto const &SetOftilesInClass :
         tileClassesSingleFrame.second) {  // B- tiles per class

      auto classRank = SetOftilesInClass.first;

      if (tilesPerClassRank.find(classRank) == tilesPerClassRank.end()) {
        std::vector<uint16_t> tiles;
        tilesPerClassRank.insert({classRank, tiles});
      }
      auto &tilesToReq = tilesPerClassRank.find(classRank)->second;
      //============================================
      // This line to determine how many classes to go over next.
      numOfClasses = numOfClasses < classRank ? classRank : numOfClasses;
      //============================================

      std::string tilesLog =
          std::to_string(frameId) + ":" + std::to_string(classRank) + ":";
      bool classHasTiles = false;
      // first tile in set.
      // We have viewport set, and multiple out of sight sets.
      if (classQualitySizeSum.find(classRank) == classQualitySizeSum.end()) {
        std::vector<uint64_t> qualitySizeSumVec(numOfQualities);
        classQualitySizeSum.insert(
            std::make_pair(classRank, qualitySizeSumVec));
      }
      std::unordered_set<std::string> tilesInSet;
      auto &qualitySizeSumVec = classQualitySizeSum.find(classRank)->second;
      for (uint8_t qualityIdx = 0; qualityIdx < numOfQualities;
           qualityIdx++) {  // C- per quality

        for (auto const &tile : SetOftilesInClass.second) {  // D- per tile
          // if tile chunk is recevied then do not count it.
          std::string tileKey =
              std::to_string(chunkId) + "_" + std::to_string(tile);
          // if the tile already included in earlier frame of higher rank
          // class, then skip (no duplicates)
          if (tilesInPrevSets.find(tileKey) != tilesInPrevSets.end()) {
            continue;
          }
          tilesInSet.insert(tileKey);
          if (clientNetworkLayer->isReceived(chunkId + 1, tile) == -1) {
            if (qualityIdx == 0) {
              if (tilesRequestToReturn.find(classRank) ==
                  tilesRequestToReturn.end()) {
                tilesRequestToReturn.insert({classRank, {}});
              }
              // tilesRequestToReturn.find(classRank)->second.push_back(
              //     {chunkId, tile});
              tilesToReq.push_back(tile);
              tilesLog += std::to_string(tile) + ",";
              classHasTiles = true;
              frameHasTiles = true;
            }

            qualitySizeSumVec[qualityIdx] +=
                tileChunkSizePerQuality_.find(qualityIdx + 1)
                    ->second.find(tile)
                    ->second[chunkId];
          }
        }
      }
      if (classHasTiles) {
        // if this class has tiles, then add tiles to request
        tilesLog.pop_back();
        // tilesRequestToReturn.push_back(tiles);
        for (auto tile : tilesInSet) {
          tilesInPrevSets.insert(tile);
        }

        if (VLOG_IS_ON(1)) {
          VLOG(1) << "FrameId[" << static_cast<int>(frameId) << "] - "
                  << "set[" << static_cast<int>(classRank) << "] : " << tilesLog
                  << std::endl;
        }
      } else {
        classQualitySizeSum.erase(classRank);
      }
    }
    if (!frameHasTiles) {
      frameIdSetQualitySizeSumToReturn.erase(frameId);
    }
  }
}

std::map<int, int> AbrAlgorithm::getQualityIdx(
    std::map<int, std::map<uint8_t, std::vector<uint64_t>>>
        &frameIdSetQualitySizeSum,
    std::vector<std::string> qualitiesAssignments, uint32_t frameIdToRender,
    uint8_t numOfQualities, float predictedBw, float baseTime,
    std::string model) {
  // find the quality that we can get all tiles within each frame before
  // frame deadline (avoid rebuffering at all costs).

  // Constraints:
  // 1- all tiles for frame[i] must arrive before tiles of frame[i+1]
  // 2- all tiles in one class will be of the same quality across frames.
  // 3- quality of high class must be equal or greater than low classes.
  // For instance if you have 2 classes, and 3 qualities, then possible
  // quality assignment would be given that class1 has higher rank than
  // class2, and 3 being the highest quality):
  // class_1  class_2
  //    3       3
  //    3       2
  //    3       1
  //    2       2
  //    2       1
  //    1       1
  // current video time is the time of the last played frame + time
  // passed since last frame was rendered.
  float currentVideoTime =
      (((frameIdToRender - 1) * 40.0) + Util::getTimePassedSinceLastFrame()) /
      1e3;  // current video time.

  std::vector<int> sortedFrameIds;
  for (auto const &tileClassesSingleFrame : frameIdSetQualitySizeSum) {
    sortedFrameIds.push_back(tileClassesSingleFrame.first);
  }

  currentVideoTime += baseTime;
  std::map<int, int> chunkQualityAssignment;
  float timeCascade = currentVideoTime;
  int qualityIdx = 0;
  int frameIdx = 0;
  int currChunk = -1;
  int maxQ = -1;
  for (; qualityIdx < (int)qualitiesAssignments.size(); qualityIdx++) {
    bool qualityFound = false;
    int chunkId = -1;
    int frameId = -1;
    auto solution = qualitiesAssignments[qualityIdx];
    float downloadTime = 0;
    // try get all frames within the chunk with this quality(solution)
    for (frameId = frameIdx; frameId < (int)sortedFrameIds.size(); frameId++) {
      chunkId = int((sortedFrameIds[frameId] - 1) / 25);
      if (currChunk == -1) {
        currChunk = chunkId;
      }
      if (currChunk != chunkId) {
        qualityFound = true;
        break;
      }
      uint64_t totalFrameTileSizes = 0;
      for (auto const &tileClass :
           frameIdSetQualitySizeSum[sortedFrameIds[frameId]]) {
        int qualityIdx = int(solution[tileClass.first * 2]) - 49;
        totalFrameTileSizes += tileClass.second[qualityIdx];
      }
      auto dt = totalFrameTileSizes / predictedBw;
      auto frameTilesDeadline = ((sortedFrameIds[frameId] - 1.0) * 40.0) / 1e3;
      if (dt + timeCascade < frameTilesDeadline) {
        downloadTime += dt;
      } else {
        break;
      }
    }
    if (qualityFound) {
      chunkQualityAssignment.insert({currChunk, qualityIdx});
      qualityIdx = 0;
      frameIdx = frameId;
      currChunk = chunkId;
      timeCascade += downloadTime;
    } else if (!qualityFound &&
               (frameId == (int)sortedFrameIds.size() &&
                (qualityIdx + 1) !=
                    (int)qualitiesAssignments
                        .size())) {  // found the quality of the last chunk.
      chunkQualityAssignment.insert({currChunk, qualityIdx});
      break;
    }

    else if (!qualityFound &&
             (qualityIdx + 1) ==
                 (int)qualitiesAssignments
                     .size()) {  // no assignment is working.
      // get tiles for the current chunk with lowest quality.
      for (; frameIdx < (int)sortedFrameIds.size(); frameIdx++) {
        chunkId = int((sortedFrameIds[frameIdx] - 1) / 25);
        if (currChunk != chunkId) {
          break;
        }
        uint64_t totalFrameTileSizes = 0;
        for (auto const &tileClass :
             frameIdSetQualitySizeSum[sortedFrameIds[frameIdx]]) {
          int qualityIdx = int(solution[tileClass.first * 2]) - 49;
          totalFrameTileSizes += tileClass.second[qualityIdx];
        }
        auto dt = totalFrameTileSizes / predictedBw;
        timeCascade += dt;
      }
      chunkQualityAssignment.insert({currChunk, qualityIdx});
      qualityIdx = 0;
      if (frameIdx == (int)sortedFrameIds.size()) {
        break;
      }
      currChunk = int((sortedFrameIds[frameIdx] - 1) / 25);
    }
  }
  if ("Flare" == model) {
    for (auto &chunkQassignment : chunkQualityAssignment) {
      maxQ = std::max(maxQ, chunkQassignment.second);
    }
    for (auto &chunkQassignment : chunkQualityAssignment) {
      chunkQualityAssignment[chunkQassignment.first] = maxQ;
    }
  }

  return chunkQualityAssignment;
}

void AbrAlgorithm::getTileSetSizePerQualityJournal(
    std::map<int, std::map<uint8_t, std::vector<uint64_t>>>
        &frameIdSetQualitySizeSumToReturn,
    std::map<uint8_t, std::vector<std::pair<int, uint16_t>>>
        &tilesRequestToReturn,
    TilePredictor *tilePredictor, ClientNetworkLayer *clientNetworkLayer,
    uint32_t frameIdToRender, uint8_t numOfQualities, uint8_t &numOfClasses,
    int stChunk) {
  std::set<std::string> tilesInPrevSets;

  // get all tiles need per frame in each class.
  // frame[i] needs M tiles for Class C1, and N tiles for Class C2
  // note that M ∩ N = Φ, tiles in class M cannot be in N (not duplicate tiles)
  auto tileClassesOfFutureFrames =
      tilePredictor->getPredictedTilesFlareLR({{100, 100}});
  if (tileClassesOfFutureFrames.size() == 0) {
    return;
  }

  for (auto const &tileClassesSingleFrame :
       tileClassesOfFutureFrames) {  // A- per frame
    auto frameId = tileClassesSingleFrame.first;
    if (frameId < frameIdToRender) {
      continue;
    }
    auto chunkId = ((frameId - 1) / 25);
    if (chunkId <= stChunk) {
      continue;
    }
    if (frameIdSetQualitySizeSumToReturn.find(frameId) ==
        frameIdSetQualitySizeSumToReturn.end()) {
      std::map<uint8_t, std::vector<uint64_t>> setQualitySizeSum;
      frameIdSetQualitySizeSumToReturn.insert(
          std::make_pair(frameId, setQualitySizeSum));
    }
    bool frameHasTiles = false;

    auto &classQualitySizeSum =
        frameIdSetQualitySizeSumToReturn.find(frameId)->second;
    for (auto const &SetOftilesInClass :
         tileClassesSingleFrame.second) {  // B- tiles per class

      auto classRank = SetOftilesInClass.first;

      //============================================
      // This line to determine how many classes to go over next.
      numOfClasses = numOfClasses < classRank ? classRank : numOfClasses;
      //============================================

      std::string tiles =
          std::to_string(frameId) + ":" + std::to_string(classRank) + ":";
      bool classHasTiles = false;
      // first tile in set.
      // We have viewport set, and multiple out of sight sets.
      if (classQualitySizeSum.find(classRank) == classQualitySizeSum.end()) {
        std::vector<uint64_t> qualitySizeSumVec(numOfQualities);
        classQualitySizeSum.insert(
            std::make_pair(classRank, qualitySizeSumVec));
      }
      std::unordered_set<std::string> tilesInSet;
      auto &qualitySizeSumVec = classQualitySizeSum.find(classRank)->second;
      for (uint8_t qualityIdx = 1; qualityIdx < numOfQualities;
           qualityIdx++) {  // C- per quality

        for (auto const &tile : SetOftilesInClass.second) {  // D- per tile
          // if tile chunk is recevied then do not count it.
          std::string tileKey =
              std::to_string(chunkId) + "_" + std::to_string(tile);
          // if the tile already included in earlier frame of higher rank
          // class, then skip (no duplicates)
          if (tilesInPrevSets.find(tileKey) != tilesInPrevSets.end()) {
            continue;
          }
          tilesInSet.insert(tileKey);
          if (clientNetworkLayer->isReceived(chunkId + 1, tile) ==
              -1) {                 // tile is not received.
            if (qualityIdx == 1) {  // do some initializations
              if (tilesRequestToReturn.find(classRank) ==
                  tilesRequestToReturn.end()) {
                tilesRequestToReturn.insert({classRank, {}});
              }
              tilesRequestToReturn.find(classRank)->second.push_back(
                  {chunkId, tile});
              tiles += std::to_string(tile) + ",";
              classHasTiles = true;
              frameHasTiles = true;
            }

            qualitySizeSumVec[qualityIdx] +=
                tileChunkSizePerQuality_.find(qualityIdx + 1)
                    ->second.find(tile)
                    ->second[chunkId];
          }
        }
      }
      if (classHasTiles) {
        // if this class has tiles, then add tiles to request
        tiles.pop_back();
        // tilesRequestToReturn.push_back(tiles);
        for (auto tile : tilesInSet) {
          tilesInPrevSets.insert(tile);
        }

        if (VLOG_IS_ON(1)) {
          VLOG(1) << "FrameId[" << static_cast<int>(frameId) << "] - "
                  << "set[" << static_cast<int>(classRank) << "] : " << tiles
                  << std::endl;
        }
      } else {
        classQualitySizeSum.erase(classRank);
      }
    }
    if (!frameHasTiles) {
      frameIdSetQualitySizeSumToReturn.erase(frameId);
    }
  }
}

void AbrAlgorithm::journalAbr(AbrAlgorithm *abrAlgorithm,
                              TilePredictor *tilePredictor,
                              BandwidthPredictor *bandwidthPredictor,
                              ClientNetworkLayer *clientNetworkLayer,
                              VideoPlayer *videoPlayer) {
  // every 100ms, update tile list.
  long stime = Util::getTime();
  int backgroundBufferSize = 3;
  int missBgSt, missBgEn;
  int lastForegroundChunkRecieved = -1;
  uint8_t numOfQualities = abrAlgorithm->getNumberOfQualities();
  uint8_t numOfClasses = 0;
  // This set will contain all tiles in prev sets (to contain duplicates)
  // tilechunk_tileIdx

  std::map<int, std::map<uint8_t, std::vector<uint64_t>>>
      frameIdSetQualitySizeSum;
  std::map<uint8_t, std::vector<std::pair<int, uint16_t>>> tilesRequest;

  std::pair<int, uint16_t> chunkTileAwaited(-1, 0);

  int chunkId;
  int frameIdToRender;
  while (true) {
    // if foreground chunk is not fully received.
    // or if the foreground buffer is full. Then pause ABR.
    if (FLAGS_JournalCoraseABR) {
      while (true) {
        frameIdToRender = videoPlayer->getFrameToRenderId();
        chunkId = (frameIdToRender - 1) / 25;

        if (chunkTileAwaited.first == -1 || chunkTileAwaited.first < chunkId) {
          break;
        }
        // this should check the bg buffer as well.
        if (clientNetworkLayer->isReceived(chunkTileAwaited.first + 1,
                                           chunkTileAwaited.second) != -1) {
          lastForegroundChunkRecieved = chunkTileAwaited.first;
          if (lastForegroundChunkRecieved - chunkId <
              ((int)abrAlgorithm->predictionWindow_ / 25)) {
            break;
          }
        }
        Util::sleep(stime, 10);
        stime += 10;
      }
    } else {
      frameIdToRender = videoPlayer->getFrameToRenderId();
      chunkId = (frameIdToRender - 1) / 25;
    }

    missBgSt = std::ceil((frameIdToRender - 1) / 25.0);
    missBgEn = missBgSt + backgroundBufferSize;

    // next chunk Id to fetch.
    // int stChunk = std::max(chunkId, chunkTileAwaited.first + 1);
    // stChunk = -1; // FLAGS_JournalCoraseABR ? stChunk : -1;
    // this will return the size per class per frame.
    // along with number of class = max(class rank),
    numOfClasses = 0;
    abrAlgorithm->getTileSetSizePerQualityJournal(
        frameIdSetQualitySizeSum, tilesRequest, tilePredictor,
        clientNetworkLayer, frameIdToRender, numOfQualities, numOfClasses,
        FLAGS_JournalCoraseABR ? chunkTileAwaited.first : -1);
    numOfClasses++;
    float predictedBw =
        (bandwidthPredictor->getMpcBandwidthPrediction());  // Bytes Per Second
    std::string bgTiles = "";
    float bgDownloadTime = 0;
    for (auto idx = missBgSt; idx < missBgEn; idx++) {
      if (clientNetworkLayer->isReceived(idx + 1, 0) == -1) {
        bgTiles += std::to_string(idx) +
                   "_0_0,";  // tile id is zero, and quality as well.s
        bgDownloadTime +=
            abrAlgorithm->fullVideoChunksSizes_[idx] / predictedBw;
      }
    }

    float baseTime =
        bgDownloadTime;  // change this based on the BG buffer size.

    auto qualityAssignments = abrAlgorithm->getPossibleQualityAssignment(
        numOfQualities, numOfClasses);

    std::vector<std::string> qualityAssignmentsVec;
    for (int quality = numOfQualities; quality > 1; quality--) {
      // for all possible solutions
      for (auto const &solution : qualityAssignments.find(quality)->second) {
        qualityAssignmentsVec.push_back(solution);
      }
    }

    auto qualityMap = abrAlgorithm->getQualityIdx(
        frameIdSetQualitySizeSum, qualityAssignmentsVec, frameIdToRender,
        numOfQualities, predictedBw, baseTime, "Flare");

    // in coarse model we make a decision every chunk,
    // so we must await the arrival of the last tile in the chunk,
    // before we make new request.
    std::pair<int, uint16_t> chunkToWait(-1, 0);

    std::string req = "Tiles\n" + bgTiles;
    for (auto &classTilesPair : tilesRequest) {
      auto &classRank = classTilesPair.first;
      auto &tilesInClass = classTilesPair.second;
      for (auto &tilePair : tilesInClass) {
        // if it is coarse do not correct chunk request.
        if (FLAGS_JournalCoraseABR) {
          if (chunkTileAwaited.first == tilePair.first ||
              tilePair.first < chunkToWait.first) {
            continue;
          }
          // this is assuming only one class of tiles.
          // we try to find what tile-chunk to await before calling abr again.
          // first chunkId then tileId.
          if (chunkToWait.first == -1) {
            chunkToWait.first = tilePair.first;
          }
          if (tilePair.first == chunkToWait.first) {
            chunkToWait.second = tilePair.second;
          }
        }
        auto &solution = qualityAssignmentsVec[qualityMap[tilePair.first]];
        int quality = int(solution[classRank * 2]) - 48;
        req += std::to_string(tilePair.first) + "_" +
               std::to_string(tilePair.second) + "_" + std::to_string(quality) +
               ",";
      }
    }

    // this is the tile-chunk to await before abr
    chunkTileAwaited = chunkToWait.first == -1 ? chunkTileAwaited : chunkToWait;
    req.pop_back();
    // std::cout << frameIdToRender << ":" << req << "\n======\n";
    req += "\nQuality\n" + std::to_string(0);
    clientNetworkLayer->setRequest(req);
    tilesRequest.clear();
    frameIdSetQualitySizeSum.clear();
    Util::sleep(stime, ABR_FREQ);
    stime += 100;
  }
}

// ToDo fix how flare send tiles, it now needs to be chunkidx_tileidx_quality
// try and be consistent with how utility does it.
void AbrAlgorithm::flareAbr(AbrAlgorithm *abrAlgorithm,
                            TilePredictor *tilePredictor,
                            BandwidthPredictor *bandwidthPredictor,
                            ClientNetworkLayer *clientNetworkLayer,
                            VideoPlayer *videoPlayer) {
  // every 100ms, update tile list.
  long stime = Util::getTime();
  float videoTime = 0;

  uint8_t numOfQualities = abrAlgorithm->getNumberOfQualities();
  uint8_t numOfClasses = 0;
  // This set will contain all tiles in prev sets (to contain duplicates)
  // tilechunk_tileIdx

  std::map<int, std::map<uint8_t, std::vector<uint64_t>>>
      frameIdSetQualitySizeSum;
  std::map<int, std::map<uint8_t, std::vector<uint16_t>>> tilesRequest;
  while (true) {
    // get the predicted tiles every ABR_FREQ(100ms).
    // we will have mutliple sets (e.g. viewport tiles, viewport edge tiles ,
    // further tiles, rest of tiles)

    // all frameId must be >= frameIdToRender, to ensure we don't request data
    // for old frames.
    auto frameIdToRender = videoPlayer->getFrameToRenderId();
    // this will return the size per class per frame.
    // along with number of class = max(class rank),
    numOfClasses = 0;
    abrAlgorithm->getTileSetSizePerQuality(
        frameIdSetQualitySizeSum, tilesRequest, tilePredictor,
        clientNetworkLayer, frameIdToRender, numOfQualities, numOfClasses);
    numOfClasses++;
    float predictedBw =
        (bandwidthPredictor->getMpcBandwidthPrediction());  // Bytes Per Second

    float baseTime = 0;

    auto qualityAssignments = abrAlgorithm->getPossibleQualityAssignment(
        numOfQualities, numOfClasses);
    std::vector<std::string> qualityAssignmentsVec;
    for (int quality = numOfQualities; quality > 0; quality--) {
      // for all possible solutions
      for (auto const &solution : qualityAssignments.find(quality)->second) {
        qualityAssignmentsVec.push_back(solution);
      }
    }
    auto qualityMap = abrAlgorithm->getQualityIdx(
        frameIdSetQualitySizeSum, qualityAssignmentsVec, frameIdToRender,
        numOfQualities, predictedBw, baseTime, "Flare");

    std::string req = "Tiles\n";  //+ urgentTileRequestAndSize.first;
    // chunkId --> classRank --> <tiles>
    for (auto &tilesInChunkPair : tilesRequest) {
      auto &chunkId = tilesInChunkPair.first;
      auto &tilesInChunk = tilesInChunkPair.second;
      for (auto &tilesClassPair : tilesInChunk) {
        auto &tileClass = tilesClassPair.first;
        for (auto &tile : tilesClassPair.second) {
          auto &solution = qualityAssignmentsVec[qualityMap[chunkId]];
          int quality = int(solution[tileClass * 2]) - 48;
          req += std::to_string(chunkId) + "_" + std::to_string(tile) + "_" +
                 std::to_string(quality) + ",";
        }
      }
    }
    req.pop_back();

    req += "\nQuality\n" + std::to_string(0);
    // std::cout << frameIdToRender << ":" << req << std::endl;
    clientNetworkLayer->setRequest(req);
    tilesRequest.clear();
    frameIdSetQualitySizeSum.clear();
    Util::sleep(stime, ABR_FREQ);
    videoTime += (Util::getTime() - stime);
    stime += 100;
  }
}

std::pair<std::string, int> AbrAlgorithm::buildBackgroundUrgentTilesRequest(
    uint32_t frameIdToRender, ClientNetworkLayer *clientNetworkLayer,
    std::map<float, std::vector<uint16_t>> &urgetTiles) {
  // Objective: get all urgent tiles in lowest quality for the next two seconds

  // find the chunk ids correspond to the next two seconds.
  int stChunkId = (((frameIdToRender)-1) / 25);
  int enChunkId = stChunkId;  //+ 1;
  if ((frameIdToRender - 1) % 25 != 0) {
    enChunkId++;
  }

  std::string finalRequest = "";

  int size = 0;
  for (auto idx = stChunkId; idx <= enChunkId; idx++) {
    for (auto &tileSet : urgetTiles) {
      if (tileSet.first == 1) {
        continue;
      }
      for (auto &tile : tileSet.second) {
        if (clientNetworkLayer->isReceived(idx + 1, tile) == -1) {
          // the one at the end corresponds to quality; 1 == lowest quality
          finalRequest +=
              std::to_string(idx) + "_" + std::to_string(tile) + "_1,";
          size +=
              tileChunkSizePerQuality_.find(1)->second.find(tile)->second[idx];
        }
      }
    }
  }

  std::pair<std::string, int> reqSize = {finalRequest, size};
  return reqSize;
}

std::vector<std::pair<int, uint16_t>>
AbrAlgorithm::getBackgroundLessUrgentTilesInfo(
    uint32_t frameIdToRender, ClientNetworkLayer *clientNetworkLayer,
    std::map<float, std::vector<uint16_t>> &urgetTiles) {
  // Less urgent chunk corresponds to future-seconds 2-4.
  // So, it starts at sec =  urgent_chunkId + 2
  int stChunkId = (((frameIdToRender)-1) / 25) + 1;
  int enChunkId = stChunkId + 1;
  if ((frameIdToRender - 1) % 25 != 0) {
    stChunkId++;
  }
  // tileId, tile size --> size will be used by tiles scheduler.
  std::vector<std::pair<int, uint16_t>> tilesInfo;

  for (auto chunkIdx = stChunkId; chunkIdx <= enChunkId; chunkIdx++) {
    for (auto &tileSet : urgetTiles) {
      if (tileSet.first == 1) {
        continue;
      }
      for (auto &tile : tileSet.second) {
        if (clientNetworkLayer->isReceived(chunkIdx + 1, tile) == -1) {
          std::string tileKey =
              std::to_string(chunkIdx) + "_" + std::to_string(tile);
          tilesInfo.push_back({chunkIdx, tile});
        }
      }
    }
  }

  return tilesInfo;
}

void AbrAlgorithm::updateTilesAndgetTotalSize(
    long &totalSize, std::vector<std::pair<int, uint16_t>> &updatedTiles,
    std::map<float, std::vector<uint16_t>> &tilesMap, int chunkId,
    ClientNetworkLayer *clientNetworkLayer) {
  updatedTiles.clear();
  totalSize = 0;
  for (auto &fracTiles : tilesMap) {
    if (fracTiles.first == 1) {
      continue;
    }
    for (auto &tile : fracTiles.second) {
      if (clientNetworkLayer->isReceived(chunkId + 1, tile) > 0) {
        // std::cout << "tile:" << tile << "_" << (chunkId + 1)
        //      << " --> not received!\n";
        continue;
      }
      totalSize += tileChunkSizePerQuality_[1][tile][chunkId];
      updatedTiles.push_back({chunkId, tile});
    }
  }
}

void AbrAlgorithm::updateTilesAndgetTotalSize(
    long &totalSize, std::vector<std::pair<int, uint16_t>> &tilesToUpdate,
    ClientNetworkLayer *clientNetworkLayer) {
  totalSize = 0;
  std::vector<std::pair<int, uint16_t>> updatedTiles;
  for (auto &tileInfo : tilesToUpdate) {
    if (clientNetworkLayer->isReceived(tileInfo.first + 1, tileInfo.second) >
        0) {
      continue;
    }
    totalSize += tileChunkSizePerQuality_[1][tileInfo.second][tileInfo.first];
    updatedTiles.push_back({tileInfo.first, tileInfo.second});
  }
  tilesToUpdate = updatedTiles;
}

std::string AbrAlgorithm::scheduler(
    std::vector<std::vector<std::pair<int, uint16_t>>> &backgroundTiles,
    std::vector<int> chunkIdxs, float bgBw,
    std::vector<std::pair<std::pair<int, uint16_t>, uint8_t>> &fgTiles,
    float fgBw) {
  std::string request;
  float totalBw = bgBw + fgBw;
  float fgMsShare = (fgBw / totalBw) * 100.0;
  float bgMsShare = 100 - fgMsShare;
  int bgTileIdx = 0;
  int bgchunkIdx = chunkIdxs[0];
  float fgMsTarget = fgMsShare;
  float bgMsTarget;
  for (int fgIdx = 0; fgIdx < (int)fgTiles.size(); fgIdx++) {
    // <tileId, chunkId>
    auto &fgTileInfo = fgTiles[fgIdx].first;
    auto tileQuality = fgTiles[fgIdx].second;
    float dtime = (tileChunkSizePerQuality_[tileQuality][fgTileInfo.second]
                                           [fgTileInfo.first] *
                   1e3) /
                  totalBw;

    std::string fgTile = std::to_string(fgTileInfo.first) + "_" +
                         std::to_string(fgTileInfo.second) + "_" +
                         std::to_string(tileQuality) + ",";
    if (fgMsTarget - dtime >= 0) {  // fg tile fits in the FG slot
      request += fgTile;
      fgMsTarget -= dtime;
    } else {  // fg tile spills to bg slot

      // by how much it overspilled.
      float fgExtraMs = std::abs(fgMsTarget - dtime) / fgMsShare;
      // reconfigure BG share accordingly.
      bgMsTarget = (1 + fgExtraMs) * bgMsShare;
      // fill the BG tiles
      for (; bgchunkIdx <= chunkIdxs[chunkIdxs.size() - 1]; bgchunkIdx++) {
        for (; bgTileIdx < (int)backgroundTiles[bgchunkIdx].size();
             bgTileIdx++) {
          auto &bgTileInfo = backgroundTiles[bgchunkIdx][bgTileIdx];
          uint64_t tileSize =
              FLAGS_UtilityCoraseBackgroundStream == "coarse"
                  ? fullVideoChunksSizes_[bgTileInfo.first]
                  : tileChunkSizePerQuality_[1][bgTileInfo.second]
                                            [bgTileInfo.first];
          float dtime = (tileSize * 1e3) / totalBw;
          request += std::to_string(bgTileInfo.first) + "_" +
                     std::to_string(bgTileInfo.second);
          request +=
              FLAGS_UtilityCoraseBackgroundStream == "coarse" ? "_0," : "_1,";
          bgMsTarget -= dtime;
          if (bgMsTarget < 0) {  // bg tile spills to fg slot
            request += fgTile;
            float bgExtraMs = std::abs(bgMsTarget) / bgMsShare;
            fgMsTarget = (1 + bgExtraMs) * fgMsShare;
            break;
          }
        }
        if (bgTileIdx < (int)backgroundTiles[bgchunkIdx].size()) {
          break;
        }
        bgTileIdx = 0;
      }
      if (bgMsTarget >= 0) {  // all bg tiles are scheduled.
        request += fgTile;
        fgMsTarget = 100;
      }
    }
  }

  return request;
}

void AbrAlgorithm::utilityAbr(AbrAlgorithm *abrAlgorithm,
                              TilePredictor *tilePredictor,
                              BandwidthPredictor *bandwidthPredictor,
                              ClientNetworkLayer *clientNetworkLayer,
                              VideoPlayer *videoPlayer) {
  // every 100ms, update tile list.
  long stime = Util::getTime();
  float videoTime = 0;

  // This set will contain all tiles in prev sets (to contain duplicates)
  // tilechunk_tileIdx
  std::vector<std::pair<float, float>> predictedCorr;
  std::vector<std::pair<int, int>> vpResolutions = {{100, 100}};
  int chunkId;

  const int backgroundHorizonInSec = 3;
  std::vector<std::vector<std::pair<int, uint16_t>>> backgroundTiles(
      backgroundHorizonInSec);
  std::vector<long> backgroundTilesSizes(backgroundHorizonInSec);
  std::set<int> groundTruth;

  std::pair<int, uint16_t> chunkTileAwaited(-1, 0);

  // These variables will be used if FLAGS_UtilityCoraseForegroundStream is true
  int coarseForgroundChunkIdToRequest = 0;
  std::vector<std::pair<std::pair<int, uint16_t>, uint8_t>>
      coarseForegroundTiles;

  uint8_t backgroundQualityIdx =
      FLAGS_UtilityCoraseBackgroundStream == "fine" ? 1 : 0;

  while (true) {
    auto frameIdToRender = videoPlayer->getFrameToRenderId();
    chunkId = (frameIdToRender - 1) / 25;
    if (chunkId == 60) {
      // video ended!
      break;
    }

    if (FLAGS_UtilityCoraseForegroundStream) {
      // check if the last request foreground tile-chunk is received or passed
      // its deadline. If so, clear the old foreground request and update the
      // awaited tile-chunk.
      if ((clientNetworkLayer->isReceived(chunkTileAwaited.first + 1,
                                          chunkTileAwaited.second) >
               backgroundQualityIdx ||
           chunkTileAwaited.first < chunkId) &&
          (coarseForgroundChunkIdToRequest - chunkId < 2)) {
        coarseForegroundTiles.clear();
        if (clientNetworkLayer->isReceived(chunkTileAwaited.first + 1,
                                           chunkTileAwaited.second) >
            backgroundQualityIdx) {
          coarseForgroundChunkIdToRequest++;
        }
        chunkTileAwaited.first = -1;  // we are not waiting on foreground tiles.
      }

      coarseForgroundChunkIdToRequest =
          std::max(coarseForgroundChunkIdToRequest, chunkId);
    }

    // START background tiles block
    if (FLAGS_UtilityCoraseBackgroundStream == "fine") {
      // if the background stream is fine grained,
      // determine background tiles based on displacement.

      for (auto idx = chunkId; idx < chunkId + backgroundHorizonInSec; idx++) {
        if (idx >= 60) {
          backgroundTiles[idx - chunkId] = {};
          continue;
        }

        std::map<float, std::vector<uint16_t>> tempBgTiles;

        if (idx != chunkId) {
          tilePredictor->getBackgroundTiles(
              tempBgTiles, abrAlgorithm->backgroundDisplacement_[idx], 0);
          abrAlgorithm->updateTilesAndgetTotalSize(
              std::ref(backgroundTilesSizes[idx - chunkId]),
              std::ref(backgroundTiles[idx - chunkId]), tempBgTiles, idx,
              clientNetworkLayer);
        } else if (idx == chunkId &&
                   groundTruth.find(chunkId) == groundTruth.end()) {
          tilePredictor->getBackgroundTiles(
              tempBgTiles, abrAlgorithm->backgroundDisplacement_[idx],
              (chunkId * 25) + 1);
          abrAlgorithm->updateTilesAndgetTotalSize(
              std::ref(backgroundTilesSizes[idx - chunkId]),
              std::ref(backgroundTiles[idx - chunkId]), tempBgTiles, idx,
              clientNetworkLayer);
          std::string log = "";
          for (auto v : tempBgTiles) {
            if (v.first != 1) {
              for (auto x : v.second) {
                log += std::to_string(x) + ", ";
              }
            }
          }
          log.pop_back();
          groundTruth.insert(chunkId);
        }

        else {
          // get the size of the background tiles, and remove tiles that have
          // been
          // recieved already.
          abrAlgorithm->updateTilesAndgetTotalSize(
              std::ref(backgroundTilesSizes[idx - chunkId]),
              std::ref(backgroundTiles[idx - chunkId]), clientNetworkLayer);
        }
      }
    } else if (FLAGS_UtilityCoraseBackgroundStream == "coarse") {
      // if the background stream is coarse grained,
      // determine background chunks to fetch.
      for (auto idx = chunkId; idx < chunkId + backgroundHorizonInSec; idx++) {
        backgroundTiles[idx - chunkId] = {};
        if (idx >= 60) {
          continue;
        }
        if (clientNetworkLayer->isReceived(idx + 1, 0) == 0) {
          backgroundTilesSizes[idx - chunkId] = 0;
          continue;
        }
        backgroundTilesSizes[idx - chunkId] =
            abrAlgorithm->fullVideoChunksSizes_[idx];
        backgroundTiles[idx - chunkId].push_back({idx, 0});
      }
    } else {
      // if background stream is turned off (i.e., no background tile/chunks to
      // fetch)
      // create empty background request.
      backgroundTilesSizes = {0, 0, 0};
      backgroundTiles = {{}, {}, {}};
      // get all viewport tiles for the first frame only,
      // so the video player can start.
      std::map<float, std::vector<uint16_t>> tempBgTiles;

      if (frameIdToRender == 1) {
        // return all viewport tiles with zero displacement.
        tilePredictor->getBackgroundTiles(tempBgTiles, {{0, 0}, {0, 0}}, 0);

        // get the size of the viewport tiles in the lowest quality
        abrAlgorithm->updateTilesAndgetTotalSize(
            std::ref(backgroundTilesSizes[0]), std::ref(backgroundTiles[0]),
            tempBgTiles, 0, clientNetworkLayer);
      }
    }  // END background tiles block

    // update user predictions.
    predictedCorr.clear();
    tilePredictor->getPredictedCorr(predictedCorr);

    // START bandwidth calc block
    // update bandwidth estimation.
    float predictedBw =
        (bandwidthPredictor->getMpcBandwidthPrediction());  // Bytes Per Second

    // High-priority background chunks/tiles with render deadline of the next
    // 2
    // seconds.
    float downloadTimeBgHPInMS =
        predictedBw == 0
            ? 0
            : ((backgroundTilesSizes[0] + backgroundTilesSizes[1]) * 1e3) /
                  predictedBw;
    // Med-priority background chunks/tiles with render deadline of the next
    // 2-3
    // seconds.
    float downloadTimeBgMPInMS =
        predictedBw == 0 ? 0 : (backgroundTilesSizes[2] * 1e3) / predictedBw;

    // bandwidth will be shared between foreground tiles and Med-priority
    // background tiles/chunks.
    // high-priorty background fetched first.
    float bandwidthBgMP = backgroundTilesSizes[2] /
                          (1 - (downloadTimeBgMPInMS / 1e3));  // Bytes/Sec

    float bandwidthFg = 0;
    std::vector<std::pair<std::pair<int, uint16_t>, uint8_t>> foregroundTiles;
    // END bandwidth calc block

    // if the download time of background tiles/chunks less than 1 sec,
    // then there is a room to fetch foreground tiles;
    // START foreground tile block
    if (downloadTimeBgHPInMS + downloadTimeBgMPInMS < 1000) {
      // if foreground tiles to be fetched on chunk basis (i.e., coarse
      // foreground),
      // and still waiting on an ongoing request. Then, skip ABR call.
      if (FLAGS_UtilityCoraseForegroundStream && chunkTileAwaited.first != -1) {
        goto end_forground_block;
      }
      // generate the utility matrix for predicted-to-render tiles in the next
      // 25 frames (1sec).
      auto utilityMatrix = tilePredictor->buildUtilityMatrix(
          predictedCorr, vpResolutions, FLAGS_UtilityCoraseForegroundStream
                                            ? coarseForgroundChunkIdToRequest
                                            : -1);

      // sort tiles by their max utility.
      auto orderedUtilityMatrix =
          abrAlgorithm->orderTilesByMaxUtility(utilityMatrix, frameIdToRender);

      bandwidthFg = predictedBw == 0 ? 0 : (predictedBw - bandwidthBgMP);

      // std::cout << downloadTimeBgHPInMS << ":" << bandwidthFg << "\n";
      foregroundTiles =
          abrAlgorithm->qualityABR(utilityMatrix, frameIdToRender,
                                   bandwidthFg > 0 ? bandwidthFg : 2.5 * 1e6,
                                   downloadTimeBgHPInMS, clientNetworkLayer);
    }  // END foreground tile block
  end_forground_block:

    if (FLAGS_UtilityCoraseForegroundStream) {
      // This is not new request (chunkTileAwaited.first != -1);
      // so update currentFgRequest. Otherwise it is a new request.
      // And, currentFgRequest should be foregroundTiles and
      // chunkTileAwaited.first should be updated.
      auto &tileChunksLoop = chunkTileAwaited.first != -1
                                 ? coarseForegroundTiles
                                 : foregroundTiles;
      std::vector<std::pair<std::pair<int, uint16_t>, uint8_t>>
          updatedCoarseForegroundTiles;
      int chunkIdx = -1;
      uint16_t tileIdx = 0;
      for (auto &chunkTile : tileChunksLoop) {
        chunkIdx = chunkTile.first.first;
        tileIdx = chunkTile.first.second;
        auto qaulityIdx = chunkTile.second;
        if (clientNetworkLayer->isReceived(chunkIdx + 1, tileIdx) !=
            qaulityIdx) {
          updatedCoarseForegroundTiles.push_back(
              {{chunkIdx, tileIdx}, qaulityIdx});
        }
      }
      coarseForegroundTiles = updatedCoarseForegroundTiles;
      foregroundTiles = coarseForegroundTiles;
      if (chunkTileAwaited.first == -1) {
        chunkTileAwaited.first = chunkIdx;
        chunkTileAwaited.second = tileIdx;
      }
    }

    std::string req = "";
    for (auto bgChunkIdx : {0, 1}) {
      for (auto &tile : backgroundTiles[bgChunkIdx]) {
        req += std::to_string(tile.first) + "_" + std::to_string(tile.second);
        req += FLAGS_UtilityCoraseBackgroundStream == "coarse" ? "_0," : "_1,";
      }
    }

    if (FLAGS_UtilityCoraseBackgroundStream == "off") {
      for (auto &chunkTiles : foregroundTiles) {
        req += std::to_string(chunkTiles.first.first) + "_" +
               std::to_string(chunkTiles.first.second) + "_" +
               std::to_string(chunkTiles.second) + ",";
      }
    } else if (foregroundTiles.size() > 0) {
      auto reqScheduled = abrAlgorithm->scheduler(
          backgroundTiles, {2}, bandwidthBgMP, foregroundTiles, bandwidthFg);
      req += reqScheduled;
    } else {
      for (auto &tile : backgroundTiles[2]) {
        req += std::to_string(tile.first) + "_" + std::to_string(tile.second);
        req += FLAGS_UtilityCoraseBackgroundStream == "coarse" ? "_0," : "_1,";
      }
    }

    if (req.size()) {
      req.pop_back();
    }
    std::string finalReq = "Tiles\n" + req;
    finalReq += "\nQuality\n" + std::to_string(0);
    clientNetworkLayer->setRequest(finalReq);
    Util::sleep(stime, ABR_FREQ);
    videoTime += (Util::getTime() - stime);
    stime += ABR_FREQ;
  }
}

std::map<float, std::vector<std::pair<int, uint16_t>>>
AbrAlgorithm::orderTilesByMaxUtility(
    std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
    uint16_t frameIdToRender) {
  // base frame id.
  float frameIdSt = utilityMatrix.find({-1, -1})->second[0];

  std::map<float, std::vector<std::pair<int, uint16_t>>> sortedUtilityMatrix;
  for (auto const tileChunk : utilityMatrix) {
    if (tileChunk.first.first == -1) {  // base frame id.
      continue;
    }
    // 50 = number of frames in the future (25) * number of classes (2)
    auto maxUtility = 25.0 - tileChunk.second[24];
    // deduct the utility of the frames that have already passed deadline.

    if (frameIdToRender > frameIdSt && frameIdToRender != 1) {
      maxUtility -= tileChunk.second[frameIdToRender - frameIdSt];
    }
    if (sortedUtilityMatrix.find(maxUtility) == sortedUtilityMatrix.end()) {
      std::vector<std::pair<int, uint16_t>> tileChunks;
      sortedUtilityMatrix.insert(std::make_pair(maxUtility, tileChunks));
    }
    sortedUtilityMatrix.find(maxUtility)->second.push_back(tileChunk.first);
  }

  // debug log, print utility matrix before and after sorting.
  if (VLOG_IS_ON(1)) {
    LOG(INFO) << "=== Utility Matrix [" << frameIdSt << "]===";

    for (auto const tileChunk : utilityMatrix) {
      std::string p = std::to_string(tileChunk.first.first) + "_" +
                      std::to_string(tileChunk.first.second) + ":";
      for (auto const utility : tileChunk.second) {
        p += std::to_string(utility) + ",";
      }
      p.pop_back();
      LOG(INFO) << p;
    }
    LOG(INFO) << "=== Sorted Utility Matrix [" << frameIdSt << "]===";
    for (auto const utilityChunks : sortedUtilityMatrix) {
      std::string p = std::to_string(50 - utilityChunks.first) + ":";
      for (auto const tile : utilityChunks.second) {
        p += std::to_string(tile.first) + "_" + std::to_string(tile.second) +
             ",";
      }
      p.pop_back();
      LOG(INFO) << p;
    }
  }
  return sortedUtilityMatrix;
}

std::vector<std::pair<int, uint16_t>>
AbrAlgorithm::getTilesWithMaxOverallUtility(
    std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
    std::map<float, std::vector<std::pair<int, uint16_t>>> sortedTilesByUtility,
    uint16_t frameIdToRender, float estimatedBw, float baseTime,
    ClientNetworkLayer *clientNetworkLayer) {
  /*std::cout << baseTime << std::endl;
  std::cout << "BW: " << estimatedBw * 8 / 1e6 << std::endl;
  std::cout << "Frame Id: " << std::to_string(frameIdToRender) << std::endl;*/
  /*
  case 1: simple 5 tiles, all same utility.
  utilityMatrix = {
      {{0,102}, {2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26,
                 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50}},
      {{0,103}, {2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26,
                 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50}},
      {{0,42}, {2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26,
                28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50}},
      {{0,43}, {2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26,
                28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50}},
      {{0,44}, {2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26,
                28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50}}};

  sortedUtilityMatrix = {{0, {{0,102}, {0,103},{0,42}, {0,43}, {0,44}}}};
  */

  // ToDo: create a test unit for this function.
  /*
  case 2: 5 tiles, 42 has higher utilization but later deadline in compare to
  43.
          so, 43 should be requested first.

  utilityMatrix = {
      {"0_102", {2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26,
                 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50}},
      {"0_103", {2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26,
                 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50}},
      {"0_42", {0,  0,  0,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22,
                24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46}},
      {"0_43", {1.2, 3.2, 4.6, 6,  7.9, 9.5, 11, 12, 14, 16,   18, 20,  22,
                24,  26,  28,  30, 32,  34,  36, 38, 40, 41.5, 42, 43.5}},
      {"0_44", {1.2, 3.2, 4.6, 6,  7.9, 9.5, 11, 12, 14, 16,   18, 20,  22,
                24,  26,  28,  30, 32,  34,  36, 38, 40, 41.5, 42, 43.5}}};

  sortedUtilityMatrix = {
      {0, {"0_102", "0_103"}}, {4, {"0_42"}}, {6.5, {"0_43", "0_44"}}};
  */

  /*utilityMatrix = {
      {"0_102", {0,  0,  0,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26,
                 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50}},
      {"0_103", {0,  0,  0,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26,
                 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50}},
      {"0_42", {0,  0,  0,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22,
                24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46}},
      {"0_43", {1.2, 3.2, 4.6, 6,  7.9, 9.5, 11, 12, 14, 16,   18, 20,  22,
                24,  26,  28,  30, 32,  34,  36, 38, 40, 41.5, 42, 43.5}},
      {"0_44", {1.2, 3.2, 4.6, 6,  7.9, 9.5, 11, 12, 14, 16,   18, 20,  22,
                24,  26,  28,  30, 32,  34,  36, 38, 40, 41.5, 42, 43.5}}};

  sortedUtilityMatrix = {
      {50, {"0_102", "0_103"}}, {46, {"0_42"}}, {43.5, {"0_43", "0_44"}}};*/

  // all times in this function are in ms.
  struct tileNode {
    // tileChunk_tileId
    std::pair<int, uint16_t> tile;
    // expected time to recv tile.
    float EstArrivalTime;
    // expected time to transmit tile.
    float EstDownloadTime;
    tileNode *nextTile;
    tileNode *prevTile;
  };

  // tile with highest priority.
  tileNode *headTile = new tileNode();
  // tile with lowest priority.
  tileNode *tailTile = headTile;
  // total utility of all tiles to be downloaded.
  // Our goal is to maximize this.
  float overallUtility = 0;

  // This is the current time (base time).
  float curTime = ((frameIdToRender - 1) * 40.0);

  if (frameIdToRender != 1) {
    curTime += Util::getTimePassedSinceLastFrame();
  }
  curTime += baseTime;
  // base frame id.
  int frameIdSt = utilityMatrix.find({-1, -1})->second[0];

  // We start by choosing the lowest quality.
  // This would maximize the number of tiles we can get.

  // Tiles with highest priority first.
  // utilityTilesPair<utility, set of tiles with this utility>
  for (auto const &utilityTilesPair : sortedTilesByUtility) {
    for (auto const tile : utilityTilesPair.second) {
      uint16_t tileId = tile.second;
      int chunkId = tile.first;

      // -1 means not recieved, and 1: that's background quality.
      // so, let's try and improve the quality.
      if (clientNetworkLayer->isReceived(chunkId + 1, tileId) > 1) {
        continue;
      }
      // estimated time to download the tile chunk in lowest quality possible.
      float estDownloadTime = 1e3 * (tileChunkSizePerQuality_.find(2)
                                         ->second.find(tileId)
                                         ->second[chunkId] /
                                     estimatedBw);  //

      // estimated arrival time of the tile = its download time + time of being
      // last in list.
      float estArrivalTime = estDownloadTime + curTime;
      // map the estimated arrival time of the tile to frame to cal. actual
      // utility.
      int arrvFrameId = int(estArrivalTime / 40) - frameIdSt;

      float actualUtility = 0;
      // if we expect to receive the tile within 1 sec from now, then we can
      // estimate its utility. We don't skip if arrvFrameId >= 25 as we can
      // place it early.
      if (arrvFrameId < 25) {
        actualUtility =
            utilityMatrix[tile][24] - utilityMatrix[tile][arrvFrameId];
      }
      overallUtility += actualUtility;

      // this the first tile to add (highest priority/utility)
      if (tailTile->tile.first == 0 && tailTile->tile.second == 0) {
        // This should rarely happen.
        if (arrvFrameId > 25) {
          // LOG(ERROR)
          //  << "Tile with highest priorty needs > 1 second to be received.";
          continue;
        }
        tailTile->tile = tile;
        tailTile->EstArrivalTime = estArrivalTime;
        tailTile->EstDownloadTime = estDownloadTime;
        tailTile->prevTile = nullptr;
        tailTile->nextTile = nullptr;
      } else {
        // this pointer is used to trace over the request doubly-linkedlist
        tileNode *trace = tailTile;
        // this points at the best location at which the new tile can be
        // placed before. newtile_location = potentialPosition -> prev_tile
        tileNode *potentialPosition = nullptr;
        //  this keeps track of the new utility while as we are trying to find
        //  the best location for the tile.
        float updatedUtility = overallUtility;

        while (trace != nullptr) {
          // UTILITY LOSS

          // cumlative utility vector for tile to be pushed further.
          auto const &utilityNxtTile = utilityMatrix[trace->tile];
          // its estimated arrival frame Id before push.
          float oldEstArrNxtTileFrameId =
              int((trace->EstArrivalTime) / 40) - frameIdSt;
          // its newly estimated arrival frame Id after push.
          float newEstArrvNxtTileFrameId =
              int((trace->EstArrivalTime + estDownloadTime) / 40) - frameIdSt;
          // how much utility is expected to be lost becuase of push.
          float utilityLoss;
          // if its estimated to be received after 1 sec, then max utility is
          // upper bound.
          if (newEstArrvNxtTileFrameId >= 25) {
            utilityLoss =
                utilityNxtTile[24] - utilityNxtTile[oldEstArrNxtTileFrameId];
          } else {
            // otherwise, it will be the new estimated arrival time.
            utilityLoss = utilityNxtTile[newEstArrvNxtTileFrameId] -
                          utilityNxtTile[oldEstArrNxtTileFrameId];
          }

          // UTILITY GAIN
          // cumlative utility vector for current tile.
          auto const &utilitycurrTile = utilityMatrix[tile];
          // its estimated arrival frame Id before.
          float oldEstArrCurrTileFrameId = newEstArrvNxtTileFrameId;
          // its newly estimated arrival frame Id after push.

          float newEstArrvCurrTileFrameId =
              int(((trace->EstArrivalTime - trace->EstDownloadTime) +
                   estDownloadTime) /
                  40) -
              frameIdSt;
          // how much utility is expected to be lost becuase of push.
          float utilityGain;
          // if previously its estimated to be received after 1 sec, then max
          // utility is
          // upper bound.
          if (oldEstArrCurrTileFrameId >= 25) {
            utilityGain = utilitycurrTile[24] -
                          utilitycurrTile[newEstArrvCurrTileFrameId];
          } else {
            // otherwise, it will be the new estimated arrival time.
            utilityGain = utilitycurrTile[oldEstArrCurrTileFrameId] -
                          utilitycurrTile[newEstArrvCurrTileFrameId];
          }
          updatedUtility = updatedUtility + utilityGain - utilityLoss;

          if (updatedUtility >= overallUtility) {
            overallUtility = updatedUtility;
            potentialPosition = trace;
          }
          trace = trace->prevTile;
        }

        tileNode *currTileNode = new tileNode();
        currTileNode->tile = tile;
        // it's best place could be the end of list.
        if (potentialPosition == nullptr) {
          currTileNode->EstArrivalTime =
              tailTile->EstArrivalTime + estDownloadTime;
          currTileNode->prevTile = tailTile;
          currTileNode->nextTile = nullptr;

        } else {
          currTileNode->EstArrivalTime = (potentialPosition->EstArrivalTime -
                                          potentialPosition->EstDownloadTime) +
                                         estDownloadTime;
          currTileNode->prevTile = potentialPosition->prevTile;
          currTileNode->nextTile = potentialPosition;
          currTileNode->nextTile->prevTile = currTileNode;
          if (potentialPosition == headTile) {
            headTile = currTileNode;
          }
        }
        currTileNode->EstDownloadTime = estDownloadTime;
        if (currTileNode->prevTile != nullptr) {  // it is not the head.
          currTileNode->prevTile->nextTile = currTileNode;
        }
        trace = currTileNode;
        tailTile = currTileNode;
        // remove all tiles node with estimated arrival frame Id >=25;
        // if tile in middle dropped then we have to update overall utility.
        bool tileDropped = false;
        while (trace != nullptr) {
          if (trace->prevTile != nullptr) {
            trace->EstArrivalTime =
                trace->prevTile->EstArrivalTime + trace->EstDownloadTime;
          }

          auto estArrvTileFrameId = int(trace->EstArrivalTime / 40) - frameIdSt;

          if (estArrvTileFrameId >= 25) {
            if (trace == headTile) {
              headTile = nullptr;
              break;
            }
            tailTile = trace->prevTile;
            trace->prevTile->nextTile = nullptr;
            trace->prevTile = nullptr;
            trace = nullptr;
            break;
          }
          auto const &utilityNxtTile = utilityMatrix[trace->tile];
          float utilityDiff =
              utilityNxtTile[24] - utilityNxtTile[estArrvTileFrameId];
          bool dontAdvance = false;
          if (utilityDiff == 0) {
            tileDropped = true;
            if (trace->prevTile != nullptr) {
              trace->prevTile->nextTile = trace->nextTile;
              if (trace->nextTile != nullptr) {
                trace->nextTile->prevTile = trace->prevTile;
              }
              tileNode *temp = trace;
              trace = trace->prevTile;
              temp->nextTile = nullptr;
              temp->prevTile = nullptr;
            } else {
              dontAdvance = true;
              trace = trace->nextTile;
              trace->prevTile->nextTile = nullptr;
            }
          }
          if (dontAdvance) {
            continue;
          }
          tailTile = trace;
          trace = trace->nextTile;
        }

        if (tileDropped) {  // recalcuate utility as it might improve.
          trace = headTile;
          float newUtility = 0;
          while (trace != nullptr) {
            auto &tileUtilityVector = utilityMatrix[trace->tile];
            newUtility +=
                tileUtilityVector[24] -
                tileUtilityVector[int(trace->EstArrivalTime / 40) - frameIdSt];
            trace = trace->nextTile;
          }
          overallUtility = newUtility;
        }
      }

      curTime = tailTile->EstArrivalTime;
    }
  }
  std::vector<std::pair<int, uint16_t>> tilesToRequest;
  while (headTile != nullptr) {
    tilesToRequest.push_back(headTile->tile);
    headTile = headTile->nextTile;
  }
  if (tilesToRequest.size() == 1 && tilesToRequest[0].first == 0 &&
      tilesToRequest[0].second == 0) {
    return {};
  }
  return tilesToRequest;
}

std::vector<std::pair<int, uint16_t>>
AbrAlgorithm::sortTilesByUtilityAndQuality(
    ClientNetworkLayer *clientNetworkLayer, uint8_t quality,
    std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
    tileNode *headRequest) {
  // quality * utility, <chunkid,tileid>
  // this is sorted such that tiles
  std::map<float, std::pair<int, uint16_t>, std::greater<float>>
      tilesUtilitySum;
  if (headRequest == nullptr) {
    for (auto &tileUtilityPair : utilityMatrix) {
      auto &tile = tileUtilityPair.first;
      float prevQaulity = 0;

      if (clientNetworkLayer->isReceived(tile.first + 1, tile.second) != -1) {
        prevQaulity = tileChunkPSNRPerQuality_[1][tile.second][tile.first];
      }
      float tileValueDiff =
          utilityMatrix[tile][24] *
          (tileChunkPSNRPerQuality_[quality][tile.second][tile.first] -
           prevQaulity);
      tilesUtilitySum.insert({tileValueDiff, tile});
    }
  } else {
    tileNode *trace = headRequest;
    while (trace != nullptr) {
      auto &tile = trace->tile;
      float prevQaulity = 0;
      if (trace->quality != 0) {
        prevQaulity =
            tileChunkPSNRPerQuality_[trace->quality][tile.second][tile.first];
      }

      float tileValueDiff =
          utilityMatrix[tile][24] *
          (tileChunkPSNRPerQuality_[quality][tile.second][tile.first] -
           prevQaulity);
      tilesUtilitySum.insert({tileValueDiff, tile});
      trace = trace->nextTile;
    }
  }
  std::vector<std::pair<int, uint16_t>> tilesSortedByUtilitySum;
  for (auto qualityTileChunkPair : tilesUtilitySum) {
    if (qualityTileChunkPair.first == 0) {
      continue;
    }
    tilesSortedByUtilitySum.push_back(qualityTileChunkPair.second);
  }
  return tilesSortedByUtilitySum;
}

std::vector<std::pair<std::pair<int, uint16_t>, uint8_t>>
AbrAlgorithm::qualityABR(
    std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
    uint16_t frameIdToRender, float estimatedBw, float baseTime,
    ClientNetworkLayer *clientNetworkLayer) {
  std::map<std::pair<int, uint16_t>, tileNode *> tilesNodeMap;
  // tile with highest priority.
  tileNode *headTile = nullptr;
  // tile with lowest priority.
  tileNode *tailTile = nullptr;
  // total utility of all tiles to be downloaded.
  // Our goal is to maximize this.
  float overallValue = 0;

  // This is the current time (base time).
  // todo: this should be updated to newest frameIdToRender
  float curTime = ((frameIdToRender - 1) * 40.0);

  if (frameIdToRender != 1) {
    curTime += Util::getTimePassedSinceLastFrame();
  }
  curTime += baseTime;
  // base frame id.
  int frameIdSt = utilityMatrix.find({-1, -1})->second[0];
  utilityMatrix.erase({-1, -1});
  tileNode *trace;
  for (int qualityIdx = 2; qualityIdx <= numberOfQualities_; qualityIdx++) {
    auto sortedTiles = sortTilesByUtilityAndQuality(
        clientNetworkLayer, qualityIdx, utilityMatrix, headTile);
    for (auto &tile : sortedTiles) {
      if (clientNetworkLayer->isReceived(tile.first + 1, tile.second) > 1) {
        if (tilesNodeMap.find(tile) != tilesNodeMap.end()) {
          removeNodeAndUpdateUtility(utilityMatrix, headTile, tailTile,
                                     frameIdSt, curTime, tilesNodeMap[tile],
                                     overallValue);
          tilesNodeMap.erase(tile);
        }
        continue;
      }
      // should be removed!
      // if (!FLAGS_UtilityCoraseBackgroundStream) {
      //   if (tile.first == 0) {
      //     continue;
      //   }
      // }
      // create tile node and add to the linked list.
      if (tilesNodeMap.find(tile) == tilesNodeMap.end()) {
        float estDownloadTime =
            1e3 * (tileChunkSizePerQuality_[1][tile.second][tile.first] /
                   estimatedBw);
        tileNode *tileN =
            new tileNode{tile, 0, estDownloadTime, 1, 0, nullptr, nullptr};

        // if bg is full chunk, it will have zero for tile index.
        uint16_t tileIdx =
            FLAGS_UtilityCoraseBackgroundStream == "coarse" ? 0 : tile.second;
        if (clientNetworkLayer->isReceived(tile.first + 1, tileIdx) == -1) {
          tileN->quality = 0;
        }
        tilesNodeMap.insert({tile, tileN});
      }
      auto tileN = tilesNodeMap[tile];
      auto &tileUtilityVec = utilityMatrix[tile];
      auto tileOldPsnr =
          tileN->quality == 0 ? 0 : tileChunkPSNRPerQuality_[tileN->quality]
                                                            [tile.second]
                                                            [tile.first];
      if (FLAGS_UtilityCoraseBackgroundStream == "coarse" &&
          tileN->quality == 1) {
        tileOldPsnr = fullVideoChunksPSNR_[tile.first];
      }
      auto tileNewPsnr =
          tileChunkPSNRPerQuality_[qualityIdx][tile.second][tile.first];

      // calc the old value of the tile.
      float tileOldValue = 0;
      if (tileN->EstArrivalTime != 0) {  // this node already in the linkedlist
        tileOldValue =
            (tileUtilityVec[24] -
             tileUtilityVec[int(tileN->EstArrivalTime / 40) - frameIdSt]) *
            tileOldPsnr;
      } else {                      // new tile node, if so place at the end.
        if (tailTile == nullptr) {  // first node in the request.
          tailTile = tileN;
          headTile = tileN;
          tileN->EstArrivalTime = tileN->EstDownloadTime + curTime;
        } else {  // if not then place at the end.
          tailTile->nextTile = tileN;
          tileN->prevTile = tailTile;
          tailTile = tileN;
          tileN->EstArrivalTime =
              tileN->prevTile->EstArrivalTime + tileN->EstDownloadTime;
        }
        tileOldValue =
            (tileUtilityVec[24] -
             tileUtilityVec[int(tileN->EstArrivalTime / 40) - frameIdSt]) *
            tileOldPsnr;
      }

      float overallValueUpdated = overallValue - tileOldValue;
      float downloadTimeUpdated =
          1e3 * (tileChunkSizePerQuality_[qualityIdx][tile.second][tile.first] /
                 estimatedBw);

      // find the new arrival time after updating the tile quality.
      // start by placing the tile at the tail, calc the new arrival time.
      float estArrTimeUpdated = 0;
      if (tileN != tailTile) {
        estArrTimeUpdated =
            ((tailTile->EstArrivalTime - tileN->EstDownloadTime) +
             downloadTimeUpdated);
      } else {
        estArrTimeUpdated = (tileN->EstArrivalTime - tileN->EstDownloadTime) +
                            downloadTimeUpdated;
      }

      int arrFrmId = int(estArrTimeUpdated / 40) - frameIdSt;
      float tileValueUpdated = 0;

      if (arrFrmId < 25) {
        tileValueUpdated =
            (utilityMatrix[tile][24] - utilityMatrix[tile][arrFrmId]) *
            tileNewPsnr;
      }
      // if adding the tile to tail improves the overall value of the request,
      // then set placeAtTail to true
      bool placeAtTail = false;
      if (tileValueUpdated + overallValueUpdated > overallValue) {
        overallValueUpdated += tileValueUpdated;
        overallValue = overallValueUpdated;
        placeAtTail = true;
      }
      tileNode *potentionalPos = returnBestPosition(
          utilityMatrix, tailTile, tileN, frameIdSt, downloadTimeUpdated,
          tileNewPsnr, overallValueUpdated, overallValue);

      // TILE PLACEMENT
      // Place tile in its potention new place if exists.
      if (!placeAtTail && potentionalPos == nullptr) {
        goto EXIT_L1;
      } else if (potentionalPos != nullptr) {
        placeAtTail = false;
      }

      if (placeAtTail && tailTile == tileN) {
        tailTile->EstArrivalTime =
            (tailTile->EstArrivalTime - tailTile->EstDownloadTime) +
            downloadTimeUpdated;
        tailTile->EstDownloadTime = downloadTimeUpdated;
        tailTile->quality = qualityIdx;
        goto EXIT_L1;
      }

      updateArrivalTimeOfSuccessorNodes(tailTile, tileN, potentionalPos,
                                        downloadTimeUpdated, placeAtTail);

      moveAndUpdateTile(headTile, tailTile, tileN, potentionalPos,
                        downloadTimeUpdated, curTime, qualityIdx, placeAtTail);

    EXIT_L1:
      checkTilesUtility(utilityMatrix, tilesNodeMap, headTile, tailTile,
                        frameIdSt, estimatedBw, curTime);

      // check the updated utility.
      tilesNodeMap.clear();
      trace = headTile;
      overallValue = 0;
      while (trace != nullptr) {
        tilesNodeMap.insert({trace->tile, trace});
        auto tile = trace->tile;
        auto tileUtilityVec = utilityMatrix[tile];
        auto tilePsnr = trace->quality == 0
                            ? 0
                            : tileChunkPSNRPerQuality_[trace->quality]
                                                      [tile.second][tile.first];
        auto tileUtility =
            tileUtilityVec[24] -
            tileUtilityVec[int(trace->EstArrivalTime / 40) - frameIdSt];
        overallValue += tileUtility * tilePsnr;
        trace = trace->nextTile;
      }
    }
  }
  std::vector<std::pair<std::pair<int, uint16_t>, uint8_t>> tilesToReturn;
  while (headTile != nullptr) {
    tilesToReturn.push_back({headTile->tile, headTile->quality});
    headTile = headTile->nextTile;
  }

  return tilesToReturn;
}

void AbrAlgorithm::removeNodeAndUpdateUtility(
    std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
    tileNode *&headTile, tileNode *&tailTile, int frameIdSt, float currTime,
    tileNode *&tileN, float &overallValue) {
  // remove node from linkedlist.
  if (tileN->prevTile == nullptr) {  // remove head.
    headTile = headTile->nextTile;
    if (headTile != nullptr) {
      headTile->prevTile->nextTile = nullptr;
      headTile->prevTile = nullptr;
    } else {  // this happens if tail and head are the only node (which to be
              // removed)
      tailTile = nullptr;
    }
  } else if (tileN->nextTile == nullptr) {  // remove tail
    tailTile = tailTile->prevTile;
    if (tailTile != nullptr) {
      tailTile->nextTile->prevTile = nullptr;
      tailTile->nextTile = nullptr;
    } else {  // this happens if tail and head are the only node (which to be
              // removed)
      headTile = nullptr;
    }
  } else {
    tileN->prevTile->nextTile = tileN->nextTile;
    tileN->nextTile->prevTile = tileN->prevTile;
    tileN->nextTile = nullptr;
    tileN->prevTile = nullptr;
  }
  // node is removed now.

  tileNode *trace = headTile;
  overallValue = 0;
  while (trace != nullptr) {
    if (trace->prevTile == nullptr) {
      trace->EstArrivalTime = currTime + trace->EstDownloadTime;
    } else {
      trace->EstArrivalTime =
          trace->prevTile->EstArrivalTime + trace->EstDownloadTime;
    }
    auto estArrvFrameId = int(trace->EstArrivalTime / 40) - frameIdSt;
    overallValue += (utilityMatrix[trace->tile][24] -
                     utilityMatrix[trace->tile][estArrvFrameId]) *
                    tileChunkSizePerQuality_[trace->quality][trace->tile.second]
                                            [trace->tile.first];
    trace = trace->nextTile;
  }
}

AbrAlgorithm::tileNode *AbrAlgorithm::returnBestPosition(
    std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
    tileNode *&tailTile, tileNode *&tileN, int frameIdSt,
    float downloadTimeUpdated, float tileNewPsnr, float overallValueUpdated,
    float &overallValue) {
  // Since tile might be already in the linkedlist of tileNodes, so instead
  // of remvoing the tile and update the utility and download time for all
  // successor nodes. We set this bool to true once we reach the tile, and
  // stop updating DT and utility of its predecessor.
  bool tileFound = false;
  tileNode *potentionalPos = nullptr;
  tileNode *trace = tailTile;
  // try and find the best position for this tile,
  // if none exist, then keep it where it is.
  while (trace != nullptr) {
    // start tile_quality_update_loop

    if (trace->tile == tileN->tile) {
      trace = trace->prevTile;
      tileFound = true;
      continue;
    }

    // UTILITY LOSS
    // tile to switch position with.
    int toSwitchTileArrvFrameId = int(trace->EstArrivalTime / 40) - frameIdSt;
    float toSwitchTileArrvTimeUpdated =
        (trace->EstArrivalTime) + downloadTimeUpdated;
    if (!tileFound) {
      toSwitchTileArrvTimeUpdated -= tileN->EstDownloadTime;
    }
    int toSwitchTileArrvFrameIdUpdated =
        int(toSwitchTileArrvTimeUpdated / 40) - frameIdSt;
    auto toSwitchTilePsnr =
        trace->quality == 0 ? 0 : tileChunkPSNRPerQuality_[trace->quality]
                                                          [trace->tile.second]
                                                          [trace->tile.first];

    float utilityLoss = toSwitchTilePsnr;
    // it the estimated new arrival frame Id is beyond 1 sec;
    // then upper bound is the max frmae id possible == 24.
    if (toSwitchTileArrvFrameIdUpdated >= 25) {
      utilityLoss *= (utilityMatrix[trace->tile][24] -
                      utilityMatrix[trace->tile][toSwitchTileArrvFrameId]);
    } else {
      utilityLoss *=
          (utilityMatrix[trace->tile][toSwitchTileArrvFrameIdUpdated] -
           utilityMatrix[trace->tile][toSwitchTileArrvFrameId]);
    }
    // HERE
    // UTILITY GAIN
    int tileArrvFrameIdOld = int(tileN->EstArrivalTime / 40) - frameIdSt;
    int tileArrvFrameIdUpdated =
        int((toSwitchTileArrvTimeUpdated - trace->EstDownloadTime) / 40) -
        frameIdSt;

    // the new arrv time can be > old arrv time
    float utilityGain = tileNewPsnr;
    // this happens becuase we place the tile at the end when we start looking
    // for new position
    if (tileArrvFrameIdOld >= 25 && tileArrvFrameIdUpdated < 25) {
      utilityGain *= utilityMatrix[tileN->tile][24] -
                     utilityMatrix[tileN->tile][tileArrvFrameIdUpdated];
    } else if (tileArrvFrameIdOld < 25 && tileArrvFrameIdUpdated < 25) {
      // this might be negative now.
      utilityGain *= utilityMatrix[tileN->tile][tileArrvFrameIdOld] -
                     utilityMatrix[tileN->tile][tileArrvFrameIdUpdated];
    } else if (tileArrvFrameIdUpdated >= 25 && tileArrvFrameIdOld < 25) {
      // this definitely negative as new arrv time is > old arrv time.
      utilityGain *= (utilityMatrix[tileN->tile][tileArrvFrameIdOld] -
                      utilityMatrix[tileN->tile][24]);
    }
    if (utilityGain > 1e7 || utilityGain < -1e7 || utilityLoss > 1e7 ||
        utilityLoss < -1e7) {
      std::cout << tileN->tile.first << ":" << tileN->tile.second << "\n";
      std::cout << frameIdSt << ":" << tileN->EstArrivalTime << "\n";
      std::cout << utilityGain << "= " << tileArrvFrameIdOld << " vs "
                << tileArrvFrameIdUpdated << "\n";
      std::cout << utilityLoss << "= " << toSwitchTileArrvFrameId << " vs "
                << toSwitchTileArrvFrameIdUpdated << "\n";
      std::cout << "------\n";
    }
    overallValueUpdated += utilityGain - utilityLoss;
    if (overallValueUpdated >= overallValue) {
      potentionalPos = trace;
      overallValue = overallValueUpdated;
    }
    trace = trace->prevTile;
  }  // end tile_quality_update_loop
  return potentionalPos;
}

void AbrAlgorithm::updateArrivalTimeOfSuccessorNodes(tileNode *&tailTile,
                                                     tileNode *&tileN,
                                                     tileNode *&potentionalPos,
                                                     float downloadTimeUpdated,
                                                     bool placeAtTail) {
  tileNode *trace;
  float tileStartOld =
      tileN->prevTile != nullptr ? tileN->prevTile->EstArrivalTime : 0;

  float tileStartUpdated = placeAtTail
                               ? tailTile->EstArrivalTime
                               : (potentionalPos->prevTile != nullptr
                                      ? potentionalPos->prevTile->EstArrivalTime
                                      : 0);
  float diffInDT = downloadTimeUpdated - tileN->EstDownloadTime;

  // determine whether tile will be moving early in request or not.
  // if early, the update the estimated arrival times for all tiles after
  // potentionalPos. If late,  then the update should start from the tileN

  bool left = false;
  if (tileStartOld <= tileStartUpdated) {
    trace = tileN;
  } else {
    trace = potentionalPos;
    left = true;
  }

  bool found = false;
  // update arrival times for successor tiles.
  while (trace != nullptr) {
    // start  while: update EstArrivalTime

    // if left, should check whether if I reached the tileN
    // otherwise the potentional pos.
    if (left && trace == tileN) {
      found = true;
      trace = trace->nextTile;
      continue;
    } else if (!left && trace == potentionalPos) {
      found = true;
    }

    if (!found) {
      if (left) {
        trace->EstArrivalTime += downloadTimeUpdated;
      } else if (!left) {
        trace->EstArrivalTime -= tileN->EstDownloadTime;
      }
    } else {
      trace->EstArrivalTime += diffInDT;
    }
    trace = trace->nextTile;
  }  // end while: update EstArrivalTime
}

void AbrAlgorithm::moveAndUpdateTile(tileNode *&headTile, tileNode *&tailTile,
                                     tileNode *&tileN,
                                     tileNode *&potentionalPos,
                                     float downloadTimeUpdated, float currTime,
                                     uint8_t qualityIdx, bool placeAtTail) {
  // change tile position and update its download time, estArrtivalTime, and
  // quality.
  tileN->quality = qualityIdx;

  if (placeAtTail) {
    if (tailTile != tileN) {
      if (tileN->prevTile == nullptr) {
        headTile = tileN->nextTile;
      } else {
        tileN->prevTile->nextTile = tileN->nextTile;
      }
      tileN->nextTile->prevTile = tileN->prevTile;
      tileN->nextTile = nullptr;
      tileN->prevTile = tailTile;
      tailTile->nextTile = tileN;
      tailTile = tileN;
    }
    tileN->EstArrivalTime = downloadTimeUpdated;
    if (tileN->prevTile != nullptr) {
      tileN->EstArrivalTime += tileN->prevTile->EstArrivalTime;
    } else {
      tileN->EstArrivalTime += currTime;
    }
  } else if (potentionalPos->prevTile == tileN) {  // keep in the same position
    tileN->EstArrivalTime =
        (tileN->EstArrivalTime - tileN->EstDownloadTime) + downloadTimeUpdated;
  } else {  // move the tileN to potentional Postition -> prev.

    if (potentionalPos->prevTile == nullptr) {  // move to head.
      if (tileN->nextTile == nullptr) {
        // move tail to head.
        tileN->prevTile->nextTile = nullptr;
        tailTile = tileN->prevTile;
      } else {
        // move node (not tail) to head
        tileN->prevTile->nextTile = tileN->nextTile;
        tileN->nextTile->prevTile = tileN->prevTile;
      }
      tileN->nextTile = potentionalPos;
      potentionalPos->prevTile = tileN;
      tileN->prevTile = nullptr;
      headTile = tileN;
      tileN->EstArrivalTime = currTime + downloadTimeUpdated;

    } else {
      // move anywhere but not the head nor tail.
      if (tileN->nextTile == nullptr) {  // move tail
        tileN->prevTile->nextTile = nullptr;
        tailTile = tileN->prevTile;
      } else if (tileN->prevTile == nullptr) {  // move the head.
        tileN->nextTile->prevTile = nullptr;
        headTile = tileN->nextTile;
      } else {
        tileN->prevTile->nextTile = tileN->nextTile;
        tileN->nextTile->prevTile = tileN->prevTile;
      }
      tileN->nextTile = potentionalPos;
      tileN->prevTile = potentionalPos->prevTile;
      potentionalPos->prevTile = tileN;
      tileN->prevTile->nextTile = tileN;  // cannot this if I am the head.
      tileN->EstArrivalTime =
          tileN->prevTile->EstArrivalTime + downloadTimeUpdated;
    }
  }
  tileN->EstDownloadTime = downloadTimeUpdated;
}

void AbrAlgorithm::checkTilesUtility(
    std::map<std::pair<int, uint16_t>, std::vector<float>> utilityMatrix,
    std::map<std::pair<int, uint16_t>, tileNode *> &tilesNodeMap,
    tileNode *&headTile, tileNode *&tailTile, int frameIdSt, float estimatedBw,
    float currTime) {
  // if the tile has utility == 0 or tile quality 0 or 1, then drop the tile.
  tileNode *trace = headTile;
  while (trace != nullptr) {
    auto estArrFrameId = int(trace->EstArrivalTime / 40) - frameIdSt;
    auto tileUtilityVec = utilityMatrix[trace->tile];
    float tileUtility = 0;
    if (estArrFrameId < 25) {
      tileUtility = tileUtilityVec[24] - tileUtilityVec[estArrFrameId];
    }
    if (tileUtility != 0 && trace->quality >= 2) {
      trace = trace->nextTile;
      continue;
    }
    bool removeTile = true;
    // if (tileUtility == 0) {
    //   while (trace->quality > 2) {
    //     trace->quality -= 1;
    //     auto EstDownloadTimeUpdated =
    //         1e3 *
    //         (tileChunkSizePerQuality_[trace->quality][trace->tile.second]
    //                                        [trace->tile.first] /
    //                estimatedBw);
    //     auto estArrTime = EstDownloadTimeUpdated;
    //     if (trace->prevTile != nullptr) {
    //       estArrTime += trace->prevTile->EstArrivalTime;
    //     } else {
    //       estArrTime += currTime;
    //     }
    //     estArrFrameId = int(estArrTime / 40) - frameIdSt;
    //     tileUtility = 0;
    //     if (estArrFrameId < 25) {
    //       tileUtility = tileUtilityVec[24] - tileUtilityVec[estArrFrameId];
    //     }
    //     if (tileUtility != 0) {
    //       trace->EstDownloadTime = EstDownloadTimeUpdated;
    //       trace->EstArrivalTime = estArrTime;
    //       removeTile = false;
    //       break;
    //     }
    //   }
    // }

    // remove tile.
    tileNode *temp = trace;
    if (removeTile) {
      if (trace->prevTile == nullptr) {  // remove head;
        headTile = trace->nextTile;
        if (headTile != nullptr) {  // this is the only node in linkedlist
          headTile->prevTile->nextTile = nullptr;
          headTile->prevTile = nullptr;
          headTile->EstArrivalTime = headTile->EstDownloadTime + currTime;
          temp = headTile;
        } else {
          temp = nullptr;
          tailTile = nullptr;
        }
      } else if (trace->nextTile == nullptr) {
        tailTile = tailTile->prevTile;
        if (tailTile != nullptr) {
          tailTile->nextTile->prevTile = nullptr;
          tailTile->nextTile = nullptr;
        } else {
          headTile = nullptr;
        }
        temp = nullptr;
      } else {
        trace->prevTile->nextTile = trace->nextTile;
        trace->nextTile->prevTile = trace->prevTile;
        temp = trace->nextTile;
      }
      tilesNodeMap.erase(trace->tile);

      // update the estimated time for successor tiles.
      tileNode *updateNode = temp;
      while (updateNode != nullptr) {
        updateNode->EstArrivalTime = updateNode->EstDownloadTime;
        if (updateNode->prevTile != nullptr) {
          updateNode->EstArrivalTime += updateNode->prevTile->EstArrivalTime;
        } else {
          updateNode->EstArrivalTime += currTime;
        }
        updateNode = updateNode->nextTile;
      }
      trace = temp;
    }  // end while
    else {
      trace = trace->nextTile;
    }
  }
}

uint8_t AbrAlgorithm::getNumberOfQualities() { return numberOfQualities_; }

std::map<int, std::vector<std::string>>
AbrAlgorithm::getPossibleQualityAssignment(int quality, int tileClass) {
  std::map<int, std::vector<std::string>> returnMap;
  if (tileClass == 1) {
    for (int q = quality; q > 0; q--) {
      std::vector<std::string> qualityVec{std::to_string(q)};
      returnMap.insert(std::make_pair(q, qualityVec));
    }
    return returnMap;
  }

  auto result = getPossibleQualityAssignment(quality, tileClass - 1);
  for (int q1 = quality; q1 > 0; q1--) {
    std::vector<std::string> solution;
    for (int q2 = q1; q2 > 0; q2--) {
      for (auto sol : result.find(q2)->second) {
        solution.push_back(std::to_string(q1) + "," + sol);
      }
    }
    returnMap.insert(std::make_pair(q1, solution));
  }

  return returnMap;
}

long AbrAlgorithm::getTilesSizes(
    std::vector<std::pair<std::pair<int, uint16_t>, uint8_t>> &fgTiles) {
  long totalSize = 0;
  for (auto &tile : fgTiles) {
    totalSize += tileChunkSizePerQuality_.find(tile.second)
                     ->second.find(tile.first.second)
                     ->second[tile.first.first];
  }
  return totalSize;
}

/***********************PANO*****************************/

// this uses alpha - beta pruning to
// find the highest overall psnr
// and lowest size (size that matches bitrate).

// prune if lower PSNR or if similar PSNR and smaller size.
// if no possible assignment it is lowest quality for all.

void AbrAlgorithm::panoAbr(AbrAlgorithm *abrAlgorithm,
                           TilePredictor *tilePredictor,
                           BandwidthPredictor *bandwidthPredictor,
                           ClientNetworkLayer *clientNetworkLayer,
                           VideoPlayer *videoPlayer,
                           std::string panoTilesGroupsPath,
                           std::string panoVideoBitrate) {
  abrAlgorithm->buildBitrateAssigment(
      {}, int(abrAlgorithm->predictionWindow_ / 25) + 1, 16);

  // index at zero is not used.
  uint8_t tilesGroups[12 * 12 + 1];

  // number of groups is 30 by default
  uint8_t groupCount[30 + 1] = {0};
  std::map<uint8_t, std::vector<uint16_t>> groupsTiles;

  abrAlgorithm->readTilesGroups(tilesGroups, groupsTiles, groupCount, 12, 12,
                                panoTilesGroupsPath);

  std::map<uint8_t, std::vector<float>> chunksBitrates;
  abrAlgorithm->readChunksBitrates(chunksBitrates, panoVideoBitrate);

  // this can be done in the constructor
  abrAlgorithm->fillGroupQualityInfo(tilesGroups, 12 * 12 + 1);

  std::map<uint8_t, std::vector<std::pair<int, uint16_t>>> tilesRequest;
  // this the id of the chunk currently being downloaded.
  int chunkToRequest = 0;
  while (true) {
    auto frameId = videoPlayer->getFrameToRenderId();
    int chunkId = (frameId - 1) / 25;
    auto bufferChunkStat =
        abrAlgorithm->getBufferChunkStatus(clientNetworkLayer, frameId);

    // if 100ms wake up, if the current chunk being download (is done and buffer
    // size <= 2) or passed its deadline ,
    // then update the request. if not sleep.

    // Request for this chunk is sent and it has not been fully downloaded.
    // OR
    // buffer is full

    // how pano works.
    // 1- predict for the next N seconds.
    // 2- as long as buffer size < N, request chunk c.
    // 3- wait until c is received, then repeat. Or current played > requested.

    if (chunkToRequest != 0 &&
        (bufferChunkStat[(chunkToRequest - 1) - chunkId] != 144 &&
         (chunkToRequest - 1) >= chunkId)) {
      continue;
    }
    auto bitrateAssignmentIdx = abrAlgorithm->mpcBitratePerChunk(
        bandwidthPredictor, clientNetworkLayer, chunksBitrates,
        videoPlayer->getFrameToRenderId(), 16);

    auto assignment = abrAlgorithm->bitrateAssignments_[bitrateAssignmentIdx];

    auto areaPerTile = tilePredictor->getOverlappingAreaSizePerTile({100, 100});
    auto areaPerGroup = abrAlgorithm->areaPerGroup(areaPerTile, tilesGroups);
    auto groupsQualityPerChunk = abrAlgorithm->selectIntraGroupQuality(
        bitrateAssignmentIdx, chunksBitrates, areaPerGroup);

    std::map<int, std::map<int, std::vector<uint16_t>>> chunkQualityTileMap;
    bool update = false;
    for (auto &chunkAreaPair : areaPerGroup) {
      auto chunkIdx = chunkAreaPair.first;
      if (chunkIdx < chunkToRequest) {
        continue;
      }
      if (chunkQualityTileMap.find(chunkIdx) == chunkQualityTileMap.end()) {
        chunkQualityTileMap.insert({chunkIdx, {}});
      }
      auto &qualityTilesMap = chunkQualityTileMap.find(chunkIdx)->second;
      for (auto const &groupPair :
           chunkAreaPair.second) {  // for group [sorted by area]
        update = true;
        auto groupId = groupPair.first;
        for (auto tileId : groupsTiles.find(groupId)->second) {  // for tile
          int qualityId = groupsQualityPerChunk.find(chunkIdx)->second[groupId];
          qualityId *= -1;
          if (qualityTilesMap.find(qualityId) == qualityTilesMap.end()) {
            qualityTilesMap.insert({qualityId, {}});
          }
          qualityTilesMap.find(qualityId)->second.push_back(tileId);
          ;
        }
      }
    }
    if (update) {
      // this ensures that we only send the request for the chunk once.
      chunkToRequest++;
    }

    std::string req = "Tiles\n";
    for (auto &chunksQualityTiles : chunkQualityTileMap) {
      for (auto qualityTiles : chunksQualityTiles.second) {
        for (auto tile : qualityTiles.second) {
          req += std::to_string(chunksQualityTiles.first) + "_" +
                 std::to_string(tile) + "_" +
                 std::to_string(qualityTiles.first * -1) + ",";
        }
      }
    }
    if (VLOG_IS_ON(3)) {
      std::vector<int> sizes(int(abrAlgorithm->predictionWindow_ / 25), 0);
      for (int chunkIds = 0; chunkIds < (int)groupsQualityPerChunk.size();
           chunkIds++) {
        for (int idx = 1; idx < (int)groupsQualityPerChunk[chunkIds].size();
             idx++) {
          sizes[chunkIds] +=
              abrAlgorithm
                  ->groupChunkSizePerQuality_[groupsQualityPerChunk[chunkIds]
                                                                   [idx]][idx]
                                             [chunkIds + chunkId];
        }
      }
      VLOG(3) << sizes[0] * 8 / 1e6 << "   :   " << sizes[1] * 8 / 1e6
              << "    :   " << sizes[2] * 8 / 1e6 << "\n======\n";
    }
    req.pop_back();
    // std::cout << req << "\n";
    req += "\nQuality\n" + std::to_string(0);
    clientNetworkLayer->setRequest(req);
  }
}

int AbrAlgorithm::mpcBitratePerChunk(
    BandwidthPredictor *bandwidthPredictor,
    ClientNetworkLayer *clientNetworkLayer,
    std::map<uint8_t, std::vector<float>> &chunksBitrates, uint32_t frameId,
    int maxQuality) {
  // this should return the highest chunk bitrate with minimum rebuffering.
  // determine buffer occupancy
  auto bufferChunkStat = getBufferChunkStatus(clientNetworkLayer, frameId);

  float predictedBw =
      (bandwidthPredictor->getMpcBandwidthPrediction() * 8.0) / 1e6;  // mbps
  assert(predictedBw >= 0);
  if (predictedBw == 0) {
    return bitrateAssignments_.size() - 1;
  }

  int stChunkId = ((frameId - 1) / 25);
  // (first frame in next chunk - current frame) * <frame duration in sec>
  uint32_t timeLeftInCurrChunk = (((stChunkId + 1) * 25) - (frameId - 1)) * 40;

  // initially set to the highest possible quality.
  int idxBestBitrate = 0;
  float maxReward = -FLT_MAX;
  for (int idx = 0; idx < (int)bitrateAssignments_.size(); idx++) {
    auto &bitrateAssignment = bitrateAssignments_[idx];
    float rebuffering = 0;
    float buffer = 0;
    float reward = 0;
    for (int chunkIdx = 0; chunkIdx < (int)bufferChunkStat.size(); chunkIdx++) {
      // time to be added to buffer if this chunk is downloaded.
      auto bufferTimeForThisChunk =
          (chunkIdx == 0 ? timeLeftInCurrChunk : 1000);
      if (bufferChunkStat[chunkIdx] == 144) {  // chunk is fully received.
        buffer += bufferTimeForThisChunk;
        continue;
      }
      float fracOfMissedChunk = (144 - bufferChunkStat[chunkIdx]) / 144.0;
      float chunkDownloadTimeInMs =
          1e3 *
          ((chunksBitrates[bitrateAssignment[chunkIdx]][chunkIdx + stChunkId] *
            fracOfMissedChunk) /
           predictedBw);

      // it the download time > buffer size, then rebuffer.
      if (buffer - chunkDownloadTimeInMs < 0) {
        rebuffering += std::abs(buffer - chunkDownloadTimeInMs);
        buffer = bufferTimeForThisChunk;
      } else {
        buffer = (buffer - chunkDownloadTimeInMs) + bufferTimeForThisChunk;
      }
      // we deduct maxQuality from bitrateAssignment
      // because 0 quality is the highest, and 14 is the lowest
      reward +=
          (maxQuality - bitrateAssignment[chunkIdx]) * bufferTimeForThisChunk;
    }

    reward = reward - rebuffering * maxQuality;
    // std::cout << y << ":" << reward << "\n";
    if (VLOG_IS_ON(3)) {
      VLOG(3) << "Sol id:" << idx << "[" << std::to_string(bitrateAssignment[0])
              << "," << std::to_string(bitrateAssignment[1]) << ","
              << std::to_string(bitrateAssignment[2]) << "]"
              << chunksBitrates[bitrateAssignment[0]][stChunkId] << ","
              << chunksBitrates[bitrateAssignment[1]][stChunkId + 1] << ","
              << chunksBitrates[bitrateAssignment[2]][2 + stChunkId] << reward
              << "====";
    }
    if (reward > maxReward) {
      maxReward = reward;
      idxBestBitrate = idx;
    }
  }
  return idxBestBitrate;
}

std::map<int, std::vector<uint8_t>> AbrAlgorithm::selectIntraGroupQuality(
    int bitrateAssignmentIdx,
    std::map<uint8_t, std::vector<float>> chunksBitrates,
    std::map<int, std::map<uint8_t, float>> groupsAreaPerChunk) {
  std::map<int, std::vector<uint8_t>> intraGroupQPerChunk;
  auto &chunkBitrateAssignment = bitrateAssignments_[bitrateAssignmentIdx];
  int idx = 0;
  // chunk(i, i+1,i+2) --> group--> area.
  for (auto &chunkAreaPerGroup : groupsAreaPerChunk) {  // per chunk

    auto chunkId = chunkAreaPerGroup.first;
    auto chunkTargetSize =
        (chunksBitrates[chunkBitrateAssignment[idx]][chunkId] * 1e6) /
        8.0;  // Bytes

    // size of all groups in lowest quality.
    std::vector<uint8_t> groupsQ(chunkAreaPerGroup.second.size() + 1, 1);
    float initSize = 0;
    for (auto groupArea : chunkAreaPerGroup.second) {
      auto groupId = groupArea.first;
      initSize += groupChunkSizePerQuality_.find(1)
                      ->second.find(groupId)
                      ->second[chunkId];
    }
    // increase the quality of each group by one at a time;
    // if the total size >= assigned bitrate; Stop.
    for (int qualityId = 2; qualityId <= (int)groupChunkSizePerQuality_.size();
         qualityId++) {
      auto sortedGroups = sortedGroupsByMaxPsnrImpact(
          chunkAreaPerGroup.second, groupsQ, chunkId, qualityId);
      for (auto &gainGroups : sortedGroups) {
        // gain is zero (does not overlapp, or diff psnr is zero)
        if (gainGroups.first == 0) {
          continue;
        }
        for (auto groupId : gainGroups.second) {
          auto oldSize =
              groupChunkSizePerQuality_[groupsQ[groupId]][groupId][chunkId];
          auto newSize = groupChunkSizePerQuality_[qualityId][groupId][chunkId];
          // if we cannot increase the quality of the current group; try
          // next group.
          if (initSize + newSize - oldSize > chunkTargetSize) {
            continue;
          }
          initSize += (newSize - oldSize);
          groupsQ[groupId] = qualityId;
        }
      }
    }
    intraGroupQPerChunk.insert({chunkId, groupsQ});
    idx++;
  }
  return intraGroupQPerChunk;
}

std::map<int, std::map<uint8_t, float>> AbrAlgorithm::areaPerGroup(
    std::map<uint16_t, std::map<float, std::vector<uint16_t>>> areaPerTile,
    uint8_t tilesGroups[]) {
  std::map<int, std::map<uint8_t, float>> groupTotalAreaPerChunk;

  for (auto &frameTilesArea : areaPerTile) {  // per frame
    auto frameId = frameTilesArea.first;
    int chunkId = frameId / 25;
    if (groupTotalAreaPerChunk.find(chunkId) == groupTotalAreaPerChunk.end()) {
      groupTotalAreaPerChunk.insert({chunkId, {}});
    }
    for (auto &tilesArea : frameTilesArea.second) {  // tile area
      auto tileArea = 1 - tilesArea.first;
      for (auto tile : tilesArea.second) {
        auto groupId = tilesGroups[tile];
        if (groupTotalAreaPerChunk[chunkId].find(groupId) ==
            groupTotalAreaPerChunk[chunkId].end()) {
          groupTotalAreaPerChunk[chunkId].insert({groupId, 0});
        }
        groupTotalAreaPerChunk[chunkId][groupId] += tileArea;
      }
    }
  }
  return groupTotalAreaPerChunk;
}

std::map<float, std::vector<uint8_t>> AbrAlgorithm::sortedGroupsByMaxPsnrImpact(
    std::map<uint8_t, float> groupsArea,
    std::vector<uint8_t> groupCurQualityVec, int chunkId,
    uint8_t qualityTarget) {
  std::map<float, std::vector<uint8_t>> sortedGroups;
  for (auto &groupArea : groupsArea) {
    auto newPsnr =
        groupChunkPSNRPerQuality_[qualityTarget][groupArea.first][chunkId];
    auto oldPsnr =
        groupChunkPSNRPerQuality_[groupCurQualityVec[groupArea.first]]
                                 [groupArea.first][chunkId];
    auto gain = (newPsnr - oldPsnr) * groupsArea.find(groupArea.first)->second;
    // map is ascendingly ordered, we flip the gain so that groups with highest
    // gain comes first.
    float sortedGain = gain * -1;
    if (sortedGroups.find(sortedGain) == sortedGroups.end()) {
      sortedGroups.insert({sortedGain, {}});
    }
    sortedGroups[sortedGain].push_back(groupArea.first);
  }

  return sortedGroups;
}

void AbrAlgorithm::fillGroupQualityInfo(uint8_t tilesGroups[], int numOfTiles) {
  std::map<uint8_t, uint8_t> numTilesPerGroup;
  for (int tileIdx = 1; tileIdx < numOfTiles; tileIdx++) {
    auto groupId = tilesGroups[tileIdx];
    if (numTilesPerGroup.find(groupId) == numTilesPerGroup.end()) {
      numTilesPerGroup.insert({groupId, 0});
    }
    numTilesPerGroup[groupId] += 1;
    for (uint8_t qualityIdx = 1; qualityIdx <= numberOfQualities_;
         qualityIdx++) {
      if (groupChunkSizePerQuality_.find(qualityIdx) ==
          groupChunkSizePerQuality_.end()) {
        groupChunkSizePerQuality_.insert({qualityIdx, {}});
        groupChunkPSNRPerQuality_.insert({qualityIdx, {}});
      }
      if (groupChunkSizePerQuality_[qualityIdx].find(groupId) ==
          groupChunkSizePerQuality_[qualityIdx].end()) {
        groupChunkSizePerQuality_[qualityIdx].insert({groupId, {}});
      }
      for (int chunkIdx = 0;
           chunkIdx < (int)tileChunkSizePerQuality_[qualityIdx][tileIdx].size();
           chunkIdx++) {
        if (groupChunkSizePerQuality_[qualityIdx][groupId].size() <
            tileChunkSizePerQuality_[qualityIdx][tileIdx].size()) {
          groupChunkSizePerQuality_[qualityIdx][groupId].push_back(
              tileChunkSizePerQuality_[qualityIdx][tileIdx][chunkIdx]);
          groupChunkPSNRPerQuality_[qualityIdx][groupId].push_back(
              tileChunkPSNRPerQuality_[qualityIdx][tileIdx][chunkIdx]);
        } else {
          groupChunkSizePerQuality_[qualityIdx][groupId][chunkIdx] +=
              tileChunkSizePerQuality_[qualityIdx][tileIdx][chunkIdx];
          groupChunkPSNRPerQuality_[qualityIdx][groupId][chunkIdx] +=
              tileChunkPSNRPerQuality_[qualityIdx][tileIdx][chunkIdx];
        }
      }
    }
  }
  for (auto &groupChunkPSNRPerQuality : groupChunkPSNRPerQuality_) {
    for (auto &qualityPnsr : groupChunkPSNRPerQuality.second) {
      for (auto idx = 0; idx < (int)qualityPnsr.second.size(); idx++) {
        qualityPnsr.second[idx] /= numTilesPerGroup[qualityPnsr.first];
      }
    }
  }
}

std::vector<uint16_t> AbrAlgorithm::getBufferChunkStatus(
    ClientNetworkLayer *clientNetworkLayer, uint32_t frameId) {
  // if not all tiles are received then video chunk is not received.
  std::vector<uint16_t> numOfTilesRecvPerChunk;
  int stChunkId = ((frameId - 1) / 25) + 1;  // server adds one.
  int enChunkId = std::ceil((predictionWindow_ + frameId - 1) / 25.0) + 1;
  for (auto chunkId = stChunkId; chunkId < enChunkId; chunkId++) {
    uint16_t numberOfTilesRecieved = 0;
    for (u_int16_t tileId = 1; tileId <= 144; tileId++) {
      if (clientNetworkLayer->isReceived(chunkId, tileId) >= 1) {
        numberOfTilesRecieved++;
      }
    }
    numOfTilesRecvPerChunk.push_back(numberOfTilesRecieved);
  }
  return numOfTilesRecvPerChunk;
}

void AbrAlgorithm::buildBitrateAssigment(std::vector<uint8_t> assignment,
                                         int numOfChunkInHorizon,
                                         int numOfPossibleBitrates) {
  if (numOfChunkInHorizon == 0) {
    bitrateAssignments_.push_back(assignment);
    return;
  }
  for (uint8_t bitrate = 0; bitrate < numOfPossibleBitrates; bitrate++) {
    assignment.push_back(bitrate);
    buildBitrateAssigment(assignment, numOfChunkInHorizon - 1,
                          numOfPossibleBitrates);
    assignment.pop_back();
  }
}

void AbrAlgorithm::readTilesGroups(
    uint8_t tilesGroups[],
    std::map<uint8_t, std::vector<uint16_t>> &groupsTiles, uint8_t groupCount[],
    int numTilesW, int numbTilesH, std::string tilesGroupsFilePath) {
  std::ifstream infile(tilesGroupsFilePath);
  std::string line;
  while (std::getline(infile, line)) {
    std::vector<int> groupLine;
    try {
      folly::split(",", line, groupLine);
      auto groupId = groupLine[0];
      auto stR = groupLine[1];
      auto enR = groupLine[2];
      auto stC = groupLine[3];
      auto enC = groupLine[4];
      if (groupsTiles.find(groupId) == groupsTiles.end()) {
        groupsTiles.insert({groupId, {}});
      }
      for (auto rowIdx = stR - 1; rowIdx < enR; rowIdx++) {
        for (auto colIdx = stC; colIdx <= enC; colIdx++) {
          auto tileId = rowIdx * numTilesW + colIdx;
          tilesGroups[tileId] = groupId;
          groupsTiles[groupId].push_back(tileId);
          groupCount[groupId]++;
        }
      }
    } catch (std::invalid_argument &e) {
      LOG(ERROR) << "AbrAlgorithm::readTilesGroups(): cannot read line :"
                 << line;
    }
  }
}

void AbrAlgorithm::readChunksBitrates(
    std::map<uint8_t, std::vector<float>> &chunksBitrates,
    std::string chunkBitratesFilePath) {
  std::ifstream infile(chunkBitratesFilePath);
  uint8_t brIdx = 0;
  std::string line;
  while (std::getline(infile, line)) {
    std::vector<float> bitrates;
    line.pop_back();
    line = line.substr(1);
    try {
      folly::split(",", line, bitrates);
      chunksBitrates.insert({brIdx++, bitrates});
    } catch (std::invalid_argument &e) {
      LOG(ERROR) << "AbrAlgorithm::readChunksBitrates(): cannot read line :"
                 << line;
    }
  }
}