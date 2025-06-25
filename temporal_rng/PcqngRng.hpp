#ifndef PCQNGRNG_INLCUDED_
#define PCQNGRNG_INLCUDED_


#include "IRng.hpp"
#include "LfsrCorrector.hpp"

#include <Windows.h>


class Buffer;


class PcqngRng : public IRng
{
public:
    PcqngRng(Buffer& inBuffer, Buffer& outBuffer, Buffer& outAcBuffer);
    ~PcqngRng();

public:
    virtual void Start();
    virtual void Stop();
    virtual void ClearBuffer();

private:
    static unsigned __stdcall Runner(void* self);

private:
    Buffer& inBuffer_;
    HANDLE runnerThread_;
    HANDLE endEvent_;
    LfsrCorrector lfsrCorrector_;
    int initCount_;
};


#endif // PCQNGRNG_INLCUDED_
