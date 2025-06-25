#ifndef INLCUDE_HPP_PCQNGSYNCPRD_
#define INLCUDE_HPP_PCQNGSYNCPRD_


#include "IPrd.hpp"
#include "LfsrCorrector.hpp"
#include "LpFilter.hpp"
#include "BackProp.H"
class IRng;


class PcqngSyncPrd : public IPrd
{
public:
    PcqngSyncPrd(IRng* rng_);

public:
    virtual double Generate();
    double* GetLogStreams();

private:
    void SetTargets(int mode);

private:
    LfsrCorrector lfsrCorrector_;
    LpFilter lpFilters_[7];
    LfsrCorrector lfsrTargetsCorrector_;
    double* logStreams_;
    CBackProp* annA_;
    CBackProp* annB_;
    CBackProp* annC_;
    CBackProp* annD_;
    CBackProp* annE_;
    double medianA_;
    double medianB_;
    double medianC_;
    double medianD_;
    double medianE_;

//    char* targetBuffer_;
};



#endif // INLCUDE_HPP_PCQNGSYNCPRD_
