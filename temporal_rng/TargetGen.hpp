#ifndef INCLUDE_HPP_TARGETGEN_
#define INCLUDE_HPP_TARGETGEN_


#include <Windows.h>


class TargetGen
{
public:
    TargetGen();
    ~TargetGen();


public:
    void PcqngRawFeed(unsigned long rawWord32);
    unsigned char GetTargetData();


private:
    CRITICAL_SECTION targetOutMutex_;
    unsigned char markedTargetBits_;
    unsigned char prevBit_;
    unsigned char targetData_;
    bool requestingTargetBits_;
};


#endif // INCLUDE_HPP_TARGETGEN_

