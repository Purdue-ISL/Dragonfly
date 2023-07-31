/*
 * VideoPlayer.h
 *
 *  Created on: May 1, 2021
 *      Author: eghabash
 */

#ifndef VIDEOPLAYER_H_
#define VIDEOPLAYER_H_

#include <cstdint>
#include <map>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "Decoder.h"
#include "TilePredictor.h"

class VideoPlayer {
  struct Chunk {
    // pointer to received chunk bytes in memory;
    uint8_t *chunk;

    // chunk size in bytes
    uint32_t chunkSize;

    // chunk quality 1==lowest
    uint8_t qualityIdx;
  };

  // linked list to order the tiles in stitched frame.
  template <typename T> struct Node {
    T *tile;
    Node *nextTile;
  };

  std::mutex decodedTileChunksMutex_;

  uint8_t FPS_;

  uint32_t frameId_;

  std::map<uint32_t, std::map<uint16_t, struct Chunk>> chunks_;

  std::map<int, struct Chunk> chunksBg_;

  // key: frameId, value: set of all tiles to skip.
  std::map<uint32_t, std::unordered_set<uint16_t>> tilesInFrameToSkip;

  // Per frame, what are the tiles in the viewport.
  std::map<uint32_t, std::vector<uint16_t>> groundTruth_;

  std::vector<std::pair<float, float>> groundTruthCoordinates_;

  // per presentation time "chunk", per tile-index, the decode tile-frames.
  std::map<uint32_t,
           std::map<uint16_t, std::pair<std::vector<uint8_t *>, uint8_t>>>
      decodedTileChunks_;

  template <typename T>
  void stitchTileFrame(std::map<uint16_t, T *> &viewport, int frameId,
                       int framePauseCount);

  // This function will take viewport map as input which contains
  // the tiles to construct the viewport frame. And,
  // returns vector of linkedlist. The nodes in the vector
  // correspond to the first tiles in each row.
  template <typename T>
  void orderTilesToLinkedList(std::map<uint16_t, T *> &viewport,
                              std::vector<Node<T> *> &viewportLinkedList);
  std::mutex recvChunKMutex_;
  std::mutex recvBgChunKMutex_;
  std::mutex skipTileMutex_;

  bool skipThisTile(uint16_t tileId);

  void freeSkipTileMapCurrentFrame();

  std::vector<uint16_t> getTiles(TilePredictor *tilePredictor,
                                 std::pair<float, float> vpCorr);

  std::pair<float, float> getVpCorrInRealTime();

  std::ifstream userVpCorr;

public:
  static void startVideoWithRebuffer(VideoPlayer *videoPlayer,
                                     TilePredictor *tilePredictor);

  static void startVideoJournal(VideoPlayer *videoPlayer,
                                TilePredictor *tilePredictor, bool rebuffer);

  static void startVideoWithSkip(VideoPlayer *videoPlayer,
                                 TilePredictor *tilePredictor);

  static void startVideoLive(VideoPlayer *videoPlayer,
                             TilePredictor *tilePredictor);

  // this frees the raw tiles of the past chunks (chunkIdx-2 and chunkIdx-1)
  void freePastChunks(uint16_t chunkIdx);

  void fillViewportTiles(VideoPlayer *videoPlayer, std::vector<uint16_t> &tiles,
                         std::map<uint16_t, uint8_t *> &viewport,
                         std::string &tilesQuality);

  void decodeBackground(VideoPlayer *videoPlayer, Decoder *decoderBG);

  static void decode(VideoPlayer *videoPlayer, TilePredictor *tilePredictor,
                     Decoder *decoderEL, Decoder *decoderBG);

  VideoPlayer(std::string tilesPerFrameTracePath,
              std::string vpCorrPerFrameTracePath);

  void addChunk(uint8_t *chunkPointer, uint32_t chunkSize,
                uint32_t tileChunkIdx, uint16_t tileIdx, uint8_t quality);

  uint32_t getFrameToRenderId();

  void setTileToSkip(uint32_t frameId, uint16_t tile);

  virtual ~VideoPlayer();
};

#endif /* VIDEOPLAYER_H_ */
