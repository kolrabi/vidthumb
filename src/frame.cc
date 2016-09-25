#include "frame.hh"

#include <cairo/cairo.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cassert>
#include <iostream>

namespace vidthumb {

static inline float PixelDiff(const uint8_t* pRGB1, const uint8_t* pRGB2)
{
    float diff = 0.0f;
    for (int i=0; i<3; i++) {
        float d = (pRGB1[i]-pRGB2[i])/255.0f;
        diff += d*d;
    }
    return diff;
}

Frame::Frame() :
    pData   { nullptr },
    Width   { 0 },
    Height  { 0 },
    Stride  { 0 }
{
}

Frame::Frame(const uint8_t *pPixels, size_t width, size_t height, size_t lineStride) :
    pData   { nullptr },
    Width   { width },
    Height  { height },
    Stride  { 0 }
{
    this->Stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);
    this->pData = (uint8_t*)aligned_alloc(32, height * this->Stride);

    for (size_t y=0; y<height; y++) { 
        memcpy(this->pData + y*this->Stride, pPixels + y*lineStride, width*4);
    }
}

Frame& Frame::operator=(const Frame& rhs)
{
    if (this->pData)
        free(this->pData);

    this->pData = (uint8_t*)aligned_alloc(32, rhs.Height * rhs.Stride);
    ::memcpy(this->pData, rhs.pData, rhs.Height * rhs.Stride);

    this->Width = rhs.Width;
    this->Height = rhs.Height;
    this->Stride = rhs.Stride;

    return *this;
}

Frame& Frame::operator=(Frame&& rhs)
{
    if (this->pData)
        free(this->pData);

    this->pData = rhs.pData;
    this->Width = rhs.Width;
    this->Height = rhs.Height;
    this->Stride = rhs.Stride;

    rhs.pData = nullptr;
    rhs.Width = 0;
    rhs.Height = 0;
    rhs.Stride = 0;
    return *this;
}

Frame::~Frame()
{
    if (this->pData)
        free(this->pData);
}

float Frame::GetDifference(const Frame* pOther)
{
    assert(this->pData != nullptr);
    assert(pOther->pData != nullptr);

    if (this->Width != pOther->Width || this->Height != pOther->Height)
        return -1.0f;

    if (this->pData == nullptr || pOther->pData == nullptr) {
        return 0.0f;
    }

    float diff = 0;
    for (size_t y=0; y<this->Height; y++) {
        for (size_t x=0; x<this->Width*4; x+=4) {
            diff += PixelDiff( this->pData + x+y*this->Stride, pOther->pData + x+y*pOther->Stride );
            /*
            float yThis = 0.299 * rgbThis[0] + 0.587 * rgbThis[1] + 0.114 * rgbThis[2];
            float yThat = 0.299 * rgbThat[0] + 0.587 * rgbThat[1] + 0.114 * rgbThat[2];

            float yDiff = std::abs(yThat-yThis);
            diff += yDiff;
            */
        }
    }

    return std::sqrt(diff) / std::sqrt((float)(this->Width * this->Height * 3));
}

float Frame::GetContrast() const
{
    assert(this->pData != nullptr);

    if (this->pData == nullptr) {
        return 0.0f;
    }

    float mean = 0.0f;
    float variance = 0.0f;

    size_t y;

    #pragma omp parallel for default(shared) private(y) reduction(+:mean)
    for (y=0; y<this->Height; y++) {
        float lineMean = 0.0f;
        for (size_t x=0; x<this->Width*4; x+=4) {
            float val = 0.299f * this->pData[x+y*this->Stride + 0] +
                        0.587f * this->pData[x+y*this->Stride + 1] +
                        0.114f * this->pData[x+y*this->Stride + 2] ;
    
            val      /= 255.0f;
            lineMean     += val;
        }
        mean += lineMean; 

    }

    mean /= (float)(this->Width * this->Height);

    #pragma omp parallel for default(shared) private(y) reduction(+:variance)
    for (y=0; y<this->Height; y++) {
        float lineVar = 0.0;
        for (size_t x=0; x<this->Width*4; x+=4) {
            float val = 0.299f * this->pData[x+y*this->Stride + 0] +
                        0.587f * this->pData[x+y*this->Stride + 1] +
                        0.114f * this->pData[x+y*this->Stride + 2] ;
            val      /= 255.0f;
            lineVar += (val-mean) * (val-mean);
        }
        variance += lineVar;
    }

    variance /= (float)(this->Width * this->Height - 1);

    //std::cerr << "light: " << lightValue << " dark: " << darkValue << " contrast: " << std::abs( lightValue - darkValue ) / std::max( lightValue, darkValue ) << std::endl;
    //std::cerr << "variance: " << variance << " stddev: " << std::sqrt(variance) << std::endl;
    return std::sqrt(variance);

/*
    float lightValue = 0.0f;
    float midValue   = 0.0f;
    float darkValue  = 0.0f;

    float histogram[16] = { 0.0f };

    for (size_t y=0; y<this->Height; y++) {
        for (size_t x=0; x<this->Width*4; x+=4) {
            float val = 0.299f * this->pData[x+y*this->Stride + 0] +
                        0.587f * this->pData[x+y*this->Stride + 1] +
                        0.114f * this->pData[x+y*this->Stride + 2] ;
    
            size_t index = 15 * val;
            histogram[index] ++;
        }
    }
*/


    //return std::abs( lightValue - darkValue ) / std::max( lightValue, darkValue );  
}

void Frame::Save(const char *pFileName) const 
{
  cairo_surface_t *      surface       = (cairo_surface_t*)this->CreateCairoSurface();

  cairo_surface_write_to_png(surface, pFileName);
  //fprintf(stderr, "status: %04x\n", status);

  cairo_surface_destroy(surface);
}

void* Frame::CreateCairoSurface() const
{
    return cairo_image_surface_create_for_data(this->pData, CAIRO_FORMAT_RGB24, this->Width, this->Height, this->Stride);
}

}
