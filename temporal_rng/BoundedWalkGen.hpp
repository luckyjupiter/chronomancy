#ifndef BOUNDEDWALKGEN_INCLUDED_
#define BOUNDEDWALKGEN_INCLUDED_


#include "IPrdGen.hpp"
#include "stdint.h"
#include <vector>

class IRng;
class Buffer;

class BoundedWalkGen : public IPrdGen
{
public:
    BoundedWalkGen(IRng& rng, bool isBiasChannel, double avgBoundHitSec, double avgBufferSizeSec);

    virtual double Generate();
    virtual void ClearBuffers();
    virtual std::string GetId();
    virtual void SetPrdMode(int prdMode);
    virtual int  GetPrdMode();

private:
    int boundSteps_;
    int inWordPointer_;
    int inBitPointer_;
    int minReadWords_;
	std::vector<uint8_t> randWords_;
    Buffer& inBuffer_;
};


#endif // BOUNDEDWALKGEN_INCLUDED_
