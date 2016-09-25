#include "stream.hh"
#include "ffmpeg_stream.hh"
#include "zip_stream.hh"

namespace vidthumb {

Stream* 
Stream::Open(const char *pFileName, size_t targetWidth, size_t targetHeight)
{
    Stream* pStream;

    // try zip first
    pStream = new ZipStream(targetWidth, targetHeight);
    if (pStream->Open(pFileName))
        return pStream;
    delete pStream;

    // try ffmpeg next
    pStream = new FFMpegStream(targetWidth, targetHeight);
    if (pStream->Open(pFileName))
        return pStream;
    delete pStream;

    return nullptr;
}

Stream::Stream(size_t targetWidth, size_t targetHeight) :
    TargetWidth                 { targetWidth },
    TargetHeight                { targetHeight },
    pFrameData                  { nullptr },
    pTargetFrameData            { nullptr },
    frameNum                    { 0 },
    totalFrameCount             { 0 }
{
}

Stream::~Stream()
{
}

}
