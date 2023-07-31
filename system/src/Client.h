/*
 * Client.h
 *
 *  Created on: Apr 24, 2021
 *      Author: eghabash
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#include <string>

class Client {

public:
  Client(std::string tilesPerFrameTracePath,
         std::string vpCorrPerFrameTracePath, std::string tileChunkSizesPath,
         std::string tileChunksQaulityPath,
         std::string backgroundDisplacementPath, std::string serverIp,
         std::string panoTilesGroupsPath, std::string panoVideoBitrate,
         std::string fullVideoChunkSizePath,
         std::string fullVideoChunkPSNRPath);
  virtual ~Client();
};

#endif /* CLIENT_H_ */
