#ifndef TWISTERRNG_INCLUDED_
#define TWISTERRNG_INCLUDED_

#include "MersenneTwister.hpp"
#include "IRng.hpp"
#include <Windows.h>

class Buffer;


class TwisterRng : public IRng
{
public:
    TwisterRng(Buffer& outBuffer, Buffer& outAcBuffer);
    ~TwisterRng();

public:
    virtual void Start();
    virtual void Stop();

private:
    static unsigned __stdcall Runner(void* self);

private:
    HANDLE milliTimerPulse_;
    HANDLE runnerThread_;
    HANDLE endEvent_;
    MersenneTwister twister_;
};


#endif // TWISTERRNG_INCLUDED_
