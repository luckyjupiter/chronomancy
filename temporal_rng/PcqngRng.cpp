#include "PcqngRng.hpp"
#include "LpFilter.hpp"
#include "Buffer.hpp"
#include <process.h>


PcqngRng::PcqngRng(Buffer& inBuffer, Buffer& outBuffer, Buffer& outAcBuffer)
    : IRng(outBuffer, outAcBuffer, "PCQNG")
    , inBuffer_(inBuffer)
    , runnerThread_(0)
    , endEvent_(CreateEvent(NULL, TRUE, FALSE, NULL))
    , initCount_(10)
{
    bitRate_ = 119000*4;
}


PcqngRng::~PcqngRng()
{
    Stop();
    CloseHandle(endEvent_);
    endEvent_ = 0;
}


void PcqngRng::Start()
{
    // Start the runner thread
    ResetEvent(endEvent_);
    initCount_ = 10;
    runnerThread_ = (HANDLE)_beginthreadex(NULL, 0, PcqngRng::Runner, this, 0, NULL);
}


void PcqngRng::Stop()
{
    SetEvent(endEvent_);
    WaitForSingleObject(runnerThread_, INFINITE);
    CloseHandle(runnerThread_);
    runnerThread_ = 0;
}


void PcqngRng::ClearBuffer()
{
//    inBuffer_.Clear();
//    outBuffer_.Clear();
}


unsigned __stdcall PcqngRng::Runner(void* self)
{
    PcqngRng* obj = (PcqngRng*)self;

    uint8_t  eBits;
    uint8_t* correctedWords;
    std::vector<uint8_t> inWord;
    std::vector<uint8_t> outWords;
    outWords.resize(17);
    correctedWords = &outWords[0];

    if (SetThreadAffinityMask(GetCurrentThread(), 1) == 0)
        throw "PcqngCore: SetThreadAffinityMask FAILED";
    Sleep(1);

    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL) == 0)
        throw "PcqngCore: SetThreadPriority FAILED";

//    static double bitcntt = 0;
    while (1)
    {
        if (WaitForSingleObject(obj->endEvent_, 0) == WAIT_OBJECT_0)
            break;

        obj->inBuffer_.Read(inWord, 1);
        eBits = inWord[0];

        for (int x=0; x<4; ++x)
        {
            for (int i=0; i<17; ++i)
            {
                correctedWords[i] = 0;
                for (int b=0; b<7; ++b)
                {
//                    ++bitcntt;
                    correctedWords[i] <<= 1;
                    correctedWords[i] |= obj->lfsrCorrector_.Next((eBits>>(6-b))&0x1);
                }
            }
            
            if (obj->initCount_ == 0) {
				obj->rawBuffer_->Write(outWords);
                obj->outBuffer_.Write(outWords);
			}
            else
                --obj->initCount_;
        }
/*
        static int l = 0;
        if (++l == 10000)
        {
            static LARGE_INTEGER prevM, thisM, freqM;
            QueryPerformanceCounter(&thisM);
            QueryPerformanceFrequency(&freqM);
            printf("bps: %f\n", bitcntt/((double)(thisM.QuadPart-prevM.QuadPart)/(double)freqM.QuadPart));
            bitcntt = 0;
            prevM = thisM;
            l = 0;
        }*/
    }

    return 0;
}
