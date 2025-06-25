#ifndef BPZANALOGFLIPMULTIPRD_INCLUDED_
#define BPZANALOGFLIPMULTIPRD_INCLUDED_


#include "IPrdGen.hpp"
#include "stdint.h"
#include "LpFilter.hpp"
#include "BackProp.H"
#include "xmlParser.h"

#include <vector>
#include <map>

class IRng;
class Buffer;


class BpZAnalogFlipMultiPrd : public IPrdGen
{
public:
    BpZAnalogFlipMultiPrd(IRng& rng, Buffer& inBuffer);

public:
    virtual double Generate();
    virtual void   ClearBuffers();
    virtual std::string GetId();
    virtual void SetPrdMode(int prdMode);
    virtual int GetPrdMode();

private:
    void   GenerateSevenTuple(std::vector<double>& sevenTuple);
    void   LfsrFeed(uint8_t rngWord);
    void   FeedAnns(std::vector<CBackProp*>& anns, std::vector<double>& sevenTuple, std::vector<double>& annResults);
    double ZCombineAnns(std::vector<double>& annResults);
    double CurveFit(CBackProp* ann);
    void   ReadConfigFile();
    CBackProp* RestoreBpAnn(char const* annFileName);

private:
    IRng& rng_;
    Buffer& inBuffer_;
    int inBlockLength_;
    int combinedCount_;
    int prdMode_;

    double filterCoef_;
    double filterAmplitude_;
    std::vector<LpFilter> lpFilters_;
    std::string prdId_;

    std::vector<CBackProp*> tRawNormAnns_;
    std::vector<CBackProp*> tRawUniAnns_;
    std::vector<CBackProp*> tFactNormAnns_;
    std::vector<CBackProp*> tFactUniAnns_;
    std::vector<CBackProp*> hFactUniAnns_;

    std::map<CBackProp*, double> annMedian_;
    std::map<CBackProp*, double> annFitSd_;
    std::map<CBackProp*, std::vector<double> > annFitCoefs_;
};


#endif // BPZANALOGFLIPMULTIPRD_INCLUDED_
