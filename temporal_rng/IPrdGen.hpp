#ifndef IPRDGEN_INCLUDED_
#define IPRDGEN_INCLUDED_


#include <string>


class IPrdGen
{
public:
    virtual double      Generate()       = 0;
    virtual void        ClearBuffers()   = 0;
    virtual std::string GetId()          = 0;
    virtual void SetPrdMode(int prdMode) = 0;
    virtual int  GetPrdMode()            = 0;
};


#endif // IPRDGEN_INCLUDED_
