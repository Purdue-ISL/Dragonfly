/*
 * Decoder.cpp
 *
 *  Created on: Apr 22, 2021
 *      Author: eghabash
 */

#include "Decoder.h"

int read_packet(void *opaque, uint8_t *buffer, int bufferSize) {
  struct BufferReader *br = (struct BufferReader *)opaque;
  bufferSize = FFMIN(bufferSize, br->size);

  memcpy(buffer, br->ptr, bufferSize);
  br->ptr += bufferSize;
  br->size -= bufferSize;

  return bufferSize;
}

void Decoder::initAVCodec() {
  avCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
  avCodecContext = avcodec_alloc_context3(avCodec);
  avCodecContext->width = 320;
  avCodecContext->height = 160;
  avCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
  avCodecContext->thread_count = 8;
  avCodecContext->thread_type = FF_THREAD_FRAME;
  avcodec_open2(avCodecContext, avCodec, NULL);
}

void Decoder::initCustomFormatContext() {
  formatContext = avformat_alloc_context();
  int bufferSize = 80960;
  uint8_t *buffer = (uint8_t *)(av_malloc(bufferSize));
  avioContext = avio_alloc_context(buffer, bufferSize, 0, &bufferReader,
                                   &read_packet, NULL, NULL);

  formatContext->pb = avioContext;
}

Decoder::Decoder() {
  initAVCodec();
  initCustomFormatContext();
}

Decoder::~Decoder() {
  avformat_close_input(&formatContext);
  avformat_free_context(formatContext);
  avcodec_free_context(&avCodecContext);
  av_freep(&avioContext->buffer);
  av_freep(&avioContext);
  av_freep(&avCodec);
}

int fId = 1;
void Decoder::decode(uint8_t *encodedFrame, uint32_t size,
                     std::vector<uint8_t *> &decodedTileFrames) {
  av_log_set_level(AV_LOG_PANIC);
  int ret = -1;
  AVFrame *avFrame;
  AVPacket *avPacket;
  AVFrame *pFrameARGB;
  int ret1 = -1, ret2 = -1, frameId = 0;

  formatContext = avformat_alloc_context();
  int bufferSize = 80960;
  uint8_t *buffer = (uint8_t *)(av_malloc(bufferSize));
  avioContext = avio_alloc_context(buffer, bufferSize, 0, &bufferReader,
                                   &read_packet, NULL, NULL);

  formatContext->pb = avioContext;

  bufferReader.ptr = encodedFrame;
  bufferReader.size = size;

  formatContext->pb->seekable = 0;

  // open input
  ret = avformat_open_input(&formatContext, NULL, NULL, NULL);
  // std::cout<<ret<<std::endl;
  if (ret < 0) {
    //		%%		System.out.printf("Accessing chunk failed!\n");
    //		%%		throw new IllegalStateException();
    std::cout << "Accessing chunk failed!\n";

    return;
  }

  // lookup info about the stream.
  ret = avformat_find_stream_info(formatContext, NULL);
  if (ret < 0) {
    // System.out.printf("Finding stream
    // failed!\n"); 			throw new IllegalStateException();
    std::cout << "Error finding stream info" << std::endl;
  }

  // determine the size of the buffer

  // create swscontext to transform from I420P to ARGB
  struct SwsContext *sws_ctx = sws_getContext(
      avCodecContext->width, avCodecContext->height, avCodecContext->pix_fmt,
      avCodecContext->width, avCodecContext->height, AV_PIX_FMT_YUV420P,
      SWS_BICUBIC, NULL, NULL, NULL);

  if (sws_ctx == NULL) {
    std::cout << "Sws context is Null" << std::endl;
    //%		System.out.println("Can not use sws");
    //%		throw new IllegalStateException();
  }

  // initialize AVPacket.
  avPacket = new AVPacket();
  avPacket->pts = AV_NOPTS_VALUE;
  avPacket->dts = AV_NOPTS_VALUE;
  uint8_t *buffer2 = NULL;
  int numBytes = av_image_get_buffer_size(
      AV_PIX_FMT_YUV420P, avCodecContext->width, avCodecContext->height, 1);

  while (av_read_frame(formatContext, avPacket) >= 0) {
    avFrame = av_frame_alloc();
    pFrameARGB = av_frame_alloc();

    buffer2 = static_cast<uint8_t *>(av_malloc(numBytes));
    av_image_fill_arrays(pFrameARGB->data, pFrameARGB->linesize, buffer2,
                         AV_PIX_FMT_YUV420P, avCodecContext->width,
                         avCodecContext->height, 1);

    frameId++;
    ret1 = avcodec_send_packet(avCodecContext, avPacket);

    if (ret1 < 0) {
      std::cout << "Could not decode frame" << frameId << " - send"
                << std::endl;

      continue;
    }

    ret2 = avcodec_receive_frame(avCodecContext, avFrame);
    if (ret2 < 0) {
      std::cout << "Could not decode frame" << frameId << " - receive"
                << std::endl;
      continue;
    }
    //			System.out.printf("Frame %d decoded
    // successfully!\n",frameId);

    sws_scale(sws_ctx, avFrame->data, avFrame->linesize, 0,
              avCodecContext->height, pFrameARGB->data, pFrameARGB->linesize);

    decodedTileFrames.push_back(buffer2);

    //		free(buffer2);
    av_packet_unref(avPacket);
    av_frame_free(&avFrame);
    av_frame_free(&pFrameARGB);
  }
  avformat_close_input(&formatContext);
  avformat_free_context(formatContext);
  av_freep(&avioContext->buffer);
  av_freep(&avioContext);
  sws_freeContext(sws_ctx);

  /*for (auto frame : decodedTileFrames) {
    std::ofstream myfile;
    std::string filename = std::to_string(fId++);
    const char* c = filename.c_str();
    std::cout << "Decoding" << std::endl;
    myfile.open(c, std::fstream::out);
    int idx;
    for (idx = 0; idx < 320 * 160 * 4; idx++) {
      myfile << frame[idx];
    }

    myfile.close();
    std::cout << filename << " Done" << std::endl;
  }*/
}

// int main(int argc, const char** argv)
//{
//	Decoder* decoder = new Decoder();
//
//	FILE *p_file = NULL;
//	p_file = fopen("/Users/eghabash/Desktop/360
// Video/Project-V360/split/YuvW12H12/gop30/encoded_payloadExtract/7_c_6/1.h264","rb");
//	fseek(p_file,0,SEEK_END);
//	int size = ftell(p_file);
//	fseek(p_file,0,SEEK_SET);
//
//	//std::cout<<size<<std::endl;
//
//	uint8_t * buffer = (uint8_t *) malloc(size * sizeof(uint8_t)); // Enough
// memory for the file
//
//	fread(buffer, size, 1, p_file); // Read in the entire file
//	fclose(p_file); // Close the file
//
//	int x = 5;
//
//	//buffer[size] = '\0';
//	//std::cout<<"TEST"<<std::endl;
//
//	decoder->decode(buffer, size);
//
//	return 0;
//
//}
