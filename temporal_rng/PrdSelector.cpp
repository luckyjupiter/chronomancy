#include "PrdSelector.hpp"
#include "RngBuffer.hpp"
#include "FtdiRng.hpp"
//#include "QwPrd4.hpp"
#include "QwSyncPrd4.hpp"
#include "PcqngRng.hpp"
//#include "PcqngPrd.hpp"
#include "PcqngSyncPrd.hpp"


PrdSelector::PrdSelector(IRng* rng)
    : IPrd(rng)
    , source_(0)
{
    rngBuffer_ = new RngBuffer();

    // check for a QWPRD4
    rng_ = 0;
    try
    {
        rng_ = new FtdiRng(rngBuffer_);
        rng_->Start();
        prd_ = new QwSyncPrd4(rng_);
        source_ = 4;
    }
    catch (...)
    {
        rng_ = 0;
    }

    // default to a PCQNG
    if (rng_ == 0)
    {
        rng_ = new PcqngRng(rngBuffer_);
        prd_ = new PcqngSyncPrd(rng_);
        source_ = -2;
    }
}


PrdSelector::~PrdSelector()
{
    delete prd_;
    delete rng_;
    delete rngBuffer_;
}


double PrdSelector::Generate()
{
    return prd_->Generate();
}


void PrdSelector::SetMode(int mode)
{
    prd_->SetMode(mode);
    mode_ = mode;
}


int PrdSelector::GetMode()
{
    return mode_;
}


int PrdSelector::GetSource()
{
    return source_;
}


double* PrdSelector::GetLogStreams()
{
    if (source_ == -2)
    {
        PcqngSyncPrd* syncPrd = (PcqngSyncPrd*)prd_;
        return syncPrd->GetLogStreams();
    }
    if (source_ == 4)
    {
        QwSyncPrd4* syncPrd = (QwSyncPrd4*)prd_;
        return syncPrd->GetLogStreams();
    }
    
    return 0;
}
