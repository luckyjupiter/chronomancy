#ifndef PSEUDORNG_INCLUDED_
#define PSEUDORNG_INCLUDED_


#include "MersenneTwister.hpp"

#include <Windows.h>

class Buffer;


class PseudoRng
{
public:
    PseudoRng(Buffer& buffer);
    ~PseudoRng();

public:
    void Start();
    void Stop();

private:
    static unsigned __stdcall Runner(void* self);

private:
    HANDLE milliTimerPulse_;
    HANDLE runnerThread_;
    HANDLE endEvent_;
    Buffer& buffer_;
};


#endif // PSEUDORNG_INCLUDED_
