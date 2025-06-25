#ifndef PCQNGCORE_INCLUDED_
#define PCQNGCORE_INCLUDED_


#include "stdint.h"
#include "LpFilter.hpp"

#include <Windows.h>


class Buffer;


class PcqngCore
{
public:
    PcqngCore(Buffer& outBuffer);
    ~PcqngCore();

public:
    void Start();
    void Stop();
    uint8_t PrecogGetTarget();

private:
    static unsigned __stdcall Runner(void* self);

    void TscProcessing();
    void (PcqngCore::*TscProcessing_)();
    void InitTscProcessing();
    void RampUpTscProcessing();
    void NormalTscProcessing();
    void PrecogProcessing();

private:
    HANDLE milliTimerPulse_;
    HANDLE runnerThread_;
    HANDLE endEvent_;
    HANDLE precogEvent_;

    int64_t tscDiff_;
    double qFactor_;
    LpFilter lpFilter_;
    int procCounter_;

    Buffer& outBuffer_;
    uint8_t precogBuffer_;
};


#endif // PCQNGCORE_INCLUDED_
