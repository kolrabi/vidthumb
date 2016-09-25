#pragma once

#include <cstdint>
#include <cstddef>

namespace vidthumb 
{

class Frame;

class Stream
{
public:

    static Stream*      Open(const char *pFileName, size_t targetWidth, size_t targetHeight);

    virtual             ~Stream();

    virtual bool        GetNextFrame(Frame& frame, bool highQuality = false) = 0;
    virtual bool        SkipNextFrame() = 0;
    virtual void        Rewind() = 0;

    size_t              GetTargetWidth() const { return this->TargetWidth; }
    size_t              GetTargetHeight() const { return this->TargetHeight; }

    size_t              GetFrameNum() const { return this->frameNum; }
    size_t              GetTotalFrameCount() const { return this->totalFrameCount; }

protected:

                        Stream(size_t targetWidth, size_t targetHeight);

    virtual bool        Open(const char *pFileName) = 0;

    size_t              TargetWidth;
    size_t              TargetHeight;

    uint8_t*            pFrameData;
    uint8_t*            pTargetFrameData;

    size_t              frameNum;
    size_t              totalFrameCount;
};

}
