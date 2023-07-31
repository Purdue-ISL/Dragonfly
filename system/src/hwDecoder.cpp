/*
 * Decoder.cpp
 *
 *  Created on: Apr 22, 2021
 *      Author: eghabash
 */
// ISSUE: Videotoolbox has a lower limit on the resolution it can use with
// hardware
// https://trac.ffmpeg.org/ticket/5595?cversion=0&cnum_hist=1

#include "Decoder.h"
#include "Util.h"

int read_packet(void *opaque, uint8_t *buffer, int bufferSize) {
  struct BufferReader *br = (struct BufferReader *)opaque;
  bufferSize = FFMIN(bufferSize, br->size);

  memcpy(buffer, br->ptr, bufferSize);
  br->ptr += bufferSize;
  br->size -= bufferSize;

  return bufferSize;
}

static AVBufferRef *hw_device_ctx = NULL;
static enum AVPixelFormat hw_pix_fmt;
static FILE *output_file = NULL;

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts) {
  const enum AVPixelFormat *p;

  for (p = pix_fmts; *p != -1; p++) {
    if (*p == hw_pix_fmt)
      return *p;
  }

  fprintf(stderr, "Failed to get HW surface format.\n");
  return AV_PIX_FMT_NONE;
}

static int hw_decoder_init(AVCodecContext *ctx,
                           const enum AVHWDeviceType type) {
  int err = 0;

  if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type, NULL, NULL, 0)) < 0) {
    fprintf(stderr, "Failed to create specified HW device.\n");
    return err;
  }
  ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
  ctx->width = 320;
  ctx->height = 160;
  ctx->pix_fmt = AV_PIX_FMT_YUV420P;

  return err;
}

static int decode_write(AVCodecContext *avctx, AVPacket *packet) {
  AVFrame *frame = NULL, *sw_frame = NULL;
  AVFrame *tmp_frame = NULL;
  uint8_t *buffer = NULL;
  int size;
  int ret = 0;
  ret = avcodec_send_packet(avctx, packet);
  if (ret < 0) {
    std::cout << "ERROR1" << std::endl;
    return ret;
  }

  while (1) {
    if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
      std::cout << "ERROR2" << std::endl;
      ret = AVERROR(ENOMEM);
      goto fail;
    }

    ret = avcodec_receive_frame(avctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      av_frame_free(&frame);
      av_frame_free(&sw_frame);
      return 0;
    } else if (ret < 0) {
      std::cout << "ERROR3" << std::endl;
      goto fail;
    }

    if (frame->format == hw_pix_fmt) {
      /* retrieve data from GPU to CPU */
      if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
        std::cout << "ERROR4" << std::endl;
        goto fail;
      }
      tmp_frame = sw_frame;
    } else
      tmp_frame = frame;

    size = av_image_get_buffer_size(AVPixelFormat(tmp_frame->format),
                                    tmp_frame->width, tmp_frame->height, 1);
    buffer = (uint8_t *)av_malloc(size);
    if (!buffer) {
      std::cout << "ERROR5" << std::endl;
      ret = AVERROR(ENOMEM);
      goto fail;
    }
    ret = av_image_copy_to_buffer(
        buffer, size, (const uint8_t *const *)tmp_frame->data,
        (const int *)tmp_frame->linesize, AVPixelFormat(tmp_frame->format),
        tmp_frame->width, tmp_frame->height, 1);
    if (ret < 0) {
      std::cout << "ERROR6" << std::endl;
      goto fail;
    }
    if ((ret = fwrite(buffer, 1, size, output_file)) < 0) {
      std::cout << "ERROR7" << std::endl;
      goto fail;
    }

  fail:
    av_frame_free(&frame);
    av_frame_free(&sw_frame);
    av_freep(&buffer);
    if (ret < 0)
      return ret;
  }
}

