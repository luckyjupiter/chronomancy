#ifndef INETPRD_INCLUDED_
#define INETPRD_INCLUDED_


#include "IPrdGen.hpp"
#include "stdint.h"
#include <Winsock.h>
/*#include "LpFilter.hpp"
#include "BackProp.H"
#include "xmlParser.h"

#include <vector>
#include <map>

class IRng;
class Buffer;*/


class InetPrd : public IPrdGen
{
public:
    InetPrd();
    ~InetPrd();

public:
    virtual double Generate();
    virtual void   ClearBuffers();
    virtual std::string GetId();

private:
    static unsigned __stdcall InetPrd::Runner(void* self);

    SOCKET udpSock_;
    unsigned long long timeStamp_;
    double data_;
    HANDLE newDataEvent_;
    CRITICAL_SECTION mutex_;
    HANDLE runnerThread_;
    /*    IRng& rng_;
    Buffer& inBuffer_;
    int inBlockLength_;
    int combinedCount_;

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
    std::map<CBackProp*, std::vector<double> > annFitCoefs_;*/
};


#endif // INETPRD_INCLUDED_
