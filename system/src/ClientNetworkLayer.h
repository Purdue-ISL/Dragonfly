/*
 * ClientNetworkLayer.h
 *
 *  Created on: May 1, 2021
 *      Author: eghabash
 */

#ifndef CLIENTNETWORKLAYER_H_
#define CLIENTNETWORKLAYER_H_

#include <boost/functional/hash.hpp>
#include <map>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "BandwidthPredictor.h"
#include "VideoPlayer.h"

#define RESPONSE_MAX_LENGTH 100
#define PORT 7717
#define PRIORITY_LEVELS 10

class ClientNetworkLayer {
  // set of tiles to be requests, filled by ABR algorithm.
  std::string request_;

  // To synchronize read and write the request List.
  std::mutex reqMutex_;

  // TODO document what each function does.

  int socket_;

  // chunkidx, tileidx --> quality.
  std::map<std::pair<int, uint16_t>, uint8_t> receivedTileChunks_;

  int connectToServer(std::string serverIp);

  std::string getRequest();

  std::string getRequestHeader(std::string request);

  std::map<std::string, std::string> parseHeader(std::string responseHeader);

public:
  ClientNetworkLayer(std::string serverIp);

  void static sender(ClientNetworkLayer *client);

  void static receiver(ClientNetworkLayer *client, VideoPlayer *videoPlayer,
                       BandwidthPredictor *bandwidthPredictor);

  /**
   * @brief This function returns the qualtiy of received tiles
   *
   * @param chunkId
   * @param tileId
   * @return int: returns the quauliy of tile(1 == lowest quality),
   *                  otherwise -1.
   */
  int isReceived(int chunkId, uint16_t tileId);

  void setRequest(std::string requestList);

  virtual ~ClientNetworkLayer();
};

#endif /* CLIENTNETWORKLAYER_H_ */
