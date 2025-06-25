#ifndef PCQNGTARGETGEN_INCLUDED_
#define PCQNGTARGETGEN_INCLUDED_


#include "ITargetGen.hpp"
#include "LfsrCorrector.hpp"
#include "stdint.h"

class Buffer;
class PcqngCore;

class PcqngTargetGen : public ITargetGen
{
public:
    PcqngTargetGen(Buffer& inBuffer, size_t targetBuffLen, PcqngCore& pcqngCore);
    ~PcqngTargetGen();

public:
    virtual uint8_t GetNextTarget();
    virtual uint8_t GetNextPrecogTarget();
    virtual void SetTargetBufferSize(size_t size);
    virtual void ClearBuffers();

private:
    void GenerateTarget();

private:
    PcqngCore& pcqngCore_;
    Buffer& inBuffer_;
    Buffer* targetBuffer_;
    LfsrCorrector lfsrCorrector_;
    int initCount_;
};


#endif // PCQNGTARGETGEN_INCLUDED_
