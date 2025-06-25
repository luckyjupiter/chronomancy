//
// Copyright © 2006 Psigenics Corporation.
// 
// PsiGen.hpp
//


#ifndef INCLUDE_HPP_PSIGEN_
#define INCLUDE_HPP_PSIGEN_


#include <Windows.h>
#include "MersenneTwister.h"


class PsiGen
{
public:
    PsiGen();
    ~PsiGen();


public:
    unsigned char GetPsiData();

    void PcqngRawFeed(unsigned long rawWord32);
    void InitXorTable();


private:
    unsigned char xorTable_[256];
    CRITICAL_SECTION psiOutMutex_;
    bool requestingPsiBits_;

    unsigned char psiData_;
    unsigned char markedPsiBits_;
    unsigned char firstPrdBit_;
    int streamNumber_;
    int iterations_;

    MTRand mtRand_;
    int voteCount_[7];
};


#endif // INCLUDE_HPP_PSIGEN_
