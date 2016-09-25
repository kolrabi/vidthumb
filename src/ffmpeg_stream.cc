#include "ffmpeg_stream.hh"
#include "frame.hh"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <algorithm>

namespace vidthumb {

FFMpegStream::FFMpegStream(size_t targetWidth, size_t targetHeight) :
    Stream                      { targetWidth, targetHeight },
    ResultCode                  { 0 },
    pFormatContext              { nullptr },
    VideoStreamIndex            { 0 },
    pVideoStreamCodecContext    { nullptr },
    pFrame                      { nullptr },
    pTargetFrame                { nullptr },
    pSwsContextLQ               { nullptr },
    pSwsContextHQ               { nullptr }
{
}

FFMpegStream::~FFMpegStream()
{
    this->Close();
}

bool FFMpegStream::Open(const char *pFileName)
{
    this->Close();

    // open file
    this->ResultCode = avformat_open_input(&this->pFormatContext, pFileName, nullptr, nullptr);
    if (this->ResultCode != 0) {
        this->Close();
        return false;
    }

    // find stream information
    this->ResultCode = avformat_find_stream_info(this->pFormatContext, nullptr);
    if (this->ResultCode < 0) {
        this->Close();
        return false;
    }

    av_dump_format(this->pFormatContext, 0, pFileName, 0);
    
    // find video stream
    AVCodecContext* pCodecContext = nullptr;
    for (size_t i=0; i<this->pFormatContext->nb_streams; i++) {
        AVCodecContext* pStreamCodecContext = this->pFormatContext->streams[i]->codec;
        if (pStreamCodecContext->codec_type == AVMEDIA_TYPE_VIDEO) {
            this->VideoStreamIndex = i;
            pCodecContext = pStreamCodecContext;

            auto fpsRatio = this->pFormatContext->streams[i]->avg_frame_rate;
            double fps = (double)fpsRatio.num / fpsRatio.den;
            this->totalFrameCount = fps * this->pFormatContext->duration / (double)AV_TIME_BASE;
            break;
        }
    }

    if (pCodecContext == nullptr) {
        fprintf(stderr, "Could not find video stream.\n");
        this->Close();
        return false;
    }

    // open video codec
    AVCodec* pCodec = avcodec_find_decoder(pCodecContext->codec_id);
    if (pCodec == nullptr) {
        fprintf(stderr, "Could not find suitable codec.\n");
        this->Close();
        return false;
    }

    this->pVideoStreamCodecContext = avcodec_alloc_context3(pCodec);
    this->ResultCode = avcodec_copy_context(this->pVideoStreamCodecContext, pCodecContext);
    if (this->ResultCode != 0) {
        this->Close();
        return false;
    }

    this->ResultCode = avcodec_open2(this->pVideoStreamCodecContext, pCodec, nullptr);
    if (this->ResultCode != 0) {
        this->Close();
        return false;
    }

    // set up frame buffer
    this->pFrame = av_frame_alloc();
    if (!this->pFrame) {
        fprintf(stderr, "Could not allocate frame data.\n");
        this->Close();
        return false;
    }

    this->pTargetFrame = av_frame_alloc();
    if (!this->pTargetFrame) {
        fprintf(stderr, "Could not allocate frame data.\n");
        this->Close();
        return false;
    }

    AVPixelFormat   format = AV_PIX_FMT_RGB32;
    size_t width  = this->pVideoStreamCodecContext->width;
    size_t height = this->pVideoStreamCodecContext->height;

    this->pFrameData = (uint8_t*)aligned_alloc(32, avpicture_get_size(this->pVideoStreamCodecContext->pix_fmt, width, height));

    avpicture_fill((AVPicture*)this->pFrame, this->pFrameData, this->pVideoStreamCodecContext->pix_fmt, width, height);

    // rescale target size
    float scale = std::min( (float)this->TargetWidth / width, (float)this->TargetHeight / height );
    this->TargetWidth = width*scale;
    this->TargetHeight = height*scale;

    this->pTargetFrameData = (uint8_t*)aligned_alloc(32, avpicture_get_size(format, this->TargetWidth, this->TargetHeight));
    avpicture_fill((AVPicture*)this->pTargetFrame, this->pTargetFrameData, format, this->TargetWidth, this->TargetHeight);

//    this->pSwsContext = sws_getContext(width, height, this->pVideoStreamCodecContext->pix_fmt, this->TargetWidth, this->TargetHeight, format, SWS_LANCZOS, nullptr, nullptr, nullptr);

    this->pSwsContextLQ = sws_getContext(width, height, this->pVideoStreamCodecContext->pix_fmt, this->TargetWidth, this->TargetHeight, format, SWS_POINT, nullptr, nullptr, nullptr);
    this->pSwsContextHQ = sws_getContext(width, height, this->pVideoStreamCodecContext->pix_fmt, this->TargetWidth, this->TargetHeight, format, SWS_LANCZOS, nullptr, nullptr, nullptr);

    return true;
}

void FFMpegStream::Close()
{
    if (this->ResultCode != 0) {
        switch(this->ResultCode) {
            default:
                fprintf(stderr, "Unknown error: %08x\n", this->ResultCode);
        }
    }

    if (this->pFormatContext)
        avformat_close_input(&this->pFormatContext);

    this->pFormatContext = nullptr;
    this->VideoStreamIndex = 0;

    if (this->pVideoStreamCodecContext) {
        avcodec_close(this->pVideoStreamCodecContext);
        av_free(this->pVideoStreamCodecContext);
    }
    this->pVideoStreamCodecContext = nullptr; 

    if (this->pSwsContextLQ)
        sws_freeContext(this->pSwsContextLQ);
    this->pSwsContextLQ = nullptr;

    if (this->pSwsContextHQ)
        sws_freeContext(this->pSwsContextHQ);
    this->pSwsContextHQ = nullptr;

    if (this->pFrame) 
        av_frame_free(&this->pFrame);
    this->pFrame = nullptr;

    if (this->pFrameData)
        free(this->pFrameData);
    this->pFrameData = nullptr;

    if (this->pTargetFrame) 
        av_frame_free(&this->pTargetFrame);
    this->pTargetFrame = nullptr;

    if (this->pTargetFrameData)
        free(this->pTargetFrameData);
    this->pTargetFrameData = nullptr;
}

bool FFMpegStream::IsOpen() const 
{
    return this->pFormatContext != nullptr;
}

bool FFMpegStream::GetNextFrame(Frame& frame, bool highQuality)
{
    int frameFinished = 0;
    while(!frameFinished) {
        AVPacket packet;

        this->ResultCode = av_read_frame(this->pFormatContext, &packet);
        if (this->ResultCode < 0)
            return false;

        if (packet.stream_index != (int)this->VideoStreamIndex) {
            av_free_packet(&packet);
            continue;
        }

        avcodec_decode_video2(this->pVideoStreamCodecContext, this->pFrame, &frameFinished, &packet);
        av_free_packet(&packet);
    }

    sws_scale(
        highQuality ? this->pSwsContextHQ : this->pSwsContextLQ, 
        this->pFrame->data, this->pFrame->linesize, 
        0, this->pVideoStreamCodecContext->height, 
        this->pTargetFrame->data, this->pTargetFrame->linesize
    );

    frame = Frame(this->pTargetFrame->data[0], this->TargetWidth, this->TargetHeight, this->pTargetFrame->linesize[0]);
    this->frameNum ++;
    return true;
}


bool FFMpegStream::SkipNextFrame()
{
    int frameFinished = 0;
    while(!frameFinished) {
        AVPacket packet;

        this->ResultCode = av_read_frame(this->pFormatContext, &packet);
        if (this->ResultCode < 0)
            return false;

        if (packet.stream_index != (int)this->VideoStreamIndex) {
            av_free_packet(&packet);
            continue;
        }

        avcodec_decode_video2(this->pVideoStreamCodecContext, this->pFrame, &frameFinished, &packet);
        av_free_packet(&packet);
    }
    this->frameNum ++;
    return true;
}

void FFMpegStream::Rewind()
{
    avformat_seek_file(this->pFormatContext, this->VideoStreamIndex, 0,0,0, AVSEEK_FLAG_FRAME);
    this->frameNum = 0;
}

}
