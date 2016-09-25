#pragma once

#include "stream.hh"

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;

namespace vidthumb 
{

class FFMpegStream : public Stream
{
public:

                        FFMpegStream(size_t targetWidth, size_t targetHeight);
    virtual             ~FFMpegStream();

    bool                GetNextFrame(Frame& frame, bool highQuality = false) override;
    bool                SkipNextFrame() override;

    void                Rewind() override;

protected:

    int                 ResultCode;
    AVFormatContext*    pFormatContext;
    size_t              VideoStreamIndex;
    AVCodecContext*     pVideoStreamCodecContext;

    AVFrame*            pFrame;
    AVFrame*            pTargetFrame;

    SwsContext*         pSwsContextLQ;
    SwsContext*         pSwsContextHQ;

    bool                Open(const char *pFileName) override;
    void                Close();
    bool                IsOpen() const;
};

}
