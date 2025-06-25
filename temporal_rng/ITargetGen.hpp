#ifndef ITARGETGEN_INCLUDED_
#define ITARGETGEN_INCLUDED_


#include "stdint.h"


class ITargetGen
{
public:
    virtual uint8_t GetNextTarget() = 0;
    virtual uint8_t GetNextPrecogTarget() = 0;
    virtual void SetTargetBufferSize(size_t size) = 0;
    virtual void ClearBuffers() = 0;
};


#endif // ITARGETGEN_INCLUDED_
