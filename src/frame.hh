#pragma once

#include <cstdint>
#include <cstddef>

namespace vidthumb {

class Frame
{
public:

    Frame();
    Frame(const uint8_t *pPixels, size_t width, size_t height, size_t lineStride);
    ~Frame();

    Frame& operator=(const Frame& rhs);
    Frame& operator=(Frame&& rhs);

    size_t GetWidth() const { return this->Width; }
    size_t GetHeight() const { return this->Height; }

    float GetDifference(const Frame* other);
    float GetContrast() const;

    void Save(const char *pFileName) const;

    void* CreateCairoSurface() const;

private:

    uint8_t*    pData;
    size_t      Width;
    size_t      Height;
    size_t      Stride;
};

}
