/*
 * Decoder.h
 *
 *  Created on: Apr 22, 2021
 *      Author: eghabash
 */

#ifndef DECODER_H_
#define DECODER_H_

extern "C" {

#include <inttypes.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avassert.h>
#include <libavutil/avutil.h>
#include <libavutil/file.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <fstream>
#include <iostream>
#include <vector>

struct BufferReader {
  uint8_t *ptr;
  size_t size; ///< size left in the buffer
};

class Decoder {
public:
  AVFormatContext *formatContext;
  AVCodec *avCodec;
  AVCodecContext *avCodecContext;
  AVIOContext *avioContext;
  struct BufferReader bufferReader;

  Decoder(int width, int height);

  virtual ~Decoder();

  void decodeOptimized(uint8_t *encodedFrame, uint32_t size,
                       std::vector<AVFrame *> &decodedAVFrames);

  void decodeNotOptimized(uint8_t *encodedFrame, uint32_t size,
                          std::vector<uint8_t *> &decodedTileFrames);

  void dec(uint8_t *encodedFrame, uint32_t size,
           std::vector<uint8_t *> &decodedTileFrames);

private:
  void initAVCodec(int width, int height);

  void initCustomFormatContext();
};

#endif /* DECODER_H_ */
