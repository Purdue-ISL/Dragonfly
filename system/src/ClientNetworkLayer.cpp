/*
 * ClientNetworkLayer.cpp
 *
 *  Created on: May 1, 2021
 *      Author: eghabash
 */

#include "ClientNetworkLayer.h"

#include <arpa/inet.h>
#include <folly/String.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <fstream>
#include <iostream>

#include "Util.h"
#include "glog/logging.h"

std::pair<uint32_t, uint16_t> extractTileInfo(std::string tileIndex) {
  uint32_t timestamp;
  uint16_t tileIdx;

  std::vector<std::string> tileInfo;

  boost::algorithm::split_regex(tileInfo, tileIndex, boost::regex("_"));
  // LOG(INFO) << "RECV:" << tileInfo[0] << " " << tileInfo[1];
  // presentation start time of a chunk (chunk deadline)
  // if we don't get chunk by timestamp/chunkId, it may cause a re-buffering
  // event.
  try {
    timestamp = static_cast<uint16_t>(stoi(tileInfo[0]));

    // which part of the viewport this tile corresponds to.
    tileIdx = static_cast<uint32_t>(stoi(tileInfo[1]));

  } catch (std::invalid_argument &e) {
    LOG(ERROR) << "extractTileInfo(): adding chunk\n" << tileIndex;
  }

  return std::make_pair(timestamp, tileIdx);
}

ClientNetworkLayer::ClientNetworkLayer(std::string serverIp) {
  socket_ = connectToServer(serverIp);
}

int ClientNetworkLayer::connectToServer(std::string serverIp) {
  int sock = -1;
  struct sockaddr_in serv_addr;
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    LOG(ERROR)
        << "ClientNetworkLayer::connectToServer(): Socket creation error";
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  // Convert IPv4 and IPv6 addresses from text to binary form
  if (inet_pton(AF_INET, serverIp.c_str(), &serv_addr.sin_addr) <= 0) {
    LOG(ERROR) << "ClientNetworkLayer::connectToServer(): Invalid address"
                  "Address not supported ";
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    LOG(ERROR) << "ClientNetworkLayer::connectToServer(): nConnection Failed";
    return -1;
  }

  return sock;
}

void ClientNetworkLayer::sender(ClientNetworkLayer *client) {
  std::string request;
  std::string reqHeader;
  while (true) {
    request = client->getRequest();
    if (request == "") {
      continue;
    }
    // LOG(INFO) << "Request (sender) -->"<<request;
    reqHeader = client->getRequestHeader(request);
    send(client->socket_, reqHeader.c_str(), reqHeader.size(), 0);
  }
}

