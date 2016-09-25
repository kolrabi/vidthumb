#include "zip_stream.hh"
#include "frame.hh"

#include <cstring>
#include <zlib.h>

#include <IL/il.h>
#include <IL/ilu.h>

#include <algorithm>

namespace vidthumb {

struct ZipLocalHeader {
  uint8_t  magic[4];
  uint16_t version;
  uint16_t flags;
  uint16_t method;
  uint16_t time;
  uint16_t date;
  uint32_t crc32;
  uint32_t compressedSize;
  uint32_t uncompressedSize;
  uint16_t nameLength;
  uint16_t extraLength;
} __attribute__ ((packed));

ZipStream::ZipStream(size_t targetWidth, size_t targetHeight) :
    Stream                      { targetWidth, targetHeight },
    pFile                       { nullptr },
    hasValidSize                { nullptr }
{
    static bool isInited = false;

    if (!isInited) {
        ilInit();
        iluInit();
        isInited = true;
    }

}

ZipStream::~ZipStream()
{
    this->Close();
}

bool ZipStream::Open(const char *pFileName)
{
    this->Close();

    this->pFile = fopen(pFileName, "rb");
    if (!pFile)
        return false;

    ZipLocalHeader header;
    if (!fread(&header, sizeof(header), 1, this->pFile)) {
        this->Close();
        return false;
    }

    if (::memcmp(header.magic, "PK\x03\x04", 4)) {
        this->Close();
        return false;
    }

    do {
        this->totalFrameCount++;

        fseek(this->pFile, header.nameLength, SEEK_CUR);
        fseek(this->pFile, header.extraLength, SEEK_CUR);
        fseek(this->pFile, header.compressedSize, SEEK_CUR);
    } while(fread(&header, sizeof(header), 1, this->pFile));

    this->Rewind();
    return true;
}

void ZipStream::Close()
{
    if (this->pFile) {
        fclose(this->pFile);
        this->pFile = nullptr;
    }
}

bool ZipStream::IsOpen() const 
{
    return this->pFile != nullptr;
}

bool ZipStream::GetNextFrame(Frame& frame, bool highQuality)
{
    ZipLocalHeader header;
    while(fread(&header, sizeof(header), 1, this->pFile)) {
        char* name = (char*)alloca(header.nameLength+1);

        fread(name, header.nameLength, 1, this->pFile);
        name[header.nameLength] = 0;

        //fseek(this->pFile, header.nameLength, SEEK_CUR);
        fseek(this->pFile, header.extraLength, SEEK_CUR);

        if (header.compressedSize == 0)
            continue;

        uint8_t* data = new uint8_t[header.compressedSize];
        if (!fread(data, header.compressedSize, 1, this->pFile)) {
            delete [] data;
            return false;
        }

        if (!strstr(name, ".thumb") && this->LoadFrame(frame, header, data, highQuality)) {
            delete [] data;
            return true;
        }

        delete [] data;
    }

    return false;
}


bool ZipStream::SkipNextFrame()
{
    ZipLocalHeader header;
    if (!fread(&header, sizeof(header), 1, this->pFile)) {
        return false;
    }
    
    fseek(this->pFile, header.nameLength, SEEK_CUR);
    fseek(this->pFile, header.extraLength, SEEK_CUR);
    fseek(this->pFile, header.compressedSize, SEEK_CUR);

    return true;
}

void ZipStream::Rewind()
{
    fseek(this->pFile, 0, SEEK_SET);
    this->frameNum = 0;
}

bool ZipStream::LoadFrame(Frame& frame, const ZipLocalHeader& header, const void* data, bool highQuality)
{
    uint8_t* pUncompressed = new uint8_t[header.uncompressedSize];
    if (!this->Uncompress(header, data, pUncompressed)) {
        delete [] pUncompressed;
        return false;
    }

    bool success = true;
    ILuint image = ilGenImage();
    ilBindImage(image);

    if (!ilLoadL(IL_TYPE_UNKNOWN, pUncompressed, header.uncompressedSize)) {
        success = false;
    } else {
            // rescale target size
            ILuint width = ilGetInteger(IL_IMAGE_WIDTH);
            ILuint height = ilGetInteger(IL_IMAGE_HEIGHT);
            float scale = std::min( (float)this->TargetWidth / width, (float)this->TargetHeight / height );
            width = width*scale;
            height = height*scale;

            ilConvertImage(IL_BGRA, IL_UNSIGNED_BYTE);

        iluImageParameter(ILU_FILTER, highQuality ? ILU_BILINEAR : ILU_NEAREST);
        iluScale(width, height, 1);

        uint8_t* pPixels = new uint8_t[width*height*4];
        ilCopyPixels(0,0,0, width, height, 1, IL_BGRA, IL_UNSIGNED_BYTE, pPixels);
        frame = Frame(pPixels, width, height, width*4);
        delete [] pPixels;
    }

    ilDeleteImage(image);

    delete [] pUncompressed;
    return success;
}

bool ZipStream::Uncompress(const ZipLocalHeader& header, const void* pCompressed, void* pUncompressed)
{
    switch(header.method) {
        // store
        case 0: {
            ::memcpy(pUncompressed, pCompressed, header.uncompressedSize);
            return true;
        }

         // deflate
        case 8: {
            int       result;
            z_stream  stream;

            stream.zalloc     = Z_NULL;
            stream.zfree      = Z_NULL;
            stream.opaque     = Z_NULL;
            stream.avail_in   = 0;
            stream.next_in    = Z_NULL;
            stream.avail_out  = 0;
            stream.next_out   = Z_NULL;

            result = inflateInit2(&stream, -MAX_WBITS);
            if (result != Z_OK) {
                return false;
            }

            stream.avail_in   = header.compressedSize;
            stream.next_in    = (Bytef*)pCompressed;
            stream.avail_out  = header.uncompressedSize;
            stream.next_out   = (Bytef*)pUncompressed;

            return inflate(&stream, Z_FINISH) == Z_STREAM_END;
        }
    }
    return false;
}

}
