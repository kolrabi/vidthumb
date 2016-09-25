#pragma once

#include "stream.hh"

#include <cstdio>

namespace vidthumb 
{

struct ZipLocalHeader;

class ZipStream : public Stream
{
public:

                        ZipStream(size_t targetWidth, size_t targetHeight);
    virtual             ~ZipStream();

    bool                GetNextFrame(Frame& frame, bool highQuality = false) override;
    bool                SkipNextFrame() override;

    void                Rewind() override;

protected:


    FILE*               pFile;
    bool 				hasValidSize;

    bool                Open(const char *pFileName) override;
    void                Close();
    bool                IsOpen() const;

    bool 				LoadFrame(Frame& frame, const ZipLocalHeader& header, const void* data, bool highQuality);
    bool 				Uncompress(const ZipLocalHeader& header, const void* pCompressed, void* pUncompressed);
};

}