void ClientNetworkLayer::receiver(ClientNetworkLayer *client,
                                  VideoPlayer *videoPlayer,
                                  BandwidthPredictor *bandwidthPredictor) {
  bool leftover = false;

  // if the last request is incomplete, this will hold the incomplete part.
  std::string leftoverString = "";

  // if we receive multiple request at a time, this vector will hold each one of
  // them. Some of the request maybe incomplete.
  std::vector<std::string> responsesVecTemp;

  // this buffer will hold the received bytes at socket.
  char *data = (char *)malloc(sizeof(char) * RESPONSE_MAX_LENGTH);

  std::map<std::string, std::string> respHeader;

  int respHeaderFileBoarderIndex = 0;

  int bytesRead = 0;

  // these timers are used to measure the download time of tile chunks.
  // stime: time when response header received (read).
  // etime: when the full chunk is received from tcp socket.
  long stime = -1;
  long etime;

  // this contains the bandwidth while downloading the tile chunk.
  float bandwidth;

  FILE *recvLog;
  std::string filename = "recv_log_" + Util::getLogTimestamp() + ".txt";
  recvLog = fopen(filename.c_str(), "wb");
  fprintf(recvLog, "%-20s %-20s %-20s %-20s %-20s %-20s \n", "chunk_id",
          "tile_idx", "quality", "chunk_size", "recv_time", "bandwidth(mbps)");

  while (true) {
    // each response contains header followed by a file.
    // both header and file end with \r\n\r\n.
    // RESPONSE_MAX_LENGTH must be less than header.

    // Mainly each response will end \r\n\r\n.
    // file must be uint8_t*.

    bzero(data, RESPONSE_MAX_LENGTH);
    bytesRead = read(client->socket_, data, RESPONSE_MAX_LENGTH);
    if (stime == -1) {
      stime = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
    }

    if (leftover) {
      // this finds if data buffer contains
      // part the response and tile bytes altogether
      // we assume that data buffer can only have either full response or
      // part of res header, part of file.
      int headerPosEndLimit = (leftoverString + data).find("\r\n\r\n");
      if (headerPosEndLimit != (int)std::string::npos) {
        // this is the index at which tile bytes resides.
        respHeaderFileBoarderIndex =
            (headerPosEndLimit + 4) - leftoverString.size();
      }
      // get response header only (take out any tile bytes.)
      boost::algorithm::split_regex(responsesVecTemp, leftoverString + data,
                                    boost::regex("\r\n\r\n"));
      leftoverString = "";
      leftover = false;
    } else {
      boost::algorithm::split_regex(responsesVecTemp, data,
                                    boost::regex("\r\n\r\n"));
    }
    if (responsesVecTemp.size() > 2) {
      // Error
      LOG(ERROR) << "ClientNetworkLayer::receiver(): client receiver side,"
                    " received multiple response headers!\n"
                    "Quick fix reduce the RESPONSE_MAX_LENGTH to 1";
      return;
    }

    if (responsesVecTemp[0].size() != 0 && responsesVecTemp.size() == 1) {
      // the response header has not fully received.
      leftoverString = responsesVecTemp[0];
      leftover = true;
    } else {
      // one response header has been received.
      // parse the header to get content length, tile index, and more info.
      respHeader = client->parseHeader(responsesVecTemp[0]);

      // this the length of tile in bytes exculding what might have already been
      // read in previous read() call.
      int chunkSize;
      try {
        // LOG(INFO) << responsesVecTemp[0];
        // length of tile + 4bytes of "\r\n\r\n"
        chunkSize = std::stoi(respHeader["Content-Length"]) + 4;

      } catch (std::invalid_argument &e) {
        LOG(ERROR) << "ClientNetworkLayer::receiver(): failed to extract "
                      "response header:\n"
                   << responsesVecTemp[0];
        return;
      }
      // allocate memory for tile.
      uint8_t *chunk = (uint8_t *)malloc(chunkSize * sizeof(uint8_t));
      if (chunk == NULL) {
        LOG(ERROR) << "ClientNetworkLayer::receiver(): Chunk did not allocat "
                      "successfully!";
      }

      // part of the file received along with response header.
      if (respHeaderFileBoarderIndex != 0) {
        // read file part from response header data buffer;
        memcpy(chunk, data + respHeaderFileBoarderIndex,
               bytesRead - respHeaderFileBoarderIndex);
      } else {
        bytesRead = 0;
      }
      int bufferPos = bytesRead - respHeaderFileBoarderIndex;
      respHeaderFileBoarderIndex = 0;
      int remainingBytes = chunkSize - bufferPos;
      // LOG(INFO) << responsesVecTemp[0] << "\n====\n";
      do {
        bytesRead = read(client->socket_, chunk + bufferPos, remainingBytes);
        bufferPos += bytesRead;
        remainingBytes -= bytesRead;
      } while (remainingBytes != 0);
      etime = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();

      // tileIndex == /<tile index>/<presentation time>.h264
      // tileInfo-->first(chunk id)/second(tile id)

      auto tileInfo = extractTileInfo(respHeader["Tile-Index"]);

      auto tileQuality = stoi(respHeader["Tile-Quality"]);

      if (client->receivedTileChunks_.find({tileInfo.first, tileInfo.second}) ==
          client->receivedTileChunks_.end()) {
        client->receivedTileChunks_.insert(
            {{tileInfo.first, tileInfo.second}, tileQuality});
      } else {
        client->receivedTileChunks_[{tileInfo.first, tileInfo.second}] =
            tileQuality;
      }

      videoPlayer->addChunk(chunk, chunkSize, tileInfo.first, tileInfo.second,
                            tileQuality);

      bandwidth =
          ((chunkSize * 8.0) / 1e6) / ((etime - stime) / 1000.0);  // mbps

      fprintf(recvLog, "%-20s %-20s %-20s %-20s %-20s %-20s \n",
              std::to_string(tileInfo.first).c_str(),
              std::to_string(tileInfo.second).c_str(),
              respHeader["Tile-Quality"].c_str(),
              std::to_string(chunkSize).c_str(),
              std::to_string(etime - stime).c_str(),
              std::to_string(bandwidth).c_str());
      fflush(recvLog);

      bandwidthPredictor->addTileInfo(chunkSize, etime - stime);

      stime = -1;
    }
  }
}

std::string ClientNetworkLayer::getRequestHeader(std::string request) {
  std::stringstream reqHeader;
  reqHeader << boost::format("GET\n%s\nHTTP/1.1\r\n") % request;
  reqHeader << boost::format("accept-encoding: %s\r\n") % "gzip, deflate";
  reqHeader << boost::format("accept-language: %s\r\n") % "en-us";
  reqHeader << boost::format("\r\n");

  return reqHeader.str();
}

std::map<std::string, std::string> ClientNetworkLayer::parseHeader(
    std::string responseHeader) {
  std::map<std::string, std::string> header;
  std::vector<std::string> headersVec;
  std::vector<std::string> splitVec;

  // get response header rows.
  folly::split("\r\n", responseHeader, headersVec);

  // parse the first row [http version and status code]
  folly::split(" ", headersVec[0], splitVec);
  header.insert(std::make_pair("HTTP version", splitVec[0]));
  header.insert(std::make_pair("Status Code", splitVec[1] + " " + splitVec[2]));
  for (uint8_t headerIdx = 1; headerIdx < headersVec.size(); headerIdx++) {
    const auto splitIdx = headersVec[headerIdx].find(':');
    if (std::string::npos != splitIdx) {
      const auto key = headersVec[headerIdx].substr(0, splitIdx);
      const auto value = headersVec[headerIdx].substr(splitIdx + 1);
      header.insert(std::make_pair(key, value));
    }
  }
  return header;
}

void ClientNetworkLayer::setRequest(std::string requestList) {
  reqMutex_.lock();
  request_ = requestList;
  reqMutex_.unlock();
}

std::string ClientNetworkLayer::getRequest() {
  reqMutex_.lock();
  std::string request = request_;
  request_ = "";
  reqMutex_.unlock();
  return request;
}

int ClientNetworkLayer::isReceived(int chunkId, uint16_t tileId) {
  if (receivedTileChunks_.find({chunkId, tileId}) ==
      receivedTileChunks_.end()) {
    return -1;
  }
  return receivedTileChunks_.find({chunkId, tileId})->second;
}

ClientNetworkLayer::~ClientNetworkLayer() {
  // TODO free class variables.
}
