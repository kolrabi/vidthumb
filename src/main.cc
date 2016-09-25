#include "frame.hh"
#include "stream.hh"

#include <cstdio>
extern "C" {
#include <libavformat/avformat.h>
}

#include <vector>
#include <algorithm>
#include <iostream>

#include <cairo/cairo.h>

int main(int argc, char **argv)
{
    if (argc < 3) {
        return -1;
    }

    bool portrait = false;
    if (!::strcmp(argv[1], "-p")) {
        portrait = true;
        argc--;
        argv++;
    }

    av_register_all();

    const char *pStreamName = argv[1];
    const char *pOverViewName = argv[2];

    // TODO: make these command line parameters
    size_t thumbWidth   = portrait ? 890 : 320;
    size_t thumbHeight  = portrait ? 1280 : 200;
    size_t rowCount     = 6;
    size_t colCount     = 6;

    size_t thumbCount   = rowCount * colCount;

    vidthumb::Stream* pStream = vidthumb::Stream::Open(pStreamName, thumbWidth, thumbHeight);
    if (pStream) {
        std::vector<float>  frameDiffs, frameContrasts, temp;
        vidthumb::Frame     frames[2];

        vidthumb::Frame*    pCurFrame   = frames + 0;
        vidthumb::Frame*    pLastFrame  = frames + 1;
        size_t              curFrame    = 0;

        std::vector<size_t> candidateFrames, selectedFrames;

        float meanDiff = 0.0f;
        float meanVar = 0.0f;
        float medianDiff = 0.0f;
        float medianVar = 0.0f;
        size_t totalFrames = pStream->GetTotalFrameCount();
        bool ignoreDiffs = false;

        // read frame differences
        std::cerr << "Reading "<< totalFrames <<" frame differences..." << std::endl;
        int pct = 0;
        while(pStream->GetNextFrame(*pCurFrame, false)) {
            float diff = curFrame == 0 ? 0 : pCurFrame->GetDifference(pLastFrame);
            float var  = pCurFrame->GetContrast();
            frameDiffs.push_back(diff);
            frameContrasts.push_back(var);

            if (diff < 0.0f)
                ignoreDiffs = true;

        // std::cerr << diff <<" "<< var << std::endl;

            meanDiff += diff;
            meanVar += var;

            candidateFrames.push_back(curFrame);

            std::swap(pCurFrame, pLastFrame);
            curFrame++;

            int newPct = (100.0 * curFrame) / totalFrames;
            if (newPct != pct) {
                pct = newPct;
                if ((pct % 10) == 0)
                    std::cerr << pct << std::flush;
                else
                    std::cerr << "." << std::flush;
            }
        }

        std::cerr << std::endl;

        meanDiff /= curFrame;
        meanVar /= curFrame;

        temp = frameContrasts;
        std::sort(temp.begin(), temp.end());
        medianVar = temp[temp.size()/2];

        temp = frameDiffs;
        std::sort(temp.begin(), temp.end());
        medianDiff = temp[temp.size()/2];

        auto frameFilter = [&](size_t n){
            bool remove = frameContrasts[n] < medianVar * 0.5f || (!ignoreDiffs && frameDiffs[n] > medianDiff * 2.0) ;
            return remove;
        };

        std::cerr << "mean diff: "<< meanDiff <<" mean variance: " << meanVar << " median variance: "<< medianVar << " median diff: " << medianDiff << std::endl;

        auto it = candidateFrames.begin();

        if (candidateFrames.size() >= thumbCount) 
        while (it != candidateFrames.end()) {
            if (frameFilter(*it))
                it = candidateFrames.erase(it);
            else 
                it++;
        }

        if (thumbCount > candidateFrames.size()) {
            thumbCount = candidateFrames.size();

            colCount = std::floor( std::sqrt((float)thumbCount) );
            if (colCount == 0)
                colCount = 1;

            rowCount = thumbCount / colCount;
            thumbCount = colCount * rowCount;
        }

        std::cerr << "Selecting "<< thumbCount <<" frames out of " << candidateFrames.size() <<  " ..." << std::endl;

        for (size_t i=0; i<thumbCount; i++) {
            float f = (float)i / (float)thumbCount;

            selectedFrames.push_back( candidateFrames[ (size_t)(f * candidateFrames.size()) ]);
        }

        std::cerr << "Creating overview "<< pOverViewName <<"..." << std::endl;

        for (size_t i=0; i<thumbCount; i++) {
            std::cerr << i << ": " << selectedFrames[i] << std::endl;
        }

        if (colCount < rowCount)
            std::swap(colCount, rowCount);

        cairo_surface_t* pOverviewSurface = nullptr;
    
        pOverviewSurface = cairo_image_surface_create(
            CAIRO_FORMAT_RGB24, 
            thumbWidth * colCount, 
            thumbHeight * rowCount
        );

        cairo_t* pCairo = cairo_create(pOverviewSurface);

        size_t thumbIndex = 0;

        curFrame = 0;
        pStream->Rewind();
        std::swap(pCurFrame, pLastFrame);
            
        while(thumbIndex < selectedFrames.size()) {
            if (curFrame < selectedFrames[thumbIndex]) {
                // skip frame
                if (!pStream->SkipNextFrame()) {
                    break;
                }
            } else {
                // get frame
                if (!pStream->GetNextFrame(*pCurFrame, true)) {
                    break;   
                }

                std::cerr << thumbIndex << ": " << curFrame << " " << frameContrasts[curFrame] << std::endl;

                cairo_surface_t* pThumbSurface = (cairo_surface_t*)pCurFrame->CreateCairoSurface();
                size_t thumbX = thumbWidth *  (thumbIndex % colCount);
                size_t thumbY = thumbHeight * (thumbIndex / colCount);

                cairo_set_source_surface(pCairo, pThumbSurface,     thumbX, thumbY);
                cairo_rectangle(pCairo, thumbX, thumbY, thumbWidth, thumbHeight);
                cairo_fill(pCairo);
                thumbIndex++;

                cairo_surface_destroy(pThumbSurface);
                //if ((thumbIndex%colCount) == 0)
                //    cairo_surface_write_to_png(pOverviewSurface, pOverViewName);

            } 
            curFrame++;
        }

        cairo_surface_write_to_png(pOverviewSurface, pOverViewName);

        cairo_destroy(pCairo);
        cairo_surface_destroy(pOverviewSurface);

        delete pStream;
        pStream = nullptr;
    }


    return 0;
}