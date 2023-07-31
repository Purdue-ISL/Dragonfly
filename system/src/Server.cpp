/*
 * Server.cpp
 *
 *  Created on: Apr 24, 2021
 *      Author: eghabash
 */

#include "Server.h"

#include <folly/String.h>
#include <glog/logging.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

#include <gflags/gflags.h>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
DEFINE_bool(utilityAbr, true, "true : utility, false : flare");

Server::Server(std::string videoPathDir) {
  videoRootDir_ = videoPathDir;

  /// Users/eghabash/Desktop/System-github/Project-V360/

  uint8_t socketFD = initializeSocket();
  uint8_t socket = listenToSocket(socketFD);
  std::thread recieverThread(reciever, this, socket);
  std::thread senderThread(sender, this, socket);
  recieverThread.join();
  senderThread.join();
}

uint8_t Server::initializeSocket() {
  uint8_t socketFileDescriptor;
  struct sockaddr_in address;
  int opt = 1;

  // Creating socket file descriptor
  if ((socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(socketFileDescriptor, SOL_SOCKET, SO_REUSEADDR, &opt,
                 sizeof(opt))) {
    perror("setsockopt - Reuse-Address");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(socketFileDescriptor, SOL_SOCKET, SO_REUSEPORT, &opt,
                 sizeof(opt))) {
    perror("setsockopt - Reuse-Port");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(socketFileDescriptor, (struct sockaddr *)&address, sizeof(address)) <
      0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  return socketFileDescriptor;
}

uint8_t Server::listenToSocket(uint8_t socketFileDescriptor) {
  LOG(INFO) << "Listening @ PORT:" << PORT;
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  uint8_t newSocket;

  if (listen(socketFileDescriptor, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }
  if ((newSocket = accept(socketFileDescriptor, (struct sockaddr *)&address,
                          (socklen_t *)&addrlen)) < 0) {
    perror("accept");
    exit(EXIT_FAILURE);
  }
  LOG(INFO) << "Connected!";
  return newSocket;
}

void Server::reciever(Server *server, uint8_t socket) {
  // whether last request was received fully or not.
  bool leftover = false;

  // if the last request is incomplete, this will hold the incomplete part.
  std::string leftoverString = "";

  // if we receive multiple request at a time, this vector will hold each one of
  // them. Some of the request maybe incomplete.
  std::vector<std::string> requestsVecTemp;

  // this buffer will hold the received bytes at socket.
  char *data = (char *)malloc(sizeof(char) * REQUEST_MAX_LENGTH);

  while (true) {
    bzero(data, REQUEST_MAX_LENGTH);
    read(socket, data, REQUEST_MAX_LENGTH);

    /**
     *	if there was incomplete request,
     *	then add the incomplete part of it
     *	to the beginning of received data (leftoverString + data).
     * */
    if (leftover) {
      boost::algorithm::split_regex(requestsVecTemp, leftoverString + data,
                                    boost::regex("\r\n\r\n"));
      leftoverString = "";
      leftover = false;
    } else {
      boost::algorithm::split_regex(requestsVecTemp, data,
                                    boost::regex("\r\n\r\n"));
    }

    // loop over the requests.
    for (uint8_t idx = 0; idx < requestsVecTemp.size(); idx++) {
      if (requestsVecTemp[idx].size() != 0 &&
          idx == requestsVecTemp.size() - 1) {
        // If the last request is not empty, then it is  incomplete request
        // as it does not end with \r\n\r\n.

        leftoverString = requestsVecTemp[idx];
        leftover = true;
      } else if (requestsVecTemp[idx].size() != 0) {
        /**
         * if the request is empty, skip it.
         * Otherwise, add it. read more about boost::algorithm::split_regex:
         * https://www.boost.org/doc/libs/1_51_0/doc/html/boost/algorithm/split_regex.html
         * */
        auto tiles = server->parseUtilityRequestIntoTiles(requestsVecTemp[idx]);
        server->addTileList(tiles);
      }
    }
    // TODO check http version
    //	   check that it is a get

    // add -lboost_regex
    //		std::vector<std::string> requestRows;
    //		boost::algorithm::split_regex(requestRows, request,
    // boost::regex("\r\n"));
    //
    //		//TODO parse request.
    //
    //		//Get request URL
    //		int httpIdx = requestRows[0].find("HTTP");
    //		std::string url = requestRows[0].substr(4,httpIdx-5);
    //		//ToDo clear warning
    //		const char * filePath = (this->videoRootDir+url).c_str();
    //
    //		FILE *p_file = NULL;
    //		p_file = fopen(filePath,"rb");
    //
    //		if (!p_file)
    //		{
    //			//ToDo check error code.
    //			//send to user 404 or other error status.
    //		}
    //		else
    //		{
    //			//To get the file size skip to its EOF.
    //			//ToDo check time performance.
    //			fseek(p_file,0,SEEK_END);
    //			int size = ftell(p_file);
    //			fseek(p_file,0,SEEK_SET);
    //			// Enough memory for the file
    //			uint8_t * buffer = (uint8_t *) malloc(size *
    // sizeof(uint8_t));
    //			// Read in the entire file.
    //			fread(buffer, size, 1, p_file);
    //			// Close the file
    //			fclose(p_file);
    //
    //			//ToDo Make sure they don't get out of order.
    //			//send HTTP header.
    //			std::string header(this->getResponseHeader("1.1","200
    // OK","Bytes",size,"video/m4s",url));
    //			send(newSocket,header.c_str(),header.size(),0);
    //
    //			//send HTTP file.
    //			send(newSocket,buffer,size,0);
    //			free(buffer);
    //
    //
    //		}
    // GET 6/1.h264 HTTP/1.1\r\n",req+"?quality=720p&chunk_scheme=3
  }
}

void Server::sender(Server *server, uint8_t socket) {
  int fileSize;
  uint8_t *buffer;

  // quality, tile chunks (chunkId, tileId) vector ordered by priority to send.
  std::vector<std::string> tileLists;
  // we use tile index to keep track which tile from the list to send next.
  uint32_t tileIdx;
  // Tile Info contains chunkId, tileId, and quality
  std::vector<std::string> tileInfo;
  // to avoid head of line blocking we keep monitor our sending buffer size,
  // only start send packet/chunk when there is no data pending in buffer.
  long pendingData;
  while (true) {
    auto tileListsTemp = server->getTileList();

    // if new tile list received update current one.
    if (tileListsTemp.size() != 0) {
      // LOG(INFO)<<"New list received";
      tileLists = tileListsTemp;
      tileIdx = 0;
    }

    // if tile list is empty then skip or all tiles have been sent.
    if (tileLists.size() == 0 || tileIdx >= tileLists.size()) {
      continue;
    }

    // find a non duplicate tile to send.
    int chunkId = -1;
    for (; tileIdx < tileLists.size(); tileIdx++) {
      boost::algorithm::split_regex(tileInfo, tileLists[tileIdx],
                                    boost::regex("_"));
      try {
        chunkId = stoi(tileInfo[0]) + 1;
        uint16_t tileId = stoi(tileInfo[1]);
        uint8_t quality = stoi(tileInfo[2]);
        // check if the tile has already been sent or not.
        if (!server->isTileSent({chunkId, tileId}, quality)) {
          // mark as sent since we are going to send it.
          break;
        } else {
          LOG(INFO) << "tile already sent:" << chunkId - 1 << "_" << tileId
                    << "_" << tileInfo[2];
        }
      } catch (std::invalid_argument &e) {
        LOG(ERROR) << "Error: failed to extract tileInfo:\n"
                   << tileInfo[0] << ":" << tileInfo[2] << "-->"
                   << tileLists[tileIdx] << std::endl;
      }
    }

    // all tiles have been sent.
    if (tileIdx >= tileLists.size()) {
      continue;
    }
    // advance tile Index to next tile in the list.
    tileIdx++;
    int quality = std::stoi(tileInfo[2]);
    std::string tilePath = "";
    if (quality != 0) {
      std::string qualityIdx = std::to_string(
          server->QUALITYMAP_.find(std::stoi(tileInfo[2]))->second);
      // quality/tileId/chunkId
      tilePath = server->videoRootDir_ + "/QP" + qualityIdx + "/" +
                 tileInfo[1] + "/" + std::to_string(chunkId) + ".h264";
    } else {
      tilePath =
          server->videoRootDir_ + "/QP00/" + std::to_string(chunkId) + ".h264";
    }
    LOG(INFO) << "tile Sent:" << std::to_string(chunkId - 1) << "_"
              << tileInfo[1] << "_" << tileInfo[2] << "\n";
    char *filePath = new char[tilePath.length() + 1];
    strcpy(filePath, tilePath.c_str());
    FILE *p_file = NULL;
    p_file = fopen(filePath, "r");
    if (!p_file) {
      LOG(ERROR) << "Server::sender(): chunk file not found (" << filePath
                 << ")\n";
      // ToDo check error code.
      // send to user 404 or other error status.
      continue;
    }
    delete[] filePath;

    fseek(p_file, 0, SEEK_END);
    fileSize = ftell(p_file);
    fseek(p_file, 0, SEEK_SET);
    // Enough memory for the file
    buffer = (uint8_t *)malloc((fileSize + 4) * sizeof(uint8_t));
    if (buffer == NULL) {
      LOG(ERROR) << "Server::sender(): malloc buffer did not succeed!";
    }
    // Read in the entire file.
    fread(buffer, fileSize, 1, p_file);
    // Close the file
    fclose(p_file);
    // mark response end.
    buffer[fileSize] = '\r';
    buffer[fileSize + 1] = '\n';
    buffer[fileSize + 2] = '\r';
    buffer[fileSize + 3] = '\n';

    // send header
    std::string header(server->getResponseHeader(
        "1.1", "200 OK", "Bytes", fileSize, "video/m4s",
        std::to_string(chunkId) + "_" + tileInfo[1], tileInfo[2]));
    VLOG(1) << "\n" << header << "-------";

    ioctl(socket, SIOCOUTQ, &pendingData);
    LOG(INFO) << "Pending data in Buffer-before:" << pendingData << " Bytes";
    LOG(INFO) << "Server_sending[" << chunkId << "-" << tileInfo[2]
              << "], size:" << header.size() + fileSize + 4;
    send(socket, header.c_str(), header.size(), 0);
    // send file.
    send(socket, buffer, fileSize + 4, 0);
    ioctl(socket, SIOCOUTQ, &pendingData);
    LOG(INFO) << "Pending data in Buffer-afterSend:" << pendingData << " Bytes";
    // wait until tcp buffer is empty.
    while (pendingData > 5 * 1e3) {
      ioctl(socket, SIOCOUTQ, &pendingData);
    }
    ioctl(socket, SIOCOUTQ, &pendingData);
    LOG(INFO) << "Pending data in Buffer-afterWait:" << pendingData
              << " Bytes\n-----";

    free(buffer);
  }
}

std::string Server::getResponseHeader(
    std::string httpVersion, std::string statusCode, std::string acceptRange,
    int contentLength, std::string contentType, std::string tileIdx,
    std::string quality) {
  std::stringstream header;

  time_t now = time(0);
  tm *gmtm = gmtime(&now);
  std::string dt(asctime(gmtm));

  header << boost::format("HTTP%s %s\r\n") % httpVersion % statusCode;
  header << boost::format("Tile-Index: %s\r\n") % tileIdx;
  header << boost::format("Tile-Quality:%s\r\n") % quality;
  header << boost::format("Date: %s\r\n") % dt.erase(dt.size() - 1);
  header << boost::format("Accept-Ranges: %s\r\n") % acceptRange;
  header << boost::format("Content-Length: %d\r\n") % contentLength;
  header << boost::format("Content-Type: %s\r\n") % contentType;
  header << "\r\n";
  return header.str();
}

std::vector<std::string> Server::parseUtilityRequestIntoTiles(
    std::string request) {
  // LOG(INFO) << "Request_server:" << request;
  std::vector<std::string> tempVec1;
  std::vector<std::string> tempVec2;
  boost::algorithm::split_regex(tempVec1, request, boost::regex("\n"));
  std::vector<std::string> tilesReq;
  LOG(INFO) << "Request_server:" << tempVec1[2];
  boost::algorithm::split_regex(tilesReq, tempVec1[2], boost::regex(","));
  std::vector<std::string> tiles;
  for (auto const &tile : tilesReq) {
    if (tile == "" || tile == "Quality") {
      continue;
    }
    tiles.push_back(tile);
    // std::cout<<tile<<"-->"<<std::to_string(chunkId)
    // +"_0_"+tile.substr(pos+1)<<std::endl;
  }
  return tiles;
}

uint8_t Server::parseRequestIntoQuality(std::string request) {
  std::vector<std::string> tempVec;
  boost::algorithm::split_regex(tempVec, request, boost::regex("Quality"));
  boost::algorithm::split_regex(tempVec, tempVec[1], boost::regex("HTTP/1.1"));
  boost::algorithm::split_regex(tempVec, tempVec[0], boost::regex("\n"));
  try {
    uint8_t quality = static_cast<uint8_t>(stoi(tempVec[1]));
    return quality;
  } catch (std::invalid_argument &e) {
    LOG(ERROR) << "Error: failed to extract quality:\n"
               << tempVec[1] << std::endl;
    return 100;
  }
}

void Server::addTileList(std::vector<std::string> tiles) {
  reqMutex_.lock();
  request_ = tiles;
  reqMutex_.unlock();
}

bool Server::isTileSent(std::pair<int, uint16_t> tile, uint8_t quality) {
  if (tilesSent_.find(tile) == tilesSent_.end()) {
    tilesSent_.insert({tile, quality});
    return false;
  }
  if (FLAGS_utilityAbr == 0 || quality <= 1) {
    return true;
  }
  uint8_t qRec = tilesSent_[tile];
  if (qRec > 1) {
    return true;
  }
  tilesSent_[tile] = quality;
  return false;
}

std::vector<std::string> Server::getTileList() {
  std::vector<std::string> requestToReturn;
  reqMutex_.lock();
  requestToReturn = request_;
  request_ = {};
  reqMutex_.unlock();
  return requestToReturn;
}

Server::~Server() {
  // TODO Auto-generated destructor stub
}

void start(std::string videoPathDir) {
  Server *server = new Server(videoPathDir);
  // to suppress warning
  assert(server != nullptr);
}

int main(int argc, char **argv) {
  google::SetLogDestination(google::INFO, "server_log.txt");
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (std::stoi(argv[2]) == 0) {
    FLAGS_utilityAbr = false;
  }
  std::thread serverThread(start, argv[1]);

  serverThread.join();
  return 0;
}
