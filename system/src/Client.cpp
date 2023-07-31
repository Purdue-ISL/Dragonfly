/*
 * Client.cpp
 *
 *  Created on: May 2, 2021
 *      Author: eghabash
 */

#include "Client.h"

#include <thread>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "AbrAlgorithm.h"
#include "ClientNetworkLayer.h"
#include "Decoder.h"
#include "VideoPlayer.h"
DEFINE_string(model, "Utility", "Utility, Pano, Flare, Journal");
DEFINE_string(bufferModel, "skip", "skip, rebuffer, or live");
DEFINE_int32(predictionWindow, 1, " prediction window in seconds");

Client::Client(std::string tilesPerFrameTracePath,
               std::string vpCorrPerFrameTracePath,
               std::string tileChunkSizesPath,
               std::string tileChunksQaulityPath,
               std::string backgroundDisplacementPath, std::string serverIp,
               std::string panoTilesGroupsPath, std::string panoVideoBitrate,
               std::string fullVideoChunkSizePath,
               std::string fullVideoChunkPSNRPath) {
  // Initiate all instances to create threads.
  // 1- Network layer (sender and receiver).
  // 2- Video player along with the decoder.
  // 3- ABR algorithm along with tile and bandwidth predictors.
  ClientNetworkLayer *clientNetworkLayer = new ClientNetworkLayer(serverIp);
  VideoPlayer *videoPlayer =
      new VideoPlayer(tilesPerFrameTracePath, vpCorrPerFrameTracePath);
  Decoder *decoderEL = new Decoder(320, 160);
  Decoder *decoderBG = new Decoder(3840, 1920);
  AbrAlgorithm *abr =
      new AbrAlgorithm(tileChunkSizesPath, tileChunksQaulityPath,
                       backgroundDisplacementPath, fullVideoChunkSizePath,
                       fullVideoChunkPSNRPath, (size_t)FLAGS_predictionWindow);
  TilePredictor *tilePredictor = new TilePredictor(
      vpCorrPerFrameTracePath, FLAGS_model, (size_t)FLAGS_predictionWindow);
  BandwidthPredictor *bandwidthPredictor = new BandwidthPredictor();

  // Start all threads:
  // 1- receiver thread: to recive tiles.
  // 2- video player thread: to stitch and play viewport frames.
  // 3- decoder thread: to decode tiles.
  // 4- sender thread: to send requests for the wanted tiles.
  // 5- abr thread: to estimate what tiles to request and in what quality.
  std::thread recvThread(ClientNetworkLayer::receiver, clientNetworkLayer,
                         videoPlayer, bandwidthPredictor);

  std::thread videoPlayerThread;

  if (FLAGS_model == "Journal") {
    videoPlayerThread =
        std::thread(VideoPlayer::startVideoJournal, videoPlayer, tilePredictor,
                    FLAGS_bufferModel == "skip" ? false : true);

  } else if (FLAGS_bufferModel == "skip") {
    videoPlayerThread = std::thread(VideoPlayer::startVideoWithSkip,
                                    videoPlayer, tilePredictor);
  } else if (FLAGS_bufferModel == "rebuffer") {
    videoPlayerThread = std::thread(VideoPlayer::startVideoWithRebuffer,
                                    videoPlayer, tilePredictor);
  } else if (FLAGS_bufferModel == "UtilityJskip") {
    videoPlayerThread = std::thread(VideoPlayer::startVideoJournal, videoPlayer,
                                    tilePredictor, false);

  } else {
    videoPlayerThread =
        std::thread(VideoPlayer::startVideoLive, videoPlayer, tilePredictor);
  }

  std::thread videoPlayerDecoderThread(VideoPlayer::decode, videoPlayer,
                                       tilePredictor, decoderEL, decoderBG);

  std::thread senderThread(ClientNetworkLayer::sender, clientNetworkLayer);
  std::thread abrThread;
  if (FLAGS_model == "Utility") {
    abrThread =
        std::thread(AbrAlgorithm::utilityAbr, abr, tilePredictor,
                    bandwidthPredictor, clientNetworkLayer, videoPlayer);
    LOG(INFO) << "Utility";
  } else if (FLAGS_model == "Flare") {
    abrThread =
        std::thread(AbrAlgorithm::flareAbr, abr, tilePredictor,
                    bandwidthPredictor, clientNetworkLayer, videoPlayer);
    LOG(INFO) << "Flare";
  } else if (FLAGS_model == "Pano") {
    abrThread = std::thread(AbrAlgorithm::panoAbr, abr, tilePredictor,
                            bandwidthPredictor, clientNetworkLayer, videoPlayer,
                            panoTilesGroupsPath, panoVideoBitrate);
    LOG(INFO) << "Pano";
  } else {
    abrThread =
        std::thread(AbrAlgorithm::journalAbr, abr, tilePredictor,
                    bandwidthPredictor, clientNetworkLayer, videoPlayer);
    LOG(INFO) << "Journal";
  }

  videoPlayerThread.join();

  // abrThread.join();
  // senderThread.join();
  // recvThread.join();
  // videoPlayerDecoderThread.join();
}

Client::~Client() {}

int main(int argc, char **argv) {
  if (argc < 6) {
    LOG(ERROR)
        << "Usage: ./client <tiles_per_frame_trace> "
           "<vp_corrdinates_per_frame> <tile_chunk_sizes> <tile_chunk_quality>"
           "<background_displacement> <server_ip>";
    return -1;
  }
  if (FLAGS_model != "Flare" && FLAGS_model != "Pano" &&
      FLAGS_model != "Utility") {
    LOG(ERROR) << "Model must be either Utility, Flare, or Pano";
    return -1;
  }

  if (FLAGS_model == "Pano" && argc < 8) {
    LOG(ERROR)
        << "Usage: ./client <tiles_per_frame_trace> "
           "<vp_corrdinates_per_frame> <tile_chunk_sizes> <tile_chunk_quality>"
           "<background_displacement> <server_ip> <pano_tile_grouping> "
           "<pano_video_bitrate>";
    return -1;
  }

  google::SetLogDestination(google::INFO, "client_log.txt");
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  Client *client = nullptr;
  if (FLAGS_model == "Pano") {
    client = new Client(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6],
                        argv[7], argv[8], "", "");
  } else if (FLAGS_model == "Journal") {
    client = new Client(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6],
                        "", "", argv[7], "");

  } else if (FLAGS_model == "Utility" && FLAGS_bufferModel == "UtilityJskip") {
    client = new Client(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6],
                        "", "", argv[7], argv[8]);
  } else {
    client = new Client(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6],
                        "", "", "", "");
  }
  // to suppress warning
  assert(client != nullptr);
  return 0;
}
