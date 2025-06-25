#ifndef PRDCOMPARATOR_INCLUDED_
#define PRDCOMPARATOR_INCLUDED_


#include "stdint.h"
#include <string>

class IRng;
class IPrdGen;
class ITargetGen;


class PrdComparator
{
public:
    enum PsiMode {PSIMODE_NOTARGETS=0, PSIMODE_PSYCHOKINESIS=1, PSIMODE_CLAIRVOYANCE=2, PSIMODE_PRECOGNITION=3, PSIMODE_COHERENCE};

public:
    PrdComparator(IRng& rng, IPrdGen& prdGen, ITargetGen& targetGen, PsiMode psiMode);

public:
    void    PrdComparator::GenPrdResult(int count, double* psiData, double* targetData);
    void    SetPsiMode(PsiMode psiMode);
    PsiMode GetPsiMode();
	void    SetPrdAcGen(IPrdGen* prdAcGen);

private:
    IRng&       rng_;
    IPrdGen&    prdGen_;
	IPrdGen*    prdAcGen_;
    ITargetGen& targetGen_;
    int8_t      prdTargets_[1000];
    int         prdTargetIndex_;

    PsiMode     psiMode_;
};


#endif // PRDCOMPARATOR_INCLUDED_