void Decoder::dec(uint8_t *encodedFrame, uint32_t size,
                  std::vector<uint8_t *> &decodedTileFrames) {
  int video_stream, ret;
  AVStream *video = NULL;
  AVCodecContext *decoder_ctx = NULL;
  AVCodec *decoder = NULL;
  AVPacket packet;
  enum AVHWDeviceType type;
  int i;
  AVFormatContext *input_ctx = NULL;

  av_register_all();

  type = av_hwdevice_find_type_by_name(
      av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VIDEOTOOLBOX));
  hw_pix_fmt = AV_PIX_FMT_VIDEOTOOLBOX;

  /* open the input file */
  if (avformat_open_input(&input_ctx, "/Users/eghabash/Desktop/video_org.mp4",
                          NULL, NULL) != 0) {
    return;
  }
  if (avformat_find_stream_info(input_ctx, NULL) < 0) {
    return;
  }

  ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);

  video_stream = ret;

  if (!(decoder_ctx = avcodec_alloc_context3(decoder)))
    return;

  video = input_ctx->streams[video_stream];

  if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0) {
    return;
  }

  decoder_ctx->get_format = get_hw_format;

  av_opt_set_int(decoder_ctx, "refcounted_frames", 1, 0);
  if (hw_decoder_init(decoder_ctx, type) < 0) {
    return;
  }

  if ((ret = avcodec_open2(decoder_ctx, decoder, NULL)) < 0) {
    return;
  }

  output_file = fopen("/Users/eghabash/Desktop/raw", "w+b");

  int cc = 1;
  /* actual decoding and dump the raw data */
  while (ret >= 0) {
    if ((ret = av_read_frame(input_ctx, &packet)) < 0)
      break;
    if (video_stream == packet.stream_index) {
      long x = Util::getTime();
      ret = decode_write(decoder_ctx, &packet);
      std::cout << "Frame#" << cc++ << " :" << (Util::getTime() - x)
                << std::endl;
    }
    av_packet_unref(&packet);
  }

  /* flush the decoder */
  packet.data = NULL;
  packet.size = 0;
  ret = decode_write(decoder_ctx, &packet);
  av_packet_unref(&packet);

  if (output_file)
    fclose(output_file);
  avcodec_free_context(&decoder_ctx);
  av_buffer_unref(&hw_device_ctx);
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
    //		%%		System.out.printf("Accessing chunk
    // failed!\n");
    //		%%		throw new IllegalStateException();
    std::cout << "Accessing chunk failed!\n";

    return;
  }

  // lookup info about the stream.
  //		ret = avformat_find_stream_info(formatContext,
  //(PointerPointer)null); 		if (ret < 0) {
  // System.out.printf("Finding stream
  // failed!\n"); 			throw new IllegalStateException();
  //		}
  //
  //		// looking for stream id.
  //		for (int i = 0; i < formatContext.nb_streams(); i++) {
  //			if (formatContext.streams(i).codecpar().codec_type() ==
  // AVMEDIA_TYPE_VIDEO) { 				videoStreamIdx = i;
  // break;
  //			}
  //		}
  //		if (videoStreamIdx == -1) {
  //			System.out.println("Cannot find video stream");
  //			throw new IllegalStateException();
  //		} else {
  //			System.out.printf("Video stream %d with resolution
  //%dx%d\n", videoStreamIdx,
  // formatContext.streams(videoStreamIdx).codecpar().width(),
  //					formatContext.streams(videoStreamIdx).codecpar().height());
  //		}
  //
  //		avcodec_parameters_to_context(avCodecContext,
  //				formatContext.streams(videoStreamIdx).codecpar());

  // determine the size of the buffer

  // create swscontext to transform from I420P to ARGB
  struct SwsContext *sws_ctx = sws_getContext(
      avCodecContext->width, avCodecContext->height, avCodecContext->pix_fmt,
      avCodecContext->width, avCodecContext->height, AV_PIX_FMT_YUV420P,
      SWS_FAST_BILINEAR, NULL, NULL, NULL);

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
