#include "PrdComparator.hpp"
#include "IPrdGen.hpp"
#include "ITargetGen.hpp"
#include "IRng.hpp"
#include "Stats.hpp"
#include "math.h"
#include "MersenneTwister.hpp"


PrdComparator::PrdComparator(IRng& rng, IPrdGen& prdGen, ITargetGen& targetGen, PsiMode psiMode)
    : rng_(rng)
    , prdGen_(prdGen)
	, prdAcGen_(NULL)
    , targetGen_(targetGen)
    , psiMode_(psiMode)
    , prdTargetIndex_(0)
{
    // prefill precog targets
    int tWordIndex = 6;
    int tWord      = 0;
    for (int i=0; i<1000; ++i)
    {
        if (tWordIndex == 6)
            tWord = targetGen_.GetNextTarget();
        prdTargets_[i] = 2*((tWord>>tWordIndex)&0x1)-1;
        tWordIndex--;
        if (tWordIndex < 0)
            tWordIndex = 6;
    }
}

void PrdComparator::GenPrdResult(int count, double* psiData, double* targetData)
{
    for (int i=0; i<count; ++i) {
        psiData[i] = prdGen_.Generate();
		if (prdAcGen_ != NULL)
			psiData[i+count] = prdAcGen_->Generate();
	}

    int tWordIndex = 6;
    int tWord      = 0;
    for (int i=0; i<count; ++i)
    {
        switch (psiMode_)
        {
        case PSIMODE_NOTARGETS:
            targetData[i] = 1;
            break;
        case PSIMODE_PSYCHOKINESIS:
            targetData[i] = 1;
            if (tWordIndex == 6)
                targetGen_.GetNextTarget();
            break;
        case PSIMODE_PRECOGNITION:
            if (tWordIndex == 6)
                tWord = targetGen_.GetNextPrecogTarget();
            targetData[i] = 2*((tWord>>tWordIndex)&0x1)-1;
            break;
        case PSIMODE_CLAIRVOYANCE:
            targetData[i] = prdTargets_[prdTargetIndex_];
            if (tWordIndex == 6)
                tWord = targetGen_.GetNextTarget();
            prdTargets_[prdTargetIndex_] = 2*((tWord>>tWordIndex)&0x1)-1;
            prdTargetIndex_++;
            prdTargetIndex_ %= 1000;
            break;
        }
        tWordIndex--;
        if (tWordIndex < 0)
            tWordIndex = 6;
    }
}


void PrdComparator::SetPsiMode(PsiMode psiMode)
{
    psiMode_ = psiMode;
}

PrdComparator::PsiMode PrdComparator::GetPsiMode()
{
    return psiMode_;
}

void PrdComparator::SetPrdAcGen(IPrdGen* prdAcGen)
{
	prdAcGen_ = prdAcGen;
}
