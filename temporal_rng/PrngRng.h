#pragma once
#include "IRng.h"

class PrngRng :
    public IRng
{
public:
    Prng(string id, Buffer* rngBuffer, Buffer* targetBuffer);
    ~PrngRng();

    void Start();
    void Stop();
    void ClearBuffer();
    virtual int Write(vector<uint8_t> writeBuffer);
    virtual int SetBufferSizeMs(int bufferSizeMs);
    Buffer* GetTargetBuffer();
    virtual int GetBitRate();

private:
    static unsigned __stdcall Runner(void* self);

private:
    HANDLE runnerThread_;
    HANDLE endEvent_;
    LfsrCorrector lfsrCorrector;
    LfsrCorrector targetCorrector;
    int initCount_;
    Buffer* targetBuffer;
    HANDLE mmTimerEvent_;
    MMRESULT mmTimerId_;
};

